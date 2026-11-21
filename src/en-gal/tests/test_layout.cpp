// gal/layouts/<build>.toml (layout.hpp; R7): a layout reads only for the build the gate matched; its fields stay within
// their classes; what it points at exists; enum tables cover their ranges and name only the snapshot enum's values;
// what the groups need is there; and a release build refuses leads. Every class, field, value and id is invented.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "en/gal/builds.hpp"
#include "en/gal/layout.hpp"
#include "fake_image.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

BuildProfile gated(std::string_view key = "9.8.7.0-ww") {
  FakeImageSpec spec;
  spec.sections.push_back(FakeSection{".text", kCode, std::vector<std::uint8_t>(0x100, 0x00), 0});
  spec.version = FakeVersion{};
  spec.version->file_version = {9, 8, 7, 0};
  const BuiltImage built = build(spec);
  const SpanSource source(built.file);
  const auto image = read_image(source, ImageLayout::File);
  REQUIRE(image.has_value());
  const std::string builds_text =
      "schema = 1\n[[build]]\nkey = \"" + std::string(key) +
      "\"\napp = \"4.5.6\"\nfile_version = \"9.8.7.0\"\nproduct_name = \"INVENTED GAME\"\nregion = \"ww\"\n"
      "time_date_stamp = " +
      std::to_string(spec.time_date_stamp) + "\nsize_of_image = " + std::to_string(image->size_of_image) +
      "\nlayout = \"layouts/a.toml\"\nstatus = \"retired\"\n";
  const auto builds = parse_builds(builds_text);
  REQUIRE(builds.has_value());
  auto profile = gate(*image, *builds);
  REQUIRE(profile.has_value());
  return *profile;
}

constexpr std::string_view kSample = R"(schema = 1
build = "9.8.7.0-ww"
derived_from = "probe"
probe_record = "docs/ops/probes/9.8.7.0-ww.json"

[task]
group = { name = "Invented_Group", index = 3 }
register_abi = { task = "rdx", instance = "r9", group = "rcx" }
vtable_slots = 4
run_slot = 2
object_size = 0x40
status = "confirmed"

[class.Holder]
rtti = "Invented::Holder"
singleton = "HolderOne"
copy_size = 0x100
status = "confirmed"
[class.Holder.fields]
vtable = { off = 0x0, ty = "ptr" }
mode   = { off = 0x10, ty = "i32", group = "ui", range = [0, 3], enum = "mode" }
flags  = { off = 0x14, ty = "bit", bit = 6, group = "world" }
name   = { off = 0x20, ty = "utf16[16]", group = "player" }
inner  = { off = 0x40, ty = "ptr", to = "Inner", group = "world" }
slots  = { off = 0x50, ty = "array", elem = "Slot", len = 3, stride = 0x10, group = "ui" }
when   = { off = 0x80, ty = "u64", group = "boot" }

[class.Inner]
copy_size = 0x20
status = "confirmed"
[class.Inner.fields]
level = { off = 0x4, ty = "u16", group = "player", range = [1, 999] }

[class.Slot]
stride = 0x10
status = "confirmed"
[class.Slot.fields]
handle = { off = 0x0, ty = "handle", group = "ui" }

[enum.mode]
values = { 0 = "Alpha", 1 = "Beta", 2 = "Gamma", 3 = "Delta", -1 = "Alpha" }

[param.InventedParam]
slot = 4
row_size = 0x20
status = "confirmed"
fields = { name_id = { off = 0x0, ty = "i32" }, team = { off = 0x4, ty = "u8" } }

[fmg]
indexing = "flat"
group_stride = 12
npc_names = [40, 30, 4]
place_names = [41]
status = "confirmed"

[dlrf]
name_ptr_off = 0x8
status = "confirmed"

[selftest]
npc_name_ids = [101, 102]
place_name_ids = []
npc_param_ids = [7]
)";

constexpr FieldNeed kFieldNeeds[] = {
    {"Holder", "mode", FieldType::I32}, {"Inner", "level", FieldType::U16}, {"Slot", "handle", FieldType::Handle}};
constexpr std::string_view kModeNames[] = {"Alpha", "Beta", "Gamma", "Delta", "Epsilon"};
constexpr EnumNeed kEnumNeeds[] = {{"mode", kModeNames}};
constexpr std::string_view kParamNeeds[] = {"InventedParam"};
const LayoutNeeds kNeeds{kFieldNeeds, kEnumNeeds, kParamNeeds, true};

// The sample with `from` replaced by `to`, once.
std::string changed(std::string_view from, std::string_view to) {
  std::string text(kSample);
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  return text;
}

SchemaErrors refusal(const std::string& text, LayoutMode mode = LayoutMode::Release,
                     const LayoutNeeds& needs = kNeeds) {
  auto layout = load_layout(gated(), text, needs, mode, "layout.toml");
  REQUIRE_FALSE(layout.has_value());
  return layout.error();
}

bool has_error(const SchemaErrors& errors, std::string_view where, std::string_view problem) {
  for (const SchemaError& e : errors) {
    if (e.where == where && e.problem.find(problem) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("a layout reads for the build the gate matched, with every table") {
  const auto layout = load_layout(gated(), kSample, kNeeds, LayoutMode::Release);
  REQUIRE(layout.has_value());
  CHECK(layout->build() == "9.8.7.0-ww");
  CHECK(layout->build_key().index == 0);
  CHECK(layout->derived_from() == "probe");
  REQUIRE(layout->task().has_value());
  CHECK(layout->task()->group_name == "Invented_Group");
  CHECK(layout->task()->group_index == 3);
  CHECK(layout->task()->run_slot == 2);
  CHECK(layout->task()->object_size == 0x40);
  const ClassLayout* holder = layout->find_class("Holder");
  REQUIRE(holder != nullptr);
  CHECK(holder->rtti == "Invented::Holder");
  CHECK(holder->copy_size == 0x100u);
  const FieldLayout* mode = holder->field("mode");
  REQUIRE(mode != nullptr);
  CHECK(mode->type == FieldType::I32);
  CHECK(mode->group == en::proto::Group::Ui);
  CHECK(mode->range == std::pair<std::int64_t, std::int64_t>{0, 3});
  CHECK(mode->enum_table == "mode");
  CHECK(holder->field("name")->size == 32);
  CHECK(holder->field("flags")->bit == 6);
  CHECK(holder->field("slots")->size == 0x30);
  CHECK(holder->field("when")->boot);
  CHECK_FALSE(holder->field("when")->group.has_value());
  CHECK(holder->field("absent") == nullptr);
  const EnumTable* table = layout->find_enum("mode");
  REQUIRE(table != nullptr);
  CHECK(table->name_of(2) == std::optional<std::string_view>("Gamma"));
  CHECK(table->name_of(-1) == std::optional<std::string_view>("Alpha"));
  CHECK_FALSE(table->name_of(4).has_value());
  const ParamLayout* param = layout->find_param("InventedParam");
  REQUIRE(param != nullptr);
  CHECK(param->row_size == 0x20);
  CHECK(param->fields.size() == 2);
  CHECK(layout->fmg()->group_stride == 12);
  CHECK(layout->dlrf()->name_ptr_off == 8);
  CHECK(layout->selftest().npc_name_ids == std::vector<std::uint32_t>{101, 102});
  CHECK(layout->max_copy_size() == 0x100);
  CHECK_FALSE(layout->has_leads());
}

TEST_CASE("a layout written for another build is refused, and only load_layout makes one (R7)") {
  static_assert(!std::is_default_constructible_v<Layout>);
  CHECK(has_error(refusal(changed("build = \"9.8.7.0-ww\"", "build = \"9.8.7.1-jp\"")), "layout.toml: build",
                  "is not the build the gate matched"));
  const auto other = load_layout(gated("9.8.7.0-ww"), changed("build = \"9.8.7.0-ww\"", "build = \"1.0.0.0-ww\""),
                                 kNeeds, LayoutMode::Dev);
  CHECK_FALSE(other.has_value());  // a dev build too
}

TEST_CASE("a release build refuses leads; a dev build reads them and says so") {
  const std::string lead =
      changed("[class.Inner]\ncopy_size = 0x20\nstatus = \"confirmed\"", "[class.Inner]\ncopy_size = 0x20");
  CHECK(has_error(refusal(lead), "layout.toml: class.Inner", "is a lead, which a release build refuses"));
  const auto dev = load_layout(gated(), lead, kNeeds, LayoutMode::Dev);
  REQUIRE(dev.has_value());
  CHECK(dev->has_leads());
  CHECK(has_error(refusal(changed("place_names = [41]\nstatus = \"confirmed\"",
                                  "place_names = [41]\nstatus = \"lead\"")),
                  "layout.toml: fmg", "is a lead"));
  CHECK(has_error(refusal(changed("object_size = 0x40\nstatus = \"confirmed\"", "object_size = 0x40")),
                  "layout.toml: task", "is a lead"));
}

TEST_CASE("fields stay within their class, and what they name exists") {
  CHECK(has_error(refusal(changed("when   = { off = 0x80", "when   = { off = 0xFC")),
                  "layout.toml: class.Holder.fields.when", "runs past the class's copy size"));
  CHECK(has_error(refusal(changed("copy_size = 0x100", "copy_size = 0x4001")), "layout.toml: class.Holder.copy_size",
                  "must be between 1 and 16384"));
  CHECK(has_error(refusal(changed("to = \"Inner\"", "to = \"Outer\"")), "layout.toml: class.Holder.fields.inner.to",
                  "names no class of the file"));
  CHECK(has_error(refusal(changed("elem = \"Slot\"", "elem = \"Slab\"")), "layout.toml: class.Holder.fields.slots.elem",
                  "names no class of the file"));
  CHECK(has_error(refusal(changed("[class.Slot]\nstride = 0x10", "[class.Slot]")), "layout.toml: class.Slot",
                  "needs a copy_size or a stride"));
  CHECK(has_error(refusal(changed("ty = \"utf16[16]\"", "ty = \"utf16[0]\"")),
                  "layout.toml: class.Holder.fields.name.ty", "utf16[n] with n from 1 to 1024"));
  CHECK(has_error(refusal(changed("ty = \"u16\"", "ty = \"u24\"")), "layout.toml: class.Inner.fields.level.ty",
                  "must be one of"));
  CHECK(has_error(refusal(changed("bit = 6", "bit = 8")), "layout.toml: class.Holder.fields.flags.bit",
                  "must be between 0 and 7"));
  CHECK(has_error(refusal(changed("range = [1, 999]", "range = [1, 70000]")),
                  "layout.toml: class.Inner.fields.level.range[1]", "must be between 0 and 65535"));
  CHECK(has_error(
      refusal(changed("vtable = { off = 0x0, ty = \"ptr\" }", "vtable = { off = 0x0, ty = \"f32\", range = [0, 1] }")),
      "layout.toml: class.Holder.fields.vtable.range", "applies only to whole-number fields"));
  CHECK(has_error(refusal(changed("group = \"boot\"", "group = \"weather\"")),
                  "layout.toml: class.Holder.fields.when.group", "must be a read group"));
  CHECK(has_error(
      refusal(changed("vtable = { off = 0x0, ty = \"ptr\" }", "vtable = { off = 0x0, ty = \"ptr\", colour = 1 }")),
      "layout.toml: class.Holder.fields.vtable", "has an unknown key \"colour\""));
}

TEST_CASE("enum tables cover the ranges of the fields that use them, and name only the snapshot enum's values") {
  CHECK(has_error(refusal(changed("range = [0, 3], enum", "range = [0, 4], enum")),
                  "layout.toml: class.Holder.fields.mode", "has a range its enum table does not cover"));
  CHECK(has_error(refusal(changed("3 = \"Delta\"", "3 = \"Omega\"")), "layout.toml: enum.mode.values",
                  "maps to a name the snapshot's enum does not have"));
  CHECK(has_error(refusal(changed("enum = \"mode\"", "enum = \"moods\"")), "layout.toml: class.Holder.fields.mode.enum",
                  "names no enum table of the file"));
  CHECK(has_error(refusal(changed("-1 = \"Alpha\"", "00 = \"Alpha\"")), "layout.toml: enum.mode.values",
                  "maps one raw value twice"));
  CHECK(has_error(refusal(changed("-1 = \"Alpha\"", "x1 = \"Alpha\"")), "layout.toml: enum.mode.values",
                  "has a key that is not a whole number"));
}

TEST_CASE("everything the read groups need must be there, with its type") {
  CHECK(has_error(refusal(changed("level = { off = 0x4, ty = \"u16\"", "lvl = { off = 0x4, ty = \"u16\"")),
                  "layout.toml", "lacks the field \"Inner\".\"level\" that a read group needs"));
  CHECK(has_error(refusal(changed("level = { off = 0x4, ty = \"u16\"", "level = { off = 0x4, ty = \"u32\"")),
                  "layout.toml: class.Inner.fields.level", "must be of type u16"));
  CHECK(has_error(refusal(changed("[param.InventedParam]", "[param.OtherParam]")), "layout.toml",
                  "lacks the param \"InventedParam\" that a read group needs"));
  CHECK(has_error(refusal(changed("[enum.mode]", "[enum.other]")), "layout.toml",
                  "lacks the enum table \"mode\" that a read group needs"));
  LayoutNeeds no_task = kNeeds;
  no_task.task = false;
  const std::size_t task_at = std::string(kSample).find("[task]");
  const std::size_t task_end = std::string(kSample).find("[class.Holder]");
  std::string without_task(kSample);
  without_task.erase(task_at, task_end - task_at);
  CHECK(load_layout(gated(), without_task, no_task, LayoutMode::Release).has_value());
  CHECK(has_error(refusal(without_task), "layout.toml: task", "is missing"));
}

TEST_CASE("the task, the provenance and the self-test ids keep to their forms") {
  CHECK(has_error(refusal(changed("run_slot = 2", "run_slot = 4")), "layout.toml: task.run_slot",
                  "must be below vtable_slots"));
  const std::string_view abi = "register_abi = { task = \"rdx\", instance = \"r9\", group = \"rcx\" }";
  CHECK(has_error(refusal(changed(abi, "register_abi = { task = \"rdx\", instance = \"r10\", group = \"rcx\" }")),
                  "layout.toml: task.register_abi.instance", "must be one of rcx, rdx, r8, r9"));
  CHECK(has_error(refusal(changed(abi, "register_abi = { task = \"rdx\", instance = \"rdx\", group = \"rcx\" }")),
                  "layout.toml: task.register_abi", "must name three different registers"));
  CHECK(has_error(refusal(changed(abi, "register_abi = { task = \"rdx\", instance = \"r9\" }")),
                  "layout.toml: task.register_abi.group", "is missing"));
  CHECK(has_error(refusal(changed(abi, "register_abi = \"v1\"")), "layout.toml: task.register_abi", "must be a table"));
  {
    const auto layout = load_layout(gated(), std::string(kSample), kNeeds, LayoutMode::Release);
    REQUIRE(layout.has_value());
    REQUIRE(layout->task().has_value());
    CHECK(layout->task()->register_abi == RegisterAbi{ArgRegister::Rdx, ArgRegister::R9, ArgRegister::Rcx});
    REQUIRE(layout->fmg().has_value());
    CHECK(layout->fmg()->npc_names == std::vector<std::uint32_t>{40, 30, 4});
    CHECK(layout->find_class("Holder")->singleton == "HolderOne");
  }
  CHECK(has_error(refusal(changed("singleton = \"HolderOne\"", "singleton = \"Holder One\"")),
                  "layout.toml: class.Holder.singleton", "must be 1 to 128 letters"));
  CHECK(has_error(refusal(changed("derived_from = \"probe\"", "derived_from = \"guess\"")), "layout.toml: derived_from",
                  "must be probe, or the key of the build it came from"));
  CHECK(load_layout(gated(), changed("derived_from = \"probe\"", "derived_from = \"9.8.6.0-ww\""), kNeeds,
                    LayoutMode::Release)
            .has_value());
  CHECK(has_error(
      refusal(changed("probe_record = \"docs/ops/probes/9.8.7.0-ww.json\"", "probe_record = \"/abs/x.json\"")),
      "layout.toml: probe_record", "must be a relative path"));
  CHECK(has_error(refusal(changed("npc_param_ids = [7]", "npc_param_ids = [-7]")),
                  "layout.toml: selftest.npc_param_ids[0]", "must be between 0 and 4294967295"));
  CHECK(has_error(refusal(changed("indexing = \"flat\"", "indexing = \"nested\"")), "layout.toml: fmg.indexing",
                  "must be flat"));
  CHECK(has_error(refusal(changed("schema = 1", "schema = 3")), "layout.toml: schema", "must be between 1 and 2"));
  // Schema 2 is the build's table (test_table.cpp): it needs its provenance.
  CHECK(has_error(refusal(changed("schema = 1", "schema = 2")), "layout.toml: provenance", "is missing"));
  CHECK(has_error(refusal(std::string(kSample) + "\n[extra]\nx = 1\n"), "layout.toml", "has an unknown key \"extra\""));
  CHECK(has_error(refusal("not toml\n"), "layout.toml", "is not valid TOML"));
}

TEST_CASE("a lead [task] may hold only its group, which discovery never registers with; a confirmed one holds all") {
  const std::string_view task =
      "[task]\ngroup = { name = \"Invented_Group\", index = 3 }\nregister_abi = { task = \"rdx\", instance = \"r9\", "
      "group = \"rcx\" }\nvtable_slots = 4\nrun_slot = 2\nobject_size = 0x40\nstatus = \"confirmed\"\n";
  const std::string group_only = "[task]\ngroup = { name = \"Invented_Group\", index = 3 }\nstatus = \"lead\"\n";
  const auto dev = load_layout(gated(), changed(task, group_only), kNeeds, LayoutMode::Dev);
  REQUIRE(dev.has_value());
  REQUIRE(dev->task().has_value());
  CHECK(dev->task()->partial);
  CHECK(dev->task()->group_index == 3);
  CHECK(dev->task()->group_name == "Invented_Group");
  CHECK(dev->has_leads());
  // A release build refuses it as it refuses every lead.
  CHECK(has_error(refusal(changed(task, group_only)), "layout.toml: task", "is a lead"));
  // Without a status it is a lead too; a full lead is not partial.
  const auto unstated = load_layout(gated(), changed(task, "[task]\ngroup = { name = \"Invented_Group\", index = 3 }\n"),
                                    kNeeds, LayoutMode::Dev);
  REQUIRE(unstated.has_value());
  CHECK(unstated->task()->partial);
  const auto full = load_layout(gated(), changed("object_size = 0x40\nstatus = \"confirmed\"", "object_size = 0x40"),
                                kNeeds, LayoutMode::Dev);
  REQUIRE(full.has_value());
  CHECK_FALSE(full->task()->partial);
  // Confirmed, or a lead with any of R1's keys, needs them all.
  CHECK(has_error(refusal(changed(task, "[task]\ngroup = { name = \"Invented_Group\", index = 3 }\nstatus = "
                                        "\"confirmed\"\n")),
                  "layout.toml: task.register_abi", "is missing"));
  CHECK(has_error(refusal(changed(task, "[task]\ngroup = { name = \"Invented_Group\", index = 3 }\nvtable_slots = 4\n"),
                          LayoutMode::Dev),
                  "layout.toml: task.object_size", "is missing"));
  // The group itself is always required.
  CHECK(has_error(refusal(changed(task, "[task]\nstatus = \"lead\"\n"), LayoutMode::Dev), "layout.toml: task.group",
                  "is missing"));
}

TEST_CASE("check_layout makes load_layout's checks from builds.toml's entry, without the gate") {
  BuildEntry entry;
  entry.key = "9.8.7.0-ww";
  CHECK(check_layout(entry, kSample, kNeeds, LayoutMode::Release).empty());
  const SchemaErrors other = check_layout(entry, changed("build = \"9.8.7.0-ww\"", "build = \"9.8.7.1-jp\""), kNeeds,
                                          LayoutMode::Release, "layouts/a.toml");
  CHECK(has_error(other, "layouts/a.toml: build", "is not the build the gate matched"));
  const std::string lead =
      changed("[class.Inner]\ncopy_size = 0x20\nstatus = \"confirmed\"", "[class.Inner]\ncopy_size = 0x20");
  CHECK(check_layout(entry, lead, kNeeds, LayoutMode::Dev).empty());
  CHECK_FALSE(check_layout(entry, lead, kNeeds, LayoutMode::Release).empty());
}
