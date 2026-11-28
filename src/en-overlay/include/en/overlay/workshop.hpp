#pragma once
// Tools › Workshop (PAGES §4.6.8, SP8; GraceUI D258; roadmap S11.4.a): its topics' bodies, its events stream and its
// one command, for EldenNet's space (space.hpp), portable. The page is in the browser alone (`browser_only`), shown
// only while Developer views are on (`shown_with = "debug.panel"`), behind its gate once a page load and under its
// ribbon.
//
// While the switch is off the DLL keeps nothing (the facts are null) and these topics publish nothing; the moment it
// goes off, each watched state topic gets `{}` once and the events stream an empty batch past a gap (protocol 1.1),
// since GraceUI's server holds a watched topic's newest body and a stream's rows for later watchers. A watch cannot be
// refused: the space hears only that a topic is watched.
//
// Workshop is on this PC alone, by pc_only (GraceUI 0.2.57's D264, protocol 1.20; the owner's O132 (a)): the page and
// every workshop.* topic are marked, each topic drawn on Workshop alone and no setting homed there (a shortcut is not a
// home), so a paired device's page data leaves the page out and its watch of a Workshop topic is refused.
//
// Workshop is SP1's one exception: the prompt as sent, the reply as written, every event, flags, stages, known sets.
// The DLL has scrubbed every text of a key and of the character's name before it kept it; the prompt's and the reply's
// words shaped like a path or a local address are scrubbed again here (PV5), and a SteamID64's shape.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/workshop.hpp"

namespace en::overlay {

inline constexpr std::string_view kWorkshopExchange = "workshop.exchange";  // members facts, parts
inline constexpr std::string_view kWorkshopEvents = "workshop.events";      // a stream, ring kWorkshopEventRing
inline constexpr std::string_view kWorkshopWorld = "workshop.world";        // members frame, stages, flags, tracks
inline constexpr std::string_view kWorkshopKnown = "workshop.known";
inline constexpr std::string_view kWorkshopUnknown = "workshop.unknown";    // members rows, toml
inline constexpr std::string_view kWorkshopStatus = "workshop.status";      // recording_on, members recording, events
inline constexpr std::string_view kWorkshopKinds = "workshop.kinds";        // members kinds, rules
inline constexpr std::string_view kRecordingCommand = "recording.set";     // {"on": bool}: sensitive (D221)
inline constexpr std::string_view kPanelSetting = "debug.panel";

// recording.set, for the DLL: start a recording now, or stop the one being written.
struct RecordingRequest {
  bool on = false;
  friend bool operator==(const RecordingRequest&, const RecordingRequest&) = default;
};

class Workshop {
 public:
  [[nodiscard]] static bool owns_topic(std::string_view topic) noexcept;  // its state topics and its stream
  [[nodiscard]] static bool owns_command(std::string_view name) noexcept { return name == kRecordingCommand; }
  // A state topic's body from the facts; null facts (the switch off, or nothing kept yet): empty.
  [[nodiscard]] static nlohmann::json body(std::string_view topic, const proto::WorkshopView* view);

  // The events stream: a row joins the ring (its newest kWorkshopEventRing); the stream's frames for a watcher, from
  // the oldest held for a new one, in batches of at most 100; and its clearing, an empty batch past a gap, the ring
  // emptied.
  void push(const proto::WorkshopEvent& e);
  void restart() noexcept { sent_ = 0; }  // a new watcher: from the oldest held
  void collect_events(std::vector<std::string>& frames);
  [[nodiscard]] std::string clear_events();
  [[nodiscard]] std::size_t held() const noexcept { return rows_.size(); }
  [[nodiscard]] nlohmann::json rows() const;  // every row held, oldest first (the tests' and the fixtures')

 private:
  struct Row {
    std::uint64_t seq = 0;
    proto::WorkshopEvent event;
  };
  [[nodiscard]] static nlohmann::json row_of(const Row& r);
  std::deque<Row> rows_;
  std::uint64_t next_seq_ = 1;
  std::uint64_t sent_ = 0;  // the last row's seq the watched stream has had, 0 for none
};

}  // namespace en::overlay
