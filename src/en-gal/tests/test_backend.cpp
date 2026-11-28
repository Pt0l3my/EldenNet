// ErBackend over FakeSys (backend.hpp; ARCH §6.6 to §6.9, §7.1): the snapshot's fields from an invented world, phase A
// and phase B, R5's re-resolution with moving, nulled and repointed statics and vtable swaps, R11 while loading, the
// breakers on real faults, the inert task, the commands and the render feed. Everything in the world is invented.
#include <doctest.h>

#include <cmath>
#include <memory>

#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;
using en::proto::GameCommand;
using en::proto::Group;
using en::proto::GroupState;
using en::proto::Lifecycle;

namespace {

constexpr std::uint64_t kFrame = 16'667;  // 60 FPS

GroupState state_of(const Running& r, Group g) { return r.task.backend().book().state(g); }

std::shared_ptr<const std::vector<std::uint32_t>> watch(std::vector<std::uint32_t> ids) {
  return std::make_shared<const std::vector<std::uint32_t>>(std::move(ids));
}

}  // namespace

TEST_CASE("backend: at the title, phase A passes every group, and snapshots read the session and the lifecycle only") {
  Running r;
  r.start();
  REQUIRE(r.task.status().state.load() == TaskState::Running);
  CHECK(r.task.status().group_ok.load());
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    INFO(en::proto::to_string(static_cast<Group>(g)));
    CHECK(state_of(r, static_cast<Group>(g)) == (static_cast<Group>(g) == Group::Status ? GroupState::Tripped : GroupState::Ok));
  }
  REQUIRE(r.io.count_life<Lifecycle::DataReady>() == 1);
  const auto& ready = std::get<Lifecycle::DataReady>(r.io.life[0].payload);
  CHECK(ready.build == r.d.profile().key());
  CHECK((ready.caps.groups & en::proto::bit_of(Group::World)) == en::proto::bit_of(Group::World));
  r.run(1.0, 60);
  const auto& s = r.io.last();
  CHECK(r.io.published >= 3);
  CHECK(s.life.screen == en::proto::Screen::Title);
  CHECK_FALSE(s.life.main_player);
  CHECK(s.session.lobby == en::proto::LobbyState::None);
  CHECK_FALSE(s.player.has_value());
  CHECK(s.chars.empty());
  CHECK(r.task.backend().last_mode() == ReadMode::Dormant);
}

TEST_CASE("backend: in the world, every group's fields come from the world as the layout places them") {
  Running r;
  r.start();
  r.w.world();
  r.io.push_command(GameCommand{GameCommand::SetWatchFlags{7, watch({100, 1234001, 5000005, 777000, 1234002})}});
  GameCommand::ResolveText text;
  text.fmg = 5;
  REQUIRE(text.ids.try_push_back(7001));
  REQUIRE(text.ids.try_push_back(7002));
  r.io.push_command(GameCommand{text});
  GameCommand::ResolveParams params;
  REQUIRE(params.ids.try_push_back(4100));
  REQUIRE(params.ids.try_push_back(4999));
  r.io.push_command(GameCommand{params});
  r.w.set_flag(5000005, true);
  r.run(2.0, 60);
  const auto& s = r.io.last();
  REQUIRE(s.player.has_value());
  const auto& p = *s.player;
  CHECK(p.handle.raw == r.w.player_handle);
  CHECK(p.level == 12);
  CHECK(p.runes_held == 4321);
  CHECK(p.archetype == 3);
  CHECK(p.body == en::proto::BodyType::B);
  CHECK(p.hp == en::proto::Meter{1100, 1200});
  CHECK(p.fp == en::proto::Meter{70, 80});
  CHECK(p.save_slot == 2);
  CHECK(p.deaths == 3);
  CHECK(p.play_time_ms > 1500);
  CHECK(p.gear.left_weapon == 1001000);
  CHECK(p.pos == std::array<float, 3>{100.0f, 5.0f, 200.0f});
  CHECK_FALSE(p.riding);
  CHECK(s.session.role == en::proto::MultiplayRole::Host);
  CHECK(s.session.is_my_world);
  CHECK(s.session.player_set_len == 1);
  CHECK(s.life.screen == en::proto::Screen::InGame);
  CHECK(s.ui.hud == en::proto::HudState::Default);
  CHECK(s.ui.subtitles_on);
  CHECK_FALSE(s.ui.talk_cam);
  CHECK_FALSE(s.ui.popup_talk_id.has_value());
  REQUIRE(s.boss.bars.size() == 1);
  CHECK(s.boss.bars[0].name_id == 9100);
  CHECK(s.boss.active);
  REQUIRE(s.place.has_value());
  CHECK(s.place->place_id == 6100100u);
  REQUIRE(s.clock.has_value());
  CHECK(s.clock->h == 9);
  CHECK(s.clock->m == 30);
  // Six characters, nearest first, with distances of our own.
  REQUIRE(s.chars.size() == 6);
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK(s.chars[i].handle.raw == r.w.npc_handles[i]);
    CHECK(s.chars[i].dist_m == doctest::Approx(3.0 + 4.0 * static_cast<double>(i)));
    CHECK(s.chars[i].on_screen);
  }
  CHECK(s.chars[2].dead);
  CHECK(s.chars[2].hp.cur == 0);
  CHECK(s.chars[0].param == 4100);
  CHECK(std::isfinite(s.chars[0].bearing_deg));
  // The flags: on every fourth snapshot, valid where the group exists.
  CHECK(r.task.status().flag_frames.load() >= 1);
  std::optional<en::proto::FlagFrame> frame;
  for (int i = 0; i < 8 && !frame; ++i) {
    r.run(0.25, 60);
    frame = r.io.last().flags;
  }
  REQUIRE(frame.has_value());
  CHECK(frame->watch_version == 7);
  CHECK(frame->valid[0]);
  CHECK(frame->bits[0]);  // 100
  CHECK(frame->valid[1]);
  CHECK_FALSE(frame->bits[1]);  // 1234001
  CHECK(frame->valid[2]);
  CHECK(frame->bits[2]);  // 5000005, a block by address
  CHECK_FALSE(frame->valid[3]);  // 777000: no such group, reads false
  CHECK(frame->valid[4]);
  // The look-ups were answered once, within their caps (only ids and counts are the tests' concern).
  CHECK(r.io.count_life<Lifecycle::PlaythroughReady>() == 1);
  for (std::size_t i = 0; i < r.io.life_count; ++i) {
    if (const auto* pr = std::get_if<Lifecycle::PlaythroughReady>(&r.io.life[i].payload)) {
      CHECK(pr->slot == 2);
      CHECK(pr->name.size() == 13);
      CHECK(pr->keepsake == 2);
    }
  }
  CHECK(r.task.backend().book().state(Group::Text) == GroupState::Ok);
  CHECK(r.task.status().commands.load() == 3);
}

TEST_CASE("backend: the main player's death is its health at nothing; the dead bit is the world's characters' alone") {
  // The twentieth round in the game (WHY§655, §657): the main player's health read nothing for eleven seconds with
  // the bit that marks a dead character of the world clear in every frame. So the player's death is read from its
  // health, with its object there; the bit decides nothing for the player, and decides alone for the others.
  Running r;
  r.start();
  r.w.world();
  r.run(1.0, 60);
  REQUIRE(r.io.last().player.has_value());
  CHECK_FALSE(r.io.last().player->dead);
  REQUIRE_FALSE(r.io.last().chars.empty());
  CHECK_FALSE(r.io.last().chars[0].dead);
  const std::uint64_t parts = r.w.mem.get<std::uint64_t>(r.w.player + 0x30);
  const std::uint64_t vitals = r.w.mem.get<std::uint64_t>(parts + 0x0);
  // The dead bit set on the player with health left: not dead.
  r.w.mem.put<std::uint8_t>(r.w.player + 0x21, static_cast<std::uint8_t>(r.w.mem.get<std::uint8_t>(r.w.player + 0x21) | 0x20));
  r.run(0.5, 60);
  REQUIRE(r.io.last().player.has_value());
  CHECK_FALSE(r.io.last().player->dead);
  // Its health at nothing with the bit clear: dead.
  r.w.mem.put<std::uint8_t>(r.w.player + 0x21, static_cast<std::uint8_t>(r.w.mem.get<std::uint8_t>(r.w.player + 0x21) & ~0x20));
  const std::int32_t before = r.w.mem.get<std::int32_t>(vitals + 0x0);
  r.w.mem.put<std::int32_t>(vitals + 0x0, 0);
  r.run(0.5, 60);
  REQUIRE(r.io.last().player.has_value());
  CHECK(r.io.last().player->dead);
  CHECK(r.io.last().player->hp.cur == 0);
  // Back with health: alive again.
  r.w.mem.put<std::int32_t>(vitals + 0x0, before);
  r.run(0.5, 60);
  CHECK_FALSE(r.io.last().player->dead);
  // A character of the world: dead by the bit, whatever its health reads; and not dead without it at no health.
  const std::uint64_t npc = r.w.npcs[0];
  const std::uint64_t npc_parts = r.w.mem.get<std::uint64_t>(npc + 0x30);
  const std::uint64_t npc_vitals = r.w.mem.get<std::uint64_t>(npc_parts + 0x0);
  const std::uint64_t handle = r.w.mem.get<std::uint64_t>(npc + 0x8);
  const auto of = [&]() -> const en::proto::Chr* {
    for (const en::proto::Chr& c : r.io.last().chars) {
      if (c.handle.raw == handle) return &c;
    }
    return nullptr;
  };
  r.w.mem.put<std::uint8_t>(npc + 0x21, static_cast<std::uint8_t>(r.w.mem.get<std::uint8_t>(npc + 0x21) | 0x20));
  r.run(0.5, 60);
  REQUIRE(of() != nullptr);
  CHECK(of()->dead);
  r.w.mem.put<std::uint8_t>(npc + 0x21, static_cast<std::uint8_t>(r.w.mem.get<std::uint8_t>(npc + 0x21) & ~0x20));
  r.w.mem.put<std::int32_t>(npc_vitals + 0x0, 0);
  r.run(0.5, 60);
  REQUIRE(of() != nullptr);
  CHECK_FALSE(of()->dead);
}

TEST_CASE("backend: the core's text and param look-ups are answered in the snapshot") {
  Running r;
  r.start();
  r.w.world();
  r.run(0.3, 60);
  GameCommand::ResolveText text;
  text.fmg = 5;
  REQUIRE(text.ids.try_push_back(7001));
  REQUIRE(text.ids.try_push_back(7002));
  REQUIRE(text.ids.try_push_back(1));
  r.io.push_command(GameCommand{text});
  GameCommand::ResolveParams params;
  REQUIRE(params.ids.try_push_back(4100));
  REQUIRE(params.ids.try_push_back(4999));
  r.io.push_command(GameCommand{params});
  std::size_t texts = 0, rows = 0;
  for (int i = 0; i < 30 && (texts == 0 || rows == 0); ++i) {
    r.run(1.0 / 60, 60);
    texts = std::max(texts, r.io.last().new_text.size());
    rows = std::max(rows, r.io.last().new_params.size());
    if (texts == 3) {
      const auto& t = r.io.last().new_text;
      CHECK(t[0].id == 7001);
      REQUIRE(t[0].text.has_value());
      CHECK(t[0].text->size() == 11);
      CHECK_FALSE(t[1].text.has_value());  // the id's group holds no string for it
      CHECK_FALSE(t[2].text.has_value());  // no group holds the id
    }
    if (rows == 1) {
      const auto& p = r.io.last().new_params[0];
      CHECK(p.id == 4100);
      REQUIRE(p.fields.size() == 2);
      CHECK(p.fields[0].value == 9001);
      CHECK(p.fields[1].value == 11);
    }
  }
  CHECK(texts == 3);
  CHECK(rows == 1);
}

TEST_CASE("backend: while loading only the lifecycle subset is read (R11)") {
  Running r;
  r.start();
  r.w.world();
  r.run(1.0, 60);
  r.w.loading();
  const std::uint64_t before = r.task.backend().reader().total_copies();
  const std::uint64_t snaps = r.task.status().snapshots.load();
  r.run(2.0, 60);
  const std::uint64_t built = r.task.status().snapshots.load() - snaps;
  REQUIRE(built >= 7);
  const auto& s = r.io.last();
  CHECK(s.life.screen == en::proto::Screen::Loading);
  CHECK(s.life.load_counter == 1u);
  CHECK_FALSE(s.player.has_value());
  CHECK(r.task.backend().last_mode() == ReadMode::Loading);
  CHECK(r.task.status().loading_only.load() >= built);
  // Three statics and three objects a snapshot: the characters (main player), the menus (screen) and the veil.
  CHECK((r.task.backend().reader().total_copies() - before) <= built * 6);
}

TEST_CASE("backend: statics that move, are nulled and repointed are re-read every tick (R5)") {
  for (const Move mode : {Move::Poison, Move::Free, Move::Keep}) {
    Running r;
    r.start();
    r.w.world();
    r.run(1.0, 60);
    for (int m = 0; m < 4; ++m) {
      r.w.move_all(mode);
      r.run(0.6, 60);
      const auto& s = r.io.last();
      REQUIRE(s.player.has_value());
      CHECK(s.player->level == 12);
      CHECK(s.chars.size() == 6);
      CHECK(s.place.has_value());
    }
    for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
      if (static_cast<Group>(g) == Group::Status) continue;
      INFO(en::proto::to_string(static_cast<Group>(g)));
      CHECK(state_of(r, static_cast<Group>(g)) == GroupState::Ok);
      CHECK(r.task.backend().book().breaker_total(static_cast<Group>(g)) == 0);
    }
    // A static nulled: its group reads nothing this tick, with no fault; repointed, it reads again.
    const std::uint64_t field = r.w.instance("field_area");
    r.w.mem.put<std::uint64_t>(r.w.at("st_field_area"), 0);
    r.run(0.3, 60);
    CHECK_FALSE(r.io.last().place.has_value());
    r.w.mem.put<std::uint64_t>(r.w.at("st_field_area"), field);
    r.run(0.3, 60);
    CHECK(r.io.last().place.has_value());
    CHECK(r.task.backend().book().breaker_total(Group::Place) == 0);
  }
}

TEST_CASE("backend: a vtable that does not match skips its group, and is a fault only on three ticks in a row") {
  Running r;
  r.start();
  r.w.world();
  r.run(1.0, 60);
  const std::uint64_t hud = r.w.instance("hud");
  r.w.set_vtable(hud, "tomb");
  r.run(0.3, 60);  // one snapshot
  CHECK(r.io.last().ui.hud == en::proto::HudState::Unknown);
  CHECK(r.task.backend().book().breaker_total(Group::Ui) == 0);
  r.w.set_vtable(hud, "hud");
  r.run(0.3, 60);
  CHECK(r.io.last().ui.hud == en::proto::HudState::Default);
  r.w.set_vtable(hud, "tomb");
  r.run(0.8, 60);  // three snapshots in a row
  CHECK(r.task.backend().book().breaker_total(Group::Ui) == 1);
  CHECK(r.task.backend().vtable_skips() >= 4);
  r.w.set_vtable(hud, "hud");
  // One entry of the near list whose vtable is no character's is dropped from the snapshot, softly.
  r.w.set_vtable(r.w.npcs[4], "prop");
  r.run(0.3, 60);
  CHECK(r.io.last().chars.size() == 5);
  CHECK(r.task.backend().book().breaker_total(Group::World) == 0);
  CHECK(r.task.backend().soft_skips(Group::World) >= 1);
  // More than a quarter of them: a hard fault for world.
  r.w.set_vtable(r.w.npcs[0], "prop");
  r.w.set_vtable(r.w.npcs[1], "prop");
  r.run(0.3, 60);
  CHECK(r.task.backend().book().breaker_total(Group::World) == 1);
}

TEST_CASE("backend: a near list longer than a snapshot walks is read for its nearest, and is no fault (WHY§572)") {
  Running r;
  r.start();
  r.w.world();
  r.run(1.0, 60);
  REQUIRE(state_of(r, Group::World) == GroupState::Ok);
  // 119 elements, as the game's in the open world: the characters nearest first, a metre and a half apart.
  const std::uint64_t roster = r.w.instance("characters");
  const std::uint64_t run = r.w.mem.alloc(119 * 0x10);
  for (std::uint32_t i = 0; i < 119; ++i) {
    r.w.mem.put<std::uint64_t>(run + 0x10 * i, r.w.npcs[i % r.w.npcs.size()]);
    r.w.mem.put<float>(run + 0x10 * i + 0x8, 2.0f + 1.5f * static_cast<float>(i));
  }
  r.w.mem.put<std::uint64_t>(roster + 0x20, run);
  r.w.mem.put<std::uint64_t>(roster + 0x28, run + 119 * 0x10);
  r.run(1.0, 60);  // three snapshots
  CHECK(r.task.backend().near_all() == 119);
  CHECK(r.task.backend().book().breaker_total(Group::World) == 0);
  CHECK(state_of(r, Group::World) == GroupState::Ok);
  CHECK_FALSE(r.io.last().chars.empty());
  // Bounds that make no sense are still a fault: the end before the start, three snapshots in a row.
  r.w.mem.put<std::uint64_t>(roster + 0x28, run - 0x10);
  r.run(1.0, 60);
  CHECK(r.task.backend().book().breaker_total(Group::World) >= 1);
}

TEST_CASE("backend: real faults trip an optional group's breaker (L3), and a structural one makes the task inert") {
  Running r;
  r.start();
  r.w.world();
  r.run(1.0, 60);
  // The HUD's object freed under its static: every read faults.
  r.w.mem.release(r.w.instance("hud"));
  r.run(1.0, 60);
  CHECK(state_of(r, Group::Ui) == GroupState::Tripped);
  CHECK(r.task.backend().book().why(Group::Ui) == GroupWhy::Breaker);
  CHECK(r.io.count_life<Lifecycle::GroupTripped>() == 1);
  CHECK(r.task.status().state.load() == TaskState::Running);
  CHECK(r.io.last().player.has_value());
  CHECK(r.io.last().health.of(Group::Ui).state == GroupState::Tripped);
  // The probe's injection on world: every read faults, three snapshots trip it, and the task goes inert (L1).
  r.task.backend().reader().inject(Injection{Group::World, 1, 0x0000'0123'0000'0000ull});
  r.run(1.5, 60);
  CHECK(state_of(r, Group::World) == GroupState::Tripped);
  CHECK(r.task.status().state.load() == TaskState::Inert);
  CHECK(r.task.status().inert_why.load() == InertWhy::Structural);
  // Inert: the closure returns at once; nothing more is read.
  const auto& copier = static_cast<const FakeCopier&>(r.w.sys->copier());
  const std::uint64_t copies = copier.copies;
  r.run(2.0, 60);
  CHECK(copier.copies == copies);
}

TEST_CASE("backend: a manager's fields far into a large object are read by copies of their own") {
  WorldOptions o;
  o.far_roster = true;  // the roster is 0x12000 bytes, its fields 0x10000 bytes in; the layout says each is far
  Running r(o);
  r.start();
  REQUIRE(r.task.status().state.load() == TaskState::Running);
  const ClassLayout* roster = r.d.layout().find_class("characters");
  REQUIRE(roster != nullptr);
  CHECK(roster->copy_size == 0x8u);  // the first word alone, for the class's check
  CHECK(roster->far_bytes == 40);    // a pointer, a vector's two, a pointer and a number, each at a multiple of 8
  r.w.world();
  r.run(2.0, 60);
  const auto& s = r.io.last();
  // The world and the session read as they do from a near roster: the main player, the near list, the player set.
  CHECK(s.life.main_player);
  REQUIRE(s.player.has_value());
  Running plain;  // the same world with the roster's fields at its start
  plain.start();
  plain.w.world();
  plain.run(2.0, 60);
  CHECK(s.chars.size() == plain.io.last().chars.size());
  CHECK(s.chars.size() >= r.w.npcs.size());
  CHECK(state_of(r, Group::World) == GroupState::Ok);
  CHECK(state_of(r, Group::Session) == GroupState::Ok);
  CHECK(r.task.backend().book().breaker_total(Group::World) == 0);
  // Back at the title the far pointer reads empty, as a near one does.
  r.w.title();
  r.run(0.5, 60);
  CHECK_FALSE(r.io.last().life.main_player);
  CHECK(r.io.last().chars.empty());
}

TEST_CASE("backend: a clock kept as runs of bits in one word reads by the layout's shift and width") {
  WorldOptions o;
  o.clock_bits = true;
  Running r(o);
  r.start();
  r.w.world();
  r.run(1.0, 60);
  REQUIRE(r.io.last().clock.has_value());
  CHECK(r.io.last().clock->h == 9);  // the word's other bits are all set: only the two runs are read
  CHECK(r.io.last().clock->m == 30);
  r.w.set_hour(17);
  r.run(0.3, 60);
  REQUIRE(r.io.last().clock.has_value());
  CHECK(r.io.last().clock->h == 17);
  CHECK(r.io.last().clock->m == 30);
  r.w.set_hour(25);  // five bits hold it; a day does not
  r.run(0.3, 60);
  CHECK(r.task.backend().book().breaker_total(Group::Place) >= 1);
  CHECK_FALSE(r.io.last().clock.has_value());
}

TEST_CASE("backend: per-tick plausibility: a camera that is no rotation, an hour past the day, a session not solo") {
  Running r;
  r.start();
  r.w.world();
  r.run(1.0, 60);
  const std::uint64_t view = r.w.mem.get<std::uint64_t>(r.w.instance("camera") + 0x8);
  r.w.mem.put<float>(view + 0x40, 5.0f);  // a field of view past 2.5 rad
  r.w.set_hour(25);
  r.run(0.3, 60);
  CHECK(r.task.backend().book().breaker_total(Group::Camera) >= 1);
  CHECK(r.task.backend().book().breaker_total(Group::Camera) <= 2);
  CHECK(r.task.backend().book().breaker_total(Group::Place) >= 1);
  CHECK(r.task.backend().book().breaker_total(Group::Place) <= 2);
  CHECK_FALSE(r.io.last().clock.has_value());
  CHECK(std::isnan(r.io.last().chars[0].bearing_deg));
  r.w.mem.put<float>(view + 0x40, 0.9f);
  r.w.set_hour(10);
  r.w.set_lobby(1);  // hosting: Dormant, the session and the lifecycle only
  r.run(0.3, 60);
  CHECK(r.io.last().session.lobby == en::proto::LobbyState::Hosting);
  CHECK_FALSE(r.io.last().player.has_value());
  CHECK(r.task.backend().last_mode() == ReadMode::Dormant);
  r.w.set_lobby(9);  // no value the layout names
  r.run(0.3, 60);
  CHECK(r.task.backend().book().breaker_total(Group::Session) >= 1);
}

TEST_CASE("backend: the first tick checks the group by name, and phase A's structural failure makes the task inert") {
  {
    FakeWorld w;
    w.layout.replace(w.layout.find("index = 2"), 9, "index = 3");
    const Discovery d = discovered(w);
    FakeIo io;
    GalTask task(*w.sys, d, io);
    REQUIRE(install_task(*w.sys, d, task).has_value());
    for (int i = 0; i < 5; ++i) w.sys->tick(kFrame);
    CHECK(task.status().state.load() == TaskState::Inert);
    CHECK(task.status().inert_why.load() == InertWhy::GroupMismatch);
    CHECK(task.status().group_found_at.load() == 2);
    CHECK(io.published == 0);
  }
  {
    FakeWorld w;
    w.set_lobby(9);
    const Discovery d = discovered(w);
    FakeIo io;
    GalTask task(*w.sys, d, io);
    REQUIRE(install_task(*w.sys, d, task).has_value());
    for (int i = 0; i < 12; ++i) w.sys->tick(kFrame);
    CHECK(task.status().state.load() == TaskState::Inert);
    CHECK(task.status().inert_why.load() == InertWhy::PhaseA);
    CHECK(task.backend().book().why(Group::Session) == GroupWhy::PhaseA);
    CHECK(io.count_life<Lifecycle::DataReady>() == 0);
  }
  {
    // No text tables: the text group fails phase A alone, and the task runs on without it (L3).
    Running r(WorldOptions{true, 6, true, false});
    r.start();
    CHECK(r.task.status().state.load() == TaskState::Running);
    CHECK(state_of(r, Group::Text) == GroupState::Tripped);
    CHECK(r.task.backend().book().why(Group::Text) == GroupWhy::PhaseA);
  }
}

TEST_CASE("backend: a spent budget abandons the snapshot, and the next one carries gap") {
  BackendOptions tight;
  tight.budget = ReadBudget{24, 1u << 16};
  Running r(WorldOptions{}, TaskOptions{}, tight);
  r.start();
  r.w.world();
  r.run(1.0, 60);
  CHECK(r.task.status().abandoned.load() >= 1);
  CHECK(r.io.dropped >= 1);
  // Never a hard fault for it: a spent budget is the snapshot's, not a group's.
  CHECK(r.task.backend().book().breaker_total(Group::World) == 0);
}

TEST_CASE("backend: the render feed follows the tracked characters every frame") {
  Running r;
  r.start();
  r.w.world();
  r.run(0.5, 60);
  GameCommand::SetTracked tracked;
  REQUIRE(tracked.handles.try_push_back(en::proto::ChrHandle{r.w.npc_handles[1]}));
  REQUIRE(tracked.handles.try_push_back(en::proto::ChrHandle{0x1234}));
  r.io.push_command(GameCommand{tracked});
  const std::uint64_t before = r.io.renders;
  r.run(0.5, 60);
  CHECK(r.io.renders - before >= 29);
  REQUIRE(r.io.render.heads.size() == 2);
  CHECK(r.io.render.heads[0].present);
  CHECK(r.io.render.heads[0].pos[0] == doctest::Approx(107.0));
  CHECK_FALSE(r.io.render.heads[1].present);
  CHECK(r.io.render.camera_ok);
  // Gone since the snapshot (freed): simply not present, and no fault is counted; another character at its address
  // (its handle differs) is not taken for it.
  r.w.mem.release(r.w.npcs[1]);
  const auto gt = en::proto::GameThread::issue();
  en::proto::RenderFeed feed;
  const en::proto::ChrHandle want[] = {en::proto::ChrHandle{r.w.npc_handles[1]}, en::proto::ChrHandle{r.w.npc_handles[2]}};
  r.w.mem.put<std::uint64_t>(r.w.npcs[2] + 0x8, 0x5151);
  REQUIRE(r.task.backend().render_feed(gt, want, feed).has_value());
  REQUIRE(feed.heads.size() == 2);
  CHECK_FALSE(feed.heads[0].present);
  CHECK_FALSE(feed.heads[1].present);
  CHECK(r.task.backend().book().breaker_total(Group::World) == 0);
}

TEST_CASE("backend: a replaced watch list goes back to the core, never freed on the game thread (C5)") {
  Running r;
  r.start();
  auto first = watch({100});
  std::weak_ptr<const std::vector<std::uint32_t>> first_weak = first;
  r.io.push_command(GameCommand{GameCommand::SetWatchFlags{1, std::move(first)}});
  r.w.sys->tick(kFrame);
  r.io.push_command(GameCommand{GameCommand::SetWatchFlags{2, watch({100, 200})}});
  r.w.sys->tick(kFrame);
  REQUIRE(r.io.retired_count == 1);
  CHECK(r.io.retired[0]->size() == 1);
  CHECK_FALSE(first_weak.expired());  // the core holds it now, and frees it on its own thread
  r.io.retired[0].reset();
  CHECK(first_weak.expired());
}

TEST_CASE("backend: the world's leads alone read through a layout that places them and little else") {
  // As the game's layout stands after the sixteenth read-only round: the items' statics, the characters manager's
  // far fields, a character's parts with the health and the position, the near list's element, the game's state's
  // slot, the flags; no other class. Discovery is not ready to start, and the world's and the player's groups are
  // not live; the backend reads their leads alone.
  WorldOptions wo;
  wo.far_roster = true;
  FakeWorld w(wo);
  w.world();
  const auto cut = [&](std::initializer_list<std::string_view> classes) {
    std::string layout;
    bool keep = true;
    for (std::size_t at = 0; at < w.layout.size();) {
      const std::size_t end = std::min(w.layout.find('\n', at), w.layout.size() - 1) + 1;
      const std::string_view line(w.layout.data() + at, end - at);
      if (line.starts_with("[")) {
        keep = !line.starts_with("[class.");
        for (const std::string_view c : classes) {
          keep = keep || line.starts_with("[class." + std::string(c) + "]") ||
                 line.starts_with("[class." + std::string(c) + ".");
        }
      }
      if (keep) layout.append(line);
      at = end;
    }
    return layout;
  };
  const std::string layout = cut({"characters", "near_entry", "character", "character_parts", "vitals", "motion",
                                  "game_state", "event_flags", "flag_node", "world_clock", "save_data",
                                  "player_data", "item_id"});
  REQUIRE(layout.find("[class.vitals]") != std::string::npos);
  REQUIRE(layout.find("[class.player_character]") == std::string::npos);
  REQUIRE(layout.find("[class.menus]") == std::string::npos);
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  DiscoveryOptions options;
  options.frame = FrameProvider::OwnThread;
  options.partial = true;
  const auto d = discover(*w.sys, *gated, w.patterns, layout, options);
  if (!d.has_value()) {
    for (const auto& e : d.error()) MESSAGE(e.where << ": " << e.problem);
  }
  REQUIRE(d.has_value());
  CHECK_FALSE(d->report().ready);
  ErBackend backend(*w.sys, *d);
  CHECK_FALSE(backend.inert());
  const auto gt = en::proto::GameThread::issue();
  (void)backend.first_tick(gt, false);
  const std::uint64_t game = w.instance("game_state");
  const std::uint64_t parts = w.mem.get<std::uint64_t>(w.player + 0x30);
  const std::uint64_t vitals = w.mem.get<std::uint64_t>(parts + 0x0), motion = w.mem.get<std::uint64_t>(parts + 0x8);
  w.mem.put<std::int32_t>(game + 0x8, 6);
  w.mem.put<std::int32_t>(vitals + 0x0, 812);
  w.mem.put<std::int32_t>(vitals + 0x4, 1450);
  const auto n = static_cast<std::uint32_t>(w.npcs.size());
  REQUIRE(n >= 3);
  ErBackend::WorldAlone a = backend.world_alone(gt);
  CHECK(a.lacks == 0);
  CHECK(a.state);
  REQUIRE(a.save_slot.has_value());
  CHECK(*a.save_slot == 6);
  CHECK(a.characters);
  CHECK(a.player);
  REQUIRE(a.hp.has_value());
  CHECK(a.hp->cur == 812);
  CHECK(a.hp->max == 1450);
  REQUIRE(a.pos.has_value());
  CHECK((*a.pos)[0] == w.mem.get<float>(motion + 0x10));
  CHECK((*a.pos)[2] == w.mem.get<float>(motion + 0x18));
  CHECK(a.list);
  CHECK(a.near_all == n);
  CHECK(a.near_read == n);
  CHECK(a.near_chr == n);
  CHECK(a.near_distance == n);
  CHECK(a.near_rising == n);
  // A character's other fields, where asked for (the nineteenth round): each through the layout's own field, counted
  // over the list's characters that are not the main player, and none kept.
  using WA = ErBackend::WorldAlone;
  CHECK(a.fields_bound == (WA::kHandle | WA::kParam | WA::kModel | WA::kEntity | WA::kTeam | WA::kDead |
                           WA::kOnScreen | WA::kType | WA::kBlock));
  CHECK_FALSE(a.fields);
  CHECK(a.near_object == 0);
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint64_t c = w.npcs[i];
    const std::uint32_t kind = 20'000'000 + 10'000 * (i / 2);
    w.mem.put<std::uint64_t>(c + 0x8, 0x00A0'0000'0000'1000ull + i);
    w.mem.put<std::uint32_t>(c + 0x10, kind);
    w.mem.put<std::uint32_t>(c + 0x14, i == 0 ? 7 : kind / 10'000);  // one whose model is not its kind's
    w.mem.put<std::uint32_t>(c + 0x18, i == 1 ? 0 : 5000 + i);       // one placed by no map
    w.mem.put<std::uint8_t>(c + 0x20, i == 0 ? 3 : 6);
    w.mem.put<std::uint8_t>(c + 0x21, static_cast<std::uint8_t>((i == 2 ? 0x20 : 0) | (i < 2 ? 0x02 : 0)));
    w.mem.put<std::int32_t>(c + 0x1C, i == 0 ? 9 : 5);                     // the type: one of another
    w.mem.put<std::uint32_t>(c + 0x24, i == 0 ? 0x3C2C'2500u : i == 1 ? 0xFFFF'FFFFu : 0x3C2C'2400u);  // the block
  }
  w.mem.put<std::int32_t>(w.player + 0x1C, 0);
  w.mem.put<std::uint32_t>(w.player + 0x24, 0x3C2C'2400u);
  w.mem.put<std::uint64_t>(w.player + 0x8, 0x00FF'0000'0000'0001ull);
  w.mem.put<std::uint32_t>(w.player + 0x10, 0);
  w.mem.put<std::uint8_t>(w.player + 0x20, 1);
  w.mem.put<std::uint8_t>(w.player + 0x21, 0);
  const WA f = backend.world_alone(gt, true);
  CHECK(f.fields);
  CHECK(f.near_chr == n);
  CHECK(f.near_self == 0);
  CHECK(f.near_object == n);
  CHECK(f.near_handle == n);
  CHECK(f.near_handles == n);
  CHECK(f.near_param == n);
  CHECK(f.near_kinds == (n + 1) / 2);
  CHECK(f.near_kin == n - 1);
  CHECK(f.near_entity == n - 1);
  CHECK(f.near_entities == n - 1);
  CHECK(f.near_teams == 2);
  CHECK(f.near_team_most == n - 1);
  CHECK(f.near_dead == 1);
  CHECK(f.near_on_screen == 2);
  CHECK(f.self_object);
  CHECK(f.self_handle);
  CHECK_FALSE(f.self_param);
  CHECK_FALSE(f.self_dead);
  CHECK(f.self_team_alone);
  // The type and the block: two types with all but one of one; a block that reads as one in all but the character
  // that has none, and the main player's own in the rest but one.
  CHECK(f.near_types == 2);
  CHECK(f.near_type_most == n - 1);
  CHECK(f.near_block == n - 1);
  CHECK(f.near_block_self == n - 2);
  CHECK(f.self_type_none);
  REQUIRE(f.self_block.has_value());
  CHECK(*f.self_block == 0x3C2C'2400u);
  // The clock and the play time, in every call, asked for the fields or not.
  const std::uint64_t clock_at = w.instance("world_clock"), save_at = w.instance("save_data");
  w.mem.put<std::uint8_t>(clock_at + 0x10, 21);
  w.mem.put<std::uint8_t>(clock_at + 0x11, 47);
  w.mem.put<std::uint32_t>(save_at + 0x10, 3'600'123);
  const WA timed = backend.world_alone(gt);
  REQUIRE(timed.clock.has_value());
  CHECK(timed.clock->h == 21);
  CHECK(timed.clock->m == 47);
  REQUIRE(timed.play_time.has_value());
  CHECK(*timed.play_time == 3'600'123);
  // The count of deaths beside it, and the main player's focus and stamina with the health (the twenty-second
  // round): each where the layout binds it.
  REQUIRE(timed.deaths.has_value());
  CHECK(*timed.deaths == 3);
  REQUIRE(timed.fp.has_value());
  CHECK(timed.fp->cur <= timed.fp->max);
  CHECK(timed.fp->max != 0);
  REQUIRE(timed.stamina.has_value());
  CHECK(timed.stamina->cur <= timed.stamina->max);
  // The sheet through the save data's own word: the level in its range, and a name of some letters (their count
  // is the caller's to hold against the call before, and no more).
  CHECK(timed.sheet);
  REQUIRE(timed.level.has_value());
  CHECK(*timed.level == 12);
  REQUIRE(timed.name_units.has_value());
  CHECK(*timed.name_units >= 1);
  CHECK(*timed.name_units <= 16);
  const std::uint64_t sheet_at = w.mem.get<std::uint64_t>(save_at + 0x20);
  w.mem.put<std::uint32_t>(sheet_at + 0x8, 9999);  // a level past the layout's range: none
  const WA odd = backend.world_alone(gt);
  CHECK(odd.sheet);
  CHECK_FALSE(odd.level.has_value());
  CHECK(odd.name_units == timed.name_units);
  w.mem.put<std::uint32_t>(sheet_at + 0x8, 12);
  w.mem.put<std::uint8_t>(clock_at + 0x10, 24);  // an hour that is none: no clock, and no fault
  CHECK_FALSE(backend.world_alone(gt).clock.has_value());
  w.mem.put<std::uint8_t>(clock_at + 0x10, 21);
  REQUIRE(f.hp.has_value());  // the rest reads as before
  CHECK(f.hp->cur == 812);
  // Two characters with one handle show as one handle fewer; the main player in the list is counted apart, and a
  // team it shares is not its alone.
  const std::uint64_t third = w.mem.get<std::uint64_t>(w.near + 0x10 * 2);
  w.mem.put<std::uint64_t>(w.npcs[1] + 0x8, w.mem.get<std::uint64_t>(w.npcs[0] + 0x8));
  w.mem.put<std::uint64_t>(w.near + 0x10 * 2, w.player);
  w.mem.put<std::uint8_t>(w.player + 0x20, 6);
  const WA g = backend.world_alone(gt, true);
  CHECK(g.near_self == 1);
  CHECK(g.near_object == n - 1);
  CHECK(g.near_handle == n - 1);
  CHECK(g.near_handles == n - 2);
  CHECK_FALSE(g.self_team_alone);
  w.mem.put<std::uint64_t>(w.near + 0x10 * 2, third);
  // What changes in the game shows at the next call; an element that holds no fraction, or no character, is counted
  // out by itself and the list is read on (WHY§569).
  w.mem.put<std::int32_t>(vitals + 0x0, 300);
  w.mem.put<float>(motion + 0x10, w.mem.get<float>(motion + 0x10) + 2.5f);
  w.mem.put<std::uint32_t>(w.near + 0x10 * 1 + 0x8, 0x7FC00000u);  // not a number
  w.mem.put<std::uint64_t>(w.near + 0x10 * 2, w.mem.alloc(0x40));  // an address of memory that is no character
  ErBackend::WorldAlone b = backend.world_alone(gt);
  REQUIRE(b.hp.has_value());
  CHECK(b.hp->cur == 300);
  REQUIRE(b.pos.has_value());
  CHECK((*b.pos)[0] == (*a.pos)[0] + 2.5f);
  CHECK(b.near_all == n);
  CHECK(b.near_read == n);
  CHECK(b.near_chr == n - 1);
  CHECK(b.near_distance == n - 1);
  // A list longer than a snapshot allows is told whole and read for its first kWalkEntries elements.
  const std::uint64_t roster = w.instance("characters");
  const std::uint64_t run = w.mem.alloc(119 * 0x10);
  for (std::uint32_t i = 0; i < 119; ++i) {
    w.mem.put<std::uint64_t>(run + 0x10 * i, w.npcs[i % w.npcs.size()]);
    w.mem.put<float>(run + 0x10 * i + 0x8, static_cast<float>(i));
  }
  w.mem.put<std::uint64_t>(roster + 0x10000 + 0x20, run);
  w.mem.put<std::uint64_t>(roster + 0x10000 + 0x28, run + 119 * 0x10);
  ErBackend::WorldAlone c = backend.world_alone(gt);
  CHECK(c.list);
  CHECK(c.near_all == 119);
  CHECK(c.near_read == 48);
  CHECK(c.near_chr == 48);
  CHECK(c.near_distance == 48);
  CHECK(c.near_rising == 48);
  // Bounds that make no sense are no list, and nothing is read through them.
  w.mem.put<std::uint64_t>(roster + 0x10000 + 0x28, run - 0x10);
  ErBackend::WorldAlone bad = backend.world_alone(gt);
  CHECK_FALSE(bad.list);
  CHECK(bad.near_all == 0);
  CHECK(bad.near_read == 0);
  CHECK(bad.player);  // the rest is read all the same
  // At the title: no main player, an empty list, and no save's slot.
  w.title();
  w.mem.put<std::int32_t>(game + 0x8, -1);
  ErBackend::WorldAlone t = backend.world_alone(gt);
  CHECK(t.state);
  REQUIRE(t.save_slot.has_value());
  CHECK(*t.save_slot == -1);
  CHECK(t.characters);
  CHECK_FALSE(t.player);
  CHECK_FALSE(t.hp.has_value());
  CHECK_FALSE(t.pos.has_value());
  // The game's state not there yet (its static empty): nothing of it, and no fault.
  w.mem.put<std::uint64_t>(w.at("st_game_state"), 0);
  ErBackend::WorldAlone none = backend.world_alone(gt);
  CHECK_FALSE(none.state);
  CHECK_FALSE(none.save_slot.has_value());
  CHECK(backend.reader().faults(FaultKind::Copy) == 0);
  CHECK_FALSE(backend.inert());
  // The snapshot itself reads none of it with such a layout.
  en::proto::WorldSnapshot whole;
  (void)backend.snapshot(gt, whole);
  CHECK_FALSE(whole.player.has_value());
}

TEST_CASE("backend: the world's leads alone read nothing of a part the layout does not bind, and say which") {
  WorldOptions wo;
  wo.far_roster = true;
  FakeWorld w(wo);
  w.world();
  const auto backend_of = [&](std::initializer_list<std::string_view> classes, auto&& then) {
    std::string layout;
    bool keep = true;
    for (std::size_t at = 0; at < w.layout.size();) {
      const std::size_t end = std::min(w.layout.find('\n', at), w.layout.size() - 1) + 1;
      const std::string_view line(w.layout.data() + at, end - at);
      if (line.starts_with("[")) {
        keep = !line.starts_with("[class.");
        for (const std::string_view c : classes) {
          keep = keep || line.starts_with("[class." + std::string(c) + "]") ||
                 line.starts_with("[class." + std::string(c) + ".");
        }
      }
      if (keep) layout.append(line);
      at = end;
    }
    auto gated = gate_image(*w.sys, w.builds);
    REQUIRE(gated.has_value());
    DiscoveryOptions options;
    options.frame = FrameProvider::OwnThread;
    options.partial = true;
    const auto d = discover(*w.sys, *gated, w.patterns, layout, options);
    REQUIRE(d.has_value());
    ErBackend backend(*w.sys, *d);
    const auto gt = en::proto::GameThread::issue();
    (void)backend.first_tick(gt, false);
    then(backend.world_alone(gt), backend);
  };
  using WA = ErBackend::WorldAlone;
  // No field of a character but its parts: the others are not read, asked for or not.
  {
    const std::size_t from = w.layout.find("[class.character.fields]");
    REQUIRE(from != std::string::npos);
    const std::size_t to = w.layout.find("\n[", from + 1);
    std::string fields;
    for (std::size_t at = from; at < to;) {
      const std::size_t end = w.layout.find('\n', at) + 1;
      const std::string_view line(w.layout.data() + at, end - at);
      if (line.starts_with("[") || line.starts_with("parts ")) fields.append(line);
      at = end;
    }
    const std::string whole = w.layout;
    w.layout = whole.substr(0, from) + fields + whole.substr(to);
    backend_of({"characters", "near_entry", "character", "character_parts", "vitals", "motion", "game_state"},
               [&](const WA& a, ErBackend& backend) {
                 CHECK(a.lacks == (WA::kFields | WA::kClock | WA::kPlay | WA::kSheet));
                 CHECK_FALSE(a.sheet);
                 CHECK_FALSE(a.clock.has_value());
                 CHECK_FALSE(a.play_time.has_value());
                 CHECK(a.fields_bound == 0);
                 CHECK(a.hp.has_value());
                 const auto gt = en::proto::GameThread::issue();
                 const WA b = backend.world_alone(gt, true);
                 CHECK_FALSE(b.fields);
                 CHECK(b.near_object == 0);
                 CHECK_FALSE(b.self_object);
                 CHECK(b.near_chr == w.npcs.size());
               });
    w.layout = whole;
  }
  // No vitals: the parts are not read; the slot, the player's class and the list are.
  backend_of({"characters", "near_entry", "character", "character_parts", "motion", "game_state"},
             [&](const WA& a, ErBackend&) {
               CHECK(a.lacks == (WA::kParts | WA::kClock | WA::kPlay | WA::kSheet));
               CHECK(a.player);
               CHECK_FALSE(a.hp.has_value());
               CHECK_FALSE(a.pos.has_value());
               CHECK(a.save_slot.has_value());
               CHECK(a.near_all == w.npcs.size());
             });
  // No state and no element: the slot and the list are not read.
  backend_of({"characters", "character", "character_parts", "vitals", "motion"}, [&](const WA& a, ErBackend&) {
    CHECK(a.lacks == (WA::kSlot | WA::kNear | WA::kClock | WA::kPlay | WA::kSheet));
    CHECK_FALSE(a.state);
    CHECK(a.player);
    CHECK(a.hp.has_value());
    CHECK_FALSE(a.list);
  });
  // Nothing of it at all (the flags' layout): nothing is read, not even the statics.
  backend_of({"event_flags", "flag_node"}, [&](const WA& a, ErBackend& backend) {
    CHECK(a.lacks ==
          (WA::kSlot | WA::kPlayer | WA::kParts | WA::kNear | WA::kFields | WA::kClock | WA::kPlay | WA::kSheet));
    CHECK_FALSE(a.characters);
    CHECK(backend.reader().copies() == 0);
  });
}

TEST_CASE("backend: the flags group alone reads through a layout that places the flags and nothing else") {
  // As the game's layout stands after step L6: the items' statics, the event flags' store and the tree's records;
  // no other class. Discovery is not ready to start (the structural groups lack their fields), but the flags are
  // there, and the backend reads them alone.
  FakeWorld w;
  w.world();
  w.set_flag(5000005, true);
  std::string layout;
  bool keep = true;
  for (std::size_t at = 0; at < w.layout.size();) {
    const std::size_t end = std::min(w.layout.find('\n', at), w.layout.size() - 1) + 1;
    const std::string_view line(w.layout.data() + at, end - at);
    if (line.starts_with("[")) {
      keep = !line.starts_with("[class.") || line.starts_with("[class.event_flags") ||
             line.starts_with("[class.flag_node");
    }
    if (keep) layout.append(line);
    at = end;
  }
  REQUIRE(layout.find("[class.event_flags]") != std::string::npos);
  REQUIRE(layout.find("[class.characters]") == std::string::npos);
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  DiscoveryOptions options;
  options.frame = FrameProvider::OwnThread;
  const auto d = discover(*w.sys, *gated, w.patterns, layout, options);
  if (!d.has_value()) {
    for (const auto& e : d.error()) MESSAGE(e.where << ": " << e.problem);
  }
  REQUIRE(d.has_value());
  CHECK(d->has(Group::Flags));
  CHECK_FALSE(d->has(Group::Player));
  CHECK_FALSE(d->report().ready);
  ErBackend backend(*w.sys, *d);
  CHECK_FALSE(backend.inert());
  const auto gt = en::proto::GameThread::issue();
  (void)backend.first_tick(gt, false);
  REQUIRE(backend.apply(gt, GameCommand{GameCommand::SetWatchFlags{3, watch({100, 1234001, 5000005, 777000})}})
              .has_value());
  en::proto::WorldSnapshot out;
  const ErBackend::FlagsAlone done = backend.flags_alone(gt, out);
  CHECK(done.read);
  REQUIRE(done.known.has_value());
  CHECK(*done.known);  // flag 100, set in every playthrough
  REQUIRE(out.flags.has_value());
  CHECK(out.flags->watch_version == 3);
  CHECK(out.flags->valid[0]);
  CHECK(out.flags->bits[0]);        // 100
  CHECK(out.flags->valid[1]);
  CHECK_FALSE(out.flags->bits[1]);  // 1234001: its block lies by its index in the holder
  CHECK(out.flags->valid[2]);
  CHECK(out.flags->bits[2]);        // 5000005, a block by address
  CHECK_FALSE(out.flags->valid[3]); // 777000: no such group
  CHECK(backend.book().live(Group::Flags));  // untested by phase A, and not tripped
  // A flag set in the game shows at the next call.
  w.set_flag(1234001, true);
  (void)backend.flags_alone(gt, out);
  REQUIRE(out.flags.has_value());
  CHECK(out.flags->bits[1]);
  // The snapshot itself reads no flags with such a layout: the groups its mode asks are tripped.
  en::proto::WorldSnapshot whole;
  (void)backend.snapshot(gt, whole);
  CHECK_FALSE(whole.flags.has_value());
}
