// The system as the launcher sees it: processes, modules, windows, the registry, connections (win.hpp; ARCH §4.2,
// §4.3). It never reads the game's memory (O73): module lists and windows come from the system.
#include "win.hpp"  // windows.h first

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <system_error>
#include <utility>

#include "en/proto/ffi_guard.hpp"

namespace en::launcher::win {

std::optional<fs::path> appdata_folder() {
  PWSTR path = nullptr;
  std::optional<fs::path> out;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)) && path != nullptr) {
    out = fs::path(path);
  }
  CoTaskMemFree(path);
  return out;
}

namespace {

std::optional<std::wstring> reg_string(HKEY root, const wchar_t* key, const wchar_t* value) {
  wchar_t buf[1024];
  DWORD size = sizeof buf;
  if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return std::nullopt;
  return std::wstring(buf);
}

std::optional<DWORD> reg_dword(HKEY root, const wchar_t* key, const wchar_t* value) {
  DWORD v = 0;
  DWORD size = sizeof v;
  if (RegGetValueW(root, key, value, RRF_RT_REG_DWORD, nullptr, &v, &size) != ERROR_SUCCESS) return std::nullopt;
  return v;
}

bool alive(DWORD pid) {
  HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (h == nullptr) return false;
  const bool running = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
  CloseHandle(h);
  return running;
}

}  // namespace

std::optional<fs::path> steam_folder() {
  if (auto p = reg_string(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath")) return fs::path(*p);
  if (auto p = reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath")) {
    return fs::path(*p);
  }
  return std::nullopt;
}

bool steam_running(const std::vector<ProcessEntry>& processes) {
  if (running(processes, "steam.exe")) return true;
  const auto pid = reg_dword(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"pid");
  return pid && *pid != 0 && alive(*pid);
}

std::vector<ProcessEntry> list_processes() {
  std::vector<ProcessEntry> out;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return out;
  PROCESSENTRY32W e{};
  e.dwSize = sizeof e;
  for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
    out.push_back(ProcessEntry{narrow(e.szExeFile), e.th32ProcessID, e.th32ParentProcessID});
  }
  CloseHandle(snap);
  return out;
}

std::vector<std::string> list_modules(DWORD pid) {
  std::vector<std::string> out;
  HANDLE snap = INVALID_HANDLE_VALUE;
  // A process still starting can refuse the snapshot for a moment (ERROR_BAD_LENGTH, ERROR_PARTIAL_COPY).
  for (int attempt = 0; attempt < 5 && snap == INVALID_HANDLE_VALUE; ++attempt) {
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) Sleep(20);
  }
  if (snap == INVALID_HANDLE_VALUE) return out;
  MODULEENTRY32W m{};
  m.dwSize = sizeof m;
  for (BOOL ok = Module32FirstW(snap, &m); ok; ok = Module32NextW(snap, &m)) {
    out.push_back(safe_module_name(narrow(m.szModule)));
  }
  CloseHandle(snap);
  return out;
}

bool module_loaded(DWORD pid, const fs::path& dll) {
  HANDLE snap = INVALID_HANDLE_VALUE;
  for (int attempt = 0; attempt < 5 && snap == INVALID_HANDLE_VALUE; ++attempt) {
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) Sleep(20);
  }
  if (snap == INVALID_HANDLE_VALUE) return false;
  const std::wstring want = fs::absolute(dll).wstring();
  const std::wstring want_name = dll.filename().wstring();
  bool found = false;
  MODULEENTRY32W m{};
  m.dwSize = sizeof m;
  for (BOOL ok = Module32FirstW(snap, &m); ok && !found; ok = Module32NextW(snap, &m)) {
    found = CompareStringOrdinal(m.szExePath, -1, want.c_str(), -1, TRUE) == CSTR_EQUAL ||
            CompareStringOrdinal(m.szModule, -1, want_name.c_str(), -1, TRUE) == CSTR_EQUAL;
  }
  CloseHandle(snap);
  return found;
}

namespace {

struct WindowSearch {
  DWORD pid = 0;
  HWND found = nullptr;
  std::size_t count = 0;
  bool close = false;
};

BOOL CALLBACK each_window(HWND w, LPARAM p) noexcept {
  return proto::ffi_guard(proto::Component::Launcher, FALSE, [&]() -> BOOL {
    auto* s = reinterpret_cast<WindowSearch*>(p);  // our own search, passed through EnumWindows
    DWORD owner = 0;
    GetWindowThreadProcessId(w, &owner);
    if (owner != s->pid || GetWindow(w, GW_OWNER) != nullptr) return TRUE;
    if (s->close) {
      PostMessageW(w, WM_CLOSE, 0, 0);
      ++s->count;
      return TRUE;
    }
    if (IsWindowVisible(w)) {
      s->found = w;
      return FALSE;
    }
    return TRUE;
  });
}

}  // namespace

HWND main_window(DWORD pid) {
  WindowSearch s;
  s.pid = pid;
  EnumWindows(each_window, reinterpret_cast<LPARAM>(&s));
  return s.found;
}

std::size_t close_windows(DWORD pid) {
  WindowSearch s;
  s.pid = pid;
  s.close = true;
  EnumWindows(each_window, reinterpret_cast<LPARAM>(&s));
  return s.count;
}

std::optional<std::uint32_t> remote_connections(DWORD pid) {
  std::uint32_t count = 0;
  bool any_table = false;
  for (const ULONG family : {static_cast<ULONG>(AF_INET), static_cast<ULONG>(AF_INET6)}) {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, family, TCP_TABLE_OWNER_PID_CONNECTIONS, 0);
    if (size == 0) continue;
    std::vector<unsigned char> buf(size);
    if (GetExtendedTcpTable(buf.data(), &size, FALSE, family, TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != NO_ERROR) continue;
    any_table = true;
    if (family == AF_INET) {
      const auto* t = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buf.data());  // the table the call wrote
      for (DWORD i = 0; i < t->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID& r = t->table[i];
        const unsigned first = r.dwRemoteAddr & 0xFFu;  // network order: the first octet in the low byte
        if (r.dwOwningPid == pid && r.dwRemoteAddr != 0 && first != 127) ++count;
      }
    } else {
      const auto* t = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(buf.data());
      for (DWORD i = 0; i < t->dwNumEntries; ++i) {
        const MIB_TCP6ROW_OWNER_PID& r = t->table[i];
        static constexpr UCHAR kLoopback[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        static constexpr UCHAR kAny[16] = {};
        if (r.dwOwningPid == pid && std::memcmp(r.ucRemoteAddr, kLoopback, 16) != 0 &&
            std::memcmp(r.ucRemoteAddr, kAny, 16) != 0) {
          ++count;
        }
      }
    }
  }
  if (!any_table) return std::nullopt;
  return count;
}

std::vector<std::string> files_in(const fs::path& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec)) out.push_back(narrow(it->path().filename().wstring()));
  }
  return out;
}

}  // namespace en::launcher::win
