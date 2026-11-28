#pragma once
// The store (ARCH §2.2; MEM §8.6, §9): `StoreCore` answers each `DbRequest` with one `DbResult`, synchronously, on the
// thread that calls it: the tests and the eval use it so, and T9 runs it behind C8 (loop.hpp). One playthrough file is
// open at a time, through its key (files.hpp); every write is one transaction; a query reads the persona's own rows.
//
// Opening a file (MEM §8.3, §9.1, §3.8; O144): the key's file is found or made, its start backup taken, migrated, and
// its timeline checked against the save's play time: Clean, a silent trim, or the choice, which is asked only when what
// the cut would undo matters; then open conversations are closed as crashed and running jobs queued again. While the
// choice waits, only Resolve, Close and the summary's query are answered; writes wait in the core's buffer.
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "en/proto/store.hpp"
#include "en/store/playthrough.hpp"

namespace en::store {

struct StoreCfg {
  std::filesystem::path saves;     // %APPDATA%\EldenNet\saves: under it, <SteamID64>\ per account (MEM §8.1)
  std::string build = "eldennet";  // meta's created_by and last_opened_by: the build's version, never a path
  // The clocks and the disk, the tests' own when they give them: wall time in seconds since the epoch (file names and
  // rows' written times), monotonic time in ms (the breaker's window), and the free bytes under a folder.
  std::function<std::int64_t()> wall;
  std::function<std::uint64_t()> mono_ms;
  std::function<std::optional<std::uint64_t>(const std::filesystem::path&)> free_bytes;
  int busy_timeout_ms = Db::kBusyTimeoutMs;  // MEM §8.5's 2 s; the tests' own, shorter, for a lock
  // The tests' stall: called before each request the loop runs (loop.hpp), so a test can hold T9 still.
  std::function<void()> before_request;
};

// MEM §8.6's breaker: three failed transactions (any fault but Busy) within 60 s of monotonic time, or one Corrupt,
// NotADb or IoErr, trips the store for that file for the session. A refusal before SQL (Invalid, NotOpen) or a newer
// file (TooNew) is no failed transaction.
class Breaker {
 public:
  static constexpr std::uint64_t kWindowMs = 60000;
  static constexpr std::size_t kFailures = 3;
  // Counts the fault; true once tripped.
  bool note(Fault f, std::uint64_t mono_ms);
  [[nodiscard]] bool tripped() const noexcept { return cause_.has_value(); }
  [[nodiscard]] std::optional<Fault> cause() const noexcept { return cause_; }
  [[nodiscard]] static bool counts(Fault f) noexcept;

 private:
  std::vector<std::uint64_t> recent_;
  std::optional<Fault> cause_;
};

// MEM §9.1: a gap of at most 5 min of play is a crash after the last autosave, trimmed silently.
inline constexpr proto::Pt kSilentTrimMs = 300000;

class StoreCore {
 public:
  explicit StoreCore(StoreCfg cfg);
  ~StoreCore();
  StoreCore(const StoreCore&) = delete;
  StoreCore& operator=(const StoreCore&) = delete;

  // One request, one result of the same sequence number.
  [[nodiscard]] proto::DbResult handle(const proto::DbRequest& request);

  // A run of appends in one transaction (MEM §8.6: a burst of more than 16): every one or none; on a failure the
  // caller runs them one by one to find the bad one.
  [[nodiscard]] Result<void> append_all(const std::vector<proto::db::Append>& appends);

  [[nodiscard]] bool open() const noexcept { return pt_.has_value(); }
  // Whether the store tripped (MEM §8.6) for the file of a key, and why: memory is off for that file this session.
  [[nodiscard]] std::optional<Fault> tripped_for(const proto::PlaythroughKey& key) const;
  [[nodiscard]] bool awaiting_choice() const noexcept { return choice_.has_value(); }
  // The open file, for the tests' own reads.
  [[nodiscard]] const Playthrough* playthrough() const noexcept { return pt_ ? &*pt_ : nullptr; }

 private:
  struct Choice {
    proto::Pt cut = 0;
    proto::Pt pt_now = 0;
    std::uint16_t journey = 1;
    proto::db::Rollback rollback;
    std::string account_pick;  // the open's, for a new character's file
  };

  proto::db::Opened do_open(const proto::db::Open& o, std::optional<proto::db::Fault>& fault);
  proto::db::Opened do_resolve(const proto::db::Resolve& r, std::optional<proto::db::Fault>& fault);
  Result<proto::db::Opened> finish_open(proto::db::Opened opened);
  Result<void> init_file(const proto::PlaythroughKey& key, proto::Pt pt_now, std::uint16_t journey,
                         const std::string& account_pick);
  Result<std::uint64_t> forget(const proto::db::Forget& f);
  proto::DbResult run(const proto::DbRequest& request);
  void trip(Fault cause);
  void close() noexcept;
  std::int64_t wall() const;
  std::uint64_t mono() const;

  StoreCfg cfg_;
  std::optional<Playthrough> pt_;
  std::optional<Choice> choice_;
  Breaker breaker_;
  std::vector<std::pair<std::string, Fault>> tripped_;  // the files tripped this session: steamid/slot/fp8, and why
};

}  // namespace en::store
