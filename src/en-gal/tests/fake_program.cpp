// FakeProgram (fake_program.hpp).
#include "fake_program.hpp"

#include <stdexcept>

namespace en::gal::test {

namespace {

void put(std::vector<std::uint8_t>& b, std::size_t at, std::uint64_t v, int n) {
  for (int i = 0; i < n; ++i) b.at(at + static_cast<std::size_t>(i)) = static_cast<std::uint8_t>(v >> (8 * i));
}

}  // namespace

void FakeProgram::u32(Sec s, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) bytes_[s].push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

void FakeProgram::u64(Sec s, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) bytes_[s].push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

void FakeProgram::ascii(Sec s, const std::string& text) {
  for (const char c : text) bytes_[s].push_back(static_cast<std::uint8_t>(c));
  bytes_[s].push_back(0);
}

void FakeProgram::utf16(Sec s, const std::string& text) {
  for (const char c : text) {
    bytes_[s].push_back(static_cast<std::uint8_t>(c));
    bytes_[s].push_back(0);
  }
  bytes_[s].push_back(0);
  bytes_[s].push_back(0);
}

void FakeProgram::label(const std::string& name, Sec s) {
  if (!labels_.emplace(name, here(s)).second) throw std::logic_error("label defined twice: " + name);
}

std::uint32_t FakeProgram::rva(const std::string& name) const {
  const auto it = labels_.find(name);
  if (it == labels_.end()) throw std::logic_error("no label " + name);
  return it->second;
}

void FakeProgram::rel32(Sec s, const std::string& target, std::uint32_t after) {
  refs_.push_back(Ref{0, s, size(s), target, after, false});
  zeros(s, 4);
}

void FakeProgram::va64(Sec s, const std::string& target, bool relocate) {
  refs_.push_back(Ref{1, s, size(s), target, 0, relocate});
  zeros(s, 8);
}

void FakeProgram::rva32(Sec s, const std::string& target) {
  refs_.push_back(Ref{2, s, size(s), target, 0, false});
  zeros(s, 4);
}

void FakeProgram::function(const std::string& begin, const std::string& end, bool chained) {
  fns_.emplace_back(begin, end);
  chained_.push_back(chained);
}

BuiltImage FakeProgram::build(std::optional<FakeVersion> version) {
  FakeImageSpec spec;
  spec.image_base = image_base_;
  std::vector<std::uint32_t> fixups;
  for (const Ref& r : refs_) {
    const std::uint32_t target = rva(r.target);
    const std::uint32_t at_rva = kRva[r.sec] + r.at;
    if (r.kind == 0) {
      const std::int64_t disp = std::int64_t{target} - (std::int64_t{at_rva} + 4 + r.after);
      put(bytes_[r.sec], r.at, static_cast<std::uint64_t>(disp), 4);
    } else if (r.kind == 1) {
      put(bytes_[r.sec], r.at, image_base_ + target, 8);
      if (r.relocate) fixups.push_back(at_rva);
    } else {
      put(bytes_[r.sec], r.at, target, 4);
    }
  }
  const char* names[3] = {".text", ".rdata", ".data"};
  const std::uint32_t flags[3] = {kCode, kReadOnly, kReadWrite};
  for (int s = 0; s < 3; ++s) {
    if (bytes_[s].size() > kSize[s]) throw std::logic_error("a section is fuller than FakeProgram allows");
    spec.sections.push_back(FakeSection{names[s], flags[s], bytes_[s], kSize[s]});
  }
  for (std::size_t i = 0; i < fns_.size(); ++i) {
    spec.functions.push_back(
        FakeFunction{rva(fns_[i].first) - kRva[Text], rva(fns_[i].second) - kRva[Text], chained_[i], false});
  }
  spec.version = version;
  spec.fixups = fixups;
  BuiltImage built = en::gal::test::build(spec);
  for (int s = 0; s < 3; ++s) {
    if (built.rva.at(names[s]) != kRva[s]) throw std::logic_error("FakeImage placed a section elsewhere");
  }
  return built;
}

}  // namespace en::gal::test
