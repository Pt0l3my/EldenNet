#pragma once
// The offline and multiplayer gate (ARCH §5.1, DESIGN §3.4; C4): computed by the core on every snapshot from its
// `session` and lifecycle fields. It closes on the first snapshot that is not clean, so within one snapshot period
// (≤ 250 ms), and opens only once every processed snapshot has been clean for 5,000 ms of mono time with no gap between
// two of them longer than 1,000 ms (its hysteresis). Pure: the time is the snapshots' own `mono_ms`, so a replay of a
// recording opens and closes it on the same snapshots as the session did.
#include <cstdint>
#include <optional>

#include "en/proto/snapshot.hpp"

namespace en::core {

// Mono time in milliseconds: the snapshots' `mono_ms` (QPC on the game thread) and the core's clock.
using Mono = std::uint64_t;

inline constexpr Mono kGateHoldMs = 5000;    // clean for this long before the gate opens
inline constexpr Mono kGateMaxGapMs = 1000;  // a longer wait between two snapshots starts the hold again (impl)

// Why a snapshot is not clean: the first clause of ARCH §5.1's predicate that fails, in this order.
enum class Unclean : std::uint8_t {
  None,           // clean
  Gap,            // the snapshot is marked `gap` (an unread one was overwritten, or the budget ran out)
  SessionFailed,  // the `session` group is not Ok: untested yet, failed its self-test or tripped
  AntiCheat,      // an anti-cheat module (C14)
  NotSolo,        // in a world, but not one's own solo world: the role, the world, the lobby or the player set
  Lobby,          // at the title, with a lobby that is not None (or not read)
  Loading,        // neither in a world nor at the title: a loading screen, or a screen not read
};

[[nodiscard]] const char* to_string(Unclean why) noexcept;  // "none", "gap", "session_failed", ...

struct Verdict {
  bool clean = false;
  Unclean why = Unclean::Loading;

  friend constexpr bool operator==(const Verdict&, const Verdict&) = default;
};

// ARCH §5.1's predicate on one snapshot. `session_failed` is the gate's sticky memory of a failed or tripped `session`
// group: once it has failed, session_ok stays false for the rest of the session. A value that was not read, or lies
// outside its enum's range, is not clean (`Unknown` is never Host, None or Title).
[[nodiscard]] Verdict judge(const proto::WorldSnapshot& s, bool session_failed) noexcept;

// The gate with its hysteresis. Closed at first: it opens only after a clean hold.
class Gate {
 public:
  // Takes one processed snapshot; answers true when open() changed.
  bool step(const proto::WorldSnapshot& s) noexcept;
  // The `session` group failed outside a snapshot (its GroupTripped message): closed now, and for the session.
  bool session_failed_now() noexcept;

  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] Unclean why() const noexcept { return last_.why; }
  [[nodiscard]] bool clean() const noexcept { return last_.clean; }
  [[nodiscard]] bool session_failed() const noexcept { return session_failed_; }
  // Since when the snapshots have been clean (the hold's start), while they are.
  [[nodiscard]] std::optional<Mono> clean_since() const noexcept { return clean_since_; }

 private:
  bool open_ = false;
  bool session_failed_ = false;
  Verdict last_{};
  std::optional<Mono> clean_since_;
  std::optional<Mono> last_ms_;
};

}  // namespace en::core
