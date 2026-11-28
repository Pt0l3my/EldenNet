#pragma once
// The store's spill ring (ARCH §8.9's table, MEM §8.6; en-augur's PersistQueue is the same policy over ids): what the
// core sends the store while C8, T9's queue of 1,024, is full waits here, in order, in RAM, up to 4,096 requests, 256
// of them reserved for speech. Nothing jumps the ring: while it holds anything, every new request joins it.
//
// - Speech (a PlayerSaid, an NpcSaid page) may take any slot, the reserve included, and past a full ring it is still
//   kept, so a shown line is never lost; while the ring is full the chat shows no new page (`pages_held`).
// - The store's own requests (open, close, a conversation's row, a job, a query) are kept likewise: they are few, and
//   dropping one would undo the order of what follows it.
// - An event's append (a P event that is not speech) takes the ring's non-speech part. When that part is full, saving
//   events pauses (`behind`: the toast "Memory is behind; recent events are not being saved"), and every event's
//   append until it resumes is not saved, only counted. Saving resumes once the ring is empty and C8 holds fewer than
//   512.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>

#include "en/proto/store.hpp"

namespace en::shell {

class SpillRing {
 public:
  struct Limits {
    std::size_t ring = 4096;
    std::size_t speech_reserve = 256;
    std::size_t resume_below = 512;
  };
  // Tries C8: true when it took the request (then moved from), false when it was full (the request untouched).
  using Send = std::function<bool(proto::DbRequest&)>;
  enum class Push : std::uint8_t { Sent, Spilled, NotSaved };

  SpillRing() = default;
  explicit SpillRing(Limits l) : limits_(l) {}

  Push push(proto::DbRequest request, const Send& send);
  // The ring's oldest first, while C8 takes them; then whether saving resumes, by C8's count of what waits.
  void drain(const Send& send, std::size_t c8_waiting);

  [[nodiscard]] bool behind() const noexcept { return paused_; }
  [[nodiscard]] bool pages_held() const noexcept { return ring_.size() >= limits_.ring; }
  [[nodiscard]] std::size_t spilled() const noexcept { return ring_.size(); }
  [[nodiscard]] std::uint64_t not_saved() const noexcept { return not_saved_; }
  // An event's append: the only request the ring may leave unsaved.
  [[nodiscard]] static bool droppable(const proto::DbRequest& r) noexcept;

 private:
  Limits limits_;
  std::deque<proto::DbRequest> ring_;
  std::size_t ring_speech_ = 0;  // the speech among them
  bool paused_ = false;
  std::uint64_t not_saved_ = 0;
};

}  // namespace en::shell
