// The golden files' form of an event (golden.hpp; ARCH §8.12).
#include "en/augur/golden.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <string_view>

#include "en/augur/words.hpp"

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
            return J{{"outcome", to_string(e.outcome)}, {"bars", bars_json(e.bars, name)}};
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
                     {"via", to_string(e.via)}, {"squad", e.squad}};
          },
          [&](const Disengaged& e) {
            return J{{"subject", actor_text(e.subject, name)}, {"opponent", actor_text(e.opponent, name)},
                     {"fled", e.fled}, {"why", to_string(e.why)}};
          },
          [&](const AllyDied& e) {
            J w = J::array();
            for (const ChrHandle h : e.witnesses) w.push_back(name(h));
            return J{{"victim", actor_text(e.victim, name)},
                     {"side", std::string(to_string(e.side.kind)) + ":" +
                                  std::to_string(e.side.value)},
                     {"witnesses", w}};
          },
          [&](const CritLanded& e) { return J{{"victim", actor_text(e.victim, name)}}; },
          [&](const PlaythroughReady& e) { return J{{"slot", e.key.slot}, {"ordinal", e.key.ordinal}}; },
          [&](const JourneyChanged& e) { return J{{"ng_lvl", e.ng_lvl}}; },
          [&](const GateChanged& e) { return J{{"active", e.active}}; },
          [&](const GroupTripped& e) { return J{{"group", proto::to_string(e.group)}}; },
          [&](const LadderChanged& e) { return J{{"to", to_string(e.to)}}; },
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
  j["class"] = to_string(ev.event_class);
  j["rule"] = ev.rule ? J(*ev.rule) : J(nullptr);
  j["source"] = to_string(ev.source);
  j["subject"] = opt_actor(ev.subject, name);
  j["object"] = opt_actor(ev.object, name);
  j["key"] = ev.key ? J(key_names(*ev.key, name)) : J(nullptr);
  j["conf"] = r3(ev.confidence);
  J aud = J::array();
  for (const AudienceEntry& a : ev.audience) {
    aud.push_back(J::array({a.persona.index, to_string(a.how)}));
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

namespace {

struct KindFields {
  std::string_view kind;
  std::span<const FieldInfo> fields;
};

constexpr FieldInfo kPlace[] = {{"place_id", "u32"}, {"prev", "u32 or null"}};
constexpr FieldInfo kBossStart[] = {{"bars", "bars"}};
constexpr FieldInfo kBossEnd[] = {{"outcome", "word"}, {"bars", "bars"}};
constexpr FieldInfo kTalkStart[] = {
    {"persona", "persona or null"}, {"popup", "bool"}, {"talk_cam", "bool"}, {"popup_menu", "bool"}};
constexpr FieldInfo kTalkEnd[] = {{"persona", "persona or null"}, {"held_ms", "u32"}};
constexpr FieldInfo kPlayerDied[] = {{"killer", "actor or null"}, {"in_boss", "bool"}};
constexpr FieldInfo kMount[] = {{"riding", "bool"}};
constexpr FieldInfo kStruck[] = {{"att", "actor or null"}, {"vic", "actor"},     {"hard", "bool"},
                                 {"unseen", "bool"},       {"count", "u32"},     {"frac_total", "number"},
                                 {"dmg_type", "int"}};
constexpr FieldInfo kFrac[] = {{"subject", "actor"}, {"frac", "number"}};
constexpr FieldInfo kEngaged[] = {{"subject", "actor"}, {"opponent", "actor"}, {"via", "word"}, {"squad", "u32"}};
constexpr FieldInfo kDisengaged[] = {{"subject", "actor"}, {"opponent", "actor"}, {"fled", "bool"}, {"why", "word"}};
constexpr FieldInfo kAllyDied[] = {{"victim", "actor"}, {"side", "word"}, {"witnesses", "handles"}};
constexpr FieldInfo kCrit[] = {{"victim", "actor"}};
constexpr FieldInfo kPlaythrough[] = {{"slot", "u32"}, {"ordinal", "u32"}};
constexpr FieldInfo kJourney[] = {{"ng_lvl", "u32"}};
constexpr FieldInfo kGate[] = {{"active", "bool"}};
constexpr FieldInfo kGroup[] = {{"group", "word"}};
constexpr FieldInfo kLadder[] = {{"to", "word"}};

// The kinds payload_json writes a field of, as it writes them; every other kind's payload is empty.
constexpr KindFields kKindFields[] = {
    {"PlaceEntered", kPlace},     {"BossFightStarted", kBossStart}, {"BossFightEnded", kBossEnd},
    {"NativeTalkStarted", kTalkStart}, {"NativeTalkEnded", kTalkEnd}, {"PlayerDied", kPlayerDied},
    {"MountChanged", kMount},     {"Struck", kStruck},              {"LowHp", kFrac},
    {"Healed", kFrac},            {"Engaged", kEngaged},            {"Disengaged", kDisengaged},
    {"AllyDied", kAllyDied},      {"CritLanded", kCrit},            {"PlaythroughReady", kPlaythrough},
    {"JourneyChanged", kJourney}, {"GateChanged", kGate},           {"GroupTripped", kGroup},
    {"LadderChanged", kLadder},
};

}  // namespace

std::span<const FieldInfo> kind_fields(std::size_t kind) noexcept {
  const std::string_view name = kind_name(kind);
  for (const KindFields& k : kKindFields) {
    if (k.kind == name) return k.fields;
  }
  return {};
}

std::string fields_text(std::size_t kind) {
  std::string out;
  for (const FieldInfo& f : kind_fields(kind)) {
    if (!out.empty()) out += ", ";
    out.append(f.name).append(" (").append(f.type).append(")");
  }
  return out;
}

std::string golden_line(const Event& ev, const HandleNamer& name) {
  // Every string here is ASCII of ours; `replace` keeps dump() from ever throwing all the same (no throwing, R3).
  return event_json(ev, name).dump(-1, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

}  // namespace en::augur
