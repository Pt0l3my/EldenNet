// The scrubbed logger (dll.hpp; ARCH §1.2's T11): any thread pushes a line onto a ring of fixed lines and never waits;
// T11 writes them to logs\eldennet.log as `ELDENNET <event> key=value ... t=<ms>`, rotating it past 1 MiB (one older
// file kept). Lines made before the logger starts (T1's Pin and Gate) wait in the ring, so a second copy, which never
// starts it, never writes to the shared log (ARCH §3.2's Safety). When the ring is full a line is dropped and counted.
#include <cinttypes>
#include <cstdio>

#include "dll.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

namespace {

constexpr std::uint64_t kRotateBytes = 1u << 20;

// Made on first use by a constant-initialised pointer and never freed (R14).
constinit std::atomic<en::shell::LogRing*> g_ring{nullptr};
// The lines T11 wrote, for T6's logs stream (EldenNet's pages, System › Logs): T11 the producer, T6 the consumer; a
// line that finds it full is not offered (the page's ring is 500 rows; T6 drains this one every wake).
using TailRing = en::shell::SpscRing<en::shell::LogEntry, 256>;
constinit std::atomic<TailRing*> g_tail{nullptr};
// settings.toml's edits from the pages, posted by T6 and written here, off the core thread (D14): the newest batch
// only, each holding every edit of the session so far.
constinit std::atomic<std::vector<en::shell::SettingsEdit>*> g_edits{nullptr};
constinit wchar_t g_settings[MAX_PATH] = {};
constinit bool g_probe_build = false;
constinit std::atomic<int> g_level{static_cast<int>(en::shell::LogLevel::Info)};
constinit std::atomic<bool> g_started{false};
constinit wchar_t g_file[MAX_PATH] = {};
constinit wchar_t g_older[MAX_PATH] = {};

en::shell::LogRing* ring() noexcept {
  en::shell::LogRing* r = g_ring.load(std::memory_order_acquire);
  if (r != nullptr) return r;
  auto* made = new (std::nothrow) en::shell::LogRing();
  if (made == nullptr) return nullptr;
  if (!g_ring.compare_exchange_strong(r, made, std::memory_order_acq_rel)) {
    delete made;  // another thread made it first
    return r;
  }
  return made;
}

TailRing* tail() noexcept {
  TailRing* t = g_tail.load(std::memory_order_acquire);
  if (t != nullptr) return t;
  auto* made = new (std::nothrow) TailRing();
  if (made == nullptr) return nullptr;
  if (!g_tail.compare_exchange_strong(t, made, std::memory_order_acq_rel)) {
    delete made;
    return t;
  }
  return made;
}

// One batch of the pages' edits written to settings.toml: read again, edited (en_shell's edit_settings, which never
// writes over a file the parse refuses), written beside it and renamed over it.
void write_settings(const std::vector<en::shell::SettingsEdit>& edits) {
  const std::wstring file(g_settings);
  if (file.empty()) return;
  const std::optional<std::string> text = read_text(file, en::shell::kMaxSettingsBytes);
  const en::shell::EditedSettings edited = en::shell::edit_settings(text.value_or(""), edits, g_probe_build);
  bool ok = edited.ok;
  if (ok) {
    const std::size_t cut = file.find_last_of(L'\\');
    if (cut != std::wstring::npos) (void)make_dirs(file.substr(0, cut));
    ok = write_text(file, edited.text);
  }
  log(en::shell::Line("settings_write").flag("ok", ok).num("keys", edits.size())
          .word("why", edited.ok ? (ok ? "none" : "write") : edited.why),
      ok ? en::shell::LogLevel::Info : en::shell::LogLevel::Warn);
}

HANDLE open_log() noexcept {
  HANDLE f = CreateFileW(g_file, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  return f;
}

DWORD WINAPI writer_main(void*) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, DWORD{1}, []() -> DWORD {
    HANDLE f = open_log();
    LARGE_INTEGER size{};
    std::uint64_t written = f != INVALID_HANDLE_VALUE && GetFileSizeEx(f, &size) ? static_cast<std::uint64_t>(size.QuadPart) : 0;
    en::shell::LogEntry e;
    char out[en::shell::kLineBytes + 64];
    for (;;) {
      bool any = false;
      while (en::shell::LogRing* r = ring()) {
        if (!r->try_pop(e)) break;
        any = true;
        const int n = std::snprintf(out, sizeof out, "ELDENNET %.*s t=%" PRIu64 "\r\n", static_cast<int>(e.len),
                                    e.text.data(), e.at_ms);
        if (n <= 0) continue;
        if (TailRing* t = tail()) (void)t->try_push(e);  // the pages' copy, written or not
        if (f == INVALID_HANDLE_VALUE) continue;
        DWORD put = 0;
        WriteFile(f, out, static_cast<DWORD>(n), &put, nullptr);
        written += put;
        if (written > kRotateBytes) {
          CloseHandle(f);
          MoveFileExW(g_file, g_older, MOVEFILE_REPLACE_EXISTING);
          f = open_log();
          written = 0;
        }
      }
      if (any && f != INVALID_HANDLE_VALUE) FlushFileBuffers(f);
      if (std::vector<en::shell::SettingsEdit>* edits = g_edits.exchange(nullptr, std::memory_order_acq_rel)) {
        write_settings(*edits);
        delete edits;
      }
      Sleep(50);
    }
  });
}

}  // namespace

void log(const en::shell::Line& line, en::shell::LogLevel level) noexcept {
  if (static_cast<int>(level) > g_level.load(std::memory_order_relaxed)) return;
  if (en::shell::LogRing* r = ring()) (void)r->try_push(line.text(), since_ms());
}

void set_log_level(en::shell::LogLevel level) noexcept { g_level.store(static_cast<int>(level), std::memory_order_relaxed); }

bool take_log_tail(en::shell::LogEntry& out) noexcept {
  TailRing* t = g_tail.load(std::memory_order_acquire);
  return t != nullptr && t->try_pop(out);
}

void set_settings_file(const std::wstring& path, bool probe_build) noexcept {
  if (path.size() >= MAX_PATH) return;
  path.copy(g_settings, path.size());
  g_probe_build = probe_build;
}

void post_settings_write(std::vector<en::shell::SettingsEdit> edits) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto* batch = new std::vector<en::shell::SettingsEdit>(std::move(edits));
    delete g_edits.exchange(batch, std::memory_order_acq_rel);  // an older batch not yet written: this one holds it all
  });
}

std::uint64_t log_dropped() noexcept {
  en::shell::LogRing* r = g_ring.load(std::memory_order_acquire);
  return r == nullptr ? 0 : r->dropped();
}

bool start_logger(const std::wstring& folder) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    if (g_started.exchange(true)) return true;
    const std::wstring file = folder + L"\\eldennet.log";
    const std::wstring older = folder + L"\\eldennet.1.log";
    if (folder.empty() || file.size() >= MAX_PATH || older.size() >= MAX_PATH || !make_dirs(folder)) return false;
    file.copy(g_file, file.size());
    older.copy(g_older, older.size());
    HANDLE t = CreateThread(nullptr, 0, &writer_main, nullptr, 0, nullptr);
    if (t == nullptr) return false;
    CloseHandle(t);
    return true;
  });
}

}  // namespace eldennet
