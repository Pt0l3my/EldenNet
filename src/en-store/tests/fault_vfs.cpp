// The tests' fault VFS (fault_vfs.hpp).
#include "fault_vfs.hpp"

#include <sqlite3.h>

#include <cstring>

namespace en::store::test {

namespace {

Faults g_faults;
sqlite3_vfs* g_base = nullptr;
sqlite3_vfs g_vfs{};

struct ShimFile {
  sqlite3_file base;   // its methods: the shim's
  sqlite3_file* real;  // the default VFS's file, right after this struct
  int flags;
};

sqlite3_file* real_of(sqlite3_file* f) { return reinterpret_cast<ShimFile*>(f)->real; }

int x_close(sqlite3_file* f) { return real_of(f)->pMethods->xClose(real_of(f)); }

int x_read(sqlite3_file* f, void* buf, int amt, sqlite3_int64 off) {
  const int rc = real_of(f)->pMethods->xRead(real_of(f), buf, amt, off);
  const bool main_db = (reinterpret_cast<ShimFile*>(f)->flags & SQLITE_OPEN_MAIN_DB) != 0;
  if (rc == SQLITE_OK && main_db && off > 0 && g_faults.garble_reads) std::memset(buf, 0xA5, static_cast<size_t>(amt));
  return rc;
}

int x_write(sqlite3_file* f, const void* buf, int amt, sqlite3_int64 off) {
  if (g_faults.write_ioerr) return SQLITE_IOERR_WRITE;
  if (const std::int64_t left = g_faults.full_after; left >= 0) {
    if (left < amt) {
      g_faults.full_after = 0;
      return SQLITE_FULL;
    }
    g_faults.full_after = left - amt;
  }
  return real_of(f)->pMethods->xWrite(real_of(f), buf, amt, off);
}

int x_truncate(sqlite3_file* f, sqlite3_int64 size) { return real_of(f)->pMethods->xTruncate(real_of(f), size); }

int x_sync(sqlite3_file* f, int flags) {
  if (g_faults.sync_ioerr) return SQLITE_IOERR_FSYNC;
  return real_of(f)->pMethods->xSync(real_of(f), flags);
}

int x_file_size(sqlite3_file* f, sqlite3_int64* size) { return real_of(f)->pMethods->xFileSize(real_of(f), size); }
int x_lock(sqlite3_file* f, int level) { return real_of(f)->pMethods->xLock(real_of(f), level); }
int x_unlock(sqlite3_file* f, int level) { return real_of(f)->pMethods->xUnlock(real_of(f), level); }
int x_check_reserved(sqlite3_file* f, int* out) { return real_of(f)->pMethods->xCheckReservedLock(real_of(f), out); }
int x_file_control(sqlite3_file* f, int op, void* arg) {
  return real_of(f)->pMethods->xFileControl(real_of(f), op, arg);
}
int x_sector_size(sqlite3_file* f) { return real_of(f)->pMethods->xSectorSize(real_of(f)); }
int x_device(sqlite3_file* f) { return real_of(f)->pMethods->xDeviceCharacteristics(real_of(f)); }
int x_shm_map(sqlite3_file* f, int pg, int pgsz, int extend, void volatile** pp) {
  return real_of(f)->pMethods->xShmMap(real_of(f), pg, pgsz, extend, pp);
}
int x_shm_lock(sqlite3_file* f, int offset, int n, int flags) {
  return real_of(f)->pMethods->xShmLock(real_of(f), offset, n, flags);
}
void x_shm_barrier(sqlite3_file* f) { real_of(f)->pMethods->xShmBarrier(real_of(f)); }
int x_shm_unmap(sqlite3_file* f, int del) { return real_of(f)->pMethods->xShmUnmap(real_of(f), del); }
int x_fetch(sqlite3_file* f, sqlite3_int64 off, int amt, void** pp) {
  return real_of(f)->pMethods->xFetch(real_of(f), off, amt, pp);
}
int x_unfetch(sqlite3_file* f, sqlite3_int64 off, void* p) {
  return real_of(f)->pMethods->xUnfetch(real_of(f), off, p);
}

const sqlite3_io_methods kIo = {
    3,          x_close,       x_read,           x_write,        x_truncate,    x_sync,   x_file_size,
    x_lock,     x_unlock,      x_check_reserved, x_file_control, x_sector_size, x_device, x_shm_map,
    x_shm_lock, x_shm_barrier, x_shm_unmap,      x_fetch,        x_unfetch,
};

int v_open(sqlite3_vfs*, sqlite3_filename name, sqlite3_file* f, int flags, int* out_flags) {
  auto* s = reinterpret_cast<ShimFile*>(f);
  s->real = reinterpret_cast<sqlite3_file*>(s + 1);
  s->flags = flags;
  s->base.pMethods = nullptr;
  s->real->pMethods = nullptr;
  const int rc = g_base->xOpen(g_base, name, s->real, flags, out_flags);
  // SQLite closes a file whose methods are set, even after a failed open: the shim's then close the real one.
  if (s->real->pMethods != nullptr) s->base.pMethods = &kIo;
  return rc;
}
int v_delete(sqlite3_vfs*, const char* name, int sync) { return g_base->xDelete(g_base, name, sync); }
int v_access(sqlite3_vfs*, const char* name, int flags, int* out) { return g_base->xAccess(g_base, name, flags, out); }
int v_full(sqlite3_vfs*, const char* name, int n, char* out) { return g_base->xFullPathname(g_base, name, n, out); }
void* v_dlopen(sqlite3_vfs*, const char* name) { return g_base->xDlOpen(g_base, name); }
void v_dlerror(sqlite3_vfs*, int n, char* out) { g_base->xDlError(g_base, n, out); }
void (*v_dlsym(sqlite3_vfs*, void* h, const char* sym))(void) { return g_base->xDlSym(g_base, h, sym); }
void v_dlclose(sqlite3_vfs*, void* h) { g_base->xDlClose(g_base, h); }
int v_random(sqlite3_vfs*, int n, char* out) { return g_base->xRandomness(g_base, n, out); }
int v_sleep(sqlite3_vfs*, int us) { return g_base->xSleep(g_base, us); }
int v_time(sqlite3_vfs*, double* out) { return g_base->xCurrentTime(g_base, out); }
int v_last_error(sqlite3_vfs*, int n, char* out) { return g_base->xGetLastError(g_base, n, out); }
int v_time64(sqlite3_vfs*, sqlite3_int64* out) { return g_base->xCurrentTimeInt64(g_base, out); }

}  // namespace

FaultVfs::FaultVfs() {
  base_ = sqlite3_vfs_find(nullptr);
  g_base = base_;
  g_faults.clear();
  g_vfs = sqlite3_vfs{};
  g_vfs.iVersion = 2;
  g_vfs.szOsFile = static_cast<int>(sizeof(ShimFile)) + base_->szOsFile;
  g_vfs.mxPathname = base_->mxPathname;
  g_vfs.zName = "en-test-faults";
  g_vfs.xOpen = v_open;
  g_vfs.xDelete = v_delete;
  g_vfs.xAccess = v_access;
  g_vfs.xFullPathname = v_full;
  // Without extension loading (SQLITE_OMIT_LOAD_EXTENSION) the default VFS has no loader: neither has the shim.
  g_vfs.xDlOpen = base_->xDlOpen != nullptr ? v_dlopen : nullptr;
  g_vfs.xDlError = base_->xDlError != nullptr ? v_dlerror : nullptr;
  g_vfs.xDlSym = base_->xDlSym != nullptr ? v_dlsym : nullptr;
  g_vfs.xDlClose = base_->xDlClose != nullptr ? v_dlclose : nullptr;
  g_vfs.xRandomness = v_random;
  g_vfs.xSleep = v_sleep;
  g_vfs.xCurrentTime = v_time;
  g_vfs.xGetLastError = v_last_error;
  g_vfs.xCurrentTimeInt64 = v_time64;
  sqlite3_vfs_register(&g_vfs, 1);
}

FaultVfs::~FaultVfs() {
  g_faults.clear();
  sqlite3_vfs_unregister(&g_vfs);
  // Unregistering the default leaves SQLite to pick any other: the earlier one is named again.
  sqlite3_vfs_register(base_, 1);
}

Faults& FaultVfs::faults() { return g_faults; }

}  // namespace en::store::test
