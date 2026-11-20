// R1's probe report (probe_report.hpp): written through the closed vocabulary, linting clean, numbers and labels only.
#include <doctest.h>

#include <array>
#include <string>

#include "en/gal/probe_report.hpp"
#include "en/gal/report.hpp"

using namespace en::gal::probe;

TEST_CASE("probe report: every part written lints clean") {
  Findings f;
  f.stage = "running";
  f.elapsed_ms = 61000;
  f.gate_passed = true;
  f.derive_passes = 2;
  f.derive_ms = 3100;
  f.derive_written = true;
  f.checks.push_back(Check{"gate", "", CheckStatus::Confirmed, std::nullopt, std::nullopt});
  f.checks.push_back(Check{"group", "", CheckStatus::Confirmed, 110, 110});
  f.checks.push_back(Check{"singleton", "cs_task", CheckStatus::Missing, std::nullopt, std::nullopt});
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
  f.bases.push_back(Base{0x3B0000, 3, 12, 3, 6, 1});
  f.rising.push_back(Rising{0x4C0020, 0x20, 4, 2, 0});
  f.statics.push_back(Static{"game_man", 120, 120, {0x4C0030}});
  f.hub_search.push_back(HubSearch{2000, 4600, 4u << 20, 120, 1, 1, 0, {HubHit{1, {0x48, 0x10}, 2}}});
  CalleeCode c;
  c.read = true;
  c.extent = c.bytes = 0x180;
  c.instructions = 90;
  c.first_call = 0x40;
  c.first_ret = 0x170;
  c.call_count = 2;
  c.calls.push_back(CalleeCall{0x40, CallForm::Memory, std::nullopt, 0, std::nullopt, 0x18, 8});
  c.calls.push_back(CalleeCall{0x60, CallForm::Rip, 0x5A0000, std::nullopt, std::nullopt, 0, std::nullopt});
  c.branch_count = 1;
  c.branches.push_back(CalleeBranch{0x20, -0x10});
  ArgUse task;
  task.reg = 8;
  task.stores.push_back(ArgStore{0x30, 3, 1, 1, std::nullopt, -0x28, false, 8});
  task.saved = {3};
  task.compares.push_back(ArgCompare{0x18, 3, std::nullopt});
  c.args.push_back(task);
  f.callee_code = c;
  MemorySearch m;
  m.at_ms = 20000;
  m.found = 1;
  m.own_found = 2;
  m.stopped = SearchStop::Time;
  m.hits.push_back(MemoryHit{false, 0, RegionKind::Image, 0x04, ModuleKind::Game, SectionKind::Data, 0x4C0040, 1});
  m.hits.push_back(MemoryHit{true, 0x10, RegionKind::Private, 0x04, ModuleKind::None, SectionKind::None, std::nullopt, 0});
  f.memory_search.push_back(m);
  const auto text = write_report(f);
  REQUIRE(text.has_value());
  const std::array<const en::gal::report::Vocabulary*, 1> known = {&vocabulary()};
  CHECK(en::gal::report::lint(*text, known).has_value());
  CHECK(text->find("\"run_slot\": 1") != std::string::npos);
  CHECK(text->find("\"stopped\": \"time_cap\"") != std::string::npos);
  CHECK(text->find("\"disp\": -40") != std::string::npos);
  CHECK(text->find("\"module\": \"game\"") != std::string::npos);
}

TEST_CASE("probe report: a label that is not one is refused, and no text is left") {
  Findings f;
  f.statics.push_back(Static{"Not A Label", 0, 0, {}});
  CHECK_FALSE(write_report(f).has_value());
}
