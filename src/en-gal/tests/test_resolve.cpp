// Lodestar's resolver (resolve.hpp; proposal 0005 §7.1, §9.1): each item of a build's table is re-found in the image
// by its placement and by its witnesses, the places in the code that use it; an item that fails is refused alone, and
// only a checked item's place is handed out. The program, its code and every id here are invented for the tests.
#include <doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/resolve.hpp"
#include "fake_program.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

using P = FakeProgram;

// An invented program: one function that uses a static, a table and another function in several ways, the function
// it calls, a third that only jumps to it, and their data.
struct Rig {
  P p;
  BuiltImage built;

  Rig() {
    // The caller: every instruction below is a witness, or a place that looks like one and is not.
    p.label("caller", P::Text);
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28});  // room on the stack
    p.label("w_load", P::Text);                  // loads the static's value
    p.bytes(P::Text, {0x48, 0x8B, 0x05});
    p.rel32(P::Text, "lantern");
    p.label("w_call", P::Text);  // calls the function
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "ring_bell");
    p.label("w_table", P::Text);  // takes the table's address
    p.bytes(P::Text, {0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "rota");
    p.label("w_address", P::Text);  // takes the function's address
    p.bytes(P::Text, {0x48, 0x8D, 0x15});
    p.rel32(P::Text, "ring_bell");
    p.label("w_other", P::Text);  // loads another static
    p.bytes(P::Text, {0x48, 0x8B, 0x05});
    p.rel32(P::Text, "ledger");
    p.label("w_branch", P::Text);  // branches to the function on a condition: no use of it as a function
    p.bytes(P::Text, {0x0F, 0x84});
    p.rel32(P::Text, "ring_bell");
    p.label("w_plain", P::Text);  // refers to nothing
    p.bytes(P::Text, {0x48, 0x89, 0xC8});
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0xC3});
    p.label("caller_end", P::Text);
    p.align(P::Text, 16, 0xCC);
    p.label("ring_bell", P::Text);
    p.bytes(P::Text, {0x48, 0x89, 0xC8, 0xC3});
    p.label("ring_bell_end", P::Text);
    p.align(P::Text, 16, 0xCC);
    p.label("tail", P::Text);  // a function that only jumps to the other
    p.bytes(P::Text, {0xE9});
    p.rel32(P::Text, "ring_bell");
    p.label("tail_end", P::Text);
    p.align(P::Text, 16, 0xCC);
    p.label("w_nothing", P::Text);  // a byte that starts no instruction in 64-bit mode
    p.bytes(P::Text, {0x06});
    p.align(P::Text, 16, 0xCC);
    p.function("caller", "caller_end");
    p.function("ring_bell", "ring_bell_end");
    p.function("tail", "tail_end");

    p.label("vt_a", P::Rdata);
    p.zeros(P::Rdata, 24);
    p.label("vt_b", P::Rdata);
    p.zeros(P::Rdata, 24);
    p.label("rota", P::Rdata);
    p.zeros(P::Rdata, 4 * 16);
    p.label("rdata_end", P::Rdata);

    p.label("lantern", P::Data);
    p.u64(P::Data, 0);
    p.label("ledger", P::Data);
    p.u64(P::Data, 0);
    p.label("data_end", P::Data);
    built = p.build();
  }

  std::uint32_t at(const std::string& label) const { return p.rva(label); }
  // The end of a section as the loader maps it, which may lie past the bytes the program wrote.
  std::uint32_t end_of(const std::string& label) const {
    const SpanSource source(built.file);
    const auto image = read_image(source, ImageLayout::File);
    REQUIRE(image.has_value());
    const Section* s = image->section_at(at(label));
    REQUIRE(s != nullptr);
    return s->rva + s->mapped_size();
  }

  ItemEntry lantern(std::vector<std::string> witnesses = {"w_load"}) const {
    ItemEntry item;
    item.label = "lantern";
    item.id = 1001;
    item.kind = IdKind::Static;
    item.rva = at("lantern");
    for (const std::string& w : witnesses) item.witnesses.push_back(at(w));
    return item;
  }
  ItemEntry bell(std::vector<std::string> witnesses = {"w_call", "w_address", "tail"}) const {
    ItemEntry item;
    item.label = "ring_bell";
    item.id = 2001;
    item.kind = IdKind::Function;
    item.rva = at("ring_bell");
    item.length = at("ring_bell_end") - at("ring_bell");
    for (const std::string& w : witnesses) item.witnesses.push_back(at(w));
    return item;
  }
  ItemEntry rota() const {
    ItemEntry item;
    item.label = "rota";
    item.id = 4001;
    item.kind = IdKind::Table;
    item.rva = at("rota");
    item.stride = 16;
    item.count = 4;
    item.witnesses.push_back(at("w_table"));
    return item;
  }
  ItemEntry walker() const {
    ItemEntry item;
    item.label = "walker";
    item.id = 3001;
    item.kind = IdKind::Class;
    item.vtables = {at("vt_a"), at("vt_b")};
    return item;
  }
};

ImageInfo read(const std::vector<std::byte>& bytes, ImageLayout layout) {
  const SpanSource source(bytes);
  auto image = read_image(source, layout);
  REQUIRE(image.has_value());
  return *image;
}

ItemState check(const Rig& rig, const ItemEntry& item) {
  const ImageInfo image = read(rig.built.file, ImageLayout::File);
  const SpanSource source(rig.built.file);
  return check_item(image, source, item);
}

}  // namespace

TEST_CASE("an item of every kind is re-found: its placement, and every witness referring to it") {
  const Rig rig;
  for (const ImageLayout layout : {ImageLayout::File, ImageLayout::Memory}) {
    CAPTURE(static_cast<int>(layout));
    const std::vector<std::byte>& bytes = layout == ImageLayout::File ? rig.built.file : rig.built.memory;
    const ImageInfo image = read(bytes, layout);
    const SpanSource source(bytes);
    const ItemState lantern = check_item(image, source, rig.lantern());
    CHECK(lantern.ok());
    CHECK(lantern.id == 1001);
    CHECK(lantern.witnesses == 1);
    // A function is used by a call, by a jump, and by taking its address.
    const ItemState bell = check_item(image, source, rig.bell());
    CHECK(bell.ok());
    CHECK(bell.witnesses == 3);
    const ItemState rota = check_item(image, source, rig.rota());
    CHECK(rota.ok());
    CHECK(rota.witnesses == 1);
    const ItemState walker = check_item(image, source, rig.walker());
    CHECK(walker.ok());
    CHECK(walker.witnesses == 0);
    // An item with no witness yet (a lead) is placed, and no more.
    const ItemState bare = check_item(image, source, rig.lantern({}));
    CHECK(bare.ok());
    CHECK(bare.witnesses == 0);
  }
}

TEST_CASE("placement: a value that does not lie where its kind must is refused") {
  const Rig rig;
  const auto problem = [&](ItemEntry item) { return check(rig, item).problem; };
  // A static: data that is read and written, with room for a pointer.
  ItemEntry item = rig.lantern();
  item.rva = rig.at("vt_a");
  CHECK(problem(item) == ItemProblem::Section);  // read-only data
  item.rva = rig.at("caller");
  CHECK(problem(item) == ItemProblem::Section);  // code
  item.rva = 0x200;
  CHECK(problem(item) == ItemProblem::Section);  // the headers: no section
  item.rva = rig.end_of("lantern") - 4;  // no room for a pointer before the section ends
  CHECK(problem(item) == ItemProblem::Extent);
  // A function: a start the image's function table lists, of the table's length.
  item = rig.bell();
  item.rva += 1;
  CHECK(problem(item) == ItemProblem::NotFunctionStart);
  item = rig.bell();
  item.rva = rig.at("w_nothing");  // code that is no function's
  CHECK(problem(item) == ItemProblem::NotFunctionStart);
  item = rig.bell();
  item.rva = rig.at("lantern");
  CHECK(problem(item) == ItemProblem::Section);
  item = rig.bell();
  item.length += 1;
  CHECK(problem(item) == ItemProblem::Length);
  // A table: whole within data that is not run.
  item = rig.rota();
  item.rva = rig.at("caller");
  CHECK(problem(item) == ItemProblem::Section);
  item = rig.rota();
  item.count = (rig.end_of("rota") - item.rva) / item.stride + 1;  // one entry past the section's end
  CHECK(problem(item) == ItemProblem::Extent);
  item.count -= 1;  // and exactly to its end
  CHECK(problem(item) == ItemProblem::None);
  item = rig.rota();
  item.rva = rig.at("lantern");  // a table in written data is placed: its witness is what refuses this one
  CHECK(problem(item) == ItemProblem::WitnessElsewhere);
  // A class: its tables of methods in read-only data, aligned.
  item = rig.walker();
  item.vtables[1] = rig.at("lantern");
  CHECK(problem(item) == ItemProblem::Section);
  item = rig.walker();
  item.vtables[0] += 4;
  CHECK(problem(item) == ItemProblem::Alignment);
  item = rig.walker();
  item.vtables.clear();
  CHECK(problem(item) == ItemProblem::Section);
  item = rig.walker();
  item.vtables[1] = rig.end_of("vt_a") - 8;  // the section's last pointer
  CHECK(problem(item) == ItemProblem::None);
  item.vtables[1] = rig.end_of("vt_a");  // past its end: the next section's, or none
  CHECK(problem(item) != ItemProblem::None);
  CHECK(check_of(ItemProblem::Length) == ItemCheck::Placement);
}

TEST_CASE("witness: a place that does not use the id refuses it, and says which place") {
  const Rig rig;
  // The first witness holds, the second uses another static.
  ItemState state = check(rig, rig.lantern({"w_load", "w_other"}));
  CHECK(state.problem == ItemProblem::WitnessElsewhere);
  CHECK(state.witness == 1);
  CHECK(state.witnesses == 1);
  CHECK(check_of(state.problem) == ItemCheck::Witness);
  // An instruction that refers to nothing; a place in the middle of an instruction; a place in data.
  CHECK(check(rig, rig.lantern({"w_plain"})).problem == ItemProblem::WitnessElsewhere);
  ItemEntry inside = rig.lantern();
  inside.witnesses[0] += 2;
  CHECK(check(rig, inside).problem == ItemProblem::WitnessElsewhere);
  // One byte in, past the instruction's prefix, the bytes still decode to a load of the same static. A witness says
  // that the bytes at a place refer to the id, not that an instruction starts there: on a build the gate has matched,
  // that is what a wrong table or a patched place would break.
  inside = rig.lantern();
  inside.witnesses[0] += 1;
  CHECK(check(rig, inside).ok());
  CHECK(check(rig, rig.lantern({"ledger"})).problem == ItemProblem::WitnessOutsideCode);
  CHECK(check(rig, rig.lantern({"vt_a"})).problem == ItemProblem::WitnessOutsideCode);
  CHECK(check(rig, rig.lantern({"w_nothing"})).problem == ItemProblem::WitnessUndecodable);
  // A static is used by an operand, never by a call: the call to the function is no witness of the static.
  CHECK(check(rig, rig.lantern({"w_call"})).problem == ItemProblem::WitnessElsewhere);
  // A function: a branch on a condition is no call of it, and a use of the static is no use of the function.
  CHECK(check(rig, rig.bell({"w_branch"})).problem == ItemProblem::WitnessElsewhere);
  CHECK(check(rig, rig.bell({"w_call", "w_load"})).problem == ItemProblem::WitnessElsewhere);
  // A table is used by an operand; the function's call is not one.
  ItemEntry rota = rig.rota();
  rota.witnesses = {rig.at("w_call")};
  CHECK(check(rig, rota).problem == ItemProblem::WitnessElsewhere);
  // The same places, with the item moved: every witness now refers elsewhere.
  ItemEntry moved = rig.lantern();
  moved.rva = rig.at("ledger");
  CHECK(check(rig, moved).problem == ItemProblem::WitnessElsewhere);
}

TEST_CASE("witness: bytes that cannot be read refuse the id, and nothing else is read") {
  const Rig rig;
  const ImageInfo image = read(rig.built.file, ImageLayout::File);
  const std::optional<std::uint64_t> offset = image.offset_of(rig.at("w_load"), 1);
  REQUIRE(offset.has_value());
  const HoleSource holed(rig.built.file, *offset, *offset + 1);
  const ItemState state = check_item(image, holed, rig.lantern());
  CHECK(state.problem == ItemProblem::WitnessUnreadable);
  CHECK(state.witness == 0);
  // One read per witness, of the instruction's own bytes: placement reads nothing of the image's body.
  const HoleSource whole(rig.built.file, 0, 0);
  CHECK(check_item(image, whole, rig.bell()).ok());
  CHECK(whole.reads == 3);
  const HoleSource untouched(rig.built.file, 0, 0);
  CHECK(check_item(image, untouched, rig.walker()).ok());
  CHECK(untouched.reads == 0);
}

TEST_CASE("the library hands out the place of a checked item only, and one refusal touches no other item") {
  const Rig rig;
  const ImageInfo image = read(rig.built.file, ImageLayout::File);
  const SpanSource source(rig.built.file);
  const auto builds = parse_builds(
      "schema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\napp = \"4.5.6\"\nfile_version = \"9.8.7.0\"\n"
      "product_name = \"INVENTED GAME\"\nregion = \"ww\"\ntime_date_stamp = " +
      std::to_string(image.time_date_stamp) + "\nsize_of_image = " + std::to_string(image.size_of_image) +
      "\nlayout = \"layouts/a.toml\"\nstatus = \"supported\"\n");
  REQUIRE(builds.has_value());
  const auto profile = gate(image, *builds);
  REQUIRE(profile.has_value());
  const auto id = [](std::uint32_t n, std::string_view label, std::string_view kind) {
    return "[[id]]\nn = " + std::to_string(n) + "\nlabel = \"" + std::string(label) + "\"\nkind = \"" +
           std::string(kind) + "\"\nmeans = \"An invented thing.\"\nneeds = \"world\"\nrequired = false\n";
  };
  const auto catalogue = parse_ids("schema = 1\n" + id(1001, "lantern", "static") + id(1002, "ledger", "static") +
                                   id(2001, "ring_bell", "function") + id(3001, "walker", "class") +
                                   id(4001, "rota", "table"));
  REQUIRE(catalogue.has_value());
  const auto n = [&](const std::string& label) { return std::to_string(rig.at(label)); };
  // The ledger's witness is the lantern's: the table is wrong about the ledger, and only about it.
  const std::string text =
      "schema = 2\nbuild = \"9.8.7.0-ww\"\nderived_from = \"probe\"\nprobe_record = \"docs/ops/probes/invented.md\"\n"
      "[provenance]\nmethod = \"own-derivation\"\ntool = \"0.1.0-test.1\"\n"
      "tool_sha256 = \"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\"\nround = \"l3\"\n"
      "date = \"2026-01-02\"\n"
      "[item.lantern]\nid = 1001\nkind = \"static\"\nrva = " + n("lantern") + "\nfound_by = [\"accessor\"]\n"
      "witnesses = [" + n("w_load") + "]\n"
      "[item.ledger]\nid = 1002\nkind = \"static\"\nrva = " + n("ledger") + "\nfound_by = [\"accessor\"]\n"
      "witnesses = [" + n("w_load") + "]\n"
      "[item.ring_bell]\nid = 2001\nkind = \"function\"\nrva = " + n("ring_bell") + "\nlength = 4\n"
      "found_by = [\"relation\"]\nwitnesses = [" + n("w_call") + ", " + n("tail") + "]\n"
      "[item.walker]\nid = 3001\nkind = \"class\"\nvtables = [" + n("vt_a") + ", " + n("vt_b") + "]\n"
      "found_by = [\"class\"]\n"
      "[item.rota]\nid = 4001\nkind = \"table\"\nrva = " + n("rota") + "\nstride = 16\ncount = 4\n"
      "found_by = [\"table\"]\nwitnesses = [" + n("w_table") + "]\n";
  LayoutNeeds needs;
  needs.task = false;
  needs.ids = &*catalogue;
  const auto layout = load_layout(*profile, text, needs, LayoutMode::Dev);
  if (!layout.has_value()) {
    for (const SchemaError& e : layout.error()) MESSAGE(describe(e));
  }
  REQUIRE(layout.has_value());

  const Library library(*layout, image, source);
  REQUIRE(library.states().size() == 5);
  CHECK(library.refused() == 1);

  const auto lantern = library.place(1001, IdKind::Static);
  REQUIRE(lantern.has_value());
  CHECK(*lantern == rig.at("lantern"));
  const auto bell = library.place(2001, IdKind::Function);
  REQUIRE(bell.has_value());
  CHECK(*bell == rig.at("ring_bell"));
  const auto rota = library.place(4001, IdKind::Table);
  REQUIRE(rota.has_value());
  CHECK(*rota == rig.at("rota"));
  ItemProblem why = ItemProblem::Unknown;
  const auto vtables = library.vtables(3001, &why);
  CHECK(why == ItemProblem::None);
  REQUIRE(vtables.size() == 2);
  CHECK(vtables[0] == rig.at("vt_a"));
  CHECK(vtables[1] == rig.at("vt_b"));

  // The refused item: its place is never handed out, and its state says why.
  const auto ledger = library.place(1002, IdKind::Static);
  REQUIRE_FALSE(ledger.has_value());
  CHECK(ledger.error() == ItemProblem::WitnessElsewhere);
  REQUIRE(library.state(1002) != nullptr);
  CHECK(library.state(1002)->witness == 0);
  CHECK(library.state(1001)->ok());

  // An id the table lacks, and an id asked for as another kind.
  const auto absent = library.place(1003, IdKind::Static);
  REQUIRE_FALSE(absent.has_value());
  CHECK(absent.error() == ItemProblem::Unknown);
  CHECK(library.state(1003) == nullptr);
  const auto wrong = library.place(1001, IdKind::Function);
  REQUIRE_FALSE(wrong.has_value());
  CHECK(wrong.error() == ItemProblem::Kind);
  const auto as_place = library.place(3001, IdKind::Class);  // a class has tables of methods, not one place
  REQUIRE_FALSE(as_place.has_value());
  CHECK(as_place.error() == ItemProblem::Kind);
  CHECK(library.vtables(1001, &why).empty());
  CHECK(why == ItemProblem::Kind);
  CHECK(library.vtables(3002, &why).empty());
  CHECK(why == ItemProblem::Unknown);
  CHECK(library.vtables(3002).empty());
}

TEST_CASE("the problems' words are fixed, each its own") {
  const ItemProblem all[] = {ItemProblem::None,               ItemProblem::Unknown,
                             ItemProblem::Kind,               ItemProblem::Section,
                             ItemProblem::Alignment,          ItemProblem::NotFunctionStart,
                             ItemProblem::Length,             ItemProblem::Extent,
                             ItemProblem::WitnessOutsideCode, ItemProblem::WitnessUnreadable,
                             ItemProblem::WitnessUndecodable, ItemProblem::WitnessElsewhere};
  std::vector<std::string> words;
  for (const ItemProblem p : all) {
    const std::string word = to_string(p);
    CHECK_FALSE(word.empty());
    for (const std::string& other : words) CHECK(other != word);
    words.push_back(word);
  }
  CHECK(std::string(to_string(ItemProblem::WitnessElsewhere)) == "witness_elsewhere");
}
