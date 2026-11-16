// eldennet.dll's view of the process and the PC (dll.hpp): its clock, its paths, files, the main window, the module list,
// the parent process and starting the watcher. Nothing here reads the game's memory.
#include "dll.hpp"  // windows.h first: tlhelp32.h needs its types

#include <tlhelp32.h>

#include <algorithm>

#include "en/proto/ffi_guard.hpp"
#include "en/shell/logline.hpp"
#include "en/shell/names.hpp"

namespace eldennet {

namespace {

constinit std::atomic<std::uint64_t> g_start_ms{0};

std::uint64_t qpc_ms() noexcept {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return en::proto::watch::mono_ms(static_cast<std::uint64_t>(c.QuadPart), static_cast<std::uint64_t>(f.QuadPart));
}

std::wstring env(const wchar_t* name) {
  wchar_t buf[1024];
  const DWORD n = GetEnvironmentVariableW(name, buf, 1024);
  return n == 0 || n >= 1024 ? std::wstring() : std::wstring(buf, n);
}

std::wstring parent_of(const std::wstring& path) {
  const std::size_t at = path.find_last_of(L"\\/");
  return at == std::wstring::npos ? std::wstring() : path.substr(0, at);
}

// The window search's answer, per thread (only T1 searches): no LPARAM carries a pointer, so nothing is cast.
constinit thread_local bool t_window_found = false;

BOOL CALLBACK window_of_ours(HWND hwnd, LPARAM) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, TRUE, [&]() -> BOOL {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) != FALSE && GetWindow(hwnd, GW_OWNER) == nullptr) {
      t_window_found = true;
      return FALSE;
    }
    return TRUE;
  });
}

}  // namespace

std::uint64_t mono_ms() noexcept { return qpc_ms(); }
std::uint64_t since_ms() noexcept {
  const std::uint64_t now = qpc_ms();
  const std::uint64_t start = g_start_ms.load(std::memory_order_relaxed);
  return now >= start ? now - start : 0;
}
void set_start() noexcept { g_start_ms.store(qpc_ms(), std::memory_order_relaxed); }

Paths find_paths(HMODULE module) {
  Paths p;
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (n < path.size()) {
      path.resize(n);
      break;
    }
    path.resize(path.size() * 2);
  }
  p.dll = path;
  p.dll_dir = parent_of(path);
  p.gal = p.dll_dir + L"\\gal";
  // ELDENNET_APPDATA: the folder in %APPDATA%'s place, for development and the tests (the launcher's --appdata).
  std::wstring root = env(L"ELDENNET_APPDATA");
  p.data_overridden = !root.empty();
  if (root.empty()) root = env(L"APPDATA");
  p.data_root = root;
  p.data = root.empty() ? std::wstring() : root + L"\\EldenNet";
  return p;
}

std::optional<std::string> read_text(const std::wstring& path, std::size_t max_bytes) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return std::nullopt;
  LARGE_INTEGER size{};
  std::optional<std::string> out;
  if (GetFileSizeEx(f, &size) && size.QuadPart >= 0 && static_cast<std::uint64_t>(size.QuadPart) <= max_bytes) {
    std::string text(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD got = 0;
    if (text.empty() || (ReadFile(f, text.data(), static_cast<DWORD>(text.size()), &got, nullptr) && got == text.size())) {
      out = std::move(text);
    }
  } else if (size.QuadPart >= 0) {
    out = std::string();  // too large: the caller's parser refuses it by its size
    out->resize(max_bytes + 1);
  }
  CloseHandle(f);
  return out;
}

bool write_text(const std::wstring& path, std::string_view text) {
  const std::wstring part = path + L".part";
  HANDLE f = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  DWORD put = 0;
  const bool ok = WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &put, nullptr) && put == text.size();
  CloseHandle(f);
  return ok && MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
}

bool make_dirs(const std::wstring& path) {
  if (path.empty()) return false;
  if (CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) return true;
  const std::wstring up = parent_of(path);
  if (up.empty() || up == path || !make_dirs(up)) return false;
  return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool main_window() noexcept {
  t_window_found = false;
  EnumWindows(&window_of_ours, 0);
  return t_window_found;
}

std::vector<std::wstring> module_names() {
  std::vector<std::wstring> out;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snap == INVALID_HANDLE_VALUE) return out;
  MODULEENTRY32W m{};
  m.dwSize = sizeof m;
  for (BOOL ok = Module32FirstW(snap, &m); ok != FALSE; ok = Module32NextW(snap, &m)) out.emplace_back(m.szModule);
  CloseHandle(snap);
  return out;
}

std::size_t other_copies(HMODULE ours) {
  std::size_t n = 0;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snap == INVALID_HANDLE_VALUE) return 0;
  MODULEENTRY32W m{};
  m.dwSize = sizeof m;
  for (BOOL ok = Module32FirstW(snap, &m); ok != FALSE; ok = Module32NextW(snap, &m)) {
    if (m.hModule != ours && GetProcAddress(m.hModule, "eldennet_instance_marker") != nullptr) ++n;
  }
  CloseHandle(snap);
  return n;
}

std::wstring parent_name() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return {};
  PROCESSENTRY32W e{};
  e.dwSize = sizeof e;
  const DWORD self = GetCurrentProcessId();
  DWORD parent = 0;
  for (BOOL ok = Process32FirstW(snap, &e); ok != FALSE; ok = Process32NextW(snap, &e)) {
    if (e.th32ProcessID == self) parent = e.th32ParentProcessID;
  }
  std::wstring name;
  e.dwSize = sizeof e;
  for (BOOL ok = Process32FirstW(snap, &e); ok != FALSE && parent != 0; ok = Process32NextW(snap, &e)) {
    if (e.th32ProcessID == parent) name = e.szExeFile;
  }
  CloseHandle(snap);
  return std::wstring(en::shell::file_name_of(name));  // empty: the parent has exited (the clause is skipped)
}

bool start_process(const std::wstring& exe, std::wstring command_line) {
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                      &si, &pi)) {
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

std::string word_of(std::wstring_view name) {
  std::string out;
  for (const wchar_t c : name) {
    if (c > 0x7E || c < 0x20) return "redacted";
    out += static_cast<char>(c);
  }
  return en::shell::is_word(out) ? out : "redacted";
}

}  // namespace eldennet
