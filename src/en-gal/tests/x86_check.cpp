// en_gal_x86_check: compares en-gal's x86-64 decoder with what LLVM's disassembler says of the same bytes
// (x86_objdump.py writes the listing). Each line is one instruction:
//
//   <address> <bytes> <length> <rip target|r|-> <branch target|-> <immediates|-> <kind>
//
// where <bytes> holds the instruction and up to 15 bytes that follow it, `r` marks a RIP-relative operand whose target
// LLVM did not print, <immediates> holds the immediate values LLVM printed (comma-separated, hexadecimal, two's
// complement in 64 bits), and <kind> is `ok`, `pseudo` (a vector compare or carry-less multiply, whose predicate
// immediate LLVM may fold into the mnemonic, as cmpltps does: its immediates are not compared) or `unknown` (LLVM could
// not decode it).
//
//   en_gal_x86_check <listing>            our own objects: every disagreement is a finding
//   en_gal_x86_check --random <listing>   random bytes: three kinds of disagreement are known and counted apart
//
// The known kinds, which no compiler we build with emits: a REX followed by another prefix, or before VEX, EVEX or XOP
// (the manuals say such a REX is ignored; LLVM honours parts of it); APX's REX2 (D5) and EVEX map 4, which LLVM 18
// decodes and we refuse; and opcodes a map leaves undefined, which LLVM refuses and we size by the map's shape. It
// prints the counts and examples of each kind, and exits 1 on any finding.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "en/gal/x86.hpp"

namespace {

using en::gal::x86::Encoding;
using en::gal::x86::Instruction;

std::optional<std::vector<std::byte>> parse_hex(const std::string& s) {
  if (s.size() % 2 != 0) return std::nullopt;
  std::vector<std::byte> out;
  for (std::size_t i = 0; i < s.size(); i += 2) {
    unsigned v = 0;
    if (std::sscanf(s.substr(i, 2).c_str(), "%2x", &v) != 1) return std::nullopt;
    out.push_back(std::byte{static_cast<std::uint8_t>(v)});
  }
  return out;
}

std::optional<std::uint64_t> parse_u64(const std::string& s) {
  if (s == "-" || s == "r") return std::nullopt;
  return std::stoull(s, nullptr, 16);
}

std::uint64_t mask(std::uint64_t v, std::size_t size) {
  return size >= 8 ? v : v & ((std::uint64_t{1} << (8 * size)) - 1);
}

// The target as LLVM computes it: modulo 2^64, so that one before address 0 wraps.
std::uint64_t wrapped(std::uint64_t at, std::uint8_t length, std::optional<std::int64_t> disp) {
  return at + length + static_cast<std::uint64_t>(disp.value_or(0));
}

// Instructions whose 8-bit immediate names a fourth register in its high nibble (FMA4, VPERMIL2, the VEX blends and
// XOP's multiply-adds): LLVM prints it as a register, not as an immediate.
bool register_in_immediate(const Instruction& insn) {
  const std::uint8_t op = insn.opcode;
  if (insn.encoding == Encoding::Vex && insn.map == 3) {
    return (op >= 0x48 && op <= 0x4C) || (op >= 0x5C && op <= 0x5F) || (op >= 0x68 && op <= 0x6F) ||
           (op >= 0x78 && op <= 0x7F);
  }
  if (insn.encoding == Encoding::Xop && insn.map == 8) {
    return (op >= 0x85 && op <= 0x87) || op == 0x8E || op == 0x8F || (op >= 0x95 && op <= 0x97) || op == 0x9E ||
           op == 0x9F || op == 0xA2 || op == 0xA3 || op == 0xA6 || op == 0xB6;
  }
  return false;
}

bool is_rex(std::uint8_t b) { return b >= 0x40 && b <= 0x4F; }
bool is_legacy_prefix(std::uint8_t b) {
  return b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 || b == 0x26 || b == 0x2E || b == 0x36 ||
         b == 0x3E || b == 0x64 || b == 0x65;
}

// For random bytes: which known kind of disagreement this is, or nothing.
std::optional<std::string> known_kind(const std::vector<std::byte>& bytes, const std::string& kind) {
  std::size_t i = 0;
  bool rex_before = false;
  bool stray = false;
  while (i < bytes.size() && i < 15) {
    const auto b = std::to_integer<std::uint8_t>(bytes[i]);
    if (is_rex(b)) {
      if (rex_before) stray = true;
      rex_before = true;
    } else if (is_legacy_prefix(b)) {
      if (rex_before) stray = true;
    } else {
      if (b == 0xD5) return "apx-rex2";
      if (b == 0x62 && i + 1 < bytes.size() && (std::to_integer<std::uint8_t>(bytes[i + 1]) & 7) == 4)
        return "apx-map4";
      if (rex_before && (b == 0xC4 || b == 0xC5 || b == 0x62 || b == 0x8F)) stray = true;
      break;
    }
    ++i;
  }
  if (stray) return "stray-rex";
  if (kind == "unknown") return "undefined-opcode";
  return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
  const bool random = argc == 3 && std::string(argv[1]) == "--random";
  if (argc != 2 && !random) {
    std::cerr << "usage: en_gal_x86_check [--random] <listing>\n";
    return 2;
  }
  std::ifstream in(argv[argc - 1]);
  if (!in) {
    std::cerr << "en_gal_x86_check: cannot read the listing\n";
    return 2;
  }
  std::uint64_t lines = 0;
  std::uint64_t agreed = 0;
  std::uint64_t refused_alike = 0;
  std::map<std::string, std::uint64_t> findings;
  std::map<std::string, std::uint64_t> known;
  std::map<std::string, std::vector<std::string>> shown;  // up to 6 examples of each kind
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    std::string addr_s, bytes_s, len_s, rip_s, br_s, imm_s, kind;
    if (!(fields >> addr_s >> bytes_s >> len_s >> rip_s >> br_s >> imm_s >> kind)) {
      std::cerr << "en_gal_x86_check: a malformed line: " << line << "\n";
      return 2;
    }
    const std::uint64_t addr = std::stoull(addr_s, nullptr, 16);
    const auto bytes = parse_hex(bytes_s);
    if (!bytes) {
      std::cerr << "en_gal_x86_check: malformed bytes: " << line << "\n";
      return 2;
    }
    const std::size_t want_len = std::stoul(len_s);
    ++lines;

    const auto decoded = en::gal::x86::decode(*bytes);
    std::string problem;
    if (kind == "unknown") {
      if (!decoded) {
        ++refused_alike;
        continue;
      }
      problem = "llvm-unknown-we-decode";
    } else if (!decoded) {
      problem = std::string("we-refuse(") + en::gal::x86::to_string(decoded.error()) + ")";
    } else {
      const Instruction& insn = *decoded;
      const std::optional<std::uint64_t> want_rip = parse_u64(rip_s);
      const std::optional<std::uint64_t> want_br = parse_u64(br_s);
      if (insn.length != want_len) {
        problem = "length";
      } else if (insn.rip_relative != (want_rip.has_value() || rip_s == "r")) {
        problem = "rip-relative";
      } else if (want_rip && wrapped(addr, insn.length, en::gal::x86::displacement(insn, *bytes)) != *want_rip) {
        problem = "rip-target";
      } else if (insn.relative != want_br.has_value()) {
        problem = "relative-branch";
      } else if (want_br && wrapped(addr, insn.length, en::gal::x86::immediate(insn, *bytes)) != *want_br) {
        problem = "branch-target";
      } else if (!insn.relative && kind != "pseudo") {
        // The immediates: LLVM's values against ours, each masked to the immediate's size.
        std::vector<std::uint64_t> theirs;
        if (imm_s != "-") {
          std::istringstream list(imm_s);
          std::string item;
          while (std::getline(list, item, ',')) theirs.push_back(std::stoull(item, nullptr, 16));
        }
        std::vector<std::pair<std::uint64_t, std::size_t>> ours;
        // 3DNow!'s operation byte, and a register in an immediate, are not printed as immediates.
        if (insn.imm_size != 0 && insn.map != 15 && !register_in_immediate(insn)) {
          ours.emplace_back(static_cast<std::uint64_t>(*en::gal::x86::immediate(insn, *bytes)), insn.imm_size);
        }
        if (insn.imm2_size != 0) {
          ours.emplace_back(static_cast<std::uint64_t>(*en::gal::x86::immediate2(insn, *bytes)), insn.imm2_size);
        }
        if (theirs.size() != ours.size()) {
          problem = "immediate-count";
        } else {
          bool all = true;
          for (const std::uint64_t v : theirs) {
            bool found = false;
            for (const auto& [mine, size] : ours) found = found || mask(v, size) == mask(mine, size);
            all = all && found;
          }
          if (!all) problem = "immediate-value";
        }
      }
    }
    if (problem.empty()) {
      ++agreed;
      continue;
    }
    const std::optional<std::string> excused = random ? known_kind(*bytes, kind) : std::nullopt;
    const std::string label = excused ? "known " + *excused : problem;
    ++(excused ? known : findings)[label];
    if (shown[label].size() < 6) {
      std::ostringstream s;
      s << label << " (" << problem << "): " << line;
      if (decoded) {
        s << "  (ours: length " << int{decoded->length} << ", disp " << int{decoded->disp_size} << "@"
          << int{decoded->disp_at} << (decoded->rip_relative ? " rip" : "") << ", imm " << int{decoded->imm_size} << "@"
          << int{decoded->imm_at} << (decoded->relative ? " rel" : "") << ", map " << int{decoded->map} << ")";
      }
      shown[label].push_back(s.str());
    }
  }
  std::uint64_t found = 0;
  for (const auto& [_, n] : findings) found += n;
  std::uint64_t excused = 0;
  for (const auto& [_, n] : known) excused += n;
  std::cout << "x86 decoder against LLVM: " << lines << " instructions, " << agreed << " agree, " << refused_alike
            << " that LLVM cannot decode refused alike, " << excused << " known kinds of disagreement, " << found
            << " findings\n";
  for (const auto& [label, n] : known) std::cout << "  " << label << ": " << n << "\n";
  for (const auto& [label, n] : findings) std::cout << "  " << label << ": " << n << "\n";
  for (const auto& [label, examples] : shown) {
    if (!random && known.count(label) != 0) continue;
    for (const std::string& s : examples) std::cout << "  " << s << "\n";
  }
  return found == 0 && lines > 0 ? 0 : 1;
}
