// The launcher's console lines off its watch loop (lines.hpp; WHY§290).
#include "en/launcher/lines.hpp"

#include <charconv>
#include <chrono>

namespace en::launcher {

bool LineQueue::push(std::string line) {
  {
    std::lock_guard<std::mutex> lock(m_);
    if (closed_ || lines_.size() >= capacity_) {
      ++dropped_;
      return false;
    }
    lines_.push_back(std::move(line));
    ++pushed_;
  }
  ready_.notify_one();
  return true;
}

std::optional<std::string> LineQueue::pop() {
  std::unique_lock<std::mutex> lock(m_);
  ready_.wait(lock, [&] { return closed_ || !lines_.empty(); });
  if (lines_.empty()) return std::nullopt;
  std::string line = std::move(lines_.front());
  lines_.pop_front();
  return line;
}

void LineQueue::written() {
  {
    std::lock_guard<std::mutex> lock(m_);
    ++written_;
  }
  done_.notify_all();
}

void LineQueue::close() {
  {
    std::lock_guard<std::mutex> lock(m_);
    closed_ = true;
  }
  ready_.notify_all();
}

bool LineQueue::drain(std::uint32_t ms) {
  std::unique_lock<std::mutex> lock(m_);
  return done_.wait_for(lock, std::chrono::milliseconds(ms), [&] { return written_ >= pushed_; });
}

std::uint64_t LineQueue::pushed() const {
  std::lock_guard<std::mutex> lock(m_);
  return pushed_;
}
std::uint64_t LineQueue::dropped() const {
  std::lock_guard<std::mutex> lock(m_);
  return dropped_;
}
std::uint64_t LineQueue::written_count() const {
  std::lock_guard<std::mutex> lock(m_);
  return written_;
}

ConsoleHook parse_console_hook(std::string_view text) {
  ConsoleHook h;
  const auto number = [](std::string_view item, std::string_view key) -> std::optional<std::uint64_t> {
    if (item.substr(0, key.size()) != key) return std::nullopt;
    const std::string_view v = item.substr(key.size());
    std::uint64_t n = 0;
    const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
    if (ec != std::errc() || end != v.data() + v.size() || v.empty()) return std::nullopt;
    return n;
  };
  while (!text.empty()) {
    const std::size_t comma = text.find(',');
    const std::string_view item = text.substr(0, comma);
    text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
    if (const auto n = number(item, "block_after=")) {
      h.block_after = *n;
    } else if (const auto m = number(item, "slow_ms="); m && *m <= 10000) {
      h.slow_ms = static_cast<std::uint32_t>(*m);
    } else if (const auto q = number(item, "queue="); q && *q >= 1 && *q <= LineQueue::kDefaultCapacity) {
      h.queue = static_cast<std::size_t>(*q);
    } else {
      return ConsoleHook{};
    }
  }
  return h;
}

}  // namespace en::launcher
