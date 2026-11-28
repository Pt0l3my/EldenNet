#pragma once
// The memory pages' facts of the fixture states (memory.hpp): invented people, words and times. At the title nothing is
// loaded; in the world the file holds two journeys, the first (before NG+) read only and the second the current one,
// so that the spoiler goldens show both: a person met only on the first journey is absent from the second's pages.
#include <string_view>

#include "en/overlay/memory.hpp"

namespace en::overlay::test {

inline const MemoryFacts* memory_fixture(std::string_view state) {
  if (state != "world") return nullptr;
  static const MemoryFacts f = [] {
    MemoryFacts m;
    m.revision = 4;
    m.loaded = true;
    m.can_forget = true;
    m.current = 2;
    m.backups = 3;
    m.backup_times = {LocalTime{2026, 11, 27, 14, 5}, LocalTime{2026, 11, 26, 9, 40}, LocalTime{2026, 11, 25, 21, 2}};
    JourneyFacts first;
    first.journey = 1;
    MemberFacts keeper;
    keeper.persona = "lantern_keeper";
    keeper.name = "Oswin the Lamplighter";
    keeper.met = 4;
    keeper.seen = 2;
    keeper.first_met_pt = 600000;
    keeper.last_met_pt = 9000000;
    keeper.journal = true;
    keeper.summary = "A traveller who asks after the lamp, and listens to the answer.";
    keeper.disposition = 1;
    keeper.written_pt = 9100000;
    keeper.keepsakes = {KeepsakeFacts{"They asked whose lamp it was.", 0.8, false, "You", true, 900000}};
    MemberFacts ferry;
    ferry.persona = "ferry_warden";
    ferry.name = "Merrit of the Ferry";
    ferry.met = 1;
    ferry.first_met_pt = 3000000;
    ferry.last_met_pt = 3000000;
    first.members = {ferry, keeper};
    first.conversations = {ConversationFacts{7, "lantern_keeper", "Oswin the Lamplighter", 8800000, 8900000, "travel",
                                             6}};
    JourneyFacts second;
    second.journey = 2;
    second.current = true;
    MemberFacts keeper2 = keeper;
    keeper2.met = 1;
    keeper2.seen = 0;
    keeper2.first_met_pt = 720000;
    keeper2.last_met_pt = 720000;
    keeper2.here = true;
    keeper2.speaking = false;
    keeper2.written_pt.reset();
    keeper2.journal = false;
    keeper2.summary.clear();
    keeper2.keepsakes.clear();
    keeper2.queued = 1;
    second.members = {keeper2};
    second.conversations = {ConversationFacts{12, "lantern_keeper", "Oswin the Lamplighter", 700000, std::nullopt, "",
                                              2}};
    m.journeys = {first, second};
    m.transcripts[12] = {LineFacts{true, "", "Is the lamp yours?", 701000},
                         LineFacts{false, "Oswin the Lamplighter", "It keeps me as I keep it.", 704000}};
    return m;
  }();
  return &f;
}

}  // namespace en::overlay::test
