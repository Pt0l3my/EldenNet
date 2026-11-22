#pragma once
// The toasts' policy (NPC §10.5), in the core so that one rule holds for every surface and runs on a simulated clock:
// at most two up at once, each for 6 s (a status refusal may ask for less); one per class per minute for the classes
// of errors and degradations; held while a boss fight lasts, and shown when it ends; a queue of eight, from which the
// oldest info toast is dropped first; error before degradation before info. The model lists what is up now
// (proto::UiModel::toasts), each with an id unique in the session, which a surface posts once as a GraceUI notice.
// The texts are DESIGN §2.7's and our own (src/en-core/README.md lists them, drafts until the owner approves them).
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "en/core/gate.hpp"  // Mono
#include "en/proto/ui.hpp"

namespace en::core {

// What a toast is about: the rate limit counts per class.
enum class ToastClass : std::uint8_t {
  Status,     // a refusal at the talk key: the persona will not speak, is asleep, has nothing to say (not limited)
  Danger,     // "Not now: danger is near.", "The moment has passed." (not limited)
  Input,      // GraceUI refused the box its input (not limited)
  Timeout,    // no answer in time
  Auth,       // the key was rejected
  RateLimit,  // the provider's rate limit
  Provider,   // another failure of the provider, or of the prompt
  Degraded,   // a group EldenNet reads stopped working
  Fault,      // a fault of the core or the overlay
};
inline constexpr std::size_t kToastClasses = 9;

class Toasts {
 public:
  static constexpr Mono kHoldMs = 6000;
  static constexpr Mono kRateMs = 60000;
  static constexpr std::size_t kVisible = 2;
  static constexpr std::size_t kQueue = 8;

  // Posts a toast at `now`. A limited class already posted in the last minute drops it; a toast whose text is up or
  // waiting already is not posted twice. Returns whether it was taken.
  bool post(ToastClass cls, proto::ToastLevel level, std::string text, Mono now, Mono hold_ms = kHoldMs);
  // Takes down the toasts whose time is up, and puts up what waits (none while a boss fight lasts).
  void step(Mono now, bool boss_fight);
  [[nodiscard]] std::vector<proto::Toast> visible() const;
  [[nodiscard]] std::size_t waiting() const noexcept { return queue_.size(); }

 private:
  struct Entry {
    proto::Toast toast;
    Mono hold_ms = kHoldMs;
    Mono until = 0;  // when up
  };
  std::deque<Entry> queue_;  // waiting, oldest first
  std::vector<Entry> up_;    // up now, oldest first
  std::uint32_t next_id_ = 1;
  std::vector<Mono> last_{std::vector<Mono>(kToastClasses, 0)};
  std::vector<bool> posted_{std::vector<bool>(kToastClasses, false)};
};

}  // namespace en::core
