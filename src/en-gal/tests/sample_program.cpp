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

}  // namespace en::gal::test
