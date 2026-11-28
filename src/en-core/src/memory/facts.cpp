// MEM §2.2's fold (facts.hpp).
#include "en/core/facts.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <nlohmann/json.hpp>

#include "en/augur/audience.hpp"

namespace en::core {

namespace {

using json = nlohmann::json;
using proto::FactRow;
using proto::How;
using proto::PersonaFacts;
using proto::StoredEvent;

std::string number(std::int64_t v) {
  char buf[24];
  const auto r = std::to_chars(buf, buf + sizeof buf, v);
  return std::string(buf, r.ptr);
}

json payload_of(const StoredEvent& ev) {
  json j = json::parse(ev.payload, nullptr, false);
  return j.is_object() ? j : json::object();
}

std::int64_t int_of(const json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_number_integer() ? it->get<std::int64_t>() : 0;
}

std::string text_of(const json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

class Facts {
 public:
  Facts(PersonaFacts& f, const StoredEvent& ev) : f_(f), ev_(ev) {}

  [[nodiscard]] std::optional<std::int64_t> num(std::string_view key) const { return fact_number(f_, key); }
  void set(std::string_view key, std::string value) {
    auto it = std::lower_bound(f_.facts.begin(), f_.facts.end(), key,
                               [](const FactRow& r, std::string_view k) { return r.key < k; });
    if (it != f_.facts.end() && it->key == key) {
      if (it->value == value) return;
      it->value = std::move(value);
      it->src_event = ev_.id;
      it->updated_pt = ev_.t_play_ms;
    } else {
      f_.facts.insert(it, FactRow{std::string(key), std::move(value), ev_.id, ev_.t_play_ms});
    }
    changed_.emplace_back(key);
  }
  void set(std::string_view key, std::int64_t v) { set(key, number(v)); }
  void add(std::string_view key) { set(key, num(key).value_or(0) + 1); }
  std::vector<std::string> changed() && { return std::move(changed_); }

 private:
  PersonaFacts& f_;
  const StoredEvent& ev_;
  std::vector<std::string> changed_;
};

// A meeting: one visit, merged with the previous within 10 min of play.
void meeting(Facts& f, const StoredEvent& ev, const json& p) {
  const auto last = f.num(fact::kLastMetPt);
  const bool merged = last && ev.t_play_ms >= *last && ev.t_play_ms - *last <= kMeetingMergeMs;
  if (!merged) f.add(fact::kMetCount);
  if (!f.num(fact::kFirstMetPt)) {
    f.set(fact::kFirstMetPt, ev.t_play_ms);
    if (ev.place_id) f.set(fact::kFirstMetPlace, *ev.place_id);
  }
  f.set(fact::kLastMetPt, ev.t_play_ms);
  if (ev.place_id) f.set(fact::kLastMetPlace, *ev.place_id);
  if (const std::string stage = text_of(p, "stage"); !stage.empty()) f.set(fact::kStageSeen, stage);
}

// `world_seen`: the union of the meetings' `known`, keeping the higher certainty (MEM §12.7).
void seen_world(Facts& f, const PersonaFacts& facts, const StoredEvent& ev, const json& known) {
  std::map<std::string, std::string> items;
  for (auto& [item, word] : fact_pairs(facts, fact::kWorldSeen)) items[item] = word;
  for (const json& pair : known) {
    if (!pair.is_array() || pair.size() != 2 || !pair[0].is_string() || !pair[1].is_string()) continue;
    const std::string item = pair[0].get<std::string>();
    const std::string word = pair[1].get<std::string>();
    auto it = items.find(item);
    if (it == items.end() || certainty_rank(word) > certainty_rank(it->second)) items[item] = word;
  }
  json out = json::array();
  for (const auto& [item, word] : items) out.push_back(json::array({item, word}));
  f.set(fact::kWorldSeen, out.dump());
  f.set(fact::kWorldSeenPt, ev.t_play_ms);
}

void cued(Facts& f, const PersonaFacts& facts, const StoredEvent& ev, const std::string& item) {
  std::map<std::string, std::string> items;
  for (auto& [i, pt] : fact_pairs(facts, fact::kWorldCued)) items[i] = pt;
  items[item] = number(ev.t_play_ms);
  json out = json::array();
  for (const auto& [i, pt] : items) {
    std::int64_t v = 0;
    std::from_chars(pt.data(), pt.data() + pt.size(), v);
    out.push_back(json::array({i, v}));
  }
  f.set(fact::kWorldCued, out.dump());
}

}  // namespace

int certainty_rank(std::string_view word) noexcept {
  if (word == "fact") return 3;
  if (word == "rumour") return 2;
  if (word == "belief") return 1;
  return 0;
}

const FactRow* find_fact(const PersonaFacts& facts, std::string_view key) noexcept {
  for (const FactRow& r : facts.facts) {
    if (r.key == key) return &r;
  }
  return nullptr;
}

std::optional<std::int64_t> fact_number(const PersonaFacts& facts, std::string_view key) noexcept {
  const FactRow* r = find_fact(facts, key);
  if (r == nullptr) return std::nullopt;
  std::int64_t v = 0;
  const auto res = std::from_chars(r->value.data(), r->value.data() + r->value.size(), v);
  if (res.ec != std::errc{} || res.ptr != r->value.data() + r->value.size()) return std::nullopt;
  return v;
}

std::vector<std::pair<std::string, std::string>> fact_pairs(const PersonaFacts& facts, std::string_view key) {
  std::vector<std::pair<std::string, std::string>> out;
  const FactRow* r = find_fact(facts, key);
  if (r == nullptr) return out;
  const json j = json::parse(r->value, nullptr, false);
  if (!j.is_array()) return out;
  for (const json& pair : j) {
    if (!pair.is_array() || pair.size() != 2 || !pair[0].is_string()) continue;
    if (pair[1].is_string()) {
      out.emplace_back(pair[0].get<std::string>(), pair[1].get<std::string>());
    } else if (pair[1].is_number_integer()) {
      out.emplace_back(pair[0].get<std::string>(), number(pair[1].get<std::int64_t>()));
    }
  }
  return out;
}

std::vector<std::string> fold_fact(PersonaFacts& facts, const StoredEvent& ev, How how) {
  Facts f(facts, ev);
  const std::string self = "persona:" + facts.persona;
  if (how == How::Direct) {
    if (ev.kind == "ConversationEnded" && ev.subject == self) {
      const json p = payload_of(ev);
      // A meeting needs a line of the persona's committed: the conversation's lines less the Tarnished's.
      if (int_of(p, "committed") - int_of(p, "player_lines") >= 1) {
        meeting(f, ev, p);
        if (const auto k = p.find("known"); k != p.end() && k->is_array()) seen_world(f, facts, ev, *k);
      }
    } else if (ev.kind == "NativeTalkEnded" && ev.subject == self) {
      meeting(f, ev, payload_of(ev));
    } else if (ev.kind == "PersonaSeen" && ev.subject == self) {
      f.add(fact::kSeenCount);
    } else if (ev.kind == "PersonaAttacked" && ev.subject == self) {  // the victim; the Tarnished is the object
      f.add(fact::kTimesAttacked);
      f.set(fact::kLastAttackedPt, ev.t_play_ms);
    } else if (ev.kind == "FoughtBeside" && ev.subject == self) {
      f.add(fact::kFoughtBeside);
      const json p = payload_of(ev);
      if (const auto b = p.find("boss"); b != p.end() && b->is_number_integer()) {
        f.set(fact::kFoughtLastBoss, b->get<std::int64_t>());
      }
    } else if (ev.kind == "NpcSaid" && ev.subject == self) {
      const json p = payload_of(ev);
      if (const std::string cue = text_of(p, "cue"); !cue.empty()) cued(f, facts, ev, cue);
    }
  } else if (how == How::Witnessed && ev.kind == "PlayerDied") {
    f.add(fact::kSeenPlayerDie);
  }
  return std::move(f).changed();
}

std::vector<PersonaFacts> fold_all(const std::vector<StoredEvent>& events) {
  std::map<std::string, PersonaFacts> by;
  std::vector<const StoredEvent*> order;
  order.reserve(events.size());
  for (const StoredEvent& e : events) order.push_back(&e);
  std::stable_sort(order.begin(), order.end(),
                   [](const StoredEvent* a, const StoredEvent* b) { return a->id < b->id; });
  for (const StoredEvent* e : order) {
    for (const auto& [persona, how] : augur::perceivers(*e, augur::VisCtx{e->journey})) {
      PersonaFacts& f = by[persona];
      f.persona = persona;
      (void)fold_fact(f, *e, how);
    }
  }
  std::vector<PersonaFacts> out;
  for (auto& [id, f] : by) {
    if (!f.facts.empty()) out.push_back(std::move(f));
  }
  return out;
}

}  // namespace en::core
