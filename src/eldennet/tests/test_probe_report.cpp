// R1's probe report (probe_report.hpp): written through the closed vocabulary, linting clean, numbers and labels only.
#include <doctest.h>

#include <array>
#include <string>

#include "en/gal/report.hpp"
#include "en/shell/probe_report.hpp"

using namespace en::shell;

TEST_CASE("probe report: every part written lints clean") {
  ProbeFindings f;
  f.stage = "running";
  f.elapsed_ms = 61000;
  f.gate_passed = true;
  f.derive_passes = 2;
  f.derive_ms = 3100;
  f.derive_written = true;
  f.checks.push_back(ProbeCheck{"gate", "", CheckStatus::Confirmed, std::nullopt, std::nullopt});
  f.checks.push_back(ProbeCheck{"group", "", CheckStatus::Confirmed, 110, 110});
  f.checks.push_back(ProbeCheck{"singleton", "cs_task", CheckStatus::Missing, std::nullopt, std::nullopt});
  f.hinstance = {0x4C0010};
  f.hinstance_count = 1;
  f.register_status = "ok";
  f.callee = 0x123450;
  f.registers = {1, 2, 8};
  f.group_index = 110;
  f.object_size = 0x800;
  f.vtable_slots = 64;
  f.invocations = 900;
  f.slot_calls = {{0, 1}, {1, 900}, {2, 14}};
  f.object_written = 0x18;
  f.census_done = true;
  f.census_vtables = {{0x2A0000, 3}};
  f.bases.push_back(ProbeBase{0x3B0000, 3, 12, 3, 6, 1});
  f.rising.push_back(ProbeRising{0x4C0020, 0x20, 4, 2, 0});
  f.statics.push_back(ProbeStatic{"game_man", 120, 120, {0x4C0030}});
  const auto text = write_probe_report(f);
  REQUIRE(text.has_value());
  const std::array<const en::gal::report::Vocabulary*, 1> known = {&probe_vocabulary()};
  CHECK(en::gal::report::lint(*text, known).has_value());
  CHECK(text->find("\"run_slot\": 1") != std::string::npos);
}

TEST_CASE("probe report: a label that is not one is refused, and no text is left") {
  ProbeFindings f;
  f.statics.push_back(ProbeStatic{"Not A Label", 0, 0, {}});
  CHECK_FALSE(write_probe_report(f).has_value());
}
