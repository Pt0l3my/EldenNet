// FakeSys and FakeMemory (fake_sys.hpp): the tests' process of bytes.
#include "fake_sys.hpp"

#include <algorithm>

namespace en::gal::test {

void FakeMemory::map(std::uint64_t at, std::vector<std::byte> bytes) { regions_[at] = Region{std::move(bytes), true}; }

std::uint64_t FakeMemory::alloc(std::size_t size) {
  const std::uint64_t at = heap_;
  regions_[at] = Region{std::vector<std::byte>(std::max<std::size_t>(size, 1)), true};
  heap_ += ((size + 0xFFF) & ~std::uint64_t{0xFFF}) + 0x1000;  // a page apart: a read past the end faults
  return at;
}

void FakeMemory::release(std::uint64_t at) {
  if (auto it = regions_.find(at); it != regions_.end()) it->second.readable = false;
}

const FakeMemory::Region* FakeMemory::region_of(std::uint64_t at, std::uint64_t len, std::uint64_t* base) const noexcept {
  auto it = regions_.upper_bound(at);
  if (it == regions_.begin()) return nullptr;
  --it;
  const std::uint64_t off = at - it->first;
  if (off > it->second.bytes.size() || len > it->second.bytes.size() - off) return nullptr;
  *base = it->first;
  return &it->second;
}

bool FakeMemory::read(std::uint64_t at, std::span<std::byte> dst) const noexcept {
  ++reads_;
  std::uint64_t base = 0;
  const Region* r = region_of(at, dst.size(), &base);
  if (r == nullptr || !r->readable) return false;
  std::memcpy(dst.data(), r->bytes.data() + (at - base), dst.size());
  return true;
}

bool FakeMemory::write(std::uint64_t at, std::span<const std::byte> src) {
  std::uint64_t base = 0;
  const Region* r = region_of(at, src.size(), &base);
  if (r == nullptr) return false;
  auto& bytes = const_cast<Region*>(r)->bytes;  // our own map's region, found through the const search
  std::memcpy(bytes.data() + (at - base), src.data(), src.size());
  return true;
}

void FakeMemory::put_utf16(std::uint64_t at, std::u16string_view text) {
  for (std::size_t i = 0; i < text.size(); ++i) put<char16_t>(at + 2 * i, text[i]);
  put<char16_t>(at + 2 * text.size(), u'\0');
}

CopyOutcome FakeCopier::copy(std::uint64_t address, std::span<std::byte> dst) noexcept {
  ++copies;
  if (mem_.read(address, dst)) return {true, 0};
  std::fill(dst.begin(), dst.end(), std::byte{0});
  return {false, 0xC0000005u};
}

bool FakeImageSource::read(std::uint64_t offset, std::span<std::byte> dst) const noexcept {
  if (offset > size_ || dst.size() > size_ - offset) return false;
  return mem_.read(base_ + offset, dst);
}

FakeSys::FakeSys(FakeMemory& mem, std::uint64_t base, std::uint64_t image_size)
    : base_(base), image_(mem, base, image_size), copier_(mem) {}

proto::Expected<std::uint64_t, SysError> FakeSys::call_getter(std::uint64_t g, std::uint64_t cls) noexcept {
  if (registration) return proto::Unexpected{SysError::NotInit};
  ++getter_calls;
  if (!getter) return proto::Unexpected{SysError::Unsupported};
  const auto answer = getter(g, cls);
  if (!answer) return proto::Unexpected{SysError::Faulted};
  return *answer;
}

proto::Expected<void, SysError> FakeSys::register_task(std::uint64_t function, std::uint64_t instance,
                                                       const TaskShape& shape, TaskBody& body) noexcept {
  if (registration) return proto::Unexpected{SysError::Twice};
  if (shape.vtable_slots == 0 || shape.vtable_slots > kMaxTaskSlots || shape.run_slot >= shape.vtable_slots) {
    return proto::Unexpected{SysError::BadShape};
  }
  if (fault_register) return proto::Unexpected{SysError::Faulted};
  registration = Registration{function, instance, shape, &body};
  calls_.assign(shape.vtable_slots, 0);
  return {};
}

void FakeSys::tick(std::uint64_t dt_us, bool watch) {
  now_us_ += dt_us;
  drift_ = 0;
  if (!registration) return;
  ++calls_[registration->shape.run_slot];
  const FrameCtx frame{now_us_, static_cast<std::uint32_t>(std::min<std::uint64_t>(dt_us, 0xFFFF'FFFF))};
  AllocWatch::armed = watch;
  run_task(*registration->body, frame);
  AllocWatch::armed = false;
}

}  // namespace en::gal::test
