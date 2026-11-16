// settings.toml for the first in-game build (settings.hpp): what v0.1 reads, what it accepts for later, what refuses
// the file (the defaults then), and the probe table only in a probe build. The files are invented.
#include <doctest.h>

#include <string>

#include "en/shell/settings.hpp"

using namespace en::shell;

TEST_CASE("settings: an empty file and a missing one give the defaults") {
  const LoadedSettings s = parse_settings("", false);
  CHECK_FALSE(s.refused);
  CHECK(s.settings.enabled);
  CHECK_FALSE(s.settings.record_sessions);
  CHECK(s.settings.log_level == LogLevel::Info);
  CHECK(s.settings.copy == CopyMode::Seh);
  CHECK_FALSE(s.settings.overlay_connect);
  CHECK(s.settings.probe.round == ProbeRound::None);
}

TEST_CASE("settings: what v0.1 reads") {
  const LoadedSettings s = parse_settings(R"(schema = 2
enabled = false
debug = { panel = false, record_sessions = true, record_redact_text = false, log_level = "debug" }
gal = { copy = "rpm" }
)", false);
  REQUIRE_FALSE(s.refused);
  CHECK_FALSE(s.settings.enabled);
  CHECK(s.settings.record_sessions);
  CHECK(s.settings.log_level == LogLevel::Debug);
  CHECK(s.settings.copy == CopyMode::Rpm);
}

TEST_CASE("settings: ARCH §11.5's tables for later increments are accepted and not read") {
  const LoadedSettings s = parse_settings(R"(schema = 2
features = { chat = true, barks = true }
autonomy = { enabled = true, period_s = 120 }
hotkeys = { talk = "T" }
[reactions]
engaged = { on = true, cooldown_s = 20 }
[allow."kind:creature"]
dialogue = false
)", false);
  CHECK_FALSE(s.refused);
  CHECK(s.problems.empty());
}

TEST_CASE("settings: overlay.connect = true is noted and stays off in this build") {
  const LoadedSettings s = parse_settings("overlay = { connect = true }\n", false);
  CHECK_FALSE(s.refused);
  CHECK_FALSE(s.settings.overlay_connect);
  REQUIRE(s.notes.size() == 1);
}

TEST_CASE("settings: a problem refuses the whole file, worded without its values") {
  for (const char* text : {"enabled = \"yes\"\n", "debug = { log_level = \"loud\" }\n", "colour = 3\n",
                           "schema = 3\n", "gal = { copy = \"dma\", speed = 2 }\n", "debug = 1\n",
                           "enabled = [1, }]\n", "this is = not toml = at all\n"}) {
    INFO(text);
    const LoadedSettings s = parse_settings(text, false);
    CHECK(s.refused);
    REQUIRE_FALSE(s.problems.empty());
    CHECK(s.settings.enabled);  // the defaults
    for (const SettingsProblem& p : s.problems) {
      CHECK(p.problem.find("loud") == std::string::npos);
      CHECK(p.problem.find("dma") == std::string::npos);
    }
  }
  const LoadedSettings big = parse_settings(std::string(kMaxSettingsBytes + 1, '#'), false);
  CHECK(big.refused);
}

TEST_CASE("settings: the probe table only in a probe build") {
  const char* text = R"(probe = { round = "r1", register = "auto", object_size = 0x400, settle_s = 3, fault_group = "world", fault_every = 5 }
)";
  const LoadedSettings release = parse_settings(text, false);
  CHECK(release.refused);
  REQUIRE(release.problems.size() == 1);
  CHECK(release.problems[0].where == "probe");
  const LoadedSettings probe = parse_settings(text, true);
  REQUIRE_FALSE(probe.refused);
  CHECK(probe.settings.probe.round == ProbeRound::R1);
  CHECK(probe.settings.probe.register_when == ProbeRegister::Auto);
  CHECK(probe.settings.probe.object_size == 0x400);
  CHECK(probe.settings.probe.settle_s == 3);
  CHECK(probe.settings.probe.fault_group == en::proto::Group::World);
  CHECK(probe.settings.probe.fault_every == 5);
  CHECK(parse_settings("probe = { object_size = 0x8000 }\n", true).refused);
}
