#pragma once
// System › Performance (PAGES §4.6.7; DESIGN §1.3's C3; roadmap S11.4.a): what EldenNet measured of its own work this
// session and what it keeps, in plain data the DLL fills on T6 and en-overlay's space publishes (the topic
// `system.performance`).
//
// - **Its own steps**, from their histograms (en-gal's C13 histogram, µs in quarter-octave buckets: a step's 99th
//   percentile is its bucket's upper bound, so it reads up to a fifth high and never low; the mean and the longest are
//   exact): the reader's every step on T2, each reading of the game it built, and the core's every step on T6.
// - **Never a figure of GraceUI's or the game's as EldenNet's.** This version registers no callback on GraceUI's frame
//   and runs none of its code on the game's threads (O101), so it has no frame cost of its own to give, with the menu
//   open or closed; and it counts none of its memory. Those figures are left out until they have a source.
// - **What it keeps**: no database in this version; the conversations' log is held in memory alone (en-core's
//   MemoryLog), its entries, those that gave way, and how many it holds at most. Never a path or a SteamID.
#include <cstddef>
#include <cstdint>

namespace en::proto {

// One kind of step's times this session; steps 0: none yet (its rows are left out).
struct StepTimes {
  std::uint64_t steps = 0;
  std::uint64_t sum_us = 0;  // the mean is sum_us ÷ steps
  std::uint64_t p99_us = 0;  // the 99th percentile's bucket's upper bound, at most max_us
  std::uint64_t max_us = 0;

  friend bool operator==(const StepTimes&, const StepTimes&) = default;
};

struct PerfView {
  StepTimes reader;   // T2: the reader's every step (C3's task)
  StepTimes reading;  // T2: each reading of the game built (a snapshot)
  StepTimes core;     // T6: the core's every step (a reading, an event of the model client or the surfaces, a tick)
  bool log_known = false;            // the conversations' log below is the core's (the DLL fills it each wake)
  std::uint64_t log_entries = 0;     // in memory now: its lines, and its conversations' starts and ends
  std::uint64_t log_dropped = 0;     // the oldest that gave way to newer ones this session
  std::size_t log_capacity = 0;      // the most it holds

  friend bool operator==(const PerfView&, const PerfView&) = default;
};

// C3's limits for the reader's step (the task, at 32 characters): its mean and its 99th percentile. C3's others (the
// render feed, EldenNet's panels, its on_present callback and the frame rate) have no figure of EldenNet's own in this
// version.
inline constexpr std::uint64_t kReaderMeanUs = 150;
inline constexpr std::uint64_t kReaderP99Us = 500;

}  // namespace en::proto
