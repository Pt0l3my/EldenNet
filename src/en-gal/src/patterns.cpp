// en-gal's byte patterns (patterns.hpp): the grammar, the multi-pattern matcher, patterns.toml's schema and the rules
// a pattern's hits must pass (ARCH §6.2, §6.3).
#include "en/gal/patterns.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

#include "bytes.hpp"
#include "toml_reader.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;

// ---- The grammar ----

std::size_t BytePattern::fixed() const noexcept {
  return static_cast<std::size_t>(std::count(mask.begin(), mask.end(), std::uint8_t{0xFF}));
}

bool BytePattern::matches(std::span<const std::byte> at) const noexcept {
  if (at.size() < value.size() || mask.size() != value.size()) return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if ((std::to_integer<std::uint8_t>(at[i]) & mask[i]) != value[i]) return false;
  }
  return true;
}

std::string BytePattern::text() const {
  constexpr char kDigits[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size() * 3);
  for (std::size_t i = 0; i < value.size() && i < mask.size(); ++i) {
    if (i > 0) out += ' ';
    if (mask[i] == 0) {
      out += "??";
    } else {
      out += kDigits[value[i] >> 4];
      out += kDigits[value[i] & 0xF];
    }
  }
  return out;
}

const char* to_string(GrammarProblem problem) noexcept {
  switch (problem) {
    case GrammarProblem::Empty: return "holds no byte";
    case GrammarProblem::BadCharacter: return "holds a character that is not a hex digit, ? or whitespace";
    case GrammarProblem::HalfByte: return "holds a hex digit without its pair";
    case GrammarProblem::HalfWildcard: return "holds a ? without its pair";
    case GrammarProblem::Wildcard: return "holds a wildcard where only exact bytes are allowed";
    case GrammarProblem::TooLong: return "holds too many bytes";
    case GrammarProblem::TooFewFixed: return "holds too few fixed bytes";
  }
  return "is not a pattern";
}

std::string describe(const GrammarError& error) {
  return std::string(to_string(error.problem)) + " (at character " + std::to_string(error.at) + ")";
}

namespace {

constexpr bool is_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

constexpr int hex_value(char c) noexcept {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

Expected<BytePattern, GrammarError> parse_pattern(std::string_view text, const GrammarLimits& limits) {
  BytePattern out;
  std::size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (is_space(c)) {
      ++i;
      continue;
    }
    const char d = i + 1 < text.size() ? text[i + 1] : ' ';
    if (c == '?') {
      if (d != '?') return Unexpected{GrammarError{GrammarProblem::HalfWildcard, i}};
      if (!limits.wildcards) return Unexpected{GrammarError{GrammarProblem::Wildcard, i}};
      out.value.push_back(0);
      out.mask.push_back(0);
    } else if (hex_value(c) >= 0) {
      if (d == '?') return Unexpected{GrammarError{GrammarProblem::HalfWildcard, i + 1}};
      if (hex_value(d) < 0) {
        if (is_space(d) || i + 1 >= text.size()) return Unexpected{GrammarError{GrammarProblem::HalfByte, i}};
        return Unexpected{GrammarError{GrammarProblem::BadCharacter, i + 1}};
      }
      out.value.push_back(static_cast<std::uint8_t>(hex_value(c) * 16 + hex_value(d)));
      out.mask.push_back(0xFF);
    } else {
      return Unexpected{GrammarError{GrammarProblem::BadCharacter, i}};
    }
    if (out.value.size() > limits.max_bytes) return Unexpected{GrammarError{GrammarProblem::TooLong, i}};
    i += 2;
  }
  if (out.value.empty()) return Unexpected{GrammarError{GrammarProblem::Empty, 0}};
  if (out.fixed() < limits.min_fixed) return Unexpected{GrammarError{GrammarProblem::TooFewFixed, 0}};
  return out;
}

// ---- The matcher ----

namespace {

// Bytes common in x64 code, which make poor anchors.
constexpr bool common_byte(std::uint8_t b) noexcept {
  switch (b) {
    case 0x00:
    case 0xFF:
    case 0xCC:
    case 0x48:
    case 0x8B:
    case 0x89:
    case 0x0F:
    case 0xE8:
    case 0x4C:
    case 0x90: return true;
    default: return false;
  }
}

bool test_bit(const std::vector<std::uint64_t>& bits, std::uint32_t i) noexcept {
  const std::size_t word = i >> 6;
  return word < bits.size() && ((bits[word] >> (i & 63u)) & 1u) != 0;
}

void set_bit(std::vector<std::uint64_t>& bits, std::uint32_t i) noexcept {
  const std::size_t word = i >> 6;
  if (word < bits.size()) bits[word] |= std::uint64_t{1} << (i & 63u);
}

}  // namespace

MultiMatcher::MultiMatcher(std::span<const BytePattern> patterns, std::size_t max_hits)
    : patterns_(patterns.begin(), patterns.end()),
      pair_bits_(65536 / 64, 0),
      single_bits_(256 / 64, 0),
      counts_(patterns.size(), 0),
      hits_(patterns.size()),
      max_hits_(max_hits) {
  for (std::size_t p = 0; p < patterns_.size(); ++p) {
    const BytePattern& pat = patterns_[p];
    hits_[p].reserve(max_hits_);
    if (pat.mask.size() != pat.value.size()) continue;  // not a pattern parse_pattern makes: never matched
    longest_ = std::max(longest_, pat.size());
    std::optional<std::size_t> best;
    int best_score = -1;
    for (std::size_t k = 0; k + 1 < pat.size(); ++k) {
      if (pat.mask[k] == 0 || pat.mask[k + 1] == 0) continue;
      const int score = (common_byte(pat.value[k]) ? 0 : 1) + (common_byte(pat.value[k + 1]) ? 0 : 1);
      if (score > best_score) {
        best_score = score;
        best = k;
      }
    }
    if (best) {
      const auto key = static_cast<std::uint16_t>(pat.value[*best] | (pat.value[*best + 1] << 8));
      pairs_.push_back(Anchor{key, static_cast<std::uint32_t>(p), static_cast<std::uint32_t>(*best)});
      set_bit(pair_bits_, key);
      continue;
    }
    const auto first = std::find(pat.mask.begin(), pat.mask.end(), std::uint8_t{0xFF});
    if (first == pat.mask.end()) continue;  // no fixed byte: anchored nowhere, never matched
    const auto k = static_cast<std::uint32_t>(first - pat.mask.begin());
    singles_.push_back(Anchor{pat.value[k], static_cast<std::uint32_t>(p), k});
    set_bit(single_bits_, pat.value[k]);
  }
  std::sort(pairs_.begin(), pairs_.end(), [](const Anchor& a, const Anchor& b) { return a.key < b.key; });
}

void MultiMatcher::begin(std::uint64_t start) noexcept {
  std::fill(counts_.begin(), counts_.end(), 0);
  for (std::vector<std::uint64_t>& h : hits_) h.clear();
  next_ = start;
  unscanned_ = 0;
}

void MultiMatcher::check(std::uint32_t pattern, std::uint64_t start, std::uint64_t at,
                         std::span<const std::byte> chunk) noexcept {
  const BytePattern& pat = patterns_[pattern];
  const std::uint64_t into = start - at;
  if (start < at || !bytes::within(into, pat.size(), chunk.size())) return;
  if (!pat.matches(chunk.subspan(static_cast<std::size_t>(into), pat.size()))) return;
  ++counts_[pattern];
  if (hits_[pattern].size() < max_hits_) hits_[pattern].push_back(start);  // within the capacity reserved
}

void MultiMatcher::feed(std::uint64_t at, std::span<const std::byte> chunk, bool ends_run) noexcept {
  const std::optional<std::uint64_t> end_or = bytes::add(at, chunk.size());
  if (!end_or) return;
  const std::uint64_t end = *end_or;
  if (at > next_) {  // a gap: positions no chunk gave
    unscanned_ += at - next_;
    next_ = at;
  }
  if (end <= next_) return;
  const std::uint64_t from = next_;  // starts examined here are in [from, limit)
  std::uint64_t limit = end;
  if (!ends_run) limit = end - from > overlap() ? end - overlap() : from;
  if (limit <= from) return;

  const auto byte_at = [&](std::uint64_t pos) {
    return std::to_integer<std::uint8_t>(chunk[static_cast<std::size_t>(pos - at)]);
  };
  for (std::uint64_t q = from; q < end; ++q) {
    const std::uint8_t b0 = byte_at(q);
    if (!singles_.empty() && test_bit(single_bits_, b0)) {
      for (const Anchor& a : singles_) {
        if (a.key != b0 || q - from < a.offset) continue;
        const std::uint64_t start = q - a.offset;
        if (start < limit) check(a.pattern, start, at, chunk);
      }
    }
    if (q + 1 >= end) break;
    const auto key = static_cast<std::uint16_t>(b0 | (byte_at(q + 1) << 8));
    if (!test_bit(pair_bits_, key)) continue;
    const auto range = std::equal_range(pairs_.begin(), pairs_.end(), Anchor{key, 0, 0},
                                        [](const Anchor& x, const Anchor& y) { return x.key < y.key; });
    for (auto it = range.first; it != range.second; ++it) {
      if (q - from < it->offset) continue;  // its start would come before this feed's first
      const std::uint64_t start = q - it->offset;
      if (start < limit) check(it->pattern, start, at, chunk);
    }
  }
  next_ = limit;
}

void MultiMatcher::finish(std::uint64_t end) noexcept {
  if (end > next_) {
    unscanned_ += end - next_;
    next_ = end;
  }
}

std::uint64_t MultiMatcher::count(std::size_t pattern) const noexcept {
  return pattern < counts_.size() ? counts_[pattern] : 0;
}

std::span<const std::uint64_t> MultiMatcher::hits(std::size_t pattern) const noexcept {
  if (pattern >= hits_.size()) return {};
  return hits_[pattern];
}

// ---- patterns.toml ----

const PatternSpec* PatternSet::find(std::string_view id) const noexcept {
  for (const PatternSpec& p : patterns) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

namespace {

using toml_read::Errors;
using toml_read::Table;

bool is_section_name(std::string_view s) noexcept {
  return !s.empty() && s.size() <= 8 && std::all_of(s.begin(), s.end(), [](char c) { return c > 0x20 && c < 0x7F; });
}

// "CS::ChrIns": identifiers joined by "::", at most 128 bytes.
bool is_class_name(std::string_view s) noexcept {
  if (s.empty() || s.size() > 128) return false;
  std::size_t start = 0;
  while (true) {
    const std::size_t sep = s.find("::", start);
    const std::string_view part = s.substr(start, sep == std::string_view::npos ? s.npos : sep - start);
    if (!toml_read::is_identifier(part, 128)) return false;
    if (sep == std::string_view::npos) return true;
    start = sep + 2;
  }
}

std::optional<Capture> read_capture(const toml::table& t, const std::string& path, Errors& errors,
                                    std::size_t pattern_size, bool idiom, IdiomCapture* idiom_out) {
  Table c(t, path, errors);
  const auto at = c.integer("at", true, 0, 4096);
  const auto kind = c.text("kind", true);
  const auto insn_end = c.integer("insn_end", true, 0, 4096);
  if (idiom && idiom_out != nullptr) {
    const auto section = c.text("section", true);
    if (section && !is_section_name(*section)) errors.add(c.where("section"), "must be a section name of 1 to 8 bytes");
    if (section) idiom_out->section = *section;
    const auto align = c.integer("align", false, 1, 4096);
    if (align && !bytes::is_power_of_two(static_cast<std::uint64_t>(*align))) {
      errors.add(c.where("align"), "must be a power of two");
    }
    idiom_out->align = static_cast<std::uint32_t>(align.value_or(1));
    idiom_out->function = c.boolean("function", false).value_or(false);
  }
  c.done();
  if (kind && *kind != "rip_rel32") errors.add(c.where("kind"), "must be rip_rel32");
  if (!at || !insn_end) return std::nullopt;
  if (static_cast<std::uint64_t>(*at) + 4 > pattern_size || *insn_end < *at + 4 ||
      static_cast<std::uint64_t>(*insn_end) > pattern_size) {
    errors.add(path, "lies outside the pattern");
    return std::nullopt;
  }
  return Capture{static_cast<std::uint32_t>(*at), static_cast<std::uint32_t>(*insn_end)};
}

void read_provenance(const toml::table& t, const std::string& path, Errors& errors, Provenance& out) {
  Table p(t, path, errors);
  if (const auto method = p.text("method", true)) {
    if (*method != "own-derivation") errors.add(p.where("method"), "must be own-derivation");
    out.method = *method;
  }
  if (const toml::array* leads = p.array("leads", false)) {
    out.leads = toml_read::strings_of(*leads, p.where("leads"), errors, 16, 64).value_or(std::vector<std::string>{});
  }
  if (const auto evidence = p.text("evidence", true)) {
    if (!toml_read::is_relative_path(*evidence)) errors.add(p.where("evidence"), "must be a relative path");
    out.evidence = *evidence;
  }
  if (const auto date = p.text("date", true)) {
    if (!toml_read::is_date(*date)) errors.add(p.where("date"), "must be a date, YYYY-MM-DD");
    out.date = *date;
  }
  p.done();
}

void read_pattern(const toml::table& t, const std::string& path, Errors& errors, PatternSpec& out) {
  Table p(t, path, errors);
  if (const auto id = p.text("id", true)) {
    if (!toml_read::is_identifier(*id, 64)) errors.add(p.where("id"), "must be 1 to 64 letters, digits and _");
    out.id = *id;
  }
  const auto kind = p.text("kind", true);
  if (kind) {
    if (*kind == "function") {
      out.kind = PatternKind::Function;
    } else if (*kind == "static") {
      out.kind = PatternKind::Static;
    } else if (*kind == "idiom") {
      out.kind = PatternKind::Idiom;
    } else {
      errors.add(p.where("kind"), "must be one of function, static, idiom");
    }
  }
  if (const auto section = p.text("section", true)) {
    if (!is_section_name(*section)) errors.add(p.where("section"), "must be a section name of 1 to 8 bytes");
    out.section = *section;
  }
  bool have_bytes = false;
  if (const auto text = p.text("bytes", true)) {
    auto parsed = parse_pattern(*text);
    if (parsed) {
      out.bytes = std::move(*parsed);
      have_bytes = true;
    } else {
      errors.add(p.where("bytes"), describe(parsed.error()));
    }
  }
  if (out.kind == PatternKind::Idiom) {
    out.expect_min = 1;
    out.expect_max = kMaxPatternHits;
  }
  if (const toml::node* expect = p.node("expect_matches", false)) {
    if (expect->is_array()) {
      if (const auto r = toml_read::range_of(*expect, p.where("expect_matches"), errors, 1, kMaxPatternHits)) {
        out.expect_min = static_cast<std::uint32_t>(r->first);
        out.expect_max = static_cast<std::uint32_t>(r->second);
      }
    } else if (const auto n = toml_read::integer_of(*expect, p.where("expect_matches"), errors, 1, kMaxPatternHits)) {
      out.expect_min = out.expect_max = static_cast<std::uint32_t>(*n);
    }
  }
  if (const toml::array* builds = p.array("builds", false)) {
    if (auto keys = toml_read::strings_of(*builds, p.where("builds"), errors, 32, 32)) {
      for (std::size_t i = 0; i < keys->size(); ++i) {
        if (!toml_read::is_build_key((*keys)[i])) {
          errors.add(toml_read::indexed(p.where("builds"), i), "must be a build key such as 1.2.3.4-ww");
        }
      }
      out.builds = std::move(*keys);
    }
  }
  if (const toml::table* provenance = p.table("provenance", true)) {
    read_provenance(*provenance, p.where("provenance"), errors, out.provenance);
  }

  const std::size_t size = have_bytes ? out.bytes.size() : 4096;  // without bytes, captures are checked no further
  if (kind && out.kind == PatternKind::Function) {
    if (const auto anchor = p.integer("anchor_min", false, 5, 4096))
      out.anchor_min = static_cast<std::uint32_t>(*anchor);
    if (const toml::node* len = p.node("fn_len", true)) {
      if (const auto r = toml_read::range_of(*len, p.where("fn_len"), errors, 1, 0x7FFFFFFF)) {
        out.fn_len_min = static_cast<std::uint32_t>(r->first);
        out.fn_len_max = static_cast<std::uint32_t>(r->second);
      }
    }
    if (const auto prologue = p.text("prologue", false); prologue && !prologue->empty()) {
      const auto parsed = parse_pattern(*prologue, GrammarLimits{64, 1, false});
      if (parsed) {
        out.prologue = parsed->value;
      } else {
        errors.add(p.where("prologue"), describe(parsed.error()));
      }
    }
  } else if (kind && out.kind == PatternKind::Static) {
    if (const toml::table* capture = p.table("capture", true)) {
      if (const auto c = read_capture(*capture, p.where("capture"), errors, size, false, nullptr)) out.capture = *c;
    }
    if (const auto target = p.text("target_section", true)) {
      if (!is_section_name(*target)) errors.add(p.where("target_section"), "must be a section name of 1 to 8 bytes");
      out.target_section = *target;
    }
    if (const auto rtti = p.text("rtti", false)) {
      if (!is_class_name(*rtti)) errors.add(p.where("rtti"), "must be a class name such as Space::Class");
      out.rtti = *rtti;
    }
  } else if (kind && out.kind == PatternKind::Idiom) {
    if (const auto suffix = p.text("path_suffix", false)) {
      const bool printable = !suffix->empty() && suffix->size() <= 64 &&
                             std::all_of(suffix->begin(), suffix->end(), [](char c) { return c > ' ' && c < 0x7F; });
      if (!printable) errors.add(p.where("path_suffix"), "must be 1 to 64 printable ASCII characters");
      out.path_suffix = *suffix;
    }
    if (const toml::table* captures = p.table("captures", true)) {
      if (captures->empty() || captures->size() > 8) errors.add(p.where("captures"), "must hold 1 to 8 captures");
      for (auto&& [name, node] : *captures) {
        const std::string at = p.where("captures") + "." + std::string(name.str());
        if (!toml_read::is_identifier(name.str(), 32)) {
          errors.add(p.where("captures"), "has a capture whose name is not 1 to 32 letters, digits and _");
          continue;
        }
        const toml::table* c = node.as_table();
        if (c == nullptr) {
          errors.add(at, "must be a table");
          continue;
        }
        IdiomCapture ic;
        ic.name = std::string(name.str());
        if (const auto cap = read_capture(*c, at, errors, size, true, &ic)) {
          ic.capture = *cap;
          out.captures.push_back(std::move(ic));
        }
      }
    }
  }
  p.done();
}

}  // namespace

Expected<PatternSet, SchemaErrors> parse_patterns(std::string_view text, std::span<const std::string_view> required_ids,
                                                  std::string_view file) {
  Errors errors{std::string(file)};
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Table root(*doc, "", errors);
  (void)root.integer("schema", true, 1, 1);
  PatternSet set;
  if (const toml::array* patterns = root.array("pattern", false)) {
    if (patterns->size() > 256) errors.add("pattern", "has more than 256 patterns");
    for (std::size_t i = 0; i < patterns->size() && i < 256; ++i) {
      const std::string path = toml_read::indexed("pattern", i);
      const toml::table* t = patterns->get(i)->as_table();
      if (t == nullptr) {
        errors.add(path, "must be a table");
        continue;
      }
      PatternSpec spec;
      read_pattern(*t, path, errors, spec);
      if (!spec.id.empty() && set.find(spec.id) != nullptr) errors.add(path + ".id", "is not unique");
      set.patterns.push_back(std::move(spec));
    }
  }
  root.done();
  for (const std::string_view id : required_ids) {
    if (set.find(id) == nullptr) errors.add("", "lacks the required pattern " + toml_read::shown(id));
  }
  if (!errors.empty()) return Unexpected{errors.take()};
  return set;
}

// ---- The rules over an image ----

const char* to_string(HitProblem problem) noexcept {
  switch (problem) {
    case HitProblem::NoSection: return "the image has no section of its name";
    case HitProblem::Count: return "it matched other than expect_matches times";
    case HitProblem::Unscanned: return "too few matches, where part of its section could not be read";
    case HitProblem::NoFunction: return "a hit lies in no function of the exception directory";
    case HitProblem::Fragment: return "a hit lies in a fragment of a function";
    case HitProblem::NearEntry: return "a hit starts before entry + anchor_min";
    case HitProblem::PastEnd: return "a hit runs past its function's end";
    case HitProblem::Length: return "the function's length is outside fn_len";
    case HitProblem::Prologue: return "the entry's bytes differ from the prologue";
    case HitProblem::Unreadable: return "bytes a check needs could not be read";
    case HitProblem::TargetSection: return "a capture's target lies outside its section";
    case HitProblem::TargetAlignment: return "a capture's target is not aligned";
    case HitProblem::TargetNotFunction: return "a capture's target does not start a function";
    case HitProblem::Ambiguous: return "the hits resolve to different functions or statics";
  }
  return "it did not resolve";
}

namespace {

constexpr std::uint32_t kPage = 4096;

// The image's bytes at `rva` as the loader maps them (in a file, zeros past what the file holds of a section).
bool read_rva(const ImageInfo& image, const ByteSource& source, std::uint64_t rva, std::span<std::byte> dst) {
  if (rva > 0xFFFFFFFFu) return false;
  return read_mapped(image, source, static_cast<std::uint32_t>(rva), dst);
}

// The target of the displacement at hit + capture.at, which may lie anywhere, even outside the image; nothing when the
// displacement cannot be read.
std::optional<std::int64_t> target_of(const ImageInfo& image, const ByteSource& source, std::uint32_t hit,
                                      const Capture& capture) {
  std::array<std::byte, 4> raw{};
  if (!read_rva(image, source, std::uint64_t{hit} + capture.at, raw)) return std::nullopt;
  const std::uint32_t u = bytes::u32(raw, 0).value_or(0);
  const std::int64_t disp =
      u >= 0x80000000u ? static_cast<std::int64_t>(u) - 0x100000000LL : static_cast<std::int64_t>(u);
  return std::int64_t{hit} + capture.insn_end + disp;
}

// Whether `target` lies in `section`.
bool lies_in(const Section* section, std::int64_t target) {
  return section != nullptr && target >= 0 && target <= std::int64_t{0xFFFFFFFF} &&
         section->contains(static_cast<std::uint32_t>(target));
}

void fail(PatternResult& r, HitProblem problem, std::uint32_t at) {
  if (r.problem) return;
  r.problem = problem;
  r.problem_at = at;
}

void resolve_function(const ImageInfo& image, const ByteSource& source, const PatternSpec& spec, PatternResult& r) {
  bool first = true;
  for (const std::uint32_t hit : r.hits) {
    const RuntimeFunction* fn = image.function_at(hit);
    if (fn == nullptr) return fail(r, HitProblem::NoFunction, hit);
    const std::optional<bool> fragment = is_fragment(image, source, *fn);
    if (!fragment) return fail(r, HitProblem::Unreadable, fn->unwind);
    if (*fragment) return fail(r, HitProblem::Fragment, hit);
    if (hit - fn->begin < spec.anchor_min) return fail(r, HitProblem::NearEntry, hit);
    if (std::uint64_t{hit} + spec.bytes.size() > fn->end) return fail(r, HitProblem::PastEnd, hit);
    if (fn->length() < spec.fn_len_min || fn->length() > spec.fn_len_max) return fail(r, HitProblem::Length, hit);
    if (!spec.prologue.empty()) {
      std::vector<std::byte> entry(spec.prologue.size());
      if (!read_rva(image, source, fn->begin, entry)) return fail(r, HitProblem::Unreadable, fn->begin);
      for (std::size_t i = 0; i < entry.size(); ++i) {
        if (std::to_integer<std::uint8_t>(entry[i]) != spec.prologue[i]) return fail(r, HitProblem::Prologue, hit);
      }
    }
    if (first) {
      r.entry = fn->begin;
      r.length = fn->length();
      first = false;
    } else if (fn->begin != r.entry) {
      return fail(r, HitProblem::Ambiguous, hit);
    }
  }
}

void resolve_static(const ImageInfo& image, const ByteSource& source, const PatternSpec& spec, PatternResult& r) {
  const Section* data = image.section(spec.target_section);
  bool first = true;
  for (const std::uint32_t hit : r.hits) {
    const std::optional<std::int64_t> target = target_of(image, source, hit, spec.capture);
    if (!target) return fail(r, HitProblem::Unreadable, hit);
    if (!lies_in(data, *target)) return fail(r, HitProblem::TargetSection, hit);
    const auto rva = static_cast<std::uint32_t>(*target);
    if (rva % 8 != 0) return fail(r, HitProblem::TargetAlignment, hit);
    if (first) {
      r.target = rva;
      first = false;
    } else if (rva != r.target) {
      return fail(r, HitProblem::Ambiguous, hit);
    }
  }
}

void resolve_idiom(const ImageInfo& image, const ByteSource& source, const PatternSpec& spec, PatternResult& r) {
  for (const std::uint32_t hit : r.hits) {
    IdiomHit found{hit, {}};
    bool ok = true;
    for (const IdiomCapture& c : spec.captures) {
      const std::optional<std::int64_t> target = target_of(image, source, hit, c.capture);
      if (!target || !lies_in(image.section(c.section), *target) || c.align == 0 ||
          static_cast<std::uint32_t>(*target) % c.align != 0) {
        ok = false;
        break;
      }
      const auto rva = static_cast<std::uint32_t>(*target);
      if (c.function) {
        const RuntimeFunction* fn = image.function_at(rva);
        if (fn == nullptr || fn->begin != rva) {
          ok = false;
          break;
        }
      }
      found.targets.push_back(rva);
    }
    if (ok) {
      r.idiom_hits.push_back(std::move(found));
    } else {
      ++r.idiom_rejected;
    }
  }
}

}  // namespace

ScanReport scan_patterns(const ImageInfo& image, const ByteSource& source, const PatternSet& set,
                         const ScanOptions& options) {
  ScanReport report;
  report.results.resize(set.patterns.size());
  for (std::size_t i = 0; i < set.patterns.size(); ++i) {
    report.results[i].id = set.patterns[i].id;
    report.results[i].kind = set.patterns[i].kind;
  }

  // Each section once, with every pattern that is matched in it.
  std::vector<std::string_view> done;
  for (const PatternSpec& first : set.patterns) {
    if (std::find(done.begin(), done.end(), first.section) != done.end()) continue;
    done.push_back(first.section);
    std::vector<std::size_t> members;
    std::vector<BytePattern> patterns;
    for (std::size_t i = 0; i < set.patterns.size(); ++i) {
      if (set.patterns[i].section != first.section) continue;
      members.push_back(i);
      patterns.push_back(set.patterns[i].bytes);
    }
    const Section* s = image.section(first.section);
    if (s == nullptr) {
      for (const std::size_t i : members) fail(report.results[i], HitProblem::NoSection, 0);
      continue;
    }

    MultiMatcher matcher(patterns, kMaxPatternHits);
    // A file's section is scanned over the raw data the file holds of it (file_span), a mapped image's whole.
    const bool memory = image.layout == ImageLayout::Memory;
    const FileSpan held = image.file_span(*s);
    const std::uint32_t length = memory ? s->mapped_size() : held.size;
    const std::uint64_t base = memory ? std::uint64_t{s->rva} : held.offset;
    const std::uint64_t chunk_size =
        std::max<std::uint64_t>({options.chunk_bytes, 2 * std::uint64_t{matcher.longest()}, kPage});
    const std::uint64_t step = chunk_size - matcher.overlap();
    std::vector<std::byte> buffer(static_cast<std::size_t>(chunk_size));
    matcher.begin(s->rva);
    for (std::uint64_t pos = 0; pos < length; pos += step) {
      const std::uint64_t len = std::min<std::uint64_t>(chunk_size, length - pos);
      const bool last = pos + len >= length;
      const std::span<std::byte> chunk = std::span<std::byte>(buffer).first(static_cast<std::size_t>(len));
      if (source.read(base + pos, chunk)) {
        matcher.feed(s->rva + pos, chunk, last);
      } else {
        // Page by page, on the source's page boundaries: each run of readable pages is matched on its own, with every
        // position in it examined, and the pages between are a gap.
        std::uint64_t run = 0;  // where the current run of readable pages started, within the chunk
        bool in_run = false;
        for (std::uint64_t page = 0; page < len;) {
          const std::uint64_t here = base + pos + page;
          const std::uint64_t n = std::min<std::uint64_t>(kPage - here % kPage, len - page);
          const bool readable =
              source.read(here, chunk.subspan(static_cast<std::size_t>(page), static_cast<std::size_t>(n)));
          if (readable && !in_run) {
            run = page;
            in_run = true;
          } else if (!readable && in_run) {
            matcher.feed(s->rva + pos + run,
                         chunk.subspan(static_cast<std::size_t>(run), static_cast<std::size_t>(page - run)), true);
            in_run = false;
          }
          page += n;
        }
        if (in_run) {
          matcher.feed(s->rva + pos + run, chunk.subspan(static_cast<std::size_t>(run)), last);
        }
      }
      if (last) break;
    }
    matcher.finish(std::uint64_t{s->rva} + length);
    report.scanned += length - std::min<std::uint64_t>(length, matcher.unscanned());
    report.unscanned += matcher.unscanned();

    for (std::size_t m = 0; m < members.size(); ++m) {
      PatternResult& r = report.results[members[m]];
      const PatternSpec& spec = set.patterns[members[m]];
      r.count = matcher.count(m);
      for (const std::uint64_t hit : matcher.hits(m)) r.hits.push_back(static_cast<std::uint32_t>(hit));
      if (r.count < spec.expect_min || r.count > spec.expect_max) {
        fail(r, r.count < spec.expect_min && matcher.unscanned() > 0 ? HitProblem::Unscanned : HitProblem::Count, 0);
        continue;
      }
      switch (spec.kind) {
        case PatternKind::Function: resolve_function(image, source, spec, r); break;
        case PatternKind::Static: resolve_static(image, source, spec, r); break;
        case PatternKind::Idiom: resolve_idiom(image, source, spec, r); break;
      }
    }
  }
  return report;
}

}  // namespace en::gal
