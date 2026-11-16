#include "en/proto/ffi_guard.hpp"

#include <array>
#include <atomic>

namespace en::proto {

namespace {

// constinit: made at compile time, so no initialiser runs when the DLL loads; atomics of bool have no destructor.
constinit std::array<std::atomic<bool>, kComponentSlots> g_tripped{};

std::atomic<bool>* flag(Component component) noexcept {
  const auto index = static_cast<std::size_t>(component);
  return index < g_tripped.size() ? &g_tripped[index] : nullptr;
}

}  // namespace

const char* to_string(Component component) noexcept {
  switch (component) {
    case Component::DllMain: return "DllMain";
    case Component::Task: return "Task";
    case Component::Present: return "Present";
    case Component::Overlay: return "Overlay";
    case Component::LlmTransport: return "LlmTransport";
    case Component::LlmWorker: return "LlmWorker";
    case Component::Tools: return "Tools";
  }
  return "Unknown";
}

bool tripped(Component component) noexcept {
  const std::atomic<bool>* f = flag(component);
  return f != nullptr && f->load(std::memory_order_acquire);
}

void trip(Component component) noexcept {
  if (std::atomic<bool>* f = flag(component)) f->store(true, std::memory_order_release);
}

void clear_trip(Component component) noexcept {
  if (std::atomic<bool>* f = flag(component)) f->store(false, std::memory_order_release);
}

}  // namespace en::proto
