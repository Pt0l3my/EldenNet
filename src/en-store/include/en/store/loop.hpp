#pragma once
// T9, the store's thread (ARCH §1.2, §1.3's C8; MEM §8.6): StoreCore behind two bounded queues of our own, 1,024 each
// way, a mutex and a condition variable around a ring. The core only tries to send (`send` never blocks: a full queue
// answers Full, and the shell keeps the request in its spill ring); the loop blocks on its queue with a 250 ms timeout
// (housekeeping only), drains everything there, and runs each request as its own transaction. In a burst of more than
// 16 consecutive appends the run shares one transaction; a failure runs them one by one to find the bad one. Each
// result is pushed back in order, and `notify` (the core's wake signal) is called after each drained batch.
//
// Housekeeping: on a quiet 250 ms after writes, at most once per 10 s of monotonic time, a passive checkpoint. Its
// result is dropped unless it trips the breaker; then a result of sequence number 0, the store's own, says so.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "en/proto/expected.hpp"
#include "en/proto/store.hpp"
#include "en/store/store.hpp"

namespace en::store {

template <class T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}
  // Never blocks: false when full, the item untouched.
  bool try_push(T& item) {
    std::lock_guard<std::mutex> lock(m_);
    if (q_.size() >= capacity_) return false;
    q_.push_back(std::move(item));
    cv_.notify_one();
    return true;
  }
  // Waits while full (T9's side of the results: the core drains them every wake), or until `stop` says so.
  bool push_wait(T& item, const std::function<bool()>& stop) {
    std::unique_lock<std::mutex> lock(m_);
    not_full_.wait(lock, [&] { return q_.size() < capacity_ || stop(); });
    if (q_.size() >= capacity_) return false;
    q_.push_back(std::move(item));
    cv_.notify_one();
    return true;
  }
  bool try_pop(T& out) {
    std::lock_guard<std::mutex> lock(m_);
    if (q_.empty()) return false;
    out = std::move(q_.front());
    q_.pop_front();
    not_full_.notify_one();
    return true;
  }
  // Waits up to `timeout` for an item, or until `wake` says to stop (checked under the lock).
  template <class Rep, class Period>
  bool pop_wait(T& out, std::chrono::duration<Rep, Period> timeout, const std::function<bool()>& wake) {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait_for(lock, timeout, [&] { return !q_.empty() || wake(); });
    if (q_.empty()) return false;
    out = std::move(q_.front());
    q_.pop_front();
    not_full_.notify_one();
    return true;
  }
  void notify_all() {
    cv_.notify_all();
    not_full_.notify_all();
  }
  [[nodiscard]] std::size_t size() const {
    std::lock_guard<std::mutex> lock(m_);
    return q_.size();
  }

 private:
  const std::size_t capacity_;
  mutable std::mutex m_;
  std::condition_variable cv_;
  std::condition_variable not_full_;
  std::deque<T> q_;
};

inline constexpr std::size_t kC8Capacity = 1024;  // ARCH §1.3
inline constexpr std::size_t kBurstAppends = 16;  // MEM §8.6
inline constexpr std::uint64_t kCheckpointGapMs = 10000;
struct Full {};

class StoreHandle {
 public:
  StoreHandle(StoreCfg cfg, std::function<void()> notify);
  ~StoreHandle();  // stops and joins T9 (the tests' end; the DLL keeps its handle for the process, R14)
  StoreHandle(const StoreHandle&) = delete;
  StoreHandle& operator=(const StoreHandle&) = delete;

  // Never blocks (the core's side): Full when C8's 1,024 are waiting.
  [[nodiscard]] proto::Expected<void, Full> send(proto::DbRequest request);
  // The same for the shell's spill ring: true when C8 took it (moved from), false when full (the request untouched).
  [[nodiscard]] bool try_send(proto::DbRequest& request) { return requests_.try_push(request); }
  // The next result, in the order the requests ran.
  [[nodiscard]] bool poll(proto::DbResult& out);
  [[nodiscard]] std::size_t waiting() const { return requests_.size(); }
  // How many housekeeping checkpoints have run, for the tests.
  [[nodiscard]] std::uint64_t checkpoints() const noexcept { return checkpoints_.load(); }
  void stop();

 private:
  void run();
  void flush_appends(std::vector<proto::DbRequest>& run);
  void housekeeping();
  [[nodiscard]] bool stopping();
  [[nodiscard]] std::uint64_t mono() const;

  // Declared before the core, so they are copied out of the configuration before it moves in.
  std::function<void()> before_;
  std::function<std::uint64_t()> mono_;
  StoreCore core_;
  std::function<void()> notify_;
  bool dirty_ = false;                         // a write ran since the last checkpoint
  std::optional<std::uint64_t> checkpointed_;  // when the last one ran
  std::atomic<std::uint64_t> checkpoints_{0};
  BoundedQueue<proto::DbRequest> requests_{kC8Capacity};
  BoundedQueue<proto::DbResult> results_{kC8Capacity};
  std::mutex stop_m_;
  bool stopping_ = false;
  std::thread thread_;
};

// ARCH §2.2's `Store::spawn`: T9 started, its handle answered.
struct Store {
  [[nodiscard]] static std::unique_ptr<StoreHandle> spawn(StoreCfg cfg, std::function<void()> notify);
};

}  // namespace en::store
