// The command line, the texts, the watcher's records, the spikes' tally and the pre-flight's order.
#include <doctest.h>

#include <memory>
#include <new>
#include <string>
#include <vector>

#include "en/launcher/options.hpp"
#include "en/launcher/record.hpp"
#include "en/launcher/texts.hpp"

using namespace en::launcher;
namespace w = en::proto::watch;

namespace {
Options parse_ok(const std::vector<std::string>& args) {
  auto o = parse_options(args);
  REQUIRE_MESSAGE(o.has_value(), (o.has_value() ? std::string{} : o.error()));
  return *o;
}
std::string parse_error(const std::vector<std::string>& args) {
  auto o = parse_options(args);
  REQUIRE_FALSE(o.has_value());
  return o.error();
}
}  // namespace

TEST_CASE("options: the defaults start the game, inject when suspended, back up") {
  const Options o = parse_ok({});
  CHECK(o.action == Action::Start);
  CHECK(o.inject_at == InjectAt::Suspended);
  CHECK_FALSE(o.no_backup);
  CHECK(o.spike == w::Spike::None);
  CHECK(o.runs == 1);
}

TEST_CASE("options: every option, in both forms, and what follows --") {
  const Options o = parse_ok({"--game",
                              "C:\\G\\fake_game.exe",
                              "--dll=x.dll",
                              "--no-backup",
                              "--also",
                              "a.dll",
                              "--also=b.dll",
                              "--inject-at",
                              "title",
                              "--title-wait",
                              "30",
                              "--appid-method",
                              "env",
                              "--remove-after=exit",
                              "--console",
                              "--no-dialogs",
                              "--yes",
                              "--no-steam-check",
                              "--gal",
                              "g",
                              "--steam",
                              "s",
                              "--appdata",
                              "ad",
                              "--spike",
                              "s0a",
                              "--runs",
                              "20",
                              "--run-timeout",
                              "90",
                              "--",
                              "--frames",
                              "60",
                              "--no-window"});
  CHECK(*o.game == "C:\\G\\fake_game.exe");
  CHECK(*o.dll == "x.dll");
  CHECK(o.no_backup);
  CHECK(o.also == std::vector<std::string>{"a.dll", "b.dll"});
  CHECK(o.inject_at == InjectAt::Title);
  CHECK(o.title_wait_s == 30);
  CHECK(*o.appid_method == AppidMethod::Env);
  CHECK(*o.remove_after == RemoveAfter::Exit);
  CHECK(o.console);
  CHECK(o.no_dialogs);
  CHECK(o.yes);
  CHECK(o.no_steam_check);
  CHECK(*o.gal == "g");
  CHECK(*o.steam == "s");
  CHECK(*o.appdata == "ad");
  CHECK(o.spike == w::Spike::S0a);
  CHECK(o.runs == 20);
  CHECK(o.run_timeout_s == 90);
  CHECK(o.game_args == std::vector<std::string>{"--frames", "60", "--no-window"});
}

TEST_CASE("options: Proton's forwarded command is set aside; the actions go alone") {
  const Options o = parse_ok({"C:\\games\\fake_game.exe", "-some-flag", "--no-backup"});
  CHECK(o.ignored == 2);
  CHECK(o.action == Action::Start);
  CHECK(parse_ok({"--backup-only"}).action == Action::BackupOnly);
  CHECK(parse_ok({"--cleanup"}).action == Action::Cleanup);
  CHECK(parse_ok({"--print-diagnostics"}).action == Action::Diagnostics);
  CHECK(parse_ok({"--help"}).action == Action::Help);
  const Options watch = parse_ok({"--watch", "4321", "--console"});
  CHECK(watch.action == Action::Watch);
  CHECK(watch.watch_pid == 4321);
  CHECK(parse_ok({"--spike", "s0d-crash", "--runs", "5"}).spike == w::Spike::S0dCrash);
}

TEST_CASE("options: refusals") {
  CHECK(parse_error({"--bogus"}) == "unknown option: --bogus");
  CHECK(parse_error({"--game"}) == "missing value for --game");
  CHECK(parse_error({"--game="}) == "missing value for --game");
  CHECK(parse_error({"--no-backup=1"}) == "--no-backup takes no value");
  CHECK(parse_error({"--inject-at", "later"}) == "--inject-at is suspended, window or title");
  CHECK(parse_error({"--runs", "0", "--spike", "s0a"}) == "--runs is 1 to 1000");
  CHECK(parse_error({"--runs", "3"}) == "--runs goes with --spike s0a or s0d-crash");
  CHECK(parse_error({"--runs", "3", "--spike", "s0b"}) == "--runs goes with --spike s0a or s0d-crash");
  CHECK(parse_error({"--spike", "s9"}) == "--spike is s0a, s0b, s0c, s0d or s0d-crash");
  CHECK(parse_error({"--watch", "0"}) == "--watch takes the game's process id");
  CHECK(parse_error({"--watch", "12", "--cleanup"}).find("go alone") != std::string::npos);
  CHECK(parse_error({"--vanilla-offline", "--also", "a.dll"}).find("injects nothing") != std::string::npos);
  CHECK(parse_error({"--vanilla-offline", "--inject-at", "title"}).find("injects nothing") != std::string::npos);
  CHECK(parse_error({"--backup-only", "--spike", "s0a"}) == "--spike and game arguments go with a start");
  CHECK(parse_error({"--yes"}) == "--yes goes with --no-dialogs");
  CHECK(parse_error({"--title-wait", "601"}) == "--title-wait is 0 to 600 seconds");
  CHECK(usage_text().find("--spike") != std::string::npos);
}

TEST_CASE("options: --probe (R1) brings the console, and keeps the backup, EldenNet's DLL and one plain start") {
  const Options o = parse_ok({"--probe"});
  CHECK(o.probe);
  CHECK(o.console);
  CHECK(o.action == Action::Start);
  CHECK(parse_ok({"--probe", "--inject-at", "title", "--game", "C:\\G\\fake_game.exe"}).inject_at == InjectAt::Title);
  const std::string refused = "--probe goes with a start, and not with --no-backup, --vanilla-offline or --spike";
  CHECK(parse_error({"--probe", "--no-backup"}) == refused);
  CHECK(parse_error({"--probe", "--vanilla-offline"}) == refused);
  CHECK(parse_error({"--probe", "--spike", "s0b"}) == refused);
  CHECK(parse_error({"--probe=1"}) == "--probe takes no value");
  CHECK(parse_error({"--probe", "--backup-only"}) == refused);
  CHECK_FALSE(parse_ok({}).probe);
  CHECK(usage_text().find("--probe") != std::string::npos);
}

TEST_CASE("texts: R1's steps on the console follow EldenNet's answer and T1's state") {
  using w::InitState;
  using w::ProbeRequest;
  CHECK_FALSE(r1_step(ProbeRequest::R1, InitState::None, false, 0).has_value());
  CHECK_FALSE(r1_step(ProbeRequest::R1, InitState::Settings, false, 0).has_value());
  CHECK(r1_step(ProbeRequest::R1, InitState::WaitWindow, false, 0) == R1Step::Refused);  // a DLL without --probe
  CHECK(r1_step(ProbeRequest::Refused, InitState::Settings, false, 0) == R1Step::Refused);
  CHECK_FALSE(r1_step(ProbeRequest::Accepted, InitState::WaitWindow, false, 0).has_value());
  CHECK(r1_step(ProbeRequest::Accepted, InitState::Discover, false, 0) == R1Step::Reading);
  CHECK(r1_step(ProbeRequest::Accepted, InitState::Register, false, 0) == R1Step::PressMark);
  CHECK(r1_step(ProbeRequest::Accepted, InitState::FirstTick, true, 29999) == R1Step::Called);
  CHECK(r1_step(ProbeRequest::Accepted, InitState::FirstTick, true, 30000) == R1Step::NoConfirmation);
  CHECK(r1_step(ProbeRequest::Accepted, InitState::Ready, true, 0) == R1Step::Confirmed);
  CHECK(r1_step(ProbeRequest::Accepted, InitState::Banner, false, 0) == R1Step::NotCalled);
  CHECK(r1_step(ProbeRequest::Accepted, InitState::Banner, true, 0) == R1Step::CallFailed);
  for (auto st = static_cast<int>(R1Step::Asked); st <= static_cast<int>(R1Step::ReportsMissing); ++st) {
    const std::string t = r1_text(static_cast<R1Step>(st));
    CHECK_FALSE(t.empty());
    CHECK(std::string(r1_word(static_cast<R1Step>(st))).find(' ') == std::string::npos);
    CHECK(t.find(":\\") == std::string::npos);  // no drive's path; %APPDATA% names the folder
  }
  CHECK(r1_text(R1Step::PressMark).find("press Pause") != std::string::npos);
  CHECK(std::string(r1_word(R1Step::Confirmed)) == "task_confirmed");
}

TEST_CASE("texts: ARCH §4.7's wording, the ids and the exit codes") {
  CHECK(error_id(ErrorCode::E01) == "E01");
  CHECK(error_id(ErrorCode::E11) == "E11");
  CHECK(exit_code(ErrorCode::E01) == 11);
  CHECK(exit_code(ErrorCode::E08) == 18);
  CHECK(error_text(ErrorCode::E02) == "Steam is not running. Start Steam, then run the EldenNet launcher again.");
  CHECK(error_text(ErrorCode::E05, "3.14.15.0") ==
        "This game build (3.14.15.0) is not supported by this EldenNet version yet. Launch anyway? EldenNet will stay "
        "inactive until an update arrives.");
  CHECK(error_text(ErrorCode::E09, "the file is missing", 2) ==
        "The game could not be started: the file is missing (code 2).");
  CHECK(error_text(ErrorCode::E10, "CreateRemoteThread", 5) ==
        "EldenNet could not be loaded into the game: the CreateRemoteThread step failed (code 5). The game was closed "
        "again. Antivirus software sometimes blocks this step; see the install guide. Start the game without EldenNet "
        "instead?");
  CHECK(error_action(ErrorCode::E10) == "Start without EldenNet");
  CHECK(error_action(ErrorCode::E02).empty());
  CHECK(notices().size() == 4);
}

TEST_CASE("records: how a session ended") {
  CHECK(classify_end(0, false) == EndKind::Clean);
  CHECK(classify_end(42, false) == EndKind::Exit);
  CHECK(classify_end(0xC0000005u, false) == EndKind::Crash);
  CHECK(classify_end(0xC0000409u, false) == EndKind::Crash);
  CHECK(classify_end(0x80000003u, false) == EndKind::Crash);
  CHECK(classify_end(0x46470099u, false) == EndKind::Exit);
  CHECK(classify_end(0, true) == EndKind::Crash);
  CHECK(classify_end(kClosedByLauncher, true) == EndKind::Closed);
}

TEST_CASE("records: the status and the crash record copied from a watch block; module names made safe") {
  alignas(w::Block) unsigned char page[w::kBlockSize] = {};
  w::Block* b = ::new (page) w::Block();
  w::initialise(*b, 77);
  CHECK_FALSE(copy_crash(*b).has_value());
  b->status.init.store(static_cast<std::uint32_t>(w::InitState::Ready));
  b->status.items_resolved.store(17);
  b->status.items_total.store(17);
  b->status.snapshots.store(123);
  const DllStatus s = copy_status(*b);
  CHECK(s.init == w::InitState::Ready);
  CHECK(s.items_resolved == 17);
  CHECK(s.snapshots == 123);
  b->status.init.store(999);
  CHECK(copy_status(*b).init == w::InitState::None);

  b->crash.code = 0xC0000005u;
  b->crash.thread = static_cast<std::uint32_t>(w::ThreadKind::Game);
  b->crash.offset = 0x1234;
  b->crash.chained = 1;
  const std::wstring name = L"fake_game.exe";
  w::set_module_name(b->crash, name.c_str(), name.size());
  b->crash.state.store(static_cast<std::uint32_t>(w::CrashState::Written));
  const auto c = copy_crash(*b);
  REQUIRE(c.has_value());
  CHECK(c->code == 0xC0000005u);
  CHECK(c->thread == w::ThreadKind::Game);
  CHECK(c->chained);
  CHECK(c->offset == 0x1234);
  CHECK(c->module == "fake_game.exe");

  CHECK(safe_module_name("C:\\Users\\Someone\\mods\\x.dll") == "x.dll");
  CHECK(safe_module_name("a/b/m\xC3\xB6.dll") == "m??.dll");
  CHECK(safe_module_name(std::string(100, 'a')).size() == 64);
}

TEST_CASE("records: the JSON form holds numbers, fixed words and module names; the next start explains a crash") {
  SessionRecord r;
  r.launcher_version = "0.0.1";
  r.build = "3.14.15.0-ww";
  r.started = "20261116-101500";
  r.ended = "20261116-102000";
  r.mode = "suspended";
  r.exit_code = 0xC0000005u;
  r.kind = EndKind::Crash;
  r.crash = CrashInfo{0xC0000005u, w::ThreadKind::Game, false, 0x40, "fake_game.exe"};
  r.modules = {"fake_game.exe", "kernel32.dll"};
  r.marks = {1000, 2000};
  CHECK(worth_keeping(r));
  const auto j = to_json(r);
  CHECK(j["kind"] == "crash");
  CHECK(j["exit_code_hex"] == "0xC0000005");
  CHECK(j["crash"]["thread"] == "game");
  CHECK(j["modules"].size() == 2);
  CHECK(j["explained"] == false);
  const auto text = explanation(j);
  REQUIRE(text.has_value());
  CHECK(text->find("0xC0000005, in fake_game.exe") != std::string::npos);
  auto shown = j;
  shown["explained"] = true;
  CHECK_FALSE(explanation(shown).has_value());

  SessionRecord clean;
  CHECK_FALSE(worth_keeping(clean));
  clean.kind = EndKind::Closed;
  CHECK_FALSE(worth_keeping(clean));
  clean.kind = EndKind::Exit;
  clean.exit_code = 86;
  CHECK(worth_keeping(clean));
  CHECK(explanation(to_json(clean))->find("exit code 86") != std::string::npos);
  CHECK_FALSE(explanation(nlohmann::json::array()).has_value());
}

TEST_CASE("spikes: outcomes, the tally and each spike's rule") {
  CHECK(outcome_of(w::InitState::Ready) == RunOutcome::Ready);
  CHECK(outcome_of(w::InitState::Banner) == RunOutcome::Banner);
  CHECK(outcome_of(w::InitState::Discover) == RunOutcome::Timeout);
  CHECK(outcome_of(w::InitState::None) == RunOutcome::NoDll);

  std::vector<RunResult> runs(3);
  for (std::uint32_t i = 0; i < 3; ++i) {
    runs[i].run = i + 1;
    runs[i].outcome = RunOutcome::Ready;
    runs[i].end = EndKind::Closed;
  }
  Tally t = tally(runs);
  CHECK(t.runs == 3);
  CHECK(t.ready == 3);
  CHECK(passed(w::Spike::S0a, t));
  CHECK(tally_line(t).rfind("runs=3 ready=3 banner=0", 0) == 0);
  runs[1].outcome = RunOutcome::Banner;  // L1 with no crash still passes S0-A
  CHECK(passed(w::Spike::S0a, tally(runs)));
  runs[2].end = EndKind::Crash;
  CHECK_FALSE(passed(w::Spike::S0a, tally(runs)));
  runs[2].end = EndKind::Closed;
  runs[2].outcome = RunOutcome::Timeout;
  CHECK_FALSE(passed(w::Spike::S0a, tally(runs)));

  std::vector<RunResult> crashes(2);
  for (auto& r : crashes) {
    r.outcome = RunOutcome::Ready;
    r.end = EndKind::Crash;
    r.record_kept = true;
    r.crash_filter = true;
  }
  t = tally(crashes);
  CHECK(t.records == 2);
  CHECK(t.filter_records == 2);
  CHECK(passed(w::Spike::S0dCrash, t));
  crashes[1].record_kept = false;
  CHECK_FALSE(passed(w::Spike::S0dCrash, tally(crashes)));
  CHECK_FALSE(passed(w::Spike::S0a, Tally{}));
  const auto j = to_json(tally(runs), w::Spike::S0a);
  CHECK(j["spike"] == "s0a");
  CHECK(j["passed"] == false);
  CHECK(to_json(runs[0], w::Spike::S0a)["outcome"] == "ready");
}

TEST_CASE("pre-flight: ARCH §4.3's order, Steam's check optional, the DLL's only when injecting") {
  PreflightFacts ok{true, false, false, true};
  CHECK_FALSE(preflight_failure(ok, true, true).has_value());
  PreflightFacts f = ok;
  f.steam_running = false;
  f.eac_running = true;
  f.game_running = true;
  f.dll_ours = false;
  CHECK(preflight_failure(f, true, true) == ErrorCode::E02);
  CHECK(preflight_failure(f, false, true) == ErrorCode::E04);
  f.eac_running = false;
  CHECK(preflight_failure(f, false, true) == ErrorCode::E03);
  f.game_running = false;
  CHECK(preflight_failure(f, false, true) == ErrorCode::E08);
  CHECK_FALSE(preflight_failure(f, false, false).has_value());

  const std::vector<ProcessEntry> procs = {{"System", 4, 0}, {"Fake_Game.EXE", 100, 4}};
  CHECK(running(procs, "fake_game.exe"));
  CHECK_FALSE(running(procs, "steam.exe"));
  CHECK(proxy_dlls_beside({"fake_game.exe", "DINPUT8.dll", "dxgi.dll", "other.dll"}) ==
        std::vector<std::string>{"dinput8.dll", "dxgi.dll"});
}
