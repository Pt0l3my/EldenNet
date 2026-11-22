// Shapes (shape.hpp; proposal 0005 §6.1; O102): a recipe as a run of instruction forms, matched with our own decoder,
// and the accessor finder on a shape (fd4.hpp). Each form accepts its instruction whatever registers and distances it
// uses and refuses its look-alikes; a shape stands only where its instructions follow one another; and on the tests'
// invented programs the shape accepts exactly the sites the byte pattern accepts, with every place the same. The
// programs and their code are invented for the tests.
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/shape.hpp"
#include "fake_program.hpp"
#include "sample_program.hpp"

using namespace en::gal;
using namespace en::gal::test;
using shape::Form;

namespace {

using P = FakeProgram;

const shape::Shape kAccessor{
    {Form::LoadSlot, Form::TestLoaded, Form::BranchSetShort, Form::AddressArg1, Form::CallRelative}};

// An invented program of small pieces of code, each under a label: the accessor's head with one thing changed.
struct Rig {
  P p;
  BuiltImage built;

  // load, test, branch, address, call; each piece a list of bytes, with a reference where `ref` names one.
  void head(const std::string& label, std::vector<std::uint8_t> load, std::vector<std::uint8_t> test,
            std::vector<std::uint8_t> branch, std::vector<std::uint8_t> address) {
    p.label(label, P::Text);
    p.bytes(P::Text, load);
    p.rel32(P::Text, "slot");
    p.bytes(P::Text, test);
    p.bytes(P::Text, branch);
    p.bytes(P::Text, address);
    p.rel32(P::Text, "object");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "helper");
    p.bytes(P::Text, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});  // what the branch skips to
    p.align(P::Text, 16, 0xCC);
  }

  Rig() {
    p.label("helper", P::Text);
    p.bytes(P::Text, {0xC3});
    p.align(P::Text, 16, 0xCC);
    // The head as written with the first register, then with others: the same forms, other bytes.
    head("plain", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    head("other_register", {0x48, 0x8B, 0x3D}, {0x48, 0x85, 0xFF}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    head("high_register", {0x4C, 0x8B, 0x0D}, {0x4D, 0x85, 0xC9}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    // Look-alikes, each wrong in one step.
    head("tests_another", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xFF}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    head("half_width_load", {0x90, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    head("branch_when_clear", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x74, 0x0C}, {0x48, 0x8D, 0x0D});
    head("branch_back", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x75, 0xF0}, {0x48, 0x8D, 0x0D});
    head("second_argument", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x15});
    head("narrow_load", {0x44, 0x8B, 0x05}, {0x45, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    head("narrow_test", {0x48, 0x8B, 0x05}, {0x90, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    head("something_between", {0x48, 0x8B, 0x05}, {0x90, 0x48, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    // The branch with a 32-bit distance: another form.
    head("near_branch", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x0F, 0x85, 0x0C, 0x00, 0x00, 0x00},
         {0x48, 0x8D, 0x0D});
    // A load that is not RIP-relative, with the same bytes after it (the register's own field, then 4 bytes).
    p.label("not_a_slot", P::Text);
    p.bytes(P::Text, {0x48, 0x8B, 0x83, 0x10, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x75, 0x0C, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "object");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "helper");
    p.bytes(P::Text, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
    p.align(P::Text, 16, 0xCC);
    // A call through a register in the last step.
    p.label("call_through", P::Text);
    p.bytes(P::Text, {0x48, 0x8B, 0x05});
    p.rel32(P::Text, "slot");
    p.bytes(P::Text, {0x48, 0x85, 0xC0, 0x75, 0x0C, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "object");
    p.bytes(P::Text, {0xFF, 0xD0, 0x90, 0x90, 0x90});
    p.bytes(P::Text, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
    p.align(P::Text, 16, 0xCC);
    // A head behind a byte that puts a straight decode out of step: the first byte of a longer instruction.
    p.label("stray", P::Text);
    p.bytes(P::Text, {0xB8});
    head("out_of_step", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0}, {0x75, 0x0C}, {0x48, 0x8D, 0x0D});
    p.label("text_end", P::Text);
    p.function("helper", "plain");

    p.label("names", P::Rdata);
    p.zeros(P::Rdata, 16);
    p.label("slot", P::Data);
    p.u64(P::Data, 0);
    p.label("object", P::Data);
    p.zeros(P::Data, 32);
    built = p.build();
  }

  std::uint32_t at(const std::string& label) const { return p.rva(label); }
};

struct Opened {
  ImageInfo image;
  SpanSource source;
  ImageBytes img;
  explicit Opened(const std::vector<std::byte>& bytes)
      : image(read(bytes)), source(bytes), img(image, source) {}
  static ImageInfo read(const std::vector<std::byte>& bytes) {
    const SpanSource s(bytes);
    auto image = read_image(s, ImageLayout::File);
    REQUIRE(image.has_value());
    return *image;
  }
  const SectionCopy& text() const {
    const SectionCopy* t = img.named(".text");
    REQUIRE(t != nullptr);
    return *t;
  }
};

// One of the finder's sites, as the comparison sees it.
using SiteKey = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, std::string>;

std::vector<SiteKey> keys(const fd4::Result& result, bool without_unreadable) {
  std::vector<SiteKey> out;
  for (const fd4::Site& s : result.sites) {
    if (without_unreadable && s.refused == fd4::Refusal::Unreadable) continue;
    out.emplace_back(s.hit, s.static_slot, s.class_object, s.getter, s.path,
                     s.refused ? static_cast<int>(*s.refused) : -1, s.name.value_or(""));
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

TEST_CASE("a shape stands where its forms' instructions follow one another, whatever registers they use") {
  const Rig rig;
  const Opened o(rig.built.file);
  for (const char* label : {"plain", "other_register", "high_register", "out_of_step"}) {
    CAPTURE(label);
    const std::optional<shape::Hit> hit = shape::match_at(o.img, o.text(), kAccessor, rig.at(label));
    REQUIRE(hit.has_value());
    CHECK(hit->rva == rig.at(label));
    CHECK(hit->end == rig.at(label) + 24);
    CHECK(hit->gives[0] == rig.at("slot"));    // the load's slot
    CHECK(hit->gives[1] == 0);                 // a test gives nothing
    CHECK(hit->gives[2] == 0);
    CHECK(hit->gives[3] == rig.at("object"));  // the address taken
    CHECK(hit->gives[4] == rig.at("helper"));  // the call's target
    REQUIRE(hit->branch_to.has_value());
    CHECK(*hit->branch_to == rig.at(label) + 24);  // past the call: the block the branch skips ends there
  }
}

TEST_CASE("a shape refuses its look-alikes: each form is the instruction it names, and no other") {
  const Rig rig;
  const Opened o(rig.built.file);
  for (const char* label : {"tests_another", "half_width_load", "branch_when_clear", "branch_back", "second_argument",
                            "something_between", "near_branch", "not_a_slot", "call_through", "narrow_load",
                            "narrow_test"}) {
    CAPTURE(label);
    CHECK_FALSE(shape::match_at(o.img, o.text(), kAccessor, rig.at(label)).has_value());
  }
  // The branch with a 32-bit distance is a form of its own.
  const shape::Shape near{
      {Form::LoadSlot, Form::TestLoaded, Form::BranchSetNear, Form::AddressArg1, Form::CallRelative}};
  const std::optional<shape::Hit> hit = shape::match_at(o.img, o.text(), near, rig.at("near_branch"));
  REQUIRE(hit.has_value());
  CHECK(hit->end == rig.at("near_branch") + 28);
  CHECK_FALSE(shape::match_at(o.img, o.text(), near, rig.at("plain")).has_value());
  // A part of the shape stands wherever that part does: the load and its test alone, in the look-alikes too.
  const shape::Shape start{{Form::LoadSlot, Form::TestLoaded}};
  CHECK(shape::match_at(o.img, o.text(), start, rig.at("second_argument")).has_value());
  CHECK(shape::match_at(o.img, o.text(), start, rig.at("call_through")).has_value());
  CHECK_FALSE(shape::match_at(o.img, o.text(), start, rig.at("tests_another")).has_value());
  // Outside the section, at its last byte, and one byte into a head: nothing.
  CHECK_FALSE(shape::match_at(o.img, o.text(), kAccessor, 0x10).has_value());
  CHECK_FALSE(shape::match_at(o.img, o.text(), kAccessor, rig.at("text_end") - 1).has_value());
  CHECK_FALSE(shape::match_at(o.img, o.text(), kAccessor, rig.at("plain") + 1).has_value());
}

TEST_CASE("a mismatch names the first step that differs, and how") {
  const Rig rig;
  const Opened o(rig.built.file);
  const auto miss = [&](const char* label) { return shape::mismatch_at(o.img, o.text(), kAccessor, rig.at(label)); };
  using shape::Differs;
  using shape::Mismatch;
  CHECK_FALSE(miss("plain").has_value());
  CHECK_FALSE(miss("high_register").has_value());
  // A load that is the form in all but its width: a 32-bit value is no pointer to a manager.
  CHECK(miss("narrow_load") == Mismatch{0, Differs::Narrow});
  CHECK(miss("not_a_slot") == Mismatch{0, Differs::Other});
  CHECK(miss("half_width_load") == Mismatch{0, Differs::Other});  // another instruction stands first
  CHECK(miss("tests_another") == Mismatch{1, Differs::OtherRegister});
  CHECK(miss("something_between") == Mismatch{1, Differs::Other});
  CHECK(miss("branch_when_clear") == Mismatch{2, Differs::Other});
  CHECK(miss("branch_back") == Mismatch{2, Differs::Other});
  CHECK(miss("near_branch") == Mismatch{2, Differs::Other});
  CHECK(miss("second_argument") == Mismatch{3, Differs::Other});
  CHECK(miss("call_through") == Mismatch{4, Differs::Other});
  CHECK(shape::mismatch_at(o.img, o.text(), kAccessor, rig.at("text_end") - 1).has_value());
  CHECK(shape::mismatch_at(o.img, o.text(), kAccessor, 0x10) == Mismatch{0, Differs::Undecodable});
  CHECK_FALSE(shape::mismatch_at(o.img, o.text(), shape::Shape{}, rig.at("plain")).has_value());
  CHECK(std::string(shape::to_string(Differs::Narrow)) == "narrow");
  CHECK(std::string(shape::to_string(Differs::OtherRegister)) == "other_register");
}

TEST_CASE("find looks at every offset: a site a straight decode would miss is found, and the cap keeps the count") {
  const Rig rig;
  const Opened o(rig.built.file);
  const shape::Matches all = shape::find(o.img, o.text(), kAccessor, 64);
  CHECK(all.count == 4);
  REQUIRE(all.hits.size() == 4);
  std::vector<std::uint32_t> at;
  for (const shape::Hit& h : all.hits) at.push_back(h.rva);
  CHECK(at == std::vector<std::uint32_t>{rig.at("plain"), rig.at("other_register"), rig.at("high_register"),
                                         rig.at("out_of_step")});
  const shape::Matches capped = shape::find(o.img, o.text(), kAccessor, 2);
  CHECK(capped.count == 4);
  CHECK(capped.hits.size() == 2);
  // A shape that starts with another form is looked for at every offset too.
  const shape::Shape calls{{Form::AddressArg1, Form::CallRelative}};
  // Fifteen heads: all but the one into the second argument's register and the one that calls through a register.
  CHECK(shape::find(o.img, o.text(), calls, 64).count == 13);
}

TEST_CASE("a shape's words and its validity") {
  for (const Form f : {Form::LoadSlot, Form::TestLoaded, Form::BranchSetShort, Form::BranchSetNear, Form::AddressArg1,
                       Form::CallRelative}) {
    CHECK(shape::form_of(shape::to_string(f)) == f);
  }
  CHECK(std::string(shape::to_string(Form::AddressArg1)) == "address_arg1");
  CHECK_FALSE(shape::form_of("mov").has_value());
  CHECK_FALSE(shape::form_of("").has_value());
  CHECK(shape::gives_place(Form::LoadSlot));
  CHECK(shape::gives_place(Form::CallRelative));
  CHECK_FALSE(shape::gives_place(Form::TestLoaded));
  CHECK(kAccessor.valid());
  CHECK_FALSE(shape::Shape{}.valid());
  CHECK_FALSE((shape::Shape{{Form::TestLoaded, Form::LoadSlot}}).valid());  // a test before any load
  CHECK_FALSE((shape::Shape{std::vector<Form>(shape::kMaxSteps + 1, Form::CallRelative)}).valid());
  CHECK((shape::Shape{std::vector<Form>(shape::kMaxSteps, Form::CallRelative)}).valid());
  const Rig rig;
  const Opened o(rig.built.file);
  CHECK(shape::find(o.img, o.text(), shape::Shape{}, 64).count == 0);
  // The accessor's roles: each step of the form that gives its place.
  fd4::Accessor accessor{kAccessor, 0, 3, 4};
  CHECK(accessor.valid());
  accessor.class_step = 4;
  CHECK_FALSE(accessor.valid());
  accessor = fd4::Accessor{kAccessor, 0, 3, 9};
  CHECK_FALSE(accessor.valid());
  CHECK(fd4::find(o.img, accessor, fd4::Config{}).sites.empty());
}

TEST_CASE("on the invented programs the shape accepts exactly the sites the pattern accepts, every place the same") {
  // The pattern the derivation cuts for these programs' accessors, registers wildcarded, and its captures.
  const auto bytes = parse_pattern("?? 8B ?? ?? ?? ?? ?? ?? 85 ?? 75 ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ??");
  REQUIRE(bytes.has_value());
  fd4::Idiom idiom;
  idiom.bytes = *bytes;
  idiom.static_slot = {3, 7};
  idiom.class_object = {15, 19};
  idiom.getter = {20, 24};
  const fd4::Accessor accessor{kAccessor, 0, 3, 4};
  fd4::Config config;
  config.path_suffix = "Lone.h";
  config.names = fd4::NameLayout{0x10, fd4::NameEncoding::Ascii};

  struct Case {
    const char* name;
    Sample sample;
    std::size_t sites;     // the shape's
    std::size_t accepted;  // of them
  };
  const Case cases[] = {{"sample", build_sample(), 8, 6},
                        {"variants", build_variants(), 6, 6},
                        {"path early", build_path_early(), 0, 0}};
  for (const Case& c : cases) {
    CAPTURE(c.name);
    for (const ImageLayout layout : {ImageLayout::File, ImageLayout::Memory}) {
      const std::vector<std::byte>& raw = layout == ImageLayout::File ? c.sample.built.file : c.sample.built.memory;
      const SpanSource source(raw);
      const auto image = read_image(source, layout);
      REQUIRE(image.has_value());
      const ImageBytes img(*image, source);
      const fd4::Result by_pattern = fd4::find(img, idiom, config);
      const fd4::Result by_shape = fd4::find(img, accessor, config);
      // What the pattern hits and then refuses as no idiom, the shape never hits; everything else is the same site,
      // with the same static, class object, getter, path, verdict and name.
      CHECK(keys(by_shape, false) == keys(by_pattern, true));
      CHECK(by_shape.hits == by_shape.sites.size());
      CHECK(by_shape.unnamed == by_pattern.unnamed);
      REQUIRE(by_shape.singletons.size() == by_pattern.singletons.size());
      for (std::size_t i = 0; i < by_shape.singletons.size(); ++i) {
        CHECK(by_shape.singletons[i].name == by_pattern.singletons[i].name);
        CHECK(by_shape.singletons[i].static_slot == by_pattern.singletons[i].static_slot);
        CHECK(by_shape.singletons[i].sites == by_pattern.singletons[i].sites);
        CHECK(by_shape.singletons[i].ambiguous == by_pattern.singletons[i].ambiguous);
      }
      if (c.sites != 0) {
        CHECK(by_shape.sites.size() == c.sites);
        CHECK(static_cast<std::size_t>(std::count_if(by_shape.sites.begin(), by_shape.sites.end(),
                                                     [](const fd4::Site& s) { return !s.refused; })) == c.accepted);
      }
    }
  }
}
