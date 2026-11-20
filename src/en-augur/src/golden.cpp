// The golden files' form of an event (golden.hpp; ARCH §8.12).
#include "en/augur/golden.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <string_view>

namespace en::augur {

namespace {

using J = nlohmann::ordered_json;

double r3(double v) { return std::round(v * 1000.0) / 1000.0; }

template <class... F>
struct Overloaded : F... {
  using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

constexpr std::array<std::string_view, 3> kClasses = {"P", "K", "T"};
constexpr std::array<std::string_view, 3> kHows = {"direct", "overheard", "witnessed"};
constexpr std::array<std::string_view, 7> kSources = {"lifecycle", "snapshot_diff", "flag_watch", "pipeline",
                                                      "timer",     "hook",          "subscriber"};
constexpr std::array<std::string_view, 3> kOutcomes = {"Victory", "Death", "Left"};
constexpr std::array<std::string_view, 4> kVias = {"Hit", "LockClosing", "Near", "LockOnly"};
constexpr std::array<std::string_view, 2> kWhys = {"Timeout", "Distance"};
constexpr std::array<std::string_view, 5> kLevels = {"Starting", "L1", "L2", "L3", "L4"};
constexpr std::array<std::string_view, 3> kSides = {"tarnished", "card", "team"};
struct InterruptWord {
  InterruptSet bit;
  std::string_view word;
};
constexpr std::array<InterruptWord, 7> kInterruptWords = {{
    {InterruptSet::ChatInput, "chat_input"},
    {InterruptSet::Ambient, "ambient"},
    {InterruptSet::Autonomous, "autonomous"},
    {InterruptSet::Conversation, "conversation"},
    {InterruptSet::ConversationOutput, "conversation_output"},
    {InterruptSet::OwnLinesExceptPlayerFell, "own_lines_except_player_fell"},
    {InterruptSet::OwnLinesWithin20m, "own_lines_within_20m"},
}};

template <std::size_t N>
std::string_view word(const std::array<std::string_view, N>& words, std::size_t i) {
  return i < N ? words[i] : std::string_view("?");
}

J opt_actor(const std::optional<ActorRef>& a, const HandleNamer& name) {
  return a ? J(actor_text(*a, name)) : J(nullptr);
}

J bars_json(const proto::FixedVector<BossRef, 3>& bars, const HandleNamer& name) {
  J out = J::array();
  for (const BossRef& b : bars) out.push_back(J{{"boss", actor_text(b.boss, name)}, {"name_id", b.name_id}});
  return out;
}

J payload_json(const EventKind& kind, const HandleNamer& name) {
  return std::visit(
      Overloaded{
          [&](const PlaceEntered& e) {
            return J{{"place_id", e.place_id}, {"prev", e.prev ? J(*e.prev) : J(nullptr)}};
          },
          [&](const BossFightStarted& e) { return J{{"bars", bars_json(e.bars, name)}}; },
          [&](const BossFightEnded& e) {
            return J{{"outcome", word(kOutcomes, static_cast<std::size_t>(e.outcome))}, {"bars", bars_json(e.bars, name)}};
          },
          [&](const NativeTalkStarted& e) {
            return J{{"persona", e.persona ? J(e.persona->index) : J(nullptr)},
                     {"popup", e.signals.popup},
                     {"talk_cam", e.signals.talk_cam},
                     {"popup_menu", e.signals.popup_menu}};
          },
          [&](const NativeTalkEnded& e) {
            return J{{"persona", e.persona ? J(e.persona->index) : J(nullptr)}, {"held_ms", e.held_ms}};
          },
          [&](const PlayerDied& e) { return J{{"killer", opt_actor(e.killer, name)}, {"in_boss", e.in_boss}}; },
          [&](const MountChanged& e) { return J{{"riding", e.riding}}; },
          [&](const Struck& e) {
            return J{{"att", opt_actor(e.att, name)}, {"vic", actor_text(e.vic, name)}, {"hard", e.hard},
                     {"unseen", e.unseen}, {"count", e.count}, {"frac_total", r3(e.frac_total)},
                     {"dmg_type", e.dmg_type}};
          },
          [&](const LowHp& e) { return J{{"subject", actor_text(e.subject, name)}, {"frac", r3(e.frac)}}; },
          [&](const Healed& e) { return J{{"subject", actor_text(e.subject, name)}, {"frac", r3(e.frac)}}; },
          [&](const Engaged& e) {
            return J{{"subject", actor_text(e.subject, name)}, {"opponent", actor_text(e.opponent, name)},
                     {"via", word(kVias, static_cast<std::size_t>(e.via))}, {"squad", e.squad}};
          },
          [&](const Disengaged& e) {
            return J{{"subject", actor_text(e.subject, name)}, {"opponent", actor_text(e.opponent, name)},
                     {"fled", e.fled}, {"why", word(kWhys, static_cast<std::size_t>(e.why))}};
          },
          [&](const AllyDied& e) {
            J w = J::array();
            for (const ChrHandle h : e.witnesses) w.push_back(name(h));
            return J{{"victim", actor_text(e.victim, name)},
                     {"side", std::string(word(kSides, static_cast<std::size_t>(e.side.kind))) + ":" +
                                  std::to_string(e.side.value)},
                     {"witnesses", w}};
          },
          [&](const CritLanded& e) { return J{{"victim", actor_text(e.victim, name)}}; },
          [&](const PlaythroughReady& e) { return J{{"slot", e.key.slot}, {"ordinal", e.key.ordinal}}; },
          [&](const JourneyChanged& e) { return J{{"ng_lvl", e.ng_lvl}}; },
          [&](const GateChanged& e) { return J{{"active", e.active}}; },
          [&](const GroupTripped& e) { return J{{"group", proto::to_string(e.group)}}; },
          [&](const LadderChanged& e) { return J{{"to", word(kLevels, static_cast<std::size_t>(e.to))}}; },
          [&](const auto&) { return J::object(); },  // kinds with no field, and those v0.1 never emits
      },
      kind);
}

}  // namespace

std::string GoldenNames::operator()(ChrHandle h) {
  const auto it = seen_.find(h.raw);
  if (it != seen_.end()) return "h" + std::to_string(it->second);
  const auto n = static_cast<std::uint32_t>(seen_.size() + 1);
  seen_.emplace(h.raw, n);
  return "h" + std::to_string(n);
}

std::string actor_text(const ActorRef& a, const HandleNamer& name) {
  switch (a.kind) {
    case ActorRef::Kind::Player:
      return "player";
    case ActorRef::Kind::Persona:
      return "persona:" + std::to_string(a.id) + "#" + name(a.handle);
    case ActorRef::Kind::Card:
      return "card:" + std::to_string(a.id) + "." + std::to_string(a.variant) + "#" + name(a.handle);
    case ActorRef::Kind::Mute:
      break;
  }
  return "mute#" + name(a.handle);
}

std::string key_names(const std::string& key, const HandleNamer& name) {
  std::string out;
  std::size_t at = 0;
  while (at < key.size()) {
    if (key[at] != '#') {
      out += key[at++];
      continue;
    }
    std::size_t end = at + 1;
    std::uint64_t v = 0;
    while (end < key.size() && std::isxdigit(static_cast<unsigned char>(key[end]))) {
      const char c = key[end];
      v = v * 16 + static_cast<std::uint64_t>(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
      ++end;
    }
    if (end == at + 1) {
      out += key[at++];
      continue;
    }
    out += name(ChrHandle{v});
    at = end;
  }
  return out;
}

nlohmann::ordered_json event_json(const Event& ev, const HandleNamer& name) {
  J j;
  j["seq"] = J::array({ev.id.step, ev.id.index});
  j["t"] = ev.t_mono_ms;
  j["t_play"] = ev.t_play_ms;
  j["kind"] = kind_name(ev.kind);
  j["class"] = word(kClasses, static_cast<std::size_t>(ev.event_class));
  j["rule"] = ev.rule ? J(*ev.rule) : J(nullptr);
  j["source"] = word(kSources, static_cast<std::size_t>(ev.source));
  j["subject"] = opt_actor(ev.subject, name);
  j["object"] = opt_actor(ev.object, name);
  j["key"] = ev.key ? J(key_names(*ev.key, name)) : J(nullptr);
  j["conf"] = r3(ev.confidence);
  J aud = J::array();
  for (const AudienceEntry& a : ev.audience) {
    aud.push_back(J::array({a.persona.index, word(kHows, static_cast<std::size_t>(a.how))}));
  }
  j["audience"] = aud;
  j["count"] = ev.count;
  j["merged"] = ev.merged;
  J ints = J::array();
  for (const InterruptWord& w : kInterruptWords) {
    if (any(ev.interrupts & w.bit)) ints.push_back(w.word);
  }
  j["interrupts"] = ints;
  j["payload"] = payload_json(ev.kind, name);
  return j;
}

std::string golden_line(const Event& ev, const HandleNamer& name) {
  // Every string here is ASCII of ours; `replace` keeps dump() from ever throwing all the same (no throwing, R3).
  return event_json(ev, name).dump(-1, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

}  // namespace en::augur
