// R1's probe report (probe_report.hpp).
#include "en/shell/probe_report.hpp"

namespace en::shell {

namespace report = gal::report;
using report::Key;
using report::Type;

namespace {

constexpr std::array<std::string_view, 7> kStages = {"start",   "derived",  "waiting", "registered",
                                                     "running", "refused", "failed"};
constexpr std::array<std::string_view, 8> kItems = {"gate",  "fd4_idiom",  "name_layout", "register",
                                                    "register_abi", "group", "task_shape",  "singleton"};
constexpr std::array<std::string_view, 4> kChecks = {"confirmed", "differs", "missing", "not_run"};
constexpr std::array<std::string_view, 5> kCalls = {"not_called", "waiting_for_mark", "refused", "ok", "faulted"};

constexpr std::array<Key, 49> kKeys = {{
    {"stage", Type::Word, Type::Object, kStages},
    {"elapsed_ms", Type::Unsigned},
    {"gate_passed", Type::Boolean},
    {"derive", Type::Object},
    {"passes", Type::Unsigned},
    {"ms", Type::Unsigned},
    {"written", Type::Boolean},
    {"checks", Type::Array, Type::Object},
    {"item", Type::Word, Type::Object, kItems},
    {"label", Type::Label},
    {"status", Type::Word, Type::Object, kChecks},
    {"value", Type::Unsigned},
    {"expected", Type::Unsigned},
    {"hinstance", Type::Object},
    {"slots", Type::Array, Type::Unsigned},
    {"count", Type::Unsigned},
    {"register", Type::Object},
    {"call", Type::Word, Type::Object, kCalls},
    {"callee", Type::Unsigned},
    {"registers", Type::Array, Type::Unsigned},
    {"group_index", Type::Unsigned},
    {"object_size", Type::Unsigned},
    {"vtable_slots", Type::Unsigned},
    {"task", Type::Object},
    {"first_tick_ms", Type::Unsigned},
    {"invocations", Type::Unsigned},
    {"slot_calls", Type::Array, Type::Object},
    {"slot", Type::Unsigned},
    {"calls", Type::Unsigned},
    {"object_written", Type::Unsigned},
    {"object_nonzero", Type::Unsigned},
    {"census", Type::Object},
    {"done", Type::Boolean},
    {"copies", Type::Unsigned},
    {"faults", Type::Unsigned},
    {"objects", Type::Unsigned},
    {"vtables", Type::Array, Type::Object},
    {"rva", Type::Unsigned},
    {"bases", Type::Array, Type::Object},
    {"type_descriptor", Type::Unsigned},
    {"found", Type::Unsigned},
    {"derived", Type::Unsigned},
    {"slots_min", Type::Unsigned},
    {"slots_max", Type::Unsigned},
    {"run_slot", Type::Unsigned},
    {"rising", Type::Object},
    {"candidates", Type::Unsigned},
    {"hits", Type::Array, Type::Object},
    {"offset", Type::Unsigned},
}};
constexpr std::array<Key, 22> kMore = {{
    {"rose", Type::Unsigned},
    {"still", Type::Unsigned},
    {"other", Type::Unsigned},
    {"statics", Type::Array, Type::Object},
    {"checked", Type::Unsigned},
    {"at", Type::Array, Type::Unsigned},
    {"copy_faults", Type::Unsigned},
    {"stack_refused", Type::Unsigned},
    {"allocations", Type::Unsigned},
    {"names", Type::Object},
    {"of", Type::Unsigned},
    {"agree", Type::Unsigned},
    {"qualified", Type::Unsigned},
    {"name_slots", Type::Array, Type::Object},
    {"null_ptr", Type::Unsigned},
    {"to_rdata", Type::Unsigned},
    {"to_data", Type::Unsigned},
    {"to_code", Type::Unsigned},
    {"to_other", Type::Unsigned},
    {"outside", Type::Unsigned},
    {"plain", Type::Unsigned},
    {"inline_text", Type::Unsigned},
}};

constexpr std::array<Key, kKeys.size() + kMore.size()> joined() {
  std::array<Key, kKeys.size() + kMore.size()> out{};
  for (std::size_t i = 0; i < kKeys.size(); ++i) out[i] = kKeys[i];
  for (std::size_t i = 0; i < kMore.size(); ++i) out[kKeys.size() + i] = kMore[i];
  return out;
}
constexpr std::array<Key, kKeys.size() + kMore.size()> kAll = joined();
constexpr report::Vocabulary kVocabulary{"probe", 1, kAll};

}  // namespace

const report::Vocabulary& probe_vocabulary() noexcept { return kVocabulary; }

proto::Expected<std::string, report::Problem> write_probe_report(const ProbeFindings& f) {
  report::Writer w(kVocabulary);
  w.word("stage", f.stage);
  w.number("elapsed_ms", f.elapsed_ms);
  w.boolean("gate_passed", f.gate_passed);
  w.begin_object("derive");
  w.number("passes", f.derive_passes);
  w.number("ms", f.derive_ms);
  w.boolean("written", f.derive_written);
  w.end_object();
  w.begin_array("checks");
  for (const ProbeCheck& c : f.checks) {
    w.begin_object();
    w.word("item", c.item);
    if (!c.label.empty()) w.label("label", c.label);
    w.word("status", kChecks[static_cast<std::size_t>(c.status)]);
    if (c.value) w.number("value", *c.value);
    if (c.expected) w.number("expected", *c.expected);
    w.end_object();
  }
  w.end_array();
  w.begin_object("hinstance");
  w.begin_array("slots");
  for (const std::uint32_t s : f.hinstance) w.number(s);
  w.end_array();
  w.number("count", f.hinstance_count);
  w.end_object();
  w.begin_object("register");
  w.word("call", f.register_status);
  w.number("callee", f.callee);
  w.begin_array("registers");
  for (const std::uint8_t r : f.registers) w.number(r);
  w.end_array();
  w.number("group_index", f.group_index);
  w.number("object_size", f.object_size);
  w.number("vtable_slots", f.vtable_slots);
  w.end_object();
  w.begin_object("task");
  w.number("first_tick_ms", f.first_tick_ms);
  w.number("invocations", f.invocations);
  w.begin_array("slot_calls");
  for (const auto& [slot, calls] : f.slot_calls) {
    w.begin_object();
    w.number("slot", slot);
    w.number("calls", calls);
    w.end_object();
  }
  w.end_array();
  w.number("object_written", f.object_written);
  w.number("object_nonzero", f.object_nonzero);
  w.end_object();
  w.begin_object("census");
  w.boolean("done", f.census_done);
  w.number("copies", f.census_copies);
  w.number("faults", f.census_faults);
  w.number("objects", f.census_objects);
  w.begin_array("vtables");
  for (const auto& [rva, n] : f.census_vtables) {
    w.begin_object();
    w.number("rva", rva);
    w.number("count", n);
    w.end_object();
  }
  w.end_array();
  w.begin_array("bases");
  for (const ProbeBase& b : f.bases) {
    w.begin_object();
    w.number("type_descriptor", b.type_descriptor);
    w.number("found", b.found);
    w.number("derived", b.derived);
    w.number("slots_min", b.slots_min);
    w.number("slots_max", b.slots_max);
    if (b.run_slot) w.number("run_slot", *b.run_slot);
    w.end_object();
  }
  w.end_array();
  w.end_object();
  w.begin_object("rising");
  w.number("candidates", f.rising_candidates);
  w.number("passes", f.rising_passes);
  w.begin_array("hits");
  for (const ProbeRising& r : f.rising) {
    w.begin_object();
    w.number("slot", r.slot);
    w.number("offset", r.offset);
    w.number("rose", r.rose);
    w.number("still", r.still);
    w.number("other", r.other);
    w.end_object();
  }
  w.end_array();
  w.end_object();
  w.begin_array("statics");
  for (const ProbeStatic& s : f.statics) {
    w.begin_object();
    w.label("label", s.label);
    w.number("candidates", s.candidates);
    w.number("checked", s.checked);
    w.begin_array("at");
    for (const std::uint32_t a : s.found) w.number(a);
    w.end_array();
    w.end_object();
  }
  w.end_array();
  w.number("copy_faults", f.copy_faults);
  w.number("stack_refused", f.stack_refused);
  w.number("allocations", f.allocations);
  w.begin_object("names");
  w.number("of", f.names.of);
  w.number("agree", f.names.agree);
  w.number("qualified", f.names.qualified);
  if (f.names.offset) w.number("offset", *f.names.offset);
  w.begin_array("name_slots");
  for (const ProbeNameSlot& n : f.names.slots) {
    w.begin_object();
    w.number("offset", n.offset);
    w.number("null_ptr", n.null_ptr);
    w.number("to_rdata", n.to_rdata);
    w.number("to_data", n.to_data);
    w.number("to_code", n.to_code);
    w.number("to_other", n.to_other);
    w.number("outside", n.outside);
    w.number("plain", n.plain);
    w.number("qualified", n.qualified);
    w.number("other", n.other_text);
    w.number("inline_text", n.inline_text);
    w.end_object();
  }
  w.end_array();
  w.end_object();
  return w.finish();
}

}  // namespace en::shell
