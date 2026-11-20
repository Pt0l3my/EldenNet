#pragma once
// The F11 debug view's content (part 12; DESIGN §14's "ladder and groups" as far as v0.1 goes, and its "live events
// with audiences": Augur's, roadmap S10.2.d; GraceUI D156): EldenNet's own status, Augur's newest events and en-core's
// newest changes, laid out as lines of words and numbers. Portable and pure, so it is tested in every build.
//
// No game text can reach it: every word is the name of one of our own enums (en-core's phases and levels, en-proto's
// groups, en-gal's faults, Augur's kinds and words) or a word of this file's, and each is checked again as it is
// written (letters, digits and `_.+-`, anything else '?'); everything else is a number. The view shows no name, no
// place's name, no line of dialogue, no path, no address and no handle: an event's subject is its kind of actor and,
// for a persona or a card, its index; a place is its id.
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
inline constexpr float kViewHeight = 500.0f;  // kViewLines lines of Dear ImGui's own 13 px font, 2 px apart
inline constexpr float kViewLineGap = 2.0f;   // the style's ItemSpacing.y
inline constexpr std::array<std::uint8_t, 3> kViewGround = {22, 28, 38};   // a dark slate, opaque
inline constexpr std::array<std::uint8_t, 3> kViewRibbon = {120, 52, 40};  // the developer ribbon (PAGES SP8)
inline constexpr const char* kRibbonText = "EldenNet developer view (F11 closes it)";

// Where an event came from, as the view marks it: a square of its colour before the event's line, in the line's first
// two (blank) cells, kViewMarkSize pixels a side from kViewMarkX and kViewMarkY below the line's top. The tests count
// these squares on the game's frame.
enum class EventFrom : std::uint8_t {
  Lifecycle,  // en-core's slice (Augur's Lifecycle source): the start, loading, the title, the gate, the ladder
  World,      // the game's world (snapshot diffs, the timer, and later the flag watch and the hooks)
  Other,      // EldenNet's own pipeline and subscribers (none in v0.1)
};
inline constexpr std::array<std::array<std::uint8_t, 3>, 3> kViewMarks = {{
    {90, 140, 210},   // Lifecycle: a blue
    {80, 190, 110},   // World: a green
    {210, 150, 60},   // Other: an amber
}};
inline constexpr float kViewMarkX = 9.0f;
inline constexpr float kViewMarkY = 4.0f;
inline constexpr float kViewMarkSize = 6.0f;

// One change of en-core's slice, as the view lists it: the ladder's and the lifecycle's moves (DESIGN §14's "ladder").
struct ViewChange {
  const char* what = "";    // en-core's Change::What name: "phase", "gate_opened", ...
  const char* to = "";      // its new value's name
  std::uint64_t at_ms = 0;  // mono ms
};

// One of Augur's events, as the view lists it (the DLL fills it from the event as Augur's words write it, words.hpp).
struct ViewAudienceRow {
  std::uint32_t persona = 0;  // the profile's index
  const char* how = "";       // "direct", "overheard", "witnessed"
};
struct ViewEvent {
  const char* kind = "";        // "Struck"
  const char* cls = "";         // "P", "K", "T"
  EventFrom from = EventFrom::World;
  const char* subject = nullptr;  // "player", "persona", "card", "mute"; null when the event has none
  bool subject_id = false;      // a persona's or a card's: `id` is its index
  std::uint32_t id = 0;
  const char* reason = nullptr;  // the event's reason (words.hpp), or none
  bool reason_number = false;
  std::uint64_t number = 0;
  std::uint16_t count = 1;  // a K event's merged count
  bool merged = false;
  const char* radius = "none";  // the audience's radius
  std::uint32_t audience = 0;   // its rows (the profiles that perceived it)
  ViewAudienceRow first{};      // the first of them, when there is one
  std::uint64_t at_ms = 0;  // mono ms
};

// A bounded ring of the newest N items (T6's alone; fixed storage, no allocation): the view's events and changes.
template <class T, std::size_t N>
class NewestRing {
 public:
  constexpr void push(const T& item) noexcept {
    items_[static_cast<std::size_t>(seen_ % N)] = item;
    ++seen_;
  }
  [[nodiscard]] constexpr std::uint64_t seen() const noexcept { return seen_; }  // every item ever pushed
  [[nodiscard]] constexpr std::size_t size() const noexcept {
    return seen_ < N ? static_cast<std::size_t>(seen_) : N;
  }
  // The i-th newest held (0 the newest). Precondition: i < size().
  [[nodiscard]] constexpr const T& newest(std::size_t i) const noexcept {
    return items_[static_cast<std::size_t>((seen_ - 1 - i) % N)];
  }
  // What it holds, the newest last, into `out` (cleared first): the fixed snapshot T6 hands the view.
  template <std::size_t M>
  constexpr void copy_to(proto::FixedVector<T, M>& out) const noexcept {
    static_assert(M >= N);
    out.clear();
    for (std::size_t i = size(); i-- > 0;) (void)out.try_push_back(newest(i));
  }

 private:
  std::array<T, N> items_{};
  std::uint64_t seen_ = 0;
};

inline constexpr std::size_t kViewEvents = 10;
inline constexpr std::size_t kViewChanges = 5;
inline constexpr std::size_t kViewFaultKinds = 6;
using EventRing = NewestRing<ViewEvent, kViewEvents>;
using ChangeRing = NewestRing<ViewChange, kViewChanges>;

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
  // Augur's newest events and en-core's newest changes, each the newest last, with how many there have been, and the
  // time their ages are measured against.
  proto::FixedVector<ViewEvent, kViewEvents> events;
  std::uint64_t events_seen = 0;
  proto::FixedVector<ViewChange, kViewChanges> changes;
  std::uint64_t now_ms = 0;
};

inline constexpr std::size_t kViewLineBytes = 96;  // with its NUL
inline constexpr std::size_t kViewLines = 32;

struct ViewLine {
  std::array<char, kViewLineBytes> text{};
  bool heading = false;  // drawn in the heading's colour
  bool marked = false;   // an event's line: its square drawn first, in kViewMarks[from]
  EventFrom from = EventFrom::World;
};
using ViewLines = proto::FixedVector<ViewLine, kViewLines>;

// The view's lines for `s`, into `out` (cleared first). Allocation-free; a line longer than kViewLineBytes - 1 is cut.
void view_lines(const ViewStatus& s, ViewLines& out) noexcept;

// A word of ours as the view writes it: letters, digits and `_.+-` kept, anything else '?', at most `max` characters;
// null is "none". Answers how many characters were written (no NUL).
std::size_t put_word(const char* word, char* out, std::size_t max) noexcept;

}  // namespace en::overlay
