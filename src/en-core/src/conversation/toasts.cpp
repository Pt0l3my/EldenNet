// The toasts' policy (toasts.hpp; NPC §10.5).
#include "en/core/toasts.hpp"

#include <algorithm>

namespace en::core {

namespace {

bool limited(ToastClass c) noexcept {
  return c != ToastClass::Status && c != ToastClass::Danger && c != ToastClass::Input;
}

int rank(proto::ToastLevel l) noexcept {
  switch (l) {
    case proto::ToastLevel::Error: return 2;
    case proto::ToastLevel::Warning: return 1;  // a degradation
    case proto::ToastLevel::Info: return 0;
  }
  return 0;
}

}  // namespace

bool Toasts::post(ToastClass cls, proto::ToastLevel level, std::string text, Mono now, Mono hold_ms) {
  const auto c = static_cast<std::size_t>(cls);
  if (limited(cls) && posted_[c] && now - last_[c] < kRateMs) return false;
  const auto same = [&](const Entry& e) { return e.toast.text == text; };
  if (std::any_of(up_.begin(), up_.end(), same) || std::any_of(queue_.begin(), queue_.end(), same)) return false;
  if (text.size() > proto::kToastMaxBytes) {
    std::size_t cut = proto::kToastMaxBytes;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
    text.resize(cut);
  }
  if (queue_.size() >= kQueue) {
    // The oldest info toast goes first; with none, the oldest of the lowest level.
    auto victim = queue_.begin();
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
      if (rank(it->toast.level) < rank(victim->toast.level)) victim = it;
    }
    if (rank(victim->toast.level) > rank(level)) return false;  // everything waiting outranks it
    queue_.erase(victim);
  }
  posted_[c] = true;
  last_[c] = now;
  queue_.push_back(Entry{proto::Toast{next_id_++, level, std::move(text)}, hold_ms, 0});
  return true;
}

void Toasts::step(Mono now, bool boss_fight) {
  std::erase_if(up_, [&](const Entry& e) { return now >= e.until; });
  if (boss_fight) return;  // held, and shown when the fight ends
  while (up_.size() < kVisible && !queue_.empty()) {
    auto best = queue_.begin();
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
      if (rank(it->toast.level) > rank(best->toast.level)) best = it;
    }
    Entry e = std::move(*best);
    queue_.erase(best);
    e.until = now + e.hold_ms;
    up_.push_back(std::move(e));
  }
}

std::vector<proto::Toast> Toasts::visible() const {
  std::vector<proto::Toast> out;
  out.reserve(up_.size());
  for (const Entry& e : up_) out.push_back(e.toast);
  return out;
}

}  // namespace en::core
