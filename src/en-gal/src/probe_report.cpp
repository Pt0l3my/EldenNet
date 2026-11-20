// R1's probe report (probe_report.hpp).
#include "en/gal/probe_report.hpp"

#include <algorithm>

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
// A call's form (tail: a direct jump out of the function; since spike.8 tail_memory and tail_register, jumps through
// memory or a register), and a flag test's operand's (spike.7). An operand's form is written by its own words below.
constexpr std::array<std::string_view, 8> kForms = {"direct", "register", "memory", "rip",
                                                    "tail", "tail_memory", "tail_register", "immediate"};
constexpr std::array<std::string_view, 4> kRegions = {"private", "mapped", "image", "other"};
constexpr std::array<std::string_view, 3> kModules = {"ours", "game", "other"};
constexpr std::array<std::string_view, 5> kSections = {"header", "code", "rdata", "data", "other"};
constexpr std::array<std::string_view, 3> kStops = {"done", "bytes_cap", "time_cap"};
constexpr std::array<std::string_view, 17> kConds = {"o", "no", "b",  "ae", "e", "ne", "be", "a", "s",
                                                     "ns", "p", "np", "l", "ge", "le", "g", "other"};
constexpr std::array<std::string_view, 4> kOps = {"cmp", "test", "other", "none"};
constexpr std::array<std::string_view, 3> kPoints = {"object", "vtable", "slot"};
// Spike.9: task_tagged, our task's address under a high-bit tag.
constexpr std::array<std::string_view, 10> kValues = {"zero", "small", "group", "task",  "vtable",
                                                      "ours", "game",  "heap",  "other", "task_tagged"};
// Spike.8 (WHY§240): what a register holds, an instruction's class, and the table's first shape that failed; spike.9's
// modified, a followed value changed.
constexpr std::array<std::string_view, 7> kSrcs = {"none", "argument", "first_word", "entry", "rip", "memory",
                                                   "modified"};
constexpr std::array<std::string_view, 21> kClasses = {"mov", "movx", "lea",   "cmp",  "test", "jcc", "jmp",
                                                       "call", "ret", "push", "pop", "xor",  "add", "sub",
                                                       "and", "or",  "shift", "imul", "nop",  "int3", "other"};
constexpr std::array<std::string_view, 5> kFails = {"wrapper_tail", "wrapper_rcx", "tail_summary", "tail_bound",
                                                    "tail_null_test"};

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
// Spike.7 (WHY§235): the arguments mapped into the callee's calls, its flag tests and inner callees; the vtable's and
// slots' search; the hub's memory compared across the call.
constexpr std::array<Key, 39> kSpike7 = {{
    {"reached", Type::Boolean},
    {"pass", Type::Array, Type::Object},
    {"from", Type::Unsigned},
    {"first_word", Type::Boolean},
    {"guard", Type::Array, Type::Object},
    {"cond", Type::Word, Type::Object, kConds},
    {"op", Type::Word, Type::Object, kOps},
    {"op_at", Type::Unsigned},
    {"operands", Type::Array, Type::Object},
    {"inner", Type::Array, Type::Object},
    {"site", Type::Unsigned},
    {"tail", Type::Boolean},
    {"committed_bytes", Type::Unsigned},
    {"vtable_found", Type::Unsigned},
    {"own_vtable_found", Type::Unsigned},
    {"slot_found", Type::Unsigned},
    {"own_slot_found", Type::Unsigned},
    {"slot_distinct", Type::Unsigned},
    {"own_slot_distinct", Type::Unsigned},
    {"points_to", Type::Word, Type::Object, kPoints},
    {"hub_diff", Type::Array, Type::Object},
    {"before_blocks", Type::Unsigned},
    {"after_blocks", Type::Unsigned},
    {"compared", Type::Unsigned},
    {"moved", Type::Unsigned},
    {"gone", Type::Unsigned},
    {"new_blocks", Type::Unsigned},
    {"new_bytes", Type::Unsigned},
    {"changed", Type::Unsigned},
    {"steady_words", Type::Unsigned},
    {"capped", Type::Boolean},
    {"kept_bytes", Type::Unsigned},
    {"words", Type::Array, Type::Object},
    {"old", Type::Object},
    {"new", Type::Object},
    {"steady", Type::Boolean},
    {"kind", Type::Word, Type::Object, kValues},
    {"fresh", Type::Boolean},
    {"group_half", Type::Boolean},
}};

// Spike.8 (WHY§240): the call's return value, the forms, the sources, the table, the control diff.
constexpr std::array<Key, 38> kSpike8 = {{
    {"returned", Type::Object},
    {"returned_low", Type::Unsigned},
    {"source", Type::Object},
    {"src", Type::Word, Type::Object, kSrcs},
    {"load_at", Type::Unsigned},
    {"scale", Type::Unsigned},
    {"base_src", Type::Object},
    {"index_src", Type::Object},
    {"vtable_src", Type::Object},
    {"pass_src", Type::Array, Type::Object},
    {"high", Type::Boolean},
    {"large", Type::Boolean},
    {"forms", Type::Array, Type::Object},
    {"forms_dropped", Type::Boolean},
    {"length", Type::Unsigned},
    {"class", Type::Word, Type::Object, kClasses},
    {"unreached", Type::Boolean},
    {"in_control", Type::Boolean},
    {"in_control_words", Type::Unsigned},
    {"hub_diff_control", Type::Object},
    {"table", Type::Object},
    {"matched", Type::Boolean},
    {"failed", Type::Word, Type::Object, kFails},
    {"singleton_slot", Type::Unsigned},
    {"hop", Type::Signed},
    {"array", Type::Signed},
    {"bound", Type::Unsigned},
    {"slot_disp", Type::Signed},
    {"before", Type::Object},
    {"after", Type::Object},
    {"singleton", Type::Object},
    {"is_instance", Type::Boolean},
    {"singleton_lead", Type::Unsigned},
    {"entries", Type::Unsigned},
    {"nonzero", Type::Unsigned},
    {"nonzero_at", Type::Array, Type::Unsigned},
    {"entry", Type::Object},
    {"entry_vtable", Type::Object},
}};
constexpr std::array<Key, 2> kSpike8More = {{
    {"slot_value", Type::Object},
    {"via_guard", Type::Boolean},
}};

// Spike.9 (WHY§248): the group's function's reads of our task and of its entry, their uses and the task's stores; the
// walks' roots; tagged pointers; the singleton's and the entry's classes against the leads.
constexpr std::array<std::string_view, 3> kRoots = {"task_hub", "singleton", "group_entry"};
constexpr std::array<std::string_view, 3> kMatches = {"none", "name", "imp"};
constexpr std::array<std::string_view, 9> kUses = {"compared", "compared_other", "tested", "stored", "passed",
                                                   "called",   "base",           "changed", "other"};
constexpr std::array<Key, 29> kSpike9 = {{
    {"root", Type::Word, Type::Object, kRoots},
    {"hub_diff_controls", Type::Array, Type::Object},
    {"tag", Type::Unsigned},
    {"tagged_found", Type::Unsigned},
    {"own_tagged_found", Type::Unsigned},
    {"vtable_tagged_found", Type::Unsigned},
    {"own_vtable_tagged_found", Type::Unsigned},
    {"tagged_hits", Type::Array, Type::Object},
    {"singleton_class", Type::Object},
    {"entry_class", Type::Object},
    {"vtable_known", Type::Boolean},
    {"classed", Type::Boolean},
    {"match", Type::Word, Type::Object, kMatches},
    {"lead", Type::Unsigned},
    {"parent", Type::Unsigned},
    {"entry_src", Type::Array, Type::Object},
    {"holds_entry", Type::Boolean},
    {"task_read_count", Type::Unsigned},
    {"task_reads", Type::Array, Type::Object},
    {"entry_read_count", Type::Unsigned},
    {"entry_reads", Type::Array, Type::Object},
    {"task_store_count", Type::Unsigned},
    {"task_stores", Type::Array, Type::Object},
    {"direct", Type::Boolean},
    {"use_count", Type::Unsigned},
    {"uses", Type::Array, Type::Object},
    {"use", Type::Word, Type::Object, kUses},
    {"origin", Type::Object},
    {"by", Type::Unsigned},
}};

constexpr std::size_t kKeyCount = kKeys.size() + kMore.size() + kSpike6.size() + kSpike7.size() + kSpike8.size() +
                                  kSpike8More.size() + kSpike9.size();
constexpr std::array<Key, kKeyCount> joined() {
  std::array<Key, kKeyCount> out{};
  std::size_t n = 0;
  for (const Key& k : kKeys) out[n++] = k;
  for (const Key& k : kMore) out[n++] = k;
  for (const Key& k : kSpike6) out[n++] = k;
  for (const Key& k : kSpike7) out[n++] = k;
  for (const Key& k : kSpike8) out[n++] = k;
  for (const Key& k : kSpike8More) out[n++] = k;
  for (const Key& k : kSpike9) out[n++] = k;
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

void write_pass(report::Writer& w, const std::vector<PassedArg>& pass) {
  w.begin_array("pass");
  for (const PassedArg& p : pass) {
    w.begin_object();
    w.number("reg", p.reg);
    w.number("from", p.from);
    w.boolean("first_word", p.first_word);
    w.end_object();
  }
  w.end_array();
}

// What a register holds (spike.8), nested to the source's depth.
void write_source(report::Writer& w, std::string_view key, const Source& s) {
  w.begin_object(key);
  w.word("src", kSrcs[static_cast<std::size_t>(s.kind)]);
  switch (s.kind) {
    case Source::Kind::Argument:
    case Source::Kind::FirstWord:
      w.number("from", s.from);
      break;
    case Source::Kind::Entry:
      w.number("reg", s.from);
      break;
    case Source::Kind::Rip:
      w.number("load_at", s.at);
      w.number("target", s.target);
      w.number("width", s.width);
      break;
    case Source::Kind::Memory:
      w.number("load_at", s.at);
      optional_number(w, "base", s.base);
      if (!s.base_src.empty()) write_source(w, "base_src", s.base_src.front());
      optional_number(w, "index", s.index);
      if (!s.index_src.empty()) write_source(w, "index_src", s.index_src.front());
      if (s.index) w.number("scale", s.scale);
      w.signed_number("disp", s.disp);
      w.number("width", s.width);
      break;
    case Source::Kind::Modified:
      w.number("op_at", s.at);
      w.word("class", kClasses[static_cast<std::size_t>(s.op)]);
      w.number("width", s.width);
      optional_number(w, "imm", s.imm);
      if (s.large) w.boolean("large", true);
      optional_number(w, "by", s.by);
      if (!s.origin.empty()) write_source(w, "origin", s.origin.front());
      break;
    case Source::Kind::None:
      break;
  }
  w.end_object();
}

// Spike.9: a field read of our task or of the group's entry, and what its value is used for.
void write_reads(report::Writer& w, std::string_view key, const std::vector<FieldRead>& reads) {
  w.begin_array(key);
  for (const FieldRead& r : reads) {
    w.begin_object();
    w.number("offset", r.offset);
    w.signed_number("disp", r.disp);
    w.number("width", r.width);
    w.number("base", r.base);
    write_source(w, "base_src", r.base_src);
    optional_number(w, "index", r.index);
    if (r.index) w.number("scale", r.scale);
    if (r.direct) w.boolean("direct", true);
    w.number("use_count", r.use_count);
    w.begin_array("uses");
    for (const FieldUse& u : r.uses) {
      w.begin_object();
      w.word("use", kUses[static_cast<std::size_t>(u.kind)]);
      w.number("offset", u.offset);
      optional_number(w, "imm", u.imm);
      if (u.large) w.boolean("large", true);
      optional_number(w, "reg", u.reg);
      optional_number(w, "target", u.target);
      if (u.kind == UseKind::Stored) {
        optional_number(w, "base", u.base);
        if (u.base_src) write_source(w, "base_src", *u.base_src);
        optional_number(w, "index", u.index);
        w.signed_number("disp", u.disp);
        if (u.rip) w.boolean("rip", true);
        w.number("width", u.width);
      }
      if (u.op) w.word("class", kClasses[static_cast<std::size_t>(*u.op)]);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }
  w.end_array();
}

void write_stores(report::Writer& w, const std::vector<TaskStore>& stores) {
  w.begin_array("task_stores");
  for (const TaskStore& s : stores) {
    w.begin_object();
    w.number("offset", s.offset);
    w.number("via", s.via);
    write_source(w, "source", s.value);
    optional_number(w, "base", s.base);
    if (s.base_src) write_source(w, "base_src", *s.base_src);
    optional_number(w, "index", s.index);
    w.signed_number("disp", s.disp);
    if (s.rip) w.boolean("rip", true);
    w.number("width", s.width);
    w.end_object();
  }
  w.end_array();
}

void write_operand(report::Writer& w, const Operand& o) {
  w.begin_object();
  switch (o.form) {
    case OperandForm::Register: w.word("form", "register"); break;
    case OperandForm::Memory: w.word("form", "memory"); break;
    case OperandForm::Rip: w.word("form", "rip"); break;
    case OperandForm::Immediate: w.word("form", "immediate"); break;
  }
  optional_number(w, "reg", o.reg);
  if (o.high) w.boolean("high", true);
  optional_number(w, "from", o.from);
  if (o.first_word) w.boolean("first_word", true);
  if (o.source) write_source(w, "source", *o.source);
  optional_number(w, "base", o.base);
  optional_number(w, "base_arg", o.base_arg);
  if (o.base_src) write_source(w, "base_src", *o.base_src);
  optional_number(w, "index", o.index);
  if (o.index && o.scale != 0) w.number("scale", o.scale);
  if (o.form == OperandForm::Memory) w.signed_number("disp", o.disp);
  optional_number(w, "target", o.target);
  if (o.section != SectionKind::None) w.word("section", kSections[static_cast<std::size_t>(o.section) - 1]);
  if (o.width != 0) w.number("width", o.width);
  optional_number(w, "imm", o.imm);
  if (o.large) w.boolean("large", true);
  w.end_object();
}

void write_forms(report::Writer& w, const CalleeCode& c) {
  if (c.forms_dropped) w.boolean("forms_dropped", true);
  w.begin_array("forms");
  for (const Form& f : c.forms) {
    w.begin_object();
    w.number("offset", f.offset);
    w.number("length", f.length);
    w.word("class", kClasses[static_cast<std::size_t>(f.cls)]);
    if (f.cls == InsnClass::Jcc) w.word("cond", kConds[std::min<std::size_t>(f.cond, kConds.size() - 1)]);
    if (f.to) w.signed_number("to", *f.to);
    if (f.unreached) w.boolean("unreached", true);
    if (!f.operands.empty()) {
      w.begin_array("operands");
      for (const Operand& o : f.operands) write_operand(w, o);
      w.end_array();
    }
    w.end_object();
  }
  w.end_array();
}

// A summary's fields, in the object the caller opened.
void write_code(report::Writer& w, const CalleeCode& c) {
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
    if (k.rip) w.boolean("rip", true);
    optional_number(w, "base", k.base);
    optional_number(w, "index", k.index);
    if ((k.form == CallForm::Memory || k.form == CallForm::TailMemory) && !k.rip) w.signed_number("disp", k.disp);
    optional_number(w, "vtable_of", k.vtable_of);
    if (k.base_src) write_source(w, "base_src", *k.base_src);
    if (k.vtable_src) write_source(w, "vtable_src", *k.vtable_src);
    w.boolean("reached", k.reached);
    write_pass(w, k.pass);
    if (!k.pass_src.empty()) {
      w.begin_array("pass_src");
      for (const PassedSource& p : k.pass_src) {
        w.begin_object();
        w.number("reg", p.reg);
        write_source(w, "source", p.source);
        w.end_object();
      }
      w.end_array();
    }
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
  w.begin_array("guard");
  for (const CalleeGuard& g : c.guards) {
    w.begin_object();
    w.number("offset", g.offset);
    w.signed_number("to", g.to);
    w.word("cond", kConds[std::min<std::size_t>(g.cond, kConds.size() - 1)]);
    w.word("op", kOps[static_cast<std::size_t>(g.op)]);
    optional_number(w, "op_at", g.op_at);
    if (g.width != 0) w.number("width", g.width);
    w.begin_array("operands");
    for (const Operand& o : g.operands) write_operand(w, o);
    w.end_array();
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
      w.boolean("first_word", s.first_word);
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
  // Spike.9: where there are any.
  if (c.task_read_count != 0) {
    w.number("task_read_count", c.task_read_count);
    write_reads(w, "task_reads", c.task_reads);
  }
  if (c.entry_read_count != 0) {
    w.number("entry_read_count", c.entry_read_count);
    write_reads(w, "entry_reads", c.entry_reads);
  }
  if (c.task_store_count != 0) {
    w.number("task_store_count", c.task_store_count);
    write_stores(w, c.task_stores);
  }
  write_forms(w, c);
}

void write_callee(report::Writer& w, const CalleeCode& c, const std::vector<InnerCallee>& inner) {
  w.begin_object("callee_code");
  write_code(w, c);
  w.begin_array("inner");
  for (const InnerCallee& i : inner) {
    w.begin_object();
    w.number("rva", i.rva);
    w.number("site", i.site);
    w.boolean("tail", i.tail);
    if (i.depth != 1) {
      w.number("depth", i.depth);
      w.number("parent", i.parent);
      w.begin_array("entry_src");
      for (const PassedSource& p : i.entry_src) {
        w.begin_object();
        w.number("reg", p.reg);
        write_source(w, "source", p.source);
        if (p.holds_entry) w.boolean("holds_entry", true);
        w.end_object();
      }
      w.end_array();
    }
    write_code(w, i.code);
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
  w.number("committed_bytes", m.committed_bytes);
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
  w.number("vtable_found", m.vtable_found);
  w.number("own_vtable_found", m.own_vtable_found);
  w.number("slot_found", m.slot_found);
  w.number("own_slot_found", m.own_slot_found);
  w.number("slot_distinct", m.slot_distinct);
  w.number("own_slot_distinct", m.own_slot_distinct);
  const auto hits = [&](std::string_view key, const std::vector<MemoryHit>& list) {
    w.begin_array(key);
    for (const MemoryHit& h : list) {
      w.begin_object();
      w.boolean("own", h.own);
      w.word("points_to", kPoints[static_cast<std::size_t>(h.points_to)]);
      optional_number(w, "slot", h.slot);
      w.number("offset", h.offset);
      optional_number(w, "tag", h.tag);
      w.word("region", kRegions[static_cast<std::size_t>(h.region)]);
      w.number("protect", h.protect);
      if (h.module != ModuleKind::None) w.word("module", kModules[static_cast<std::size_t>(h.module) - 1]);
      if (h.section != SectionKind::None) w.word("section", kSections[static_cast<std::size_t>(h.section) - 1]);
      optional_number(w, "rva", h.rva);
      w.number("near_group", h.near_group);
      w.end_object();
    }
    w.end_array();
  };
  hits("hits", m.hits);
  // Spike.9: our object's and our vtable's addresses under a high-bit tag.
  w.number("tagged_found", m.tagged_found);
  w.number("own_tagged_found", m.own_tagged_found);
  w.number("vtable_tagged_found", m.vtable_tagged_found);
  w.number("own_vtable_tagged_found", m.own_vtable_tagged_found);
  hits("tagged_hits", m.tagged_hits);
  w.end_object();
}

void write_value(report::Writer& w, std::string_view key, const WordValue& v) {
  w.begin_object(key);
  w.word("kind", kValues[static_cast<std::size_t>(v.kind)]);
  optional_number(w, "value", v.value);
  optional_number(w, "offset", v.offset);
  optional_number(w, "tag", v.tag);
  if (v.section != SectionKind::None) w.word("section", kSections[static_cast<std::size_t>(v.section) - 1]);
  optional_number(w, "rva", v.rva);
  if (v.fresh) w.boolean("fresh", *v.fresh);
  if (v.group_half) w.boolean("group_half", true);
  w.end_object();
}

void write_diff(report::Writer& w, const HubDiff& d, std::string_view key = {}) {
  w.begin_object(key);
  if (!d.root.empty()) w.word("root", d.root);
  w.number("at_ms", d.at_ms);
  w.number("before_blocks", d.before_blocks);
  w.number("after_blocks", d.after_blocks);
  w.number("compared", d.compared);
  w.number("moved", d.moved);
  w.number("gone", d.gone);
  w.number("new_blocks", d.new_blocks);
  w.number("new_bytes", d.new_bytes);
  w.number("changed", d.changed);
  optional_number(w, "steady_words", d.steady_words);
  w.boolean("capped", d.capped);
  w.number("kept_bytes", d.kept_bytes);
  optional_number(w, "in_control_words", d.in_control_words);
  w.begin_array("words");
  for (const DiffWord& x : d.words) {
    w.begin_object();
    w.number("depth", x.depth);
    w.begin_array("path");
    for (const std::uint32_t o : x.path) w.number(o);
    w.end_array();
    write_value(w, "old", x.before);
    write_value(w, "new", x.after);
    if (x.steady) w.boolean("steady", *x.steady);
    if (x.in_control) w.boolean("in_control", *x.in_control);
    w.end_object();
  }
  w.end_array();
  w.end_object();
}

void write_read(report::Writer& w, std::string_view key, const TableRead& r) {
  w.begin_object(key);
  w.number("at_ms", r.at_ms);
  w.boolean("read", r.read);
  if (r.singleton) write_value(w, "singleton", *r.singleton);
  w.boolean("is_instance", r.is_instance);
  optional_number(w, "singleton_lead", r.singleton_lead);
  w.number("entries", r.entries);
  w.number("nonzero", r.nonzero);
  w.begin_array("nonzero_at");
  for (const std::uint32_t i : r.nonzero_at) w.number(i);
  w.end_array();
  if (r.entry) write_value(w, "entry", *r.entry);
  if (r.entry_vtable) write_value(w, "entry_vtable", *r.entry_vtable);
  if (r.slot_value) write_value(w, "slot_value", *r.slot_value);
  w.end_object();
}

void write_table(report::Writer& w, const Table& t) {
  w.begin_object("table");
  w.boolean("matched", t.matched);
  if (!t.matched) {
    if (!t.failed.empty()) w.word("failed", t.failed);
    w.end_object();
    return;
  }
  w.number("singleton_slot", t.singleton_slot);
  if (t.hop) w.signed_number("hop", *t.hop);
  if (t.via_guard) w.boolean("via_guard", true);
  if (t.array) w.signed_number("array", *t.array);
  w.signed_number("disp", t.disp);
  w.number("bound", t.bound);
  w.number("rva", t.rva);
  optional_number(w, "site", t.site);
  if (t.slot_disp) w.signed_number("slot_disp", *t.slot_disp);
  optional_number(w, "target", t.target);
  if (t.before) write_read(w, "before", *t.before);
  if (t.after) write_read(w, "after", *t.after);
  const auto cls = [&](std::string_view key, const ClassMatch& m) {
    w.begin_object(key);
    w.boolean("vtable_known", m.vtable);
    w.boolean("classed", m.classed);
    if (m.classed) w.number("type_descriptor", m.type_descriptor);
    w.word("match", kMatches[static_cast<std::size_t>(m.match)]);
    optional_number(w, "lead", m.lead);
    w.end_object();
  };
  if (t.singleton_class) cls("singleton_class", *t.singleton_class);
  if (t.entry_class) cls("entry_class", *t.entry_class);
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
  if (f.returned) write_value(w, "returned", *f.returned);
  optional_number(w, "returned_low", f.returned_low);
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
  if (f.callee_code) write_callee(w, *f.callee_code, f.callee_inner);
  w.begin_array("memory_search");
  for (const MemorySearch& m : f.memory_search) write_memory(w, m);
  w.end_array();
  w.begin_array("hub_diff");
  for (const HubDiff& d : f.hub_diff) write_diff(w, d);
  w.end_array();
  if (f.hub_diff_control) write_diff(w, *f.hub_diff_control, "hub_diff_control");
  if (!f.hub_diff_controls.empty()) {
    w.begin_array("hub_diff_controls");
    for (const HubDiff& d : f.hub_diff_controls) write_diff(w, d);
    w.end_array();
  }
  if (f.table) write_table(w, *f.table);
  return w.finish();
}

}  // namespace en::gal::probe
