// EldenNet's memory pages (memory.hpp; PAGES §4.6.1, §4.6.4, §4.6.5, SP1 to SP9).
#include "en/overlay/memory.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>

namespace en::overlay {

using nlohmann::json;

namespace {


std::optional<std::uint64_t> digits(std::string_view s) {
  if (s.empty() || s.size() > 19) return std::nullopt;
  std::uint64_t v = 0;
  const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc() || p != s.data() + s.size()) return std::nullopt;
  return v;
}

// A family's member: "<family>/<member>".
std::optional<std::string_view> member_of(std::string_view topic, std::string_view family) {
  if (topic.size() <= family.size() + 1 || topic.substr(0, family.size()) != family || topic[family.size()] != '/') {
    return std::nullopt;
  }
  return topic.substr(family.size() + 1);
}

std::string key_of(std::uint16_t journey, const std::string& persona) {
  return std::to_string(journey) + "." + persona;
}

std::string times(std::uint32_t n) { return n == 1 ? "once" : std::to_string(n) + " times"; }

// A wall time as "2026-11-27 14:05 UTC" (a backup's), from the days since the epoch (the civil calendar's rule).
std::string utc_words(std::int64_t wall) {
  const std::int64_t days = wall >= 0 ? wall / 86400 : (wall - 86399) / 86400;
  const std::int64_t secs = wall - days * 86400;
  std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t m = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t y = yoe + era * 400 + (m <= 2 ? 1 : 0);
  char buf[96];
  std::snprintf(buf, sizeof buf, "%04lld-%02lld-%02lld %02lld:%02lld UTC", static_cast<long long>(y),
                static_cast<long long>(m), static_cast<long long>(d), static_cast<long long>(secs / 3600),
                static_cast<long long>(secs % 3600 / 60));
  return buf;
}

std::string_view status_word(const MemberFacts& m) {
  if (m.here) return "Here now";
  if (m.fell) return "Fell";
  return "Alive when last met";
}

std::string_view end_word(std::string_view r) {
  if (r.empty()) return "Still open";
  if (r == "walked_away") return "You walked away";
  if (r == "danger") return "Danger came";
  if (r == "travel") return "You travelled";
  if (r == "death") return "You fell";
  if (r == "idle") return "It went quiet";
  if (r == "crash") return "The game stopped";
  if (r == "trimmed") return "An earlier save was loaded";
  if (r == "retarget") return "You turned to someone else";
  if (r == "closed") return "You ended it";
  return "It ended";
}

json table(json columns, json rows) { return json{{"columns", std::move(columns)}, {"rows", std::move(rows)}}; }
json column(std::string_view id, std::string_view label) { return json{{"id", id}, {"label", label}}; }

const MemberFacts* member(const MemoryFacts* f, std::string_view key) {
  if (f == nullptr) return nullptr;
  const auto k = MemoryPages::member_key(key);
  if (!k) return nullptr;
  const JourneyFacts* j = f->journey(k->first);
  if (j == nullptr) return nullptr;
  for (const MemberFacts& m : j->members) {
    if (m.persona == k->second) return &m;
  }
  return nullptr;
}

json status_items(const MemberFacts& m) {
  json items = json::array();
  const auto item = [&](std::string_view label, std::string value) {
    if (!value.empty()) items.push_back(json{{"label", label}, {"value", std::move(value)}});
  };
  item("Status", std::string(status_word(m)));
  if (m.last_met_pt) item("Last met", "At " + play_clock(*m.last_met_pt) + " of play");
  if (m.first_met_pt) item("First met", "At " + play_clock(*m.first_met_pt) + " of play");
  if (m.met != 0) item("Times met", std::to_string(m.met));
  if (m.journal) item("Disposition", std::string(disposition_word(m.disposition)));
  // MEM §2.2's rendered facts alone (SP1), "fought beside you" without the boss (SP4).
  if (m.seen != 0) item("Seen you pass by", times(m.seen));
  if (m.fought_beside != 0) item("Fought beside you", times(m.fought_beside));
  if (m.saw_fall != 0) item("Saw you fall", times(m.saw_fall));
  if (m.struck != 0) item("Times you struck them", std::to_string(m.struck));
  return items;
}

}  // namespace

const JourneyFacts* MemoryFacts::journey(std::uint16_t j) const noexcept {
  for (const JourneyFacts& f : journeys) {
    if (f.journey == j) return &f;
  }
  return nullptr;
}

std::string_view disposition_word(int d) noexcept {
  switch (d) {
    case -2: return "Hostile";
    case -1: return "Wary";
    case 1: return "Warm";
    case 2: return "Fond";
    default: return "Neutral";
  }
}

std::string play_clock(std::uint64_t ms) {
  const std::uint64_t minutes = ms / 60000;
  const std::uint64_t m = minutes % 60;
  return std::to_string(minutes / 60) + ":" + (m < 10 ? "0" : "") + std::to_string(m);
}

std::optional<std::pair<std::uint16_t, std::string>> MemoryPages::member_key(std::string_view key) {
  const std::size_t dot = key.find('.');
  if (dot == std::string_view::npos || dot + 1 >= key.size()) return std::nullopt;
  const auto j = digits(key.substr(0, dot));
  if (!j || *j == 0 || *j > 0xFFFF) return std::nullopt;
  const std::string_view persona = key.substr(dot + 1);
  for (const char c : persona) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return std::nullopt;
  }
  return std::pair<std::uint16_t, std::string>{static_cast<std::uint16_t>(*j), std::string(persona)};
}

bool MemoryPages::owns(std::string_view topic) noexcept {
  if (topic == kJourneysTopic || topic == kMemoryStateTopic || topic == kNearbyTopic) return true;
  for (const std::string_view f : {kPlaythroughFamily, kMetFamily, kConversationsFamily, kConversationFamily,
                                   kMemoriesFamily, kJournalFamily}) {
    if (member_of(topic, f)) return true;
  }
  return false;
}

json MemoryPages::body(std::string_view topic, const MemoryFacts* f) {
  const bool loaded = f != nullptr && f->loaded;
  if (topic == kJourneysTopic) {
    json choices = json::array();
    if (loaded) {
      for (const JourneyFacts& j : f->journeys) {
        json c = {{"id", std::to_string(j.journey)}, {"label", "Journey " + std::to_string(j.journey)}};
        if (!j.current) c["read_only"] = true;  // an earlier journey: read only (D254)
        choices.push_back(std::move(c));
      }
      return json{{"choices", std::move(choices)}, {"current", std::to_string(f->current)}};
    }
    return json{{"choices", std::move(choices)}};
  }
  if (topic == kMemoryStateTopic) {
    json items = json::array();
    if (loaded) {
      items.push_back(json{{"label", "Backups"}, {"value", std::to_string(f->backups)}});
      std::size_t n = 0;
      for (const std::int64_t w : f->backup_walls) {
        items.push_back(json{{"label", n == 0 ? "Newest" : "Then"}, {"value", utc_words(w)}});
        if (++n == 3) break;
      }
      items.push_back(json{{"label", "What a backup holds"},
                           {"value", "Each holds what was remembered when it was taken, the forgotten memories "
                                     "included, until three newer backups replace it."}});
    }
    return json{{"items", std::move(items)}, {"can_forget", loaded && f->can_forget}};
  }
  if (topic == kNearbyTopic) {
    json rows = json::array();
    if (loaded) {
      if (const JourneyFacts* j = f->journey(f->current)) {
        for (const MemberFacts& m : j->members) {
          if (!m.here || m.met == 0) continue;  // SP7: perceived now; met on this journey (SP4)
          json r = {{"id", key_of(j->journey, m.persona)}, {"name", m.name},
                    {"state", m.speaking ? "Speaking" : "Here"}};
          if (m.journal) r["disposition"] = disposition_word(m.disposition);
          rows.push_back(std::move(r));
        }
      }
    }
    return table(json::array({column("name", "Name"), column("state", "Now"), column("disposition", "Disposition")}),
                 std::move(rows));
  }
  if (const auto j = member_of(topic, kPlaythroughFamily)) {
    json rows = json::array();
    const auto n = digits(*j);
    const JourneyFacts* jf = loaded && n ? f->journey(static_cast<std::uint16_t>(*n)) : nullptr;
    if (jf == nullptr) return json::object();  // no such journey: absence is silent
    {
      for (const MemberFacts& m : jf->members) {
        if (m.met == 0) continue;  // only people met on this journey (SP4, SP5)
        json r = {{"id", key_of(jf->journey, m.persona)}, {"name", m.name}, {"status", status_word(m)},
                  {"met", std::to_string(m.met)}};
        if (m.last_met_pt) r["last_met"] = play_clock(*m.last_met_pt);
        rows.push_back(std::move(r));
      }
    }
    return table(json::array({column("name", "Name"), column("status", "Status"), column("last_met", "Last met"),
                              column("met", "Times met")}),
                 std::move(rows));
  }
  if (const auto key = member_of(topic, kMetFamily)) {
    const MemberFacts* m = loaded ? member(f, *key) : nullptr;
    if (m == nullptr || m->met == 0) return json::object();
    return json{{"items", status_items(*m)}};
  }
  if (const auto j = member_of(topic, kConversationsFamily)) {
    json rows = json::array();
    const auto n = digits(*j);
    const JourneyFacts* jf = loaded && n ? f->journey(static_cast<std::uint16_t>(*n)) : nullptr;
    if (jf == nullptr) return json::object();
    {
      for (const ConversationFacts& c : jf->conversations) {
        rows.push_back(json{{"id", std::to_string(c.id)}, {"with", c.name}, {"when", play_clock(c.started_pt)},
                            {"lines", std::to_string(c.lines)}, {"ended", end_word(c.end_reason)}});
      }
    }
    return table(json::array({column("with", "With"), column("when", "When"), column("lines", "Lines"),
                              column("ended", "How it ended")}),
                 std::move(rows));
  }
  if (const auto id = member_of(topic, kConversationFamily)) {
    const auto n = digits(*id);
    if (!loaded || !n) return json::object();
    const auto it = f->transcripts.find(*n);
    if (it == f->transcripts.end()) return json::object();  // not read yet: absence is silent
    json rows = json::array();
    std::size_t i = 0;
    for (const LineFacts& l : it->second) {
      rows.push_back(json{{"id", std::to_string(++i)}, {"who", l.tarnished ? "You" : l.speaker}, {"line", l.text},
                          {"when", play_clock(l.pt)}});
    }
    return table(json::array({column("who", "Who"), column("line", "Line"), column("when", "When")}),
                 std::move(rows));
  }
  if (const auto j = member_of(topic, kMemoriesFamily)) {
    json rows = json::array();
    const auto n = digits(*j);
    const JourneyFacts* jf = loaded && n ? f->journey(static_cast<std::uint16_t>(*n)) : nullptr;
    if (jf == nullptr) return json::object();
    {
      for (const MemberFacts& m : jf->members) {
        if (m.met == 0 && !m.journal) continue;
        json r = {{"id", key_of(jf->journey, m.persona)}, {"name", m.name},
                  {"keepsakes", std::to_string(m.keepsakes.size())}};
        r["written"] = m.written_pt ? play_clock(*m.written_pt) : std::string("Not yet");
        rows.push_back(std::move(r));
      }
    }
    return table(json::array({column("name", "NPC"), column("keepsakes", "Keepsakes"),
                              column("written", "Journal written")}),
                 std::move(rows));
  }
  if (const auto key = member_of(topic, kJournalFamily)) {
    const MemberFacts* m = loaded ? member(f, *key) : nullptr;
    if (m == nullptr) return json::object();
    json card = json::array();
    const auto item = [&](std::string_view label, std::string value) {
      if (!value.empty()) card.push_back(json{{"label", label}, {"value", std::move(value)}});
    };
    item("Their journal", m->journal ? m->summary : std::string("Nothing written yet."));
    if (m->journal) item("Disposition", std::string(disposition_word(m->disposition)));
    if (!m->told_name.empty()) item("The name you told them", m->told_name);
    if (m->written_pt) item("Written", "At " + play_clock(*m->written_pt) + " of play");
    for (json& s : status_items(*m)) {
      if (s["label"] != "Status" && s["label"] != "Disposition") card.push_back(std::move(s));
    }
    json keepsakes = json::array();
    std::size_t i = 0;
    for (const KeepsakeFacts& k : m->keepsakes) {
      const int w = static_cast<int>(k.weight * 10 + 0.5);
      keepsakes.push_back(json{{"id", std::to_string(++i)}, {"when", play_clock(k.pt)}, {"keepsake", k.text},
                               {"weight", std::to_string(w / 10) + "." + std::to_string(w % 10)},
                               {"basis", k.told ? "You told them" : "Seen"}, {"about", k.about},
                               {"kept", k.kept ? "Kept" : ""}});
    }
    std::string work = "None waiting";
    if (m->running != 0) {
      work = "Being written now";
    } else if (m->queued != 0) {
      work = m->queued == 1 ? "One waiting" : std::to_string(m->queued) + " waiting";
    }
    if (!m->last_job.empty() && m->running == 0) work += "; the last: " + m->last_job;
    json forget = json::array({json{{"id", *key}, {"what", "Everything " + m->name + " remembers of this journey: "
                                                           "their facts, journal, keepsakes and conversations."}}});
    return json{{"card", json{{"items", std::move(card)}}},
                {"keepsakes", table(json::array({column("when", "When"), column("keepsake", "Keepsake"),
                                                 column("weight", "Weight"), column("basis", "Basis"),
                                                 column("about", "About"), column("kept", "Protected")}),
                                    std::move(keepsakes))},
                {"work", json{{"items", json::array({json{{"label", "Memory work"}, {"value", work}}})}}},
                {"forget", table(json::array({column("what", "What goes")}), std::move(forget))}};
  }
  return json::object();
}

std::string MemoryPages::command(std::string_view name, const json& args, const MemoryFacts* f) {
  if (!args.is_object() || args.value("confirm", false) != true) return "confirm_required";
  if (f == nullptr || !f->loaded) return "not_now";
  if (!f->can_forget) return "busy";
  if (name == kForgetJourney) {
    requests_.push_back(MemoryRequest{MemoryRequest::Kind::ForgetJourney, f->current, {}, 0});
    return {};
  }
  const auto key = args.find("key");
  if (key == args.end() || !key->is_string()) return "invalid_value";
  const auto k = member_key(key->get<std::string>());
  if (!k) return "invalid_value";
  if (k->first != f->current) return "read_only";  // an earlier journey's
  if (member(f, key->get<std::string>()) == nullptr) return "not_found";
  requests_.push_back(MemoryRequest{MemoryRequest::Kind::ForgetNpc, k->first, k->second, 0});
  return {};
}

void MemoryPages::want(const std::set<std::string>& watched, const MemoryFacts* f) {
  if (f == nullptr || !f->loaded) {
    asked_.clear();
    return;
  }
  for (const std::string& topic : watched) {
    const auto id = member_of(topic, kConversationFamily);
    const auto n = id ? digits(*id) : std::nullopt;
    if (!n || f->transcripts.count(*n) != 0 || !asked_.insert(*n).second) continue;
    requests_.push_back(MemoryRequest{MemoryRequest::Kind::Transcript, f->current, {}, *n});
  }
}

std::vector<MemoryRequest> MemoryPages::take_requests() {
  std::vector<MemoryRequest> out = std::move(requests_);
  requests_.clear();
  return out;
}

}  // namespace en::overlay
