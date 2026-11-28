// The store's spill ring (spill.hpp).
#include "en/shell/spill.hpp"

#include <utility>
#include <variant>

namespace en::shell {

bool SpillRing::droppable(const proto::DbRequest& r) noexcept {
  return std::holds_alternative<proto::db::Append>(r.op) && !r.speech();
}

SpillRing::Push SpillRing::push(proto::DbRequest request, const Send& send) {
  const bool event = droppable(request);
  if (event && paused_) {
    ++not_saved_;
    return Push::NotSaved;
  }
  if (ring_.empty() && send(request)) return Push::Sent;
  if (event) {
    // The non-speech part: the ring less the speech reserve, the store's own requests in it too.
    if (ring_.size() - ring_speech_ >= limits_.ring - limits_.speech_reserve || ring_.size() >= limits_.ring) {
      paused_ = true;
      ++not_saved_;
      return Push::NotSaved;
    }
  }
  if (request.speech()) ++ring_speech_;
  ring_.push_back(std::move(request));
  return Push::Spilled;
}

void SpillRing::drain(const Send& send, std::size_t c8_waiting) {
  while (!ring_.empty()) {
    const bool speech = ring_.front().speech();
    if (!send(ring_.front())) break;
    if (speech) --ring_speech_;
    ring_.pop_front();
    ++c8_waiting;
  }
  if (paused_ && ring_.empty() && c8_waiting < limits_.resume_below) paused_ = false;
}

}  // namespace en::shell
