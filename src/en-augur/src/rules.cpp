// The rule files: schema, validation and loading (rules.hpp; ARCH §8.5, DESIGN §6.3).
#include "en/augur/rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <set>

#include <toml.hpp>

#include "en/proto/toml_check.hpp"

namespace en::augur {

namespace {

constexpr std::array<PayloadInfo, 29> kPayloads = {{
    {PayloadId::Place, "place", "PlaceEntered", {"place_id", ""}},
    {PayloadId::BossStart, "boss_start", "BossFightStarted", {"fight_id", ""}},
    {PayloadId::BossOutcome, "boss_outcome", "BossFightEnded", {"fight_id", ""}},
    {PayloadId::TalkStart, "native_talk_start", "NativeTalkStarted", {"", ""}},
    {PayloadId::TalkEnd, "native_talk_end", "NativeTalkEnded", {"", ""}},
    {PayloadId::PlayerDied, "player_died", "PlayerDied", {"", ""}},
    {PayloadId::Mount, "mount", "MountChanged", {"", ""}},
    {PayloadId::Struck, "struck", "Struck", {"att", ""}},
    {PayloadId::LowHp, "low_hp", "LowHp", {"", ""}},
    {PayloadId::Healed, "healed", "Healed", {"", ""}},
    {PayloadId::Engaged, "engaged", "Engaged", {"opponent", ""}},
    {PayloadId::Disengaged, "disengaged", "Disengaged", {"opponent", ""}},
    {PayloadId::AllyDied, "ally_died", "AllyDied", {"", ""}},
    {PayloadId::Crit, "crit", "CritLanded", {"", ""}},
    {PayloadId::Journey, "journey", "JourneyChanged", {"ng_lvl", ""}},
    {PayloadId::DataReady, "data_ready", "DataReady", {"", ""}},
    {PayloadId::Playthrough, "playthrough", "PlaythroughReady", {"slot", ""}},
    {PayloadId::LoadingStarted, "loading_started", "LoadingStarted", {"", ""}},
    {PayloadId::LoadingEnded, "loading_ended", "LoadingEnded", {"", ""}},
    {PayloadId::Title, "title", "TitleReached", {"", ""}},
    {PayloadId::Gate, "gate", "GateChanged", {"active", ""}},
    {PayloadId::Group, "group", "GroupTripped", {"group", ""}},
    {PayloadId::Ladder, "ladder", "LadderChanged", {"to", ""}},
    {PayloadId::PersonaSeen, "persona_seen", "PersonaSeen", {"", ""}},
    {PayloadId::PersonaApproached, "persona_approached", "PersonaApproached", {"", ""}},
    {PayloadId::PersonaAttacked, "persona_attacked", "PersonaAttacked", {"", ""}},
    {PayloadId::PersonaDied, "persona_died", "PersonaDied", {"", ""}},
    {PayloadId::FoughtBeside, "fought_beside", "FoughtBeside", {"fight_id", ""}},
    {PayloadId::KilledNear, "killed_near", "KilledNear", {"", ""}},
}};

constexpr std::string_view kPriorityKey = "priority";

struct InterruptName {
  std::string_view name;
  InterruptSet bits;
};
constexpr std::array<InterruptName, 8> kInterruptNames = {{
    {"chat_input", InterruptSet::ChatInput},
    {"ambient", InterruptSet::Ambient},
    {"autonomous", InterruptSet::Autonomous},
    {"conversation", InterruptSet::Conversation},
    {"conversation_output", InterruptSet::ConversationOutput},
    {"own_lines_except_player_fell", InterruptSet::OwnLinesExceptPlayerFell},
    {"own_lines_within_20m", InterruptSet::OwnLinesWithin20m},
    {"all", kInterruptAll},
}};

constexpr std::array<std::string_view, 6> kRadiusNames = {"speech", "witness", "death", "combat", "none", "subject"};

struct DetectorName {
  std::string_view name;
  DetectorKind kind;
};
constexpr std::array<DetectorName, 11> kDetectorNames = {{
    {"rising_edge", DetectorKind::RisingEdge},
    {"falling_edge", DetectorKind::FallingEdge},
    {"changed_stable", DetectorKind::ChangedStable},
    {"crosses_below", DetectorKind::CrossesBelow},
    {"crosses_above", DetectorKind::CrossesAbove},
    {"drop_over_window", DetectorKind::DropOverWindow},
    {"rise_over_window", DetectorKind::RiseOverWindow},
    {"enter_radius", DetectorKind::EnterRadius},
    {"count_increment", DetectorKind::CountIncrement},
    {"engagement", DetectorKind::Engagement},
    {"fact", DetectorKind::Fact},
}};

// The bounds of ARCH §8.5, and this build's own (impl).
constexpr std::int64_t kMaxDebounceMs = 60000, kMaxMergeMs = 30000, kMaxTtlS = 600, kMaxCooldownS = 3600,
                       kMaxDedupS = 86400, kMaxWindowMs = 10000, kMaxStableMs = 60000;
constexpr std::size_t kMaxIdBytes = 48, kMaxKeyBytes = 96, kMaxRequiresBytes = 1024;

bool is_rule_id(std::string_view s) {
  if (s.empty() || s.size() > kMaxIdBytes || !(s[0] >= 'a' && s[0] <= 'z')) return false;
  return std::all_of(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

std::string shown(std::string_view s) {
  const bool plain = !s.empty() && s.size() <= 40 && std::all_of(s.begin(), s.end(), [](char c) {
    return c > 0x20 && c < 0x7F && c != '"' && c != '\\';
  });
  return plain ? "\"" + std::string(s) + "\"" : std::string("(a long or unprintable word)");
}

class Errors {
 public:
  Errors(std::vector<RuleError>& out, std::size_t max) : out_(out), max_(max) {}
  void add(const std::string& file, std::size_t line, std::string problem) {
    if (out_.size() >= max_) {
      ++dropped_;
      return;
    }
    out_.push_back(RuleError{file, line, std::move(problem)});
  }
  void finish() {
    if (dropped_ > 0) out_.push_back(RuleError{"", 0, "and " + std::to_string(dropped_) + " more problems"});
  }
  [[nodiscard]] bool any() const { return !out_.empty() || dropped_ > 0; }

 private:
  std::vector<RuleError>& out_;
  std::size_t max_;
  std::size_t dropped_ = 0;
};

std::size_t line_of(const toml::node& n) { return static_cast<std::size_t>(n.source().begin.line); }

// One table of a rule, read key by key: every key it holds must be read, else it is unknown.
class Table {
 public:
  Table(const toml::table& t, std::string file, std::string where, Errors& errors)
      : t_(t), file_(std::move(file)), where_(std::move(where)), errors_(errors) {}

  void error(const toml::node* at, const std::string& key, const std::string& problem) {
    errors_.add(file_, at != nullptr ? line_of(*at) : line_of(t_), where_ + (key.empty() ? "" : "." + key) + ": " + problem);
  }
  const toml::node* get(std::string_view key) {
    known_.emplace_back(key);
    return t_.get(key);
  }
  std::optional<std::string> text(std::string_view key, bool required) {
    const toml::node* n = get(key);
    if (n == nullptr) {
      if (required) error(nullptr, std::string(key), "is missing");
      return std::nullopt;
    }
    if (const auto* s = n->as_string()) return s->get();
    error(n, std::string(key), "must be a string");
    return std::nullopt;
  }
  std::optional<std::int64_t> integer(std::string_view key, std::int64_t lo, std::int64_t hi) {
    const toml::node* n = get(key);
    if (n == nullptr) return std::nullopt;
    const auto* i = n->as_integer();
    if (i == nullptr || i->get() < lo || i->get() > hi) {
      error(n, std::string(key), "must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
      return std::nullopt;
    }
    return i->get();
  }
  std::optional<double> number(std::string_view key, bool required) {
    const toml::node* n = get(key);
    if (n == nullptr) {
      if (required) error(nullptr, std::string(key), "is missing");
      return std::nullopt;
    }
    if (const auto* f = n->as_floating_point(); f != nullptr && std::isfinite(f->get())) return f->get();
    if (const auto* i = n->as_integer()) return static_cast<double>(i->get());
    error(n, std::string(key), "must be a number");
    return std::nullopt;
  }
  std::optional<bool> boolean(std::string_view key) {
    const toml::node* n = get(key);
    if (n == nullptr) return std::nullopt;
    if (const auto* b = n->as_boolean()) return b->get();
    error(n, std::string(key), "must be true or false");
    return std::nullopt;
  }
  void done() {
    for (auto&& [k, v] : t_) {
      if (std::find(known_.begin(), known_.end(), k.str()) == known_.end()) {
        error(&v, "", "has an unknown key " + shown(k.str()));
      }
    }
  }
  [[nodiscard]] const toml::table& raw() const { return t_; }

 private:
  const toml::table& t_;
  std::string file_;
  std::string where_;
  Errors& errors_;
  std::vector<std::string> known_;
};

std::optional<PayloadId> payload_by_name(std::string_view name) {
  for (const PayloadInfo& p : kPayloads) {
    if (p.name == name) return p.id;
  }
  return std::nullopt;
}

std::optional<PayloadId> payload_for_kind(std::string_view kind) {
  std::optional<PayloadId> found;
  for (const PayloadInfo& p : kPayloads) {
    if (p.kind == kind) {
      if (found) return std::nullopt;  // more than one: the rule must name it
      found = p.id;
    }
  }
  return found;
}

// The type compatibility of ARCH §8.5: edges on bools, changed_stable on bool, enum, id or integer signals, crossings
// and windows on numbers, count_increment on integers, enter_radius on dist_m, the machine on `engaged`, facts on
// `lifecycle`.
bool compatible(DetectorKind d, const SignalInfo& s) {
  switch (d) {
    case DetectorKind::RisingEdge:
    case DetectorKind::FallingEdge:
      return s.type == SignalType::Bool;
    case DetectorKind::ChangedStable:
      return s.type == SignalType::Bool || s.type == SignalType::Enum || s.type == SignalType::Id ||
             s.type == SignalType::Integer;
    case DetectorKind::CrossesBelow:
    case DetectorKind::CrossesAbove:
    case DetectorKind::DropOverWindow:
    case DetectorKind::RiseOverWindow:
      return s.type == SignalType::Number;
    case DetectorKind::EnterRadius:
      return s.id == SignalId::DistM;
    case DetectorKind::CountIncrement:
      return s.type == SignalType::Integer;
    case DetectorKind::Engagement:
      return s.type == SignalType::Engagement;
    case DetectorKind::Fact:
      return s.type == SignalType::Fact;
  }
  return false;
}

// Reads `detect` into the spec, with each kind's own keys.
std::optional<DetectorSpec> read_detect(const toml::table& t, const SignalInfo* signal, const std::string& file,
                                        const std::string& where, Errors& errors) {
  Table d(t, file, where + ".detect", errors);
  DetectorSpec spec;
  bool ok = true;
  const auto kind = d.text("kind", true);
  std::optional<DetectorKind> found;
  if (kind) {
    for (const DetectorName& n : kDetectorNames) {
      if (n.name == *kind) found = n.kind;
    }
    if (!found) {
      d.error(t.get("kind"), "kind", "is not a detector: " + shown(*kind));
      ok = false;
    }
  }
  if (!found) {
    d.done();
    return std::nullopt;
  }
  spec.kind = *found;
  if (signal != nullptr && !compatible(spec.kind, *signal)) {
    d.error(t.get("kind"), "kind", "does not take the signal " + shown(signal->name));
    ok = false;
  }
  const auto need = [&](std::string_view key) -> std::optional<double> { return d.number(key, true); };
  switch (spec.kind) {
    case DetectorKind::RisingEdge:
    case DetectorKind::FallingEdge:
    case DetectorKind::CountIncrement:
      break;
    case DetectorKind::ChangedStable: {
      if (const auto ms = d.integer("stable_ms", 0, kMaxStableMs)) spec.stable_ms = static_cast<Mono>(*ms);
      if (const toml::node* to = d.get("to")) {
        const auto* a = to->as_array();
        if (a == nullptr || a->empty() || a->size() > 16) {
          d.error(to, "to", "must be a list of 1 to 16 values");
          ok = false;
          break;
        }
        for (const toml::node& v : *a) {
          if (const auto* i = v.as_integer(); i != nullptr && i->get() >= 0 && i->get() <= 0xFFFFFFFFll) {
            spec.to.push_back(static_cast<double>(i->get()));
          } else if (const auto* b = v.as_boolean()) {
            spec.to.push_back(b->get() ? 1.0 : 0.0);
          } else if (const auto* s = v.as_string(); s != nullptr && s->get() == "none") {
            spec.to_none = true;
          } else {
            d.error(&v, "to", "holds a value that is not a whole number, true, false or \"none\"");
            ok = false;
          }
        }
      }
      break;
    }
    case DetectorKind::CrossesBelow:
    case DetectorKind::CrossesAbove: {
      const bool below = spec.kind == DetectorKind::CrossesBelow;
      const auto v = need("value");
      const auto r = need(below ? "rearm_above" : "rearm_below");
      if (!v || !r) {
        ok = false;
        break;
      }
      spec.value = *v;
      spec.rearm = *r;
      if (below ? *r < *v : *r > *v) {
        d.error(t.get(below ? "rearm_above" : "rearm_below"), below ? "rearm_above" : "rearm_below",
                below ? "must be at least value (hysteresis)" : "must be at most value (hysteresis)");
        ok = false;
      }
      break;
    }
    case DetectorKind::DropOverWindow:
    case DetectorKind::RiseOverWindow: {
      const auto f = need("fraction");
      const auto w = d.integer("window_ms", 1, kMaxWindowMs);
      if (!f || !w) {
        if (!w && t.get("window_ms") == nullptr) d.error(nullptr, "window_ms", "is missing");
        ok = false;
        break;
      }
      if (!(*f > 0 && *f <= 1)) {
        d.error(t.get("fraction"), "fraction", "must be above 0 and at most 1");
        ok = false;
      }
      spec.value = *f;
      spec.window_ms = static_cast<Mono>(*w);
      break;
    }
    case DetectorKind::EnterRadius: {
      const auto m = need("m");
      const auto r = need("rearm_m");
      const auto dwell = d.integer("dwell_ms", 0, kMaxStableMs);
      if (!m || !r) {
        ok = false;
        break;
      }
      if (!(*m > 0) || *r < *m) {
        d.error(t.get("rearm_m"), "rearm_m", "must be at least m, and m above 0");
        ok = false;
      }
      spec.value = *m;
      spec.rearm = *r;
      spec.window_ms = dwell ? static_cast<Mono>(*dwell) : 0;
      break;
    }
    case DetectorKind::Engagement: {
      const auto on = d.text("on", true);
      if (on && *on == "engaged") {
        spec.on = 0;
      } else if (on && *on == "disengaged") {
        spec.on = 1;
      } else {
        if (on) d.error(t.get("on"), "on", "must be \"engaged\" or \"disengaged\"");
        ok = false;
      }
      break;
    }
    case DetectorKind::Fact: {
      const auto v = d.text("value", true);
      bool known = false;
      for (std::size_t i = 0; v && i < kFactNames.size(); ++i) {
        if (kFactNames[i] == *v) {
          spec.on = static_cast<std::uint8_t>(i);
          known = true;
        }
      }
      if (!known) {
        if (v) d.error(t.get("value"), "value", "is not a lifecycle fact: " + shown(*v));
        ok = false;
      }
      break;
    }
  }
  d.done();
  if (!ok) return std::nullopt;
  return spec;
}

// The placeholders of a key template: each {name} must be one the payload offers.
bool key_ok(std::string_view key, const PayloadInfo& p, std::string& problem) {
  std::size_t at = 0;
  while (at < key.size()) {
    const char c = key[at];
    if (c == '}') {
      problem = "has a '}' without its '{'";
      return false;
    }
    if (c != '{') {
      if (!(c > 0x20 && c < 0x7F)) {
        problem = "holds a character outside printable ASCII";
        return false;
      }
      ++at;
      continue;
    }
    const std::size_t end = key.find('}', at);
    if (end == std::string_view::npos) {
      problem = "has a '{' without its '}'";
      return false;
    }
    const std::string_view name = key.substr(at + 1, end - at - 1);
    if (name != "subject" && name != p.placeholders[0] && name != p.placeholders[1]) {
      problem = "names a placeholder its payload does not offer: " + shown(name);
      return false;
    }
    if (name.empty()) {
      problem = "has an empty placeholder";
      return false;
    }
    at = end + 1;
  }
  return true;
}

void read_rule(const toml::table& t, std::size_t n, const RuleFile& file, const PredicateCompiler& predicates,
               Errors& errors, std::vector<Rule>& out) {
  const std::string where = "rule[" + std::to_string(n) + "]";
  Table r(t, file.name, where, errors);
  Rule rule;
  rule.file = file.name;
  rule.line = line_of(t);
  bool ok = true;

  const auto id = r.text("id", true);
  if (id && !is_rule_id(*id)) {
    r.error(t.get("id"), "id", "must be a lower-case word of letters, digits and '_', at most 48 bytes");
    ok = false;
  }
  if (id) rule.id = std::make_shared<const std::string>(*id);

  const auto emits = r.text("emits", true);
  std::optional<std::size_t> kind;
  if (emits) {
    kind = kind_index(*emits);
    if (!kind) {
      r.error(t.get("emits"), "emits", "is not in the event catalogue: " + shown(*emits));
      ok = false;
    }
  }

  const auto sig_name = r.text("signal", true);
  const SignalInfo* signal = nullptr;
  if (sig_name) {
    if (sig_name->rfind("flag:", 0) == 0) {
      r.error(t.get("signal"), "signal", "flag signals wait for the flag catalogue and FlagWatch (v0.2)");
      ok = false;
    } else if (const auto s = signal_by_name(*sig_name)) {
      signal = &signal_info(*s);
      rule.signal = *s;
    } else {
      r.error(t.get("signal"), "signal", "is not in the signal registry: " + shown(*sig_name));
      ok = false;
    }
  }

  // subjects = { kinds = [...], alive = bool, in_combat = bool }
  if (const toml::node* s = r.get("subjects")) {
    if (const auto* st = s->as_table()) {
      Table sub(*st, file.name, where + ".subjects", errors);
      if (const toml::node* kinds = sub.get("kinds")) {
        const auto* a = kinds->as_array();
        if (a == nullptr || a->empty()) {
          sub.error(kinds, "kinds", "must be a list of generic, persona, player and world");
          ok = false;
        } else {
          for (const toml::node& k : *a) {
            const auto* ks = k.as_string();
            const std::string_view v = ks != nullptr ? std::string_view(ks->get()) : std::string_view{};
            const std::uint8_t bit = v == "generic" ? kKindGeneric : v == "persona" ? kKindPersona
                                   : v == "player"  ? kKindPlayer  : v == "world"   ? kKindWorld : 0;
            if (bit == 0) {
              sub.error(&k, "kinds", "holds a kind that is not generic, persona, player or world");
              ok = false;
            }
            rule.subjects |= bit;
          }
        }
      } else {
        sub.error(nullptr, "kinds", "is missing");
        ok = false;
      }
      rule.alive = sub.boolean("alive").value_or(false);
      rule.in_combat = sub.boolean("in_combat").value_or(false);
      sub.done();
    } else {
      r.error(s, "subjects", "must be a table");
      ok = false;
    }
  } else {
    r.error(nullptr, "subjects", "is missing");
    ok = false;
  }
  if (signal != nullptr && rule.subjects != 0) {
    const std::uint8_t need = static_cast<std::uint8_t>(
        ((rule.subjects & (kKindGeneric | kKindPersona)) ? kChr : 0) | ((rule.subjects & kKindPlayer) ? kPlayer : 0) |
        ((rule.subjects & kKindWorld) ? kWorld : 0));
    if ((need & signal->subjects) != need) {
      r.error(t.get("subjects"), "subjects", "names a kind the signal " + shown(signal->name) + " is not sampled for");
      ok = false;
    }
  }

  if (const toml::node* d = r.get("detect")) {
    if (const auto* dt = d->as_table()) {
      if (auto spec = read_detect(*dt, signal, file.name, where, errors)) {
        rule.detect = std::move(*spec);
      } else {
        ok = false;
      }
    } else {
      r.error(d, "detect", "must be a table");
      ok = false;
    }
  } else {
    r.error(nullptr, "detect", "is missing");
    ok = false;
  }

  if (const auto db = r.integer("debounce_ms", 0, kMaxDebounceMs)) rule.debounce_ms = static_cast<Mono>(*db);
  if (rule.debounce_ms > 0 &&
      (rule.detect.kind == DetectorKind::Engagement || rule.detect.kind == DetectorKind::Fact)) {
    r.error(t.get("debounce_ms"), "debounce_ms", "is not taken by the engagement machine or a fact");
    ok = false;
  }

  // payload: named, or the one builder of the kind.
  const auto payload = r.text("payload", false);
  std::optional<PayloadId> builder;
  if (payload) {
    builder = payload_by_name(*payload);
    if (!builder) {
      r.error(t.get("payload"), "payload", "is not a payload builder: " + shown(*payload));
      ok = false;
    } else if (kind && payload_info(*builder).kind != kind_name(*kind)) {
      r.error(t.get("payload"), "payload", "builds " + std::string(payload_info(*builder).kind) + ", not " + *emits);
      ok = false;
    }
  } else if (kind) {
    builder = payload_for_kind(kind_name(*kind));
    if (!builder) {
      const std::string_view why = waits_for(*kind);
      r.error(t.get("emits"), "emits", why.empty() ? std::string("needs its payload named")
                                                   : *emits + " is not emitted by this build: it waits for " + std::string(why));
      ok = false;
    }
  }
  if (builder) rule.payload = *builder;

  const auto cls = r.text("class", true);
  if (cls) {
    if (*cls == "P") rule.cls = Class::P;
    else if (*cls == "K") rule.cls = Class::K;
    else if (*cls == "T") rule.cls = Class::T;
    else {
      r.error(t.get("class"), "class", "must be P, K or T");
      ok = false;
    }
  }
  const auto key = r.text("key", false);
  const auto ttl = r.integer("ttl_s", 0, kMaxTtlS);
  const auto merge = r.integer("merge_ms", 0, kMaxMergeMs);
  const auto dedup = r.integer("dedup_s", 0, kMaxDedupS);
  const auto cooldown = r.integer("cooldown_s", 0, kMaxCooldownS);
  if (t.get("ttl_s") && !ttl) ok = false;
  if (t.get("merge_ms") && !merge) ok = false;
  if (t.get("dedup_s") && !dedup) ok = false;
  if (t.get("cooldown_s") && !cooldown) ok = false;
  rule.ttl_s = static_cast<std::uint32_t>(ttl.value_or(0));
  rule.merge_ms = static_cast<std::uint32_t>(merge.value_or(0));
  rule.dedup_s = static_cast<std::uint32_t>(dedup.value_or(0));
  rule.cooldown_s = static_cast<std::uint32_t>(cooldown.value_or(0));
  if (cls && rule.cls == Class::K) {
    if (!key) {
      r.error(nullptr, "key", "is missing: a K rule needs its key");
      ok = false;
    }
    if (rule.ttl_s < 1) {
      r.error(t.get("ttl_s"), "ttl_s", "must be at least 1 for a K rule");
      ok = false;
    }
  }
  if (cls && rule.cls == Class::T && (t.get("ttl_s") || t.get("merge_ms") || t.get("dedup_s"))) {
    r.error(t.get("ttl_s") ? t.get("ttl_s") : t.get("merge_ms") ? t.get("merge_ms") : t.get("dedup_s"), "",
            "a T rule takes no ttl_s, merge_ms or dedup_s");
    ok = false;
  }
  if (cls && rule.cls != Class::P && t.get("dedup_s")) {
    r.error(t.get("dedup_s"), "dedup_s", "is taken by P rules only");
    ok = false;
  }
  if (cls && rule.cls != Class::K && t.get("merge_ms")) {
    r.error(t.get("merge_ms"), "merge_ms", "is taken by K rules only");
    ok = false;
  }
  if (cls && rule.cls == Class::P && rule.dedup_s > 0 && !key) {
    r.error(nullptr, "key", "is missing: dedup_s needs a key");
    ok = false;
  }
  if (key) {
    std::string problem;
    if (key->empty() || key->size() > kMaxKeyBytes) {
      r.error(t.get("key"), "key", "must be 1 to 96 bytes");
      ok = false;
    } else if (builder && !key_ok(*key, payload_info(*builder), problem)) {
      r.error(t.get("key"), "key", problem);
      ok = false;
    }
    rule.key = *key;
  }

  if (const auto conf = r.number("confidence", false)) {
    if (!(*conf > 0 && *conf <= 1)) {
      r.error(t.get("confidence"), "confidence", "must be above 0 and at most 1");
      ok = false;
    }
    rule.confidence = static_cast<float>(*conf);
  } else if (t.get("confidence")) {
    ok = false;
  }

  if (const toml::node* a = r.get("audience")) {
    if (const auto* at = a->as_table()) {
      Table aud(*at, file.name, where + ".audience", errors);
      if (const auto radius = aud.text("radius", true)) {
        const auto it = std::find(kRadiusNames.begin(), kRadiusNames.end(), *radius);
        if (it == kRadiusNames.end()) {
          aud.error(at->get("radius"), "radius", "must be speech, witness, death, combat, none or subject");
          ok = false;
        } else {
          rule.radius = static_cast<Radius>(it - kRadiusNames.begin());
        }
      } else {
        ok = false;
      }
      aud.done();
    } else {
      r.error(a, "audience", "must be a table");
      ok = false;
    }
  }

  if (const toml::node* i = r.get("interrupts")) {
    const auto* a = i->as_array();
    if (a == nullptr) {
      r.error(i, "interrupts", "must be a list of names");
      ok = false;
    } else {
      for (const toml::node& v : *a) {
        const auto* s = v.as_string();
        const auto it = std::find_if(kInterruptNames.begin(), kInterruptNames.end(),
                                     [&](const InterruptName& in) { return s != nullptr && in.name == s->get(); });
        if (it == kInterruptNames.end()) {
          r.error(&v, "interrupts", "holds a name that is not an interrupt");
          ok = false;
        } else {
          rule.interrupts = rule.interrupts | it->bits;
        }
      }
    }
  }

  if (const auto req = r.text("requires", false)) {
    if (req->size() > kMaxRequiresBytes) {
      r.error(t.get("requires"), "requires", "is longer than 1 KB");
      ok = false;
    } else if (*req != "true") {
      auto compiled = predicates.compile(*req);
      if (compiled) {
        rule.requires_id = *compiled;
      } else {
        r.error(t.get("requires"), "requires", "does not compile: " + compiled.error());
        ok = false;
      }
    }
  } else if (t.get("requires")) {
    ok = false;
  }

  if (const auto pr = r.integer(kPriorityKey, 0, 100)) {
    rule.priority = static_cast<std::uint8_t>(*pr);
  } else if (t.get(kPriorityKey)) {
    ok = false;
  }

  r.done();
  if (!ok || !kind || !builder || !cls) return;
  rule.kind = *kind;
  out.push_back(std::move(rule));
}
}  // namespace

std::span<const PayloadInfo> payloads() noexcept { return kPayloads; }

const PayloadInfo& payload_info(PayloadId id) noexcept { return kPayloads[static_cast<std::size_t>(id)]; }

std::string_view waits_for(std::size_t kind) noexcept {
  const std::string_view k = kind_name(kind);
  if (k == "GraceRested" || k == "RestEnded" || k == "GraceLit" || k == "FlagChanged")
    return "flag edges, which need the flag catalogue and FlagWatch (v0.2)";
  if (k == "StageChanged") return "the knowledge subscriber, which emits it in code, not a rule (v0.3)";
  if (k == "IdleNear") return "cards with ambient lines (v0.4)";
  if (k == "FoeKilled") return "S7's check of last_hit_by for NPC attackers (v0.4)";
  if (k == "StatusStarted" || k == "StatusEnded" || k == "StatusBurst") return "S8's statuses (v0.4)";
  if (k == "CameNear") return "speakers to greet: personas and cards, with the pacing core (v0.4; O147)";
  if (k == "AutonomyTick") return "profiled NPCs and cards with autonomy, with the pacing core (v0.4; O147)";
  if (k == "PlayerSaid" || k == "NpcSaid" || k == "CalloutSent" || k == "CalloutReplied" ||
      k == "ConversationStarted" || k == "ConversationEnded")
    return "the Pipeline source: conversations and the call-out (v0.2 on)";
  if (k == "NativeLineHeard") return "S3's scripted-line detector (v0.2 on)";
  if (k == "NpcThought" || k == "DecisionMade") return "nothing: a reserved seam, never emitted in v1";
  return "";
}

proto::Expected<std::shared_ptr<const RuleSet>, std::vector<RuleError>> RuleSet::load(
    std::span<const RuleFile> files, const PredicateCompiler& predicates, const RuleLimits& limits) {
  std::vector<RuleError> problems;
  Errors errors(problems, limits.max_errors);
  auto set = std::make_shared<RuleSet>();
  std::uint64_t h = fnv1a64("");
  for (const RuleFile& file : files) {
    h = fnv1a64(file.name, h);
    h = fnv1a64(std::string_view("\0", 1), h);
    h = fnv1a64(file.text, h);
    h = fnv1a64(std::string_view("\0", 1), h);
    if (file.user && !limits.allow_custom) {
      errors.add(file.name, 0, "is a user rule file, and augur.allow_custom_rules is off");
      continue;
    }
    if (file.text.size() > limits.max_file_bytes) {
      errors.add(file.name, 0, "is larger than " + std::to_string(limits.max_file_bytes >> 10) + " KB");
      continue;
    }
    if (const auto unsafe = proto::unsafe_for_toml(file.text, file.user ? proto::TomlText::Player : proto::TomlText::Gal)) {
      errors.add(file.name, unsafe->line, std::string("is refused before it is parsed: ") + unsafe->problem);
      continue;
    }
    toml::parse_result parsed = toml::parse(file.text);
    if (!parsed) {
      const toml::parse_error& e = parsed.error();
      errors.add(file.name, static_cast<std::size_t>(e.source().begin.line),
                 "is not valid TOML: " + std::string(e.description()));
      continue;
    }
    const toml::table& root = parsed.table();
    for (auto&& [k, v] : root) {
      if (k.str() != "rule") errors.add(file.name, line_of(v), "has an unknown key " + shown(k.str()));
    }
    const toml::node* rules = root.get("rule");
    if (rules == nullptr) continue;  // a file with no rule is allowed
    const auto* a = rules->as_array();
    if (a == nullptr || !a->is_array_of_tables()) {
      errors.add(file.name, line_of(*rules), "rule must be an array of tables ([[rule]])");
      continue;
    }
    for (std::size_t i = 0; i < a->size(); ++i) {
      read_rule(*a->get(i)->as_table(), i, file, predicates, errors, set->rules_);
    }
  }
  // Ids are unique across the set.
  std::set<std::string> ids;
  for (const Rule& r : set->rules_) {
    if (!ids.insert(*r.id).second) errors.add(r.file, r.line, "rule id " + shown(*r.id) + " is not unique");
  }
  if (set->rules_.size() > limits.max_rules) {
    errors.add("", 0, "the rule files hold more than " + std::to_string(limits.max_rules) + " rules");
  }
  errors.finish();
  if (!problems.empty()) return proto::Unexpected{std::move(problems)};
  for (std::size_t i = 0; i < set->rules_.size(); ++i) set->rules_[i].index = i;
  set->hash_ = h;
  return std::shared_ptr<const RuleSet>(std::move(set));
}

std::string describe(const RuleError& e) {
  if (e.file.empty()) return e.problem;
  return e.file + ":" + std::to_string(e.line) + ": " + e.problem;
}

std::uint64_t fnv1a64(std::string_view bytes, std::uint64_t h) noexcept {
  for (const char c : bytes) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001b3ull;
  }
  return h;
}

std::string hash_text(std::uint64_t h) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "fnv1a64:%016llx", static_cast<unsigned long long>(h));
  return buf;
}

}  // namespace en::augur
