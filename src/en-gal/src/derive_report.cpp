// derive.json (derive.hpp): the derivation's result through the numbers-only writer (report.hpp). Its vocabulary is
// closed: fixed keys, fixed words, the leads' labels, numbers and patterns of at most 64 bytes. Section names outside
// the usual ones are written as "other", and no name, string or path read from the executable has a key to go under.
//
// The vocabulary is schema 1's as DataPiler 0.2.41's guard holds a copy of it (its derive.py): a key or a word added
// here would make that guard refuse the whole report. So the diagnostics R0c added (under "stats", and a few counts
// beside the results) use schema 1's keys only, nested where they say what they count; src/en-tools/README.md lists
// what each means in its place. A new key waits for a schema 2 and a DataPiler that reads it.
#include <array>
#include <string_view>

#include "en/gal/derive.hpp"

namespace en::gal::derive {

namespace {

using report::Key;
using report::Type;
using report::Writer;

constexpr std::array<std::string_view, 7> kStatus = {"derived",    "ambiguous",  "refused", "missing",
                                                     "not_unique", "needs_slot", "depends"};
constexpr std::array<std::string_view, 9> kRefusal = {"unreadable",    "static_section", "static_alignment",
                                                      "class_section", "getter_section", "getter_not_function",
                                                      "no_path",       "path_section",   "path_text"};
constexpr std::array<std::string_view, 4> kRegion = {"ww", "jp", "other", "none"};
constexpr std::array<std::string_view, 4> kProduct = {"matches", "differs", "absent", "not_given"};
constexpr std::array<std::string_view, 20> kSection = {
    ".text", ".rdata", ".data",  ".pdata", ".xdata", ".rsrc",    ".reloc",  ".tls",   ".idata",   ".edata",
    ".bss",  ".CRT",   "_RDATA", ".00cfg", ".gfids", ".retplne", ".voltbl", ".didat", ".buildid", "other"};
constexpr std::array<std::string_view, 4> kCapture = {"static", "class", "getter", "path"};
constexpr std::array<std::string_view, 2> kEncoding = {"ascii", "utf16"};
constexpr std::array<std::string_view, 8> kRegister = {"rcx", "rdx", "r8", "r9", "ecx", "edx", "r8d", "r9d"};
constexpr std::array<std::string_view, 2> kSource = {"pointer_table", "code_reference"};
constexpr std::array<std::string_view, 2> kKind = {"class", "module"};
constexpr std::array<std::string_view, 12> kItem = {"name_layout",
                                                    "singleton_instance",
                                                    "vtable_set",
                                                    "register_task",
                                                    "register_abi",
                                                    "task_group_index",
                                                    "task_run_slot",
                                                    "task_object_size",
                                                    "static_slot_instance",
                                                    "static_slot_module_base",
                                                    "static_pattern_in_memory",
                                                    "layout_fields"};

constexpr Key U(std::string_view name) { return Key{name, Type::Unsigned}; }
constexpr Key B(std::string_view name) { return Key{name, Type::Boolean}; }
constexpr Key O(std::string_view name) { return Key{name, Type::Object}; }
constexpr Key AO(std::string_view name) { return Key{name, Type::Array, Type::Object}; }
constexpr Key AU(std::string_view name) { return Key{name, Type::Array, Type::Unsigned}; }
template <std::size_t N>
constexpr Key W(std::string_view name, const std::array<std::string_view, N>& words) {
  return Key{name, Type::Word, Type::Object, words};
}

constexpr auto kKeys = std::to_array<Key>({
    // The top
    B("complete"),
    O("image"),
    O("fd4"),
    O("rtti"),
    O("register"),
    O("group"),
    AO("statics"),
    AO("confirm"),
    O("stats"),
    // The image
    U("file_size"),
    U("image_base"),
    U("entry_point"),
    U("size_of_image"),
    U("size_of_headers"),
    U("section_alignment"),
    U("file_alignment"),
    U("time_date_stamp"),
    U("characteristics"),
    U("dll_characteristics"),
    U("subsystem"),
    U("functions"),
    U("fixups"),
    AU("file_version"),
    AU("product_version"),
    W("region", kRegion),
    W("product_name", kProduct),
    AU("translation"),
    U("resource_language"),
    AO("sections"),
    W("section", kSection),
    U("rva"),
    U("virtual_size"),
    U("raw_size"),
    // The FD4 finder
    W("status", kStatus),
    U("path_strings"),
    U("path_references"),
    U("sites"),
    U("shapes"),
    U("sites_used"),
    O("idiom"),
    Key{"bytes", Type::Pattern},
    U("length"),
    U("fixed"),
    U("hits"),
    U("sites_matched"),
    AO("captures"),
    W("capture", kCapture),
    U("at"),
    U("insn_end"),
    U("align"),
    B("function"),
    B("scan_resolved"),
    U("scan_hits"),
    U("scan_rejected"),
    O("names"),
    U("offset"),
    W("encoding", kEncoding),
    U("agree"),
    U("of"),
    U("accepted"),
    O("refused"),
    U("unreadable"),
    U("static_section"),
    U("static_alignment"),
    U("class_section"),
    U("getter_section"),
    U("getter_not_function"),
    U("no_path"),
    U("path_section"),
    U("path_text"),
    U("unnamed"),
    U("unlisted"),
    U("unlisted_ambiguous"),
    AO("singletons"),
    Key{"label", Type::Label},
    U("static"),
    U("class"),
    U("getter"),
    W("refusal", kRefusal),
    // RTTI
    U("locators"),
    U("vtables_found"),
    AO("sets"),
    U("type_descriptor"),
    U("classes"),
    AU("primary"),
    AU("subobject"),
    O("task"),
    U("base_vtable"),
    U("base_slots"),
    AU("vtables"),
    U("slots_min"),
    U("slots_max"),
    AU("slot_targets"),
    U("run_slot"),
    // The registration
    U("instance_static"),
    U("loads"),
    U("calls"),
    AO("candidates"),
    U("callee"),
    W("instance_register", kRegister),
    W("task_register", kRegister),
    W("group_register", kRegister),
    B("shaped"),
    B("agrees"),
    AU("callers"),
    O("pattern"),
    U("hit"),
    U("entry"),
    U("anchor"),
    U("fn_length"),
    U("matches"),
    U("capture_at"),
    U("capture_insn_end"),
    U("target"),
    // The group
    U("strings"),
    U("index"),
    W("source", kSource),
    U("entries"),
    // The statics and the list for R1
    W("kind", kKind),
    U("vtable"),
    U("slots"),
    W("rtti_status", kStatus),
    U("slot"),
    U("references"),
    W("item", kItem),
    U("value"),
    // The counts
    U("instructions"),
    U("undecodable"),
});

constexpr report::Vocabulary kVocabulary{"derive", 1, kKeys};

std::string_view section_word(std::string_view name) {
  for (const std::string_view w : kSection) {
    if (w == name) return w;
  }
  return "other";
}

std::string_view register_word(std::uint8_t reg, bool narrow) {
  switch (reg) {
    case 1: return narrow ? "ecx" : "rcx";
    case 2: return narrow ? "edx" : "rdx";
    case 8: return narrow ? "r8d" : "r8";
    case 9: return narrow ? "r9d" : "r9";
    default: return "";
  }
}

std::string_view refusal_word(fd4::Refusal r) {
  const auto i = static_cast<std::size_t>(r);
  return i < kRefusal.size() ? kRefusal[i] : "unreadable";
}

void write_refused(Writer& w, const std::array<std::uint64_t, 9>& refused) {
  w.begin_object("refused");
  for (std::size_t i = 0; i < refused.size() && i < kRefusal.size(); ++i) w.number(kRefusal[i], refused[i]);
  w.end_object();
}

void write_cut(Writer& w, const Cut& c) {
  w.begin_object("pattern");
  w.pattern("bytes", c.bytes);
  w.number("length", c.bytes.size());
  w.number("fixed", c.bytes.fixed());
  w.number("hit", c.hit);
  w.number("entry", c.entry);
  w.number("anchor", c.hit - c.entry);
  w.number("fn_length", c.length);
  w.number("matches", c.matches);
  if (c.capture) {
    w.number("capture_at", c.capture->at);
    w.number("capture_insn_end", c.capture->insn_end);
    w.number("target", c.target);
  }
  w.boolean("scan_resolved", c.scan_resolved);
  w.end_object();
}

void write_numbers(Writer& w, std::string_view key, const std::vector<std::uint32_t>& values) {
  w.begin_array(key);
  for (const std::uint32_t v : values) w.number(v);
  w.end_array();
}

void write_image(Writer& w, const Result& r) {
  const Fingerprint& f = r.fingerprint;
  w.begin_object("image");
  w.number("file_size", r.file_size);
  w.number("image_base", f.image_base);
  w.number("entry_point", f.entry_point);
  w.number("size_of_image", f.size_of_image);
  w.number("size_of_headers", f.size_of_headers);
  w.number("section_alignment", f.section_alignment);
  w.number("file_alignment", f.file_alignment);
  w.number("time_date_stamp", f.time_date_stamp);
  w.number("characteristics", f.characteristics);
  w.number("dll_characteristics", f.dll_characteristics);
  w.number("subsystem", f.subsystem);
  w.number("functions", f.functions);
  w.number("fixups", r.stats.fixups);
  if (f.has_version) {
    w.begin_array("file_version");
    for (const std::uint16_t v : f.file_version) w.number(v);
    w.end_array();
    w.begin_array("product_version");
    for (const std::uint16_t v : f.product_version) w.number(v);
    w.end_array();
  }
  w.word("region", f.region);
  w.word("product_name", f.product_name);
  if (f.product_name_characters != 0) {
    // The ProductName's characters, and those left once symbols are removed: fewer means it matched without them.
    w.begin_object("names");
    w.number("of", f.product_name_characters);
    w.number("agree", f.product_name_kept);
    w.end_object();
  }
  if (f.translation) {
    w.begin_array("translation");
    w.number(f.translation->language);
    w.number(f.translation->codepage);
    w.end_array();
    w.number("resource_language", f.resource_language);
  }
  w.begin_array("sections");
  for (const Section& s : f.sections) {
    w.begin_object();
    w.word("section", section_word(s.name));
    w.number("rva", s.rva);
    w.number("virtual_size", s.virtual_size);
    w.number("raw_size", s.raw_size);
    w.number("characteristics", s.characteristics);
    w.end_object();
  }
  w.end_array();
  w.end_object();
}

void write_fd4(Writer& w, const Fd4Result& r) {
  w.begin_object("fd4");
  w.word("status", to_string(r.status));
  w.number("path_strings", r.path_strings);
  w.number("path_references", r.path_references);
  w.number("sites", r.sites);
  w.number("shapes", r.shapes);
  w.number("sites_used", r.sites_used);
  if (r.idiom) {
    const fd4::Idiom& i = *r.idiom;
    w.begin_object("idiom");
    w.pattern("bytes", i.bytes);
    w.number("length", i.bytes.size());
    w.number("fixed", i.bytes.fixed());
    w.number("hits", r.idiom_hits);
    w.number("sites_matched", r.sites_matched);
    w.begin_array("captures");
    const auto capture = [&](std::string_view name, const Capture& c, std::string_view section, std::uint32_t align,
                             bool function) {
      w.begin_object();
      w.word("capture", name);
      w.number("at", c.at);
      w.number("insn_end", c.insn_end);
      w.word("section", section);
      w.number("align", align);
      w.boolean("function", function);
      w.end_object();
    };
    capture("static", i.static_slot, ".data", 8, false);
    capture("class", i.class_object, ".data", 1, false);
    capture("getter", i.getter, ".text", 1, true);
    if (i.path) capture("path", *i.path, ".rdata", 1, false);
    w.end_array();
    w.boolean("scan_resolved", r.scan_resolved);
    w.number("scan_hits", r.scan_hits);
    w.number("scan_rejected", r.scan_rejected);
    w.end_object();
  }
  if (r.names) {
    w.begin_object("names");
    w.number("offset", r.names->offset);
    w.word("encoding", r.names->encoding == fd4::NameEncoding::Ascii ? "ascii" : "utf16");
    w.number("agree", r.names_agree);
    w.number("of", r.names_of);
    w.end_object();
  }
  w.number("accepted", r.accepted);
  write_refused(w, r.refused);
  w.number("unnamed", r.unnamed);
  w.number("unlisted", r.unlisted);
  w.number("unlisted_ambiguous", r.unlisted_ambiguous);
  w.begin_array("singletons");
  for (const SingletonResult& s : r.singletons) {
    w.begin_object();
    w.label("label", s.label);
    w.word("status", to_string(s.status));
    if (s.status == Status::Derived || s.status == Status::Ambiguous || s.status == Status::Refused) {
      w.number("static", s.static_slot);
      w.number("class", s.class_object);
    }
    if (s.status == Status::Derived || s.status == Status::Ambiguous) {
      w.number("getter", s.getter);
      w.number("sites", s.sites);
    }
    if (s.refusal) w.word("refusal", refusal_word(*s.refusal));
    w.end_object();
  }
  w.end_array();
  w.end_object();
}

void write_rtti(Writer& w, const Result& r) {
  w.begin_object("rtti");
  w.number("locators", r.stats.locators);
  w.number("vtables_found", r.stats.vtables);
  w.begin_array("sets");
  for (const VtableSetResult& s : r.sets) {
    w.begin_object();
    w.label("label", s.label);
    w.word("status", to_string(s.status));
    if (s.type_descriptor != 0) w.number("type_descriptor", s.type_descriptor);
    w.number("classes", s.classes);
    write_numbers(w, "primary", s.primary);
    write_numbers(w, "subobject", s.subobject);
    w.end_object();
  }
  w.end_array();
  const TaskResult& t = r.task;
  w.begin_object("task");
  w.word("status", to_string(t.status));
  if (t.base.type_descriptor != 0) w.number("type_descriptor", t.base.type_descriptor);
  if (t.base.vtable != 0) {
    w.number("base_vtable", t.base.vtable);
    w.number("base_slots", t.base.slots);
  }
  w.number("classes", t.vtables.size());
  write_numbers(w, "vtables", t.vtables);
  w.number("slots_min", t.slots_min);
  w.number("slots_max", t.slots_max);
  write_numbers(w, "slot_targets", t.slot_targets);
  if (t.run_slot) w.number("run_slot", *t.run_slot);
  w.end_object();
  w.end_object();
}

void write_register(Writer& w, const RegisterResult& r) {
  w.begin_object("register");
  w.word("status", to_string(r.status));
  if (r.instance_static != 0) w.number("instance_static", r.instance_static);
  w.number("loads", r.loads);
  w.number("calls", r.calls);
  w.begin_array("candidates");
  for (const RegisterCandidate& c : r.candidates) {
    w.begin_object();
    w.number("callee", c.callee);
    w.number("sites", c.sites);
    if (c.abi.instance) w.word("instance_register", register_word(*c.abi.instance, false));
    if (c.abi.task) w.word("task_register", register_word(*c.abi.task, false));
    if (c.abi.group) w.word("group_register", register_word(*c.abi.group, true));
    w.boolean("shaped", c.abi.shaped);
    w.boolean("agrees", c.abi_agrees);
    w.boolean("function", c.function);
    write_numbers(w, "callers", c.callers);
    w.end_object();
  }
  w.end_array();
  if (r.pattern) write_cut(w, *r.pattern);
  w.end_object();
}

void write_group(Writer& w, const GroupResult& g) {
  w.begin_object("group");
  w.word("status", to_string(g.status));
  w.number("strings", g.strings);
  w.number("references", g.references);
  w.number("matches", g.pointers);
  w.begin_array("candidates");
  for (const GroupCandidate& c : g.candidates) {
    w.begin_object();
    w.number("index", c.index);
    w.word("source", c.source);
    w.number("at", c.at);
    if (c.entries != 0) w.number("entries", c.entries);
    if (c.stride != 0) w.number("length", c.stride);
    if (c.name_offset) w.number("offset", *c.name_offset);
    w.word("encoding", c.wide ? "utf16" : "ascii");
    if (c.stored_index) w.number("value", *c.stored_index);
    w.end_object();
  }
  w.end_array();
  w.end_object();
}

void write_statics(Writer& w, const std::vector<StaticResult>& statics) {
  w.begin_array("statics");
  for (const StaticResult& s : statics) {
    w.begin_object();
    w.label("label", s.label);
    w.word("kind", s.module ? "module" : "class");
    w.word("status", to_string(s.status));
    if (!s.module) {
      w.word("rtti_status", to_string(s.rtti.status));
      if (s.rtti.type_descriptor != 0) w.number("type_descriptor", s.rtti.type_descriptor);
      if (s.rtti.vtable != 0) {
        w.number("vtable", s.rtti.vtable);
        w.number("slots", s.rtti.slots);
      }
    }
    if (s.slot) {
      w.number("slot", *s.slot);
      w.number("references", s.references);
    }
    if (s.pattern) write_cut(w, *s.pattern);
    if (!s.module) w.number("matches", s.same_name);
    w.end_object();
  }
  w.end_array();
}

// One histogram of path loads: each bucket's place from the site's start (or, hoisted, its distance before it), its
// length, the other references into .rdata before it (index), the register it writes (slot), its opcode (value).
void write_buckets(Writer& w, std::string_view key, const std::vector<PathBucket>& buckets, std::string_view count) {
  w.begin_array(key);
  for (const PathBucket& b : buckets) {
    w.begin_object();
    w.word("capture", "path");
    if (b.hoisted) {
      w.number("anchor", b.before);
    } else {
      w.number("at", b.at);
      w.number("insn_end", b.insn_end);
    }
    w.number("length", b.length);
    w.number("index", b.ordinal);
    w.number("slot", b.reg);
    w.number("value", b.opcode);
    w.number(count, b.count);
    w.end_object();
  }
  w.end_array();
}

// The FD4 diagnostics, under stats.fd4.
void write_fd4_diagnostics(Writer& w, const Fd4Result& r) {
  const Fd4Diagnostics& g = r.diagnostics;
  w.begin_object("fd4");
  w.number("path_references", r.path_references);
  w.number("sites", r.sites);
  w.begin_array("candidates");  // per reference to a path, what the site search found, by SiteOutcome
  for (std::size_t i = 0; i < g.outcomes.size(); ++i) {
    w.begin_object();
    w.number("index", i);
    w.number("references", g.outcomes[i]);
    w.end_object();
  }
  w.end_array();
  w.begin_array("sets");  // the head shapes, largest first: their sites, and how many the idiom matches
  for (std::size_t i = 0; i < g.heads.size(); ++i) {
    w.begin_object();
    w.number("index", i);
    w.number("sites", g.heads[i].first);
    w.number("sites_matched", g.heads[i].second);
    w.end_object();
  }
  w.end_array();
  write_buckets(w, "captures", g.site_paths, "sites");
  w.begin_object("idiom");
  w.number("hits", r.idiom_hits);
  write_buckets(w, "captures", g.hit_paths, "hits");
  w.end_object();
  w.number("accepted", g.first_reference_accepted);
  write_refused(w, g.first_reference_refused);
  w.begin_object("names");
  w.number("of", g.class_objects);
  w.number("agree", g.names_best);
  if (g.best_names && g.names_best != 0) {
    w.number("offset", g.best_names->offset);
    w.word("encoding", g.best_names->encoding == fd4::NameEncoding::Ascii ? "ascii" : "utf16");
  }
  w.number("fixups", g.class_fixups);
  w.number("unreadable", g.class_beyond_file);
  w.end_object();
  w.number("classes", g.class_objects);
  w.number("loads", g.statics);
  w.number("calls", g.getters);
  w.end_object();
}

void write_census(Writer& w, const std::vector<SectionCensus>& sections) {
  w.begin_array("sections");
  for (const SectionCensus& k : sections) {
    w.begin_object();
    w.word("section", section_word(k.name));
    w.number("rva", k.rva);
    w.number("virtual_size", k.virtual_size);
    w.number("instructions", k.instructions);
    w.number("undecodable", k.undecodable);
    w.number("entries", k.runs);
    w.number("hits", k.runs_after_stop);
    w.number("matches", k.runs_at_escape);
    w.number("functions", k.in_functions);
    w.number("path_references", k.path_references);
    w.end_object();
  }
  w.end_array();
}

void write_confirm(Writer& w, const std::vector<ConfirmItem>& items) {
  w.begin_array("confirm");
  for (const ConfirmItem& c : items) {
    w.begin_object();
    w.word("item", c.item);
    if (!c.label.empty()) w.label("label", c.label);
    if (c.value) w.number("value", *c.value);
    w.end_object();
  }
  w.end_array();
}

}  // namespace

const report::Vocabulary& vocabulary() noexcept { return kVocabulary; }

proto::Expected<std::string, report::Problem> write_report(const Result& result) {
  Writer w(kVocabulary);
  w.boolean("complete", complete(result));
  write_image(w, result);
  write_fd4(w, result.fd4);
  write_rtti(w, result);
  write_register(w, result.registration);
  write_group(w, result.group);
  write_statics(w, result.statics);
  write_confirm(w, result.confirm);
  w.begin_object("stats");
  w.number("instructions", result.stats.instructions);
  w.number("undecodable", result.stats.undecodable);
  w.number("fixups", result.stats.fixups);
  w.number("locators", result.stats.locators);
  w.number("vtables_found", result.stats.vtables);
  write_census(w, result.stats.sections);
  write_fd4_diagnostics(w, result.fd4);
  w.end_object();
  return w.finish();
}

}  // namespace en::gal::derive
