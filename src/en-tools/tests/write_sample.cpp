// en_tools_write_sample: writes the derivation tests' invented program (en-gal's tests/sample_program.hpp) as an
// executable file, with its two leads files, into a folder, for cli_test.py to run eldennet-engine on. Nothing in them
// is a game's.
//
//   en_tools_write_sample <dir>   writes <dir>/sample.exe, <dir>/leads.toml, <dir>/leads-clean.toml, <dir>/slots.txt,
//                                 and R0c's variants: <dir>/variants.exe and <dir>/leads-variants.toml
#include <cstdio>
#include <fstream>
#include <string>

#include "sample_program.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fputs("usage: en_tools_write_sample <dir>\n", stderr);
    return 2;
  }
  const std::string dir = argv[1];
  const en::gal::test::Sample s = en::gal::test::build_sample();
  std::ofstream exe(dir + "/sample.exe", std::ios::binary);
  for (const std::byte b : s.built.file) exe.put(static_cast<char>(b));
  std::ofstream(dir + "/leads.toml") << en::gal::test::kSampleLeads;
  std::ofstream(dir + "/leads-clean.toml") << en::gal::test::kSampleLeadsClean;
  // The statics' slots, as R1 would find them: label=rva.
  std::ofstream(dir + "/slots.txt") << "ledger=0x" << std::hex << s.program.rva("st_ledger") << "\n";
  const en::gal::test::Sample v = en::gal::test::build_variants();
  std::ofstream variants(dir + "/variants.exe", std::ios::binary);
  for (const std::byte b : v.built.file) variants.put(static_cast<char>(b));
  std::ofstream(dir + "/leads-variants.toml") << en::gal::test::kVariantLeads;
  return exe && variants ? 0 : 1;
}
