// --describe: the fake game's own account of every invented fact that discovery and the layouts are to find (ARCH §6.1
// to §6.4), as one JSON object on standard output: the fingerprint, the FD4-style sites, the AOB items with their
// patterns, the registration's ABI and task shape, the classes' vtables, the field offsets and the enums. The truth
// against which the derivation kit and the GAL's tests compare their answers, and which the harness checks against
// the file from outside. It also checks its own patterns against its own code in memory ("self_check").
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include <nlohmann/json.hpp>

#include "program.hpp"

extern "C" {
struct FgSiteRow {
  const std::uint8_t* site;
  void* const* statik;  // null for the decoy that has none
  const std::uint8_t* rtclass;
  const char* path;
  const std::uint8_t* path_load;  // the instruction that loads the path
  std::uint64_t kind;             // PathKind
};
extern const FgSiteRow fg_fd4_site_table[];
extern const std::uint8_t* const fg_register_call_table[];
}

namespace fakegame {
namespace {

using nlohmann::json;

// SAFETY: addresses of this module's own code and data, turned into numbers relative to its base.
std::uint64_t base() { return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)); }
std::uint64_t rva(const void* p) { return reinterpret_cast<std::uintptr_t>(p) - base(); }
template <class F>
std::uint64_t fn_rva(F* fn) {
  void* p = nullptr;
  std::memcpy(&p, &fn, sizeof p);
  return rva(p);
}
template <class F>
const std::uint8_t* fn_bytes(F* fn) {
  const std::uint8_t* p = nullptr;
  std::memcpy(&p, &fn, sizeof p);
  return p;
}
std::string hex(std::uint64_t v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "0x%llX", static_cast<unsigned long long>(v));
  return buf;
}
// SAFETY: the distance from an object of ours to one of its own members, as a number.
template <class Obj, class Member>
std::uint64_t off(const Obj* obj, const Member* member) {
  return static_cast<std::uint64_t>(reinterpret_cast<const std::byte*>(member) - reinterpret_cast<const std::byte*>(obj));
}

// The idiom every FD4-style site shares (asm.cpp): mov reg,[rip+static] (rbx, rdi, rsi or r12); test reg,reg; jnz;
// lea rcx,[rip+class]; call getter. The path is loaded after it, in ways of its own (PathKind).
constexpr const char* kIdiom = "?? 8B ?? ?? ?? ?? ?? ?? 85 ?? 75 ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ??";

// How a site loads its path (asm.cpp's site table): the register it writes, its opcode as x86.hpp gives it (map * 256 +
// the opcode byte), its length, the other loads into .rdata before it, and whether it comes before the head.
enum PathKind : std::uint64_t {
  kPathAfter = 1,     // lea rcx, [rip+path] after the getter's call (the first sites' shape)
  kPathFormat = 2,    // a format string loaded first, then lea rcx, [rip+path]
  kPathRdx = 3,       // lea rdx, [rip+path] right after the getter's call
  kPathPointer = 4,   // mov rcx, [rip+slot], a pointer to the path
  kPathAbsolute = 5,  // mov rcx, imm64, the path's address
  kPathHoisted = 6,   // lea rsi, [rip+path] before the head
  kNotIdiom = 7,      // the decoy: its "static" is read through rsi
};
struct PathLoadShape {
  int reg;
  int opcode;
  int length;
  int ordinal;
  bool hoisted;
  const char* form;
};
PathLoadShape shape_of(std::uint64_t kind) {
  switch (kind) {
    case kPathFormat: return {1, 0x8D, 7, 1, false, "address"};
    case kPathRdx: return {2, 0x8D, 7, 0, false, "address"};
    case kPathPointer: return {1, 0x8B, 7, 0, false, "pointer"};
    case kPathAbsolute: return {1, 0xB9, 10, 0, false, "absolute"};
    case kPathHoisted: return {6, 0x8D, 7, 0, true, "address"};
    default: return {1, 0x8D, 7, 0, false, "address"};
  }
}

struct AobItem {
  const char* role;
  const char* kind;  // "static" or "function"
  const std::uint8_t* fn;
  std::uint32_t anchor;
  const char* pattern;
  std::uint32_t capture_at;  // 0 for a function
  std::uint32_t insn_end;
  const void* target;  // the static; null for a function
  const char* prologue;
};

// Compares the pattern's fixed bytes with the code at `at`.
bool matches(const std::uint8_t* at, const char* pattern) {
  std::size_t i = 0;
  for (const char* p = pattern; *p != '\0';) {
    while (*p == ' ') ++p;
    if (*p == '\0') break;
    if (p[0] == '?' && p[1] == '?') {
      p += 2;
      ++i;
      continue;
    }
    const unsigned long byte = std::strtoul(std::string(p, 2).c_str(), nullptr, 16);
    if (at[i] != byte) return false;
    p += 2;
    ++i;
  }
  return true;
}

std::int32_t disp_at(const std::uint8_t* at) {
  std::int32_t d = 0;
  std::memcpy(&d, at, sizeof d);
  return d;
}

json pdata_of(const void* code) {
  DWORD64 image = 0;
  // SAFETY: an address inside this module's own code.
  const RUNTIME_FUNCTION* fn = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(code), &image, nullptr);
  if (fn == nullptr) return nullptr;
  return json{{"begin", fn->BeginAddress}, {"end", fn->EndAddress}};
}

const char* role_of(const std::string& name) {
  static const char* const kRoles[][2] = {
      {"TaskLoom", "CSTask"},          {"ActorRoster", "WorldChrMan"},  {"LobbyWarden", "CSSessionManager"},
      {"MarkLedger", "CSEventFlagMan"}, {"GaugeBoard", "CSFeMan"},      {"MenuDesk", "CSMenuMan"},
      {"VeilKeeper", "CSNowLoadingHelper"}, {"SkyClock", "WorldAreaTime"}, {"EyeRig", "CSCamera"},
      {"TableVault", "SoloParamRepository"}, {"PhraseBook", "MsgRepository"}};
  for (const auto& r : kRoles) {
    if (name == r[0]) return r[1];
  }
  return nullptr;
}

const char* rtti_of(const std::string& name) {
  static const char* const kClasses[][2] = {
      {"TaskLoom", "FG::TaskLoomImp"},     {"ActorRoster", "FG::ActorRosterImp"}, {"LobbyWarden", "FG::LobbyWardenImp"},
      {"MarkLedger", "FG::MarkLedgerImp"}, {"GaugeBoard", "FG::GaugeBoardImp"},   {"MenuDesk", "FG::MenuDeskImp"},
      {"VeilKeeper", "FG::VeilKeeperImp"}, {"SkyClock", "FG::SkyClockImp"},       {"EyeRig", "FG::EyeRigImp"},
      {"TableVault", "FG::TableVaultImp"}, {"PhraseBook", "FG::PhraseBookImp"}};
  for (const auto& r : kClasses) {
    if (name == r[0]) return r[1];
  }
  return nullptr;
}

json image_json(HINSTANCE instance) {
  // SAFETY: this module's own mapped headers.
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(instance);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const std::byte*>(instance) + dos->e_lfanew);
  json sections = json::array();
  const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
  for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    char name[9]{};
    std::memcpy(name, sec[i].Name, 8);
    sections.push_back({{"name", name}, {"rva", sec[i].VirtualAddress}, {"size", sec[i].Misc.VirtualSize}});
  }
  json image{{"base", hex(base())},
             {"time_date_stamp", nt->FileHeader.TimeDateStamp},
             {"size_of_image", nt->OptionalHeader.SizeOfImage},
             {"sections", sections}};

  HRSRC res = FindResourceW(instance, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16));  // RT_VERSION
  if (res != nullptr) {
    const DWORD size = SizeofResource(instance, res);
    const void* data = LockResource(LoadResource(instance, res));
    std::string copy(static_cast<const char*>(data), size);
    // SAFETY: VerQueryValueW answers pointers into `copy`, our own copy of the resource, which outlives them.
    UINT len = 0;
    VS_FIXEDFILEINFO* ffi = nullptr;
    if (VerQueryValueW(copy.data(), L"\\", reinterpret_cast<void**>(&ffi), &len) != 0 && ffi != nullptr) {
      const unsigned v[4] = {HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
                             HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS)};
      image["file_version"] = std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." +
                              std::to_string(v[3]);
      image["region"] = v[3] == 0 ? "ww" : v[3] == 1 ? "jp" : "?";
    }
    WORD* translation = nullptr;
    if (VerQueryValueW(copy.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translation), &len) != 0 &&
        translation != nullptr && len >= 4) {
      image["translation"] = {translation[0], translation[1]};
      wchar_t key[64];
      std::swprintf(key, 64, L"\\StringFileInfo\\%04x%04x\\ProductName", translation[0], translation[1]);
      wchar_t* product = nullptr;
      if (VerQueryValueW(copy.data(), key, reinterpret_cast<void**>(&product), &len) != 0 && product != nullptr) {
        std::string utf8;  // the name has a sign outside ASCII (U+2122), and no surrogate
        for (const wchar_t* c = product; *c != L'\0'; ++c) {
          const auto u = static_cast<std::uint32_t>(*c);
          if (u < 0x80) {
            utf8.push_back(static_cast<char>(u));
          } else if (u < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | (u >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (u & 0x3F)));
          } else {
            utf8.push_back(static_cast<char>(0xE0 | (u >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (u & 0x3F)));
          }
        }
        image["product_name"] = utf8;
      }
    }
  }
  return image;
}

}  // namespace

int describe(HINSTANCE instance) {
  world_publish_hinstance(instance);
  world_build_for_describe();
  json failures = json::array();

  // --- FD4-style sites, grouped by the name their runtime class gives ---
  json singletons = json::array();
  json decoys = json::array();
  json sites_by_name = json::object();
  for (const FgSiteRow* row = fg_fd4_site_table; row->site != nullptr; ++row) {
    const std::string name = fg_rtclass_name(row->rtclass);
    const std::string path = row->path;
    const PathLoadShape load = shape_of(row->kind);
    const std::uint64_t at = rva(row->site);
    const std::uint64_t insn = rva(row->path_load);
    json path_load{{"kind", row->kind},       {"insn", insn},           {"form", load.form},
                   {"reg", load.reg},         {"opcode", load.opcode},  {"length", load.length},
                   {"ordinal", load.ordinal}, {"hoisted", load.hoisted}};
    if (load.hoisted) {
      path_load["before"] = at - insn;
    } else {
      path_load["at"] = insn - at + (row->kind == kPathAbsolute ? 2 : load.length - 4);
      path_load["insn_end"] = insn - at + static_cast<std::uint64_t>(load.length);
    }
    json site{{"site", at},
              {"static", row->statik != nullptr ? rva(row->statik) : 0},
              {"class", rva(row->rtclass)},
              {"path", rva(row->path)},
              {"path_ok", path.size() >= 14 && path.compare(path.size() - 14, 14, "FD4Singleton.h") == 0},
              {"path_load", path_load}};
    if (!matches(row->site, kIdiom)) failures.push_back("fd4 idiom at " + hex(rva(row->site)));
    // The site's own displacements must lead to its static, class and path.
    // SAFETY: the site and its path's load are this module's own code; the addresses are compared, never followed.
    const std::uint8_t* s = row->site;
    const std::uint8_t* p = row->path_load;
    const void* loaded = row->kind == kPathPointer    ? *reinterpret_cast<const void* const*>(p + 7 + disp_at(p + 3))
                         : row->kind == kPathAbsolute ? *reinterpret_cast<const void* const*>(p + 2)
                                                      : p + 7 + disp_at(p + 3);
    if ((row->statik != nullptr && s + 7 + disp_at(s + 3) != reinterpret_cast<const std::uint8_t*>(row->statik)) ||
        s + 19 + disp_at(s + 15) != row->rtclass || loaded != row->path) {
      failures.push_back("fd4 displacements at " + hex(rva(s)));
    }
    if (row->kind == kNotIdiom) {
      decoys.push_back({{"name", name},
                        {"sites", json::array({site})},
                        {"expect", "refused: no idiom, its static is no RIP-relative operand"}});
      continue;
    }
    sites_by_name[name].push_back(site);
  }
  for (auto it = sites_by_name.begin(); it != sites_by_name.end(); ++it) {
    const char* role = role_of(it.key());
    json entry{{"name", it.key()}, {"sites", it.value()}};
    if (role != nullptr) {
      entry["role"] = role;
      entry["static_rva"] = it.value()[0]["static"];
      entry["class_rva"] = it.value()[0]["class"];
      entry["rtti"] = rtti_of(it.key());
      singletons.push_back(entry);
    } else {
      entry["expect"] = it.key() == "Duplicata" ? "ambiguous: two statics behind one name"
                        : it.key() == "Strayling" ? "refused: its path does not end in FD4Singleton.h"
                                                  : "refused: its static is in .rdata";
      decoys.push_back(entry);
    }
  }
  json fd4{{"idiom", kIdiom},
           {"idiom_length", 24},
           {"operands",
            {{"static", {{"at", 3}, {"insn_end", 7}}},
             {"class", {{"at", 15}, {"insn_end", 19}}},
             {"getter", {{"at", 20}, {"insn_end", 24}}}}},
           {"path_suffix", "FD4Singleton.h"},
           {"name_ptr_off", FG_RTCLASS_NAME_OFF},
           {"name_encoding", "ascii"},
           {"getter_rva", fn_rva(fg_rtclass_name)},
           {"getter_pdata", pdata_of(fn_bytes(fg_rtclass_name))},
           {"singletons", singletons},
           {"decoys", decoys}};

  // --- AOB items ---
  const AobItem items[] = {
      {"global_hinstance", "static", fn_bytes(fg_boot_publish_instance), 5,
       "48 89 CF 0F C9 48 C1 C9 0B 48 89 0D ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 89 3D ?? ?? ?? ??", 26, 30,
       &fg_static_hinstance, "57 48 83 EC 20"},
      {"GameDataMan", "static", fn_bytes(fg_save_ledger_swap), 5,
       "48 89 CB 48 87 0D ?? ?? ?? ?? 48 85 DB 0F 95 C2 88 15 ?? ?? ?? ??", 6, 10, &fg_static_save_ledger,
       "53 48 83 EC 20"},
      {"GameMan", "static", fn_bytes(fg_run_warden_slot), 5,
       "4C 8B 0D ?? ?? ?? ?? 83 C8 FF 4D 85 C9 74 04 41 8B 41 14", 3, 7, &fg_static_run_warden, "56 48 83 EC 20"},
      {"FieldArea", "static", fn_bytes(fg_field_plot_place), 5,
       "4C 8B 15 ?? ?? ?? ?? 31 C0 4D 85 D2 74 04 49 63 42 1C", 3, 7, &fg_static_field_plot, "57 48 83 EC 20"},
      {"register_task", "function", fn_bytes(fg_register_task), 7,
       "48 89 CB 48 89 D6 44 89 C7 48 8B 0D ?? ?? ?? ?? 48 85 C9 75 ?? E8 ?? ?? ?? ??", 0, 0, nullptr,
       "53 56 57 48 83 EC 20"},
  };
  json aob = json::array();
  for (const AobItem& item : items) {
    const std::uint8_t* hit = item.fn + item.anchor;
    json entry{{"role", item.role},  {"kind", item.kind},       {"pattern", item.pattern}, {"anchor", item.anchor},
               {"hit_rva", rva(hit)}, {"fn_rva", rva(item.fn)}, {"pdata", pdata_of(item.fn)},
               {"prologue", item.prologue}};
    if (!matches(hit, item.pattern)) failures.push_back(std::string("aob pattern of ") + item.role);
    if (!matches(item.fn, item.prologue)) failures.push_back(std::string("aob prologue of ") + item.role);
    if (item.target != nullptr) {
      entry["capture"] = {{"at", item.capture_at}, {"kind", "rip_rel32"}, {"insn_end", item.insn_end}};
      entry["target_rva"] = rva(item.target);
      entry["target_section"] = ".data";
      if (hit + item.insn_end + disp_at(hit + item.capture_at) != item.target) {
        failures.push_back(std::string("aob capture of ") + item.role);
      }
    }
    aob.push_back(entry);
  }
  json calls = json::array();
  for (const std::uint8_t* const* c = fg_register_call_table; *c != nullptr; ++c) calls.push_back(rva(*c));

  // --- The task: its ABI, its shape and the group table ---
  json groups = json::array();
  for (std::uint32_t i = 0; i < group_count(); ++i) {
    std::string narrow;
    for (const char16_t* c = group_name(i); *c != u'\0'; ++c) narrow.push_back(static_cast<char>(*c));
    groups.push_back({{"index", i}, {"name", narrow}});
  }
  const auto* loom = static_cast<const FG::TaskLoomImp*>(fg_static_taskloom);
  json task{
      {"register_rva", fn_rva(fg_register_task)},
      {"register_abi", {{"task", "rcx"}, {"instance", "rdx"}, {"group", "r8d"}}},
      {"register_returns", "1 when the task was kept, else 0"},
      {"register_bound", FG_GROUP_BOUND},               // the work compares the group with it, an immediate
      {"register_keeps_off", FG_LOOM_LAST_TASK_OFF},    // where in the hub the work stores the task
      {"register_keeps_vtable_off", FG_LOOM_LAST_VTABLE_OFF},  // and the task's vtable
      // The registration is a wrapper of R1 run 7's shape (spike.8): it loads its rack from the static at `slot_rva`
      // (making it with `alloc` and `build` while it is null) and tail-jumps to `tail`, which bounds the group by
      // `bound`, tests the rack's shelf at `shelves_off` + group*8 and jumps through the shelf's vtable at
      // `shelf_slot_off` to `shelf_register`, which calls the work (the hub in rcx, the group in edx, the task in r8).
      // With --table-gap the stand-in group has no shelf (`gap`), and the registration answers 0.
      {"register_rack",
       {{"slot_rva", rva(&fg_static_rack)},
        {"alloc_rva", fn_rva(fg_rack_alloc)},
        {"build_rva", fn_rva(fg_rack_build)},
        {"tail_rva", fn_rva(fg_register_rack)},
        {"bound", FG_RACK_BOUND},
        {"shelves_off", FG_RACK_SHELVES_OFF},
        {"shelf_slot_off", FG_SHELF_REGISTER_OFF},
        {"shelf_register_rva", fn_rva(fg_shelf_register)},
        {"shelf_vtable_rva", rva(fg_shelf_vtable)},
        {"gap", options().table_gap},
        {"made", fg_static_rack != nullptr}}},
      {"register_work_rva", fn_rva(fg_register_keep)},
      {"hub_run_fns_off", 8 + offsetof(FG::TaskLoomFields, run_fns)},
      {"register_call_sites", calls},
      {"register_thread", "any; the hub queues the task and activates it on the game thread"},
      {"vtable_slots", FG::kTaskSlots},
      {"attach_slot", FG::kAttachSlot},
      {"run_slot", FG::kRunSlot},
      {"query_slot", FG::kQuerySlot},
      {"query_every", FG::kQueryEvery},
      {"slot_signatures",
       {{"attach", "void(self, hub)"}, {"run", "void(self, const FrameInfo*)"}, {"query", "u32(self)"}}},
      {"object_size", FG::kTaskFieldsSize},
      {"object_fields",
       {{"group", {{"off", 8 + offsetof(FG::TaskFields, group)}, {"ty", "u32"}}},
        {"state", {{"off", 8 + offsetof(FG::TaskFields, state)}, {"ty", "u32"}}},
        {"hub", {{"off", 8 + offsetof(FG::TaskFields, hub)}, {"ty", "ptr"}}},
        {"runs", {{"off", 8 + offsetof(FG::TaskFields, runs)}, {"ty", "u64"}}}}},
      {"frame_info",
       {{"frame", {{"off", offsetof(FG::FrameInfo, frame)}, {"ty", "u64"}}},
        {"dt", {{"off", offsetof(FG::FrameInfo, dt)}, {"ty", "f32"}}},
        {"group", {{"off", offsetof(FG::FrameInfo, group)}, {"ty", "u32"}}}}},
      {"group_table",
       {{"count_off", 8 + offsetof(FG::TaskLoomFields, group_count)},
        {"ptr_off", 8 + offsetof(FG::TaskLoomFields, groups)},
        {"stride", sizeof(FG::GroupEntry)},
        {"index_off", offsetof(FG::GroupEntry, index)},
        {"name_off", offsetof(FG::GroupEntry, name)},
        {"name", "u16[64]"}}},
      {"groups", groups},
      {"standin", {{"name", "Fg_ActorSettle"}, {"index", standin_group()}, {"stands_for", "ChrIns_PostPhysics"}}},
      {"loom_offsets_checked", loom != nullptr && off(loom, &loom->f.group_count) == FG_LOOM_GROUP_COUNT_OFF}};

  // --- Classes: each sample's vtable, from a live object ---
  json classes = json::array();
  for (const ClassSample& s : class_samples()) {
    const void* vtable = nullptr;
    std::memcpy(&vtable, static_cast<const std::byte*>(s.object) + s.offset, sizeof vtable);
    classes.push_back({{"name", s.name},
                       {"mangled", s.mangled},
                       {"vtable_rva", rva(vtable)},
                       {"subobject_offset", s.offset},
                       {"actor", s.actor},
                       {"bases", s.bases}});
  }
#if defined(_MSC_VER)
  const char* rtti_kind = "msvc";
#else
  const char* rtti_kind = "itanium (GCC's: the MSVC RTTI walk finds nothing in this build)";
#endif
  json rtti{{"kind", rtti_kind},
            {"namespace", "FG"},
            {"actor_base", "FG::Actor"},
            {"player", "FG::PlayerActor"},
            {"classes", classes}};

  // --- Layouts: each field's offset from its object's start ---
  const auto* roster = static_cast<const FG::ActorRosterImp*>(fg_static_roster);
  const auto* ledger = static_cast<const FG::SaveLedgerImp*>(fg_static_save_ledger);
  const auto* hero = ledger->f.hero;
  const auto* warden = static_cast<const FG::RunWardenImp*>(fg_static_run_warden);
  const auto* plot = static_cast<const FG::FieldPlotImp*>(fg_static_field_plot);
  const auto* lobby = static_cast<const FG::LobbyWardenImp*>(fg_static_lobby);
  const auto* menu = static_cast<const FG::MenuDeskImp*>(fg_static_menu);
  const auto* gauges = static_cast<const FG::GaugeBoardImp*>(fg_static_gauges);
  const auto* veil = static_cast<const FG::VeilKeeperImp*>(fg_static_veil);
  const auto* sky = static_cast<const FG::SkyClockImp*>(fg_static_sky);
  const auto* eye = static_cast<const FG::EyeRigImp*>(fg_static_eye);
  const auto* marks = static_cast<const FG::MarkLedgerImp*>(fg_static_marks);
  const auto* phrases = static_cast<const FG::PhraseBookImp*>(fg_static_phrases);
  const auto* vault = static_cast<const FG::TableVaultImp*>(fg_static_tables);
  const auto* player = static_cast<const FG::PlayerActor*>(roster->f.main_player);
  auto field = [](std::uint64_t o, const char* ty) { return json{{"off", o}, {"ty", ty}}; };
  json layout{
      {"Actor",
       {{"size", sizeof(FG::Actor)},
        {"vtable", field(0, "ptr")},
        {"entity_id", field(off(player, &player->a.entity_id), "u32")},
        {"model", field(off(player, &player->a.model), "u32")},
        {"handle", field(off(player, &player->a.handle), "u64")},
        {"npc_param_id", field(off(player, &player->a.npc_param_id), "u32")},
        {"chr_type", field(off(player, &player->a.chr_type), "i32")},
        {"block_id", field(off(player, &player->a.block_id), "u32")},
        {"team", field(off(player, &player->a.team), "u8")},
        {"flags", {{"off", off(player, &player->a.flags)}, {"ty", "u8"}, {"on_screen_bit", FG::kActorOnScreenBit},
                   {"dead_bit", FG::kActorDeadBit}}},
        {"last_hit_by", field(off(player, &player->a.last_hit_by), "u64")},
        {"parts", field(off(player, &player->a.parts), "ptr")}}},
      {"PlayerActor",
       {{"size", sizeof(FG::PlayerActor)},
        {"locked_on", field(off(player, &player->p.locked_on), "u64")},
        {"mount_state", {{"off", off(player, &player->p.mount_state)}, {"ty", "u8"}, {"mounted", 3}}}}},
      {"ActorParts",
       {{"vitals", field(offsetof(FG::ActorParts, vitals), "ptr")},
        {"motion", field(offsetof(FG::ActorParts, motion), "ptr")},
        {"react", field(offsetof(FG::ActorParts, react), "ptr")},
        {"grip", field(offsetof(FG::ActorParts, grip), "ptr")}}},
      {"VitalsPart",
       {{"max_hp", field(offsetof(FG::VitalsPart, max_hp), "i32")},
        {"hp", field(offsetof(FG::VitalsPart, hp), "i32")},
        {"max_fp", field(offsetof(FG::VitalsPart, max_fp), "i32")},
        {"fp", field(offsetof(FG::VitalsPart, fp), "i32")},
        {"max_stamina", field(offsetof(FG::VitalsPart, max_stamina), "i32")},
        {"stamina", field(offsetof(FG::VitalsPart, stamina), "i32")}}},
      {"MotionPart", {{"pos", field(offsetof(FG::MotionPart, pos), "f32[4]")}}},
      {"ReactPart", {{"recv_dmg_type", field(offsetof(FG::ReactPart, recv_dmg_type), "i32")}}},
      {"GripPart", {{"throw_state", field(offsetof(FG::GripPart, throw_state), "u8")}}},
      {"ActorRosterImp",
       {{"near_begin", field(off(roster, &roster->f.near_begin), "ptr")},
        {"near_end", field(off(roster, &roster->f.near_end), "ptr")},
        {"near_entry", {{"stride", sizeof(FG::NearEntry)}, {"actor", offsetof(FG::NearEntry, actor)},
                        {"distance", offsetof(FG::NearEntry, distance)}, {"max", FG::kMaxNear}}},
        {"players", field(off(roster, &roster->f.players), "ptr")},
        {"player_count", field(off(roster, &roster->f.player_count), "u32")},
        {"player_slot_stride", sizeof(FG::PlayerSlot)},
        {"main_player", field(off(roster, &roster->f.main_player), "ptr")}}},
      {"SaveLedgerImp",
       {{"hero", field(off(ledger, &ledger->f.hero), "ptr")},
        {"settings", field(off(ledger, &ledger->f.settings), "ptr")},
        {"death_count", field(off(ledger, &ledger->f.death_count), "u32")},
        {"ng_level", field(off(ledger, &ledger->f.ng_level), "u32")},
        {"play_time_ms", field(off(ledger, &ledger->f.play_time_ms), "u32")},
        {"boss_fight", field(off(ledger, &ledger->f.boss_fight), "u8")},
        {"settings_show_subtitles", field(offsetof(FG::SettingsBlock, show_subtitles), "u8")}}},
      {"HeroSheet",
       {{"level", field(off(hero, &hero->h.level), "u32")},
        {"runes", field(off(hero, &hero->h.runes), "u32")},
        {"archetype", field(off(hero, &hero->h.archetype), "u8")},
        {"body", field(off(hero, &hero->h.body), "u8")},
        {"keepsake", field(off(hero, &hero->h.keepsake), "u8")},
        {"multiplay_role", field(off(hero, &hero->h.multiplay_role), "u8")},
        {"is_my_world", field(off(hero, &hero->h.is_my_world), "u8")},
        {"name", field(off(hero, &hero->h.name), "utf16[17]")},
        {"gear", field(off(hero, &hero->h.gear), "u32[6]")}}},
      {"RunWardenImp",
       {{"talk_cam_param_id", field(off(warden, &warden->f.talk_cam_param_id), "i32")},
        {"save_slot", field(off(warden, &warden->f.save_slot), "i32")}}},
      {"FieldPlotImp", {{"place_id", field(off(plot, &plot->f.place_id), "i32")}}},
      {"LobbyWardenImp", {{"lobby_state", field(off(lobby, &lobby->lobby_state), "i32")}}},
      {"MenuDeskImp",
       {{"screen_state", field(off(menu, &menu->screen_state), "i32")},
        {"popup", field(off(menu, &menu->popup), "ptr")},
        {"popup_talk_id", field(offsetof(FG::PopupState, talk_id), "i32")}}},
      {"GaugeBoardImp",
       {{"hud_state", field(off(gauges, &gauges->hud_state), "i32")},
        {"banner_id", field(off(gauges, &gauges->banner_id), "i32")},
        {"bars", {{"off", off(gauges, &gauges->bars)}, {"stride", sizeof(FG::GaugeBar)}, {"count", 3},
                  {"handle", offsetof(FG::GaugeBar, handle)}, {"name_id", offsetof(FG::GaugeBar, name_id)},
                  {"damage", offsetof(FG::GaugeBar, damage)}}}}},
      {"VeilKeeperImp", {{"load_counter", field(off(veil, &veil->load_counter), "u32")}}},
      {"SkyClockImp",
       {{"clock", {{"off", off(sky, &sky->clock)}, {"ty", "u64"}, {"packing", "hours << 32 | minutes << 16 | seconds"}}}}},
      {"EyeRigImp",
       {{"view", field(off(eye, &eye->view), "ptr")},
        {"view_matrix", field(offsetof(FG::ViewData, matrix), "f32[16]")},
        {"view_fov", field(offsetof(FG::ViewData, fov), "f32")},
        {"view_aspect", field(offsetof(FG::ViewData, aspect), "f32")}}},
      {"MarkLedgerImp",
       {{"divisor", field(off(marks, &marks->divisor), "u32")},
        {"map_head", field(off(marks, &marks->map_head), "ptr")},
        {"map_count", field(off(marks, &marks->map_count), "u64")},
        {"holder", field(off(marks, &marks->holder), "ptr")}}},
      {"FlagNode",
       {{"size", sizeof(FG::FlagNode)},
        {"left", field(offsetof(FG::FlagNode, left), "ptr")},
        {"parent", field(offsetof(FG::FlagNode, parent), "ptr")},
        {"right", field(offsetof(FG::FlagNode, right), "ptr")},
        {"key", field(offsetof(FG::FlagNode, key), "u32")},
        {"mode", field(offsetof(FG::FlagNode, mode), "u32")},
        {"value", field(offsetof(FG::FlagNode, value), "u64")}}},
      {"PhraseBookImp",
       {{"tables", field(off(phrases, &phrases->tables), "ptr")},
        {"count", field(off(phrases, &phrases->count), "i32")}}},
      {"PhraseTable",
       {{"size", sizeof(FG::PhraseTable)},
        {"group_count", field(offsetof(FG::PhraseTable, group_count), "u32")},
        {"groups_off", field(offsetof(FG::PhraseTable, groups_off), "u64")},
        {"offsets_off", field(offsetof(FG::PhraseTable, offsets_off), "u64")}}},
      {"PhraseGroup",
       {{"stride", sizeof(FG::PhraseGroup)},
        {"offset_index", field(offsetof(FG::PhraseGroup, offset_index), "i32")},
        {"first_id", field(offsetof(FG::PhraseGroup, first_id), "i32")},
        {"last_id", field(offsetof(FG::PhraseGroup, last_id), "i32")}}},
      {"TableVaultImp",
       {{"table_count", field(off(vault, &vault->table_count), "u32")},
        {"tables", field(off(vault, &vault->tables), "ptr")}}},
      {"ParamSheet",
       {{"size", sizeof(FG::ParamSheet)},
        {"rows", field(offsetof(FG::ParamSheet, rows), "ptr")},
        {"row_count", field(offsetof(FG::ParamSheet, row_count), "u16")},
        {"row_size", field(offsetof(FG::ParamSheet, row_size), "u16")}}},
      {"ParamRowRef",
       {{"stride", sizeof(FG::ParamRowRef)},
        {"id", field(offsetof(FG::ParamRowRef, id), "u32")},
        {"data", field(offsetof(FG::ParamRowRef, data), "ptr")}}},
      {"NpcRow",
       {{"size", sizeof(FG::NpcRow)},
        {"name_id", field(offsetof(FG::NpcRow, name_id), "i32")},
        {"team", field(offsetof(FG::NpcRow, team), "u8")}}},
  };
  // The stores' invented contents, by id only (the harness checks what the GAL reads against these).
  json stores{{"flags", {{"groups", {0, 7, 4321}}, {"set", {100, 4321010}}, {"set_in_world", {7005}},
                         {"missing_group", 999}}},
              {"text", {{"tables", FG::kPhraseTables}, {"npc_names", FG::kNpcNameTable},
                        {"place_names", FG::kPlaceNameTable}, {"npc_ids", {1100, 1101, 1102}},
                        {"place_ids", {6100100}}, {"no_string", 1103}}},
              {"params", {{"slot", FG::kNpcSheetSlot}, {"row_size", sizeof(FG::NpcRow)},
                          {"ids", {81100, 81200, 81300, 81400, 81500, 81600}}}}};

  json enums{{"screen_state", {{"1", "Title"}, {"2", "Loading"}, {"4", "InGame"}}},
             {"lobby_state", {{"10", "Solo"}, {"11", "Hosting"}, {"12", "Visiting"}}},
             {"multiplay_role", {{"2", "Host"}, {"3", "Guest"}}},
             {"hud_state", {{"0", "HideAll"}, {"3", "Default"}}}};

  json out{{"fakegame", {{"schema", 1},
#if defined(_MSC_VER)
                         {"toolchain", "clang-cl"}
#else
                         {"toolchain", "mingw"}
#endif
                        }},
           {"image", image_json(instance)},
           {"fd4", fd4},
           {"aob", aob},
           {"task", task},
           {"rtti", rtti},
           {"layout", layout},
           {"stores", stores},
           {"enums", enums},
           {"self_check", {{"ok", failures.empty()}, {"failures", failures}}}};
  const std::string text = out.dump() + "\n";
  DWORD written = 0;
  HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (std_out != nullptr && std_out != INVALID_HANDLE_VALUE) {
    WriteFile(std_out, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  }
  return failures.empty() ? 0 : 4;
}

}  // namespace fakegame
