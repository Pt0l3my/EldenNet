// FakeWorld (fake_world.hpp): an invented game for the in-game part's tests. Every name, offset and value is made up.
#include "fake_world.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

namespace en::gal::test {

namespace {

using P = FakeProgram;

template <class F>
void function(P& p, const std::string& name, F&& body) {
  p.align(P::Text, 16, 0xCC);
  p.label(name, P::Text);
  body();
  p.label(name + "_end", P::Text);
  p.function(name, name + "_end");
}

// An FD4-style accessor: push rdi; sub rsp, 0x20; mov rdi, [rip+static]; test rdi, rdi; jne +0x20;
// lea rcx, [rip+class]; call getter; mov edx, site; mov r8, rax; lea rcx, [rip+path]; call log; mov rax, rdi;
// add rsp, 0x20; pop rdi; ret.
void accessor(P& p, const std::string& name, const std::string& statik, const std::string& klass, std::uint32_t site) {
  function(p, name, [&] {
    p.bytes(P::Text, {0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x3D});
    p.rel32(P::Text, statik);
    p.bytes(P::Text, {0x48, 0x85, 0xFF, 0x75, 0x20, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, klass);
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "getter");
    p.bytes(P::Text, {0xBA});
    p.u32(P::Text, site);
    p.bytes(P::Text, {0x49, 0x89, 0xC0, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "log");
    p.bytes(P::Text, {0x48, 0x89, 0xF8, 0x48, 0x83, 0xC4, 0x20, 0x5F, 0xC3});
  });
}

void statik(P& p, const std::string& label) {
  p.align(P::Data, 8);
  p.label(label, P::Data);
  p.u64(P::Data, 0);
}

// MSVC RTTI for one class with one vtable (as fake_program's sample writes it).
void rtti_class(P& p, const std::string& id, const std::string& decorated, const std::vector<std::string>& bases,
                const std::vector<std::string>& slots) {
  p.align(P::Data, 8);
  p.label("td_" + id, P::Data);
  p.va64(P::Data, "type_info_vft");
  p.u64(P::Data, 0);
  p.ascii(P::Data, decorated);
  p.align(P::Rdata, 8);
  p.label("bcd_" + id, P::Rdata);
  p.rva32(P::Rdata, "td_" + id);
  p.u32(P::Rdata, static_cast<std::uint32_t>(bases.size()));
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, 0xFFFFFFFF);
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, 0x40);
  p.rva32(P::Rdata, "chd_" + id);
  p.align(P::Rdata, 8);
  p.label("chd_" + id, P::Rdata);
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, static_cast<std::uint32_t>(1 + bases.size()));
  p.rva32(P::Rdata, "bca_" + id);
  p.label("bca_" + id, P::Rdata);
  p.rva32(P::Rdata, "bcd_" + id);
  for (const std::string& b : bases) p.rva32(P::Rdata, "bcd_" + b);
  const std::string col = "col_" + id;
  p.align(P::Rdata, 8);
  p.label(col, P::Rdata);
  p.u32(P::Rdata, 1);
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, 0);
  p.rva32(P::Rdata, "td_" + id);
  p.rva32(P::Rdata, "chd_" + id);
  p.rva32(P::Rdata, col);
  p.align(P::Rdata, 8);
  p.va64(P::Rdata, col);
  p.label("vt_" + id + "_0", P::Rdata);
  for (const std::string& s : slots) p.va64(P::Rdata, s);
  p.u64(P::Rdata, 0);
}

struct Single {
  const char* item;  // the item's label, and its static's: st_<item>
  const char* name;  // the name its class object gives
  const char* id;    // its RTTI class's id here
  const char* rtti;  // W::<Class>
};
constexpr Single kSingletons[] = {
    {"task_hub", "Hub", "hub", "W::TaskHub"},       {"characters", "Roster", "roster", "W::Roster"},
    {"session", "Lobby", "lobby", "W::Lobby"},      {"event_flags", "Marks", "marks", "W::Marks"},
    {"hud", "Hud", "hud", "W::Hud"},                {"menus", "Menus", "menus", "W::Menus"},
    {"loading", "Veil", "veil", "W::Veil"},         {"world_clock", "Clock", "clock", "W::Clock"},
    {"camera", "Eye", "eye", "W::Eye"},             {"param_store", "Tables", "tables", "W::Tables"},
    {"messages", "Words", "words", "W::Words"},
};
constexpr Single kStatics[] = {
    {"save_data", "", "save", "W::Save"}, {"game_state", "", "game", "W::Game"}, {"field_area", "", "field", "W::Field"}};

std::string decorated(const char* rtti) {  // "W::Class" -> ".?AVClass@W@@"
  const std::string s(rtti);
  return ".?AV" + s.substr(3) + "@W@@";
}

constexpr const char* kProvenance =
    R"(provenance = { method = "own-derivation", leads = ["an invented lead"], evidence = "docs/ops/probes/w.json", )"
    R"(date = "2026-11-16" })";

// The layout: every offset invented for this world.
constexpr const char* kLayout = R"(schema = 1
build = "9.8.7.0-ww"
derived_from = "probe"
probe_record = "docs/ops/probes/9.8.7.0-ww.json"

[task]
group = { name = "W_Settle", index = 2 }
register_abi = { task = "r8", instance = "rcx", group = "rdx" }
vtable_slots = 3
run_slot = 1
object_size = 0x10
status = "confirmed"

[class.task_hub]
rtti = "W::TaskHub"
singleton = "Hub"
copy_size = 0x40
status = "confirmed"
fields = { group_count = { off = 0x10, ty = "u32", group = "boot" }, groups = { off = 0x18, ty = "ptr", group = "boot" } }

[class.task_group]
stride = 0x50
status = "confirmed"
fields = { name = { off = 0x8, ty = "utf16[32]", group = "boot" } }

[class.characters]
rtti = "W::Roster"
singleton = "Roster"
copy_size = 0x60
status = "confirmed"
[class.characters.fields]
main_player = { off = 0x10, ty = "ptr", group = "world" }
by_distance = { off = 0x20, ty = "dlvector", group = "world" }
player_set = { off = 0x38, ty = "ptr", group = "session" }
player_set_len = { off = 0x40, ty = "u32", group = "session" }

[class.near_entry]
stride = 0x10
status = "confirmed"
fields = { chr = { off = 0x0, ty = "ptr", group = "world" }, distance = { off = 0x8, ty = "f32", group = "world" } }

[class.player_slot]
stride = 0x8
status = "confirmed"
fields = { chr = { off = 0x0, ty = "ptr", group = "session" } }

[class.character]
rtti = "W::Chr"
copy_size = 0x80
status = "confirmed"
[class.character.fields]
handle = { off = 0x8, ty = "handle", group = "world" }
npc_param_id = { off = 0x10, ty = "u32", group = "world" }
model = { off = 0x14, ty = "u32", group = "world" }
entity_id = { off = 0x18, ty = "u32", group = "world" }
chr_type = { off = 0x1C, ty = "i32", group = "world" }
team = { off = 0x20, ty = "u8", group = "world" }
on_screen = { off = 0x21, ty = "bit", bit = 1, group = "world" }
dead = { off = 0x21, ty = "bit", bit = 5, group = "world" }
block_id = { off = 0x24, ty = "u32", group = "world" }
last_hit_by = { off = 0x28, ty = "handle", group = "combat" }
parts = { off = 0x30, ty = "ptr", group = "world" }

[class.player_character]
rtti = "W::Hero"
copy_size = 0x98
status = "confirmed"
[class.player_character.fields]
locked_on = { off = 0x80, ty = "handle", group = "combat" }
ride_state = { off = 0x88, ty = "u8", group = "player", enum = "ride" }

[class.character_parts]
copy_size = 0x28
status = "confirmed"
[class.character_parts.fields]
vitals = { off = 0x0, ty = "ptr", group = "world" }
motion = { off = 0x8, ty = "ptr", group = "world" }
reaction = { off = 0x10, ty = "ptr", group = "combat" }
grip = { off = 0x18, ty = "ptr", group = "combat" }

[class.vitals]
copy_size = 0x20
status = "confirmed"
[class.vitals.fields]
hp = { off = 0x0, ty = "i32", group = "world" }
max_hp = { off = 0x4, ty = "i32", group = "world" }
fp = { off = 0x8, ty = "i32", group = "world" }
max_fp = { off = 0xC, ty = "i32", group = "world" }
stamina = { off = 0x10, ty = "i32", group = "world" }
max_stamina = { off = 0x14, ty = "i32", group = "world" }

[class.motion]
copy_size = 0x20
status = "confirmed"
fields = { pos = { off = 0x10, ty = "vec3f", group = "world" } }

[class.reaction]
copy_size = 0x8
status = "confirmed"
fields = { recv_dmg_type = { off = 0x4, ty = "i32", group = "combat" } }

[class.grip]
copy_size = 0x4
status = "confirmed"
fields = { throw_state = { off = 0x2, ty = "u8", group = "combat" } }

[class.save_data]
rtti = "W::Save"
copy_size = 0x40
status = "confirmed"
[class.save_data.fields]
play_time = { off = 0x10, ty = "u32", group = "player" }
death_count = { off = 0x14, ty = "u32", group = "player" }
ng_level = { off = 0x18, ty = "u32", group = "player", range = [0, 7] }
boss_fight = { off = 0x1C, ty = "u8", group = "player" }
player_data = { off = 0x20, ty = "ptr", group = "player" }
settings = { off = 0x28, ty = "ptr", group = "ui" }

[class.settings]
copy_size = 0x10
status = "confirmed"
fields = { show_subtitles = { off = 0x3, ty = "u8", group = "ui" } }

[class.player_data]
copy_size = 0x80
status = "confirmed"
[class.player_data.fields]
level = { off = 0x8, ty = "u32", group = "player", range = [1, 713] }
runes = { off = 0xC, ty = "u32", group = "player" }
archetype = { off = 0x10, ty = "u8", group = "player" }
body = { off = 0x11, ty = "u8", group = "player", enum = "body" }
keepsake = { off = 0x12, ty = "u8", group = "player" }
multiplay_role = { off = 0x13, ty = "u8", group = "session", enum = "multiplay_role" }
is_my_world = { off = 0x14, ty = "u8", group = "session", range = [0, 1] }
name = { off = 0x20, ty = "utf16[17]", group = "player" }
gear = { off = 0x48, ty = "array", elem = "item_id", len = 6, stride = 4, group = "player" }

[class.item_id]
stride = 0x4
status = "confirmed"
fields = { id = { off = 0x0, ty = "u32", group = "player" } }

[class.game_state]
rtti = "W::Game"
copy_size = 0x20
status = "confirmed"
fields = { save_slot = { off = 0x8, ty = "i32", group = "player", range = [0, 9] }, talk_cam_id = { off = 0xC, ty = "i32", group = "ui" } }

[class.field_area]
rtti = "W::Field"
copy_size = 0x20
status = "confirmed"
fields = { place_id = { off = 0x10, ty = "i32", group = "place" } }

[class.session]
rtti = "W::Lobby"
singleton = "Lobby"
copy_size = 0x20
status = "confirmed"
fields = { lobby_state = { off = 0x8, ty = "i32", group = "session", enum = "lobby" } }

[class.hud]
rtti = "W::Hud"
singleton = "Hud"
copy_size = 0x50
status = "confirmed"
[class.hud.fields]
hud_state = { off = 0x8, ty = "i32", group = "ui", range = [0, 3], enum = "hud" }
banner_id = { off = 0xC, ty = "i32", group = "ui" }
boss_bars = { off = 0x10, ty = "array", elem = "boss_bar", len = 3, stride = 0x10, group = "ui" }

[class.boss_bar]
stride = 0x10
status = "confirmed"
fields = { handle = { off = 0x0, ty = "handle", group = "ui" }, name_id = { off = 0x8, ty = "i32", group = "ui" }, damage_taken = { off = 0xC, ty = "i32", group = "ui" } }

[class.menus]
rtti = "W::Menus"
singleton = "Menus"
copy_size = 0x20
status = "confirmed"
fields = { screen_state = { off = 0x8, ty = "i32", group = "ui", enum = "screen" }, popup = { off = 0x10, ty = "ptr", group = "ui" } }

[class.popup]
copy_size = 0x8
status = "confirmed"
fields = { talk_id = { off = 0x4, ty = "i32", group = "ui" } }

[class.loading]
rtti = "W::Veil"
singleton = "Veil"
copy_size = 0x10
status = "confirmed"
fields = { load_counter = { off = 0x8, ty = "u32", group = "ui" } }

[class.world_clock]
rtti = "W::Clock"
singleton = "Clock"
copy_size = 0x20
status = "confirmed"
fields = { hour = { off = 0x10, ty = "u8", group = "place" }, minute = { off = 0x11, ty = "u8", group = "place" } }

[class.camera]
rtti = "W::Eye"
singleton = "Eye"
copy_size = 0x10
status = "confirmed"
fields = { view = { off = 0x8, ty = "ptr", group = "camera" } }

[class.view]
copy_size = 0x50
status = "confirmed"
fields = { matrix = { off = 0x0, ty = "mat4f", group = "camera" }, fov = { off = 0x40, ty = "f32", group = "camera" }, aspect = { off = 0x44, ty = "f32", group = "camera" } }

[class.event_flags]
rtti = "W::Marks"
singleton = "Marks"
copy_size = 0x30
status = "confirmed"
fields = { divisor = { off = 0x8, ty = "u32", group = "flags" }, map = { off = 0x10, ty = "dlmap", group = "flags" }, holder_base = { off = 0x20, ty = "ptr", group = "flags" } }

[class.flag_node]
copy_size = 0x30
status = "confirmed"
[class.flag_node.fields]
left = { off = 0x0, ty = "ptr", group = "flags" }
parent = { off = 0x8, ty = "ptr", group = "flags" }
right = { off = 0x10, ty = "ptr", group = "flags" }
key = { off = 0x18, ty = "u32", group = "flags" }
mode = { off = 0x1C, ty = "u32", group = "flags" }
value = { off = 0x20, ty = "u64", group = "flags" }

[class.messages]
rtti = "W::Words"
singleton = "Words"
copy_size = 0x20
status = "confirmed"
fields = { tables = { off = 0x8, ty = "ptr", group = "text" }, count = { off = 0x10, ty = "i32", group = "text" } }

[class.fmg]
copy_size = 0x20
status = "confirmed"
fields = { group_count = { off = 0x4, ty = "u32", group = "text" }, groups_off = { off = 0x8, ty = "u64", group = "text" }, offsets_off = { off = 0x10, ty = "u64", group = "text" } }

[class.fmg_group]
stride = 0xC
status = "confirmed"
fields = { offset_index = { off = 0x0, ty = "i32", group = "text" }, first_id = { off = 0x4, ty = "i32", group = "text" }, last_id = { off = 0x8, ty = "i32", group = "text" } }

[class.param_store]
rtti = "W::Tables"
singleton = "Tables"
copy_size = 0x20
status = "confirmed"
fields = { tables = { off = 0x8, ty = "ptr", group = "params" }, count = { off = 0x10, ty = "u32", group = "params" } }

[class.param_table]
copy_size = 0x18
status = "confirmed"
fields = { rows = { off = 0x0, ty = "ptr", group = "params" }, row_count = { off = 0x8, ty = "u16", group = "params" }, row_size = { off = 0xA, ty = "u16", group = "params" } }

[class.param_row]
stride = 0x10
status = "confirmed"
fields = { id = { off = 0x0, ty = "u32", group = "params" }, data = { off = 0x8, ty = "ptr", group = "params" } }

[enum.lobby]
values = { 0 = "None", 1 = "Hosting", 2 = "Joined" }
[enum.multiplay_role]
values = { 1 = "Host", 2 = "Cooperator", 3 = "Invader" }
[enum.screen]
values = { 0 = "InGame", 1 = "Loading", 2 = "Title" }
[enum.hud]
values = { 0 = "HideAll", 1 = "ShowAll", 2 = "PopupMenu", 3 = "Default" }
[enum.body]
values = { 0 = "A", 1 = "B" }
[enum.ride]
values = { 0 = "OnFoot", 7 = "Riding" }

[param.NpcParam]
slot = 3
row_size = 0x10
status = "confirmed"
fields = { name_id = { off = 0x0, ty = "i32" }, team = { off = 0x4, ty = "u8" } }

[fmg]
indexing = "flat"
group_stride = 12
npc_names = [5, 1]
place_names = [6]
status = "confirmed"

[dlrf]
name_ptr_off = 0x10
status = "confirmed"

[selftest]
npc_name_ids = [7001]
place_name_ids = [8001]
npc_param_ids = [4100]
)";

// Object sizes, as the layout gives them.
constexpr std::size_t kChr = 0x80, kHero = 0x98;

std::uint64_t handle_of(std::uint32_t index, std::uint32_t generation) {
  return (std::uint64_t{0xFB} << 56) | (std::uint64_t{generation} << 24) | index;
}

}  // namespace

FakeWorld::FakeWorld(const WorldOptions& options) {
  build_image(options);
  base = program.image_base();
  mem.map(base, built.memory);
  sys = std::make_unique<FakeSys>(mem, base, built.size_of_image);
  sys->getter = [this](std::uint64_t, std::uint64_t cls) -> std::optional<std::uint64_t> {
    return mem.get<std::uint64_t>(cls + 0x10);  // the getter's own code: mov rax, [rcx+0x10]
  };
  build_heap(options);
}

void FakeWorld::build_image(const WorldOptions& options) {
  P& p = program;
  // ---- .rdata ----
  p.label("path", P::Rdata);
  p.ascii(P::Rdata, "w\\core\\One.h");
  for (const Single& s : kSingletons) {
    p.label(std::string("name_") + s.name, P::Rdata);
    p.ascii(P::Rdata, s.name);
  }
  p.align(P::Rdata, 8);
  p.label("class_table", P::Rdata);
  p.va64(P::Rdata, "getter");
  p.label("type_info_vft", P::Rdata);
  p.va64(P::Rdata, "noop");
  p.u64(P::Rdata, 0);
  // ---- .data ----
  for (const Single& s : kSingletons) statik(p, std::string("st_") + s.item);
  for (const Single& s : kStatics) statik(p, std::string("st_") + s.item);
  statik(p, "st_module_handle");
  for (const Single& s : kSingletons) {
    p.align(P::Data, 16);
    p.label(std::string("cls_") + s.name, P::Data);
    p.va64(P::Data, "class_table");
    p.u64(P::Data, 0x40);
    p.va64(P::Data, std::string("name_") + s.name);
    p.u64(P::Data, 0);
  }
  // ---- .text ----
  function(p, "noop", [&] { p.bytes(P::Text, {0xC3}); });
  function(p, "getter", [&] { p.bytes(P::Text, {0x48, 0x8B, 0x41, 0x10, 0xC3}); });
  function(p, "log", [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });
  std::uint32_t site = 0x2001;
  for (const Single& s : kSingletons) {
    accessor(p, std::string("get_") + s.item, std::string("st_") + s.item, std::string("cls_") + s.name, site++);
  }
  // The registration, with this world's own registers: the instance in rcx, the group in edx, the task in r8.
  function(p, "register_task", [&] {
    p.bytes(P::Text, {0x53, 0x56, 0x48, 0x83, 0xEC, 0x28});                    // push rbx; push rsi; sub rsp, 0x28
    p.bytes(P::Text, {0x48, 0x89, 0xCB, 0x89, 0xD6, 0x41, 0x89, 0x70, 0x08});  // mov rbx, rcx; mov esi, edx; mov [r8+8], esi
    p.bytes(P::Text, {0x49, 0x89, 0x58, 0x10, 0x31, 0xC0});                    // mov [r8+0x10], rbx; xor eax, eax
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0x5E, 0x5B, 0xC3});              // add rsp, 0x28; pop rsi; pop rbx; ret
  });
  // Readers of the AOB statics, each of a shape of its own: mov rax, [rip+static]; test rax, rax; je +3;
  // mov eax, [rax+disp8].
  const std::pair<const char*, std::uint8_t> readers[] = {{"save_data", 0x14}, {"game_state", 0x08}, {"field_area", 0x10}};
  for (const auto& [item, disp] : readers) {
    function(p, std::string("read_") + item, [&] {
      p.bytes(P::Text, {0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05});
      p.rel32(P::Text, std::string("st_") + item);
      p.bytes(P::Text, {0x48, 0x85, 0xC0, 0x74, 0x03, 0x8B, 0x40, disp});
      p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3});
    });
  }
  function(p, "publish_module", [&] {
    p.bytes(P::Text, {0x56, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x89, 0x0D});  // push rsi; sub rsp, 0x20; mov [rip+m], rcx
    p.rel32(P::Text, "st_module_handle");
    p.bytes(P::Text, {0x48, 0x31, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5E, 0xC3});
  });
  // One slot function per class, so every vtable is a run of pointers into code.
  for (const char* f : {"f_a", "f_b", "f_c", "f_d", "f_e"}) function(p, f, [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });
  // ---- RTTI ----
  for (const Single& s : kSingletons) rtti_class(p, s.id, decorated(s.rtti), {}, {"f_a"});
  for (const Single& s : kStatics) rtti_class(p, s.id, decorated(s.rtti), {}, {"f_b"});
  rtti_class(p, "chr", ".?AVChr@W@@", {}, {"f_c", "f_d"});
  rtti_class(p, "hero", ".?AVHero@W@@", {"chr"}, {"f_c", "f_e"});
  rtti_class(p, "npc", ".?AVNpc@W@@", {"chr"}, {"f_c", "f_a"});
  rtti_class(p, "beast", ".?AVBeast@W@@", {"npc", "chr"}, {"f_c", "f_b"});
  rtti_class(p, "prop", ".?AVProp@W@@", {}, {"f_d"});
  rtti_class(p, "tomb", ".?AVTomb@W@@", {}, {"f_e"});
  built = p.build();

  // ---- gal/'s texts ----
  builds = "schema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\napp = \"4.5.6\"\nfile_version = \"9.8.7.0\"\n"
           "product_name = \"INVENTED GAME\"\nregion = \"ww\"\ntime_date_stamp = 0x12345678\nsize_of_image = " +
           std::to_string(built.size_of_image) + "\nlayout = \"layouts/w.toml\"\ndlrf_name_layout = " +
           (options.name_layout ? "true" : "false") + "\nstatus = \"supported\"\n";
  std::string pt = "schema = 1\n";
  pt += "[[pattern]]\nid = \"register_task\"\nkind = \"function\"\nsection = \".text\"\n"
        "bytes = \"48 89 CB 89 D6 41 89 70 08 49 89 58 10 31 C0\"\nanchor_min = 6\nfn_len = [0x10, 0x100]\n"
        "prologue = \"53 56\"\n";
  pt += std::string(kProvenance) + "\n";
  for (const auto& [item, disp] : readers) {
    char hex[3];
    std::snprintf(hex, sizeof hex, "%02X", disp);
    pt += std::string("[[pattern]]\nid = \"") + item + "\"\nkind = \"static\"\nsection = \".text\"\n" +
          "bytes = \"48 8B 05 ?? ?? ?? ?? 48 85 C0 74 03 8B 40 " + hex + "\"\n" +
          "capture = { at = 3, kind = \"rip_rel32\", insn_end = 7 }\ntarget_section = \".data\"\n" + kProvenance + "\n";
  }
  pt += "[[pattern]]\nid = \"module_handle\"\nkind = \"static\"\nsection = \".text\"\n"
        "bytes = \"48 89 0D ?? ?? ?? ?? 48 31 C0\"\ncapture = { at = 3, kind = \"rip_rel32\", insn_end = 7 }\n"
        "target_section = \".data\"\n" +
        std::string(kProvenance) + "\n";
  pt += "[[pattern]]\nid = \"fd4_idiom\"\nkind = \"idiom\"\nsection = \".text\"\n"
        "bytes = \"48 8B 3D ?? ?? ?? ?? 48 85 FF 75 20 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? BA ?? ?? ?? ?? 49 89 C0 48 8D "
        "0D ?? ?? ?? ?? E8 ?? ?? ?? ??\"\nexpect_matches = [1, 64]\npath_suffix = \"One.h\"\n"
        "captures = { static = { at = 3, kind = \"rip_rel32\", insn_end = 7, section = \".data\", align = 8 }, "
        "class = { at = 15, kind = \"rip_rel32\", insn_end = 19, section = \".data\" }, "
        "getter = { at = 20, kind = \"rip_rel32\", insn_end = 24, section = \".text\", function = true }, "
        "path = { at = 35, kind = \"rip_rel32\", insn_end = 39, section = \".rdata\" } }\n" +
        std::string(kProvenance) + "\n";
  patterns = pt;
  layout = kLayout;
}

std::uint64_t FakeWorld::make_character(const std::string& cls, std::uint32_t index, std::int32_t max_hp,
                                        std::int32_t hp, float x, float z) {
  const std::uint64_t c = mem.alloc(cls == "hero" ? kHero : kChr);
  set_vtable(c, cls);
  mem.put<std::uint64_t>(c + 0x8, handle_of(index, generation_));
  mem.put<std::uint32_t>(c + 0x10, index == 0 ? 0 : 4000 + 100 * (index % 3));
  mem.put<std::uint32_t>(c + 0x14, 0x2200 + index);
  mem.put<std::uint32_t>(c + 0x18, 30000 + index);
  mem.put<std::int32_t>(c + 0x1C, index == 0 ? 0 : 2);
  mem.put<std::uint8_t>(c + 0x20, static_cast<std::uint8_t>(index == 0 ? 1 : 12));
  mem.put<std::uint8_t>(c + 0x21, static_cast<std::uint8_t>((1u << 1) | (hp == 0 ? (1u << 5) : 0u)));
  mem.put<std::uint32_t>(c + 0x24, 0x0B0C0000u | index);
  const std::uint64_t parts = mem.alloc(0x28);
  const std::uint64_t vitals = mem.alloc(0x20), motion = mem.alloc(0x20), reaction = mem.alloc(0x8), grip = mem.alloc(0x4);
  mem.put<std::uint64_t>(parts + 0x0, vitals);
  mem.put<std::uint64_t>(parts + 0x8, motion);
  mem.put<std::uint64_t>(parts + 0x10, reaction);
  mem.put<std::uint64_t>(parts + 0x18, grip);
  const std::int32_t v[6] = {hp, max_hp, 70, 80, 100, 110};
  for (int i = 0; i < 6; ++i) mem.put<std::int32_t>(vitals + 4 * static_cast<std::uint64_t>(i), v[i]);
  mem.put<float>(motion + 0x10, x);
  mem.put<float>(motion + 0x14, 5.0f);
  mem.put<float>(motion + 0x18, z);
  mem.put<std::int32_t>(reaction + 0x4, 0);
  mem.put<std::uint8_t>(grip + 0x2, 0);
  mem.put<std::uint64_t>(c + 0x30, parts);
  return c;
}

void FakeWorld::build_heap(const WorldOptions& options) {
  const auto obj = [&](const char* item, std::size_t size, const char* id) {
    const std::uint64_t o = mem.alloc(size);
    set_vtable(o, id);
    mem.put<std::uint64_t>(at(std::string("st_") + item), o);
    return o;
  };
  // The task hub and its group table.
  const std::uint64_t hub = obj("task_hub", 0x40, "hub");
  const std::uint64_t groups = mem.alloc(4 * 0x50);
  const char16_t* names[] = {u"W_Begin", u"W_Think", u"W_Settle", u"W_End"};
  for (std::uint32_t i = 0; i < 4; ++i) mem.put_utf16(groups + 0x50 * i + 0x8, names[i]);
  mem.put<std::uint32_t>(hub + 0x10, 4);
  mem.put<std::uint64_t>(hub + 0x18, groups);
  // The characters' roster, its near list and player set.
  const std::uint64_t roster = obj("characters", 0x60, "roster");
  near = mem.alloc(48 * 0x10);
  mem.put<std::uint64_t>(roster + 0x20, near);
  mem.put<std::uint64_t>(roster + 0x28, near);
  const std::uint64_t players = mem.alloc(4 * 8);
  mem.put<std::uint64_t>(roster + 0x38, players);
  mem.put<std::uint32_t>(roster + 0x40, 4);
  // The session, the HUD, the menus, the loading veil, the clock, the camera.
  obj("session", 0x20, "lobby");
  const std::uint64_t hud = obj("hud", 0x50, "hud");
  mem.put<std::int32_t>(hud + 0x8, 0);
  const std::uint64_t menus = obj("menus", 0x20, "menus");
  mem.put<std::int32_t>(menus + 0x8, 2);  // the title
  const std::uint64_t popup = mem.alloc(0x8);
  mem.put<std::int32_t>(popup + 0x4, -1);
  mem.put<std::uint64_t>(menus + 0x10, popup);
  obj("loading", 0x10, "veil");
  const std::uint64_t clock = obj("world_clock", 0x20, "clock");
  mem.put<std::uint8_t>(clock + 0x10, 9);
  mem.put<std::uint8_t>(clock + 0x11, 30);
  const std::uint64_t eye = obj("camera", 0x10, "eye");
  const std::uint64_t view = mem.alloc(0x50);
  for (int i = 0; i < 4; ++i) mem.put<float>(view + 4 * static_cast<std::uint64_t>(i * 5), 1.0f);
  mem.put<float>(view + 0x40, 0.9f);
  mem.put<float>(view + 0x44, 1.75f);
  mem.put<std::uint64_t>(eye + 0x8, view);
  // The save data, the player's sheet and the settings; the game state; the field area.
  const std::uint64_t save = obj("save_data", 0x40, "save");
  const std::uint64_t sheet = mem.alloc(0x80);
  mem.put<std::uint32_t>(sheet + 0x8, 12);
  mem.put<std::uint32_t>(sheet + 0xC, 4321);
  mem.put<std::uint8_t>(sheet + 0x10, 3);
  mem.put<std::uint8_t>(sheet + 0x11, 1);
  mem.put<std::uint8_t>(sheet + 0x12, 2);
  mem.put<std::uint8_t>(sheet + 0x13, 1);  // Host
  mem.put<std::uint8_t>(sheet + 0x14, 1);
  mem.put_utf16(sheet + 0x20, u"Tess Wayfarer");
  for (std::uint32_t i = 0; i < 6; ++i) mem.put<std::uint32_t>(sheet + 0x48 + 4 * i, 1000000 + 1000 * i);
  const std::uint64_t settings = mem.alloc(0x10);
  mem.put<std::uint8_t>(settings + 0x3, 1);
  mem.put<std::uint64_t>(save + 0x20, sheet);
  mem.put<std::uint64_t>(save + 0x28, settings);
  mem.put<std::uint32_t>(save + 0x14, 3);
  const std::uint64_t game = obj("game_state", 0x20, "game");
  mem.put<std::int32_t>(game + 0x8, 2);
  mem.put<std::int32_t>(game + 0xC, -1);
  const std::uint64_t field = obj("field_area", 0x20, "field");
  mem.put<std::int32_t>(field + 0x10, 6100100);
  mem.put<std::uint64_t>(at("st_module_handle"), base);

  // The flags: a tree of three groups under a sentinel head (head.parent is the root; the leaves point at the head).
  const std::uint64_t marks = obj("event_flags", 0x30, "marks");
  mem.put<std::uint32_t>(marks + 0x8, 1000);
  holder = mem.alloc(1024);
  free_block = mem.alloc(125);
  const std::uint64_t head = mem.alloc(0x30), n0 = mem.alloc(0x30), n1 = mem.alloc(0x30), n2 = mem.alloc(0x30);
  const auto node = [&](std::uint64_t n, std::uint64_t left, std::uint64_t parent, std::uint64_t right, std::uint32_t key,
                        std::uint32_t mode, std::uint64_t value) {
    mem.put<std::uint64_t>(n + 0x0, left);
    mem.put<std::uint64_t>(n + 0x8, parent);
    mem.put<std::uint64_t>(n + 0x10, right);
    mem.put<std::uint32_t>(n + 0x18, key);
    mem.put<std::uint32_t>(n + 0x1C, mode);
    mem.put<std::uint64_t>(n + 0x20, value);
  };
  node(head, n0, n1, n2, 0, 0, 0);
  node(n1, n0, head, n2, 1234, 1, 2);         // the root: group 1234, its block the holder's third (holder + 250)
  node(n0, head, n1, head, 0, 1, 0);          // group 0, at holder + 0
  node(n2, head, n1, head, 5000, 2, free_block);  // group 5000, by address
  mem.put<std::uint64_t>(marks + 0x10, head);
  mem.put<std::uint64_t>(marks + 0x18, 3);
  mem.put<std::uint64_t>(marks + 0x20, holder);
  set_flag(100, true);  // the known flag (ARCH §6.7)

  // The text tables: 24 tables, three of them holding invented names (ids only are the tests' concern).
  const std::uint64_t words = obj("messages", 0x20, "words");
  if (options.texts) {
    const std::uint64_t tables = mem.alloc(24 * 8);
    const auto fmg = [&](std::uint32_t first, std::u16string_view text) {
      const std::uint64_t f = mem.alloc(0x200);
      mem.put<std::uint32_t>(f + 0x4, 1);      // one group
      mem.put<std::uint64_t>(f + 0x8, 0x40);   // groups at +0x40
      mem.put<std::uint64_t>(f + 0x10, 0x60);  // offsets at +0x60
      mem.put<std::int32_t>(f + 0x40, 0);
      mem.put<std::int32_t>(f + 0x44, static_cast<std::int32_t>(first));
      mem.put<std::int32_t>(f + 0x48, static_cast<std::int32_t>(first + 1));
      mem.put<std::int64_t>(f + 0x60, 0x100);  // first's string; first + 1 has none (0)
      mem.put<std::int64_t>(f + 0x68, 0);
      mem.put_utf16(f + 0x100, text);
      return f;
    };
    mem.put<std::uint64_t>(tables + 8 * 5, fmg(7001, u"Odd Lantern"));
    mem.put<std::uint64_t>(tables + 8 * 1, fmg(7101, u"Other Lantern"));
    mem.put<std::uint64_t>(tables + 8 * 6, fmg(8001, u"Quiet Vale"));
    mem.put<std::uint64_t>(words + 0x8, tables);
    mem.put<std::int32_t>(words + 0x10, 24);
  }
  // The param tables: slot 3 holds three rows, sorted by id.
  const std::uint64_t vault = obj("param_store", 0x20, "tables");
  if (options.params) {
    const std::uint64_t tables = mem.alloc(8 * 8);
    const std::uint64_t table = mem.alloc(0x18);
    const std::uint64_t rows = mem.alloc(3 * 0x10);
    for (std::uint32_t i = 0; i < 3; ++i) {
      const std::uint64_t data = mem.alloc(0x10);
      mem.put<std::int32_t>(data + 0x0, static_cast<std::int32_t>(9000 + i));
      mem.put<std::uint8_t>(data + 0x4, static_cast<std::uint8_t>(10 + i));
      mem.put<std::uint32_t>(rows + 0x10 * i, 4000 + 100 * i);
      mem.put<std::uint64_t>(rows + 0x10 * i + 0x8, data);
    }
    mem.put<std::uint64_t>(table + 0x0, rows);
    mem.put<std::uint16_t>(table + 0x8, 3);
    mem.put<std::uint16_t>(table + 0xA, 0x10);
    mem.put<std::uint64_t>(tables + 8 * 3, table);
    mem.put<std::uint64_t>(vault + 0x8, tables);
    mem.put<std::uint32_t>(vault + 0x10, 8);
  }

  // The characters, created at world(): the player and `actors` others.
  npcs.clear();
  player = make_character("hero", 0, 1200, 1100, 100.0f, 200.0f);
  player_handle = handle_of(0, generation_);
  for (std::uint32_t i = 0; i < options.actors; ++i) {
    const char* cls = i % 3 == 0 ? "npc" : i % 3 == 1 ? "beast" : "chr";
    const float r = 3.0f + 4.0f * static_cast<float>(i);
    npcs.push_back(make_character(cls, i + 1, 400 + 150 * static_cast<std::int32_t>(i),
                                  i == 2 ? 0 : 380 + 150 * static_cast<std::int32_t>(i), 100.0f + r, 200.0f));
    npc_handles.push_back(handle_of(i + 1, generation_));
  }
  // A boss bar on the farthest one.
  if (!npcs.empty()) {
    mem.put<std::uint64_t>(hud + 0x10, npc_handles.back());
    mem.put<std::int32_t>(hud + 0x18, 9100);
  }
}

void FakeWorld::rebuild_near() {
  const std::uint64_t roster = instance("characters");
  for (std::size_t i = 0; i < npcs.size(); ++i) {
    mem.put<std::uint64_t>(near + 0x10 * i, npcs[i]);
    mem.put<float>(near + 0x10 * i + 0x8, 3.0f + 4.0f * static_cast<float>(i));
  }
  mem.put<std::uint64_t>(roster + 0x20, near);
  mem.put<std::uint64_t>(roster + 0x28, near + 0x10 * npcs.size());
}

void FakeWorld::title() {
  const std::uint64_t roster = instance("characters");
  mem.put<std::uint64_t>(roster + 0x10, 0);
  mem.put<std::uint64_t>(roster + 0x28, near);
  mem.put<std::uint64_t>(mem.get<std::uint64_t>(roster + 0x38), 0);
  mem.put<std::int32_t>(instance("menus") + 0x8, 2);
  mem.put<std::int32_t>(instance("hud") + 0x8, 0);
}

void FakeWorld::loading() {
  title();
  mem.put<std::int32_t>(instance("menus") + 0x8, 1);
  const std::uint64_t veil = instance("loading");
  mem.put<std::uint32_t>(veil + 0x8, mem.get<std::uint32_t>(veil + 0x8) + 1);
}

void FakeWorld::world() {
  const std::uint64_t roster = instance("characters");
  mem.put<std::uint64_t>(roster + 0x10, player);
  mem.put<std::uint64_t>(mem.get<std::uint64_t>(roster + 0x38), player);
  rebuild_near();
  mem.put<std::int32_t>(instance("menus") + 0x8, 0);
  mem.put<std::int32_t>(instance("hud") + 0x8, 3);
}

void FakeWorld::advance_play(std::uint32_t ms) {
  const std::uint64_t save = instance("save_data");
  mem.put<std::uint32_t>(save + 0x10, mem.get<std::uint32_t>(save + 0x10) + ms);
}

void FakeWorld::set_lobby(std::int32_t raw) { mem.put<std::int32_t>(instance("session") + 0x8, raw); }
void FakeWorld::set_hour(std::uint8_t hour) { mem.put<std::uint8_t>(instance("world_clock") + 0x10, hour); }

void FakeWorld::set_flag(std::uint32_t id, bool on) {
  const std::uint32_t group = id / 1000;
  std::uint64_t block = 0;
  if (group == 0) block = holder;
  if (group == 1234) block = holder + 250;
  if (group == 5000) block = free_block;
  if (block == 0) return;
  const std::uint64_t at = block + (id % 1000) / 8;
  auto byte = mem.get<std::uint8_t>(at);
  const auto bit = static_cast<std::uint8_t>(1u << (7 - id % 8));
  byte = on ? static_cast<std::uint8_t>(byte | bit) : static_cast<std::uint8_t>(byte & ~bit);
  mem.put<std::uint8_t>(at, byte);
}

std::uint64_t FakeWorld::relocate(std::uint64_t old, std::size_t size, Move mode) {
  const std::uint64_t moved = mem.alloc(size);
  std::vector<std::byte> bytes(size);
  (void)mem.read(old, bytes);
  mem.write(moved, bytes);
  switch (mode) {
    case Move::Keep: break;
    case Move::Poison: {
      std::vector<std::byte> zero(size);
      mem.write(old, zero);
      set_vtable(old, "tomb");
      break;
    }
    case Move::Free: mem.release(old); break;
  }
  return moved;
}

void FakeWorld::move_all(Move mode) {
  const std::pair<const char*, std::size_t> items[] = {
      {"task_hub", 0x40},  {"characters", 0x60}, {"session", 0x20},     {"event_flags", 0x30},
      {"hud", 0x50},       {"menus", 0x20},      {"loading", 0x10},     {"world_clock", 0x20},
      {"camera", 0x10},    {"param_store", 0x20}, {"messages", 0x20},   {"save_data", 0x40},
      {"game_state", 0x20}, {"field_area", 0x20}};
  for (const auto& [item, size] : items) {
    const std::uint64_t slot = at(std::string("st_") + item);
    const std::uint64_t old = mem.get<std::uint64_t>(slot);
    if (old != 0) mem.put<std::uint64_t>(slot, relocate(old, size, mode));
  }
  // The main player moves too, and the roster and the player set follow it.
  const std::uint64_t roster = instance("characters");
  const bool in_world = mem.get<std::uint64_t>(roster + 0x10) != 0;
  player = relocate(player, kHero, mode);
  if (in_world) {
    mem.put<std::uint64_t>(roster + 0x10, player);
    mem.put<std::uint64_t>(mem.get<std::uint64_t>(roster + 0x38), player);
  }
}

}  // namespace en::gal::test
