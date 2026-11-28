// MEM §9.6's oracle at the core's level (S12.3.a): `wren_and_kale` through the eval with its store kept, trimmed at
// T1 to T3 by each path the store has (the silent trim, Forget after the choice, a save of an earlier journey), and
// each trimmed file compared with a replay of the same scenario up to the cut; and Keep and New character through the
// core. All data is invented.
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "en/store/db.hpp"
#include "eval_run.hpp"
#include "golden_run.hpp"

using namespace en;
using namespace en::tools;
namespace fs = std::filesystem;
using Kind = EvalStep::Kind;

namespace {

const fs::path kTalk = fs::path(EN_TOOLS_SCENARIOS) / "talk";

EvalScenario scenario(const char* name) {
  const fs::path path = kTalk / name;
  const auto text = read_file(path.string(), 256 * 1024);
  REQUIRE(text.has_value());
  auto s = parse_eval_scenario(*text, path.parent_path());
  REQUIRE_MESSAGE(s.has_value(), (s ? std::string() : s.error()));
  return std::move(*s);
}

// A folder of the test's own, removed after.
class Folder {
 public:
  Folder() {
    std::random_device rd;
    path_ = fs::temp_directory_path() / ("en-oracle-" + std::to_string(rd()) + "-" + std::to_string(rd()));
    fs::create_directories(path_);
  }
  ~Folder() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  Folder(const Folder&) = delete;
  Folder& operator=(const Folder&) = delete;
  [[nodiscard]] fs::path at(const std::string& name) const { return path_ / name; }

 private:
  fs::path path_;
};

EvalStep step(Kind k) {
  EvalStep s;
  s.kind = k;
  return s;
}
EvalStep wait(core::Mono ms) {
  EvalStep s = step(Kind::Wait);
  s.ms = ms;
  return s;
}
EvalStep say(const char* text) {
  EvalStep s = step(Kind::Say);
  s.text = text;
  return s;
}
EvalStep choose(const char* answer) {
  EvalStep s = step(Kind::Choose);
  s.choice = answer;
  return s;
}
EvalStep expect_mode(const char* mode) {
  EvalStep s = step(Kind::Expect);
  s.mode = mode;
  return s;
}

std::size_t index_of(const EvalScenario& s, Kind k, std::size_t from = 0) {
  for (std::size_t i = from; i < s.steps.size(); ++i) {
    if (s.steps[i].kind == k) return i;
  }
  FAIL("the scenario has no such step");
  return 0;
}

EvalScenario truncated(const EvalScenario& s, std::size_t n) {
  EvalScenario t = s;
  t.steps.resize(n);
  return t;
}

// Runs `s` with its store kept in `dir`, the save first loaded at `start_pt` when given; every expectation must hold.
EvalRun run_kept(const EvalScenario& s, const fs::path& dir, std::optional<std::int64_t> start_pt = std::nullopt) {
  EvalOptions o;
  o.store_dir = dir;
  o.start_pt = start_pt;
  auto run = run_eval(s, EvalLlm::Mock, o);
  REQUIRE_MESSAGE(run.has_value(), (run ? std::string() : run.error()));
  for (const std::string& f : run->failed) FAIL_CHECK(f);
  return std::move(*run);
}

// The playthrough's live file under a kept store folder: the one .sqlite outside backups/ and archive/.
fs::path live_file(const fs::path& dir) {
  std::vector<fs::path> found;
  for (const auto& e : fs::recursive_directory_iterator(dir / "saves")) {
    const fs::path& p = e.path();
    const std::string parent = p.parent_path().filename().string();
    if (p.extension() == ".sqlite" && parent != "backups" && parent != "archive") found.push_back(p);
  }
  REQUIRE(found.size() == 1);
  return found.front();
}

std::vector<fs::path> archived_files(const fs::path& dir) {
  std::vector<fs::path> out;
  for (const auto& e : fs::recursive_directory_iterator(dir / "saves")) {
    if (e.path().extension() == ".sqlite" && e.path().parent_path().filename() == "archive") out.push_back(e.path());
  }
  return out;
}

// A query's rows, each as its columns' text joined ("-" for NULL).
std::vector<std::string> rows(const fs::path& file, const std::string& sql) {
  auto db = store::Db::open(file, store::FileKind::Playthrough, false);
  REQUIRE(db.has_value());
  auto s = store::Stmt::prepare(*db, sql);
  REQUIRE_MESSAGE(s.has_value(), sql);
  std::vector<std::string> out;
  for (;;) {
    auto r = s->step();
    REQUIRE(r.has_value());
    if (!*r) break;
    std::string line;
    for (int i = 0; i < s->columns(); ++i) line += (s->null(i) ? std::string("-") : s->text(i)) + "|";
    out.push_back(line);
  }
  return out;
}
std::string one(const fs::path& file, const std::string& sql) {
  const auto r = rows(file, sql);
  REQUIRE(r.size() == 1);
  return r.front().substr(0, r.front().size() - 1);
}
std::int64_t number(const fs::path& file, const std::string& sql) { return std::stoll(one(file, sql)); }

void same_rows(const std::vector<std::string>& got, const std::vector<std::string>& want, const char* what) {
  if (got == want) return;
  std::string diff;
  for (const std::string& r : got) {
    if (std::find(want.begin(), want.end(), r) == want.end()) diff += "\n  trimmed only: " + r;
  }
  for (const std::string& r : want) {
    if (std::find(got.begin(), got.end(), r) == got.end()) diff += "\n  replay only:  " + r;
  }
  if (diff.empty()) diff = "\n  the same rows in another order";
  FAIL_CHECK(what << ":" << diff);
}

// Every table of the playthrough file as the oracle compares it (MEM §9.6: every table, wall times left out), up to
// the cut: the trimmed side's own session adds rows after it (its boot's PlaythroughReady), the replay none. `open`
// lists the conversations the replay has open at the cut, whose end and counts the trim alone writes (checked apart).
struct Compared {
  const char* table;
  std::string sql;
};
std::vector<Compared> compared(std::int64_t cut, std::int64_t next_event, const std::string& open) {
  const std::string at = std::to_string(cut);
  const std::string c = at + " AND id < " + std::to_string(next_event);  // events: none of the trimmed side's session
  const std::string not_open = open.empty() ? std::string("1") : "id NOT IN (" + open + ")";
  const std::string jobs_not_open =
      open.empty() ? std::string("1") : "(conv_id IS NULL OR conv_id NOT IN (" + open + "))";
  return {
      {"events", "SELECT id, journey, epoch, t_play_ms, t_play_end_ms, kind, rule, source, conf, subject, object, "
                 "conv_id, place_id, region, block, tod, dkey, text, payload FROM events WHERE t_play_ms <= " + c +
                     " ORDER BY id"},
      {"event_audience", "SELECT a.event_id, a.persona, a.journey, a.how FROM event_audience a JOIN events e ON e.id "
                         "= a.event_id WHERE e.t_play_ms <= " + at + " AND e.id < " + std::to_string(next_event) +
                             " ORDER BY 1, 2"},
      {"conversations", "SELECT id, journey, epoch, persona, stage, started_pt, ended_pt, end_reason, player_lines, "
                        "committed_lines, first_event_id, last_event_id, shown_context, persona_hash, kb_hash, "
                        "model_fp FROM conversations WHERE started_pt <= " + at + " AND " + not_open + " ORDER BY id"},
      {"conversations open at the cut", "SELECT id, journey, epoch, persona, stage, started_pt, first_event_id, "
                                        "persona_hash, kb_hash, model_fp FROM conversations WHERE started_pt <= " + at +
                                            " AND NOT (" + not_open + ") ORDER BY id"},
      {"npc_facts", "SELECT journey, persona, key, value, src_event, updated_pt FROM npc_facts ORDER BY 1, 2, 3"},
      {"npc_journal", "SELECT journey, persona, version, as_of_pt, from_event_id, upto_event_id, conv_id, summary, "
                      "keepsakes, next_keepsake, disposition, told_name, told_name_verified, told_name_heard, "
                      "model_fp, prompt_hash, persona_hash, job_id FROM npc_journal ORDER BY 1, 2, 3"},
      {"npc_lived", "SELECT journey, persona, version, journal_version, as_of_pt, towards, aims, learned, self_line, "
                    "self_pt, manner, manner_pt, next_entry, model_fp, prompt_hash, persona_hash, job_id FROM "
                    "npc_lived ORDER BY 1, 2, 3"},
      {"memory_progress", "SELECT journey, persona, watermark, updated_pt FROM memory_progress ORDER BY 1, 2"},
      {"memory_jobs", "SELECT id, journey, persona, kind, cause, conv_id, from_event_id, upto_event_id, "
                      "journal_version, state, attempts, created_pt, last_error FROM memory_jobs WHERE " +
                          jobs_not_open + " ORDER BY id"},
      {"memories", "SELECT id, journey, persona, type, basis, about, emotion, importance, tags, place_id, content, "
                   "src_events, first_event_id, last_event_id, t_play_ms FROM memories ORDER BY id"},
      {"decisions", "SELECT id, journey, t_play_ms, task, subject, options, chosen FROM decisions ORDER BY id"},
      {"npc_intents", "SELECT id, journey, persona, event_id, t_play_ms, raw, status FROM npc_intents ORDER BY id"},
      {"epochs", "SELECT epoch, journey, start_pt, start_event_id, reason FROM epochs ORDER BY epoch"},
      {"meta", "SELECT k, v FROM meta WHERE k NOT IN ('next_event_id', 'next_conv_id') ORDER BY k"},
  };
}

// MEM §9.6's property checks on a trimmed file: no row of the current epoch past the cut but the trim's own session's
// (its ids at or past the counters the full file had); no orphaned audience; each watermark the current version's
// `upto`; each current lived version's `as_of_pt` at most its journal's (§13.7).
void properties(const fs::path& file, std::int64_t cut, std::int64_t next_event, std::int64_t next_conv) {
  const std::string c = std::to_string(cut);
  CHECK(number(file, "SELECT count(*) FROM events WHERE t_play_ms > " + c + " AND id < " +
                         std::to_string(next_event)) == 0);
  CHECK(number(file, "SELECT count(*) FROM conversations WHERE started_pt > " + c + " AND id < " +
                         std::to_string(next_conv)) == 0);
  CHECK(number(file, "SELECT count(*) FROM npc_journal WHERE as_of_pt > " + c) == 0);
  CHECK(number(file, "SELECT count(*) FROM memory_jobs WHERE created_pt > " + c) == 0);
  CHECK(number(file, "SELECT count(*) FROM npc_facts WHERE updated_pt > " + c) == 0);
  CHECK(number(file, "SELECT count(*) FROM npc_lived WHERE as_of_pt > " + c) == 0);
  CHECK(number(file, "SELECT count(*) FROM event_audience WHERE event_id NOT IN (SELECT id FROM events)") == 0);
  CHECK(number(file, "SELECT count(*) FROM memory_progress m WHERE m.watermark <> coalesce((SELECT v.upto_event_id "
                     "FROM npc_journal v WHERE v.journey = m.journey AND v.persona = m.persona ORDER BY v.version DESC "
                     "LIMIT 1), 0)") == 0);
  CHECK(number(file, "SELECT count(*) FROM npc_lived l WHERE l.version = (SELECT max(version) FROM npc_lived x WHERE "
                     "x.journey = l.journey AND x.persona = l.persona) AND l.as_of_pt > coalesce((SELECT max(as_of_pt) "
                     "FROM npc_journal v WHERE v.journey = l.journey AND v.persona = l.persona), 0)") == 0);
}

// The world the trimmed side loads into: the same character and flag state as at the cut, everyone far off, so that
// its session writes nothing but its own PlaythroughReady.
EvalScenario quiet_load(const EvalScenario& s, std::size_t cut_step, std::vector<EvalStep> steps) {
  EvalScenario q = s;
  for (std::size_t i = 0; i < cut_step && i < s.steps.size(); ++i) {
    if (s.steps[i].kind == Kind::Flags) q.flags = s.steps[i].flags;
  }
  q.distance_m = 400.0f;
  for (EvalScenario::Extra& x : q.extras) x.distance_m = 400.0f;
  q.steps = std::move(steps);
  return q;
}

// A load with nothing to do but open the file, as it was left (the world as the scenario starts it).
EvalScenario kept_load(const EvalScenario& s) {
  EvalScenario q = s;
  q.steps = {expect_mode("open")};
  return q;
}

// The scenario up to NG+, then six minutes and one more conversation, all in journey 1.
EvalScenario late_of(const EvalScenario& s) {
  EvalScenario late = truncated(s, index_of(s, Kind::NgPlus));
  for (const EvalStep& x : {wait(360000), step(Kind::Open), say("Still here, then."), wait(7000), step(Kind::Esc)}) {
    late.steps.push_back(x);
  }
  return late;
}

// The core opens the file at its first snapshot in the world, a little after the save's play time when loaded: a
// load that is to trim at a cut loads this much before it (the full run's first load was at 600,000 ms).
std::int64_t open_lag_of(const fs::path& full_file) {
  return number(full_file, "SELECT t_play_ms FROM events WHERE kind = 'PlaythroughReady' ORDER BY id LIMIT 1") -
         600000;
}

struct Cut {
  std::string name;
  std::int64_t pt = 0;
  std::size_t steps = 0;    // the replay: these steps, then a wait to the cut
  core::Mono tail_ms = 0;   //   of this many ms
  bool drain = false;       //   and its queued jobs drained (no conversation is open)
};

// The cut at `pt`: the steps that end by it and the wait from the last one's end.
Cut cut_at(std::string name, const EvalRun& full, std::int64_t pt, bool drain) {
  Cut c{std::move(name), pt, 0, 0, drain};
  for (std::size_t i = 0; i + 1 < full.step_pt.size(); ++i) {
    if (full.step_pt[i + 1] <= pt) c.steps = i + 1;
  }
  c.tail_ms = static_cast<core::Mono>(pt - full.step_pt[c.steps]);
  return c;
}

// Each cut of `s` by every path the store has, against a replay of `s` up to it (the doc at the top), with MEM's own
// checks for the cut (`checks`, on each trimmed file). `full` is `s` run with its store kept in folder `full`.
using CutChecks = std::function<void(const Cut&, const fs::path& file)>;
void oracle(const EvalScenario& s, const Folder& folder, const EvalRun& full, const std::vector<Cut>& cuts,
            const CutChecks& checks) {
  const std::size_t ng = index_of(s, Kind::NgPlus);
  const EvalScenario late = late_of(s);
  const std::int64_t open_lag = open_lag_of(live_file(folder.at("full")));
  for (const Cut& cut : cuts) {
    CAPTURE(cut.name);
    // The replay: the scenario up to the cut, its queued jobs drained.
    const std::string rname = "replay-" + cut.name;
    EvalScenario r = truncated(s, cut.steps);
    if (cut.tail_ms > 0) r.steps.push_back(wait(cut.tail_ms));
    if (cut.drain) r.steps.push_back(step(Kind::WaitJobs));
    (void)run_kept(r, folder.at(rname));
    const fs::path replay = live_file(folder.at(rname));
    std::string open;
    for (const std::string& id : rows(replay, "SELECT id FROM conversations WHERE ended_pt IS NULL")) {
      open += (open.empty() ? "" : ",") + id.substr(0, id.size() - 1);
    }
    CHECK(rows(replay, "SELECT id FROM memory_jobs WHERE state <> 'done'").empty());  // nothing left to drain
    // Each path: the silent trim (a file whose newest event is within 5 min of the cut), Forget after the choice (one
    // past 5 min), and a save of journey 1 against the file in journey 2 (the choice, then Forget).
    struct Path {
      const char* name;
      EvalScenario source;
      bool asks;
    };
    std::vector<Path> paths;
    {
      std::size_t m = 0;
      for (std::size_t i = 0; i < ng; ++i) {
        if (full.step_pt[i] <= cut.pt + 240000) m = i + 1;
      }
      if (m > cut.steps + (cut.tail_ms > 0 ? 1 : 0)) paths.push_back({"silent", truncated(s, m), false});
    }
    paths.push_back({"forget", late, true});
    paths.push_back({"journey", s, true});
    for (const Path& path : paths) {
      const std::string path_name = path.name;
      CAPTURE(path_name);
      const std::string name = cut.name + "-" + path.name;
      (void)run_kept(path.source, folder.at(name));
      const fs::path file = live_file(folder.at(name));
      const std::int64_t next_event = number(file, "SELECT v FROM meta WHERE k = 'next_event_id'");
      const std::int64_t next_conv = number(file, "SELECT v FROM meta WHERE k = 'next_conv_id'");
      const auto journal_before = rows(file, "SELECT * FROM npc_journal WHERE journey = 1 AND as_of_pt <= " +
                                                 std::to_string(cut.pt) + " ORDER BY persona, version");
      std::vector<EvalStep> load;
      if (path.asks) {
        load = {expect_mode("choice"), choose("forget"), expect_mode("open")};
      } else {
        load = {expect_mode("open")};
      }
      (void)run_kept(quiet_load(s, cut.steps + 1, std::move(load)), folder.at(name), cut.pt - open_lag);
      // The file was opened, and trimmed, at the cut: its own session's PlaythroughReady says when.
      CHECK(number(file, "SELECT t_play_ms FROM events WHERE kind = 'PlaythroughReady' AND id >= " +
                             std::to_string(next_event)) == cut.pt);
      for (const Compared& q : compared(cut.pt, next_event, open)) {
        const std::string table = q.table;
        CAPTURE(table);
        same_rows(rows(file, q.sql), rows(replay, q.sql), q.table);
      }
      properties(file, cut.pt, next_event, next_conv);
      // A journal version written before the cut is the file's own, every byte (T3: Kalé's journey-1 journal).
      CHECK(rows(file, "SELECT * FROM npc_journal WHERE journey = 1 ORDER BY persona, version") == journal_before);
      checks(cut, file);
    }
  }
}

}  // namespace

TEST_CASE("oracle (MEM 9.6): T1 to T3 on wren_and_kale, by every path, equal a replay up to the cut") {
  const EvalScenario s = scenario("wren_and_kale.toml");
  Folder folder;
  // The full run, and a longer one that stays in journey 1, six minutes past its end with one more conversation (so
  // that every cut is more than 5 min before the file's newest event: the choice, then Forget).
  const EvalRun full = run_kept(s, folder.at("full"));
  const fs::path full_file = live_file(folder.at("full"));
  // T1: conv C (the second conversation), between the first and second page of Kalé's second reply; T2: after the
  // castle's lord fell, before the strike; T3: the last of journey 1, before NG+.
  const std::string two_pages =
      one(full_file, "SELECT json_extract(payload, '$.pages[0].pt') || ':' || json_extract(payload, '$.pages[1].pt') "
                     "FROM events WHERE kind = 'NpcSaid' AND conv_id = 2 AND "
                     "json_array_length(payload, '$.pages') = 2");
  const std::int64_t p1 = std::stoll(two_pages.substr(0, two_pages.find(':')));
  const std::int64_t p2 = std::stoll(two_pages.substr(two_pages.find(':') + 1));
  const std::vector<Cut> cuts = {
      cut_at("T1", full, (p1 + p2) / 2, false),
      cut_at("T2", full, full.step_pt[index_of(s, Kind::Strike)], true),
      cut_at("T3", full, full.step_pt[index_of(s, Kind::NgPlus)], true),
  };
  oracle(s, folder, full, cuts, [&](const Cut& cut, const fs::path& file) {
    const std::string kale = "SELECT value FROM npc_facts WHERE journey = 1 AND persona = 'kale' AND key = ";
    if (cut.name == "T1") {
      // MEM §9.6's T1 on this scenario: the reply across the cut keeps its first page; conv C ends `trimmed`, its
      // job taken back to its last kept line and queued again (it still has 2 of the Tarnished's lines and 4
      // committed, MEM §3.1); journal v1 is current. met_count is 1: conv C merged into A's meeting (within
      // 10 min), the design's §2.8 counting B before C.
      CHECK(one(file, "SELECT text FROM events WHERE kind = 'NpcSaid' AND conv_id = 2 ORDER BY id DESC LIMIT 1") ==
            one(full_file, "SELECT substr(text, 1, json_extract(payload, '$.pages[0].end')) FROM events WHERE kind = "
                           "'NpcSaid' AND conv_id = 2 AND json_array_length(payload, '$.pages') = 2"));
      CHECK(one(file, "SELECT end_reason || ':' || committed_lines FROM conversations WHERE id = 2") == "trimmed:4");
      CHECK(one(file, "SELECT state || ':' || upto_event_id FROM memory_jobs WHERE conv_id = 2") ==
            "queued:" + one(file, "SELECT max(id) FROM events WHERE conv_id = 2"));
      CHECK(one(file, "SELECT max(version) FROM npc_journal WHERE persona = 'kale'") == "1");
      CHECK(one(file, kale + "'met_count'") == "1");
    } else if (cut.name == "T2") {
      // T2: no blow yet; v3 gone, so disposition 0 and no k3; the watermark v2's upto; Kalé's lived v1 (step F2)
      // gone with v3. slew:godrick's proof was written with v3, after the cut, and goes with it: at the cut the
      // claim proves at render, from the flags (§2.8's step D), as the replay's does.
      CHECK(rows(file, kale + "'times_attacked'").empty());
      CHECK(one(file, "SELECT version || ':' || disposition FROM npc_journal WHERE persona = 'kale' ORDER BY "
                      "version DESC LIMIT 1") == "2:0");
      CHECK(one(file, "SELECT json_array_length(keepsakes) FROM npc_journal WHERE persona = 'kale' ORDER BY version "
                      "DESC LIMIT 1") == "2");
      CHECK(rows(file, kale + "'verified:slew:godrick'").empty());
      CHECK(one(file, "SELECT watermark FROM memory_progress WHERE persona = 'kale'") ==
            one(file, "SELECT upto_event_id FROM npc_journal WHERE persona = 'kale' AND version = 2"));
      CHECK(number(file, "SELECT count(*) FROM npc_lived") == 0);
    } else {
      // T3: journey 2's epoch and rows gone; journey 1 current; slew:godrick's proof (v3's, before the cut) kept.
      CHECK(one(file, kale + "'verified:slew:godrick'") == "1");
      CHECK(number(file, "SELECT count(*) FROM epochs WHERE journey = 2") == 0);
      CHECK(number(file, "SELECT count(*) FROM events WHERE journey = 2") == 0);
      CHECK(one(file, "SELECT v FROM meta WHERE k = 'current_journey'") == "1");
    }
  });
}

TEST_CASE("oracle (MEM 9.6): Keep keeps every row in a new epoch, and a later crash trim touches only that epoch") {
  const EvalScenario s = scenario("wren_and_kale.toml");
  Folder folder;
  const EvalRun full = run_kept(late_of(s), folder.at("keep"));
  const fs::path file = live_file(folder.at("keep"));
  const std::int64_t lag = open_lag_of(file);
  const std::string old_event = std::to_string(number(file, "SELECT v FROM meta WHERE k = 'next_event_id'"));
  const std::string old_conv = std::to_string(number(file, "SELECT v FROM meta WHERE k = 'next_conv_id'"));
  // The first timeline, every column of every row but the wall times: what Keep, and a later trim in the new epoch,
  // leave as it was.
  const std::vector<std::string> first = {
      "SELECT * FROM events WHERE id < " + old_event + " ORDER BY id",
      "SELECT * FROM event_audience WHERE event_id < " + old_event + " ORDER BY event_id, persona",
      "SELECT * FROM conversations WHERE id < " + old_conv + " ORDER BY id",
      "SELECT journey, persona, version, as_of_pt, from_event_id, upto_event_id, conv_id, summary, keepsakes, "
      "disposition, told_name, job_id FROM npc_journal ORDER BY persona, version",
      "SELECT journey, persona, version, journal_version, as_of_pt, towards, aims, learned FROM npc_lived ORDER BY "
      "persona, version"};
  std::vector<std::vector<std::string>> before;
  for (const std::string& q : first) before.push_back(rows(file, q));
  REQUIRE(before[3].size() == 3);  // v1 to v3
  REQUIRE(before[4].size() == 1);  // lived v1
  // An older save, after A's conversation and its job, before C's: the choice, Keep, and a new conversation.
  const std::int64_t at = full.step_pt[index_of(s, Kind::Open, index_of(s, Kind::WaitJobs))];
  EvalScenario kept = s;
  kept.flags = "fresh";
  kept.steps = {expect_mode("choice"), choose("keep"), expect_mode("open"), step(Kind::Open), say("Back, as I said."),
                wait(7000), step(Kind::Esc), wait(2000)};
  (void)run_kept(kept, folder.at("keep"), at - lag);
  for (std::size_t i = 0; i < first.size(); ++i) {
    CAPTURE(i);
    CHECK(rows(file, first[i]) == before[i]);
  }
  CHECK(one(file, "SELECT epoch || ':' || journey || ':' || start_pt || ':' || reason FROM epochs ORDER BY epoch DESC "
                  "LIMIT 1") == "2:1:" + std::to_string(at) + ":keep");
  CHECK(one(file, "SELECT v FROM meta WHERE k = 'current_epoch'") == "2");
  CHECK(number(file, "SELECT count(*) FROM events WHERE epoch = 2 AND id < " + old_event) == 0);
  const std::int64_t said = number(file, "SELECT t_play_ms FROM events WHERE epoch = 2 AND kind = 'NpcSaid'");
  // A crash: the save is from just before the new reply, within 5 min of the file's newest event, so the trim is
  // silent, and it cuts the new epoch alone, though every row of the first timeline is timed after it.
  (void)run_kept(kept_load(s), folder.at("keep"), said - 1 - lag);
  for (std::size_t i = 0; i < first.size(); ++i) {
    CAPTURE(i);
    CHECK(rows(file, first[i]) == before[i]);
  }
  CHECK(number(file, "SELECT count(*) FROM events WHERE epoch = 2 AND kind = 'NpcSaid'") == 0);
  CHECK(number(file, "SELECT count(*) FROM events WHERE epoch = 2 AND kind = 'PlayerSaid'") == 1);
  CHECK(one(file, "SELECT end_reason FROM conversations WHERE epoch = 2") == "trimmed");
  CHECK(number(file, "SELECT count(*) FROM epochs") == 2);
}

TEST_CASE("oracle (MEM 9.6, 8.3): New character archives the old file whole and starts an empty one") {
  const EvalScenario s = scenario("wren_and_kale.toml");
  Folder folder;
  (void)run_kept(late_of(s), folder.at("new"));
  const fs::path live = live_file(folder.at("new"));
  const std::vector<std::string> tables = {"SELECT * FROM events ORDER BY id",
                                           "SELECT * FROM conversations ORDER BY id",
                                           "SELECT * FROM npc_journal ORDER BY persona, version",
                                           "SELECT * FROM npc_facts ORDER BY journey, persona, key"};
  std::vector<std::vector<std::string>> before;
  for (const std::string& q : tables) before.push_back(rows(live, q));
  // The same name, origin and gift, its play time near the start: a new character in the old one's slot (MEM §8.3).
  EvalScenario fresh = quiet_load(s, 0, {expect_mode("choice"), choose("new"), expect_mode("open")});
  {
    EvalStep met = step(Kind::Expect);
    met.fact = EvalStep::Fact{"kale", "met_count", "<none>"};
    fresh.steps.push_back(met);
  }
  (void)run_kept(fresh, folder.at("new"), 60000);
  // The old file is in archive/, whole: every row as it was.
  const std::vector<fs::path> archived = archived_files(folder.at("new"));
  REQUIRE(archived.size() == 1);
  for (std::size_t i = 0; i < tables.size(); ++i) {
    CAPTURE(i);
    CHECK(rows(archived.front(), tables[i]) == before[i]);
  }
  // The live file is the new character's: nothing of the old one's.
  CHECK(live_file(folder.at("new")) == live);
  CHECK(number(live, "SELECT count(*) FROM events WHERE kind <> 'PlaythroughReady'") == 0);
  CHECK(number(live, "SELECT count(*) FROM conversations") == 0);
  CHECK(number(live, "SELECT count(*) FROM npc_journal") == 0);
  CHECK(number(live, "SELECT count(*) FROM npc_facts") == 0);
  // Another character in the slot archives the new one's file; this character again restores its newest archive
  // (MEM §8.3, step 3), the new character's, and the old character's file stays in the archive, never deleted. Its
  // fingerprint is the new character's too, so a load does not tell them apart.
  EvalScenario other = s;
  other.character = "Rook";
  other.steps = {expect_mode("open")};
  (void)run_kept(other, folder.at("new"), 90000);
  CHECK(archived_files(folder.at("new")).size() == 2);
  EvalScenario back = s;
  back.steps = {expect_mode("open")};
  (void)run_kept(back, folder.at("new"), 120000);
  CHECK(live_file(folder.at("new")) == live);
  CHECK(number(live, "SELECT count(*) FROM conversations") == 0);
  const std::vector<fs::path> after = archived_files(folder.at("new"));
  CHECK(std::find(after.begin(), after.end(), archived.front()) != after.end());
  CHECK(rows(archived.front(), tables[0]) == before[0]);
}

TEST_CASE("oracle (MEM 9.6, C11): a hard kill mid-reply: every acknowledged page survives, nothing else appears") {
  // The long reply (its pages capped as it streams), killed at each page's boundary, a millisecond before its play
  // time and a frame after, and at seeded moments between the line and the reply's end; the next session closes the
  // conversation as crashed. The file holds exactly the pages the surfaces acknowledged before the kill.
  EvalScenario s = scenario("lantern-long-reply.toml");
  s.store = true;
  s.character = "Wren";
  s.steps = {step(Kind::Open), say("How do you keep the lamp?"), wait(15000), step(Kind::Esc)};
  Folder folder;
  const EvalRun full = run_kept(s, folder.at("full"));
  const fs::path f = live_file(folder.at("full"));
  std::vector<std::int64_t> pages;
  for (const std::string& r : rows(f, "SELECT json_extract(p.value, '$.pt') FROM events e, json_each(e.payload, "
                                      "'$.pages') p WHERE e.kind = 'NpcSaid' ORDER BY 1")) {
    pages.push_back(std::stoll(r));
  }
  REQUIRE(pages.size() >= 2);
  const std::int64_t from = full.step_pt[2];  // the wait after the line
  std::vector<std::int64_t> kills;
  for (const std::int64_t p : pages) {
    kills.push_back(p - 1);
    kills.push_back(p + 250);
  }
  std::mt19937 rng(11);
  std::uniform_int_distribution<std::int64_t> between(from + 1, pages.back() + 1000);
  for (int i = 0; i < 8; ++i) kills.push_back(between(rng));
  for (std::size_t i = 0; i < kills.size(); ++i) {
    const std::int64_t kill = kills[i];
    CAPTURE(kill);
    EvalScenario k = s;
    k.steps = {step(Kind::Open), say("How do you keep the lamp?"), wait(static_cast<core::Mono>(kill - from)),
               step(Kind::Kill), expect_mode("open")};
    const EvalRun run = run_kept(k, folder.at("kill-" + std::to_string(i)));
    const fs::path file = live_file(folder.at("kill-" + std::to_string(i)));
    REQUIRE(run.turns.size() == 1);
    const EvalTurn& turn = run.turns.front();
    // A page is timed by the snapshot before the frame that showed it: shown by the kill if timed a frame before it,
    // and never if timed after it.
    const auto timed = [&](std::int64_t by) {
      return static_cast<std::size_t>(
          std::count_if(pages.begin(), pages.end(), [&](std::int64_t p) { return p <= by; }));
    };
    CHECK(turn.pages >= timed(kill - 250));
    CHECK(turn.pages <= timed(kill));
    CHECK(number(file, "SELECT count(*) FROM events WHERE kind = 'PlayerSaid'") == 1);
    if (turn.pages == 0) {
      CHECK(number(file, "SELECT count(*) FROM events WHERE kind = 'NpcSaid'") == 0);
    } else {
      CHECK(one(file, "SELECT text FROM events WHERE kind = 'NpcSaid'") == turn.reply);
      CHECK(number(file, "SELECT json_array_length(payload, '$.pages') FROM events WHERE kind = 'NpcSaid'") ==
            static_cast<std::int64_t>(turn.pages));
      CHECK(number(file, "SELECT max(json_extract(p.value, '$.pt')) FROM events e, json_each(e.payload, '$.pages') p "
                         "WHERE e.kind = 'NpcSaid'") <= kill);
    }
    CHECK(one(file, "SELECT end_reason FROM conversations") == "crash");
  }
}

TEST_CASE("oracle (MEM 12.11): star_falls, Kalé's steps, as the file holds them") {
  const EvalScenario sf = scenario("star_falls.toml");
  Folder folder;
  // Up to the older save (F): E's journal and the cue E2 showed.
  const std::size_t load = index_of(sf, Kind::Load);
  (void)run_kept(truncated(sf, load), folder.at("before-f"));
  const fs::path before = live_file(folder.at("before-f"));
  const std::string kale = "SELECT value FROM npc_facts WHERE journey = 1 AND persona = 'kale' AND key = ";
  // E: one keepsake, the claim, checked as slew:radahn and proved at the commit; the star's keepsake, which cites only
  // the world's line, was the world model's and is not in the journal.
  CHECK(one(before, "SELECT json_array_length(keepsakes) FROM npc_journal WHERE persona = 'kale'") == "1");
  CHECK(one(before, "SELECT json_extract(keepsakes, '$[0].check_key') FROM npc_journal WHERE persona = 'kale'") ==
        "slew:radahn");
  CHECK(one(before, kale + "'verified:slew:radahn'") == "1");
  CHECK(number(before, "SELECT count(*) FROM npc_journal, json_each(keepsakes) k WHERE json_extract(k.value, "
                       "'$.text') LIKE '%star%'") == 0);
  // E2: the reply that showed the cue carries it; D's carried none.
  CHECK(rows(before, "SELECT json_extract(payload, '$.cue') FROM events WHERE kind = 'NpcSaid' AND json_extract("
                     "payload, '$.cue') IS NOT NULL") == std::vector<std::string>{"w:general_fallen|"});
  CHECK(one(before, kale + "'world_cued'").find("w:general_fallen") != std::string::npos);
  // F and G, the whole scenario: the older save takes back D, E and E2, and NG+ starts journey 2.
  (void)run_kept(sf, folder.at("all"));
  const fs::path all = live_file(folder.at("all"));
  const std::string f = std::to_string(sf.steps[load].pt);
  CHECK(number(all, "SELECT count(*) FROM events WHERE journey = 1 AND kind IN ('PlayerSaid', 'NpcSaid') AND "
                    "t_play_ms > " + f) == 0);
  CHECK(number(all, "SELECT count(*) FROM npc_journal") == 0);
  CHECK(rows(all, kale + "'verified:slew:radahn'").empty());
  CHECK(rows(all, kale + "'world_cued'").empty());
  CHECK(one(all, kale + "'world_seen'") == "[[\"w:castle_fallen\",\"rumour\"]]");
  CHECK(one(all, "SELECT v FROM meta WHERE k = 'current_journey'") == "2");
}

TEST_CASE("oracle (MEM 12.11's T7, T3): world trims around the general and the star, and seeded cuts' folds") {
  // star_falls up to its older save, then NG+: the oracle's scenario. T7's cuts: before the general's fall, between it
  // and the star, after both (before D); and one before E2, whose cue's item a trim makes cue-able again. Then T3's
  // property: at seeded cuts, world_seen, world_seen_pt, world_cued and spoke_pt as the core folds them live (the
  // replay's) equal their refold after the trim, as every other table does.
  const EvalScenario sf = scenario("star_falls.toml");
  EvalScenario s = truncated(sf, index_of(sf, Kind::Load));
  s.steps.push_back(step(Kind::NgPlus));
  Folder folder;
  const EvalRun full = run_kept(s, folder.at("full"));
  const fs::path full_file = live_file(folder.at("full"));
  const std::size_t b = index_of(s, Kind::Flags);
  const std::size_t c = index_of(s, Kind::Flags, b + 1);
  const std::size_t d = index_of(s, Kind::Open, c);
  const std::size_t jobs = index_of(s, Kind::WaitJobs, d);
  const std::size_t e2 = index_of(s, Kind::Open, jobs);
  std::vector<Cut> cuts = {
      cut_at("before-general", full, full.step_pt[b] - 5000, true),
      cut_at("general-not-star", full, full.step_pt[b] + 1000, true),
      cut_at("after-both", full, full.step_pt[d] - 1000, true),
      cut_at("before-e2", full, full.step_pt[e2] - 1000, true),
  };
  // Six seeded cuts, a third of the way into a wait, outside the memory job's run (a replay to a cut there drains the
  // job at a time of its own, which the full run's proof does not share); one in a conversation is not drained.
  std::mt19937 rng(23);
  std::uniform_int_distribution<std::size_t> pick(1, s.steps.size() - 2);
  for (int i = 0; cuts.size() < 10 && i < 1000; ++i) {
    const std::size_t k = pick(rng);
    if (s.steps[k].kind != Kind::Wait || (k > d && k <= jobs + 1)) continue;
    bool talking = false;
    for (std::size_t j = 0; j < k; ++j) {
      if (s.steps[j].kind == Kind::Open) talking = true;
      if (s.steps[j].kind == Kind::Esc) talking = false;
    }
    const std::int64_t at = full.step_pt[k] + (full.step_pt[k + 1] - full.step_pt[k]) / 3;
    if (std::any_of(cuts.begin(), cuts.end(), [&](const Cut& x) { return x.pt == at; })) continue;
    cuts.push_back(cut_at("seeded-" + std::to_string(cuts.size()), full, at, !talking));
  }
  REQUIRE(cuts.size() == 10);
  const std::string kale = "SELECT value FROM npc_facts WHERE journey = 1 AND persona = 'kale' AND key = ";
  oracle(s, folder, full, cuts, [&](const Cut& cut, const fs::path& file) {
    if (cut.name == "before-e2") {
      CHECK(rows(file, kale + "'world_cued'").empty());  // cue-able again
      CHECK(one(full_file, kale + "'world_cued'").find("w:general_fallen") != std::string::npos);
    } else if (cut.name.rfind("seeded-", 0) != 0) {
      CHECK(one(file, kale + "'world_seen'") == "[[\"w:castle_fallen\",\"rumour\"]]");  // A's, before D
    }
  });
}

namespace {

// Every byte of every file under a store folder: the live files, their WAL and shared memory, backups, archives.
std::string all_bytes(const fs::path& dir) {
  std::string out;
  for (const auto& e : fs::recursive_directory_iterator(dir)) {
    if (!e.is_regular_file()) continue;
    out += read_file(e.path().string(), 64u << 20).value_or(std::string());
  }
  return out;
}

std::string utf16le(const std::string& ascii) {
  std::string out;
  for (const char c : ascii) {
    out.push_back(c);
    out.push_back('\0');
  }
  return out;
}

}  // namespace

TEST_CASE("oracle (MEM 8.3, 8.6): the eval's database files hold no text that must not be there") {
  // The plan's privacy rules for the playthrough file (v0.3 plan §4): the Tarnished's own name only as its hash (MEM
  // §8.3), never a path, never the fixed text of a prompt (only the lines shown_context keeps), never the model's raw
  // answer, and never what the model wrote that was not shown. Each scenario runs with a canary for the character's
  // name, one no line types, and its files are read byte by byte, the WAL, backups and archives included.
  const std::string canary = "Quillfeather";
  for (const char* name : {"wren_and_kale.toml", "star_falls.toml", "older_save.toml", "kale_news.toml",
                           "lantern-long-reply.toml"}) {
    CAPTURE(name);
    EvalScenario s = scenario(name);
    s.store = true;
    s.character = canary;
    Folder folder;
    EvalOptions o;
    o.store_dir = folder.at("store");
    auto run = run_eval(s, EvalLlm::Mock, o);  // the told name is not the save's now: some expectations fail
    REQUIRE(run.has_value());
    const std::string bytes = all_bytes(folder.at("store"));
    REQUIRE(bytes.size() > 4096);
    CHECK(bytes.find(run->turns.front().player) != std::string::npos);  // the Tarnished's line, kept as typed
    CHECK(bytes.find(canary) == std::string::npos);
    CHECK(bytes.find(utf16le(canary)) == std::string::npos);
    for (const fs::path& p : {folder.at("store"), s.folder, fs::temp_directory_path()}) {
      CHECK(bytes.find(p.string()) == std::string::npos);
    }
    // The prompt's fixed parts: no line of a system message of 40 characters or more.
    std::set<std::string> lines;
    for (const EvalTurn& t : run->turns) {
      for (const proto::Message& m : t.prompt) {
        if (m.role != proto::Role::System) continue;
        std::string text = m.joined();
        for (std::size_t at = 0; at < text.size();) {
          const std::size_t nl = std::min(text.find('\n', at), text.size());
          if (nl - at >= 40) lines.insert(text.substr(at, nl - at));
          at = nl + 1;
        }
      }
    }
    REQUIRE_FALSE(lines.empty());
    for (const std::string& l : lines) {
      CAPTURE(l.substr(0, 40));
      CHECK(bytes.find(l) == std::string::npos);
    }
    // The model's raw answer: the memory job's JSON keys of its own.
    for (const char* key : {"disposition_delta", "\"keep\":", "told_name\":"}) {
      CHECK(bytes.find(key) == std::string::npos);
    }
    // What the model wrote past the pager's cap, never shown.
    if (std::string(name) == "lantern-long-reply.toml") {
      CHECK(bytes.find("The lamp burns all night") != std::string::npos);  // what showed is there, read as written
      CHECK(bytes.find("count the travellers who pass") == std::string::npos);
      CHECK(bytes.find("Then I wait for the light") == std::string::npos);
    }
  }
}
