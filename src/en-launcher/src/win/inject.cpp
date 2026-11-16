// Starting the game and loading a DLL into it (win.hpp; ARCH §4.6): CreateProcessW suspended, then VirtualAllocEx,
// WriteProcessMemory and a thread of the game's own on LoadLibraryW with the DLL's absolute path; the module checked
// in the game's module list before the memory is freed. The launcher requests no elevation (its manifest: asInvoker).
#include <algorithm>
#include <cstring>
#include <string>

#include "en/launcher/texts.hpp"
#include "win.hpp"

namespace en::launcher::win {

namespace {

// One argument quoted as the C runtime's command-line parser reads it back.
std::wstring quote(const std::wstring& arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
  std::wstring out = L"\"";
  std::size_t slashes = 0;
  for (const wchar_t c : arg) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    slashes = 0;
    out.push_back(c);
  }
  out.append(slashes * 2, L'\\');
  out.push_back(L'"');
  return out;
}

// The launcher's environment with SteamAppId and SteamGameId set (ARCH §4.5's env method), as a block.
std::wstring environment_with_appid(std::uint32_t appid) {
  std::wstring block;
  LPWCH env = GetEnvironmentStringsW();
  for (const wchar_t* p = env; p != nullptr && *p != L'\0'; p += std::wcslen(p) + 1) {
    const std::wstring entry(p);
    const auto starts = [&](const wchar_t* name) {
      return CompareStringOrdinal(entry.c_str(), static_cast<int>(std::min(entry.size(), std::wcslen(name))), name, -1,
                                  TRUE) == CSTR_EQUAL;
    };
    if (starts(L"SteamAppId=") || starts(L"SteamGameId=")) continue;
    block += entry;
    block.push_back(L'\0');
  }
  if (env != nullptr) FreeEnvironmentStringsW(env);
  const std::wstring id = std::to_wstring(appid);
  block += L"SteamAppId=" + id;
  block.push_back(L'\0');
  block += L"SteamGameId=" + id;
  block.push_back(L'\0');
  block.push_back(L'\0');
  return block;
}

}  // namespace

Started start_game(const fs::path& exe, const std::vector<std::string>& args, bool env_appid, std::uint32_t appid) {
  Started s;
  std::wstring cmd = quote(exe.wstring());
  for (const std::string& a : args) cmd += L" " + quote(widen(a));
  std::wstring env;
  DWORD flags = CREATE_SUSPENDED;
  if (env_appid) {
    env = environment_with_appid(appid);
    flags |= CREATE_UNICODE_ENVIRONMENT;
  }
  STARTUPINFOW si{};
  si.cb = sizeof si;
  const std::wstring dir = exe.parent_path().wstring();
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, flags, env_appid ? env.data() : nullptr,
                      dir.c_str(), &si, &s.pi)) {
    s.error = GetLastError();
    s.pi = PROCESS_INFORMATION{};
  }
  return s;
}

Injected inject(HANDLE process, DWORD pid, const fs::path& dll) {
  const std::wstring path = fs::absolute(dll).wstring();
  const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (remote == nullptr) return Injected{false, kStepAlloc, GetLastError()};
  SIZE_T written = 0;
  if (!WriteProcessMemory(process, remote, path.c_str(), bytes, &written) || written != bytes) {
    const DWORD e = GetLastError();
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return Injected{false, kStepWrite, e};
  }
  // kernel32 lies at the same address in every process of a session, so our own LoadLibraryW is the game's too.
  // SAFETY: LoadLibraryW takes one pointer and answers a pointer, the shape of a thread's start routine on x64.
  const auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));
  HANDLE thread = CreateRemoteThread(process, nullptr, 0, load, remote, 0, nullptr);
  if (thread == nullptr) {
    const DWORD e = GetLastError();
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return Injected{false, kStepThread, e};
  }
  const DWORD waited = WaitForSingleObject(thread, 30000);
  if (waited != WAIT_OBJECT_0) {
    // The thread may still read the path: the memory stays, and the caller ends the game.
    CloseHandle(thread);
    return Injected{false, kStepWait, waited == WAIT_TIMEOUT ? DWORD{WAIT_TIMEOUT} : GetLastError()};
  }
  DWORD code = 0;
  GetExitCodeThread(thread, &code);
  CloseHandle(thread);
  VirtualFreeEx(process, remote, 0, MEM_RELEASE);
  // The thread's code is the low half of the module's handle, which may be 0 for a module at a 4 GiB boundary: the
  // module list decides, and a 0 only names the step.
  if (!module_loaded(pid, dll)) return Injected{false, code == 0 ? kStepLoad : kStepModule, 0};
  return Injected{true, {}, 0};
}

}  // namespace en::launcher::win
