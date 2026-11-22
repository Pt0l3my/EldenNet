#pragma once
// The adapter boundary (DESIGN §2.3, §4.7; ARCH §6.4): GameBackend is what the task reads the game through, and the
// one seam between EldenNet and a game. en-gal implements it over the game's memory (ErBackend), and the replay
// harness over a recording; each takes the game thread's token for every call, so code off the game thread has none to
// read with (R1). Snapshot in, commands out: in v1 apply() only configures what the backend reads (command.hpp).
#include <cstdint>
#include <span>

#include "en/proto/command.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/render.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/thread.hpp"

namespace en::proto {

// Why a read gave nothing this tick.
enum class ReadError : std::uint8_t {
  Inert,        // the task is inert (L1): no game object is read (ARCH §3.2, §6.7)
  Loading,      // between worlds: only the lifecycle subset is read (R11), and there is nothing else to give
  BudgetSpent,  // the tick's copies or bytes ran out: this snapshot is abandoned, and the next carries gap (ARCH §6.5)
  Faulted,      // a structural read faulted: this snapshot is abandoned, and the fault counted (ARCH §6.5, §6.7)
  Ended,        // the source has no more: a recording's end
  Unsteady,     // read from a thread of our own (O101): the structure changed while it was read, or a change of the
                // lifecycle is not yet seen twice; this snapshot is dropped, nothing is counted, the next carries gap
};

// Why a command was not applied.
enum class ApplyError : std::uint8_t {
  Inert,        // the task is inert: commands are dropped
  Invalid,      // a field out of its bounds: a watch list past kMaxWatchFlags, a param or FMG the layout lacks
  Unsupported,  // the backend does not apply this kind
};

const char* to_string(ReadError v) noexcept;
const char* to_string(ApplyError v) noexcept;

class GameBackend {
 public:
  virtual ~GameBackend() = default;
  virtual Capabilities probe(const GameThread& gt) = 0;
  virtual Expected<void, ReadError> snapshot(const GameThread& gt, WorldSnapshot& out) = 0;
  virtual Expected<void, ReadError> render_feed(const GameThread& gt, std::span<const ChrHandle> want,
                                                RenderFeed& out) = 0;
  virtual Expected<void, ApplyError> apply(const GameThread& gt, const GameCommand& cmd) = 0;

 protected:
  GameBackend() = default;
  GameBackend(const GameBackend&) = default;
  GameBackend& operator=(const GameBackend&) = default;
};

}  // namespace en::proto
