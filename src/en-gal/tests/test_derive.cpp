// The derivation kit on the tests' invented program (sample_program.hpp): the RTTI walk, the FD4 finder, the pattern
// cutter's wildcards, the leads file, and the derivation with its report: what it finds, what it refuses, and that the
// report holds no name, string or path of the program's.
#include <doctest.h>

#include <algorithm>
#include <ostream>  // doctest prints std::string_view through it
#include <set>
#include <string>
#include <vector>

#include "code.hpp"
#include "cut.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/rtti.hpp"
#include "sample_program.hpp"

namespace {

using en::gal::ImageBytes;
using en::gal::ImageLayout;
using en::gal::SpanSource;
using en::gal::derive::Status;
using en::gal::test::build_sample;
using en::gal::test::Sample;

struct Loaded {
  Sample sample;
  en::gal::ImageInfo image;
};

Loaded load(ImageLayout layout = ImageLayout::File) {
  Loaded l{build_sample(), {}};
  const auto& bytes = layout == ImageLayout::File ? l.sample.built.file : l.sample.built.memory;
  const SpanSource source(bytes);
  auto image = en::gal::read_image(source, layout);
  REQUIRE(image.has_value());
  l.image = *image;
  return l;
}

std::uint32_t rva(const Loaded& l, const std::string& label) { return l.sample.program.rva(label); }

en::gal::derive::Leads leads(const char* text) {
  auto parsed = en::gal::derive::parse_leads(text);
  if (!parsed) {
    for (const auto& e : parsed.error()) MESSAGE(en::gal::describe(e));
  }
  REQUIRE(parsed.has_value());
  return *parsed;
}

const en::gal::derive::SingletonResult& singleton(const en::gal::derive::Result& r, const std::string& label) {
  const auto it =
      std::find_if(r.fd4.singletons.begin(), r.fd4.singletons.end(), [&](const auto& s) { return s.label == label; });
  REQUIRE(it != r.fd4.singletons.end());
  return *it;
}

}  // namespace

TEST_CASE("rtti: the walk finds every locator and vtable, and the sets by their bases") {
  const Loaded l = load();
  const SpanSource source(l.sample.built.file);
  const ImageBytes img(l.image, source);
  const en::gal::rtti::Walk walk = en::gal::rtti::walk(img);
  CHECK(walk.locators.size() == 12);  // eleven classes, one with a second subobject
  CHECK(walk.vtables.size() == 12);
  CHECK(walk.rejected == 0);

  CHECK(en::gal::rtti::decorate("Kin::Body") == ".?AVBody@Kin@@");
  CHECK(en::gal::rtti::decorate("A::B::C", true) == ".?AUC@B@A@@");
  CHECK(en::gal::rtti::decorate("Lone") == ".?AVLone@@");
  CHECK_FALSE(en::gal::rtti::decorate("Kin::").has_value());
  CHECK_FALSE(en::gal::rtti::decorate("Kin Body").has_value());

  const auto body = en::gal::rtti::find_type_descriptor(img, "Kin::Body");
  CHECK(body.count == 1);
  CHECK(body.rva == rva(l, "td_body"));
  CHECK(en::gal::rtti::find_type_descriptor(img, "Kin::Nobody").count == 0);
  CHECK(en::gal::rtti::find_type_descriptor(img, "Other::Body").rva == rva(l, "td_other"));

  const auto set = en::gal::rtti::derived_from(walk, body.rva, false);
  const std::vector<std::uint32_t> primary = {rva(l, "vt_body_0"), rva(l, "vt_hero_0"), rva(l, "vt_beast_0"),
                                              rva(l, "vt_rider_0")};
  std::vector<std::uint32_t> want = primary;
  std::sort(want.begin(), want.end());
  CHECK(set.primary == want);
  CHECK(set.subobject == std::vector<std::uint32_t>{rva(l, "vt_rider_16")});
  CHECK(set.classes == 4);
  const auto strict = en::gal::rtti::derived_from(walk, body.rva, true);
  CHECK(strict.primary.size() == 3);  // the base itself left out

  const auto vts = en::gal::rtti::vtables_of(walk, rva(l, "td_rider"));
  REQUIRE(vts.size() == 2);
  CHECK(vts[0]->slots == 2);
  CHECK(vts[1]->slots == 1);
}

TEST_CASE("fd4: the finder with a pattern of the sample's shape: placements, names, ambiguity") {
  const Loaded l = load();
  const SpanSource source(l.sample.built.file);
  const ImageBytes img(l.image, source);
  en::gal::fd4::Idiom idiom;
  auto bytes = en::gal::parse_pattern(
      "48 8B 3D ?? ?? ?? ?? 48 85 FF 75 20 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? BA ?? ?? ?? ?? 49 89 C0 48 8D 0D ?? ?? "
      "?? ?? "
      "E8 ?? ?? ?? ??");
  REQUIRE(bytes.has_value());
  idiom.bytes = *bytes;
  idiom.static_slot = {3, 7};
  idiom.class_object = {15, 19};
  idiom.getter = {20, 24};
  idiom.path = en::gal::Capture{35, 39};
  en::gal::fd4::Config config;
  config.path_suffix = "Lone.h";
  config.names = en::gal::fd4::NameLayout{0x10, en::gal::fd4::NameEncoding::Ascii};
  const auto found = en::gal::fd4::find(img, idiom, config);
  CHECK(found.hits == 8);
  std::vector<std::string> names;
  for (const auto& s : found.singletons) names.push_back(s.name + (s.ambiguous ? "?" : "") + std::to_string(s.sites));
  CHECK(names == std::vector<std::string>{"Clock1", "Dup?2", "Hub2", "Roster1"});
  std::multiset<std::string> refused;
  for (const auto& s : found.sites) {
    if (s.refused) refused.insert(*s.name + ":" + en::gal::fd4::to_string(*s.refused));
  }
  CHECK(refused == std::multiset<std::string>{"Rock:the static is not in the data section",
                                              "Stray:the path does not end with the suffix"});

  // Without the path in the pattern, the path's load is found by decoding on.
  en::gal::fd4::Idiom head = idiom;
  head.bytes.value.resize(24);
  head.bytes.mask.resize(24);
  head.path.reset();
  const auto by_decoding = en::gal::fd4::find(img, head, config);
  CHECK(by_decoding.singletons.size() == 4);
  CHECK(std::count_if(by_decoding.sites.begin(), by_decoding.sites.end(), [](const auto& s) { return s.refused; }) ==
        2);

  // Without a name layout, nothing is named.
  config.names.reset();
  const auto unnamed = en::gal::fd4::find(img, idiom, config);
  CHECK(unnamed.singletons.empty());
  CHECK(unnamed.unnamed == 6);
}

TEST_CASE("cut: what a pattern wildcards, and that it is unique") {
  const Loaded l = load();
  const SpanSource source(l.sample.built.file);
  const ImageBytes img(l.image, source);
  const en::gal::SectionCopy* text = img.named(".text");
  REQUIRE(text != nullptr);
  const auto fixups = en::gal::read_relocations(l.image, source);
  REQUIRE(fixups.has_value());
  CHECK(!fixups->empty());

  // The registration's body, from entry + 6: no RIP-relative operand, a branch inside, a jump inside, an immediate -1.
  const std::uint32_t reg = rva(l, "register");
  const auto insns = en::gal::code::decode_range(l.image, *text, reg, rva(l, "register_end"));
  REQUIRE(insns.size() == 16);
  const auto all = en::gal::cut::pattern_of(img, *text, *fixups, std::span(insns).subspan(3, 13));
  CHECK(all.text() ==
        "4C 89 CB 89 CE 3B 73 34 73 0C 48 89 54 F3 40 FF 43 38 31 C0 EB 05 B8 FF FF FF FF 48 83 C4 28 5E 5B C3");
  // Cut short before their targets, the branches are wildcarded.
  const auto part = en::gal::cut::pattern_of(img, *text, *fixups, std::span(insns).subspan(3, 4));
  CHECK(part.text() == "4C 89 CB 89 CE 3B 73 34 73 ??");

  // An accessor: the static's, the class object's and the path's displacements, the calls' targets and the site
  // number (0x1001, too large to keep) are wildcarded; the short jump inside is kept.
  const std::uint32_t hub = rva(l, "get_hub");
  const auto acc = en::gal::code::decode_range(l.image, *text, hub + 5, rva(l, "get_hub_end"));
  REQUIRE(acc.size() >= 9);
  const auto idiom = en::gal::cut::pattern_of(img, *text, *fixups, std::span(acc).first(9));
  CHECK(idiom.text() ==
        "48 8B 3D ?? ?? ?? ?? 48 85 FF 75 20 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? BA ?? ?? ?? ?? 49 89 C0 48 8D 0D ?? "
        "?? ?? ?? "
        "E8 ?? ?? ?? ??");

  // A 64-bit address under a base relocation, as data in .rdata, is not code: but its bytes, were they in a pattern,
  // would be wildcarded. The cutter on the registration: unique, interior, at entry + 6.
  const auto c = en::gal::cut::cut(img, *text, *fixups, {reg, rva(l, "register_end"), 5, {}});
  REQUIRE(c.has_value());
  CHECK(c->matches == 1);
  CHECK(c->start == reg + 6);
  CHECK(c->bytes.size() >= 20);
  CHECK_FALSE(c->capture.has_value());
  // A cut that must cover the ledger's reference, with its capture.
  const std::uint32_t deaths = rva(l, "ledger_deaths");
  const auto s = en::gal::cut::cut(img, *text, *fixups, {deaths, rva(l, "ledger_deaths_end"), 5, deaths + 5});
  REQUIRE(s.has_value());
  REQUIRE(s->capture.has_value());
  CHECK(s->start == deaths + 5);
  CHECK(s->capture->at == 3);
  CHECK(s->capture->insn_end == 7);
}

TEST_CASE("leads: the file's form, and what it refuses") {
  const auto ok = leads(en::gal::test::kSampleLeads);
  CHECK(ok.product_name == "INVENTED GAME");
  CHECK(ok.fd4_path_suffix == "Lone.h");
  CHECK(ok.singletons.size() == 7);
  CHECK(ok.register_instance == "hub");
  CHECK(ok.task_group == "Gr_Settle");
  CHECK(ok.task_base == "Tasks::Base");
  CHECK(ok.vtable_sets.size() == 2);
  REQUIRE(ok.statics.size() == 2);
  CHECK(ok.statics[0].label == "ledger");
  CHECK(ok.statics[0].rtti == "Store::Ledger");
  CHECK(ok.statics[1].module);

  const auto refused = [](std::string text) {
    const auto r = en::gal::derive::parse_leads(text);
    REQUIRE_FALSE(r.has_value());
    std::vector<std::string> out;
    for (const auto& e : r.error()) out.push_back(en::gal::describe(e));
    return out;
  };
  const std::string base = en::gal::test::kSampleLeadsClean;
  const auto replace = [&](const std::string& from, const std::string& to) {
    std::string t = base;
    const std::size_t at = t.find(from);
    REQUIRE(at != std::string::npos);
    t.replace(at, from.size(), to);
    return t;
  };
  CHECK(refused(replace("hub = \"Hub\"", "Hub = \"Hub\"")).front() ==
        "leads: singletons.\"Hub\": is not a label: lower-case letters, digits and _, starting with a letter");
  CHECK(refused(replace("hub = \"Hub\"", "hub = \"C:\\\\Hub\"")).front() ==
        "leads: singletons.\"hub\": must be a name of letters, digits and _");
  CHECK(refused(replace("instance = \"hub\"", "instance = \"nobody\"")).front() ==
        "leads: register.instance: must be the label of a singleton");
  CHECK(refused(replace("base = \"Tasks::Base\"", "base = \"Tasks Base\"")).front() ==
        "leads: task.base: must be a class name such as Space::Class");
  CHECK(
      refused(replace("ledger = { rtti = \"Store::Ledger\" }", "ledger = { rtti = \"Store::Ledger\", module = true }"))
          .front() == "leads: statics.\"ledger\": must give either rtti or module = true");
  CHECK(refused(replace("ledger = {", "body = {")).front() == "leads: a label is used twice");
  CHECK(refused(replace("schema = 1", "schema = 2")).front() == "leads: schema: must be between 1 and 1");
  CHECK(refused(replace("[register]", "[register]\nbytes = \"48 8B\"")).front() ==
        "leads: register: has an unknown key \"bytes\"");
  CHECK(refused(replace("path_suffix = \"Lone.h\"", "path_suffix = \"Lone.h\\n\"")).front() ==
        "leads: fd4.path_suffix: must be 1 to 64 letters, digits, _ . - \\ or /");
}

TEST_CASE("derive: the sample's facts, from the file and with the decoys refused") {
  const Loaded l = load();
  const SpanSource source(l.sample.built.file);
  const auto r = en::gal::derive::derive(l.image, source, leads(en::gal::test::kSampleLeads));

  // The fingerprint.
  CHECK(r.fingerprint.has_version);
  CHECK(r.fingerprint.region == "ww");
  CHECK(r.fingerprint.product_name == "matches");
  CHECK(r.fingerprint.time_date_stamp == 0x12345678);

  // The FD4 idiom, cut from the sites that load the path, and the singletons by their names.
  CHECK(r.fd4.status == Status::Derived);
  CHECK(r.fd4.path_strings == 1);
  CHECK(r.fd4.path_references == 7);  // every accessor but the one with the decoy path
  CHECK(r.fd4.sites == 7);
  CHECK(r.fd4.shapes == 1);
  REQUIRE(r.fd4.idiom.has_value());
  CHECK(r.fd4.idiom->bytes.text() ==
        "48 8B 3D ?? ?? ?? ?? 48 85 FF 75 20 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? BA ?? ?? ?? ?? 49 89 C0 48 8D 0D ?? "
        "?? ?? ?? "
        "E8 ?? ?? ?? ??");
  CHECK(r.fd4.idiom->static_slot == en::gal::Capture{3, 7});
  CHECK(r.fd4.idiom->class_object == en::gal::Capture{15, 19});
  CHECK(r.fd4.idiom->getter == en::gal::Capture{20, 24});
  REQUIRE(r.fd4.idiom->path.has_value());
  CHECK(*r.fd4.idiom->path == en::gal::Capture{35, 39});
  CHECK(r.fd4.idiom_hits == 8);
  CHECK(r.fd4.sites_matched == 7);
  REQUIRE(r.fd4.names.has_value());
  CHECK(r.fd4.names->offset == 0x10);
  CHECK(r.fd4.names->encoding == en::gal::fd4::NameEncoding::Ascii);
  CHECK(r.fd4.scan_resolved);
  CHECK(r.fd4.accepted == 6);
  CHECK(r.fd4.refused[static_cast<std::size_t>(en::gal::fd4::Refusal::StaticSection)] == 1);
  CHECK(r.fd4.refused[static_cast<std::size_t>(en::gal::fd4::Refusal::PathText)] == 1);
  CHECK(singleton(r, "hub").status == Status::Derived);
  CHECK(singleton(r, "hub").static_slot == rva(l, "st_hub"));
  CHECK(singleton(r, "hub").class_object == rva(l, "cls_Hub"));
  CHECK(singleton(r, "hub").getter == rva(l, "getter"));
  CHECK(singleton(r, "hub").sites == 2);
  CHECK(singleton(r, "roster").static_slot == rva(l, "st_roster"));
  CHECK(singleton(r, "clock").static_slot == rva(l, "st_clock"));
  CHECK(singleton(r, "dup").status == Status::Ambiguous);
  CHECK(singleton(r, "stray").status == Status::Refused);
  CHECK(singleton(r, "stray").refusal == en::gal::fd4::Refusal::PathText);
  CHECK(singleton(r, "rock").status == Status::Refused);
  CHECK(singleton(r, "rock").refusal == en::gal::fd4::Refusal::StaticSection);
  CHECK(singleton(r, "absent").status == Status::Missing);
  CHECK(r.fd4.unlisted == 0);

  // RTTI: the sets, the task classes and the statics' classes.
  REQUIRE(r.sets.size() == 2);  // in the labels' order
  CHECK(r.sets[0].label == "beast");
  CHECK(r.sets[0].primary == std::vector<std::uint32_t>{rva(l, "vt_beast_0"), rva(l, "vt_rider_0")});
  CHECK(r.sets[1].label == "body");
  CHECK(r.sets[1].status == Status::Derived);
  CHECK(r.sets[1].primary.size() == 4);
  CHECK(r.sets[1].subobject == std::vector<std::uint32_t>{rva(l, "vt_rider_16")});
  CHECK(r.task.status == Status::Derived);
  CHECK(r.task.vtables.size() == 3);
  CHECK(r.task.slots_min == 3);
  CHECK(r.task.slots_max == 3);
  CHECK(r.task.slot_targets == std::vector<std::uint32_t>{1, 3, 1});
  CHECK(r.task.run_slot == std::optional<std::uint32_t>(1));

  // The registration: its callee, its argument order, and a unique interior pattern.
  CHECK(r.registration.status == Status::Derived);
  CHECK(r.registration.instance_static == rva(l, "st_hub"));
  REQUIRE_FALSE(r.registration.candidates.empty());
  const auto& reg = r.registration.candidates.front();
  CHECK(reg.callee == rva(l, "register"));
  CHECK(reg.sites == 2);
  CHECK(reg.abi.instance == std::optional<std::uint8_t>(9));
  CHECK(reg.abi.task == std::optional<std::uint8_t>(2));
  CHECK(reg.abi.group == std::optional<std::uint8_t>(1));
  CHECK(reg.abi.shaped);
  CHECK(reg.abi_agrees);
  REQUIRE(r.registration.pattern.has_value());
  CHECK(r.registration.pattern->matches == 1);
  CHECK(r.registration.pattern->scan_resolved);
  CHECK(r.registration.pattern->hit - r.registration.pattern->entry >= 5);

  // The group's index: from its table of names and from the code that names it.
  CHECK(r.group.status == Status::Derived);
  REQUIRE(r.group.candidates.size() == 2);
  CHECK(r.group.candidates[0].index == 2);
  CHECK(r.group.candidates[0].source == "pointer_table");
  CHECK(r.group.candidates[0].entries == 4);
  CHECK(r.group.candidates[1].index == 2);
  CHECK(r.group.candidates[1].source == "code_reference");

  // The statics wait for R1's slots; their classes' vtables are ready for R1's search.
  REQUIRE(r.statics.size() == 2);
  CHECK(r.statics[0].status == Status::NeedsSlot);
  CHECK(r.statics[0].rtti.vtable == rva(l, "vt_ledger_0"));
  CHECK(r.statics[1].status == Status::NeedsSlot);
  CHECK_FALSE(en::gal::derive::complete(r));  // the decoys and the absent name keep it incomplete
}

TEST_CASE("derive: with R1's slots, patterns at the statics' references; complete with clean leads") {
  for (const ImageLayout layout : {ImageLayout::File, ImageLayout::Memory}) {
    CAPTURE(layout == ImageLayout::File);
    const Loaded l = load(layout);
    const SpanSource source(layout == ImageLayout::File ? l.sample.built.file : l.sample.built.memory);
    en::gal::derive::Options options;
    options.static_slots = {{"ledger", rva(l, "st_ledger")}};
    const auto r = en::gal::derive::derive(l.image, source, leads(en::gal::test::kSampleLeadsClean), options);
    REQUIRE(r.statics.size() == 1);
    const auto& s = r.statics[0];
    CHECK(s.status == Status::Derived);
    CHECK(s.references == 2);
    REQUIRE(s.pattern.has_value());
    CHECK(s.pattern->matches == 1);
    CHECK(s.pattern->scan_resolved);
    CHECK(s.pattern->target == rva(l, "st_ledger"));
    REQUIRE(s.pattern->capture.has_value());
    CHECK(s.pattern->hit - s.pattern->entry >= 5);
    CHECK(en::gal::derive::complete(r));
  }
}

TEST_CASE("derive: the report holds numbers, words, labels and patterns only, and lints clean") {
  const Loaded l = load();
  const SpanSource source(l.sample.built.file);
  en::gal::derive::Options options;
  options.file_size = l.sample.built.file.size();
  options.static_slots = {{"ledger", rva(l, "st_ledger")}, {"module_base", rva(l, "st_module")}};
  const auto r = en::gal::derive::derive(l.image, source, leads(en::gal::test::kSampleLeads), options);
  const auto text = en::gal::derive::write_report(r);
  REQUIRE(text.has_value());
  const en::gal::report::Vocabulary* known[] = {&en::gal::derive::vocabulary()};
  CHECK(en::gal::report::lint(*text, known).has_value());
  // No name, path or text of the program's: its singleton names, class names, group names and paths.
  for (const char* secret :
       {"Hub", "Roster", "Clock", "Lone", "game\\", "Kin", "Body", "Tasks", "Gr_", "Store", "Ledger", "INVENTED"}) {
    CAPTURE(secret);
    CHECK(text->find(secret) == std::string::npos);
  }
  CHECK(text->find("\"label\": \"hub\"") != std::string::npos);
  CHECK(text->find("\"report\": \"derive\"") == 4);
  CHECK(text->find("\"item\": \"static_pattern_in_memory\"") != std::string::npos);
  CHECK(text->find("\"item\": \"layout_fields\"") != std::string::npos);
}

TEST_CASE("derive: an image with nothing to find answers missing everywhere, and still writes a clean report") {
  en::gal::test::FakeProgram p;
  p.label("f", en::gal::test::FakeProgram::Text);
  p.bytes(en::gal::test::FakeProgram::Text, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3});
  p.label("f_end", en::gal::test::FakeProgram::Text);
  p.function("f", "f_end");
  const auto built = p.build(std::nullopt);
  const SpanSource source(built.file);
  const auto image = en::gal::read_image(source, ImageLayout::File);
  REQUIRE(image.has_value());
  const auto r = en::gal::derive::derive(*image, source, leads(en::gal::test::kSampleLeads));
  CHECK_FALSE(r.fingerprint.has_version);
  CHECK(r.fd4.status == Status::Missing);
  REQUIRE(r.fd4.singletons.size() == 7);  // each lead reported, missing
  CHECK(std::all_of(r.fd4.singletons.begin(), r.fd4.singletons.end(),
                    [](const auto& s) { return s.status == Status::Missing; }));
  CHECK(r.registration.status == Status::Depends);
  CHECK(r.task.status == Status::Missing);
  CHECK(r.group.status == Status::Missing);
  CHECK_FALSE(en::gal::derive::complete(r));
  const auto text = en::gal::derive::write_report(r);
  REQUIRE(text.has_value());
  const en::gal::report::Vocabulary* known[] = {&en::gal::derive::vocabulary()};
  CHECK(en::gal::report::lint(*text, known).has_value());
}
