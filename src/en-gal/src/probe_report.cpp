// R1's probe report (probe_report.hpp).
#include "en/gal/probe_report.hpp"

namespace en::gal::probe {

using report::Key;
using report::Type;

namespace {

constexpr std::array<std::string_view, 7> kStages = {"start",   "derived",  "waiting", "registered",
                                                     "running", "refused", "failed"};
constexpr std::array<std::string_view, 8> kItems = {"gate",  "fd4_idiom",  "name_layout", "register",
                                                    "register_abi", "group", "task_shape",  "singleton"};
constexpr std::array<std::string_view, 4> kChecks = {"confirmed", "differs", "missing", "not_run"};
constexpr std::array<std::string_view, 5> kCalls = {"not_called", "waiting_for_mark", "refused", "ok", "faulted"};
constexpr std::array<std::string_view, 4> kForms = {"direct", "register", "memory", "rip"};
constexpr std::array<std::string_view, 4> kRegions = {"private", "mapped", "image", "other"};
constexpr std::array<std::string_view, 3> kModules = {"ours", "game", "other"};
constexpr std::array<std::string_view, 5> kSections = {"header", "code", "rdata", "data", "other"};
constexpr std::array<std::string_view, 3> kStops = {"done", "bytes_cap", "time_cap"};

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
constexpr std::array<Key, 37> kMore = {{
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
    {"inst_statics", Type::Unsigned},
    {"inst_null", Type::Unsigned},
    {"inst_unread", Type::Unsigned},
    {"inst_unclassed", Type::Unsigned},
    {"inst_classed", Type::Unsigned},
    {"inst_matched", Type::Unsigned},
    {"hub_search", Type::Array, Type::Object},
    {"at_ms", Type::Unsigned},
    {"nodes", Type::Unsigned},
    {"read_bytes", Type::Unsigned},
    {"refused_reads", Type::Unsigned},
    {"inside", Type::Unsigned},
    {"depth", Type::Unsigned},
    {"path", Type::Array, Type::Unsigned},
    {"near_group", Type::Unsigned},
}};
// Spike.6 (WHY§227): the callee's summary and the whole-memory search.
constexpr std::array<Key, 48> kSpike6 = {{
    {"callee_code", Type::Object},
    {"read", Type::Boolean},
    {"extent", Type::Unsigned},
    {"bytes", Type::Unsigned},
    {"instructions", Type::Unsigned},
    {"cut", Type::Boolean},
    {"undecodable", Type::Boolean},
    {"first_call", Type::Unsigned},
    {"first_ret", Type::Unsigned},
    {"returns_early", Type::Boolean},
    {"modelled_to", Type::Unsigned},
    {"call_count", Type::Unsigned},
    {"call_sites", Type::Array, Type::Object},
    {"form", Type::Word, Type::Object, kForms},
    {"target", Type::Unsigned},
    {"base", Type::Unsigned},
    {"index", Type::Unsigned},
    {"disp", Type::Signed},
    {"vtable_of", Type::Unsigned},
    {"branch_count", Type::Unsigned},
    {"branches", Type::Array, Type::Object},
    {"to", Type::Signed},
    {"args", Type::Array, Type::Object},
    {"reg", Type::Unsigned},
    {"stores", Type::Array, Type::Object},
    {"via", Type::Unsigned},
    {"base_arg", Type::Unsigned},
    {"rip", Type::Boolean},
    {"width", Type::Unsigned},
    {"saved", Type::Array, Type::Unsigned},
    {"compares", Type::Array, Type::Object},
    {"imm", Type::Unsigned},
    {"compared_other", Type::Unsigned},
    {"tested", Type::Unsigned},
    {"memory_search", Type::Array, Type::Object},
    {"regions", Type::Unsigned},
    {"read_regions", Type::Unsigned},
    {"unread_bytes", Type::Unsigned},
    {"stack_bytes", Type::Unsigned},
    {"stopped", Type::Word, Type::Object, kStops},
    {"own_known", Type::Boolean},
    {"own_found", Type::Unsigned},
    {"own_inside", Type::Unsigned},
    {"own", Type::Boolean},
    {"region", Type::Word, Type::Object, kRegions},
    {"protect", Type::Unsigned},
    {"module", Type::Word, Type::Object, kModules},
    {"section", Type::Word, Type::Object, kSections},
}};

constexpr std::size_t kKeyCount = kKeys.size() + kMore.size() + kSpike6.size();
constexpr std::array<Key, kKeyCount> joined() {
  std::array<Key, kKeyCount> out{};
  std::size_t n = 0;
  for (const Key& k : kKeys) out[n++] = k;
  for (const Key& k : kMore) out[n++] = k;
  for (const Key& k : kSpike6) out[n++] = k;
  return out;
}
constexpr std::array<Key, kKeyCount> kAll = joined();
// One key, one type: a name given twice would be read as its first entry alone.
constexpr bool unique_names() {
  for (std::size_t i = 0; i < kAll.size(); ++i) {
    for (std::size_t j = i + 1; j < kAll.size(); ++j) {
      if (kAll[i].name == kAll[j].name) return false;
    }
  }
  return true;
}
static_assert(unique_names(), "a key of the probe vocabulary is given twice");
constexpr report::Vocabulary kVocabulary{"probe", 1, kAll};

template <class T>
void optional_number(report::Writer& w, std::string_view key, const std::optional<T>& v) {
  if (v) w.number(key, static_cast<std::uint64_t>(*v));
}

void write_callee(report::Writer& w, const CalleeCode& c) {
  w.begin_object("callee_code");
  w.boolean("read", c.read);
  w.number("extent", c.extent);
  w.number("bytes", c.bytes);
  w.number("instructions", c.instructions);
  w.boolean("cut", c.cut);
  w.boolean("undecodable", c.undecodable);
  optional_number(w, "first_call", c.first_call);
  optional_number(w, "first_ret", c.first_ret);
  w.boolean("returns_early", c.returns_early);
  optional_number(w, "modelled_to", c.modelled_to);
  w.number("call_count", c.call_count);
  w.begin_array("call_sites");
  for (const CalleeCall& k : c.calls) {
    w.begin_object();
    w.number("offset", k.offset);
    w.word("form", kForms[static_cast<std::size_t>(k.form)]);
    optional_number(w, "target", k.target);
    optional_number(w, "base", k.base);
    optional_number(w, "index", k.index);
    if (k.form == CallForm::Memory) w.signed_number("disp", k.disp);
    optional_number(w, "vtable_of", k.vtable_of);
    w.end_object();
  }
  w.end_array();
  w.number("branch_count", c.branch_count);
  w.begin_array("branches");
  for (const CalleeBranch& b : c.branches) {
    w.begin_object();
    w.number("offset", b.offset);
    w.signed_number("to", b.to);
    w.end_object();
  }
  w.end_array();
  w.begin_array("args");
  for (const ArgUse& a : c.args) {
    w.begin_object();
    w.number("reg", a.reg);
    w.begin_array("stores");
    for (const ArgStore& s : a.stores) {
      w.begin_object();
      w.number("offset", s.offset);
      w.number("via", s.via);
      optional_number(w, "base", s.base);
      optional_number(w, "base_arg", s.base_arg);
      optional_number(w, "index", s.index);
      w.signed_number("disp", s.disp);
      w.boolean("rip", s.rip);
      w.number("width", s.width);
      w.end_object();
    }
    w.end_array();
    w.begin_array("saved");
    for (const std::uint8_t r : a.saved) w.number(r);
    w.end_array();
    w.begin_array("compares");
    for (const ArgCompare& k : a.compares) {
      w.begin_object();
      w.number("offset", k.offset);
      w.number("via", k.via);
      optional_number(w, "imm", k.imm);
      w.end_object();
    }
    w.end_array();
    w.number("compared_other", a.compared_other);
    w.number("tested", a.tested);
    w.end_object();
  }
  w.end_array();
  w.end_object();
}

void write_memory(report::Writer& w, const MemorySearch& m) {
  w.begin_object();
  w.number("at_ms", m.at_ms);
  w.number("ms", m.ms);
  w.number("regions", m.regions);
  w.number("read_regions", m.read_regions);
  w.number("read_bytes", m.read_bytes);
  w.number("unread_bytes", m.unread_bytes);
  w.number("stack_bytes", m.stack_bytes);
  w.number("faults", m.faults);
  w.word("stopped", kStops[static_cast<std::size_t>(m.stopped)]);
  w.boolean("own_known", m.own_known);
  w.number("found", m.found);
  w.number("inside", m.inside);
  w.number("own_found", m.own_found);
  w.number("own_inside", m.own_inside);
  w.begin_array("hits");
  for (const MemoryHit& h : m.hits) {
    w.begin_object();
    w.boolean("own", h.own);
    w.number("offset", h.offset);
    w.word("region", kRegions[static_cast<std::size_t>(h.region)]);
    w.number("protect", h.protect);
    if (h.module != ModuleKind::None) w.word("module", kModules[static_cast<std::size_t>(h.module) - 1]);
    if (h.section != SectionKind::None) w.word("section", kSections[static_cast<std::size_t>(h.section) - 1]);
    optional_number(w, "rva", h.rva);
    w.number("near_group", h.near_group);
    w.end_object();
  }
  w.end_array();
  w.end_object();
}

}  // namespace

const report::Vocabulary& vocabulary() noexcept { return kVocabulary; }

proto::Expected<std::string, report::Problem> write_report(const Findings& f) {
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
  for (const Check& c : f.checks) {
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
  for (const Base& b : f.bases) {
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
  for (const Rising& r : f.rising) {
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
  for (const Static& s : f.statics) {
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
  w.begin_array("hub_search");
  for (const HubSearch& h : f.hub_search) {
    w.begin_object();
    w.number("at_ms", h.at_ms);
    w.number("nodes", h.nodes);
    w.number("read_bytes", h.read_bytes);
    w.number("faults", h.faults);
    w.number("refused_reads", h.refused_reads);
    w.number("found", h.found);
    w.number("inside", h.inside);
    w.begin_array("hits");
    for (const HubHit& x : h.hits) {
      w.begin_object();
      w.number("depth", x.depth);
      w.begin_array("path");
      for (const std::uint32_t o : x.path) w.number(o);
      w.end_array();
      w.number("near_group", x.near_group);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }
  w.end_array();
  w.begin_object("names");
  w.number("of", f.names.of);
  w.number("agree", f.names.agree);
  w.number("qualified", f.names.qualified);
  if (f.names.offset) w.number("offset", *f.names.offset);
  w.number("inst_statics", f.names.inst_statics);
  w.number("inst_null", f.names.inst_null);
  w.number("inst_unread", f.names.inst_unread);
  w.number("inst_unclassed", f.names.inst_unclassed);
  w.number("inst_classed", f.names.inst_classed);
  w.number("inst_matched", f.names.inst_matched);
  w.begin_array("name_slots");
  for (const NameSlot& n : f.names.slots) {
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
  if (f.callee_code) write_callee(w, *f.callee_code);
  w.begin_array("memory_search");
  for (const MemorySearch& m : f.memory_search) write_memory(w, m);
  w.end_array();
  return w.finish();
}

}  // namespace en::gal::probe
