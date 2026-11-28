#pragma once
// A fault VFS of the tests' own (MEM §8.6's breaker cases): a shim over SQLite's default VFS, made the default while a
// `FaultVfs` scope lives, which passes every call through and fails on demand: a write with FULL once a byte budget is
// spent, a write with IOERR, a sync with IOERR, or garbled reads of a main database's pages after the first (CORRUPT).
// Registered in the tests only; the store's code never names it.
#include <atomic>
#include <cstdint>

struct sqlite3_vfs;

namespace en::store::test {

struct Faults {
  std::atomic<std::int64_t> full_after{-1};  // bytes written before FULL; -1: off
  std::atomic<bool> write_ioerr{false};
  std::atomic<bool> sync_ioerr{false};
  std::atomic<bool> garble_reads{false};
  void clear() {
    full_after = -1;
    write_ioerr = false;
    sync_ioerr = false;
    garble_reads = false;
  }
};

class FaultVfs {
 public:
  FaultVfs();   // registers the shim as the default VFS
  ~FaultVfs();  // the earlier default back
  FaultVfs(const FaultVfs&) = delete;
  FaultVfs& operator=(const FaultVfs&) = delete;
  Faults& faults();

 private:
  sqlite3_vfs* base_ = nullptr;
};

}  // namespace en::store::test
