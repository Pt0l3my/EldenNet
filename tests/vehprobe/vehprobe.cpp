// vehprobe.dll: our own stand-in for a crash reporter that installs a vectored exception handler (DESIGN §16.1's S0-D;
// O91; ARCH §6.5, "Vectored handlers"). It installs the handler when it loads, and its switches come from the
// environment, which a loader passes on to the game:
//
//   VEHPROBE_MODE   pass (the default): note each error-class exception and continue the search, so that frame
//                   handlers (EldenNet's guarded copy) still see it; swallow: end the process at the first one with
//                   VEHPROBE_EXIT, so that no frame handler ever sees it; off: install nothing.
//   VEHPROBE_FIRST  1 (the default): the handler goes first in the list; 0: last.
//   VEHPROBE_EXIT   the exit code `swallow` ends with (default 86).
//   VEHPROBE_LOG    a file the lines are appended to, besides standard output.
//
// It acts on exceptions of the error class (the top two bits of the code set) that are not software codes (bit 29),
// as crash reporters do: an access violation is one; a C++ exception, a debug print or a guard page is not.
// Each line starts with "VEHPROBE ".
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

enum class Mode : unsigned char { Pass, Swallow, Off };

Mode g_mode = Mode::Pass;
bool g_first = true;
UINT g_exit_code = 86;
volatile LONG g_seen = 0;
volatile LONG g_passed = 0;
volatile LONG g_ignored = 0;
PVOID g_handler = nullptr;
HANDLE g_stdout = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;

const char* mode_name(Mode m) {
  switch (m) {
    case Mode::Pass: return "pass";
    case Mode::Swallow: return "swallow";
    case Mode::Off: return "off";
  }
  return "?";
}

void say(const char* fmt, ...) {
  char line[512];
  int n = std::snprintf(line, sizeof line, "VEHPROBE ");
  va_list args;
  va_start(args, fmt);
  const int m = std::vsnprintf(line + n, sizeof line - static_cast<std::size_t>(n) - 2, fmt, args);
  va_end(args);
  n += m < 0 ? 0 : (m > static_cast<int>(sizeof line) - n - 2 ? static_cast<int>(sizeof line) - n - 2 : m);
  line[n++] = '\n';
  DWORD written = 0;
  if (g_stdout != nullptr) WriteFile(g_stdout, line, static_cast<DWORD>(n), &written, nullptr);
  if (g_log != INVALID_HANDLE_VALUE) WriteFile(g_log, line, static_cast<DWORD>(n), &written, nullptr);
}

bool env(const char* name, char* out, DWORD size) {
  const DWORD n = GetEnvironmentVariableA(name, out, size);
  return n > 0 && n < size;
}

bool acts_on(DWORD code) { return (code & 0xC0000000u) == 0xC0000000u && (code & 0x20000000u) == 0; }

LONG CALLBACK handler(EXCEPTION_POINTERS* info) {
  const DWORD code = info->ExceptionRecord->ExceptionCode;
  if (!acts_on(code)) {
    InterlockedIncrement(&g_ignored);
    return EXCEPTION_CONTINUE_SEARCH;
  }
  const LONG n = InterlockedIncrement(&g_seen);
  if (n <= 32) {
    say("saw code=0x%08lX address=%p thread=%lu n=%ld mode=%s", code, info->ExceptionRecord->ExceptionAddress,
        GetCurrentThreadId(), n, mode_name(g_mode));
  }
  if (g_mode == Mode::Swallow) {
    say("swallow code=0x%08lX exit=%u", code, g_exit_code);
    if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log);
    TerminateProcess(GetCurrentProcess(), g_exit_code);
  }
  InterlockedIncrement(&g_passed);
  return EXCEPTION_CONTINUE_SEARCH;
}

void attach() {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  g_stdout = out == INVALID_HANDLE_VALUE ? nullptr : out;
  char value[MAX_PATH];
  if (env("VEHPROBE_LOG", value, sizeof value)) {
    g_log = CreateFileA(value, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
  }
  if (env("VEHPROBE_MODE", value, sizeof value)) {
    if (std::strcmp(value, "swallow") == 0) g_mode = Mode::Swallow;
    else if (std::strcmp(value, "off") == 0) g_mode = Mode::Off;
    else if (std::strcmp(value, "pass") == 0) g_mode = Mode::Pass;
    else say("unknown VEHPROBE_MODE, using pass");
  }
  if (env("VEHPROBE_FIRST", value, sizeof value)) g_first = std::strcmp(value, "0") != 0;
  if (env("VEHPROBE_EXIT", value, sizeof value)) g_exit_code = static_cast<UINT>(std::strtoul(value, nullptr, 0));
  if (g_mode != Mode::Off) g_handler = AddVectoredExceptionHandler(g_first ? 1 : 0, handler);
  say("installed mode=%s first=%d handler=%d", mode_name(g_mode), g_first ? 1 : 0, g_handler != nullptr ? 1 : 0);
}

}  // namespace

// What the handler has seen so far, for a host that wants to ask.
extern "C" __declspec(dllexport) LONG vehprobe_seen() { return g_seen; }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
    attach();
  } else if (reason == DLL_PROCESS_DETACH) {
    say("detach seen=%ld passed=%ld ignored=%ld process_ending=%d", g_seen, g_passed, g_ignored,
        reserved != nullptr ? 1 : 0);
    // Unloaded by FreeLibrary: take the handler away. At the process's end it stays, as the process goes anyway.
    if (reserved == nullptr && g_handler != nullptr) RemoveVectoredExceptionHandler(g_handler);
    if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
  }
  return TRUE;
}
