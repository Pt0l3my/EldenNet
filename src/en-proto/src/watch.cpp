// The watch block's helpers (watch.hpp; O71, O90).
#include "en/proto/watch.hpp"

namespace en::proto::watch {

const char* to_string(InitState s) noexcept {
  switch (s) {
    case InitState::None:
      return "none";
    case InitState::Pin:
      return "pin";
    case InitState::Gate:
      return "gate";
    case InitState::Safety:
      return "safety";
    case InitState::Settings:
      return "settings";
    case InitState::WaitWindow:
      return "wait_window";
    case InitState::Connect:
      return "connect";
    case InitState::ReadyScan:
      return "ready_scan";
    case InitState::WaitEngine:
      return "wait_engine";
    case InitState::Discover:
      return "discover";
    case InitState::Register:
      return "register";
    case InitState::FirstTick:
      return "first_tick";
    case InitState::Ready:
      return "ready";
    case InitState::Inert:
      return "inert";
    case InitState::Banner:
      return "banner";
    case InitState::Exit:
      return "exit";
  }
  return "unknown";
}

const char* to_string(ProbeRequest p) noexcept {
  switch (p) {
    case ProbeRequest::None:
      return "none";
    case ProbeRequest::R1:
      return "r1";
    case ProbeRequest::Accepted:
      return "accepted";
    case ProbeRequest::Refused:
      return "refused";
  }
  return "none";
}

const char* to_string(MarkSource s) noexcept {
  switch (s) {
    case MarkSource::None:
      return "none";
    case MarkSource::Launcher:
      return "launcher";
    case MarkSource::Asked:
      return "asked";
    case MarkSource::EldenNet:
      return "eldennet";
  }
  return "none";
}

const char* to_string(Spike s) noexcept {
  switch (s) {
    case Spike::None:
      return "none";
    case Spike::S0a:
      return "s0a";
    case Spike::S0b:
      return "s0b";
    case Spike::S0c:
      return "s0c";
    case Spike::S0d:
      return "s0d";
    case Spike::S0dCrash:
      return "s0d-crash";
    case Spike::S0e:
      return "s0e";
    case Spike::S0eAlone:
      return "s0e_alone";
    case Spike::S4:
      return "s4";
  }
  return "unknown";
}

const char* to_string(ThreadKind t) noexcept {
  switch (t) {
    case ThreadKind::Unknown:
      return "unknown";
    case ThreadKind::Game:
      return "game";
    case ThreadKind::Init:
      return "init";
    case ThreadKind::Other:
      return "other";
  }
  return "unknown";
}

void initialise(Block& b, std::uint32_t game_pid) noexcept {
  b.control.spike.store(0, std::memory_order_relaxed);
  b.control.crash_request.store(0, std::memory_order_relaxed);
  b.control.watcher_pid.store(0, std::memory_order_relaxed);
  b.header.version = kVersion;
  b.header.size = static_cast<std::uint32_t>(sizeof(Block));
  b.header.game_pid = game_pid;
  b.header.magic.store(kMagic, std::memory_order_release);
}

bool valid(const Block& b) noexcept {
  return b.header.magic.load(std::memory_order_acquire) == kMagic && b.header.version == kVersion &&
         b.header.size == sizeof(Block);
}

void set_module_name(Crash& c, const wchar_t* name, std::size_t length) noexcept {
  std::size_t n = 0;
  for (std::size_t i = 0; i < length && n + 1 < kModuleNameBytes; ++i) {
    const wchar_t w = name[i];
    const bool keep = (w >= L'a' && w <= L'z') || (w >= L'A' && w <= L'Z') || (w >= L'0' && w <= L'9') || w == L'.' ||
                      w == L'_' || w == L'-' || w == L'+' || w == L' ';
    c.module[n++] = keep ? static_cast<char>(w) : '?';
  }
  for (std::size_t i = n; i < kModuleNameBytes; ++i) c.module[i] = '\0';
}

std::string_view module_name(const Crash& c) noexcept {
  std::size_t n = 0;
  while (n < kModuleNameBytes && c.module[n] != '\0') ++n;
  return std::string_view(c.module.data(), n);
}

MarkSource mark_source(const Marks& m) noexcept {
  const std::uint32_t v = m.source.load(std::memory_order_acquire);
  return v < kMarkSourceCount ? static_cast<MarkSource>(v) : MarkSource::None;
}

void launcher_holds(Marks& m) noexcept {
  auto none = static_cast<std::uint32_t>(MarkSource::None);
  (void)m.source.compare_exchange_strong(none, static_cast<std::uint32_t>(MarkSource::Launcher),
                                         std::memory_order_acq_rel);
}

bool eldennet_asks(Marks& m) noexcept {
  auto expected = static_cast<std::uint32_t>(MarkSource::None);
  if (m.source.compare_exchange_strong(expected, static_cast<std::uint32_t>(MarkSource::EldenNet),
                                       std::memory_order_acq_rel)) {
    return true;  // nobody held the key: the ring is EldenNet's at once
  }
  if (expected == static_cast<std::uint32_t>(MarkSource::Launcher)) {
    (void)m.source.compare_exchange_strong(expected, static_cast<std::uint32_t>(MarkSource::Asked),
                                           std::memory_order_acq_rel);
  }
  return false;
}

void launcher_hands_over(Marks& m) noexcept {
  auto asked = static_cast<std::uint32_t>(MarkSource::Asked);
  (void)m.source.compare_exchange_strong(asked, static_cast<std::uint32_t>(MarkSource::EldenNet),
                                         std::memory_order_acq_rel);
}

std::uint32_t add_mark(Marks& m, std::uint64_t at_ms) noexcept {
  const std::uint32_t n = m.count.load(std::memory_order_relaxed) + 1;
  m.mono_ms[(n - 1) % kMarkRing].store(at_ms, std::memory_order_relaxed);
  m.count.store(n, std::memory_order_release);
  return n;
}

}  // namespace en::proto::watch
