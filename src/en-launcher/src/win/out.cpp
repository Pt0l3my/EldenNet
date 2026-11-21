// The launcher's lines, its log, and text and time helpers (win.hpp). Since spike.10 (WHY§290) the log is written
// first, and the console (or the standard output a test reads) by a writer thread of its own through a bounded queue
// (lines.hpp), so that a console whose output is suspended never stops the watch loop; QuickEdit is off for the
// launcher's console while it runs.
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <system_error>
#include <thread>

#include "en/launcher/lines.hpp"
#include "win.hpp"

namespace en::launcher::win {

namespace {

HANDLE g_out = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;
SRWLOCK g_lock = SRWLOCK_INIT;
std::uint64_t g_start_ms = 0;
// The console's writer (WHY§290): its queue, the test hook, and the console input's mode as it was found.
LineQueue* g_queue = nullptr;  // made once, never freed: the writer may still hold it at the process's end
ConsoleHook g_hook;
HANDLE g_conin = INVALID_HANDLE_VALUE;
DWORD g_conin_mode = 0;
bool g_mode_saved = false;

void write_all(HANDLE h, const char* data, DWORD n) {
  DWORD written = 0;
  if (h != nullptr && h != INVALID_HANDLE_VALUE) WriteFile(h, data, n, &written, nullptr);
}

std::string stamp_line(const char* text) {
  char line[256];
  const int n = std::snprintf(line, sizeof line, "LAUNCHER %s t=%llu\n", text,
                              static_cast<unsigned long long>(mono_ms() - g_start_ms));
  return std::string(line, n > 0 ? static_cast<std::size_t>(n) : 0);
}

// A line for the log alone (the console's own notes).
void log_only(const std::string& line) {
  AcquireSRWLockExclusive(&g_lock);
  write_all(g_log, line.data(), static_cast<DWORD>(line.size()));
  ReleaseSRWLockExclusive(&g_lock);
}

// The writer: each line in turn, however long the console takes; the test hook can make it slow, or block it for good.
void console_writer(LineQueue* q, HANDLE out, ConsoleHook hook) {
  std::uint64_t n = 0;
  while (const std::optional<std::string> line = q->pop()) {
    if (hook.block_after && n >= *hook.block_after) {
      for (;;) Sleep(INFINITE);  // as a console with a selection: never back
    }
    if (hook.slow_ms != 0) Sleep(hook.slow_ms);
    write_all(out, line->data(), static_cast<DWORD>(line->size()));
    ++n;
    q->written();
  }
}

// A console line, queued, with g_lock held (the log's order is the console's): never waits. The first line dropped is
// noted in the log.
void to_console_locked(std::string line) {
  if (g_queue == nullptr) return;
  if (!g_queue->push(std::move(line)) && g_queue->dropped() == 1) {
    const std::string note = stamp_line("console dropped=1 queue_full=1");
    write_all(g_log, note.data(), static_cast<DWORD>(note.size()));
  }
}

// QuickEdit off for the launcher's console (a click inside it would suspend its output), its mode kept to restore.
void quick_edit_off() {
  g_conin = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, 0, nullptr);
  if (g_conin == INVALID_HANDLE_VALUE) return;
  DWORD mode = 0;
  if (GetConsoleMode(g_conin, &mode) == FALSE) {
    CloseHandle(g_conin);
    g_conin = INVALID_HANDLE_VALUE;
    return;
  }
  g_conin_mode = mode;
  const DWORD off = (mode | ENABLE_EXTENDED_FLAGS) & ~static_cast<DWORD>(ENABLE_QUICK_EDIT_MODE);
  g_mode_saved = SetConsoleMode(g_conin, off) != FALSE;
}

}  // namespace

std::wstring widen(std::string_view utf8) {
  if (utf8.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

std::string narrow(std::wstring_view wide) {
  if (wide.empty()) return {};
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(n > 0 ? n : 0), '\0');
  if (n > 0)
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
  return out;
}

std::uint64_t mono_ms() {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return proto::watch::mono_ms(static_cast<std::uint64_t>(c.QuadPart), static_cast<std::uint64_t>(f.QuadPart));
}

std::string local_stamp() {
  SYSTEMTIME t{};
  GetLocalTime(&t);
  char b[32];
  std::snprintf(b, sizeof b, "%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
  return b;
}

void open_output(bool console) {
  g_start_ms = mono_ms();
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if ((h == nullptr || h == INVALID_HANDLE_VALUE) && console) {
    // The dev console: the console the launcher was started from, or one of its own.
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  }
  g_out = (h == INVALID_HANDLE_VALUE) ? nullptr : h;
  if (g_out == nullptr || g_queue != nullptr) return;
  if (console) quick_edit_off();
  wchar_t hook[64] = {};
  const DWORD n = GetEnvironmentVariableW(L"ELDENNET_TEST_CONSOLE", hook, 64);
  if (n > 0 && n < 64) g_hook = parse_console_hook(narrow(std::wstring_view(hook, n)));
  g_queue = new LineQueue(g_hook.queue);
  std::thread(console_writer, g_queue, g_out, g_hook).detach();
}

void close_output() {
  if (g_queue == nullptr) return;
  g_queue->close();
  const bool drained = g_queue->drain(1000);  // a console that does not take them: given up after a second
  char note[128];
  std::snprintf(note, sizeof note, "console lines=%llu dropped=%llu drained=%d",
                static_cast<unsigned long long>(g_queue->pushed()),
                static_cast<unsigned long long>(g_queue->dropped()), drained ? 1 : 0);
  log_only(stamp_line(note));
  if (g_mode_saved) SetConsoleMode(g_conin, g_conin_mode);
  if (g_conin != INVALID_HANDLE_VALUE) CloseHandle(g_conin);
  g_conin = INVALID_HANDLE_VALUE;
  g_mode_saved = false;
}

void open_log(const fs::path& logs_dir) {
  std::error_code ec;
  fs::create_directories(logs_dir, ec);
  const fs::path log = logs_dir / "launcher.log";
  if (fs::exists(log, ec) && fs::file_size(log, ec) > (1u << 20)) {
    fs::rename(log, logs_dir / "launcher.1.log", ec);  // one older log is kept
  }
  g_log = CreateFileW(log.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, nullptr);
}

void say(const char* fmt, ...) {
  char line[1536];
  int n = std::snprintf(line, sizeof line, "LAUNCHER ");
  va_list args;
  va_start(args, fmt);
  const int m = std::vsnprintf(line + n, sizeof line - static_cast<std::size_t>(n) - 40, fmt, args);
  va_end(args);
  n += m < 0 ? 0 : (m > static_cast<int>(sizeof line) - n - 40 ? static_cast<int>(sizeof line) - n - 40 : m);
  n += std::snprintf(line + n, 40, " t=%llu", static_cast<unsigned long long>(mono_ms() - g_start_ms));
  line[n++] = '\n';
  // The log first, then the console's queue, in the same order for every line (WHY§290).
  AcquireSRWLockExclusive(&g_lock);
  write_all(g_log, line, static_cast<DWORD>(n));
  if (g_queue != nullptr) {
    to_console_locked(std::string(line, static_cast<std::size_t>(n)));
  } else {
    write_all(g_out, line, static_cast<DWORD>(n));  // before the console's writer starts: a usage line
  }
  ReleaseSRWLockExclusive(&g_lock);
}

void console_line(std::string_view text) {
  std::string line = "EldenNet R1: ";
  line.append(text);
  line += "\r\n";
  AcquireSRWLockExclusive(&g_lock);
  to_console_locked(std::move(line));
  ReleaseSRWLockExclusive(&g_lock);
}

}  // namespace en::launcher::win
