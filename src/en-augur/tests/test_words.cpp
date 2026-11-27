// Augur's words (words.hpp): the golden form's vocabulary, and an event's reason at a glance.
#include <doctest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "en/augur/golden.hpp"
#include "en/augur/words.hpp"

using namespace en::augur;

namespace {

Event event_of(EventKind kind) {
  Event ev;
  ev.kind = std::move(kind);
  return ev;
}

std::string reason_text(const EventKind& kind) {
  const Reason r = reason(event_of(kind));
  std::string out = r.word == nullptr ? "-" : r.word;
  if (r.has_number) out += " " + std::to_string(r.number);
  return out;
}

bool a_word(const char* w) {
  if (w == nullptr || *w == '\0') return false;
  for (const char* p = w; *p != '\0'; ++p) {
    const char c = *p;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '?')) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("words: each enum's words, and '?' outside it") {
  CHECK(std::string(to_string(Class::P)) == "P");
  CHECK(std::string(to_string(Class::T)) == "T");
  CHECK(std::string(to_string(How::Overheard)) == "overheard");
  CHECK(std::string(to_string(Radius::Witness)) == "witness");
  CHECK(std::string(to_string(Radius::None)) == "none");
  CHECK(std::string(to_string(SourceId::Lifecycle)) == "lifecycle");
  CHECK(std::string(to_string(SourceId::SnapshotDiff)) == "snapshot_diff");
  CHECK(std::string(to_string(SourceId::Subscriber)) == "subscriber");
  CHECK(std::string(to_string(ActorRef::Kind::Mute)) == "mute");
  CHECK(std::string(to_string(SideId::Kind::Team)) == "team");
  CHECK(std::string(to_string(BossOutcome::Victory)) == "Victory");
  CHECK(std::string(to_string(EngageVia::LockClosing)) == "LockClosing");
  CHECK(std::string(to_string(DisengageWhy::Distance)) == "Distance");
  CHECK(std::string(to_string(LadderLevel::L2)) == "L2");
  CHECK(std::string(to_string(static_cast<Class>(9))) == "?");
  CHECK(std::string(to_string(static_cast<Radius>(200))) == "?");
  CHECK(std::string(kind_word(EventKind{Struck{}})) == "Struck");
  CHECK(std::string(kind_word(EventKind{DecisionMade{}})) == "DecisionMade");
  for (std::size_t i = 0; i < kKindCount; ++i) {
    CHECK(std::strlen(kKindNames[i].data()) == kKindNames[i].size());  // NUL-terminated where kind_word reads them
  }
}

TEST_CASE("words: the golden form writes the same words") {
  Event ev = event_of(Engaged{ActorRef::player(), ActorRef::mute(ChrHandle{0x10}), EngageVia::Near, 3});
  ev.event_class = Class::K;
  ev.source = SourceId::SnapshotDiff;
  ev.audience.push_back(AudienceEntry{PersonaId{4}, How::Direct});
  GoldenNames names;
  const auto j = event_json(ev, [&](ChrHandle h) { return names(h); });
  CHECK(j["class"] == to_string(ev.event_class));
  CHECK(j["source"] == to_string(ev.source));
  CHECK(j["audience"][0][1] == to_string(How::Direct));
  CHECK(j["payload"]["via"] == to_string(EngageVia::Near));
}

TEST_CASE("words: each kind's reason, in our words and numbers only") {
  CHECK(reason_text(PlaceEntered{6100100, std::nullopt}) == "place 6100100");
  BossFightStarted bars;
  (void)bars.bars.try_push_back(BossRef{ActorRef::mute(ChrHandle{7}), 9100});
  CHECK(reason_text(bars) == "bars 1");
  CHECK(reason_text(BossFightEnded{BossOutcome::Death, {}}) == "Death");
  CHECK(reason_text(NativeTalkStarted{std::nullopt, TalkSignals{false, true, false}}) == "talk_cam");
  CHECK(reason_text(NativeTalkStarted{std::nullopt, TalkSignals{true, true, false}}) == "popup");
  CHECK(reason_text(NativeTalkStarted{std::nullopt, TalkSignals{}}) == "-");
  CHECK(reason_text(NativeTalkEnded{std::nullopt, 2500}) == "held_ms 2500");
  CHECK(reason_text(PlayerDied{std::nullopt, true}) == "in_boss");
  CHECK(reason_text(PlayerDied{ActorRef::mute(ChrHandle{9}), false}) == "killed");
  CHECK(reason_text(PlayerDied{std::nullopt, false}) == "died");
  CHECK(reason_text(MountChanged{true}) == "mounted");
  CHECK(reason_text(MountChanged{false}) == "dismounted");
  Struck s;
  CHECK(reason_text(s) == "hit");
  s.hard = true;
  CHECK(reason_text(s) == "hard");
  s.unseen = true;
  CHECK(reason_text(s) == "unseen");
  CHECK(reason_text(Engaged{ActorRef::player(), ActorRef::mute(ChrHandle{1}), EngageVia::Hit, 1}) == "Hit");
  Disengaged d{ActorRef::player(), ActorRef::mute(ChrHandle{1}), false, DisengageWhy::Timeout};
  CHECK(reason_text(d) == "Timeout");
  d.fled = true;
  CHECK(reason_text(d) == "fled");
  CHECK(reason_text(AllyDied{ActorRef::mute(ChrHandle{2}), SideId{}, {ChrHandle{3}, ChrHandle{4}}}) == "witnesses 2");
  CHECK(reason_text(PlaythroughReady{PlaythroughKey{3, 1}}) == "slot 3");
  CHECK(reason_text(JourneyChanged{2}) == "ng 2");
  CHECK(reason_text(GateChanged{true}) == "active");
  CHECK(reason_text(GateChanged{false}) == "inactive");
  CHECK(reason_text(GroupTripped{en::proto::Group::Camera}) == "camera");
  CHECK(reason_text(LadderChanged{LadderLevel::L1}) == "L1");
  // An HP fraction is never a reason, and the kinds with nothing telling have none.
  CHECK(reason_text(LowHp{ActorRef::player(), 0.18f}) == "-");
  CHECK(reason_text(Healed{ActorRef::player(), 0.4f}) == "-");
  CHECK(reason_text(LoadingStarted{}) == "-");
  CHECK(reason_text(CritLanded{ActorRef::mute(ChrHandle{5})}) == "-");
}

TEST_CASE("words: every reason of every kind is a word of ours") {
  // Every kind default-made, and the v0.1 kinds above: the word is ours or there is none.
  const auto check_index = [](auto index) {
    using K = std::variant_alternative_t<decltype(index)::value, EventKind>;
    const Reason r = reason(event_of(K{}));
    if (r.word != nullptr) CHECK(a_word(r.word));
  };
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (check_index(std::integral_constant<std::size_t, I>{}), ...);
  }(std::make_index_sequence<kKindCount>{});
}

TEST_CASE("words: each kind's payload schema is what the golden form writes, kind by kind (Workshop's schemas)") {
  // Every kind default-made: the payload's keys are kind_fields' names, in their order, and nothing else.
  const auto check_index = [](auto index) {
    constexpr std::size_t kIndex = decltype(index)::value;
    using K = std::variant_alternative_t<kIndex, EventKind>;
    GoldenNames names;
    const auto j = event_json(event_of(K{}), [&](ChrHandle h) { return names(h); });
    std::vector<std::string> keys;
    for (auto it = j["payload"].begin(); it != j["payload"].end(); ++it) keys.push_back(it.key());
    std::vector<std::string> listed;
    for (const FieldInfo& f : kind_fields(kIndex)) {
      listed.emplace_back(f.name);
      CHECK_FALSE(f.type.empty());
    }
    INFO(kind_name(kIndex));
    CHECK(keys == listed);
  };
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (check_index(std::integral_constant<std::size_t, I>{}), ...);
  }(std::make_index_sequence<kKindCount>{});
  CHECK(fields_text(EventKind{PlaceEntered{}}.index()) == "place_id (u32), prev (u32 or null)");
  CHECK(fields_text(EventKind{DecisionMade{}}.index()).empty());
  CHECK(kind_fields(kKindCount).empty());  // past the catalogue: none
}

TEST_CASE("words: a detector's word is the rule files' own, one each") {
  std::vector<std::string> seen;
  for (int i = 0; i <= static_cast<int>(DetectorKind::Fact); ++i) {
    const std::string w = to_string(static_cast<DetectorKind>(i));
    CHECK(w != "?");
    CHECK(std::find(seen.begin(), seen.end(), w) == seen.end());
    seen.push_back(w);
  }
  CHECK(std::string(to_string(DetectorKind::DropOverWindow)) == "drop_over_window");
  CHECK(std::string(to_string(DetectorKind::Engagement)) == "engagement");
}
