// The derivation's invented test program (sample_program.hpp).
#include "sample_program.hpp"

#include <vector>

namespace en::gal::test {

namespace {

using P = FakeProgram;

// A function of .text: a label, its bytes written by `body`, a label at its end, and its entry in .pdata.
template <class F>
void function(P& p, const std::string& name, F&& body) {
  p.align(P::Text, 16, 0xCC);
  p.label(name, P::Text);
  body();
  p.label(name + "_end", P::Text);
  p.function(name, name + "_end");
}

// One FD4-style accessor of the sample's own shape: push rdi; sub rsp, 0x20; mov rdi, [rip+static]; test rdi, rdi;
// jne +0x20; lea rcx, [rip+class]; call getter; mov edx, site; mov r8, rax; lea rcx, [rip+path]; call log;
// mov rax, rdi; add rsp, 0x20; pop rdi; ret.
void accessor(P& p, const std::string& name, const std::string& statik, const std::string& klass,
              const std::string& path, std::uint32_t site) {
  function(p, name, [&] {
    p.bytes(P::Text, {0x57, 0x48, 0x83, 0xEC, 0x20});
    p.bytes(P::Text, {0x48, 0x8B, 0x3D});
    p.rel32(P::Text, statik);
    p.bytes(P::Text, {0x48, 0x85, 0xFF, 0x75, 0x20});
    p.bytes(P::Text, {0x48, 0x8D, 0x0D});
    p.rel32(P::Text, klass);
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "getter");
    p.bytes(P::Text, {0xBA});
    p.u32(P::Text, site);
    p.bytes(P::Text, {0x49, 0x89, 0xC0, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, path);
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "log");
    p.bytes(P::Text, {0x48, 0x89, 0xF8, 0x48, 0x83, 0xC4, 0x20, 0x5F, 0xC3});
  });
}

// A runtime class object: a table pointer, a size, and the name's address at +0x10.
void class_object(P& p, const std::string& label, const std::string& name_label) {
  p.align(P::Data, 16);
  p.label(label, P::Data);
  p.va64(P::Data, "class_table");
  p.u64(P::Data, 0x40);
  p.va64(P::Data, name_label);
  p.u64(P::Data, 0);
}

void statik(P& p, P::Sec s, const std::string& label) {
  p.align(s, 8);
  p.label(label, s);
  p.u64(s, 0);
}

// MSVC RTTI for one class: its type descriptor in .data, a base class descriptor, its hierarchy and base array in
// .rdata, and a locator and a vtable for each subobject, the vtable's slots naming functions of .text.
struct Vt {
  std::uint32_t offset;
  std::vector<std::string> slots;
};

void rtti_class(P& p, const std::string& id, const std::string& decorated, const std::vector<std::string>& bases,
                const std::vector<Vt>& vtables) {
  p.align(P::Data, 8);
  p.label("td_" + id, P::Data);
  p.va64(P::Data, "type_info_vft");
  p.u64(P::Data, 0);
  p.ascii(P::Data, decorated);
  p.align(P::Rdata, 8);
  p.label("bcd_" + id, P::Rdata);
  p.rva32(P::Rdata, "td_" + id);
  p.u32(P::Rdata, static_cast<std::uint32_t>(bases.size()));
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, 0xFFFFFFFF);
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, 0x40);
  p.rva32(P::Rdata, "chd_" + id);
  p.align(P::Rdata, 8);
  p.label("chd_" + id, P::Rdata);
  p.u32(P::Rdata, 0);
  p.u32(P::Rdata, bases.size() > 1 ? 1 : 0);
  p.u32(P::Rdata, static_cast<std::uint32_t>(1 + bases.size()));
  p.rva32(P::Rdata, "bca_" + id);
  p.label("bca_" + id, P::Rdata);
  p.rva32(P::Rdata, "bcd_" + id);
  for (const std::string& b : bases) p.rva32(P::Rdata, "bcd_" + b);
  for (const Vt& v : vtables) {
    const std::string col = "col_" + id + "_" + std::to_string(v.offset);
    p.align(P::Rdata, 8);
    p.label(col, P::Rdata);
    p.u32(P::Rdata, 1);
    p.u32(P::Rdata, v.offset);
    p.u32(P::Rdata, 0);
    p.rva32(P::Rdata, "td_" + id);
    p.rva32(P::Rdata, "chd_" + id);
    p.rva32(P::Rdata, col);
  }
  for (const Vt& v : vtables) {
    p.align(P::Rdata, 8);
    p.va64(P::Rdata, "col_" + id + "_" + std::to_string(v.offset));
    p.label("vt_" + id + "_" + std::to_string(v.offset), P::Rdata);
    for (const std::string& s : v.slots) p.va64(P::Rdata, s);
  }
  p.u64(P::Rdata, 0);  // ends the last vtable's run of slots
}

}  // namespace

Sample build_sample() {
  Sample s;
  P& p = s.program;

  // ---- .rdata: strings, the class objects' table, the group table, type_info's vtable ----
  p.label("path", P::Rdata);
  p.ascii(P::Rdata, "game\\core\\Lone.h");
  p.label("path_decoy", P::Rdata);
  p.ascii(P::Rdata, "game\\core\\Lone.hpp");
  for (const char* n : {"Hub", "Roster", "Clock", "Dup", "Stray", "Rock"}) {
    p.label(std::string("name_") + n, P::Rdata);
    p.ascii(P::Rdata, n);
  }
  p.align(P::Rdata, 8);
  p.label("class_table", P::Rdata);
  p.va64(P::Rdata, "getter");
  p.label("type_info_vft", P::Rdata);
  p.va64(P::Rdata, "noop");
  p.u64(P::Rdata, 0);
  p.label("groups", P::Rdata);
  for (const char* g : {"g0", "g1", "g2", "g3"}) p.va64(P::Rdata, g);
  p.u64(P::Rdata, 0);
  const char* groups[] = {"Gr_Begin", "Gr_Think", "Gr_Settle", "Gr_End"};
  for (int i = 0; i < 4; ++i) {
    p.align(P::Rdata, 2);
    p.label("g" + std::to_string(i), P::Rdata);
    p.utf16(P::Rdata, groups[i]);
  }

  // ---- .data: statics and class objects ----
  for (const char* st :
       {"st_hub", "st_roster", "st_clock", "st_dup_a", "st_dup_b", "st_stray", "st_ledger", "st_module"}) {
    statik(p, P::Data, st);
  }
  statik(p, P::Rdata, "st_rock");  // a decoy: a "static" in read-only data
  for (const char* n : {"Hub", "Roster", "Clock", "Stray", "Rock"}) {
    class_object(p, std::string("cls_") + n, std::string("name_") + n);
  }
  class_object(p, "cls_dup_a", "name_Dup");
  class_object(p, "cls_dup_b", "name_Dup");
  statik(p, P::Data, "task_object");

  // ---- .text ----
  function(p, "noop", [&] { p.bytes(P::Text, {0xC3}); });
  function(p, "getter", [&] { p.bytes(P::Text, {0x48, 0x8B, 0x41, 0x10, 0xC3}); });  // mov rax, [rcx+0x10]; ret
  function(p, "log", [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });                 // xor eax, eax; ret
  accessor(p, "get_hub", "st_hub", "cls_Hub", "path", 0x1001);
  accessor(p, "get_hub_again", "st_hub", "cls_Hub", "path", 0x1002);
  accessor(p, "get_roster", "st_roster", "cls_Roster", "path", 0x1003);
  accessor(p, "get_clock", "st_clock", "cls_Clock", "path", 0x1004);
  accessor(p, "get_dup_a", "st_dup_a", "cls_dup_a", "path", 0x1005);
  accessor(p, "get_dup_b", "st_dup_b", "cls_dup_b", "path", 0x1006);
  accessor(p, "get_stray", "st_stray", "cls_Stray", "path_decoy", 0x1007);
  accessor(p, "get_rock", "st_rock", "cls_Rock", "path", 0x1008);

  // The registration, with the sample's own argument order: the group in ecx, the task in rdx, the instance in r9.
  function(p, "register", [&] {
    p.bytes(P::Text, {0x53, 0x56, 0x48, 0x83, 0xEC, 0x28});        // push rbx; push rsi; sub rsp, 0x28
    p.bytes(P::Text, {0x4C, 0x89, 0xCB, 0x89, 0xCE});              // mov rbx, r9; mov esi, ecx
    p.bytes(P::Text, {0x3B, 0x73, 0x34, 0x73, 0x0C});              // cmp esi, [rbx+0x34]; jae +12
    p.bytes(P::Text, {0x48, 0x89, 0x54, 0xF3, 0x40});              // mov [rbx+rsi*8+0x40], rdx
    p.bytes(P::Text, {0xFF, 0x43, 0x38, 0x31, 0xC0, 0xEB, 0x05});  // inc dword [rbx+0x38]; xor eax, eax; jmp +5
    p.bytes(P::Text, {0xB8, 0xFF, 0xFF, 0xFF, 0xFF});              // mov eax, -1
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0x5E, 0x5B, 0xC3});  // add rsp, 0x28; pop rsi; pop rbx; ret
  });
  function(p, "boot_tasks", [&] {
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28});  // sub rsp, 0x28
    for (std::uint8_t group : {std::uint8_t{2}, std::uint8_t{3}}) {
      p.bytes(P::Text, {0x4C, 0x8B, 0x0D});  // mov r9, [rip+hub]
      p.rel32(P::Text, "st_hub");
      p.bytes(P::Text, {0x48, 0x8D, 0x15});  // lea rdx, [rip+task]
      p.rel32(P::Text, "task_object");
      p.bytes(P::Text, {0xB9, group, 0x00, 0x00, 0x00});  // mov ecx, group
      p.bytes(P::Text, {0xE8});
      p.rel32(P::Text, "register");
    }
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0xC3});
  });
  // A function that loads the hub and passes it on through a register call: not a registration.
  function(p, "poke_hub", [&] {
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x0D});  // sub rsp, 0x28; mov rcx, [rip+hub]
    p.rel32(P::Text, "st_hub");
    p.bytes(P::Text, {0xFF, 0x51, 0x08, 0x48, 0x83, 0xC4, 0x28, 0xC3});  // call [rcx+8]; add rsp, 0x28; ret
  });
  // Code that names the group with its index beside it.
  function(p, "name_group", [&] {
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x15});  // sub rsp, 0x28; lea rdx, [rip+g2]
    p.rel32(P::Text, "g2");
    p.bytes(P::Text, {0xB9, 0x02, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3});  // mov ecx, 2; add rsp; ret
  });
  // Two readers of the ledger's static and one writer of the module's base.
  function(p, "ledger_deaths", [&] {
    p.bytes(P::Text,
            {0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05});  // push rbx; sub rsp, 0x20; mov rax, [rip+ledger]
    p.rel32(P::Text, "st_ledger");
    p.bytes(P::Text, {0x48, 0x85, 0xC0, 0x74, 0x03, 0x8B, 0x40, 0x24});  // test rax, rax; je +3; mov eax, [rax+0x24]
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3});
  });
  function(p, "ledger_set", [&] {
    p.bytes(P::Text,
            {0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x87, 0x0D});  // push rdi; sub rsp, 0x20; xchg [rip+ledger], rcx
    p.rel32(P::Text, "st_ledger");
    p.bytes(P::Text, {0x48, 0x89, 0xC8, 0x48, 0x83, 0xC4, 0x20, 0x5F, 0xC3});
  });
  function(p, "publish_module", [&] {
    p.bytes(P::Text,
            {0x56, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x89, 0x0D});  // push rsi; sub rsp, 0x20; mov [rip+module], rcx
    p.rel32(P::Text, "st_module");
    p.bytes(P::Text, {0x48, 0x31, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5E, 0xC3});
  });

  // Slot functions for the RTTI classes.
  for (const char* f :
       {"body_think", "body_kind", "hero_think", "hero_kind", "beast_kind", "rider_kind", "saddle_seats", "rider_seats",
        "other_kind", "task_attach", "alpha_run", "beta_run", "gamma_run", "task_query", "ledger_kind"}) {
    function(p, f, [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });
  }

  // ---- RTTI ----
  rtti_class(p, "body", ".?AVBody@Kin@@", {}, {{0, {"body_think", "body_kind"}}});
  rtti_class(p, "hero", ".?AVHero@Kin@@", {"body"}, {{0, {"hero_think", "hero_kind"}}});
  rtti_class(p, "beast", ".?AVBeast@Kin@@", {"body"}, {{0, {"body_think", "beast_kind"}}});
  rtti_class(p, "saddle", ".?AVSaddle@Kin@@", {}, {{0, {"saddle_seats"}}});
  rtti_class(p, "rider", ".?AVRider@Kin@@", {"beast", "body", "saddle"},
             {{0, {"body_think", "rider_kind"}}, {0x10, {"rider_seats"}}});
  rtti_class(p, "other", ".?AVBody@Other@@", {}, {{0, {"other_kind"}}});
  rtti_class(p, "taskbase", ".?AVBase@Tasks@@", {}, {{0, {"task_attach", "noop", "task_query"}}});
  rtti_class(p, "alpha", ".?AVAlpha@Tasks@@", {"taskbase"}, {{0, {"task_attach", "alpha_run", "task_query"}}});
  rtti_class(p, "beta", ".?AVBeta@Tasks@@", {"taskbase"}, {{0, {"task_attach", "beta_run", "task_query"}}});
  rtti_class(p, "gamma", ".?AVGamma@Tasks@@", {"taskbase"}, {{0, {"task_attach", "gamma_run", "task_query"}}});
  rtti_class(p, "ledger", ".?AVLedger@Store@@", {}, {{0, {"ledger_kind"}}});

  s.built = p.build();
  return s;
}

// ---- The variants (R0c): the FD4 idiom's path loaded otherwise, a group table of entries with a stride, statics
// whose classes have no RTTI, and a trademark sign in the product name ----

namespace {

// One accessor of the variants: `head` loads the static into the register whose ModRM and REX bytes it gives
// (mov reg, [rip+static]; test reg, reg; jnz over the block), then the block: lea rcx, [rip+class]; call getter; then
// `tail`, the path's load and the log's call, which `tail_bytes` long ends the block; `hoist` loads the path into rsi
// before the head. Each is a function of .text whose idiom starts at entry + 5 or later.
struct VariantHead {
  std::uint8_t rex;
  std::uint8_t load_modrm;  // mov reg, [rip+x]
  std::uint8_t test_modrm;  // test reg, reg
};

template <class Tail>
void variant(P& p, const std::string& name, const VariantHead& h, const std::string& statik, const std::string& klass,
             std::uint8_t tail_bytes, Tail&& tail, bool hoist = false) {
  function(p, name, [&] {
    p.bytes(P::Text, {0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x20});  // push rsi; push rdi; push rbx; sub rsp, 0x20
    if (hoist) {
      p.bytes(P::Text, {0x48, 0x8D, 0x35});  // lea rsi, [rip+path]
      p.rel32(P::Text, "v_path");
    }
    p.bytes(P::Text, {h.rex, 0x8B, h.load_modrm});
    p.rel32(P::Text, statik);
    p.bytes(P::Text, {static_cast<std::uint8_t>(h.rex | (h.rex & 0x4 ? 0x1 : 0x0)), 0x85, h.test_modrm});
    p.bytes(P::Text, {0x75, static_cast<std::uint8_t>(12 + tail_bytes)});
    p.bytes(P::Text, {0x48, 0x8D, 0x0D});
    p.rel32(P::Text, klass);
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "v_getter");
    tail();
    p.bytes(P::Text, {0x31, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0x5F, 0x5E, 0xC3});  // xor eax, eax; ...; ret
  });
}

}  // namespace

Sample build_variants() {
  Sample s;
  P& p = s.program;
  constexpr VariantHead kRdi{0x48, 0x3D, 0xFF};
  constexpr VariantHead kRbx{0x48, 0x1D, 0xDB};
  constexpr VariantHead kR12{0x4C, 0x25, 0xE4};

  // ---- .rdata: the path, a format string, a pointer to the path, names, the group table of entries ----
  p.label("v_path", P::Rdata);
  p.ascii(P::Rdata, "variant\\core\\Lone.h");
  p.label("v_format", P::Rdata);
  p.ascii(P::Rdata, "early use of %s");
  for (const char* n : {"Anvil", "Bellows", "Crucible", "Dross", "Ember", "Forge"}) {
    p.label(std::string("vname_") + n, P::Rdata);
    p.ascii(P::Rdata, n);
  }
  p.align(P::Rdata, 8);
  p.label("v_path_slot", P::Rdata);
  p.va64(P::Rdata, "v_path");
  p.label("v_class_table", P::Rdata);
  p.va64(P::Rdata, "v_getter");
  p.label("type_info_vft", P::Rdata);
  p.va64(P::Rdata, "v_noop");
  p.u64(P::Rdata, 0);
  const char* groups[] = {"Va_Open", "Va_Step", "Va_Settle", "Va_Draw", "Va_Close"};
  for (int i = 0; i < 5; ++i) {
    p.align(P::Rdata, 2);
    p.label("vg" + std::to_string(i), P::Rdata);
    p.utf16(P::Rdata, groups[i]);
  }
  // The group table: entries of 16 bytes, { index (its place + 10), flags, name }, after an unrelated word.
  p.align(P::Rdata, 16);
  p.u64(P::Rdata, 0x5A5A5A5A);
  p.label("v_groups", P::Rdata);
  for (int i = 0; i < 5; ++i) {
    p.u32(P::Rdata, static_cast<std::uint32_t>(10 + i));
    p.u32(P::Rdata, 0x100u * static_cast<std::uint32_t>(i));
    p.va64(P::Rdata, "vg" + std::to_string(i));
  }
  p.u64(P::Rdata, 0);
  p.u64(P::Rdata, 0);

  // ---- .data: statics and class objects ----
  for (const char* n : {"Anvil", "Bellows", "Crucible", "Dross", "Ember", "Forge"}) {
    statik(p, P::Data, std::string("vst_") + n);
  }
  for (const char* n : {"Anvil", "Bellows", "Crucible", "Dross", "Ember", "Forge"}) {
    p.align(P::Data, 16);
    p.label(std::string("vcls_") + n, P::Data);
    p.va64(P::Data, "v_class_table");
    p.u64(P::Data, 0x40);
    p.va64(P::Data, std::string("vname_") + n);
    p.u64(P::Data, 0);
  }
  statik(p, P::Data, "vst_plain");
  statik(p, P::Data, "vst_moved");

  // ---- .text ----
  function(p, "v_noop", [&] { p.bytes(P::Text, {0xC3}); });
  function(p, "v_getter", [&] { p.bytes(P::Text, {0x48, 0x8B, 0x41, 0x10, 0xC3}); });  // mov rax, [rcx+0x10]; ret
  function(p, "v_log", [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });
  // As the sample's: mov edx, site; mov r8, rax; lea rcx, [rip+path]; call log.
  variant(p, "v_get_anvil", kRdi, "vst_Anvil", "vcls_Anvil", 20, [&] {
    p.bytes(P::Text, {0xBA, 0x01, 0x20, 0x00, 0x00, 0x49, 0x89, 0xC0, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "v_path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "v_log");
  });
  // The format string first, then the path (in rcx, after another load into .rdata): mov r9, rax; lea r8, [fmt];
  // mov edx, site; lea rcx, [path]; call log.
  variant(p, "v_get_bellows", kRbx, "vst_Bellows", "vcls_Bellows", 27, [&] {
    p.bytes(P::Text, {0x49, 0x89, 0xC1, 0x4C, 0x8D, 0x05});
    p.rel32(P::Text, "v_format");
    p.bytes(P::Text, {0xBA, 0x02, 0x20, 0x00, 0x00, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "v_path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "v_log");
  });
  // The path in rdx, at another distance: mov r8, rax; lea rdx, [path]; mov ecx, site; call log.
  variant(p, "v_get_crucible", kR12, "vst_Crucible", "vcls_Crucible", 20, [&] {
    p.bytes(P::Text, {0x49, 0x89, 0xC0, 0x48, 0x8D, 0x15});
    p.rel32(P::Text, "v_path");
    p.bytes(P::Text, {0xB9, 0x03, 0x20, 0x00, 0x00, 0xE8});
    p.rel32(P::Text, "v_log");
  });
  // Through a pointer to the path: mov rdx, rax; mov rcx, [rip+slot]; call log.
  variant(p, "v_get_dross", kRdi, "vst_Dross", "vcls_Dross", 15, [&] {
    p.bytes(P::Text, {0x48, 0x89, 0xC2, 0x48, 0x8B, 0x0D});
    p.rel32(P::Text, "v_path_slot");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "v_log");
  });
  // Through its absolute address: mov rdx, rax; mov rcx, imm64 (relocated); call log.
  variant(p, "v_get_ember", kRbx, "vst_Ember", "vcls_Ember", 18, [&] {
    p.bytes(P::Text, {0x48, 0x89, 0xC2, 0x48, 0xB9});
    p.va64(P::Text, "v_path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "v_log");
  });
  // Loaded into rsi before the head: mov rdx, rax; mov rcx, rsi; call log.
  variant(
      p, "v_get_forge", kRdi, "vst_Forge", "vcls_Forge", 11,
      [&] {
        p.bytes(P::Text, {0x48, 0x89, 0xC2, 0x48, 0x89, 0xF1, 0xE8});
        p.rel32(P::Text, "v_log");
      },
      true);
  // A decoy the idiom's wildcards also match: mov rdi, [rbx+0x10] (no RIP-relative static), then the same head.
  function(p, "v_not_idiom", [&] {
    p.bytes(P::Text, {0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x20});
    p.bytes(P::Text, {0x48, 0x8B, 0xBB, 0x10, 0x00, 0x00, 0x00, 0x48, 0x85, 0xFF, 0x75, 0x0C, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "vcls_Anvil");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "v_getter");
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x20, 0x5B, 0x5F, 0x5E, 0xC3});
  });
  // Code that addresses the group table's first entry, and readers of the two statics.
  function(p, "v_first_group", [&] {
    p.bytes(P::Text, {0x48, 0x8D, 0x05});  // lea rax, [rip+table]
    p.rel32(P::Text, "v_groups");
    p.bytes(P::Text, {0xC3});
  });
  for (const char* st : {"vst_plain", "vst_moved"}) {
    function(p, std::string("v_read_") + st, [&] {
      p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x05});  // sub rsp, 0x28; mov rax, [rip+static]
      p.rel32(P::Text, st);
      p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0xC3});
    });
  }
  function(p, "v_moved_kind", [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });
  // RTTI only for a class of Moved's name in another namespace; none for Plain's.
  rtti_class(p, "vmoved", ".?AVMoved@Elsewhere@@", {}, {{0, {"v_moved_kind"}}});

  FakeVersion version;
  version.product_name = u"VARIANT GAME™";
  s.built = p.build(version);
  return s;
}

// The path loaded before the getter's call, in every site: mov rdi, [rip+static]; test rdi, rdi; jnz; lea rsi, [path];
// lea rcx, [class]; call getter; mov rdx, rax; mov rcx, rsi; call log.
Sample build_path_early() {
  Sample s;
  P& p = s.program;
  p.label("e_path", P::Rdata);
  p.ascii(P::Rdata, "early\\Lone.h");
  for (const char* n : {"Gorge", "Heath", "Isle"}) {
    p.label(std::string("ename_") + n, P::Rdata);
    p.ascii(P::Rdata, n);
  }
  p.align(P::Rdata, 8);
  p.label("e_class_table", P::Rdata);
  p.va64(P::Rdata, "e_getter");
  for (const char* n : {"Gorge", "Heath", "Isle"}) statik(p, P::Data, std::string("est_") + n);
  for (const char* n : {"Gorge", "Heath", "Isle"}) {
    p.align(P::Data, 16);
    p.label(std::string("ecls_") + n, P::Data);
    p.va64(P::Data, "e_class_table");
    p.u64(P::Data, 0x40);
    p.va64(P::Data, std::string("ename_") + n);
    p.u64(P::Data, 0);
  }
  function(p, "e_getter", [&] { p.bytes(P::Text, {0x48, 0x8B, 0x41, 0x10, 0xC3}); });
  function(p, "e_log", [&] { p.bytes(P::Text, {0x31, 0xC0, 0xC3}); });
  for (const char* n : {"Gorge", "Heath", "Isle"}) {
    function(p, std::string("e_get_") + n, [&] {
      p.bytes(P::Text, {0x56, 0x57, 0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x3D});
      p.rel32(P::Text, std::string("est_") + n);
      p.bytes(P::Text, {0x48, 0x85, 0xFF, 0x75, 0x1E, 0x48, 0x8D, 0x35});
      p.rel32(P::Text, "e_path");
      p.bytes(P::Text, {0x48, 0x8D, 0x0D});
      p.rel32(P::Text, std::string("ecls_") + n);
      p.bytes(P::Text, {0xE8});
      p.rel32(P::Text, "e_getter");
      p.bytes(P::Text, {0x48, 0x89, 0xC2, 0x48, 0x89, 0xF1, 0xE8});
      p.rel32(P::Text, "e_log");
      p.bytes(P::Text, {0x48, 0x89, 0xF8, 0x48, 0x83, 0xC4, 0x28, 0x5F, 0x5E, 0xC3});
    });
  }
  s.built = p.build();
  return s;
}

}  // namespace en::gal::test
