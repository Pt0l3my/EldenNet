#pragma once
// System › Performance's and System › Traces' invented facts for the fixture states (PAGES §4.6.7;
// tests/pages/<state>/topics.json): EldenNet's own steps' times as the DLL fills them from their histograms (en-proto's
// perf.hpp; every 99th percentile a bucket's upper bound, as the histogram gives it) and the conversations' log in
// memory; and the traces the calls' record keeps (en-proto's traces.hpp), the world's the lines of calls_fixture.hpp's
// conversation, with the same ids and times. No text of any kind: numbers, kinds and jobs.
#include <cstdint>
#include <string>
#include <string_view>

#include "en/proto/perf.hpp"
#include "en/proto/traces.hpp"

namespace en::overlay::test {

namespace pr = en::proto;

inline pr::StepTimes steps_of(std::uint64_t steps, std::uint64_t mean_us, std::uint64_t p99_us,
                              std::uint64_t max_us) {
  return pr::StepTimes{steps, steps * mean_us, p99_us, max_us};
}

// Starting: the core's first steps, no reading yet, the log empty. At the title the reader's steps and readings within
// C3's limits; in the world the reader's p99 over its 0.5 ms limit (a warning), the log holding a conversation's
// entries; in the multiplayer state the log full, its oldest given way; at L1 (an unknown build) the core alone.
inline const pr::PerfView* perf_fixture(std::string_view state) {
  const auto base = [](std::uint64_t entries) {
    pr::PerfView p;
    p.log_known = true;
    p.log_entries = entries;
    p.log_capacity = 4096;
    return p;
  };
  static const pr::PerfView starting = [&] {
    pr::PerfView p = base(0);
    p.core = steps_of(120, 14, 47, 210);
    return p;
  }();
  static const pr::PerfView title = [&] {
    pr::PerfView p = base(0);
    p.reader = steps_of(22'410, 41, 127, 910);
    p.reading = steps_of(1'200, 85, 191, 1'450);
    p.core = steps_of(15'300, 18, 63, 2'900);
    return p;
  }();
  static const pr::PerfView world = [&] {
    pr::PerfView p = base(37);
    p.reader = steps_of(117'800, 120, 639, 4'800);
    p.reading = steps_of(9'600, 160, 383, 3'900);
    p.core = steps_of(80'100, 22, 95, 6'100);
    return p;
  }();
  static const pr::PerfView multiplayer = [&] {
    pr::PerfView p = world;
    p.log_entries = 4096;
    p.log_dropped = 212;
    return p;
  }();
  static const pr::PerfView banner = [&] {
    pr::PerfView p = base(0);
    p.core = steps_of(40, 12, 31, 95);
    return p;
  }();
  if (state == "starting") return &starting;
  if (state == "title") return &title;
  if (state == "world") return &world;
  if (state == "multiplayer") return &multiplayer;
  return &banner;
}

inline pr::TraceRow line(std::uint64_t id, std::uint64_t at_ms, std::int64_t queue, std::int64_t first_token,
                         std::int64_t first_page, std::int64_t total) {
  pr::TraceRow r;
  r.id = id;
  r.at_ms = at_ms;
  r.kind = pr::LineKind::Reply;
  r.slot = std::string(pr::kDialogue);
  r.queue_ms = queue;
  r.first_token_ms = first_token;
  r.first_page_ms = first_page;
  r.total_ms = total;
  r.verdict = pr::verdict_of(r.kind, first_page);
  return r;
}

// Until the world no line: the title's calls are the self-test's and the model lists, which are no lines. In the world
// calls_fixture.hpp's conversation's replies: waiting; shown, quick; by the fallback, slow; withheld, the persona's own
// line its first page, quick; timed out and failed, no page; and the first, quick. The session's summary counts 21
// lines shown, so its 90th percentile is given, over the 3.0 s target.
inline const pr::TracesView* traces_fixture(std::string_view state) {
  static const pr::TracesView world = [] {
    pr::TracesView v;
    v.revision = 41;
    v.rows = {line(19, 3'905'000, -1, -1, -1, -1),        line(18, 3'655'000, 40, 640, 1'180, 1'720),
              line(17, 3'642'000, 40, 2'950, 3'420, 4'100), line(16, 3'610'000, 30, 700, 1'450, 1'300),
              line(15, 3'590'000, 25, -1, -1, 6'000),       line(13, 3'500'000, 20, -1, -1, 2'200),
              line(12, 3'480'000, 30, 610, 1'090, 1'540)};
    pr::TraceSummary& s = v.summary;
    s.calls = 24;
    s.lines = 21;
    s.unshown = 2;
    s.first_page_median_ms = 1'240;
    s.first_page_p90_ms = 3'180;
    s.first_token_median_ms = 660;
    return v;
  }();
  static const pr::TracesView none;
  if (state == "world" || state == "multiplayer") return &world;
  return &none;
}

}  // namespace en::overlay::test
