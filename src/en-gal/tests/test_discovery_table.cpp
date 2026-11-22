// Discovery through the build's table (discovery.hpp; proposal 0005 §7.1, §9.1): what the layout's [item.*] entries
// hold is taken from there and checked, and nothing looks for it; an id whose check fails is refused alone; and with
// the reader on a thread of our own, a start calls nothing of the game. The world and its texts are invented
// (fake_world.hpp); the catalogue is the repository's own.
#include <doctest.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "gal_ids_text.hpp"
#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;
using roles::Cls;
using roles::Item;

namespace {

constexpr std::string_view kProvenance = R"([provenance]
method = "own-derivation"
tool = "0.1.0-test.1"
tool_sha256 = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
round = "l4"
date = "2026-01-02"
)";

std::string hex(std::uint64_t v) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  do {
    out.insert(out.begin(), digits[v & 0xF]);
    v >>= 4;
  } while (v != 0);
  return "0x" + out;
}

// The world's layout as a schema-2 file with a table: its head changed, the provenance and `items` appended.
std::string tabled(const FakeWorld& w, const std::string& items) {
  std::string text = w.layout;
  const std::size_t at = text.find("schema = 1");
  REQUIRE(at == 0);
  text.replace(at, 10, "schema = 2");
  return text + "\n" + std::string(kProvenance) + "\n" + items;
}

// The function of the world that uses an item's static: its accessor for a singleton, its reader for the others.
std::string user_of(Item item) {
  const roles::ItemInfo& info = roles::info(item);
  if (item == Item::ModuleHandle) return "publish_module";
  return (info.kind == roles::ItemKind::Singleton ? "get_" : "read_") + std::string(info.label);
}

// One static's entry: the slot the world gives it (or `slot`), with the place in the code that uses it as its witness.
// Each of the world's functions that use a static refers to it five bytes in.
std::string static_entry(const FakeWorld& w, Item item, std::uint64_t slot = 0) {
  const roles::ItemInfo& info = roles::info(item);
  if (slot == 0) slot = w.at("st_" + std::string(info.label));
  return "[item." + std::string(info.label) + "]\nid = " + std::to_string(info.id) + "\nkind = \"static\"\nrva = " +
         hex(slot - w.base) + "\nfound_by = [\"accessor\", \"instance_class\"]\nwitnesses = [" +
         hex(w.at(user_of(item)) + 5 - w.base) +
         "]\nchecks = [\"placement\", \"agree\", \"witness\"]\nstatus = \"confirmed\"\n\n";
}

std::string every_static(const FakeWorld& w) {
  std::string out;
  for (std::size_t i = 0; i < roles::kItemCount; ++i) {
    const roles::ItemInfo& info = roles::info(static_cast<Item>(i));
    if (info.kind == roles::ItemKind::Function) continue;
    out += static_entry(w, static_cast<Item>(i));
  }
  return out;
}

std::string without_pattern(const std::string& patterns, std::string_view id) {
  const std::size_t a = patterns.find("[[pattern]]\nid = \"" + std::string(id) + "\"");
  REQUIRE(a != std::string::npos);
  const std::size_t b = patterns.find("[[pattern]]", a + 5);
  std::string out = patterns;
  out.erase(a, b == std::string::npos ? std::string::npos : b - a);
  return out;
}

DiscoveryOptions with_catalogue() {
  DiscoveryOptions o;
  o.ids = std::string(kRepositoryIdsToml);
  return o;
}

}  // namespace

TEST_CASE("table: an item the build's table holds is taken from it and checked, and needs no pattern") {
  FakeWorld w;
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  const std::string no_save = without_pattern(w.patterns, "save_data");
  CHECK_FALSE(discover(*w.sys, *gated, no_save, w.layout).has_value());  // required, and nowhere to take it from

  std::string items = static_entry(w, Item::SaveData) + static_entry(w, Item::Camera);
  items += "[item.character]\nid = 3001\nkind = \"class\"\nvtables = [";
  std::vector<std::uint64_t> tables = {w.vtable("chr"), w.vtable("hero"), w.vtable("npc"), w.vtable("beast")};
  std::sort(tables.begin(), tables.end());
  for (std::size_t i = 0; i < tables.size(); ++i) items += (i ? ", " : "") + hex(tables[i] - w.base);
  items += "]\nfound_by = [\"class\"]\nchecks = [\"placement\", \"agree\", \"class\"]\nstatus = \"confirmed\"\n";

  // A layout that holds a table is refused without the catalogue it is checked against.
  CHECK_FALSE(discover(*w.sys, *gated, no_save, tabled(w, items)).has_value());
  const auto d = discover(*w.sys, *gated, no_save, tabled(w, items), with_catalogue());
  if (!d) {
    for (const auto& e : d.error()) MESSAGE(describe(e));
  }
  REQUIRE(d.has_value());
  CHECK(d->ready());
  const DiscoveryReport& r = d->report();
  CHECK(r.table_items == 3);
  CHECK(r.table_refused == 0);
  for (const Item i : {Item::SaveData, Item::Camera}) {
    const ItemResult& item = r.items[roles::index(i)];
    INFO(roles::info(i).label);
    CHECK(item.from_table);
    CHECK(item.status == ItemStatus::Resolved);
    CHECK(item.address == w.at("st_" + std::string(roles::info(i).label)));
  }
  CHECK_FALSE(r.items[roles::index(Item::Hud)].from_table);  // not in the table yet: found as before
  CHECK(r.items[roles::index(Item::Hud)].status == ItemStatus::Resolved);
  // The class's set is the table's, to the entry: no more and no fewer.
  const VtableSet& chr = d->vtables(Cls::Character);
  CHECK(chr.all().size() == 4);
  for (const char* c : {"chr", "hero", "npc", "beast"}) CHECK(chr.contains(w.vtable(c)));
  CHECK(d->vtables(Cls::PlayerCharacter).all().size() == 1);  // not in the table: by its name, as before
}

TEST_CASE("table: an id whose check fails is refused alone, and nothing looks for it elsewhere") {
  FakeWorld w;
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  // The table puts save_data on another static's slot; the place it names as using it refers elsewhere. The pattern
  // that would find the true slot is in patterns.toml, and is not asked.
  const std::string wrong = static_entry(w, Item::SaveData, w.at("st_game_state"));
  const auto d = discover(*w.sys, *gated, w.patterns, tabled(w, wrong), with_catalogue());
  REQUIRE(d.has_value());
  const DiscoveryReport& r = d->report();
  const ItemResult& save = r.items[roles::index(Item::SaveData)];
  CHECK(save.from_table);
  CHECK(save.status == ItemStatus::Refused);
  CHECK(std::string(save.why) == "witness_elsewhere");
  CHECK(save.address == 0);
  CHECK_FALSE(d->address(Item::SaveData).has_value());
  CHECK(r.table_items == 1);
  CHECK(r.table_refused == 1);
  CHECK_FALSE(d->ready());  // a required item
  CHECK(r.items[roles::index(Item::GameState)].status == ItemStatus::Resolved);  // the others stand

  // A class whose table of methods lies in no read-only data: refused, and its group with it; no walk rescues it.
  const std::string bad_class = "[item.character]\nid = 3001\nkind = \"class\"\nvtables = [" +
                                hex(w.at("st_camera") - w.base) +
                                "]\nfound_by = [\"class\"]\nchecks = [\"placement\", \"agree\", \"class\"]\n"
                                "status = \"confirmed\"\n";
  const auto c = discover(*w.sys, *gated, w.patterns, tabled(w, bad_class), with_catalogue());
  REQUIRE(c.has_value());
  CHECK(c->report().table_refused == 1);
  CHECK(c->report().rtti_missing == 1);
  CHECK(c->vtables(Cls::Character).empty());
  CHECK(c->report().groups[static_cast<std::size_t>(en::proto::Group::World)] == GroupWhy::NotDiscovered);
}

TEST_CASE("table: with every static in the table and the reader on our own thread, a start searches and calls nothing") {
  FakeWorld w(WorldOptions{false});  // no name layout: the old way would call the game's getter for each name
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  DiscoveryOptions o = with_catalogue();
  o.frame = FrameProvider::OwnThread;
  // No pattern and no recipe at all.
  const auto d = discover(*w.sys, *gated, "schema = 1\n", tabled(w, every_static(w)), o);
  if (!d) {
    for (const auto& e : d.error()) MESSAGE(describe(e));
  }
  REQUIRE(d.has_value());
  CHECK(d->ready());
  const DiscoveryReport& r = d->report();
  CHECK(r.table_items == 15);
  CHECK(r.table_refused == 0);
  CHECK(r.pattern_hits == 0);
  CHECK(r.fd4_hits == 0);
  CHECK(r.fd4_names == 0);
  CHECK(r.getter_calls == 0);
  CHECK(w.sys->getter_calls == 0);
  for (std::size_t i = 0; i < roles::kItemCount; ++i) {
    const roles::ItemInfo& info = roles::info(static_cast<Item>(i));
    if (info.kind == roles::ItemKind::Function) continue;
    INFO(info.label);
    CHECK(r.items[i].from_table);
    CHECK(r.items[i].address == w.at("st_" + std::string(info.label)));
  }
}

TEST_CASE("table: the reader's own thread never calls the getter, so a site without a name stays unnamed") {
  FakeWorld w(WorldOptions{false});
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  DiscoveryOptions o;
  o.frame = FrameProvider::OwnThread;
  const auto d = discover(*w.sys, *gated, w.patterns, w.layout, o);
  REQUIRE(d.has_value());
  CHECK_FALSE(d->ready());
  CHECK(d->report().getter_calls == 0);
  CHECK(w.sys->getter_calls == 0);
  CHECK(d->report().fd4_accepted == 11);
  CHECK(d->report().fd4_names == 0);
  CHECK(d->report().items[roles::index(Item::Characters)].status == ItemStatus::Missing);
}
