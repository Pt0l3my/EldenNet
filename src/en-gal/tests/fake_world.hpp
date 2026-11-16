#pragma once
// FakeWorld: an invented game, mapped into a FakeSys process, for the in-game part's tests (part 4). Its image is a
// FakeProgram (fake_program.hpp) with FD4-style accessors for eleven singletons of invented names, their runtime class
// objects naming them at +0x10, a registration function of its own argument order, four functions reading AOB
// statics, and MSVC RTTI for every class (a character base with a player, an NPC and a beast, a prop that is no
// character, and a tombstone); its heap holds every object those statics reach, laid out by an invented layout (its
// gal/ texts are below). Nothing in it is any game's: every name, offset and value is made up here.
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "fake_program.hpp"
#include "fake_sys.hpp"

namespace en::gal::test {

struct WorldOptions {
  bool name_layout = true;  // builds.toml's dlrf_name_layout: names read from the class objects, else by the getter
  std::uint32_t actors = 6;
  bool params = true;  // the param store holds tables
  bool texts = true;   // the text store holds tables
};

enum class Move : std::uint8_t { Poison, Free, Keep };

class FakeWorld {
 public:
  explicit FakeWorld(const WorldOptions& options = {});

  FakeMemory mem;
  FakeProgram program;
  BuiltImage built;
  std::uint64_t base = 0;
  std::unique_ptr<FakeSys> sys;
  // gal/'s files for this world.
  std::string builds;
  std::string patterns;
  std::string layout;

  // ---- Its code and data, by label ----
  [[nodiscard]] std::uint64_t at(const std::string& label) const { return base + program.rva(label); }
  [[nodiscard]] std::uint64_t vtable(const std::string& cls) const { return at("vt_" + cls + "_0"); }
  // The live instance behind an item's static ("characters", "save_data", ...).
  [[nodiscard]] std::uint64_t instance(const std::string& item) const { return mem.get<std::uint64_t>(at("st_" + item)); }

  // ---- The world's state ----
  void title();
  void loading();
  void world();
  void advance_play(std::uint32_t ms);
  // Every singleton, the AOB statics' objects and the main player move to new places; the old ones become a
  // tombstone, are released, or stay.
  void move_all(Move mode);
  // An object's vtable replaced by another class's (`cls`: "tomb", "prop", "chr", ...).
  void set_vtable(std::uint64_t object, const std::string& cls) { mem.put<std::uint64_t>(object, vtable(cls)); }
  void set_lobby(std::int32_t raw);
  void set_hour(std::uint8_t hour);
  void set_flag(std::uint32_t id, bool on);

  std::uint64_t player = 0;
  std::vector<std::uint64_t> npcs;
  std::vector<std::uint64_t> npc_handles;
  std::uint64_t player_handle = 0;
  std::uint64_t near = 0;      // the near list's array
  std::uint64_t holder = 0;    // the flag blocks' holder (mode 1)
  std::uint64_t free_block = 0;  // a block reached by address (mode 2)

 private:
  void build_image(const WorldOptions& options);
  void build_heap(const WorldOptions& options);
  std::uint64_t make_character(const std::string& cls, std::uint32_t index, std::int32_t max_hp, std::int32_t hp,
                               float x, float z);
  void rebuild_near();
  std::uint64_t relocate(std::uint64_t old, std::size_t size, Move mode);
  std::uint32_t generation_ = 1;
};

}  // namespace en::gal::test
