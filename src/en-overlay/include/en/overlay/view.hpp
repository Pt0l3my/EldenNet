#pragma once
// The F11 debug view's content (part 12; DESIGN §14's "ladder and groups" as far as v0.1 goes; GraceUI D156): EldenNet's
// own status, laid out as lines of words and numbers. Portable and pure, so it is tested in every build.
//
// No game text can reach it: every word is the name of one of our own enums (en-core's phases and levels, en-proto's
// groups, en-gal's faults) or a word of this file's, and each is checked again as it is written (letters, digits and
// `_.+-`, anything else '?'); everything else is a number. The view shows no name, no place, no line of dialogue, no
// path and no address.
#include <array>
#include <cstddef>
#include <cstdint>

#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

namespace en::overlay {

// Where the view sits and what it looks like: GraceUI's panel layer, anchored at the display's top left (GraceUI's
// notices are at the top right), in the panel's own pixels. The tests read its ground back from the game's frame.
inline constexpr float kViewX = 24.0f;
inline constexpr float kViewY = 24.0f;
inline constexpr float kViewWidth = 680.0f;
inline constexpr float kViewHeight = 420.0f;
inline constexpr std::array<std::uint8_t, 3> kViewGround = {22, 28, 38};   // a dark slate, opaque
inline constexpr std::array<std::uint8_t, 3> kViewRibbon = {120, 52, 40};  // the developer ribbon (PAGES SP8)
inline constexpr const char* kRibbonText = "EldenNet developer view (F11 closes it)";

// One change of en-core's slice, as the view lists it (the newest last).
struct ViewChange {
  const char* what = "";    // en-core's Change::What name: "phase", "gate_opened", ...
  const char* to = "";      // its new value's name
  std::uint64_t at_ms = 0;  // mono ms
};

inline constexpr std::size_t kViewChanges = 8;
inline constexpr std::size_t kViewFaultKinds = 6;

// What the view shows. Every `const char*` is a word of ours with static storage (an enum's name); the DLL fills it
// on its core thread from en-core's state, the task's status and the watch block's.
struct ViewStatus {
  std::array<std::uint16_t, 3> version{};  // EldenNet's
  const char* label = "dev";               // the build's label (EN_VERSION_LABEL)
  bool probe_build = false;
  // T1 and discovery.
  const char* init = "none";
  std::uint32_t items_resolved = 0;
  std::uint32_t items_total = 0;
  std::uint32_t passes = 0;
  // en-core's slice: the lifecycle and the ladder.
  const char* phase = "gate";
  const char* level = "starting";
  const char* banner = "none";
  bool gate_open = false;
  const char* gate_why = "loading";
  bool dormant = true;
  bool badge = false;
  // The task and the read groups.
  std::uint64_t snapshots = 0;
  std::uint32_t task_p99_us = 0;
  const char* read_mode = "none";
  bool throttled = false;
  bool have_health = false;  // a snapshot has arrived: `health` is its groups'
  proto::ReadHealth health;
  std::uint64_t tripped = 0;  // C2's GroupTripped, a bit per proto::Group
  std::uint64_t copies = 0;
  std::array<std::uint64_t, kViewFaultKinds> faults{};
  std::array<const char*, kViewFaultKinds> fault_names{};  // en-gal's FaultKind names; null for none
  // The Marks and GraceUI.
  std::uint32_t marks = 0;
  const char* mark_source = "none";
  std::uint32_t graceui_version = 0;  // GRACEUI_VERSION_PACK
  // The newest changes, the newest last, and the time they are measured against.
  proto::FixedVector<ViewChange, kViewChanges> changes;
  std::uint64_t now_ms = 0;
};

inline constexpr std::size_t kViewLineBytes = 96;  // with its NUL
inline constexpr std::size_t kViewLines = 24;

struct ViewLine {
  std::array<char, kViewLineBytes> text{};
  bool heading = false;  // drawn in the heading's colour
};
using ViewLines = proto::FixedVector<ViewLine, kViewLines>;

// The view's lines for `s`, into `out` (cleared first). Allocation-free; a line longer than kViewLineBytes - 1 is cut.
void view_lines(const ViewStatus& s, ViewLines& out) noexcept;

// A word of ours as the view writes it: letters, digits and `_.+-` kept, anything else '?', at most `max` characters;
// null is "none". Answers how many characters were written (no NUL).
std::size_t put_word(const char* word, char* out, std::size_t max) noexcept;

}  // namespace en::overlay
