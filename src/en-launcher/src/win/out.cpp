// The launcher's lines, its log, and text and time helpers (win.hpp).
#include <cstdarg>
#include <cstdio>
#include <system_error>

#include "win.hpp"

namespace en::launcher::win {

namespace {

HANDLE g_out = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;
SRWLOCK g_lock = SRWLOCK_INIT;
std::uint64_t g_start_ms = 0;

void write_all(HANDLE h, const char* data, DWORD n) {
  DWORD written = 0;
  if (h != nullptr && h != INVALID_HANDLE_VALUE) WriteFile(h, data, n, &written, nullptr);
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
  AcquireSRWLockExclusive(&g_lock);
  write_all(g_out, line, static_cast<DWORD>(n));
  write_all(g_log, line, static_cast<DWORD>(n));
  ReleaseSRWLockExclusive(&g_lock);
}

void console_line(std::string_view text) {
  std::string line = "EldenNet R1: ";
  line.append(text);
  line += "\r\n";
  AcquireSRWLockExclusive(&g_lock);
  write_all(g_out, line.data(), static_cast<DWORD>(line.size()));
  ReleaseSRWLockExclusive(&g_lock);
}

}  // namespace en::launcher::win
