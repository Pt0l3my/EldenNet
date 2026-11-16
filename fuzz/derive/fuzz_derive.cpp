// libFuzzer target: en-gal's derivation (derive.hpp) on arbitrary executables, as probe-exe runs it: the input's first
// two bytes pick a static's slot (an offset into the image, as R1 would give one), the rest is the file. The leads are
// the tests' sample's (invented names). Besides any sanitizer report, a broken property is a finding:
// - the report the derivation writes always passes the lint (the writer's vocabulary and the lint's agree);
// - every pattern it reports is of 1 to 64 bytes with 4 fixed, and a static's capture lies within its pattern;
// - a run that is complete derived the FD4 idiom, the registration and the group.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

#include "en/gal/derive.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/report.hpp"

namespace {

using namespace en::gal;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

constexpr const char* kLeads = R"(schema = 1
product_name = "INVENTED GAME"
[fd4]
path_suffix = "Lone.h"
[singletons]
hub = "Hub"
roster = "Roster"
[register]
instance = "hub"
[task]
group = "Gr_Settle"
base = "Tasks::Base"
[vtable_sets]
body = "Kin::Body"
[statics]
ledger = { rtti = "Store::Ledger" }
)";

void check_cut(const derive::Cut& c) {
  require(!c.bytes.value.empty() && c.bytes.size() <= 64 && c.bytes.fixed() >= 4, "a pattern of 1 to 64 bytes");
  if (c.capture)
    require(c.capture->at + 4 <= c.capture->insn_end && c.capture->insn_end <= c.bytes.size(),
            "a capture within its pattern");
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  static const auto leads = derive::parse_leads(kLeads);  // NOLINT: the target's own, made once
  require(leads.has_value(), "the target's leads read");
  if (size < 2) return 0;
  const std::uint32_t slot = (std::uint32_t{data[0]} << 8 | data[1]) * 8u;
  std::vector<std::byte> bytes(size - 2);
  for (std::size_t i = 2; i < size; ++i) bytes[i - 2] = std::byte{data[i]};
  const SpanSource source(bytes);
  const auto image = read_image(source, ImageLayout::File);
  if (!image) return 0;
  derive::Options options;
  options.file_size = bytes.size();
  options.static_slots = {{"ledger", slot}};
  const derive::Result r = derive::derive(*image, source, *leads, options);
  const auto text = derive::write_report(r);
  require(text.has_value(), "the derivation's report is written");
  const report::Vocabulary* known[] = {&derive::vocabulary()};
  require(report::lint(*text, known).has_value(), "the derivation's report passes the lint");
  if (r.registration.pattern) check_cut(*r.registration.pattern);
  for (const auto& s : r.statics) {
    if (s.pattern) check_cut(*s.pattern);
  }
  if (r.fd4.idiom) require(r.fd4.idiom->bytes.size() <= 64, "the idiom within 64 bytes");
  if (derive::complete(r)) {
    require(r.fd4.status == derive::Status::Derived && r.registration.status == derive::Status::Derived &&
                r.group.status == derive::Status::Derived,
            "a complete run derived the idiom, the registration and the group");
  }
  return 0;
}
