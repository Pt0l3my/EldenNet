#pragma once
// eldennet.dll's report of an S0 run (DESIGN §16.1's S0-A to S0-D on the table-based start, roadmap S10.1.b): one
// numbers-only report, `eldennet-spike.json`, of the kind `spike_run`, in the run's folder that the launcher makes
// (`%APPDATA%\EldenNet\spikes\<word>-<n>`, named to the game in ELDENNET_SPIKE_DIR). T1 writes it once its start has
// stopped (Ready, a banner, inert), and T6 writes it again every 10 s with the reader's counts. The launcher reads it
// to tell the owner in plain words what the start did, and `eldennet-engine spike-report` reads it beside the
// launcher's logs and the recordings (DESIGN §16.1's pass rules).
//
// What it holds: the run (the spike, the game's process id, which joins it to the run's recording by the recording's
// file name, the builds' versions); how T1's start ended and, when it stopped short of a reading, why in one word
// (`waits`: the start's layout is not complete yet, gal/'s files, an unknown build, anti-cheat, the game not started);
// discovery's counts; the build's table: its ids checked at the start (placement, witnesses) and checked again once the
// start has stopped (the image untouched where the table points: no patch, no hook of ours); S0-D's guarded copies
// forced to fault on T1, by the SEH copy and by ReadProcessMemory; the group S0-D's faults are injected into; and the
// reader on EldenNet's own thread (T2): its length, snapshots, steady reading's counts (torn, held, unconfirmed), the
// guarded reads that faulted, the groups tripped and the task's p99. Numbers and fixed words only, through en-gal's
// closed-vocabulary writer: no path, no name, no text of the game's.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "en/core/core.hpp"
#include "en/gal/report.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/watch.hpp"

namespace en::shell {

inline constexpr std::string_view kSpikeRunFile = "eldennet-spike.json";

// The spikes eldennet.dll reports on: S0-A to S0-D. (The runs of GraceUI's observer, S0-E and S4, have the observer's
// report instead.)
[[nodiscard]] constexpr bool s0_spike(proto::watch::Spike s) noexcept {
  using S = proto::watch::Spike;
  return s == S::S0a || s == S::S0b || s == S::S0c || s == S::S0d || s == S::S0dCrash;
}
// Its sessions, one watched session each: the recorder runs in them whatever settings.toml says, since their rules read
// the recording (S0-B's length and faults, S0-C's gate, S0-D's breakers).
[[nodiscard]] constexpr bool s0_session(proto::watch::Spike s) noexcept {
  using S = proto::watch::Spike;
  return s == S::S0b || s == S::S0c || s == S::S0d;
}

// Why a start stopped short of a reading, when it did for a reason that is not the spike's to judge: the spike that
// needs the reading then waits for it, and nothing is counted against it.
enum class SpikeWaits : std::uint8_t {
  None,        // the start reached its reading, or stopped for a reason that is the spike's to judge (inert)
  Layout,      // the start's layout is not complete yet: a required item, a boot field or a structural group missing
  Files,       // gal/'s files are missing or were refused
  Build,       // the game's build is not one gal/builds.toml knows
  Anticheat,   // anti-cheat is present
  NotStarted,  // the game did not finish starting (no window, or its engine not up)
};
[[nodiscard]] std::string_view to_string(SpikeWaits w) noexcept;  // "none", "layout", "files", "build", ...

struct SpikeRun {
  proto::watch::Spike spike = proto::watch::Spike::None;
  std::uint32_t pid = 0;
  bool probe_build = false;
  std::array<std::uint16_t, 4> game_version{};  // the gated build's; zeros when unknown
  std::string_view region = "none";             // ww, jp, other, none
  std::array<std::uint16_t, 3> en_version{};
  proto::watch::InitState state = proto::watch::InitState::None;  // T1's terminal state; None before it
  std::optional<core::BannerReason> banner;
  SpikeWaits waits = SpikeWaits::None;
  std::uint64_t start_ms = 0;    // from T1's start to its terminal state
  std::uint64_t session_ms = 0;  // from T1's start to this write
  std::uint32_t writes = 0;      // this write's number, from 1

  struct Discovery {
    bool ran = false;
    bool ready = false;
    bool leads = false;  // the layout holds leads (a probe build accepts them)
    std::uint32_t passes = 0, items = 0, of = 0, getter_calls = 0, groups_on = 0;
  } discovery;

  // The build's table (proposal 0005 §7.1): its ids as discovery checked them, then checked again once the start had
  // stopped. `changed`: an id that passed both times at another place, or passed once and not again.
  struct Table {
    std::uint32_t items = 0, refused = 0;
    bool rechecked = false;
    std::uint32_t entries = 0, again_refused = 0, changed = 0, witnesses = 0;
  } table;

  // S0-D: guarded copies forced to fault, by each copy primitive: the SEH copy at a page of EldenNet's own with no
  // access, ReadProcessMemory at the null page.
  struct Trial {
    bool ran = false;
    std::uint32_t seh_forced = 0, seh_caught = 0, rpm_forced = 0, rpm_caught = 0;
  } trial;

  // S0-D: the group the reader's faults are injected into (a probe build's injection), when there is one.
  struct Inject {
    std::optional<proto::Group> group;
    std::uint32_t every = 0, after_s = 0;
  } inject;

  // The reader on EldenNet's own thread (T2).
  struct Reading {
    bool started = false;
    std::uint64_t ms = 0;  // since its first reading
    std::uint64_t snapshots = 0, torn = 0, held = 0, unconfirmed = 0, copy_faults = 0, injected_faults = 0;
    std::uint64_t tripped = 0;  // the groups tripped, bit per proto::Group
    std::uint32_t p99_us = 0;   // the C13 histogram's p99 since the start
  } reading;
};

[[nodiscard]] const gal::report::Vocabulary& spike_run_vocabulary() noexcept;
// The report's text (pretty-printed), or an empty string if the writer refused it (a defect of ours).
[[nodiscard]] std::string spike_run_text(const SpikeRun& r);

}  // namespace en::shell
