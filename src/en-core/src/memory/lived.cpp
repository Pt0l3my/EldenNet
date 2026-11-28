// The lived layer (lived.hpp; MEM §13).
#include "en/core/lived.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

#include "en/prompt/environment.hpp"
#include "en/prompt/messages.hpp"

namespace en::core {

namespace {

using json = nlohmann::json;

constexpr std::string_view kLivedFolder = "prompts/lived";
constexpr std::array<std::string_view, 2> kParts = {"01_frame.j2", "02_input.j2"};
constexpr std::array<std::string_view, 3> kWarm = {"fond", "grateful", "admiring"};
constexpr std::array<std::string_view, 4> kCold = {"wary", "afraid", "resentful", "scornful"};

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
  std::string t(text), w(word);
  for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (char& c : w) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (std::size_t at = t.find(w); at != std::string::npos; at = t.find(w, at + 1)) {
    const bool left = at == 0 || std::isalpha(static_cast<unsigned char>(t[at - 1])) == 0;
    const std::size_t end = at + w.size();
    if (left && (end >= t.size() || std::isalpha(static_cast<unsigned char>(t[end])) == 0)) return true;
  }
  return false;
}

bool gendered(std::string_view s) {
  for (const char* p : {"he", "she", "him", "her", "his", "hers", "himself", "herself"}) {
    if (whole(s, p)) return true;
  }
  return false;
}

json entry_json(const LivedEntry& e) {
  json o = {{"id", e.id}, {"text", e.text}, {"refs", e.refs}};
  if (!e.who.empty()) o["who"] = e.who;
  if (!e.feeling.empty()) o["feeling"] = e.feeling;
  if (!e.who.empty()) o["trust"] = e.trust;
  if (!e.replaces.empty()) o["replaces"] = e.replaces;
  return o;
}

std::optional<LivedEntry> entry_of(const json& o) {
  if (!o.is_object() || !o.contains("text") || !o["text"].is_string()) return std::nullopt;
  LivedEntry e;
  e.id = o.value("id", std::string());
  e.who = o.value("who", std::string());
  e.feeling = o.value("feeling", std::string());
  e.trust = o.value("trust", 0);
  e.replaces = o.value("replaces", std::string());
  e.text = o["text"].get<std::string>();
  if (o.contains("refs") && o["refs"].is_array()) {
    for (const json& r : o["refs"]) {
      if (r.is_string()) e.refs.push_back(r.get<std::string>());
    }
  }
  return e;
}

std::vector<LivedEntry> entries_of(const std::string& text) {
  std::vector<LivedEntry> out;
  const json a = json::parse(text, nullptr, false);
  if (!a.is_array()) return out;
  for (const json& o : a) {
    if (auto e = entry_of(o)) out.push_back(std::move(*e));
  }
  return out;
}

std::string array_text(const std::vector<LivedEntry>& v) {
  json a = json::array();
  for (const LivedEntry& e : v) a.push_back(entry_json(e));
  return a.dump();
}

}  // namespace

std::optional<Lived> lived_of(const proto::LivedVersion& v) {
  Lived l;
  l.version = v.version;
  l.journal_version = v.journal_version;
  l.as_of_pt = v.as_of_pt;
  l.towards = entries_of(v.towards);
  l.aims = entries_of(v.aims);
  l.learned = entries_of(v.learned);
  if (v.self_line) {
    if (auto e = entry_of(json::parse(*v.self_line, nullptr, false))) l.self = *e;
  }
  if (v.manner) {
    if (auto e = entry_of(json::parse(*v.manner, nullptr, false))) l.manner = *e;
  }
  l.self_pt = v.self_pt;
  l.manner_pt = v.manner_pt;
  l.next_entry = v.next_entry;
  return l;
}

proto::LivedVersion lived_version(const Lived& l, std::uint16_t journey, const std::string& persona) {
  proto::LivedVersion v;
  v.journey = journey;
  v.persona = persona;
  v.version = l.version;
  v.journal_version = l.journal_version;
  v.as_of_pt = l.as_of_pt;
  v.towards = array_text(l.towards);
  v.aims = array_text(l.aims);
  v.learned = array_text(l.learned);
  if (l.self) v.self_line = entry_json(*l.self).dump();
  if (l.manner) v.manner = entry_json(*l.manner).dump();
  v.self_pt = l.self_pt;
  v.manner_pt = l.manner_pt;
  v.next_entry = l.next_entry;
  return v;
}

void count_journal_change(LivedDue& due, const std::optional<Journal>& before, const Journal& after) {
  for (const Keepsake& k : after.keepsakes) {
    const Keepsake* old = nullptr;
    if (before) {
      for (const Keepsake& o : before->keepsakes) {
        if (o.id == k.id) old = &o;
      }
    }
    if (old == nullptr || old->text != k.text) ++due.changed;
  }
  const auto id_of = [](const Journal& j) {
    const auto p = protected_keepsake(j);
    return p ? j.keepsakes[*p].id : std::string();
  };
  if (id_of(after) != (before ? id_of(*before) : std::string())) due.protected_changed = true;
  if (after.disposition != (before ? before->disposition : 0)) due.disposition_moved = true;
}

proto::Expected<std::vector<proto::Message>, std::string> lived_messages(std::span<const ContentFile> files,
                                                                         const LivedJobInput& in,
                                                                         std::string_view reminder) {
  std::map<std::string, std::string> mine;
  for (const ContentFile& f : files) {
    if (f.path.rfind(std::string(kLivedFolder) + "/", 0) == 0) mine[f.path] = f.text;
  }
  for (const std::string_view part : kParts) {
    if (!mine.contains(std::string(kLivedFolder) + "/" + std::string(part))) {
      return proto::Unexpected{"the pack has no " + std::string(kLivedFolder) + "/" + std::string(part)};
    }
  }
  json turned = json::array();
  if (in.lived) {
    const auto add = [&](const LivedEntry& e, std::string part) {
      std::string refs;
      for (const std::string& r : e.refs) refs += (refs.empty() ? "" : ", ") + r;
      std::string label = part;
      if (!e.feeling.empty()) label += ", " + e.feeling;
      turned.push_back({{"id", e.id}, {"label", label}, {"text", e.text}, {"refs", refs}});
    };
    for (const LivedEntry& e : in.lived->towards) add(e, "towards " + std::string(e.who == "tarnished" ? "P1" : e.who));
    for (const LivedEntry& e : in.lived->aims) add(e, "aim");
    for (const LivedEntry& e : in.lived->learned) add(e, "learned");
    if (in.lived->self) add(*in.lived->self, "self");
    if (in.lived->manner) add(*in.lived->manner, "manner");
  }
  json memories = json::array();
  const auto prot = protected_keepsake(in.journal);
  for (std::size_t i = 0; i < in.journal.keepsakes.size(); ++i) {
    const Keepsake& k = in.journal.keepsakes[i];
    char w[8];
    std::snprintf(w, sizeof w, "%.2f", k.weight);
    std::string label = std::string(w) + ", " + (k.basis == Keepsake::Basis::Seen ? "seen" : "said");
    if (k.check_key && in.proved && in.proved(*k.check_key)) label += ", proved";
    if (k.about == "tarnished") label += ", with P1";
    if (prot && *prot == i) label += ", keep";
    memories.push_back({{"id", k.id}, {"label", label}, {"text", k.text}});
  }
  json people = json::array();
  for (const auto& [letter, label] : in.people) people.push_back({{"letter", letter}, {"label", label}});
  const json context = {{"name", in.name},
                        {"self_text", in.self_text},
                        {"road", in.road},
                        {"turned", turned},
                        {"summary", in.journal.summary},
                        {"memories", memories},
                        {"facts", in.facts},
                        {"feeling", in.feeling_words},
                        {"people", people},
                        {"reminder", std::string(reminder)}};
  try {
    const prompt::Environment env(std::make_shared<const prompt::MemorySource>(std::move(mine)));
    std::vector<proto::Message> out;
    for (const std::string_view part : kParts) {
      for (const prompt::Message& m : env.render(std::string(kLivedFolder) + "/" + std::string(part), context)) {
        proto::Message msg;
        msg.role = m.role == prompt::Role::system ? proto::Role::System : proto::Role::User;
        for (const prompt::Part& pp : m.parts) msg.parts.push_back(proto::Part{pp.text, pp.cache});
        proto::normalise(msg);
        out.push_back(std::move(msg));
      }
    }
    return out;
  } catch (const std::exception& e) {
    return proto::Unexpected{std::string("the lived prompt could not be made: ") + e.what()};
  }
}

json lived_schema() {
  const json refs = {{"type", "array"}, {"maxItems", 4}, {"items", {{"type", "string"}, {"pattern", "^k[0-9]{1,4}$"}}}};
  const json id = {{"type", json::array({"string", "null"})}};
  const json text = {{"type", "string"}, {"maxLength", 200}};
  const json towards = {{"type", "object"},
                        {"additionalProperties", false},
                        {"required", json::array({"id", "who", "feeling", "trust_delta", "text", "refs"})},
                        {"properties",
                         {{"id", id},
                          {"who", {{"type", "string"}, {"pattern", "^P[0-9]{1,2}$"}}},
                          {"feeling",
                           {{"enum", json::array({"fond", "grateful", "admiring", "curious", "indifferent", "wary",
                                                  "afraid", "resentful", "scornful"})}}},
                          {"trust_delta", {{"enum", json::array({-1, 0, 1})}}},
                          {"text", text},
                          {"refs", refs}}}};
  const json line = {{"type", "object"},
                     {"additionalProperties", false},
                     {"required", json::array({"id", "text", "refs"})},
                     {"properties", {{"id", id}, {"replaces", id}, {"text", text}, {"refs", refs}}}};
  const json single = {{"type", json::array({"object", "null"})},
                       {"additionalProperties", false},
                       {"properties", {{"text", text}, {"refs", refs}}}};
  return {{"type", "object"},
          {"additionalProperties", false},
          {"required", json::array({"towards", "aims", "learned", "self", "manner"})},
          {"properties",
           {{"towards", {{"type", "array"}, {"maxItems", 4}, {"items", towards}}},
            {"aims", {{"type", "array"}, {"maxItems", 2}, {"items", line}}},
            {"learned", {{"type", "array"}, {"maxItems", 2}, {"items", line}}},
            {"self", single},
            {"manner", single}}}};
}

proto::Expected<Lived, JobFailure> validate_lived(std::string_view text, std::string_view finish,
                                                  const LivedJobInput& in, LivedCheck& check) {
  // 1. Parse and clean.
  if (finish == "length") return proto::Unexpected{JobFailure::Truncated};
  if (text.empty()) return proto::Unexpected{JobFailure::Empty};
  if (text.size() > 4096) return proto::Unexpected{JobFailure::Truncated};
  json o = json::parse(text, nullptr, false);
  if (!o.is_object()) {
    const auto a = text.find('{');
    const auto b = text.rfind('}');
    if (a != std::string_view::npos && b != std::string_view::npos && b > a) {
      o = json::parse(text.substr(a, b - a + 1), nullptr, false);
    }
  }
  if (!o.is_object()) return proto::Unexpected{JobFailure::Parse};
  for (const char* k : {"towards", "aims", "learned", "self", "manner"}) {
    if (!o.contains(k)) return proto::Unexpected{JobFailure::Parse};
  }
  Lived l = in.lived.value_or(Lived{});
  l.version += 1;
  l.journal_version = in.journal.version;
  l.as_of_pt = in.journal.as_of_pt;
  std::map<std::string, const Keepsake*> memories;
  for (const Keepsake& k : in.journal.keepsakes) memories[k.id] = &k;
  // 2. Citations: the journal's ids only; an entry left with none is dropped and the old one kept.
  const auto cited = [&](const json& e) {
    std::vector<const Keepsake*> out;
    if (!e.contains("refs") || !e["refs"].is_array()) return out;
    for (const json& r : e["refs"]) {
      if (!r.is_string()) continue;
      const auto it = memories.find(r.get<std::string>());
      if (it != memories.end()) out.push_back(it->second);
    }
    return out;
  };
  const auto refs_of = [](const std::vector<const Keepsake*>& v) {
    std::vector<std::string> out;
    for (const Keepsake* k : v) out.push_back(k->id);
    return out;
  };
  // 5's text caps: over by up to a quarter, cut at a clause boundary; more, dropped.
  const auto capped = [&](std::string t, std::size_t cap) -> std::optional<std::string> {
    if (t.empty() || words(t) > cap + cap / 4) return std::nullopt;
    if (words(t) > cap) {
      const auto comma = t.rfind(',');
      if (comma == std::string::npos) return std::nullopt;
      t = t.substr(0, comma) + ".";
    }
    return t;
  };
  const auto drop = [&] { ++check.dropped; };
  const auto next_id = [&](char prefix) { return std::string(1, prefix) + std::to_string(l.next_entry++); };
  // towards: the moderate tier, at most 2 changes; `who` a person among a cited memory's actors.
  std::size_t changes = 0;
  for (const json& e : o["towards"].is_array() ? o["towards"] : json::array()) {
    if (changes >= 2 || !e.is_object()) {
      drop();
      continue;
    }
    const std::vector<const Keepsake*> c = cited(e);
    const std::string letter = e.value("who", std::string());
    const std::string who = letter == "P1" ? "tarnished" : std::string();
    const bool among = std::any_of(c.begin(), c.end(), [&](const Keepsake* k) { return k->about == who; });
    auto t = capped(e.value("text", std::string()), 20);
    const std::string feeling = e.value("feeling", std::string());
    if (c.empty() || who.empty() || !among || !t || (who == "tarnished" && gendered(*t))) {
      drop();
      continue;
    }
    // 4. Agreement: a warm feeling needs trust ≥ 0 and a cold one ≤ 0; the Tarnished's trust is the disposition.
    const auto existing =
        std::find_if(l.towards.begin(), l.towards.end(), [&](const LivedEntry& x) { return x.who == who; });
    int trust = who == "tarnished" ? in.journal.disposition : (existing != l.towards.end() ? existing->trust : 0);
    if (who != "tarnished") trust = std::clamp(trust + std::clamp(e.value("trust_delta", 0), -1, 1), -2, 2);
    const bool warm = std::find(kWarm.begin(), kWarm.end(), feeling) != kWarm.end();
    const bool cold = std::find(kCold.begin(), kCold.end(), feeling) != kCold.end();
    if ((warm && trust < 0) || (cold && trust > 0)) {
      drop();
      continue;
    }
    LivedEntry n{existing != l.towards.end() ? existing->id : next_id('t'), who, feeling, trust, {}, *t, refs_of(c)};
    if (existing != l.towards.end()) {
      *existing = std::move(n);
    } else if (l.towards.size() < 4) {
      l.towards.push_back(std::move(n));
    } else {
      drop();
      continue;
    }
    ++changes;
  }
  // aims and learned: the moderate tier, 2 cited memories, one change each; learned needs a seen or proved memory.
  const auto line_part = [&](const char* key, std::vector<LivedEntry>& part, char prefix, std::size_t cap,
                             bool learned) {
    std::size_t done = 0;
    for (const json& e : o[key].is_array() ? o[key] : json::array()) {
      if (done >= 1 || !e.is_object()) {
        drop();
        continue;
      }
      const std::vector<const Keepsake*> c = cited(e);
      auto t = capped(e.value("text", std::string()), cap);
      const bool grounded = std::any_of(c.begin(), c.end(), [&](const Keepsake* k) {
        return k->basis == Keepsake::Basis::Seen || (k->check_key && in.proved && in.proved(*k->check_key));
      });
      std::string replaces =
          e.contains("replaces") && e["replaces"].is_string() ? e["replaces"].get<std::string>() : "";
      if (!replaces.empty() && std::find(in.seeks.begin(), in.seeks.end(), replaces) == in.seeks.end())
        replaces.clear();
      if (c.size() < 2 || !t || gendered(*t) || (learned && !grounded)) {
        drop();
        continue;
      }
      const std::string id = e.contains("id") && e["id"].is_string() ? e["id"].get<std::string>() : std::string();
      const auto existing = std::find_if(part.begin(), part.end(), [&](const LivedEntry& x) { return x.id == id; });
      LivedEntry n{existing != part.end() ? existing->id : next_id(prefix), {}, {}, 0, replaces, *t, refs_of(c)};
      if (existing != part.end()) {
        *existing = std::move(n);
      } else if (part.size() < 2) {
        part.push_back(std::move(n));
      } else {
        drop();
        continue;
      }
      ++done;
    }
  };
  line_part("aims", l.aims, 'a', 15, false);
  line_part("learned", l.learned, 'l', 20, true);
  // self (rare) and manner (sustained pattern): three cited memories from different scenes, and their clocks.
  const auto scenes = [](const std::vector<const Keepsake*>& c) {
    std::set<proto::Pt> s;
    for (const Keepsake* k : c) s.insert(k->since_pt);
    return s.size();
  };
  if (o["self"].is_object()) {
    std::vector<const Keepsake*> c = cited(o["self"]);
    std::erase_if(c, [](const Keepsake* k) { return k->weight < 0.7 - 1e-9; });
    auto t = capped(o["self"].value("text", std::string()), 20);
    const bool clock = !l.self_pt || check.now - *l.self_pt >= kSelfEveryMs;
    if (c.size() >= 3 && scenes(c) >= 2 && clock && t && !gendered(*t)) {
      l.self = LivedEntry{"s", {}, {}, 0, {}, *t, refs_of(c)};
      l.self_pt = check.now;
    } else {
      drop();
    }
  }
  if (o["manner"].is_object()) {
    std::vector<const Keepsake*> c = cited(o["manner"]);
    std::erase_if(c, [](const Keepsake* k) { return k->about != "tarnished"; });
    auto t = capped(o["manner"].value("text", std::string()), 15);
    const bool clock = !l.manner_pt || check.now - *l.manner_pt >= kMannerEveryMs;
    if (c.size() >= 3 && scenes(c) >= 3 && clock && t && !gendered(*t)) {
      l.manner = LivedEntry{"m", {}, {}, 0, {}, *t, refs_of(c)};
      l.manner_pt = check.now;
    } else {
      drop();
    }
  }
  return l;
}

std::vector<std::string> lived_lines(const Lived& l) {
  std::vector<std::string> out;
  std::size_t bytes = 0;
  const auto add = [&](const std::string& s) {
    if (estimate_tokens(bytes + s.size() + 1) > kLivedTokens) return;
    bytes += s.size() + 1;
    out.push_back(s);
  };
  for (const LivedEntry& e : l.towards) {
    if (e.who == "tarnished") add("Of the Tarnished: \"" + e.text + "\"");
  }
  for (const LivedEntry& e : l.towards) {
    if (e.who != "tarnished") add("\"" + e.text + "\"");
  }
  for (const LivedEntry& e : l.aims) add("\"" + e.text + "\"");
  for (const LivedEntry& e : l.learned) add("\"" + e.text + "\"");
  if (l.self) add("\"" + l.self->text + "\"");
  if (l.manner) add("\"" + l.manner->text + "\"");
  return out;
}

}  // namespace en::core
