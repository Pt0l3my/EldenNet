// The S0 spikes in the launcher (s0.hpp): eldennet.dll's report read, S0-A's rule for a run's table, the S0 kind's
// tasks, the console's words, and the tally with a start that waits for its layout.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/launcher/options.hpp"
#include "en/launcher/record.hpp"
#include "en/launcher/s0.hpp"

using namespace en::launcher;
namespace w = en::proto::watch;

namespace {

// A report as eldennet.dll writes it (en_shell's spikerun.hpp), with the keys the launcher reads.
const char* const kReady = R"({
  "report": "spike_run",
  "schema": 1,
  "spike": "s0a",
  "pid": 77,
  "state": "ready",
  "banner": "none",
  "waits": "none",
  "discovery": {"ran": true, "ready": true, "getter_calls": 0},
  "table": {"items": 15, "refused": 0, "rechecked": true, "entries": 15, "again_refused": 0, "changed": 0},
  "trial": {"ran": true, "seh_forced": 20, "seh_caught": 20, "rpm_forced": 20, "rpm_caught": 19},
  "inject": {"group": "camera", "every": 1, "after_s": 2},
  "reading": {"started": true, "ms": 4100, "snapshots": 41}
})";

const char* const kWaits = R"({"report": "spike_run", "schema": 1, "spike": "s0b", "state": "banner",
  "banner": "discovery_failed", "waits": "layout", "table": {"items": 14, "refused": 0, "rechecked": true}})";

bool holds(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("s0: the facts of eldennet.dll's report of a run") {
  const auto f = read_s0_facts(kReady);
  REQUIRE(f.has_value());
  CHECK(f->state == "ready");
  CHECK(f->waits == "none");
  CHECK(f->table_items == 15);
  CHECK(f->table_rechecked);
  CHECK(f->forced == 40);
  CHECK(f->caught == 39);
  CHECK(f->inject_group == "camera");
  CHECK(f->reading);
  CHECK(f->reading_ms == 4100);
  CHECK(table_passed(*f));
  const auto g = read_s0_facts(kWaits);
  REQUIRE(g.has_value());
  CHECK(g->waits == "layout");
  CHECK(g->banner == "discovery_failed");
  CHECK(g->forced == 0);
  CHECK(table_passed(*g));
  // Not such a report: another kind, not JSON, not an object, too large. A value of the wrong type keeps its default.
  CHECK_FALSE(read_s0_facts(R"({"report": "lodestar", "schema": 2})").has_value());
  CHECK_FALSE(read_s0_facts("not json").has_value());
  CHECK_FALSE(read_s0_facts("[1, 2]").has_value());
  CHECK_FALSE(read_s0_facts(std::string(kMaxSpikeReportBytes + 1, ' ')).has_value());
  const auto odd = read_s0_facts(R"({"report": "spike_run", "state": 3, "table": {"items": "x", "refused": -1}})");
  REQUIRE(odd.has_value());
  CHECK(odd->state == "none");
  CHECK(odd->table_items == 0);
}

TEST_CASE("s0: S0-A's rule for one run's table") {
  S0Facts f;
  CHECK(table_passed(f));  // no table: nothing of it to fail
  f.table_items = 15;
  CHECK_FALSE(table_passed(f));  // not checked again
  f.table_rechecked = true;
  CHECK(table_passed(f));
  f.table_refused = 1;
  CHECK_FALSE(table_passed(f));
  f.table_refused = 0;
  f.table_changed = 1;
  CHECK_FALSE(table_passed(f));
  f.table_changed = 0;
  f.table_again_refused = 1;
  CHECK_FALSE(table_passed(f));
}

TEST_CASE("s0: the S0 kind's tasks, beside the read round's own") {
  // S0-B: the title, the save, two hours counted down, then quit; the owner may stop early.
  const auto b = s0_tasks(w::Spike::S0b, 1, 1);
  REQUIRE(b.size() == 4);
  CHECK(b.front().kind == TaskKind::Round);
  CHECK(b[1].kind == TaskKind::Do);
  CHECK(b[1].mark);
  CHECK(b[2].kind == TaskKind::Wait);
  CHECK(b[2].seconds == 7200);
  CHECK(holds(b[2].detail, "stop early"));
  CHECK(b.back().kind == TaskKind::End);
  // S0-C and S0-D: the owner's steps, each list ending with the game's end; S0-D's Steam overlay.
  for (const w::Spike s : {w::Spike::S0c, w::Spike::S0d}) {
    const auto t = s0_tasks(s, 1, 1);
    REQUIRE(t.size() >= 4);
    CHECK(t.front().kind == TaskKind::Round);
    CHECK(t.back().kind == TaskKind::End);
    std::size_t ends = 0;
    for (const Task& x : t) ends += x.kind == TaskKind::End ? 1 : 0;
    CHECK(ends == 1);
  }
  CHECK(holds(s0_tasks(w::Spike::S0d, 1, 1)[2].detail, "Shift+Tab"));
  // S0-A and S0-D's crashes: each run its own short list, numbered.
  const auto a = s0_tasks(w::Spike::S0a, 3, 20);
  REQUIRE(a.size() == 2);
  CHECK(holds(a.front().title, "Run 3 of 20"));
  CHECK(a.back().kind == TaskKind::End);
  CHECK(holds(s0_tasks(w::Spike::S0a, 20, 20).back().detail, "last run"));
  CHECK(holds(s0_tasks(w::Spike::S0dCrash, 1, 5).front().detail, "crash"));
  CHECK(s0_tasks(w::Spike::S0e, 1, 1).empty());
  // The read round's list is its own, unchanged by these.
  CHECK(read_tasks().front().kind == TaskKind::Round);
}

TEST_CASE("s0: a spike that cannot run whole says why, in plain words, and only S0-B's list ends there") {
  S0Facts f;
  f.state = "banner";
  f.banner = "discovery_failed";
  f.waits = "layout";
  f.table_items = 14;
  f.table_rechecked = true;
  f.forced = 40;
  f.caught = 40;
  CHECK(holds(s0_waits_text(w::Spike::S0b, f), "The start's layout is not complete yet: S0-B waits for it."));
  CHECK(holds(s0_waits_text(w::Spike::S0a, f), "counts as L1"));
  CHECK(holds(s0_waits_text(w::Spike::S0c, f), "gate waits"));
  CHECK(holds(s0_waits_text(w::Spike::S0d, f), "40 of 40 caught"));
  const auto b = s0_waiting_tasks(w::Spike::S0b, f);
  REQUIRE(b.size() == 1);
  CHECK(b.front().kind == TaskKind::End);
  CHECK(holds(b.front().detail, "S0-B waits for it"));
  CHECK(s0_waiting_tasks(w::Spike::S0c, f).empty());
  CHECK(s0_waiting_tasks(w::Spike::S0d, f).empty());
  const std::string stopped = s0_stopped_text(w::Spike::S0d, f);
  CHECK(holds(stopped, "banner (discovery_failed)"));
  CHECK(holds(stopped, "All 14 ids"));
  CHECK(holds(stopped, "40 of 40 caught"));
  f.waits = "build";
  CHECK(holds(s0_waits_text(w::Spike::S0b, f), "build is not one EldenNet knows"));
  f.state = "ready";
  CHECK(holds(s0_stopped_text(w::Spike::S0b, f), "ready"));
  CHECK(s0_who(w::Spike::S0b) == "EldenNet S0-B: ");
  CHECK(holds(s0_begin_text(w::Spike::S0a, 2, 20), "Run 2 of 20"));
  CHECK_FALSE(s0_missing_text(w::Spike::S0c).empty());
}

TEST_CASE("s0: the tally counts a waiting start for nothing, and a table that failed against S0-A") {
  RunResult r;
  r.outcome = RunOutcome::Banner;
  r.folder = 4;
  r.report = true;
  r.waits = "layout";
  std::vector<RunResult> runs(3, r);
  Tally t = tally(runs);
  CHECK(t.reports == 3);
  CHECK(t.waiting == 3);
  CHECK(t.table_failed == 0);
  CHECK(passed(w::Spike::S0a, t));
  CHECK(passed(w::Spike::S0b, tally(std::vector<RunResult>{r})));
  runs[1].table_ok = false;
  t = tally(runs);
  CHECK(t.table_failed == 1);
  CHECK_FALSE(passed(w::Spike::S0a, t));
  CHECK(holds(s0_end_text(w::Spike::S0a, t, false), "does not pass"));
  CHECK(holds(s0_end_text(w::Spike::S0b, tally(std::vector<RunResult>{r}), true), "without a crash"));
  // The run's line keeps its folder and what the report said; a run with no folder has neither.
  const auto j = to_json(r, w::Spike::S0b);
  CHECK(j["folder"] == 4);
  CHECK(j["waits"] == "layout");
  CHECK(j["report"] == true);
  CHECK_FALSE(to_json(RunResult{}, w::Spike::S0a).contains("folder"));
  CHECK(to_json(t, w::Spike::S0a)["table_failed"] == 1);
}

TEST_CASE("s0: --spike-pace-s, the tests' pace for an S0 run's tasks, goes with an S0 spike alone") {
  const auto ok = parse_options({"--spike", "s0b", "--spike-pace-s", "3"});
  REQUIRE(ok.has_value());
  CHECK(ok->spike_pace_s == 3);
  CHECK(parse_options({"--spike", "s0a", "--runs", "2", "--spike-pace-s", "1"}).has_value());
  CHECK_FALSE(parse_options({"--spike-pace-s", "3"}).has_value());
  CHECK_FALSE(parse_options({"--spike", "s0e", "--spike-pace-s", "3"}).has_value());
  CHECK_FALSE(parse_options({"--spike", "s0b", "--spike-pace-s", "0"}).has_value());
  CHECK_FALSE(parse_options({"--spike", "s0b", "--spike-pace-s", "601"}).has_value());
  CHECK(usage_text().find("--spike-pace-s") != std::string::npos);
}
