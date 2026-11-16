// en-gal's numbers-only reports (report.hpp): the writer and the lint.
#include "en/gal/report.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <optional>
#include <utility>

namespace en::gal::report {

using proto::Expected;
using proto::Unexpected;

const Key* Vocabulary::find(std::string_view name) const noexcept {
  for (const Key& k : keys) {
    if (k.name == name) return &k;
  }
  return nullptr;
}

bool is_label(std::string_view s) noexcept {
  if (s.empty() || s.size() > kMaxLabelBytes || s.front() < 'a' || s.front() > 'z') return false;
  return std::all_of(s.begin(), s.end(),
                     [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

std::string describe(const Problem& problem) {
  return problem.where.empty() ? problem.problem : problem.where + ": " + problem.problem;
}

namespace {

constexpr bool scalar(Type t) noexcept { return t != Type::Object && t != Type::Array && t != Type::Pattern; }

const char* type_name(Type t) noexcept {
  switch (t) {
    case Type::Object: return "an object";
    case Type::Array: return "an array";
    case Type::Unsigned: return "a whole number of 0 or more";
    case Type::Signed: return "a whole number";
    case Type::Boolean: return "true or false";
    case Type::Word: return "one of its words";
    case Type::Label: return "a label";
    case Type::Pattern: return "a byte pattern";
  }
  return "a value";
}

bool has_word(const Key& key, std::string_view word) noexcept {
  return std::find(key.words.begin(), key.words.end(), word) != key.words.end();
}

// A pattern a report may hold: within the grammar's rules, at most 64 bytes, and the very text the writer gives it.
bool good_pattern(const BytePattern& p) noexcept {
  return p.value.size() == p.mask.size() && !p.value.empty() && p.value.size() <= kMaxPatternBytes && p.fixed() >= 4;
}

}  // namespace

// ---- The writer ----

Writer::Writer(const Vocabulary& vocabulary) : vocabulary_(vocabulary) {
  bool kind_ok = is_label(vocabulary_.kind);
  out_ = "{\n  \"report\": \"";
  out_ += vocabulary_.kind;
  out_ += "\",\n  \"schema\": ";
  out_ += std::to_string(vocabulary_.schema);
  levels_.push_back(Level{false, nullptr, 2, "", {}});
  if (!kind_ok) refuse("report", "the vocabulary's kind is not a label");
}

void Writer::refuse(std::string where, std::string problem) {
  if (refused_) return;
  refused_ = true;
  refusal_ = Problem{std::move(where), std::move(problem)};
}

std::string Writer::where(std::string_view key) const {
  if (levels_.empty()) return std::string(key);
  const Level& top = levels_.back();
  if (top.array) return top.path + "[" + std::to_string(top.items) + "]";
  if (key.empty()) return top.path;
  return top.path.empty() ? std::string(key) : top.path + "." + std::string(key);
}

bool Writer::value_start(std::string_view key, Type type, const Key** found) {
  if (refused_) return false;
  if (done_ || levels_.empty()) {
    refuse("", "a value after the report was finished");
    return false;
  }
  Level& top = levels_.back();
  const std::string at = where(key);
  const Key* k = nullptr;
  if (top.array) {
    k = top.key;
    if (!key.empty()) {
      refuse(at, "an array's item takes no key");
      return false;
    }
    if (k == nullptr || k->element != type) {
      refuse(at, std::string("must be ") + type_name(k != nullptr ? k->element : Type::Object));
      return false;
    }
    if (top.items >= kMaxArrayItems) {
      refuse(at, "the array holds too many items");
      return false;
    }
  } else {
    if (key.empty()) {
      refuse(at, "an object's value needs a key");
      return false;
    }
    if (key == "report" || key == "schema") {
      refuse(at, "is the report's own key");
      return false;
    }
    k = vocabulary_.find(key);
    if (k == nullptr) {
      refuse(at, "is not a key of this report's vocabulary");
      return false;
    }
    if (k->type != type) {
      refuse(at, std::string("must be ") + type_name(k->type));
      return false;
    }
    if (std::find(top.keys.begin(), top.keys.end(), k) != top.keys.end()) {
      refuse(at, "is a key given twice");
      return false;
    }
    top.keys.push_back(k);
  }
  if (found != nullptr) *found = k;
  const bool inline_items = top.array && k != nullptr && scalar(k->element);
  if (top.items > 0) out_ += inline_items ? ", " : ",";
  if (!inline_items) {
    out_ += "\n";
    out_.append(2 * levels_.size(), ' ');
  }
  if (!top.array) {
    out_ += "\"";
    out_ += key;
    out_ += "\": ";
  }
  ++top.items;
  return true;
}

void Writer::begin_object(std::string_view key) {
  const std::string at = where(key);
  if (!value_start(key, Type::Object, nullptr)) return;
  if (levels_.size() >= kMaxDepth) return refuse(at, "nested too deep");
  out_ += "{";
  levels_.push_back(Level{false, nullptr, 0, at, {}});
}

void Writer::end_object() {
  if (refused_) return;
  if (levels_.size() < 2 || levels_.back().array) return refuse("", "end_object without an object open");
  const Level top = levels_.back();
  levels_.pop_back();
  if (top.items > 0) {
    out_ += "\n";
    out_.append(2 * levels_.size(), ' ');
  }
  out_ += "}";
}

void Writer::begin_array(std::string_view key) {
  const std::string at = where(key);
  const Key* k = nullptr;
  if (!value_start(key, Type::Array, &k)) return;
  if (levels_.size() >= kMaxDepth) return refuse(at, "nested too deep");
  out_ += "[";
  levels_.push_back(Level{true, k, 0, at, {}});
}

void Writer::end_array() {
  if (refused_) return;
  if (levels_.size() < 2 || !levels_.back().array) return refuse("", "end_array without an array open");
  const Level top = levels_.back();
  levels_.pop_back();
  if (top.items > 0 && !(top.key != nullptr && scalar(top.key->element))) {
    out_ += "\n";
    out_.append(2 * levels_.size(), ' ');
  }
  out_ += "]";
}

void Writer::number(std::string_view key, std::uint64_t value) {
  if (!value_start(key, Type::Unsigned, nullptr)) return;
  out_ += std::to_string(value);
}

void Writer::signed_number(std::string_view key, std::int64_t value) {
  if (!value_start(key, Type::Signed, nullptr)) return;
  out_ += std::to_string(value);
}

void Writer::boolean(std::string_view key, bool value) {
  if (!value_start(key, Type::Boolean, nullptr)) return;
  out_ += value ? "true" : "false";
}

void Writer::word(std::string_view key, std::string_view w) {
  const std::string at = where(key);
  const Key* k = nullptr;
  if (refused_) return;
  // Checked before anything is written, so that a refused word leaves no trace in the text.
  const bool in_array = !levels_.empty() && levels_.back().array;
  const Key* expected = levels_.empty() ? nullptr : in_array ? levels_.back().key : vocabulary_.find(key);
  const bool words = expected != nullptr && (in_array ? expected->element : expected->type) == Type::Word;
  if (words && !has_word(*expected, w)) return refuse(at, "is not one of its words");
  if (!value_start(key, Type::Word, &k)) return;
  out_ += "\"";
  out_ += w;
  out_ += "\"";
}

void Writer::label(std::string_view key, std::string_view l) {
  if (refused_) return;
  if (!is_label(l)) return refuse(where(key), "is not a label");
  if (!value_start(key, Type::Label, nullptr)) return;
  out_ += "\"";
  out_ += l;
  out_ += "\"";
}

void Writer::pattern(std::string_view key, const BytePattern& p) {
  if (refused_) return;
  if (!good_pattern(p)) return refuse(where(key), "is not a pattern of 1 to 64 bytes with 4 fixed");
  if (patterns_ >= kMaxPatterns) return refuse(where(key), "one pattern too many for a report");
  if (!value_start(key, Type::Pattern, nullptr)) return;
  ++patterns_;
  out_ += "\"";
  out_ += p.text();
  out_ += "\"";
}

Expected<std::string, Problem> Writer::finish() {
  if (!refused_ && !done_ && levels_.size() != 1) refuse("", "an object or array was left open");
  if (refused_) return Unexpected{refusal_};
  if (!done_) {
    out_ += "\n}\n";
    done_ = true;
    levels_.clear();
  }
  if (out_.size() > kMaxReportBytes) return Unexpected{Problem{"", "the report is larger than 256 KiB"}};
  return out_;
}

// ---- The lint ----

namespace {

using json = nlohmann::json;

constexpr std::size_t kMaxProblems = 64;

// Walks the text's tokens in order, so that every key and value is seen as the text holds it, a repeated key included
// (a parsed document would keep only the last).
class Linter {
 public:
  explicit Linter(std::span<const Vocabulary* const> vocabularies) : vocabularies_(vocabularies) {}

  bool null() { return value(Type::Object, "null is not a value a report holds"); }
  bool boolean(bool) { return value(Type::Boolean, nullptr); }
  bool number_integer(json::number_integer_t v) { return value(v >= 0 ? Type::Unsigned : Type::Signed, nullptr); }
  bool number_unsigned(json::number_unsigned_t v) { return number_value(v); }
  bool number_float(json::number_float_t, const json::string_t&) {
    return value(Type::Object, "a number that is not whole");
  }
  bool string(json::string_t& s) { return string_value(s); }
  bool binary(json::binary_t&) { return value(Type::Object, "binary data"); }
  bool start_object(std::size_t) { return open(false); }
  bool end_object() { return close(); }
  bool start_array(std::size_t) { return open(true); }
  bool end_array() { return close(); }
  bool key(json::string_t& k) { return key_seen(k); }
  bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) {
    add("", "is not JSON");
    return false;
  }

  [[nodiscard]] std::vector<Problem> take() { return std::move(problems_); }
  [[nodiscard]] bool complete() const noexcept { return root_closed_; }

 private:
  struct Frame {
    bool array = false;
    const Key* array_key = nullptr;  // an array's key
    std::string path;
    std::size_t items = 0;
    std::vector<std::string> keys;  // an object's keys so far
    const Key* pending = nullptr;   // an object's key whose value comes next
    std::string pending_name;
  };

  void add(std::string where, std::string problem) {
    if (problems_.size() < kMaxProblems) problems_.push_back(Problem{std::move(where), std::move(problem)});
  }

  std::string here() const {
    if (frames_.empty()) return "";
    const Frame& f = frames_.back();
    if (f.array) return f.path + "[" + std::to_string(f.items) + "]";
    if (f.path.empty()) return f.pending_name;
    return f.path + "." + f.pending_name;
  }

  // The type the next value must have, and its key; nothing at the root's own keys.
  std::optional<Type> expected(const Key** key) {
    *key = nullptr;
    if (frames_.empty()) return Type::Object;
    Frame& f = frames_.back();
    if (f.array) {
      *key = f.array_key;
      return f.array_key != nullptr ? std::optional<Type>(f.array_key->element) : std::nullopt;
    }
    *key = f.pending;
    return f.pending != nullptr ? std::optional<Type>(f.pending->type) : std::nullopt;
  }

  void advance() {
    if (frames_.empty()) return;
    Frame& f = frames_.back();
    ++f.items;
    if (f.array && f.items > kMaxArrayItems && f.items == kMaxArrayItems + 1) add(f.path, "holds too many items");
    f.pending = nullptr;
  }

  bool root_value_expected() const { return !frames_.empty() && frames_.size() == 1 && !frames_.back().array; }

  // A value that is not a string, an object or an array.
  bool value(Type type, const char* refusal) {
    if (root_header_value(nullptr, type)) return keep_going();
    const std::string at = here();
    const Key* key = nullptr;
    const std::optional<Type> want = expected(&key);
    if (refusal != nullptr) {
      add(at, refusal);
    } else if (want && !(*want == type || (*want == Type::Signed && type == Type::Unsigned))) {
      add(at, std::string("must be ") + type_name(*want));
    }
    advance();
    return keep_going();
  }

  bool number_value(json::number_unsigned_t v) {
    if (frames_.size() == 1 && !frames_.back().array && frames_.back().pending_name == "schema") {
      if (vocabulary_ == nullptr || v != vocabulary_->schema) add("schema", "is not the schema this tool reads");
      advance();
      return keep_going();
    }
    return value(Type::Unsigned, nullptr);
  }

  // The root's "report" and "schema", which come first and name the vocabulary.
  bool root_header_value(const std::string* s, Type type) {
    if (!root_value_expected()) return false;
    const std::string& name = frames_.back().pending_name;
    if (name != "report" && name != "schema") return false;
    if (name == "report") {
      if (s == nullptr) {
        add("report", "must name the report's kind");
      } else {
        for (const Vocabulary* v : vocabularies_) {
          if (v != nullptr && v->kind == *s) vocabulary_ = v;
        }
        if (vocabulary_ == nullptr) add("report", "is not a kind of report this tool knows");
      }
    } else if (type != Type::Unsigned) {
      add("schema", "must be a whole number");
    }
    advance();
    return true;
  }

  bool string_value(const std::string& s) {
    if (root_header_value(&s, Type::Word)) return keep_going();
    const std::string at = here();
    const Key* key = nullptr;
    const std::optional<Type> want = expected(&key);
    if (!want) {
      // An unknown key's value: its key was already refused.
    } else if (*want == Type::Word) {
      if (key == nullptr || !has_word(*key, s)) add(at, "is not one of its words");
    } else if (*want == Type::Label) {
      if (!is_label(s)) add(at, "is not a label");
    } else if (*want == Type::Pattern) {
      ++patterns_;
      if (patterns_ == kMaxPatterns + 1) add(at, "one pattern too many for a report");
      const auto parsed = parse_pattern(s, GrammarLimits{kMaxPatternBytes, 4, true});
      if (!parsed) {
        add(at, "is not a pattern of 1 to 64 bytes with 4 fixed");
      } else if (parsed->text() != s) {
        add(at, "is not a pattern in canonical form");
      }
    } else {
      add(at, std::string("is a string where the report holds ") + type_name(*want));
    }
    advance();
    return keep_going();
  }

  bool open(bool array) {
    std::string path;
    const Key* array_key = nullptr;
    if (frames_.empty()) {
      if (array || root_seen_) {
        add("", "a report is one JSON object");
        return false;
      }
      root_seen_ = true;
    } else {
      const std::string at = here();
      const Key* key = nullptr;
      const std::optional<Type> want = expected(&key);
      if (frames_.size() == 1 && !frames_.back().array &&
          (frames_.back().pending_name == "report" || frames_.back().pending_name == "schema")) {
        add(frames_.back().pending_name, "must be a word or a number");
      } else if (want && *want != (array ? Type::Array : Type::Object)) {
        add(at, std::string("must be ") + type_name(*want));
      }
      if (array && want && *want == Type::Array) array_key = key;
      path = at;
      advance();
    }
    if (frames_.size() >= kMaxDepth) {
      add(path, "nested too deep");
      return false;
    }
    frames_.push_back(Frame{array, array_key, path, 0, {}, nullptr, ""});
    return keep_going();
  }

  bool close() {
    if (frames_.empty()) return false;
    frames_.pop_back();
    if (frames_.empty()) root_closed_ = true;
    return keep_going();
  }

  bool key_seen(const std::string& k) {
    if (frames_.empty() || frames_.back().array) return false;
    Frame& f = frames_.back();
    f.pending_name = k;
    f.pending = nullptr;
    const std::string at = here();
    if (std::find(f.keys.begin(), f.keys.end(), k) != f.keys.end()) add(at, "is a key given twice");
    if (f.keys.size() < 512) f.keys.push_back(k);
    const bool root = frames_.size() == 1;
    if (root && f.items == 0 && k != "report") add(at, "a report starts with its \"report\" key");
    if (root && f.items == 1 && k != "schema") add(at, "the \"schema\" key comes second");
    if (root && (k == "report" || k == "schema")) {
      if ((k == "report" && f.items != 0) || (k == "schema" && f.items != 1)) add(at, "is out of its place");
      return keep_going();
    }
    if (vocabulary_ == nullptr) {
      add(at, "comes before the report says its kind");
      return keep_going();
    }
    f.pending = vocabulary_->find(k);
    if (f.pending == nullptr) add(at, "is not a key of this report's vocabulary");
    return keep_going();
  }

  bool keep_going() const noexcept { return problems_.size() < kMaxProblems; }

  std::span<const Vocabulary* const> vocabularies_;
  const Vocabulary* vocabulary_ = nullptr;
  std::vector<Frame> frames_;
  std::vector<Problem> problems_;
  std::size_t patterns_ = 0;
  bool root_seen_ = false;
  bool root_closed_ = false;
};

}  // namespace

Expected<void, std::vector<Problem>> lint(std::string_view text, std::span<const Vocabulary* const> vocabularies) {
  if (text.size() > kMaxReportBytes) return Unexpected{std::vector<Problem>{{"", "is larger than 256 KiB"}}};
  Linter linter(vocabularies);
  const bool parsed = json::sax_parse(text, &linter, json::input_format_t::json, true, false);
  std::vector<Problem> problems = linter.take();
  if (problems.empty() && (!parsed || !linter.complete())) problems.push_back(Problem{"", "is not one JSON object"});
  if (!problems.empty()) return Unexpected{std::move(problems)};
  return {};
}

}  // namespace en::gal::report
