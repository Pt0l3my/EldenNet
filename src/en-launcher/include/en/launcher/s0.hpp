#pragma once
// The S0 spikes in the launcher (DESIGN §16.1's S0-A to S0-D on the table-based start; roadmap S10.1.b): what it does
// for them beyond any spike's run, and the S0 kind's own data.
//
// - Each S0 run gets a folder of its own, as an observer's run does (spikes.hpp): `spikes\<word>-<n>`, named to the
//   game in ELDENNET_SPIKE_DIR, where eldennet.dll writes its report of the run, eldennet-spike.json (its en_shell's
//   spikerun.hpp: numbers and fixed words only). The launcher reads a few facts from it (read_s0_facts): how the start
//   ended, why when it stopped short of its reading for want of something (`waits`), the build's table as checked.
// - The tester's panel (tasks.hpp, win/panel.cpp; WHY§444) lists the owner's steps for each S0 spike, from the S0
//   kind's task lists below; Lodestar's read round keeps its own (read_tasks). S0-B's two hours are a Wait the panel counts
//   down, and the owner may stop early by quitting the game: the report says how long the reader ran.
// - A spike that cannot run whole says why, on the console and in its report, in plain words, and that is never
//   counted against it (s0_waits_text).
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/launcher/record.hpp"
#include "en/launcher/tasks.hpp"
#include "en/proto/watch.hpp"

namespace en::launcher {

[[nodiscard]] constexpr bool s0_run(proto::watch::Spike s) noexcept {
  using S = proto::watch::Spike;
  return s == S::S0a || s == S::S0b || s == S::S0c || s == S::S0d || s == S::S0dCrash;
}
// The S0 spikes the owner plays (one watched session each); s0a and s0d-crash are runs the launcher closes itself.
[[nodiscard]] constexpr bool s0_session(proto::watch::Spike s) noexcept {
  using S = proto::watch::Spike;
  return s == S::S0b || s == S::S0c || s == S::S0d;
}
// A run with a folder of its own: an observer's (GraceUI's report) or an S0 run's (eldennet.dll's).
[[nodiscard]] constexpr bool run_folder(proto::watch::Spike s) noexcept {
  return proto::watch::observer_run(s) || s0_run(s);
}

inline constexpr std::string_view kSpikeReportFile = "eldennet-spike.json";
inline constexpr std::size_t kMaxSpikeReportBytes = 64u << 10;

// What the launcher reads of eldennet.dll's report of a run.
struct S0Facts {
  std::string state = "none";   // T1's terminal state: ready, inert, banner, exit; none before one
  std::string banner = "none";  // the banner's reason, en-core's word
  std::string waits = "none";   // none, layout, files, build, anticheat, not_started
  std::uint32_t table_items = 0, table_refused = 0;
  bool table_rechecked = false;
  std::uint32_t table_again_refused = 0, table_changed = 0;
  std::uint32_t getter_calls = 0;
  std::uint32_t forced = 0, caught = 0;  // S0-D's trial, both copy primitives
  std::string inject_group = "none";
  bool reading = false;
  std::uint64_t reading_ms = 0;
};
// The facts of a report's text: nothing for one that is not eldennet.dll's report of a run (JSON whose "report" is
// "spike_run"). Only the keys above are read, each of its own type; any other value leaves its default.
[[nodiscard]] std::optional<S0Facts> read_s0_facts(std::string_view text);
// S0-A's rule for one run's table (DESIGN §16.1 since 2026-11-22): its ids, when the layout holds a table, all passed
// their checks at the start and again once it had stopped, none refused and none changed.
[[nodiscard]] bool table_passed(const S0Facts& f) noexcept;
// Who says a line on the console: "EldenNet S0-A: ".
[[nodiscard]] std::string s0_who(proto::watch::Spike spike);

// The S0 kind's tasks for the tester's panel, for `run` of `runs` (s0a and s0d-crash: each run its own short list).
[[nodiscard]] std::vector<Task> s0_tasks(proto::watch::Spike spike, std::uint32_t run, std::uint32_t runs);
// The tasks once the start has stopped short of its reading for want of something (f.waits): what is left to do.
[[nodiscard]] std::vector<Task> s0_waiting_tasks(proto::watch::Spike spike, const S0Facts& f);

// The console's plain words: as the run starts; once the start has stopped (what it did, and what waits); at the end,
// with the launcher's tally and its rule's verdict.
[[nodiscard]] std::string s0_begin_text(proto::watch::Spike spike, std::uint32_t run, std::uint32_t runs);
[[nodiscard]] std::string s0_stopped_text(proto::watch::Spike spike, const S0Facts& f);
[[nodiscard]] std::string s0_waits_text(proto::watch::Spike spike, const S0Facts& f);
[[nodiscard]] std::string s0_missing_text(proto::watch::Spike spike);  // the start stopped, and no report was found
[[nodiscard]] std::string s0_end_text(proto::watch::Spike spike, const Tally& t, bool passed);

}  // namespace en::launcher
