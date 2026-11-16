// en-gal's x86-64 length decoder (x86.hpp) on encodings of our own choosing, each worked out from the architecture's
// encoding rules. tests/x86_objdump.py compares it with LLVM's disassembler over whole binaries and random bytes.
#include <doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "en/gal/x86.hpp"

namespace {

using en::gal::x86::decode;
using en::gal::x86::DecodeProblem;
using en::gal::x86::Encoding;
using en::gal::x86::Flow;
using en::gal::x86::Instruction;

std::vector<std::byte> hex(const std::string& text) {
  std::vector<std::byte> out;
  std::string digits;
  for (const char c : text) {
    if (c != ' ') digits += c;
  }
  for (std::size_t i = 0; i + 1 < digits.size(); i += 2) {
    out.push_back(std::byte{static_cast<std::uint8_t>(std::stoul(digits.substr(i, 2), nullptr, 16))});
  }
  return out;
}

struct Case {
  const char* bytes;
  int length;
  int disp_at = 0;
  int disp_size = 0;
  bool rip = false;
  int imm_at = 0;
  int imm_size = 0;
  bool relative = false;
  Flow flow = Flow::None;
};

// Each case's bytes are followed by filler, so that a decoder that reads too far is seen to.
void check(const Case& c) {
  CAPTURE(c.bytes);
  std::vector<std::byte> code = hex(c.bytes);
  const std::size_t exact = code.size();
  code.insert(code.end(), 16, std::byte{0x90});
  const auto in = decode(code);
  REQUIRE(in.has_value());
  CHECK(in->length == c.length);
  CHECK(static_cast<std::size_t>(in->length) == exact);
  CHECK(in->disp_size == c.disp_size);
  if (c.disp_size != 0) CHECK(in->disp_at == c.disp_at);
  CHECK(in->rip_relative == c.rip);
  CHECK(in->imm_size == c.imm_size);
  if (c.imm_size != 0) CHECK(in->imm_at == c.imm_at);
  CHECK(in->relative == c.relative);
  CHECK(in->flow == c.flow);
  // Every shorter run of the same bytes is truncated, never misread.
  for (std::size_t n = 0; n < exact; ++n) {
    const auto cut = decode(std::span<const std::byte>(code).first(n));
    REQUIRE_FALSE(cut.has_value());
    CHECK(cut.error() == DecodeProblem::Truncated);
  }
}

}  // namespace

TEST_CASE("x86: one-byte opcodes, ModRM and its displacements") {
  const Case cases[] = {
      {"90", 1},                                                // nop
      {"C3", 1, 0, 0, false, 0, 0, false, Flow::Return},        // ret
      {"CC", 1, 0, 0, false, 0, 0, false, Flow::Trap},          // int3
      {"53", 1},                                                // push rbx
      {"41 57", 2},                                             // push r15
      {"48 89 C8", 3},                                          // mov rax, rcx (mod 11)
      {"48 8B 03", 3},                                          // mov rax, [rbx] (mod 00)
      {"48 8B 43 08", 4, 3, 1},                                 // mov rax, [rbx+8] (disp8)
      {"48 8B 83 00 01 00 00", 7, 3, 4},                        // mov rax, [rbx+0x100] (disp32)
      {"48 8B 04 24", 4},                                       // mov rax, [rsp] (SIB)
      {"48 8B 44 24 20", 5, 4, 1},                              // mov rax, [rsp+0x20] (SIB, disp8)
      {"48 8B 04 C5 10 00 00 00", 8, 4, 4},                     // mov rax, [rax*8+0x10] (SIB, no base: disp32)
      {"48 8B 45 00", 4, 3, 1},                                 // mov rax, [rbp+0] (rbp needs disp8)
      {"49 8B 45 00", 4, 3, 1},                                 // mov rax, [r13+0]
      {"49 8B 04 24", 4},                                       // mov rax, [r12] (SIB)
      {"48 8B 05 F9 0F 00 00", 7, 3, 4, true},                  // mov rax, [rip+0xff9]
      {"48 8B 1D 11 22 33 44", 7, 3, 4, true},                  // mov rbx, [rip+disp32]
      {"48 8D 0D 00 00 00 80", 7, 3, 4, true},                  // lea rcx, [rip-0x80000000]
      {"48 87 0D 01 02 03 04", 7, 3, 4, true},                  // xchg [rip+disp32], rcx
      {"C7 05 01 02 03 04 05 06 07 08", 10, 2, 4, true, 6, 4},  // mov dword [rip+disp32], imm32
      {"66 C7 05 01 02 03 04 05 06", 9, 3, 4, true, 7, 2},      // mov word [rip+disp32], imm16
      {"80 3D 01 02 03 04 05", 7, 2, 4, true, 6, 1},            // cmp byte [rip+disp32], imm8
  };
  for (const Case& c : cases) check(c);
}

TEST_CASE("x86: immediates of every size, and what 66 and REX.W do to them") {
  const Case cases[] = {
      {"04 7F", 2, 0, 0, false, 1, 1},                           // add al, imm8
      {"05 01 02 03 04", 5, 0, 0, false, 1, 4},                  // add eax, imm32
      {"66 05 01 02", 4, 0, 0, false, 2, 2},                     // add ax, imm16
      {"48 05 01 02 03 04", 6, 0, 0, false, 2, 4},               // add rax, imm32 (sign-extended)
      {"66 48 05 01 02 03 04", 7, 0, 0, false, 3, 4},            // REX.W wins over 66
      {"83 C0 01", 3, 0, 0, false, 2, 1},                        // add eax, imm8
      {"81 C1 00 01 00 00", 6, 0, 0, false, 2, 4},               // add ecx, imm32
      {"B8 01 02 03 04", 5, 0, 0, false, 1, 4},                  // mov eax, imm32
      {"66 B8 01 02", 4, 0, 0, false, 2, 2},                     // mov ax, imm16
      {"48 B8 01 02 03 04 05 06 07 08", 10, 0, 0, false, 2, 8},  // mov rax, imm64
      {"49 BF 01 02 03 04 05 06 07 08", 10, 0, 0, false, 2, 8},  // mov r15, imm64
      {"6A 05", 2, 0, 0, false, 1, 1},                           // push imm8
      {"68 01 02 03 04", 5, 0, 0, false, 1, 4},                  // push imm32
      {"69 C1 01 02 03 04", 6, 0, 0, false, 2, 4},               // imul eax, ecx, imm32
      {"6B C1 05", 3, 0, 0, false, 2, 1},                        // imul eax, ecx, imm8
      {"C1 E0 04", 3, 0, 0, false, 2, 1},                        // shl eax, 4
      {"D1 E0", 2},                                              // shl eax, 1 (no immediate)
      {"F6 C1 01", 3, 0, 0, false, 2, 1},                        // test cl, imm8 (F6 /0)
      {"F7 C1 01 02 03 04", 6, 0, 0, false, 2, 4},               // test ecx, imm32 (F7 /0)
      {"66 F7 C1 01 02", 5, 0, 0, false, 3, 2},                  // test cx, imm16
      {"F7 D1", 2},                                              // not ecx (F7 /2: no immediate)
      {"F6 D9", 2},                                              // neg cl (F6 /3)
      {"C2 08 00", 3, 0, 0, false, 1, 2, false, Flow::Return},   // ret imm16
      {"CD 2E", 2, 0, 0, false, 1, 1},                           // int imm8
      {"E4 60", 2, 0, 0, false, 1, 1},                           // in al, imm8
      {"A8 01", 2, 0, 0, false, 1, 1},                           // test al, imm8
      {"C6 00 05", 3, 0, 0, false, 2, 1},                        // mov byte [rax], imm8
      {"C6 F8 05", 3, 0, 0, false, 2, 1},                        // xabort imm8
  };
  for (const Case& c : cases) check(c);
}

TEST_CASE("x86: relative branches and moffs") {
  const Case cases[] = {
      {"EB FE", 2, 0, 0, false, 1, 1, true, Flow::Jump},                      // jmp rel8
      {"E9 01 02 03 04", 5, 0, 0, false, 1, 4, true, Flow::Jump},             // jmp rel32
      {"66 E9 01 02", 4, 0, 0, false, 2, 2, true, Flow::Jump},                // jmp rel16 (LLVM's reading)
      {"E8 01 02 03 04", 5, 0, 0, false, 1, 4, true, Flow::Call},             // call rel32
      {"66 E8 01 02 03 04", 6, 0, 0, false, 2, 4, true, Flow::Call},          // 66 does not shorten CALL
      {"75 21", 2, 0, 0, false, 1, 1, true, Flow::Branch},                    // jnz rel8
      {"0F 85 01 02 03 04", 6, 0, 0, false, 2, 4, true, Flow::Branch},        // jnz rel32
      {"E2 FE", 2, 0, 0, false, 1, 1, true, Flow::Branch},                    // loop
      {"E3 05", 2, 0, 0, false, 1, 1, true, Flow::Branch},                    // jrcxz
      {"C7 F8 01 02 03 04", 6, 0, 0, false, 2, 4, true, Flow::Branch},        // xbegin rel32
      {"FF D0", 2, 0, 0, false, 0, 0, false, Flow::CallIndirect},             // call rax
      {"FF 15 01 02 03 04", 6, 2, 4, true, 0, 0, false, Flow::CallIndirect},  // call [rip+disp32]
      {"FF E0", 2, 0, 0, false, 0, 0, false, Flow::JumpIndirect},             // jmp rax
      {"A0 01 02 03 04 05 06 07 08", 9, 1, 8},                                // mov al, [moffs64]
      {"48 A3 01 02 03 04 05 06 07 08", 10, 2, 8},                            // mov [moffs64], rax
      {"67 A1 01 02 03 04", 6, 2, 4},                                         // mov eax, [moffs32]
      {"C8 10 00 01", 4, 0, 0, false, 1, 2},                                  // enter 16, 1 (two immediates)
  };
  for (const Case& c : cases) check(c);
  const auto moffs = decode(hex("A0 01 02 03 04 05 06 07 08"));
  REQUIRE(moffs.has_value());
  CHECK(moffs->absolute);
  const auto enter = decode(hex("C8 10 00 01"));
  REQUIRE(enter.has_value());
  CHECK(enter->imm2_at == 3);
  CHECK(enter->imm2_size == 1);
}

TEST_CASE("x86: prefixes, REX and their order") {
  const Case cases[] = {
      {"F0 48 0F C1 01", 5},                        // lock xadd [rcx], rax
      {"F3 48 AB", 3},                              // rep stosq
      {"66 2E 0F 1F 84 00 00 00 00 00", 10, 6, 4},  // nopw cs:[rax+rax+0]
      {"66 66 2E 0F 1F 84 00 00 00 00 00", 11, 7, 4},
      {"0F 1F 44 00 00", 5, 4, 1},                                  // nop dword [rax+rax+0]
      {"F3 0F 1E FA", 4},                                           // endbr64
      {"64 48 8B 04 25 28 00 00 00", 9, 5, 4},                      // mov rax, fs:[0x28]
      {"65 48 8B 04 25 30 00 00 00", 9, 5, 4},                      // mov rax, gs:[0x30]
      {"48 66 05 01 02", 5, 0, 0, false, 3, 2},                     // a REX before 66 is ignored: add ax, imm16
      {"40 48 B8 01 02 03 04 05 06 07 08", 11, 0, 0, false, 3, 8},  // the last REX counts
  };
  for (const Case& c : cases) check(c);
  const auto in = decode(hex("F0 48 0F C1 01"));
  REQUIRE(in.has_value());
  CHECK(in->lock);
  CHECK(in->rex == 0x48);
  CHECK(in->wide);
  CHECK(in->prefixes == 2);
  CHECK(in->map == 1);
  CHECK(in->opcode == 0xC1);
  const auto ignored = decode(hex("48 66 05 01 02"));
  REQUIRE(ignored.has_value());
  CHECK(ignored->rex == 0);
  CHECK_FALSE(ignored->wide);
}

TEST_CASE("x86: the two- and three-byte maps") {
  const Case cases[] = {
      {"0F 05", 2},                                             // syscall
      {"0F 0B", 2, 0, 0, false, 0, 0, false, Flow::Trap},       // ud2
      {"0F A2", 2},                                             // cpuid
      {"0F B6 C1", 3},                                          // movzx eax, cl
      {"0F AF 05 01 02 03 04", 7, 3, 4, true},                  // imul eax, [rip+disp32]
      {"0F BA E0 05", 4, 0, 0, false, 3, 1},                    // bt eax, 5
      {"0F A4 C1 03", 4, 0, 0, false, 3, 1},                    // shld ecx, eax, 3
      {"0F C8", 2},                                             // bswap eax
      {"0F 10 05 01 02 03 04", 7, 3, 4, true},                  // movups xmm0, [rip+disp32]
      {"66 0F 70 C1 1B", 5, 0, 0, false, 4, 1},                 // pshufd xmm0, xmm1, 0x1b
      {"66 0F 73 D8 04", 5, 0, 0, false, 4, 1},                 // psrldq xmm0, 4
      {"0F C2 C1 01", 4, 0, 0, false, 3, 1},                    // cmpltps xmm0, xmm1
      {"66 0F 38 00 C1", 5},                                    // pshufb xmm0, xmm1
      {"66 0F 3A 0F C1 08", 6, 0, 0, false, 5, 1},              // palignr xmm0, xmm1, 8
      {"66 0F 3A 63 05 01 02 03 04 05", 10, 5, 4, true, 9, 1},  // pcmpistri xmm0, [rip+disp32], 5
      {"F3 0F B8 C1", 4},                                       // popcnt eax, ecx
      {"0F 20 C0", 3},                                          // mov rax, cr0
      {"0F 20 00", 3},                                          // control moves take no displacement whatever mod says
      {"0F 0F C1 9E", 4, 0, 0, false, 3, 1},                    // 3DNow! pfadd: the operation byte comes last
      {"66 0F 78 C0 01 02", 6, 0, 0, false, 4, 1},              // extrq xmm0, 1, 2
      {"0F 78 C1", 3},                                          // vmread ecx, eax
      {"D9 E8", 2},                                             // fld1
      {"DD 05 01 02 03 04", 6, 2, 4, true},                     // fld qword [rip+disp32]
  };
  for (const Case& c : cases) check(c);
  const auto extrq = decode(hex("66 0F 78 C0 01 02"));
  REQUIRE(extrq.has_value());
  CHECK(extrq->imm2_at == 5);
  CHECK(extrq->imm2_size == 1);
}

TEST_CASE("x86: VEX, EVEX and XOP") {
  const Case cases[] = {
      {"C5 F8 77", 3},                                       // vzeroupper: no ModRM
      {"C5 FC 28 C1", 4},                                    // vmovaps ymm0, ymm1
      {"C5 F9 70 C1 05", 5, 0, 0, false, 4, 1},              // vpshufd xmm0, xmm1, 5
      {"C4 E3 79 04 C1 05", 6, 0, 0, false, 5, 1},           // vpermilps xmm0, xmm1, 5
      {"C4 E2 78 F2 C1", 5},                                 // andn eax, eax, ecx
      {"C4 E3 7B F0 C1 05", 6, 0, 0, false, 5, 1},           // rorx eax, ecx, 5
      {"C5 FA 6F 05 01 02 03 04", 8, 4, 4, true},            // vmovdqu xmm0, [rip+disp32]
      {"C4 E3 71 4A C2 30", 6, 0, 0, false, 5, 1},           // vblendvps xmm0, xmm1, xmm2, xmm3
      {"62 F1 7C 48 58 C1", 6},                              // vaddps zmm0, zmm0, zmm1
      {"62 F1 7C 48 58 41 01", 7, 6, 1},                     // vaddps zmm0, zmm0, [rcx+64] (disp8*N)
      {"62 F1 7C 48 10 05 01 02 03 04", 10, 6, 4, true},     // vmovups zmm0, [rip+disp32]
      {"62 F3 7D 48 1F C1 01", 7, 0, 0, false, 6, 1},        // vpcmpltd k0, zmm0, zmm1
      {"62 F5 7C 48 58 C1", 6},                              // vaddph zmm0, zmm0, zmm1 (map 5)
      {"8F E8 78 C0 C1 05", 6, 0, 0, false, 5, 1},           // vprotb xmm0, xmm1, 5 (XOP map 8)
      {"8F E9 78 80 C1", 5},                                 // vfrczps xmm0, xmm1 (XOP map 9)
      {"8F EA 78 10 C1 01 02 03 04", 9, 0, 0, false, 5, 4},  // bextr eax, ecx, imm32 (XOP map 10)
      {"8F C0", 2},                                          // pop rax: 8F with a map below 8 is POP
  };
  for (const Case& c : cases) check(c);
  const auto vex = decode(hex("C4 E2 F8 F2 C1"));
  REQUIRE(vex.has_value());
  CHECK(vex->encoding == Encoding::Vex);
  CHECK(vex->map == 2);
  CHECK(vex->wide);
  const auto evex = decode(hex("62 F1 FC 48 58 C1"));
  REQUIRE(evex.has_value());
  CHECK(evex->encoding == Encoding::Evex);
  CHECK(evex->wide);
}

TEST_CASE("x86: what 64-bit mode does not define is refused") {
  for (const char* bad : {"06",
                          "07",
                          "0E",
                          "16",
                          "1F",
                          "27",
                          "2F",
                          "37",
                          "3F",
                          "60",
                          "61",
                          "82 C0 01",
                          "9A",
                          "CE",
                          "D4 0A",
                          "D5 0A",
                          "D6",
                          "EA",
                          "F1",
                          "0F 04",
                          "0F 0A",
                          "0F 24 C0",
                          "0F 36",
                          "0F 39",
                          "0F A6 C0",
                          "0F B8 C0",
                          "C6 08 00",
                          "C7 08 00 00 00 00",
                          "8F E0",
                          "FE D0",
                          "FF F8",
                          "40 C5 F8 77",
                          "C4 E4 79 04 C1",
                          "62 F4 7C 08 00 C1",
                          "62 F1 7C 48 58 C1 00"}) {
    CAPTURE(bad);
    std::vector<std::byte> code = hex(bad);
    if (std::string(bad) == "62 F1 7C 48 58 C1 00") {
      code[2] = std::byte{0x78};  // EVEX's fixed bit cleared
    }
    code.insert(code.end(), 16, std::byte{0x90});
    const auto in = decode(code);
    REQUIRE_FALSE(in.has_value());
    CHECK(in.error() == DecodeProblem::Invalid);
  }
}

TEST_CASE("x86: 15 bytes at most; nothing past the bytes given is read") {
  std::vector<std::byte> fourteen_prefixes(14, std::byte{0x66});
  fourteen_prefixes.push_back(std::byte{0x90});
  const auto ok = decode(fourteen_prefixes);
  REQUIRE(ok.has_value());
  CHECK(ok->length == 15);

  std::vector<std::byte> too_many(15, std::byte{0x66});
  too_many.push_back(std::byte{0x90});
  const auto refused = decode(too_many);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error() == DecodeProblem::TooLong);

  // An instruction that would pass 15 bytes through its displacement and immediate: six prefixes and 11 bytes (2E,
  // unlike 66, leaves the immediate at 32 bits).
  std::vector<std::byte> six_prefixes(6, std::byte{0x2E});
  for (const auto b : hex("C7 84 C0 01 02 03 04 05 06 07 08")) six_prefixes.push_back(b);
  const auto over = decode(six_prefixes);
  REQUIRE_FALSE(over.has_value());
  CHECK(over.error() == DecodeProblem::TooLong);

  CHECK(decode(std::span<const std::byte>()).error() == DecodeProblem::Truncated);
}

TEST_CASE("x86: displacements, immediates and targets are read signed") {
  const std::vector<std::byte> code = hex("48 8D 0D F0 FF FF FF");  // lea rcx, [rip-16]
  const auto lea = decode(code);
  REQUIRE(lea.has_value());
  CHECK(en::gal::x86::displacement(*lea, code) == -16);
  CHECK(en::gal::x86::rip_target(*lea, code, 0x1000) == 0x1000 + 7 - 16);
  CHECK(en::gal::x86::rip_target(*lea, code, 4) == std::nullopt);  // before address 0
  CHECK(en::gal::x86::branch_target(*lea, code, 0x1000) == std::nullopt);

  const std::vector<std::byte> jump = hex("EB FE");  // jmp to itself
  const auto jmp = decode(jump);
  REQUIRE(jmp.has_value());
  CHECK(en::gal::x86::immediate(*jmp, jump) == -2);
  CHECK(en::gal::x86::branch_target(*jmp, jump, 0x2000) == 0x2000);

  const std::vector<std::byte> mov = hex("48 B8 FF FF FF FF FF FF FF FF");
  const auto imm64 = decode(mov);
  REQUIRE(imm64.has_value());
  CHECK(en::gal::x86::immediate(*imm64, mov) == -1);
  CHECK(en::gal::x86::immediate2(*imm64, mov) == std::nullopt);
}

TEST_CASE("x86: the fields a register analysis reads") {
  const auto in = decode(hex("44 8B 43 08"));  // mov r8d, [rbx+8]
  REQUIRE(in.has_value());
  CHECK(in->reg() == 8);
  CHECK(in->rm() == 3);
  CHECK(in->mod() == 1);
  CHECK_FALSE(in->wide);
  const auto bx = decode(hex("41 BF 01 00 00 00"));  // mov r15d, 1
  REQUIRE(bx.has_value());
  CHECK(bx->opcode_reg() == 15);
}
