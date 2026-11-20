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
  c.calls.push_back(CalleeCall{0x40, CallForm::Memory, std::nullopt, 0, std::nullopt, 0x18, 8, true, {}});
  c.calls.push_back(CalleeCall{0x60, CallForm::Rip, 0x5A0000, std::nullopt, std::nullopt, 0, std::nullopt, true,
                               {PassedArg{1, 2, false}, PassedArg{8, 8, true}}});
  c.branch_count = 1;
  c.branches.push_back(CalleeBranch{0x20, -0x10});
  CalleeGuard g;
  g.offset = 0x20;
  g.to = -0x10;
  g.cond = 5;
  g.op = GuardOp::Test;
  g.op_at = 0x1C;
  g.width = 1;
  Operand flag;
  flag.form = OperandForm::Memory;
  flag.base = 1;
  flag.base_arg = 1;
  flag.disp = 0x7C;
  Operand bit;
  bit.form = OperandForm::Immediate;
  bit.imm = 1;
  g.operands = {flag, bit};
  c.guards.push_back(g);
  ArgUse task;
  task.reg = 8;
  task.stores.push_back(ArgStore{0x30, 3, 1, 1, std::nullopt, -0x28, false, 8, false});
  task.saved = {3};
  task.compares.push_back(ArgCompare{0x18, 3, std::nullopt});
  c.args.push_back(task);
  f.callee_code = c;
  MemorySearch m;
  m.at_ms = 20000;
  m.found = 1;
  m.own_found = 2;
  m.stopped = SearchStop::Time;
  m.hits.push_back(MemoryHit{false, 0, RegionKind::Image, 0x04, ModuleKind::Game, SectionKind::Data, 0x4C0040, 1,
                               PointsTo::Object, std::nullopt});
  m.hits.push_back(MemoryHit{true, 0x10, RegionKind::Private, 0x04, ModuleKind::None, SectionKind::None, std::nullopt, 0,
                               PointsTo::Slot, 1u});
  m.vtable_found = 1;
  m.own_slot_found = 64;
  m.own_slot_distinct = 64;
  f.memory_search.push_back(m);
  InnerCallee inner;
  inner.rva = 0x1EC000;
  inner.site = 0x22;
  inner.code = c;
  f.callee_inner.push_back(inner);
  HubDiff d;
  d.at_ms = 20000;
  d.changed = 3;
  d.steady_words = 1;
  DiffWord dw;
  dw.depth = 1;
  dw.path = {0x28, 0x10};
  dw.before.kind = ValueKind::Zero;
  dw.after.kind = ValueKind::Game;
  dw.after.section = SectionKind::Rdata;
  dw.after.rva = 0x2A0000;
  dw.steady = true;
  d.words.push_back(dw);
  dw.before.kind = ValueKind::Small;
  dw.before.value = 4;
  dw.after = WordValue{};
  dw.after.kind = ValueKind::Heap;
  dw.after.fresh = true;
  dw.after.group_half = true;
  d.words.push_back(dw);
  f.hub_diff.push_back(d);
  const auto text = write_report(f);
  REQUIRE(text.has_value());
  const std::array<const en::gal::report::Vocabulary*, 1> known = {&vocabulary()};
  CHECK(en::gal::report::lint(*text, known).has_value());
  CHECK(text->find("\"run_slot\": 1") != std::string::npos);
  CHECK(text->find("\"stopped\": \"time_cap\"") != std::string::npos);
  CHECK(text->find("\"disp\": -40") != std::string::npos);
  CHECK(text->find("\"module\": \"game\"") != std::string::npos);
  CHECK(text->find("\"cond\": \"ne\"") != std::string::npos);
  CHECK(text->find("\"points_to\": \"slot\"") != std::string::npos);
  CHECK(text->find("\"kind\": \"heap\"") != std::string::npos);
  CHECK(text->find("\"inner\": [") != std::string::npos);
}

TEST_CASE("probe report: a label that is not one is refused, and no text is left") {
  Findings f;
  f.statics.push_back(Static{"Not A Label", 0, 0, {}});
  CHECK_FALSE(write_report(f).has_value());
}
