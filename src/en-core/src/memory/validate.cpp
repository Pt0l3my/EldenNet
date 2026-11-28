// The memory job's reply, validated in code (memory_job.hpp; MEM §2.3, §2.5, §3.6, §3.7).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

#include "en/core/memory_job.hpp"

namespace en::core {

namespace {

using json = nlohmann::json;

constexpr std::size_t kRawMax = 8192;
constexpr std::size_t kSummaryWords = 120, kSummaryHard = 160, kKeepsakeWords = 25, kKeepsakeCut = 32;

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::size_t words(std::string_view s) {
  std::size_t n = 0;
  bool in = false;
  for (const char c : s) {
    const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
    if (!space && !in) ++n;
    in = !space;
  }
  return n;
}

bool whole(std::string_view text, std::string_view word) {
  const std::string t = lower(text), w = lower(word);
  for (std::size_t at = t.find(w); at != std::string::npos; at = t.find(w, at + 1)) {
    const bool left = at == 0 || std::isalpha(static_cast<unsigned char>(t[at - 1])) == 0;
    const std::size_t end = at + w.size();
    const bool right = end >= t.size() || std::isalpha(static_cast<unsigned char>(t[end])) == 0;
    if (left && right) return true;
  }
  return false;
}

std::size_t depth(const json& j) {
  if (!j.is_structured()) return 0;
  std::size_t d = 0;
  for (const auto& v : j) d = std::max(d, depth(v));
  return d + 1;
}

// Step 1: the last balanced {...} when the reply is more than the object.
std::optional<json> parse(std::string_view text) {
  json j = json::parse(text, nullptr, false);
  if (j.is_object()) return j;
  const std::size_t end = text.rfind('}');
  if (end == std::string_view::npos) return std::nullopt;
  int level = 0;
  for (std::size_t i = end + 1; i-- > 0;) {
    if (text[i] == '}') ++level;
    if (text[i] == '{' && --level == 0) {
      j = json::parse(text.substr(i, end - i + 1), nullptr, false);
      if (j.is_object()) return j;
      return std::nullopt;
    }
  }
  return std::nullopt;
}

// Step 2's minimum: control characters out, markup out, spaces collapsed (NPC §8.2's steps the text needs here).
std::string clean(std::string_view s) {
  std::string out;
  bool space = false;
  for (const char c : s) {
    const auto u = static_cast<unsigned char>(c);
    if (u < 0x20 || c == '*' || c == '#' || c == '<' || c == '>' || c == '`') {
      space = !out.empty();
      continue;
    }
    if (std::isspace(u) != 0) {
      space = !out.empty();
      continue;
    }
    if (space) out.push_back(' ');
    space = false;
    out.push_back(c);
  }
  return out;
}

bool cjk(std::string_view s) {
  for (std::size_t i = 0; i + 2 < s.size(); ++i) {
    const auto b = static_cast<unsigned char>(s[i]);
    if (b >= 0xE3 && b <= 0xED) return true;  // U+3000 to U+DFFF: CJK, kana, Hangul
  }
  return false;
}

// Step 3: a sentence the journal never keeps.
bool unfit(std::string_view s, const ValidateCtx& ctx) {
  static constexpr std::array<std::string_view, 7> kInjection = {"ignore",   "instruction", "system", "prompt",
                                                                 "as an ai", "assistant:",  "user:"};
  const std::string l = lower(s);
  for (const std::string_view m : kInjection) {
    if (l.find(m) != std::string::npos) return true;
  }
  if (cjk(s)) return true;
  if (ctx.banned && ctx.banned(s)) return true;
  for (const std::string& a : ctx.aliases) {
    if (!a.empty() && l.find(lower(a)) != std::string::npos) return true;
  }
  return false;
}

std::vector<std::string> sentences(std::string_view s) {
  std::vector<std::string> out;
  std::string cur;
  for (std::size_t i = 0; i < s.size(); ++i) {
    cur.push_back(s[i]);
    const bool end = s[i] == '.' || s[i] == '!' || s[i] == '?';
    if (end && (i + 1 == s.size() || s[i + 1] == ' ')) {
      out.push_back(clean(cur));
      cur.clear();
    }
  }
  if (!clean(cur).empty()) out.push_back(clean(cur));
  return out;
}

std::string joined(const std::vector<std::string>& v) {
  std::string out;
  for (const std::string& s : v) out += (out.empty() ? "" : " ") + s;
  return out;
}

bool gendered(std::string_view s) {
  for (const char* p : {"he", "she", "him", "her", "his", "hers", "himself", "herself"}) {
    if (whole(s, p)) return true;
  }
  return false;
}

double round05(double w) { return std::round(std::clamp(w, 0.0, 1.0) * 20.0) / 20.0; }

}  // namespace

const char* to_string(JobFailure f) noexcept {
  switch (f) {
    case JobFailure::None:
      return "none";
    case JobFailure::Empty:
      return "empty";
    case JobFailure::Parse:
      return "parse";
    case JobFailure::Truncated:
      return "truncated";
    case JobFailure::Summary:
      return "summary";
    case JobFailure::Provider:
      return "provider";
  }
  return "parse";
}

std::string_view retry_reminder(JobFailure f) noexcept {
  switch (f) {
    case JobFailure::Summary:
      return "Keep the summary to at most 120 words, and keep what the journal already holds.";
    case JobFailure::Truncated:
      return "Answer shorter: list unchanged keepsakes in \"keep\" by their id instead of writing them out.";
    case JobFailure::Empty:
    case JobFailure::Parse:
      return "Return only the JSON object, with exactly these keys.";
    case JobFailure::None:
    case JobFailure::Provider:
      return {};
  }
  return {};
}

proto::Expected<Validated, JobFailure> validate_reply(std::string_view text, std::string_view finish,
                                                      const JobInput& in, const ValidateCtx& ctx) {
  // 1. Parse.
  if (finish == "length") return proto::Unexpected{JobFailure::Truncated};
  if (text.empty()) return proto::Unexpected{JobFailure::Empty};
  if (text.size() > kRawMax) return proto::Unexpected{JobFailure::Truncated};
  const std::optional<json> parsed = parse(text);
  if (!parsed || depth(*parsed) > 6) return proto::Unexpected{JobFailure::Parse};
  const json& o = *parsed;
  for (const char* k : {"summary", "keep", "keepsakes", "told_name", "disposition_delta"}) {
    if (!o.contains(k)) return proto::Unexpected{JobFailure::Parse};
  }
  if (!o["summary"].is_string() || !o["keep"].is_array() || !o["keepsakes"].is_array()) {
    return proto::Unexpected{JobFailure::Parse};
  }
  Validated v;
  for (auto it = o.begin(); it != o.end(); ++it) {
    if (it.key() != "summary" && it.key() != "keep" && it.key() != "keepsakes" && it.key() != "told_name" &&
        it.key() != "disposition_delta") {
      ++v.unknown_keys;
    }
  }
  const Journal old = in.journal.value_or(Journal{});
  Journal& j = v.journal;
  j = old;
  j.version = old.version + 1;
  j.upto_event = in.upto;
  j.as_of_pt = std::max(old.as_of_pt, in.as_of_pt);
  // 2-5. The summary: cleaned, its unfit sentences out, within 120 words.
  std::vector<std::string> kept;
  const std::vector<std::string> all = sentences(o["summary"].get<std::string>());
  for (const std::string& s : all) {
    if (unfit(s, ctx)) {
      ++v.dropped;
      continue;
    }
    kept.push_back(s);
  }
  std::size_t total = words(joined(kept));
  if (total > kSummaryHard) return proto::Unexpected{JobFailure::Summary};
  while (total > kSummaryWords && !kept.empty()) {
    kept.pop_back();
    total = words(joined(kept));
  }
  if (!old.summary.empty() && (kept.empty() || (all.size() > 0 && v.dropped * 2 > all.size()))) {
    return proto::Unexpected{JobFailure::Summary};
  }
  j.summary = joined(kept);
  // 6. Keepsakes.
  std::map<std::string, const JobLine*> refs;
  for (const auto* group : {&in.lines, &in.notes, &in.context}) {
    for (const JobLine& l : *group) refs[l.id] = &l;
  }
  std::set<std::string> offered;
  for (const CheckOffer& c : in.checks) offered.insert(c.key);
  std::uint32_t next = 1;
  for (const Keepsake& k : old.keepsakes) {
    if (k.id.size() > 1)
      next = std::max<std::uint32_t>(next, static_cast<std::uint32_t>(std::stoul(k.id.substr(1))) + 1);
  }
  std::vector<Keepsake> out;
  for (const json& id : o["keep"]) {
    if (!id.is_string()) continue;
    const auto it = std::find_if(old.keepsakes.begin(), old.keepsakes.end(),
                                 [&](const Keepsake& k) { return k.id == id.get<std::string>(); });
    if (it != old.keepsakes.end() &&
        std::none_of(out.begin(), out.end(), [&](const Keepsake& k) { return k.id == it->id; })) {
      out.push_back(*it);
    }
  }
  std::uint32_t fresh = 0;
  bool name_claimed = false;
  for (const json& item : o["keepsakes"]) {
    if (!item.is_object() || !item.contains("text") || !item["text"].is_string()) continue;
    Keepsake k;
    const Keepsake* before = nullptr;
    if (item.contains("id") && item["id"].is_string()) {
      const auto it = std::find_if(old.keepsakes.begin(), old.keepsakes.end(),
                                   [&](const Keepsake& x) { return x.id == item["id"].get<std::string>(); });
      if (it != old.keepsakes.end()) before = &*it;
    }
    std::string t = clean(item["text"].get<std::string>());
    if (words(t) > kKeepsakeCut || t.empty()) {
      ++v.dropped;
      continue;
    }
    if (words(t) > kKeepsakeWords) {  // cut at the last clause boundary within 25 words
      const std::size_t comma = t.rfind(',', t.size() / 2 + t.size() / 4);
      if (comma == std::string::npos) {
        ++v.dropped;
        continue;
      }
      t = t.substr(0, comma) + ".";
    }
    if (unfit(t, ctx)) {
      ++v.dropped;
      continue;
    }
    k.text = t;
    const std::string basis = item.value("basis", std::string("said"));
    const std::string about = item.value("about", std::string("tarnished"));
    if ((basis != "said" && basis != "seen") || (about != "tarnished" && about != "self" && about != "world")) {
      ++v.dropped;
      continue;
    }
    k.about = about;
    if (about == "tarnished" && gendered(t)) {
      ++v.dropped;
      continue;
    }
    // Refs and basis (MEM §2.5).
    std::vector<const JobLine*> cited;
    if (item.contains("refs") && item["refs"].is_array()) {
      for (const json& r : item["refs"]) {
        if (!r.is_string()) continue;
        const auto it = refs.find(r.get<std::string>());
        if (it != refs.end()) {
          cited.push_back(it->second);
          k.refs.push_back(it->first);
        }
      }
    }
    if (before != nullptr) {
      for (const std::string& r : before->refs) {
        if (std::find(k.refs.begin(), k.refs.end(), r) == k.refs.end()) k.refs.push_back(r);
      }
    }
    if (before == nullptr && cited.empty()) {
      ++v.dropped;
      continue;
    }
    const bool cites_player =
        std::any_of(cited.begin(), cited.end(), [](const JobLine* l) { return l->player || l->overheard; });
    const bool cites_seen = std::any_of(cited.begin(), cited.end(), [&](const JobLine* l) {
      return (l->kind == JobLine::Kind::Note && l->seen && !l->overheard) || l->just_now || (about == "self" && l->own);
    });
    const bool only_own =
        !cited.empty() && std::all_of(cited.begin(), cited.end(), [](const JobLine* l) { return l->own; });
    if (basis == "seen") {
      k.basis = cites_seen ? Keepsake::Basis::Seen : Keepsake::Basis::Said;
      if (!cites_seen && !cites_player && before == nullptr) {
        ++v.dropped;
        continue;
      }
    } else {
      k.basis = only_own ? Keepsake::Basis::Seen : Keepsake::Basis::Said;
    }
    if (before != nullptr && before->basis == Keepsake::Basis::Said) k.basis = Keepsake::Basis::Said;  // never stronger
    if (about == "world" && !cited.empty() &&
        std::all_of(cited.begin(), cited.end(), [](const JobLine* l) { return l->world; })) {
      ++v.kept_by_world;
      continue;
    }
    if (item.contains("check_key") && item["check_key"].is_string() && k.basis == Keepsake::Basis::Said) {
      const std::string key = item["check_key"].get<std::string>();
      if (key == "name") {
        name_claimed = true;
        continue;  // folded into told_name (step 8)
      }
      if (offered.contains(key)) k.check_key = key;
    }
    const double w = item.contains("weight") && item["weight"].is_number() ? item["weight"].get<double>() : 0.5;
    if (before != nullptr) {
      k.id = before->id;
      k.weight = round05(std::clamp(w, before->weight - 0.2, before->weight + 0.2));
      k.since_pt = before->since_pt;
      k.check_key = k.check_key ? k.check_key : before->check_key;
      std::erase_if(out, [&](const Keepsake& x) { return x.id == k.id; });
    } else {
      if (fresh >= 3) continue;  // at most 3 new per job
      ++fresh;
      k.id = "k" + std::to_string(next++);
      k.weight = round05(std::min(w, 0.9));
      k.since_pt = j.as_of_pt;
    }
    k.updated_pt = j.as_of_pt;
    out.push_back(std::move(k));
  }
  // The protected keepsake survives (MEM §2.3), then eviction to 8.
  if (const auto prot = protected_keepsake(old)) {
    const Keepsake& p = old.keepsakes[*prot];
    if (std::none_of(out.begin(), out.end(), [&](const Keepsake& k) { return k.id == p.id; })) out.push_back(p);
  }
  Journal probe = j;
  probe.keepsakes = out;
  const auto keep_id = protected_keepsake(probe);
  const std::string protected_id = keep_id ? out[*keep_id].id : std::string();
  while (out.size() > 8) {
    auto victim = out.end();
    for (auto it = out.begin(); it != out.end(); ++it) {
      if (it->id == protected_id) continue;
      if (victim == out.end() || it->weight < victim->weight ||
          (it->weight == victim->weight && it->updated_pt < victim->updated_pt)) {
        victim = it;
      }
    }
    if (victim == out.end()) break;
    out.erase(victim);
  }
  j.keepsakes = std::move(out);
  v.next_keepsake = next;
  // 7. Promotion: each check key as it stands now.
  for (const Keepsake& k : j.keepsakes) {
    if (k.check_key && ctx.proved && ctx.proved(*k.check_key)) {
      v.verified.push_back(proto::FactRow{"verified:" + *k.check_key, "1", std::nullopt, ctx.now});
    }
  }
  // 8. told_name: a name the Tarnished said in the persona's hearing.
  if (o["told_name"].is_string()) {
    const std::string n = clean(o["told_name"].get<std::string>());
    const bool shape = !n.empty() && n.size() <= 16 && std::all_of(n.begin(), n.end(), [](char c) {
      return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == ' ' || c == '\'' || c == '-' ||
             static_cast<unsigned char>(c) >= 0x80;
    });
    const bool said =
        std::any_of(in.lines.begin(), in.lines.end(), [&](const JobLine& l) { return l.player && whole(l.text, n); }) ||
        std::any_of(in.notes.begin(), in.notes.end(),
                    [&](const JobLine& l) { return l.overheard && l.player && whole(l.text, n); });
    if (shape && said && lower(n) != "tarnished" && !(ctx.banned && ctx.banned(n))) {
      j.told_name = n;
      j.told_name_verified = !ctx.character.empty() && lower(n) == lower(ctx.character);
      j.told_name_heard = !std::any_of(in.lines.begin(), in.lines.end(),
                                       [&](const JobLine& l) { return l.player && whole(l.text, n); });
    }
  }
  (void)name_claimed;
  // Without a told name, the character's name never reaches a prompt through the journal (NPC §11).
  if (!j.told_name && !ctx.character.empty()) {
    std::vector<std::string> safe;
    for (const std::string& s : sentences(j.summary)) {
      if (whole(s, ctx.character)) {
        ++v.dropped;
      } else {
        safe.push_back(s);
      }
    }
    j.summary = joined(safe);
    std::erase_if(j.keepsakes, [&](const Keepsake& k) { return whole(k.text, ctx.character); });
  }
  // 9. Disposition: one step at most, clamped.
  int delta = o["disposition_delta"].is_number_integer() ? o["disposition_delta"].get<int>() : 0;
  if (delta < -1 || delta > 1) delta = 0;
  j.disposition = static_cast<std::int8_t>(std::clamp(old.disposition + delta, -2, 2));
  return v;
}

}  // namespace en::core
