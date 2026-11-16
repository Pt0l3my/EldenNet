// The JSON forms of the game messages (snapshot.hpp, lifecycle.hpp, command.hpp, render.hpp): a round trip for every
// type, through JSON values and through text, with every field away from its default, every container full and ids
// past 2^53; a float that is not finite as null; and refusals that name the path and the problem, never a value. All
// data is invented: no handle, id, name or number here is the game's.
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "en/proto/command.hpp"
#include "en/proto/json.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/render.hpp"
#include "en/proto/snapshot.hpp"

using namespace en::proto;
using nlohmann::json;

namespace {

constexpr std::uint64_t kBig = (std::uint64_t{1} << 60) + 11;  // past 2^53, where a double would lose it

template <class T>
void round_trip(const T& value) {
  const json j = value;
  CHECK(j.get<T>() == value);
  CHECK(read_json<T>(j.dump()) == value);
}

// For a value holding NaN, which is never equal to itself: the JSON form must come back the same.
template <class T>
void round_trip_form(const T& value) {
  const json j = value;
  const json again = read_json<T>(j.dump());
  CHECK(again == j);
}

template <class T>
std::string refusal(const json& j) {
  try {
    (void)j.get<T>();
  } catch (const ProtoError& e) {
    return e.what();
  }
  return "";
}

ChrHandle handle(std::uint64_t n) { return ChrHandle{kBig + n}; }

template <std::size_t N>
FixedString<N> fs(const char* text) {
  FixedString<N> s;
  REQUIRE(s.try_assign(text));
  return s;
}

ReadHealth sample_health() {
  ReadHealth h;
  h.of(Group::Session) = {GroupState::Ok, true, 0};
  h.of(Group::World) = {GroupState::Ok, true, 2};
  h.of(Group::Camera) = {GroupState::Tripped, false, 255};
  h.of(Group::Status) = {GroupState::Untested, false, 0};
  h.task_us_last = 187;
  h.task_us_p99 = 412;
  return h;
}

Player sample_player() {
  Player p;
  p.handle = handle(1);
  p.level = 713;
  p.archetype = 9;
  p.body = BodyType::B;
  p.hp = {1234, 1900};
  p.fp = {80, 150};
  p.stamina = {101, 140};
  p.runes_held = 4'000'000'000u;
  p.deaths = 77;
  p.play_time_ms = kBig;
  p.ng_lvl = 7;
  p.save_slot = 9;
  p.dead = true;
  p.riding = true;
  p.locked_on = handle(2);
  p.last_hit_by = handle(3);
  p.throw_state = 4;
  p.roundtable = true;
  p.statuses = bit_of(Status::Frostbite) | bit_of(Status::DeathBlight);
  p.block = BlockId::of(97, 44, 41, 3);
  p.pos = {-1234.5f, 0.25f, 9876.125f};
  p.gear = {11, 22, 33, 44, 55, 66};
  p.boss_flag = true;
  return p;
}

Chr sample_chr(std::uint32_t n) {
  Chr c;
  c.handle = handle(100 + n);
  c.param = 4000 + n;
  c.model = 3000 + n;
  c.entity = 1'000'000 + n;
  c.kind = static_cast<std::uint8_t>(n % 7);
  c.team = static_cast<std::uint8_t>(n % 50);
  c.hp = {n * 10, n * 10 + 5};
  c.dead = n % 2 == 1;
  if (n % 3 == 0) c.last_hit_by = handle(n);
  c.recv_dmg_type = -static_cast<std::int32_t>(n);
  c.throw_state = static_cast<std::uint8_t>(n % 6);
  c.dist_m = 0.5f * static_cast<float>(n);
  c.bearing_deg = -179.75f + static_cast<float>(n);
  c.on_screen = n % 2 == 0;
  c.statuses = n % 4 == 0 ? bit_of(Status::Poison) : StatusSet::None;
  if (n % 5 == 0) c.asleep = n % 10 == 0;
  c.block = BlockId::of(96, static_cast<std::uint8_t>(n), 0, 0);
  c.pos = {static_cast<float>(n), -static_cast<float>(n), 1.5f};
  return c;
}

WorldSnapshot sample_snapshot() {
  WorldSnapshot s;
  s.seq = kBig;
  s.mono_ms = kBig + 1;
  s.gap = true;
  s.build = BuildKey{3};
  s.health = sample_health();
  s.session = {MultiplayRole::Host, true, LobbyState::None, 1, true, true};
  s.life = {true, Screen::InGame, 4'000'000'000u};
  s.ui = {HudState::PopupMenu, std::uint16_t{65535}, 4'200'000'000u, true, true};
  s.player = sample_player();
  s.place = Place{123'456u, fs<32>("invented_vale"), true, true};
  s.clock = Clock{23, 59};
  s.boss.active = true;
  for (std::int32_t i = 0; i < 3; ++i) REQUIRE(s.boss.bars.try_push_back(BossBar{handle(50 + i), 900 + i, -i}));
  for (std::uint32_t n = 0; n < kMaxSnapshotChars; ++n) REQUIRE(s.chars.try_push_back(sample_chr(n)));
  FlagFrame f;
  f.watch_version = 42;
  f.bits.set(0).set(7).set(8).set(739).set(kMaxWatchFlags - 1);
  f.valid.set();
  f.valid.reset(5);
  s.flags = f;
  for (std::uint32_t n = 0; n < kMaxNewText; ++n) {
    TextEntry t{static_cast<std::uint16_t>(900 + n), 7000 + n, std::nullopt};
    if (n % 2 == 0) t.text = fs<256>("fmg-placeholder text, \xC3\xA9 and \xE2\x82\xAC");
    REQUIRE(s.new_text.try_push_back(t));
  }
  for (std::uint32_t n = 0; n < kMaxNewParams; ++n) {
    ParamRowInfo row;
    row.param = n % 2 == 0 ? ParamKind::BonfireWarpParam : ParamKind::WorldMapPlaceNameParam;
    row.id = 100 + n;
    for (std::uint8_t k = 0; k < 8; ++k) {
      REQUIRE(row.fields.try_push_back(ParamField{k, k == 7 ? std::numeric_limits<std::int64_t>::min() : -k * 1000}));
    }
    REQUIRE(s.new_params.try_push_back(row));
  }
  return s;
}

}  // namespace

TEST_CASE("the snapshot is fixed-capacity and trivially copyable") {
  static_assert(std::is_trivially_copyable_v<WorldSnapshot>);
  static_assert(std::is_trivially_copyable_v<RenderFeed>);
  static_assert(std::is_trivially_copyable_v<Lifecycle>);
  CHECK(sizeof(WorldSnapshot) < 32 * 1024);
}

TEST_CASE("every snapshot type survives a round trip, every field away from its default") {
  round_trip(ChrHandle{kBig});
  round_trip(BlockId::of(98, 7, 6, 5));
  round_trip(BuildKey{65535});
  round_trip(sample_health());
  round_trip(Capabilities{bit_of(Group::Session) | bit_of(Group::Text) | bit_of(Group::Status)});
  round_trip(Meter{1, 2});
  round_trip(Session{MultiplayRole::Invader, true, LobbyState::Joined, 4, true, false});
  round_trip(Life{true, Screen::Title, std::nullopt});
  round_trip(sample_player());
  round_trip(Place{std::nullopt, RegionKey{}, false, true});
  round_trip(sample_chr(9));
  round_trip(sample_snapshot());
  round_trip(WorldSnapshot{});  // every optional empty, every container empty
}

TEST_CASE("the JSON form names every field, and writes the enums and sets by name") {
  const json j = sample_snapshot();
  CHECK(j["seq"].get<std::uint64_t>() == kBig);
  CHECK(j["session"]["role"] == "Host");
  CHECK(j["life"]["screen"] == "InGame");
  CHECK(j["ui"]["hud"] == "PopupMenu");
  CHECK(j["player"]["statuses"] == json::array({"Frostbite", "DeathBlight"}));
  CHECK(j["player"]["block"].get<std::uint32_t>() == 0x612C2903u);
  CHECK(j["health"]["groups"]["camera"]["state"] == "Tripped");
  CHECK(j["health"]["groups"].size() == kGroupCount);
  CHECK(j["new_params"][0]["fields"][7][1].get<std::int64_t>() == std::numeric_limits<std::int64_t>::min());
  CHECK(j["flags"]["bits"].get<std::string>().size() == kMaxWatchFlags / 4);  // the last bit is set
  const json empty = WorldSnapshot{};
  CHECK(empty["player"].is_null());
  CHECK(empty["chars"] == json::array());
}

TEST_CASE("a flag frame's bits are hex, bit i in byte i / 8, trailing zero bytes left out") {
  FlagFrame f;
  f.bits.set(0).set(9).set(15);
  f.valid.set(4);
  const json j = f;
  CHECK(j["bits"] == "0182");
  CHECK(j["valid"] == "10");
  CHECK(json(FlagFrame{})["bits"] == "");
  round_trip(f);
  json upper = j;
  upper["bits"] = "0182000000";  // zero bytes and upper case are read too
  CHECK(upper.get<FlagFrame>() == f);
  upper["bits"] = "0ABC";  // 0x0A in byte 0: bits 1 and 3; 0xBC in byte 1: bits 10, 11, 12, 13 and 15
  const FlagFrame read_back = upper.get<FlagFrame>();
  CHECK(read_back.bits.count() == 7);
  CHECK((read_back.bits.test(1) && read_back.bits.test(3) && read_back.bits.test(13) && read_back.bits.test(15)));
}

TEST_CASE("a float that is not finite is written as null, and null reads back as NaN") {
  Chr c = sample_chr(1);
  c.bearing_deg = std::numeric_limits<float>::quiet_NaN();
  c.dist_m = std::numeric_limits<float>::infinity();
  const json j = c;
  CHECK(j["bearing_deg"].is_null());
  CHECK(j["dist_m"].is_null());
  const Chr back = j.get<Chr>();
  CHECK(std::isnan(back.bearing_deg));
  CHECK(std::isnan(back.dist_m));
  round_trip_form(c);
  RenderFeed feed;
  feed.view[5] = std::numeric_limits<float>::quiet_NaN();
  round_trip_form(feed);
}

TEST_CASE("lifecycle messages survive a round trip, each kind") {
  round_trip(Lifecycle{Lifecycle::DataReady{BuildKey{2}, Capabilities{~GroupSet::None}}});
  Lifecycle::PlaythroughReady ready;
  ready.slot = 9;
  ready.name = fs<48>("Invented T\xC3\xA4rnished \xF0\x9F\x8C\x99");
  ready.archetype = 3;
  ready.keepsake = 8;
  ready.play_time_ms = kBig;
  round_trip(Lifecycle{ready});
  round_trip(Lifecycle{Lifecycle::GroupTripped{Group::Status}});
  const json j = Lifecycle{Lifecycle::GroupTripped{Group::Camera}};
  CHECK(j == json{{"group_tripped", {{"group", "camera"}}}});
}

TEST_CASE("commands survive a round trip, each kind") {
  GameCommand::SetWatchFlags watch;
  watch.version = 7;
  round_trip(GameCommand{watch});  // a null list
  std::vector<std::uint32_t> ids(kMaxWatchFlags);
  for (std::uint32_t i = 0; i < ids.size(); ++i) ids[i] = 1'000'000'000u + i;
  watch.ids = std::make_shared<const std::vector<std::uint32_t>>(ids);
  round_trip(GameCommand{watch});
  GameCommand::SetTracked tracked;
  for (std::uint64_t i = 0; i < 8; ++i) REQUIRE(tracked.handles.try_push_back(handle(i)));
  round_trip(GameCommand{tracked});
  round_trip(GameCommand{GameCommand::SetTracked{}});  // empty: the feed stops
  GameCommand::ResolveText text;
  text.fmg = 977;
  for (std::uint32_t i = 0; i < 8; ++i) REQUIRE(text.ids.try_push_back(5000 + i));
  round_trip(GameCommand{text});
  GameCommand::ResolveParams params;
  params.param = ParamKind::NpcParam;
  for (std::uint32_t i = 0; i < 16; ++i) REQUIRE(params.ids.try_push_back(30'000 + i));
  round_trip(GameCommand{params});
  round_trip(GameCommand{GameCommand::Noop{}});
  CHECK(json(GameCommand{GameCommand::Noop{}}) == json{{"noop", json::object()}});
}

TEST_CASE("the render feed survives a round trip") {
  RenderFeed feed;
  feed.seq = kBig;
  feed.qpc_us = kBig + 5;
  for (std::size_t i = 0; i < 16; ++i) feed.view[i] = static_cast<float>(i) * 0.125f - 1.0f;
  feed.fov_rad = 1.0471976f;
  feed.aspect = 1.7777778f;
  feed.camera_ok = true;
  for (std::uint64_t i = 0; i < 8; ++i) {
    REQUIRE(feed.heads.try_push_back(Head{handle(i), {1.0f * static_cast<float>(i), 2.0f, -3.5f}, i % 2 == 0}));
  }
  round_trip(feed);
}

TEST_CASE("reading refuses what is not the form, with the path and the problem and never a value") {
  CHECK(refusal<WorldSnapshot>(json::array()) == "WorldSnapshot: must be an object");
  CHECK(refusal<WorldSnapshot>(json{{"sequence", 1}}) == "WorldSnapshot: has an unknown field \"sequence\"");
  CHECK(refusal<WorldSnapshot>(json{{"build", 70000}}) == "WorldSnapshot.build: is out of range");
  CHECK(refusal<WorldSnapshot>(json{{"seq", -1}}) == "WorldSnapshot.seq: must be a whole number of 0 or more");
  CHECK(refusal<WorldSnapshot>(json{{"life", {{"screen", "Menu"}}}}) ==
        "WorldSnapshot.life.screen: must be one of Unknown, InGame, Loading, Title");
  CHECK(refusal<WorldSnapshot>(json{{"health", {{"groups", {{"weather", json::object()}}}}}}) ==
        "WorldSnapshot.health.groups: has an unknown group \"weather\"");
  CHECK(refusal<WorldSnapshot>(json{{"health", {{"groups", {{"ui", {{"faults", 256}}}}}}}}) ==
        "WorldSnapshot.health.groups.ui.faults: is out of range");
  json chars = json::array();
  for (int i = 0; i < 33; ++i) chars.push_back(json::object());
  CHECK(refusal<WorldSnapshot>(json{{"chars", chars}}) == "WorldSnapshot.chars: has more than 32 items");
  CHECK(refusal<WorldSnapshot>(json{{"chars", {{{"pos", {1, 2}}}}}}) ==
        "WorldSnapshot.chars[0].pos: must be an array of 3 numbers");
  CHECK(refusal<WorldSnapshot>(json{{"chars", {{{"dist_m", 1e39}}}}}) == "WorldSnapshot.chars[0].dist_m: is out of range");
  CHECK(refusal<WorldSnapshot>(json{{"chars", {{{"statuses", {"Poison", "Poison"}}}}}}) ==
        "WorldSnapshot.chars[0].statuses[1]: names a member twice");
  CHECK(refusal<WorldSnapshot>(json{{"chars", {{{"team", 256}}}}}) == "WorldSnapshot.chars[0].team: is out of range");
  CHECK(refusal<WorldSnapshot>(json{{"chars", {{{"recv_dmg_type", 1.5}}}}}) ==
        "WorldSnapshot.chars[0].recv_dmg_type: must be a whole number");
  CHECK(refusal<WorldSnapshot>(json{{"place", {{"region", "Upper Case"}}}}) ==
        "WorldSnapshot.place.region: must hold lower-case letters, digits and _ only");
  CHECK(refusal<WorldSnapshot>(json{{"place", {{"region", std::string(33, 'a')}}}}) ==
        "WorldSnapshot.place.region: is longer than 32 bytes");
  CHECK(refusal<WorldSnapshot>(json{{"clock", {{"h", 24}}}}) == "WorldSnapshot.clock.h: is out of range");
  CHECK(refusal<WorldSnapshot>(json{{"flags", {{"bits", "abc"}}}}) ==
        "WorldSnapshot.flags.bits: must be an even number of hex digits, at most 512");
  CHECK(refusal<WorldSnapshot>(json{{"flags", {{"valid", "zz"}}}}) == "WorldSnapshot.flags.valid: must hold hex digits only");
  CHECK(refusal<WorldSnapshot>(json{{"new_params", {{{"fields", {{1}}}}}}}) ==
        "WorldSnapshot.new_params[0].fields[0]: must be an array of a field's index and its value");
  CHECK(refusal<WorldSnapshot>(json{{"new_text", {{{"text", std::string("a\0b", 3)}}}}}) ==
        "WorldSnapshot.new_text[0].text: holds a NUL");
  CHECK(refusal<Lifecycle>(json::object()) ==
        "Lifecycle: needs exactly one of \"data_ready\", \"playthrough_ready\" and \"group_tripped\"");
  CHECK(refusal<Lifecycle>(json{{"group_tripped", {{"group", "weather"}}}}).rfind(
            "Lifecycle.group_tripped.group: must be one of session, world, ", 0) == 0);
  CHECK(refusal<Lifecycle>(json{{"playthrough_ready", {{"name", std::string(49, 'n')}}}}) ==
        "Lifecycle.playthrough_ready.name: is longer than 48 bytes");
  CHECK(refusal<GameCommand>(json{{"noop", {{"x", 1}}}}) == "GameCommand.noop: has an unknown field \"x\"");
  CHECK(refusal<GameCommand>(json{{"noop", json::object()}, {"set_tracked", json::object()}}).rfind(
            "GameCommand: needs exactly one of", 0) == 0);
  CHECK(refusal<GameCommand>(json{{"set_watch_flags", {{"ids", std::vector<int>(kMaxWatchFlags + 1, 1)}}}}) ==
        "GameCommand.set_watch_flags.ids: has more than 2048 ids");
  CHECK(refusal<GameCommand>(json{{"resolve_text", {{"ids", std::vector<int>(9, 1)}}}}) ==
        "GameCommand.resolve_text.ids: has more than 8 items");
  CHECK(refusal<GameCommand>(json{{"resolve_params", {{"param", "ItemParam"}}}}).rfind(
            "GameCommand.resolve_params.param: must be one of NpcParam, ", 0) == 0);
  CHECK(refusal<RenderFeed>(json{{"heads", {{{"present", 1}}}}}) == "RenderFeed.heads[0].present: must be true or false");
}

TEST_CASE("fields left out keep their defaults") {
  const auto s = read_json<WorldSnapshot>(R"({"seq": 5, "player": {"level": 3}})");
  CHECK(s.seq == 5);
  REQUIRE(s.player.has_value());
  CHECK(s.player->level == 3);
  CHECK(s.player->body == BodyType::Unknown);
  CHECK(s.life.screen == Screen::Unknown);
  CHECK_FALSE(s.place.has_value());
}
