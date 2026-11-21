// Starting the game and loading a DLL into it (win.hpp; ARCH §4.6): CreateProcessW suspended, then VirtualAllocEx,
// WriteProcessMemory and a thread of the game's own on LoadLibraryW with the DLL's absolute path; the module checked
// in the game's module list before the memory is freed. The launcher requests no elevation (its manifest: asInvoker).
#include <algorithm>
#include <cstring>
#include <string>

#include "en/launcher/spikes.hpp"
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

// The game's environment as a block (spikes.hpp's game_environment over the launcher's own): SteamAppId and
// SteamGameId for ARCH §4.5's env method, and ELDENNET_SPIKE_DIR in an observer's run, never left over from the
// launcher's own environment otherwise.
std::wstring environment_block(const GameEnvironment& edit) {
  std::vector<std::wstring> entries;
  LPWCH env = GetEnvironmentStringsW();
  for (const wchar_t* p = env; p != nullptr && *p != L'\0'; p += std::wcslen(p) + 1) entries.emplace_back(p);
  if (env != nullptr) FreeEnvironmentStringsW(env);
  std::wstring block;
  for (const std::wstring& entry : game_environment(entries, edit)) {
    block += entry;
    block.push_back(L'\0');
  }
  block.push_back(L'\0');
  if (block.size() == 1) block.push_back(L'\0');  // an empty block is two NULs
  return block;
}

}  // namespace

Started start_game(const fs::path& exe, const std::vector<std::string>& args, const GameEnvironment& env_edit) {
  Started s;
  std::wstring cmd = quote(exe.wstring());
  for (const std::string& a : args) cmd += L" " + quote(widen(a));
  std::wstring env = environment_block(env_edit);
  const DWORD flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;
  STARTUPINFOW si{};
  si.cb = sizeof si;
  const std::wstring dir = exe.parent_path().wstring();
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, flags, env.data(), dir.c_str(), &si,
                      &s.pi)) {
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
