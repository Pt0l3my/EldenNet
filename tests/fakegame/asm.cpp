// The fake game's code that discovery is to find (ARCH §6.2), written in our own assembly so that its bytes are the
// same under clang-cl and MinGW at every optimisation level: the FD4-style singleton sites, the AOB items, the task
// registration front and the program's own call sites into it. Every name, string and pattern here is invented.
//
// Short jumps are written as bytes (0x75 and the distance between two labels): at -O0 clang relaxes every jump it
// assembles to its long form, which would change the patterns with the optimisation level.
#include <cstddef>

#include "fakegame.hpp"

static_assert(offsetof(FG::TaskLoomFields, group_count) + 8 == FG_LOOM_GROUP_COUNT_OFF);
static_assert(offsetof(FG::TaskLoomFields, flags) + 8 == FG_LOOM_FLAGS_OFF);
static_assert(offsetof(FG::TaskLoomFields, last_task) + 8 == FG_LOOM_LAST_TASK_OFF);
static_assert(offsetof(FG::TaskLoomFields, last_vtable) + 8 == FG_LOOM_LAST_VTABLE_OFF);
static_assert(offsetof(FG::RunWardenFields, save_slot) + 8 == FG_WARDEN_SAVE_SLOT_OFF);
static_assert(offsetof(FG::FieldPlotFields, place_id) + 8 == FG_PLOT_PLACE_OFF);
static_assert(offsetof(FG::TaskFields, hub) + 8 == FG_TASK_HUB_OFF);
static_assert(offsetof(FG::TaskFields, kind) + 8 == FG_TASK_KIND_OFF);
static_assert(offsetof(FG::ShelfFields, word) + 8 == FG_SHELF_WORD_OFF);
static_assert(offsetof(FG::ShelfFields, shared) + 8 == FG_SHELF_SHARED_OFF);
static_assert(offsetof(FG::LedgerFields, flags) + 8 == FG_LEDGER_FLAGS_OFF);
static_assert(offsetof(FG::LedgerFields, count) + 8 == FG_LEDGER_COUNT_OFF);
static_assert(offsetof(FG::LedgerFields, groups) + 8 == FG_LEDGER_GROUPS_OFF);
static_assert(offsetof(FG::LedgerFields, seen) + 8 == FG_LEDGER_SEEN_OFF);
static_assert(offsetof(FG::LedgerFields, items) + 8 == FG_LEDGER_ITEMS_OFF);
static_assert(sizeof(FG::LedgerFields::items) / sizeof(std::uint64_t) == FG_LEDGER_ITEMS);
static_assert(offsetof(FG::TaskLoomFields, desk) + 8 == FG_LOOM_DESK_OFF);
static_assert(offsetof(FgTaskBench, task) == FG_BENCH_TASK_OFF);
static_assert(offsetof(FG::RackFields, shelves) + 8 == FG_RACK_SHELVES_OFF);

#define FG_STR(x) #x
#define FG_XSTR(x) FG_STR(x)

#define FG_FUNC_BEGIN(fn)                       \
  "  .p2align 4, 0xcc\n"                        \
  "  .globl " #fn "\n"                          \
  "  .def " #fn "; .scl 2; .type 32; .endef\n"  \
  "  .seh_proc " #fn "\n" #fn ":\n"

// One FD4-style accessor (ARCH §6.2 step 1's shape): answers the instance in the static, or, while the static is null,
// asks the runtime class for its name and logs that the singleton was used before it was set, then answers null.
// The idiom starts at the accessor's entry + 5, at the label .Lsite_<fn>.
#define FG_FD4_ACCESSOR(fn, statik, rtclass, path, site) \
  FG_FUNC_BEGIN(fn)                                      \
  "  push rbx\n"                                         \
  "  .seh_pushreg rbx\n"                                 \
  "  sub rsp, 0x20\n"                                    \
  "  .seh_stackalloc 0x20\n"                             \
  "  .seh_endprologue\n"                                 \
  ".Lsite_" #fn ":\n"                                    \
  "  mov rbx, qword ptr [rip + " #statik "]\n"           \
  "  test rbx, rbx\n"                                    \
  "  .byte 0x75, 1f - 2f\n"                              \
  "2:\n"                                                 \
  "  lea rcx, [rip + " #rtclass "]\n"                    \
  "  call fg_rtclass_name\n"                             \
  "  mov rdx, rax\n"                                     \
  "  mov r8d, " #site "\n"                               \
  ".Lpath_" #fn ":\n"                                    \
  "  lea rcx, [rip + " #path "]\n"                       \
  "  call fg_fd4_unset\n"                                \
  "1:\n"                                                 \
  "  mov rax, rbx\n"                                     \
  "  add rsp, 0x20\n"                                    \
  "  pop rbx\n"                                          \
  "  ret\n"                                              \
  "  .seh_endproc\n"

// A runtime-class object (the DLRF class's role): a table pointer, an instance size, a parent, and at +0x18 the name.
#define FG_RTCLASS(label, name_label, size) \
  "  .p2align 4\n"                          \
  "  .globl " #label "\n" #label ":\n"      \
  "  .quad fg_rtclass_table\n"              \
  "  .long " #size ", 0\n"                  \
  "  .quad 0\n"                             \
  "  .quad " #name_label "\n"               \
  "  .quad 0\n"

#define FG_STATIC(label) \
  "  .p2align 3\n"       \
  "  .globl " #label "\n" #label ":\n  .quad 0\n"

// A row of the site table: {idiom start, static, runtime class, path, the path's load, its kind (describe.cpp's
// PathKind)}.
#define FG_SITE_ROW_KIND(fn, statik, rtclass, path, kind) \
  "  .quad .Lsite_" #fn ", " #statik ", " #rtclass ", " #path ", .Lpath_" #fn ", " #kind "\n"
#define FG_SITE_ROW(fn, statik, rtclass, path) FG_SITE_ROW_KIND(fn, statik, rtclass, path, 1)

// The epilogue every variant shares: the instance (or null) from `reg`, the frame undone.
#define FG_VARIANT_END(reg, frame, pop) \
  "1:\n"                                \
  "  mov rax, " #reg "\n"               \
  "  add rsp, " #frame "\n"             \
  pop "  ret\n"                         \
  "  .seh_endproc\n"

__asm__(
    "  .intel_syntax noprefix\n"

    // ---- Statics, runtime-class objects and words in .data ----
    "  .data\n"
    FG_STATIC(fg_static_taskloom)
    FG_STATIC(fg_static_roster)
    FG_STATIC(fg_static_lobby)
    FG_STATIC(fg_static_marks)
    FG_STATIC(fg_static_gauges)
    FG_STATIC(fg_static_menu)
    FG_STATIC(fg_static_veil)
    FG_STATIC(fg_static_sky)
    FG_STATIC(fg_static_eye)
    FG_STATIC(fg_static_tables)
    FG_STATIC(fg_static_phrases)
    FG_STATIC(fg_static_duplicata_a)
    FG_STATIC(fg_static_duplicata_b)
    FG_STATIC(fg_static_strayling)
    FG_STATIC(fg_static_save_ledger)
    FG_STATIC(fg_static_run_warden)
    FG_STATIC(fg_static_field_plot)
    FG_STATIC(fg_static_hinstance)
    FG_STATIC(fg_hinstance_check)
    FG_STATIC(fg_static_rack)
    FG_STATIC(fg_static_ledger)
    FG_STATIC(fg_ledger_blank)
    FG_STATIC(fg_ledger_given)
    "  .globl fg_save_ledger_ready\n"
    "fg_save_ledger_ready:\n  .byte 0\n"
    FG_RTCLASS(fg_rtclass_taskloom, fg_name_taskloom, 0x48)
    FG_RTCLASS(fg_rtclass_roster, fg_name_roster, 0x48)
    FG_RTCLASS(fg_rtclass_lobby, fg_name_lobby, 0x18)
    FG_RTCLASS(fg_rtclass_marks, fg_name_marks, 0x18)
    FG_RTCLASS(fg_rtclass_gauges, fg_name_gauges, 0x40)
    FG_RTCLASS(fg_rtclass_menu, fg_name_menu, 0x18)
    FG_RTCLASS(fg_rtclass_veil, fg_name_veil, 0x10)
    FG_RTCLASS(fg_rtclass_sky, fg_name_sky, 0x18)
    FG_RTCLASS(fg_rtclass_eye, fg_name_eye, 0x10)
    FG_RTCLASS(fg_rtclass_tables, fg_name_tables, 0x18)
    FG_RTCLASS(fg_rtclass_phrases, fg_name_phrases, 0x20)
    FG_RTCLASS(fg_rtclass_duplicata_a, fg_name_duplicata, 0x10)
    FG_RTCLASS(fg_rtclass_duplicata_b, fg_name_duplicata, 0x10)
    FG_RTCLASS(fg_rtclass_strayling, fg_name_strayling, 0x10)
    FG_RTCLASS(fg_rtclass_rockbound, fg_name_rockbound, 0x10)
    FG_RTCLASS(fg_rtclass_mimic, fg_name_mimic, 0x10)

    // ---- Strings and tables in .rdata ----
    "  .section .rdata,\"dr\"\n"
    "  .p2align 3\n"
    "  .globl fg_static_rockbound\n"
    "fg_static_rockbound:\n  .quad 0\n"
    "fg_rtclass_table:\n  .quad fg_rtclass_name\n"
    "  .globl fg_fd4_path\n"
    "fg_fd4_path:\n  .asciz \"fakegame\\\\kit\\\\fd4\\\\FD4Singleton.h\"\n"
    // The variants' words: a log format loaded before the path, and a pointer to the path.
    "fg_fd4_format:\n  .asciz \"fakegame: %s was asked for before it was set\"\n"
    "  .p2align 3\n"
    "fg_fd4_path_slot:\n  .quad fg_fd4_path\n"
    "  .globl fg_fd4_path_decoy\n"
    "fg_fd4_path_decoy:\n  .asciz \"fakegame\\\\kit\\\\fd4\\\\FD4Singleton.hpp\"\n"
    "fg_name_taskloom:\n  .asciz \"TaskLoom\"\n"
    "fg_name_roster:\n  .asciz \"ActorRoster\"\n"
    "fg_name_lobby:\n  .asciz \"LobbyWarden\"\n"
    "fg_name_marks:\n  .asciz \"MarkLedger\"\n"
    "fg_name_gauges:\n  .asciz \"GaugeBoard\"\n"
    "fg_name_menu:\n  .asciz \"MenuDesk\"\n"
    "fg_name_veil:\n  .asciz \"VeilKeeper\"\n"
    "fg_name_sky:\n  .asciz \"SkyClock\"\n"
    "fg_name_eye:\n  .asciz \"EyeRig\"\n"
    "fg_name_tables:\n  .asciz \"TableVault\"\n"
    "fg_name_phrases:\n  .asciz \"PhraseBook\"\n"
    "fg_name_duplicata:\n  .asciz \"Duplicata\"\n"
    "fg_name_strayling:\n  .asciz \"Strayling\"\n"
    "fg_name_rockbound:\n  .asciz \"Rockbound\"\n"
    "fg_name_mimic:\n  .asciz \"Mimicry\"\n"
    // Every FD4-style site, for --describe: {idiom start, static, runtime class, path}, then a row of zeros.
    "  .p2align 3\n"
    "  .globl fg_fd4_site_table\n"
    "fg_fd4_site_table:\n"
    FG_SITE_ROW(fg_get_taskloom, fg_static_taskloom, fg_rtclass_taskloom, fg_fd4_path)
    FG_SITE_ROW(fg_get_roster, fg_static_roster, fg_rtclass_roster, fg_fd4_path)
    FG_SITE_ROW(fg_get_roster_near, fg_static_roster, fg_rtclass_roster, fg_fd4_path)
    FG_SITE_ROW(fg_get_lobby, fg_static_lobby, fg_rtclass_lobby, fg_fd4_path)
    FG_SITE_ROW(fg_get_marks, fg_static_marks, fg_rtclass_marks, fg_fd4_path)
    FG_SITE_ROW(fg_get_gauges, fg_static_gauges, fg_rtclass_gauges, fg_fd4_path)
    FG_SITE_ROW(fg_get_menu, fg_static_menu, fg_rtclass_menu, fg_fd4_path)
    FG_SITE_ROW(fg_get_menu_popup_site, fg_static_menu, fg_rtclass_menu, fg_fd4_path)
    FG_SITE_ROW(fg_get_veil, fg_static_veil, fg_rtclass_veil, fg_fd4_path)
    FG_SITE_ROW(fg_get_sky, fg_static_sky, fg_rtclass_sky, fg_fd4_path)
    FG_SITE_ROW(fg_get_eye, fg_static_eye, fg_rtclass_eye, fg_fd4_path)
    FG_SITE_ROW(fg_get_tables, fg_static_tables, fg_rtclass_tables, fg_fd4_path)
    FG_SITE_ROW(fg_get_phrases, fg_static_phrases, fg_rtclass_phrases, fg_fd4_path)
    FG_SITE_ROW(fg_get_duplicata_a, fg_static_duplicata_a, fg_rtclass_duplicata_a, fg_fd4_path)
    FG_SITE_ROW(fg_get_duplicata_b, fg_static_duplicata_b, fg_rtclass_duplicata_b, fg_fd4_path)
    FG_SITE_ROW(fg_get_strayling, fg_static_strayling, fg_rtclass_strayling, fg_fd4_path_decoy)
    FG_SITE_ROW(fg_get_rockbound, fg_static_rockbound, fg_rtclass_rockbound, fg_fd4_path)
    FG_SITE_ROW_KIND(fg_get_lobby_fmt, fg_static_lobby, fg_rtclass_lobby, fg_fd4_path, 2)
    FG_SITE_ROW_KIND(fg_get_marks_rdx, fg_static_marks, fg_rtclass_marks, fg_fd4_path, 3)
    FG_SITE_ROW_KIND(fg_get_gauges_ptr, fg_static_gauges, fg_rtclass_gauges, fg_fd4_path, 4)
    FG_SITE_ROW_KIND(fg_get_veil_abs, fg_static_veil, fg_rtclass_veil, fg_fd4_path, 5)
    FG_SITE_ROW_KIND(fg_get_sky_hoist, fg_static_sky, fg_rtclass_sky, fg_fd4_path, 6)
    FG_SITE_ROW_KIND(fg_not_idiom, 0, fg_rtclass_mimic, fg_fd4_path, 7)
    "  .quad 0, 0, 0, 0, 0, 0\n"
    // The program's own calls into the registration (each call instruction), for --describe; then a zero.
    "  .globl fg_register_call_table\n"
    "fg_register_call_table:\n"
    "  .quad .Lcall_1, .Lcall_2, .Lcall_3, .Lcall_4, 0\n"

    "  .text\n"

    // ---- The name getter: a leaf, and the start of a .pdata function ----
    FG_FUNC_BEGIN(fg_rtclass_name)
    "  .seh_endprologue\n"
    "  mov rax, qword ptr [rcx + " FG_XSTR(FG_RTCLASS_NAME_OFF) "]\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- The FD4-style accessors: eleven names, two of them with a second site, and four decoys ----
    FG_FD4_ACCESSOR(fg_get_taskloom, fg_static_taskloom, fg_rtclass_taskloom, fg_fd4_path, 101)
    FG_FD4_ACCESSOR(fg_get_roster, fg_static_roster, fg_rtclass_roster, fg_fd4_path, 102)
    FG_FD4_ACCESSOR(fg_get_lobby, fg_static_lobby, fg_rtclass_lobby, fg_fd4_path, 103)
    FG_FD4_ACCESSOR(fg_get_marks, fg_static_marks, fg_rtclass_marks, fg_fd4_path, 104)
    FG_FD4_ACCESSOR(fg_get_gauges, fg_static_gauges, fg_rtclass_gauges, fg_fd4_path, 105)
    FG_FD4_ACCESSOR(fg_get_menu, fg_static_menu, fg_rtclass_menu, fg_fd4_path, 106)
    FG_FD4_ACCESSOR(fg_get_veil, fg_static_veil, fg_rtclass_veil, fg_fd4_path, 107)
    FG_FD4_ACCESSOR(fg_get_sky, fg_static_sky, fg_rtclass_sky, fg_fd4_path, 108)
    FG_FD4_ACCESSOR(fg_get_eye, fg_static_eye, fg_rtclass_eye, fg_fd4_path, 109)
    FG_FD4_ACCESSOR(fg_get_tables, fg_static_tables, fg_rtclass_tables, fg_fd4_path, 110)
    FG_FD4_ACCESSOR(fg_get_phrases, fg_static_phrases, fg_rtclass_phrases, fg_fd4_path, 111)
    FG_FD4_ACCESSOR(fg_get_roster_near, fg_static_roster, fg_rtclass_roster, fg_fd4_path, 112)
    FG_FD4_ACCESSOR(fg_get_menu_popup_site, fg_static_menu, fg_rtclass_menu, fg_fd4_path, 113)
    FG_FD4_ACCESSOR(fg_get_duplicata_a, fg_static_duplicata_a, fg_rtclass_duplicata_a, fg_fd4_path, 121)
    FG_FD4_ACCESSOR(fg_get_duplicata_b, fg_static_duplicata_b, fg_rtclass_duplicata_b, fg_fd4_path, 122)
    FG_FD4_ACCESSOR(fg_get_strayling, fg_static_strayling, fg_rtclass_strayling, fg_fd4_path_decoy, 123)
    FG_FD4_ACCESSOR(fg_get_rockbound, fg_static_rockbound, fg_rtclass_rockbound, fg_fd4_path, 124)

    // ---- The variants of R0c: second sites of five singletons, whose heads (the static's load in another register;
    // test; jnz; lea rcx, [rip+class]; call getter) keep the idiom's shape while the path is loaded otherwise, and a
    // decoy whose "static" is no RIP-relative operand. Never called: they are there to be found.
    // LobbyWarden's: the static in rdi; a format string loaded before the path, as a log call with four arguments
    // would (mov r9, rax; lea r8, [fmt]; mov edx, site; lea rcx, [path]).
    FG_FUNC_BEGIN(fg_get_lobby_fmt)
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    ".Lsite_fg_get_lobby_fmt:\n"
    "  mov rdi, qword ptr [rip + fg_static_lobby]\n"
    "  test rdi, rdi\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  lea rcx, [rip + fg_rtclass_lobby]\n"
    "  call fg_rtclass_name\n"
    "  mov r9, rax\n"
    "  lea r8, [rip + fg_fd4_format]\n"
    "  mov edx, 131\n"
    ".Lpath_fg_get_lobby_fmt:\n"
    "  lea rcx, [rip + fg_fd4_path]\n"
    "  call fg_fd4_unset\n"
    FG_VARIANT_END(rdi, 0x20, "  pop rdi\n")
    // MarkLedger's: the static in r12; the path in rdx, loaded right after the getter's call.
    FG_FUNC_BEGIN(fg_get_marks_rdx)
    "  push r12\n"
    "  .seh_pushreg r12\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    ".Lsite_fg_get_marks_rdx:\n"
    "  mov r12, qword ptr [rip + fg_static_marks]\n"
    "  test r12, r12\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  lea rcx, [rip + fg_rtclass_marks]\n"
    "  call fg_rtclass_name\n"
    ".Lpath_fg_get_marks_rdx:\n"
    "  lea rdx, [rip + fg_fd4_path]\n"
    "  mov r8, rax\n"
    "  mov ecx, 132\n"
    "  call fg_fd4_unset\n"
    FG_VARIANT_END(r12, 0x20, "  pop r12\n")
    // GaugeBoard's: the static in rsi; the path through a pointer to it, read from .rdata.
    FG_FUNC_BEGIN(fg_get_gauges_ptr)
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    ".Lsite_fg_get_gauges_ptr:\n"
    "  mov rsi, qword ptr [rip + fg_static_gauges]\n"
    "  test rsi, rsi\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  lea rcx, [rip + fg_rtclass_gauges]\n"
    "  call fg_rtclass_name\n"
    "  mov rdx, rax\n"
    ".Lpath_fg_get_gauges_ptr:\n"
    "  mov rcx, qword ptr [rip + fg_fd4_path_slot]\n"
    "  mov r8d, 133\n"
    "  call fg_fd4_unset\n"
    FG_VARIANT_END(rsi, 0x20, "  pop rsi\n")
    // VeilKeeper's: the static in rbx; the path by its absolute address, which the loader relocates.
    FG_FUNC_BEGIN(fg_get_veil_abs)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    ".Lsite_fg_get_veil_abs:\n"
    "  mov rbx, qword ptr [rip + fg_static_veil]\n"
    "  test rbx, rbx\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  lea rcx, [rip + fg_rtclass_veil]\n"
    "  call fg_rtclass_name\n"
    "  mov rdx, rax\n"
    ".Lpath_fg_get_veil_abs:\n"
    "  movabs rcx, offset fg_fd4_path\n"
    "  mov r8d, 134\n"
    "  call fg_fd4_unset\n"
    FG_VARIANT_END(rbx, 0x20, "  pop rbx\n")
    // SkyClock's: the path loaded into rsi before the head, the static in rdi.
    FG_FUNC_BEGIN(fg_get_sky_hoist)
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x28\n"
    "  .seh_stackalloc 0x28\n"
    "  .seh_endprologue\n"
    ".Lpath_fg_get_sky_hoist:\n"
    "  lea rsi, [rip + fg_fd4_path]\n"
    ".Lsite_fg_get_sky_hoist:\n"
    "  mov rdi, qword ptr [rip + fg_static_sky]\n"
    "  test rdi, rdi\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  lea rcx, [rip + fg_rtclass_sky]\n"
    "  call fg_rtclass_name\n"
    "  mov rdx, rax\n"
    "  mov rcx, rsi\n"
    "  mov r8d, 135\n"
    "  call fg_fd4_unset\n"
    FG_VARIANT_END(rdi, 0x28, "  pop rdi\n  pop rsi\n")
    // The decoy: its "static" is read through rsi (mov rbx, [rsi-0x70000000]), which the idiom's wildcards also match;
    // read as RIP-relative, as R0's rule read it, the displacement leads outside the image.
    FG_FUNC_BEGIN(fg_not_idiom)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    ".Lsite_fg_not_idiom:\n"
    "  mov rbx, qword ptr [rsi - 0x70000000]\n"
    "  test rbx, rbx\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  lea rcx, [rip + fg_rtclass_mimic]\n"
    "  call fg_rtclass_name\n"
    "  mov rdx, rax\n"
    "  mov r8d, 136\n"
    ".Lpath_fg_not_idiom:\n"
    "  lea rcx, [rip + fg_fd4_path]\n"
    "  call fg_fd4_unset\n"
    FG_VARIANT_END(rbx, 0x20, "  pop rbx\n")

    // ---- global_hinstance's role: stores the module handle and a check word made from it; answers the old value.
    // Pattern from entry + 5; the static is the third RIP-relative operand.
    FG_FUNC_BEGIN(fg_boot_publish_instance)
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov rdi, rcx\n"
    "  bswap ecx\n"
    "  ror rcx, 0x0B\n"
    "  mov qword ptr [rip + fg_hinstance_check], rcx\n"
    "  mov rax, qword ptr [rip + fg_static_hinstance]\n"
    "  mov qword ptr [rip + fg_static_hinstance], rdi\n"
    "  add rsp, 0x20\n"
    "  pop rdi\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- GameDataMan's role: swaps the SaveLedger static and notes whether it is set; answers the old value ----
    FG_FUNC_BEGIN(fg_save_ledger_swap)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov rbx, rcx\n"
    "  xchg qword ptr [rip + fg_static_save_ledger], rcx\n"
    "  test rbx, rbx\n"
    "  setne dl\n"
    "  mov byte ptr [rip + fg_save_ledger_ready], dl\n"
    "  mov rax, rcx\n"
    "  add rsp, 0x20\n"
    "  pop rbx\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- GameMan's role: the save slot through the RunWarden static, or -1 ----
    FG_FUNC_BEGIN(fg_run_warden_slot)
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov r9, qword ptr [rip + fg_static_run_warden]\n"
    "  or eax, -1\n"
    "  test r9, r9\n"
    "  .byte 0x74, 1f - 2f\n"
    "2:\n"
    "  mov eax, dword ptr [r9 + " FG_XSTR(FG_WARDEN_SAVE_SLOT_OFF) "]\n"
    "1:\n"
    "  add rsp, 0x20\n"
    "  pop rsi\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- FieldArea's role: the place id through the FieldPlot static, or 0 ----
    FG_FUNC_BEGIN(fg_field_plot_place)
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov r10, qword ptr [rip + fg_static_field_plot]\n"
    "  xor eax, eax\n"
    "  test r10, r10\n"
    "  .byte 0x74, 1f - 2f\n"
    "2:\n"
    "  movsxd rax, dword ptr [r10 + " FG_XSTR(FG_PLOT_PLACE_OFF) "]\n"
    "1:\n"
    "  add rsp, 0x20\n"
    "  pop rdi\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- register_task's role, with our own ABI: rcx the task, rdx the hub instance, r8d the group; it answers 1 when
    // the task was kept, else 0. A wrapper of the shape R1 run 7 found (WHY§240): it keeps its arguments in callee-saved
    // registers, loads its own singleton (the rack) from fg_static_rack and, while that is null, makes it with two calls
    // and stores it there; then, where both ways meet, it tail-jumps to fg_register_rack with the rack in rcx, the group
    // in edx, the task in r8 and the hub it was given in r9. R1's callee summary is checked against it.
    FG_FUNC_BEGIN(fg_register_task)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov rbx, rcx\n"
    "  mov rsi, rdx\n"
    "  mov edi, r8d\n"
    "  mov rcx, qword ptr [rip + fg_static_rack]\n"
    "  test rcx, rcx\n"
    "  .byte 0x75, 1f - 2f\n"
    "2:\n"
    "  call fg_rack_alloc\n"
    "  mov rcx, rax\n"
    "  mov rdx, rsi\n"
    "  call fg_rack_build\n"
    "  mov rcx, rax\n"
    "  mov qword ptr [rip + fg_static_rack], rax\n"
    "1:\n"
    "  mov edx, edi\n"
    "  mov r8, rbx\n"
    "  mov r9, rsi\n"
    "  add rsp, 0x20\n"
    "  pop rdi\n"
    "  pop rsi\n"
    "  pop rbx\n"
    "  .byte 0xE9\n"  // jmp fg_register_rack, always in its long form
    "  .long fg_register_rack - 3f\n"
    "3:\n"
    "  .seh_endproc\n"

    // ---- The tail function (run 7's shape, its end run 8's): the group bounded, the rack's shelf for it loaded and
    // tested for null (0 answered), else the task moved into rdx and a direct jump to the group's own function, the
    // shelf in rcx and the hub still in r9. A leaf with no unwind data, so R1 reads it up to its first return; the two
    // traps after the jump are padding no path reaches.
    "  .p2align 4, 0xcc\n"
    "  .globl fg_register_rack\n"
    "fg_register_rack:\n"
    "  cmp edx, " FG_XSTR(FG_RACK_BOUND) "\n"
    "  .byte 0x77, 1f - 2f\n"
    "2:\n"
    "  mov rcx, qword ptr [rcx + rdx*8 + " FG_XSTR(FG_RACK_SHELVES_OFF) "]\n"
    "  test rcx, rcx\n"
    "  .byte 0x74, 1f - 3f\n"
    "3:\n"
    "  mov rdx, r8\n"
    "  .byte 0xE9\n"  // jmp fg_shelf_take, always in its long form
    "  .long fg_shelf_take - 4f\n"
    "4:\n"
    "  int3\n"
    "  int3\n"
    "1:\n"
    "  xor eax, eax\n"
    "  ret\n"

    // ---- The group's own function (since spike.10 R1 run 9's shape, WHY§289): the shelf in rcx, the task in rdx (and
    // r8), the hub in r9. A null task answers 0. Else it loads the ledger (G) from its static, while that is null has it
    // made (fg_ledger_missing) and loads it again, keeps the hub it was given in fg_ledger_given (for the work: a call
    // with its arguments swapped is still refused at the hub), puts the shelf's word in r8 and tail-jumps through the
    // stub to the ledger's method, the ledger in rcx and the task in rdx. It reads nothing of the task.
    FG_FUNC_BEGIN(fg_shelf_take)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x28\n"
    "  .seh_stackalloc 0x28\n"
    "  .seh_endprologue\n"
    "  mov rbx, rdx\n"
    "  mov rdi, rcx\n"
    "  test rdx, rdx\n"
    "  .byte 0x74, 1f - 2f\n"
    "2:\n"
    "  mov rcx, qword ptr [rip + fg_static_ledger]\n"
    "  test rcx, rcx\n"
    "  .byte 0x75, 3f - 4f\n"
    "4:\n"
    "  call fg_ledger_missing\n"
    "  mov rcx, qword ptr [rip + fg_static_ledger]\n"
    "3:\n"
    "  mov qword ptr [rip + fg_ledger_given], r9\n"
    "  mov r8d, dword ptr [rdi + " FG_XSTR(FG_SHELF_WORD_OFF) "]\n"
    "  mov rdx, rbx\n"
    "  add rsp, 0x28\n"
    "  pop rdi\n"
    "  pop rbx\n"
    "  .byte 0xE9\n"  // jmp fg_ledger_stub, always in its long form
    "  .long fg_ledger_stub - 5f\n"
    "5:\n"
    "1:\n"
    "  xor eax, eax\n"
    "  add rsp, 0x28\n"
    "  pop rdi\n"
    "  pop rbx\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- The stub (R1 run 9's, 8 bytes): r9 zeroed, a tail jump to the ledger's method. A leaf with no unwind data.
    "  .p2align 4, 0xcc\n"
    "  .globl fg_ledger_stub\n"
    "fg_ledger_stub:\n"
    "  xor r9d, r9d\n"
    "  .byte 0xE9\n"  // jmp fg_ledger_admit, always in its long form
    "  .long fg_ledger_admit - 1f\n"
    "1:\n"
    "  int3\n"

    // ---- The ledger's method (G's, spike.10): the ledger in rcx, the task in rdx, the shelf's word in r8d, r9 zero. It
    // notes the task in the hub's rows; refuses a word past the rack's bound, or at or past the ledger's groups; reads
    // the task's hub field (kept on the ledger) and its kind; where the ledger is strict (--task-gate) and the kind is
    // not the game's, answers 1 keeping nothing (R1 run 9's end); else keeps the task in its list under the tag while
    // the list has room, and hands it to the work with the word as its group. It answers 1.
    FG_FUNC_BEGIN(fg_ledger_admit)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov rbx, rcx\n"
    "  mov rsi, rdx\n"
    "  mov edi, r8d\n"
    "  call fg_ledger_note\n"
    "  cmp edi, " FG_XSTR(FG_RACK_BOUND) "\n"
    "  .byte 0x77, 1f - 2f\n"
    "2:\n"
    "  cmp edi, dword ptr [rbx + " FG_XSTR(FG_LEDGER_GROUPS_OFF) "]\n"
    "  .byte 0x73, 1f - 3f\n"
    "3:\n"
    "  mov r10, qword ptr [rsi + " FG_XSTR(FG_TASK_HUB_OFF) "]\n"
    "  mov qword ptr [rbx + " FG_XSTR(FG_LEDGER_SEEN_OFF) "], r10\n"
    "  mov r11d, dword ptr [rsi + " FG_XSTR(FG_TASK_KIND_OFF) "]\n"
    "  test byte ptr [rbx + " FG_XSTR(FG_LEDGER_FLAGS_OFF) "], 1\n"
    "  .byte 0x74, 4f - 5f\n"
    "5:\n"
    "  cmp r11d, " FG_XSTR(FG_TASK_KIND) "\n"
    "  .byte 0x75, 1f - 4f\n"
    "4:\n"
    "  mov eax, dword ptr [rbx + " FG_XSTR(FG_LEDGER_COUNT_OFF) "]\n"
    "  cmp eax, " FG_XSTR(FG_LEDGER_ITEMS) "\n"
    "  .byte 0x73, 6f - 7f\n"
    "7:\n"
    "  mov rcx, rsi\n"
    "  movabs rdx, " FG_XSTR(FG_TASK_TAG) "\n"
    "  or rcx, rdx\n"
    "  mov qword ptr [rbx + rax*8 + " FG_XSTR(FG_LEDGER_ITEMS_OFF) "], rcx\n"
    "  inc eax\n"
    "  mov dword ptr [rbx + " FG_XSTR(FG_LEDGER_COUNT_OFF) "], eax\n"
    "6:\n"
    "  mov rcx, rbx\n"
    "  mov edx, edi\n"
    "  mov r8, rsi\n"
    "  call fg_ledger_keep\n"
    "1:\n"
    "  mov eax, 1\n"
    "  add rsp, 0x20\n"
    "  pop rdi\n"
    "  pop rsi\n"
    "  pop rbx\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- The work, with the hub in rcx, the group in edx and the task in r8. It checks the hub against its static,
    // the group against a fixed bound (an immediate) and the hub's count and the task against null, keeps the task and
    // its vtable in the hub's last_task and last_vtable fields, then queues the task. Anything else is refused by
    // fg_register_rejected, which faults (a game given bad arguments would not check).
    FG_FUNC_BEGIN(fg_register_keep)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  push rdi\n"
    "  .seh_pushreg rdi\n"
    "  sub rsp, 0x30\n"
    "  .seh_stackalloc 0x30\n"
    "  .seh_endprologue\n"
    "  mov rsi, rcx\n"
    "  mov edi, edx\n"
    "  mov rbx, r8\n"
    "  mov rax, qword ptr [rip + fg_static_taskloom]\n"
    "  cmp rax, rsi\n"
    "  .byte 0x75, 4f - 5f\n"
    "5:\n"
    "  cmp edi, " FG_XSTR(FG_GROUP_BOUND) "\n"
    "  .byte 0x73, 6f - 2f\n"
    "2:\n"
    "  cmp edi, dword ptr [rsi + " FG_XSTR(FG_LOOM_GROUP_COUNT_OFF) "]\n"
    "  .byte 0x73, 6f - 7f\n"
    "7:\n"
    "  test rbx, rbx\n"
    "  .byte 0x74, 8f - 9f\n"
    "9:\n"
    "  mov qword ptr [rsi + " FG_XSTR(FG_LOOM_LAST_TASK_OFF) "], rbx\n"
    "  mov rax, qword ptr [rbx]\n"
    "  mov qword ptr [rsi + " FG_XSTR(FG_LOOM_LAST_VTABLE_OFF) "], rax\n"
    "  mov r8, rbx\n"
    "  mov edx, edi\n"
    "  mov rcx, rsi\n"
    "  call fg_hub_enqueue\n"
    "3:\n"
    "  add rsp, 0x30\n"
    "  pop rdi\n"
    "  pop rsi\n"
    "  pop rbx\n"
    "  ret\n"
    "4:\n"
    "  mov ecx, 1\n"
    "  call fg_register_rejected\n"
    "  .byte 0xEB, 3b - 1f\n"
    "1:\n"
    "6:\n"
    "  mov ecx, 2\n"
    "  call fg_register_rejected\n"
    "  .byte 0xEB, 3b - 1f\n"
    "1:\n"
    "8:\n"
    "  mov ecx, 3\n"
    "  call fg_register_rejected\n"
    "  .byte 0xEB, 3b - 1f\n"
    "1:\n"
    "  .seh_endproc\n"

    // ---- The program's own call sites into the registration: each loads the hub from its static into rdx. Since
    // spike.10 each passes its task as a game's call site might (R1's game_sites): loaded from the rows, the bench's
    // member, an allocator's result, loaded.
    FG_FUNC_BEGIN(fg_boot_register_tasks)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  push rsi\n"
    "  .seh_pushreg rsi\n"
    "  sub rsp, 0x28\n"
    "  .seh_stackalloc 0x28\n"
    "  .seh_endprologue\n"
    "  mov rbx, rcx\n"
    "  mov rsi, rdx\n"
    "  mov rdx, qword ptr [rip + fg_static_taskloom]\n"
    "  mov rcx, qword ptr [rbx]\n"
    "  mov r8d, dword ptr [rbx + 0x08]\n"
    ".Lcall_1:\n"
    "  call fg_register_task\n"
    "  mov rdx, qword ptr [rip + fg_static_taskloom]\n"
    "  lea rcx, [rsi + " FG_XSTR(FG_BENCH_TASK_OFF) "]\n"
    "  mov r8d, dword ptr [rbx + 0x18]\n"
    ".Lcall_2:\n"
    "  call fg_register_task\n"
    "  mov ecx, 2\n"
    "  call fg_task_make\n"
    "  mov rcx, rax\n"
    "  mov rdx, qword ptr [rip + fg_static_taskloom]\n"
    "  mov r8d, dword ptr [rbx + 0x28]\n"
    ".Lcall_3:\n"
    "  call fg_register_task\n"
    "  mov rdx, qword ptr [rip + fg_static_taskloom]\n"
    "  mov rcx, qword ptr [rbx + 0x30]\n"
    "  mov r8d, dword ptr [rbx + 0x38]\n"
    ".Lcall_4:\n"
    "  call fg_register_task\n"
    "  add rsp, 0x28\n"
    "  pop rsi\n"
    "  pop rbx\n"
    "  ret\n"
    "  .seh_endproc\n"

    // ---- The registration's sibling (spike.10): our own ABI too (rcx the task, rdx the hub, r8d the group); it hands
    // the task to the ledger to forget (a tail jump). Its two call sites are never run: they are there to be found.
    FG_FUNC_BEGIN(fg_unregister_task)
    "  .seh_endprologue\n"
    "  mov r9, rdx\n"
    "  mov rdx, rcx\n"
    "  mov rcx, qword ptr [rip + fg_static_ledger]\n"
    "  .byte 0xE9\n"  // jmp fg_ledger_forget, always in its long form
    "  .long fg_ledger_forget - 1f\n"
    "1:\n"
    "  .seh_endproc\n"
    FG_FUNC_BEGIN(fg_boot_forget_tasks)
    "  push rbx\n"
    "  .seh_pushreg rbx\n"
    "  sub rsp, 0x20\n"
    "  .seh_stackalloc 0x20\n"
    "  .seh_endprologue\n"
    "  mov rbx, rcx\n"
    "  mov rdx, qword ptr [rip + fg_static_taskloom]\n"
    "  mov rcx, qword ptr [rbx]\n"
    "  mov r8d, dword ptr [rbx + 0x08]\n"
    "  call fg_unregister_task\n"
    "  mov rdx, qword ptr [rip + fg_static_taskloom]\n"
    "  mov rcx, qword ptr [rbx + 0x10]\n"
    "  mov r8d, dword ptr [rbx + 0x18]\n"
    "  call fg_unregister_task\n"
    "  add rsp, 0x20\n"
    "  pop rbx\n"
    "  ret\n"
    "  .seh_endproc\n"

    "  .att_syntax prefix\n");
