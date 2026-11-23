// settings.toml for the first in-game build (settings.hpp): what v0.1 reads, what it accepts for later, what refuses
// the file (the defaults then), and the probe table only in a probe build. The files are invented.
#include <doctest.h>

#include <optional>
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
  CHECK(s.settings.overlay_connect);  // GraceUI's connect is on by default (part 12)
  CHECK_FALSE(s.settings.debug_panel);  // the F11 view is a developer view, off by default (PAGES SP8)
  CHECK(s.settings.probe.round == ProbeRound::None);
}

TEST_CASE("settings: what v0.1 reads") {
  const LoadedSettings s = parse_settings(R"(schema = 2
enabled = false
debug = { panel = true, record_sessions = true, record_redact_text = false, log_level = "debug" }
gal = { copy = "rpm" }
overlay = { connect = false }
)", false);
  REQUIRE_FALSE(s.refused);
  CHECK_FALSE(s.settings.enabled);
  CHECK(s.settings.debug_panel);
  CHECK_FALSE(s.settings.overlay_connect);
  CHECK(s.settings.record_sessions);
  CHECK(s.settings.log_level == LogLevel::Debug);
  CHECK(s.settings.copy == CopyMode::Rpm);
}

TEST_CASE("settings: ARCH §11.5's tables for later increments are accepted and not read") {
  const LoadedSettings s = parse_settings(R"(schema = 2
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

TEST_CASE("settings: features and range are read for the core, within its bounds; the defaults otherwise (WHY§476)") {
  using en::core::Behaviour;
  CHECK(parse_settings("", false).settings.behaviour == Behaviour{});
  LoadedSettings s = parse_settings(R"(schema = 2
features = { chat = false, barks = true, speak_up = false, callout = true, memory = true, labels = false, npc_to_npc = true, self_awareness = false, status_awareness = true }
range = { talk_m = 3, hear_m = 30.0, fight_m = 12.5 }
)", false);
  REQUIRE_FALSE(s.refused);
  const Behaviour& b = s.settings.behaviour;
  CHECK_FALSE(b.features.chat);
  CHECK_FALSE(b.features.speak_up);
  CHECK_FALSE(b.features.labels);
  CHECK_FALSE(b.features.self_awareness);
  CHECK(b.features.barks);
  CHECK(b.features.status_awareness);
  CHECK(b.range.talk_m == 3.0f);  // a whole number is a number too; the bounds are included
  CHECK(b.range.hear_m == 30.0f);
  CHECK(b.range.fight_m == 12.5f);
  CHECK_FALSE(en::core::check_behaviour(b).has_value());
  // ARCH §11.5's own file, as its defaults.
  s = parse_settings("features = { chat = true, barks = true, speak_up = true, callout = true, memory = true, labels = "
                     "true, npc_to_npc = true, self_awareness = true, status_awareness = true }\n"
                     "range    = { talk_m = 8.0, hear_m = 20.0, fight_m = 40.0 }\n",
                     false);
  REQUIRE_FALSE(s.refused);
  CHECK(s.settings.behaviour == Behaviour{});
  // Out of bounds, the wrong type, an unknown key or `features.ambient` (gone, ARCH §11.5): the whole file refused, the
  // defaults used, the problem's place named and never its value.
  for (const char* text : {"range = { talk_m = 2.5 }\n", "range = { hear_m = 31 }\n", "range = { fight_m = 9.99 }\n",
                           "range = { talk_m = nan }\n", "range = { talk_m = inf }\n", "range = { talk_m = \"8\" }\n",
                           "features = { chat = 1 }\n", "features = { ambient = \"off\" }\n",
                           "range = { reach_m = 8.0 }\n", "features = true\n",
                           "features = { chat = false }\nrange = { talk_m = 99 }\n"}) {
    INFO(text);
    const LoadedSettings r = parse_settings(text, false);
    CHECK(r.refused);
    REQUIRE(r.problems.size() == 1);
    CHECK(r.settings.behaviour == Behaviour{});
    CHECK(r.problems[0].problem.find("99") == std::string::npos);
    CHECK(r.problems[0].problem.find("2.5") == std::string::npos);
  }
  CHECK(parse_settings("range = { talk_m = 2.5 }\n", false).problems[0].where == "range.talk_m");
  CHECK(parse_settings("range = { talk_m = 2.5 }\n", false).problems[0].problem == "must be a number from 3 to 16");
}

TEST_CASE("settings: a pages' change of a behaviour key, on the behaviour held; bounds are the core's") {
  using en::core::Behaviour;
  CHECK(is_behaviour_key("features.chat"));
  CHECK(is_behaviour_key("features.status_awareness"));
  CHECK(is_behaviour_key("range.fight_m"));
  for (const char* k : {"features", "features.", "features.ambient", "range.reach_m", "debug.panel", "chat", ""}) {
    CHECK_FALSE(is_behaviour_key(k));
  }
  Behaviour b;
  SettingsEdit chat{"features.chat", false, false, {}};
  CHECK(edit_behaviour(b, chat));
  CHECK_FALSE(b.features.chat);
  SettingsEdit talk{"range.talk_m", false, false, {}, true, 12.0};
  CHECK(edit_behaviour(b, talk));
  CHECK(b.range.talk_m == 12.0f);
  CHECK_FALSE(b.features.chat);  // the earlier change held
  // The wrong type, or no behaviour key: false, and nothing changed.
  const Behaviour before = b;
  CHECK_FALSE(edit_behaviour(b, SettingsEdit{"features.chat", true, false, "off"}));
  CHECK_FALSE(edit_behaviour(b, SettingsEdit{"features.chat", false, true, {}, true, 1.0}));
  CHECK_FALSE(edit_behaviour(b, SettingsEdit{"range.talk_m", false, true, {}}));
  CHECK_FALSE(edit_behaviour(b, SettingsEdit{"debug.panel", false, true, {}}));
  CHECK(b == before);
  // Out of bounds is taken here and refused by the core's check, by its key.
  CHECK(edit_behaviour(b, SettingsEdit{"range.hear_m", false, false, {}, true, 50.0}));
  CHECK(en::core::check_behaviour(b) == std::optional<std::string>("range.hear_m"));
  // A number written to settings.toml reads back.
  const auto written = edit_settings("schema = 2\n", {talk}, false);
  REQUIRE(written.ok);
  CHECK(parse_settings(written.text, false).settings.behaviour.range.talk_m == 12.0f);
  CHECK(edit_settings("schema = 2\n", {SettingsEdit{"range.talk_m", false, false, {}, true, 99.0}}, false).why ==
        "result_refused");
}

TEST_CASE("settings: overlay.connect is GraceUI's connect, a boolean; overlay holds no other key") {
  LoadedSettings s = parse_settings("overlay = { connect = true }\n", false);
  CHECK_FALSE(s.refused);
  CHECK(s.settings.overlay_connect);
  CHECK(s.notes.empty());
  s = parse_settings("overlay = { connect = \"yes\" }\n", false);
  CHECK(s.refused);
  s = parse_settings("overlay = { connect = true, hooks = true }\n", false);
  CHECK(s.refused);
  s = parse_settings("debug = { panel = 1 }\n", false);
  CHECK(s.refused);
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
  const char* text = R"(probe = { round = "read", settle_s = 3, fault_group = "world", fault_every = 5 }
)";
  const LoadedSettings release = parse_settings(text, false);
  CHECK(release.refused);
  REQUIRE(release.problems.size() == 1);
  CHECK(release.problems[0].where == "probe");
  const LoadedSettings probe = parse_settings(text, true);
  REQUIRE_FALSE(probe.refused);
  CHECK(probe.settings.probe.round == ProbeRound::Read);
  CHECK(probe.settings.probe.settle_s == 3);
  CHECK(probe.settings.probe.fault_group == en::proto::Group::World);
  CHECK(probe.settings.probe.fault_every == 5);
  CHECK(parse_settings("probe = { settle_s = 121 }\n", true).refused);
  // The read-only round (Lodestar's L5): a probe build's too, and a word of its own.
  const LoadedSettings read = parse_settings("probe = { round = \"read\", settle_s = 1 }\n", true);
  REQUIRE_FALSE(read.refused);
  CHECK(read.settings.probe.round == ProbeRound::Read);
  CHECK(std::string(to_string(read.settings.probe.round)) == "read");
  CHECK(parse_settings("probe = { round = \"read\" }\n", false).refused);
  CHECK(parse_settings("probe = { round = \"write\" }\n", true).refused);
  // R1 was retired (O106): its word and its own keys are refused as any unknown one is.
  const LoadedSettings r1 = parse_settings("probe = { round = \"r1\" }\n", true);
  CHECK(r1.refused);
  REQUIRE(r1.problems.size() == 1);
  CHECK(r1.problems[0].where == "probe.round");
  CHECK(r1.settings.probe.round == ProbeRound::None);
  CHECK(parse_settings("probe = { register = \"auto\" }\n", true).refused);
  CHECK(parse_settings("probe = { object_size = 0x400 }\n", true).refused);
}

TEST_CASE("settings: the pages' edits, written over an accepted file and never over a refused one") {
  using en::shell::SettingsEdit;
  const std::vector<SettingsEdit> level{SettingsEdit{"debug.log_level", true, false, "debug"}};
  // No file yet: one of EldenNet's own, schema 2 and the edit.
  auto made = en::shell::edit_settings("", level, false);
  REQUIRE(made.ok);
  CHECK(made.text.find("schema = 2") != std::string::npos);
  auto read = en::shell::parse_settings(made.text, false);
  CHECK_FALSE(read.refused);
  CHECK(read.settings.log_level == en::shell::LogLevel::Debug);

  // A file of the player's: its other keys and the later increments' tables kept, an inline table edited in place.
  const std::string mine =
      "schema = 2\nenabled = true\ndebug = { panel = true, record_sessions = true }\n"
      "features = { chat = true }\n[pacing]\ngap_s = 2.5\n";
  const std::vector<SettingsEdit> two{SettingsEdit{"debug.log_level", true, false, "warn"},
                                      SettingsEdit{"debug.panel", false, false, {}}};
  auto edited = en::shell::edit_settings(mine, two, false);
  REQUIRE(edited.ok);
  read = en::shell::parse_settings(edited.text, false);
  REQUIRE_FALSE(read.refused);
  CHECK(read.settings.log_level == en::shell::LogLevel::Warn);
  CHECK_FALSE(read.settings.debug_panel);
  CHECK(read.settings.record_sessions);
  CHECK(read.settings.enabled);
  CHECK(edited.text.find("chat = true") != std::string::npos);
  CHECK(edited.text.find("gap_s = 2.5") != std::string::npos);

  // Never over a file the parse refuses, nor into a key that is not a table's, nor to a file the parse would refuse.
  auto refused = en::shell::edit_settings("schema = 2\ndebug = { log_level = \"loud\" }\n", level, false);
  CHECK_FALSE(refused.ok);
  CHECK(refused.why == "refused");
  CHECK(en::shell::edit_settings("enabled = true\n", {SettingsEdit{"enabled.on", false, true, {}}}, false).why ==
        "not_a_table");
  CHECK(en::shell::edit_settings("", {SettingsEdit{"debug.colour", true, false, "red"}}, false).why ==
        "result_refused");
  CHECK(en::shell::edit_settings("", {SettingsEdit{"schema", false, true, {}}}, false).why == "unknown_key");
}
