// The scenario DSL and the golden runner (ARCH §8.12): the parser's refusals, the three routes' expectations and
// goldens, determinism over 10 runs (C12), the backpressure test of 50% snapshot loss against the dense run, a
// recording's inputs, and our own diff.
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "en/augur/golden.hpp"
#include "en/shell/record.hpp"
#include "golden_run.hpp"
#include "scn.hpp"

namespace en::tools {
namespace {

namespace fs = std::filesystem;

const char* const kRoutes[] = {"camp-skirmish", "boss-attempts", "road-and-gate"};

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

Scenario scenario(const char* route) {
  auto s = parse_scn(read(fs::path(EN_TOOLS_SCENARIOS) / (std::string(route) + ".scn")));
  REQUIRE(s);
  return *s;
}

Run run_of(const Scenario& sc, unsigned drop = 0, std::uint64_t seed = 0, Simulated* keep = nullptr) {
  auto sim = simulate(sc, drop, seed);
  REQUIRE(sim);
  auto run = run_core(sim->steps);
  REQUIRE(run);
  if (keep != nullptr) keep->names = sim->names;
  return *run;
}

TEST_CASE("the parser: what v0.1 can say, and each refusal with its line") {
  const auto bad = [](const std::string& text, const std::string& words) {
    const auto r = parse_scn(text);
    if (r) {
      MESSAGE("accepted: " << text);
      return false;
    }
    const bool ok = r.error().problem.find(words) != std::string::npos;
    if (!ok) MESSAGE(r.error().problem);
    return ok;
  };
  CHECK(bad("at 0 spawn a team=6 pos=1,2,3\n", "has no route line"));
  CHECK(bad("route r\nat 100 set_flag rest=false\n", "waits for the flag catalogue"));
  CHECK(bad("route r\nat 100 say kale hello\n", "waits for the Pipeline source"));
  CHECK(bad("route r\nexpect bark engaged speaker=s1\n", "bark rules are v0.4's"));
  CHECK(bad("route r\nat 100 dance\n", "an unknown verb"));
  CHECK(bad("route r\nexpect event Frobbed\n", "an event kind of the catalogue is missing"));
  CHECK(bad("route r\nexpect event Struck within 9..3\n", "within takes <from>..<to>"));
  CHECK(bad("route r\nstep 10\n", "step is 50 to 5000 ms"));
  CHECK(bad("route Bad_Name\n", "route takes one name"));
  CHECK(bad("route r\nwobble\n", "a line starts with"));
  const auto line = parse_scn("route r\n\n# a comment\nat x spawn\n");
  REQUIRE_FALSE(line);
  CHECK(line.error().line == 4);
  // Errors the player finds when it applies an action, with its line.
  auto sc = parse_scn("route r\nat 0 spawn a team=6 pos=1,2\n");
  REQUIRE(sc);
  const auto sim = simulate(*sc);
  REQUIRE_FALSE(sim);
  CHECK(sim.error().line == 2);
  CHECK(sim.error().problem == "pos is x,y,z in metres");
  auto who = parse_scn("route r\nat 0 hit ghost -> player dmg=5\n");
  REQUIRE(who);
  CHECK_FALSE(simulate(*who));
}

TEST_CASE("the three routes: their expectations hold, and each golden file agrees exactly, times included") {
  for (const char* route : kRoutes) {
    CAPTURE(route);
    const Scenario sc = scenario(route);
    CHECK(sc.route == route);
    Simulated sim;
    const Run run = run_of(sc, 0, 0, &sim);
    for (const std::string& f : check_expects(sc, run.events, sim.names)) MESSAGE(f);
    CHECK(check_expects(sc, run.events, sim.names).empty());
    const std::string golden = read(fs::path(EN_TOOLS_GOLDEN) / (std::string(route) + ".events.jsonl"));
    const std::string text = golden_text(route, std::string(route) + ".scn", run);
    CHECK(line_diff(golden, text).empty());
    // CHANGES.md names the route and its hash (golden check's rule).
    const std::string changes = read(fs::path(EN_TOOLS_GOLDEN) / "CHANGES.md");
    CHECK(changes.find(augur::hash_text(augur::fnv1a64(golden))) != std::string::npos);
    // No handle of a session and no name in a golden file: handles are h1, h2, ...; names are the scenario's only in
    // the expectations.
    for (const char* name : {"#ash", "briar", "cinder", "bulwark", "ravener", "howl", "yelp", "#4096", "#1000"}) {
      CHECK(golden.find(name) == std::string::npos);
    }
  }
}

TEST_CASE("property: determinism: the same inputs give byte-identical events over 10 runs (C12)") {
  for (const char* route : kRoutes) {
    CAPTURE(route);
    const Scenario sc = scenario(route);
    const std::string first = golden_text(route, "x", run_of(sc));
    for (int i = 1; i < 10; ++i) CHECK(golden_text(route, "x", run_of(sc)) == first);
    // The same random loss, the same events too.
    const std::string lossy = golden_text(route, "x", run_of(sc, 50, 7));
    CHECK(golden_text(route, "x", run_of(sc, 50, 7)) == lossy);
  }
}

// What a 50% loss may change, by the design's own rules (README, "Snapshot loss"): a per-hit event (Struck: the hits
// of a dropped snapshot reach the next kept one as one drop, so merges, counts and `hard` differ), and a state briefer
// than the gap it falls in, which the kept snapshots never show (a near pass, a lock-on before the closing, a heal
// within 1 s, a critical hit's two snapshots, a low HP the next hit kills): those may be missing, never invented. The
// kinds below are of states that last, so they must all be there. GateChanged is not among them: the gate's 5 s hold
// starts again after any gap over 1 s (ARCH §5.1), so whether and when it opens is the gate's own, by design.
bool lasting(const augur::Event& e) {
  static const std::set<std::string_view> kinds = {
      "DataReady",      "PlaythroughReady", "LoadingStarted", "LoadingEnded",   "TitleReached",
      "LadderChanged",  "PlaceEntered",     "BossFightStarted", "BossFightEnded", "PlayerDied",     "MountChanged",
      "JourneyChanged", "NativeTalkStarted", "NativeTalkEnded", "AllyDied"};
  return kinds.count(augur::kind_name(e.kind)) != 0;
}

TEST_CASE("backpressure: 50% snapshot loss at random gives the dense run's events, at most a gap later") {
  for (const char* route : kRoutes) {
    const Scenario sc = scenario(route);
    Simulated names;
    const Run dense = run_of(sc, 0, 0, &names);
    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
      CAPTURE(route);
      CAPTURE(seed);
      auto sim = simulate(sc, 50, seed);
      REQUIRE(sim);
      // The longest run of missing snapshots, as time.
      std::uint64_t prev = 0, longest = 0;
      for (const Step& s : sim->steps) {
        if (const auto* p = std::get_if<std::unique_ptr<proto::WorldSnapshot>>(&s.input.payload); p != nullptr && *p) {
          longest = std::max(longest, (*p)->mono_ms - prev);
          prev = (*p)->mono_ms;
        }
      }
      auto sparse = run_core(sim->steps);
      REQUIRE(sparse);
      const augur::HandleNamer namer = [&](augur::ChrHandle h) { return names.names[h.raw]; };
      const auto key_of = [&](const augur::Event& e) {
        const auto j = augur::event_json(e, namer);
        return j["kind"].dump() + j["subject"].dump() + j["object"].dump() + j["key"].dump();
      };
      // Per event (kind, subject, object, key, and the payload for the lasting kinds), in order: every event of the
      // lossy run matches one of the dense run's, never earlier (gaps never invent an edge, §8.12); every lasting one
      // is there, at most two gaps and a second later (an edge moves to the next kept snapshot, and a window that
      // starts there ends at the first kept one past it, a timer at the whole second after). Struck is per hit: its
      // entries start where the kept snapshots show the drops.
      std::map<std::string, std::vector<std::uint64_t>> want, got;
      std::set<std::string> must;
      // The payload too, but for NativeTalkEnded's held_ms, which is a time (its start is seen up to a gap late).
      const auto with_payload = [&](const augur::Event& e) {
        return lasting(e) && !std::holds_alternative<augur::NativeTalkEnded>(e.kind);
      };
      for (const auto& e : dense.events) {
        const std::string k = key_of(e) + (with_payload(e) ? augur::event_json(e, namer)["payload"].dump() : "");
        want[k].push_back(e.t_mono_ms);
        if (lasting(e)) must.insert(k);
      }
      for (const auto& e : sparse->events) {
        got[key_of(e) + (with_payload(e) ? augur::event_json(e, namer)["payload"].dump() : "")].push_back(e.t_mono_ms);
      }
      for (const auto& [k, times] : got) {
        CAPTURE(k);
        if (k.rfind("\"Struck\"", 0) == 0 || k.rfind("\"GateChanged\"", 0) == 0) continue;
        const auto& d = want[k];
        std::size_t j = 0;
        for (const std::uint64_t t : times) {
          CHECK(j < d.size());  // never more than the dense run's
          if (j >= d.size()) break;
          CHECK(t >= d[j]);  // never earlier
          ++j;
        }
      }
      for (const std::string& k : must) {
        CAPTURE(k);
        REQUIRE(got[k].size() == want[k].size());
        for (std::size_t i = 0; i < want[k].size(); ++i) {
          CHECK(got[k][i] - want[k][i] <= 2 * longest + 1000);
        }
      }
    }
  }
}

TEST_CASE("a recording's inputs replay through Augur as a scenario's do, and our own diff shows a change") {
  // A recording written as eldennet.dll's core thread writes it, of a scenario's inputs.
  const Scenario sc = scenario("camp-skirmish");
  auto sim = simulate(sc);
  REQUIRE(sim);
  std::string file = shell::header_line(shell::RecordingHeader{}, 0);
  for (const Step& s : sim->steps) {
    if (const auto* p = std::get_if<std::unique_ptr<proto::WorldSnapshot>>(&s.input.payload)) {
      file += shell::snapshot_line(**p, s.now);
    } else if (const auto* l = std::get_if<proto::Lifecycle>(&s.input.payload)) {
      file += shell::lifecycle_line(*l, s.now);
    }
  }
  const fs::path path = fs::path(EN_TOOLS_WORK) / "camp.ensnap.jsonl";
  fs::create_directories(path.parent_path());
  {
    std::ofstream out(path, std::ios::binary);
    out << file;
  }
  const auto lines = load_recording(path.string(), "test");
  REQUIRE(lines);
  auto steps = steps_of(*lines);
  auto replayed = run_core(steps);
  REQUIRE(replayed);
  const Run direct = run_of(sc);
  // The recording keeps 7 significant digits of each float: the events are the same.
  CHECK(golden_text("r", "r", *replayed) == golden_text("r", "r", direct));
  // Our own diff names the first line that differs.
  const std::string diff = line_diff("a\nb\nc\n", "a\nB\nc\n");
  CHECK(diff.find("@@ golden line 2") != std::string::npos);
  CHECK(diff.find("-b") != std::string::npos);
  CHECK(diff.find("+B") != std::string::npos);
  CHECK(line_diff("x\n", "x\n").empty());
}

}  // namespace
}  // namespace en::tools
