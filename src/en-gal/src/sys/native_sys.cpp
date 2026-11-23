// NativeSys (native_sys.hpp): the SysLayer in game, on Windows (clang-cl).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

#include "en/gal/guarded.hpp"
#include "en/gal/native_sys.hpp"
#include "sys_internal.hpp"

namespace en::gal {

namespace {

// A range's pages, each committed, readable and no guard page (VirtualQuery, which touches none of them).
bool readable_range(std::uint64_t at, std::uint64_t len) noexcept {
  std::uint64_t p = at;
  const std::uint64_t end = at + len;
  while (p < end) {
    MEMORY_BASIC_INFORMATION mbi{};
    // SAFETY: VirtualQuery reads the process's page tables for this address; it reads nothing at it.
    if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(p)), &mbi, sizeof mbi) == 0) return false;
    const DWORD prot = mbi.Protect;
    if (mbi.State != MEM_COMMIT || (prot & (PAGE_GUARD | PAGE_NOACCESS)) != 0 || prot == 0) return false;
    // SAFETY: the region's base and size, as numbers.
    const std::uint64_t next = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (next <= p) return false;
    p = next;
  }
  return true;
}

// The main module's mapped image: reads are guarded copies of pages VirtualQuery says may be read.
class ImageSource final : public ByteSource {
 public:
  ImageSource(std::uint64_t base, std::uint64_t size) : base_(base), size_(size) {}
  [[nodiscard]] std::uint64_t size() const noexcept override { return size_; }
  [[nodiscard]] bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept override {
    if (offset > size_ || dst.size() > size_ - offset) return false;
    if (dst.empty()) return true;
    if (!readable_range(base_ + offset, dst.size())) return false;
    return seh_copy(base_ + offset, dst).copied;
  }

 private:
  std::uint64_t base_;
  std::uint64_t size_;
};

class SehCopier final : public Copier {
 public:
  [[nodiscard]] CopyOutcome copy(std::uint64_t address, std::span<std::byte> dst) noexcept override {
    return seh_copy(address, dst);
  }
};

// ReadProcessMemory on our own process (S0-D's alternative): no exception is raised, whatever the address.
class RpmCopier final : public Copier {
 public:
  [[nodiscard]] CopyOutcome copy(std::uint64_t address, std::span<std::byte> dst) noexcept override {
    SIZE_T n = 0;
    // SAFETY: the system copies from our own process into storage we own; the address is only a number to it.
    const BOOL ok = ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)),
                                      dst.data(), dst.size(), &n);
    if (ok != 0 && n == dst.size()) return {true, 0};
    std::fill(dst.begin(), dst.end(), std::byte{0});
    return {false, 0xC0000005u};
  }
};

}  // namespace

struct NativeSys::Impl {
  Impl(std::uint64_t base, std::uint64_t size) : image(base, size) {}
  ImageSource image;
  SehCopier seh;
  RpmCopier rpm;
  std::uint64_t no_access = 0;
};

NativeSys::NativeSys(CopyKind copy) : kind_(copy) {
  // SAFETY: the main module's handle is its base address, as a number.
  base_ = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  // SizeOfImage from the mapped headers, through guarded copies.
  std::uint64_t size = 0;
  IMAGE_DOS_HEADER dos{};
  std::array<std::byte, sizeof dos> dos_bytes{};
  if (base_ != 0 && seh_copy(base_, dos_bytes).copied) {
    std::memcpy(&dos, dos_bytes.data(), sizeof dos);
    IMAGE_NT_HEADERS64 nt{};
    std::array<std::byte, sizeof nt> nt_bytes{};
    if (dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && dos.e_lfanew < 0x10000 &&
        seh_copy(base_ + static_cast<std::uint64_t>(dos.e_lfanew), nt_bytes).copied) {
      std::memcpy(&nt, nt_bytes.data(), sizeof nt);
      if (nt.Signature == IMAGE_NT_SIGNATURE) size = nt.OptionalHeader.SizeOfImage;
    }
  }
  impl_ = std::make_unique<Impl>(base_, size);
  refresh_stacks();
  note_own_stack();
}

NativeSys::~NativeSys() = default;

const ByteSource& NativeSys::image() const noexcept { return impl_->image; }

Copier& NativeSys::copier() noexcept {
  if (kind_ == CopyKind::Rpm) return impl_->rpm;
  return impl_->seh;
}

Copier& NativeSys::copier_of(CopyKind kind) noexcept {
  if (kind == CopyKind::Rpm) return impl_->rpm;
  return impl_->seh;
}

void NativeSys::note_own_stack() noexcept {
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  stacks_.add(AddressRange{low, high});
}

bool NativeSys::forget_stale_stacks(std::uint64_t address, std::uint64_t len) noexcept {
  // Each overlapping range is still a stack if its allocation starts where the range does and holds a guard page.
  for (;;) {
    bool removed = false;
    for (std::size_t i = 0; i < stacks_.size(); ++i) {
      const AddressRange r = stacks_.at(i);
      if (!r.overlaps(address, len)) continue;
      bool stack = false;
      for (std::uint64_t p = r.begin; p < r.end;) {
        MEMORY_BASIC_INFORMATION mbi{};
        // SAFETY: VirtualQuery reads the page tables for this address, not the address.
        if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(p)), &mbi, sizeof mbi) == 0) break;
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mbi.AllocationBase));
        const auto region = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mbi.BaseAddress));
        if (mbi.State == MEM_FREE || base != r.begin) break;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_GUARD) != 0) {
          stack = true;
          break;
        }
        if (region + mbi.RegionSize <= p) break;
        p = region + mbi.RegionSize;
      }
      if (!stack) {
        stacks_.remove(r);
        removed = true;
        break;
      }
    }
    if (!removed) break;
  }
  return !stacks_.overlaps(address, len);
}

std::size_t NativeSys::rescan_stacks() noexcept {
  stacks_.clear();
  (void)refresh_stacks();
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  stacks_.remove(AddressRange{low, high});
  return stacks_.size();
}

#if EN_PROBE
std::optional<probe::MemoryRegion> NativeSys::region_at(std::uint64_t at) const noexcept {
  MEMORY_BASIC_INFORMATION mbi{};
  // SAFETY: VirtualQuery reads the page tables for this address, not the address.
  if (at >= kAddressEnd ||
      VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(at)), &mbi, sizeof mbi) == 0) {
    return std::nullopt;
  }
  probe::MemoryRegion r;
  // SAFETY: the region's base and its allocation's, as numbers.
  r.base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
  r.allocation_base = reinterpret_cast<std::uintptr_t>(mbi.AllocationBase);
  r.size = mbi.RegionSize;
  r.committed = mbi.State == MEM_COMMIT;
  r.protect = r.committed ? mbi.Protect : 0;
  r.kind = mbi.Type == MEM_PRIVATE ? probe::RegionKind::Private
           : mbi.Type == MEM_MAPPED ? probe::RegionKind::Mapped
           : mbi.Type == MEM_IMAGE  ? probe::RegionKind::Image
                                    : probe::RegionKind::Other;
  return r;
}
#endif

std::size_t NativeSys::refresh_stacks() noexcept {
  // Every allocation that holds a guard page, whole (the shape of a thread's stack); in an image, its guard pages alone.
  std::uint64_t p = kLowestAddress;
  std::uint64_t alloc = 0, alloc_end = 0;
  bool alloc_guard = false;
  const auto flush = [&] {
    if (alloc_guard && alloc_end > alloc) stacks_.add(AddressRange{alloc, alloc_end});
    alloc_guard = false;
  };
  while (p < kAddressEnd) {
    MEMORY_BASIC_INFORMATION mbi{};
    // SAFETY: VirtualQuery reads the page tables for this address, not the address.
    if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(p)), &mbi, sizeof mbi) == 0) break;
    const auto region = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
    const auto base = reinterpret_cast<std::uintptr_t>(mbi.AllocationBase);
    const std::uint64_t end = region + mbi.RegionSize;
    if (base != alloc) {
      flush();
      alloc = base;
    }
    alloc_end = end;
    const bool guard = mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_GUARD) != 0;
    if (guard && mbi.Type == MEM_IMAGE) {
      stacks_.add(AddressRange{region, end});
    } else if (guard && mbi.State != MEM_FREE) {
      alloc_guard = true;
    }
    if (end <= p) break;
    p = end;
  }
  flush();
  return stacks_.size();
}

std::uint64_t NativeSys::now_us() const noexcept { return sys::qpc_us(); }
std::uint32_t NativeSys::thread_id() const noexcept { return GetCurrentThreadId(); }
std::uint64_t NativeSys::slot_calls(std::uint32_t slot) const noexcept { return sys::slot_calls(slot); }
std::uint64_t NativeSys::task_object() const noexcept { return sys::task_object(); }

proto::Expected<std::uint64_t, SysError> NativeSys::call_getter(std::uint64_t getter, std::uint64_t class_object) noexcept {
  if (sys::registered()) return proto::Unexpected{SysError::NotInit};  // init only (ARCH §6.2 step 3)
  const sys::CallOutcome out = sys::call_getter(getter, class_object);
  if (!out.ok) return proto::Unexpected{SysError::Faulted};
  return out.answer;
}

proto::Expected<std::uint64_t, SysError> NativeSys::register_task(std::uint64_t function, std::uint64_t instance,
                                                                  const TaskShape& shape, TaskBody& body) noexcept {
  if (sys::registered()) return proto::Unexpected{SysError::Twice};
  if (shape.vtable_slots == 0 || shape.vtable_slots > kMaxTaskSlots ||
      shape.run_slot >= shape.vtable_slots || shape.object_size > kMaxCopyBytes) {
    return proto::Unexpected{SysError::BadShape};
  }
  const std::uint64_t block = sys::make_task_object(shape, body);
  if (block == 0) return proto::Unexpected{SysError::Twice};
  std::array<std::uint64_t, 4> args{};
  const auto put = [&](ArgRegister r, std::uint64_t v) { args[static_cast<std::size_t>(r)] = v; };
  put(shape.abi.task, block);
  put(shape.abi.instance, instance);
  put(shape.abi.group, shape.group_index);
  sys::set_registered();  // one call, never retried (ARCH §3.2's Register)
  const sys::CallOutcome out = sys::call_register(function, args);
  if (!out.ok) return proto::Unexpected{SysError::Faulted};
  return out.answer;
}

std::uint64_t NativeSys::no_access_page() noexcept {
  if (impl_->no_access == 0) {
    // SAFETY: a page of our own, reserved with no access and never committed or freed: any read of it faults.
    impl_->no_access = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 4096, MEM_RESERVE, PAGE_NOACCESS));
  }
  return impl_->no_access;
}

}  // namespace en::gal
