// The fake game's entry: the command line, the status lines, the window, the boot order (ARCH §1.4 and §3.3 as a game
// shows them to EldenNet: a window, then global_hinstance, then the statics), and the frame loop, which runs every
// registered task at a set rate, with hitches, stalls, moves, loading screens, faults and DLL loads on request.
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "program.hpp"  // windows.h first, then the headers that need it

#include <mmsystem.h>
#include <shellapi.h>

namespace fakegame {
namespace {

Options g_options;
DWORD g_game_thread = 0;
volatile bool g_quit = false;

double g_started = 0.0;
HANDLE g_stdout = nullptr;
HANDLE g_file = INVALID_HANDLE_VALUE;
SRWLOCK g_status_lock = SRWLOCK_INIT;

#if defined(_MSC_VER)
constexpr const char* kToolchain = "clang-cl";
#else
constexpr const char* kToolchain = "mingw";
#endif

double now_s() {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return static_cast<double>(c.QuadPart) / static_cast<double>(f.QuadPart);
}

std::string narrow(const std::wstring& w) {
  std::string out;
  for (wchar_t c : w) out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
  return out;
}

const wchar_t* base_name(const std::wstring& path) {
  const std::size_t cut = path.find_last_of(L"\\/");
  return cut == std::wstring::npos ? path.c_str() : path.c_str() + cut + 1;
}

const char* moment_name(LoadMoment m) {
  switch (m) {
    case LoadMoment::Boot: return "boot";
    case LoadMoment::Window: return "window";
    case LoadMoment::Ready: return "ready";
    case LoadMoment::Title: return "title";
    case LoadMoment::World: return "world";
  }
  return "?";
}

// --load: the stand-in for a loader that loads a DLL into the game at one moment (ARCH §3.3's late load).
void load_dlls(LoadMoment moment) {
  for (DllLoad& load : g_options.loads) {
    if (load.done || load.moment != moment) continue;
    load.done = true;
    HMODULE module = LoadLibraryW(load.path.c_str());
    status("load moment=%s dll=%ls ok=%d error=%lu", moment_name(moment), base_name(load.path), module != nullptr ? 1 : 0,
           module != nullptr ? 0UL : GetLastError());
  }
}

// The keys and characters that reach the game's own window procedure, past GraceUI's filter (its holder's, its
// hotkeys' and its menu's never do): the conversation's cases count them for input leaks (GraceUI invariants 7, 8).
std::atomic<std::uint32_t> g_game_keys{0};
std::atomic<std::uint32_t> g_game_chars{0};

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) g_game_keys.fetch_add(1, std::memory_order_relaxed);
  if (message == WM_CHAR) g_game_chars.fetch_add(1, std::memory_order_relaxed);
  if (message == WM_CLOSE) {
    g_quit = true;
    DestroyWindow(window);
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

HWND make_window(HINSTANCE instance) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof wc;
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszClassName = L"FakeGameWindow";
  RegisterClassExW(&wc);
  HWND window = CreateWindowExW(0, wc.lpszClassName, L"Fake Game (EldenNet test fixture)",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 360, nullptr,
                                nullptr, instance, nullptr);
  if (window != nullptr) {
    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
  }
  return window;
}

// --press-pause-at: the player presses and releases Pause, as a key press reaches the system (the launcher's Mark is a
// system hotkey, O90).
void press_pause() {
  INPUT in[2] = {};
  in[0].type = INPUT_KEYBOARD;
  in[0].ki.wVk = VK_PAUSE;
  in[1] = in[0];
  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
  const UINT sent = SendInput(2, in, sizeof(INPUT));
  status("pressed key=pause sent=%u", sent);
}

void pump() {
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

void wait_pumping(double seconds, bool window) {
  const double until = now_s() + seconds;
  while (now_s() < until && !g_quit) {
    if (window) pump();
    Sleep(5);
  }
}

// --press-keys' list, "F:KEY,F:KEY": each key GraceUI's menu reads, with its virtual key and scan code.
bool parse_keys(const std::wstring& list, std::vector<Options::KeyPress>& out) {
  struct Named {
    const wchar_t* name;
    unsigned vk;
    unsigned scan;
  };
  static constexpr Named kKeys[] = {{L"f5", VK_F5, 0x3F},      {L"next", VK_NEXT, 0x51}, {L"prior", VK_PRIOR, 0x49},
                                    {L"up", VK_UP, 0x48},      {L"down", VK_DOWN, 0x50}, {L"left", VK_LEFT, 0x4B},
                                    {L"right", VK_RIGHT, 0x4D}, {L"return", VK_RETURN, 0x1C},
                                    {L"escape", VK_ESCAPE, 0x01}, {L"t", 0x54, 0x14}, {L"v", 0x56, 0x2F},
                                    {L"tab", VK_TAB, 0x0F},        {L"f11", VK_F11, 0x57}};
  std::size_t at = 0;
  while (at < list.size()) {
    const std::size_t end = std::min(list.find(L',', at), list.size());
    const std::wstring item = list.substr(at, end - at);
    const std::size_t colon = item.find(L':');
    if (colon == std::wstring::npos || colon == 0) return false;
    Options::KeyPress k;
    for (std::size_t i = 0; i < colon; ++i) {
      if (item[i] < L'0' || item[i] > L'9') return false;
      k.frame = k.frame * 10 + static_cast<std::uint64_t>(item[i] - L'0');
    }
    const std::wstring name = item.substr(colon + 1);
    for (const Named& n : kKeys) {
      if (name == n.name) {
        k.vk = n.vk;
        k.scan = n.scan;
      }
    }
    if (k.vk == 0) return false;
    out.push_back(k);
    at = end + 1;
  }
  return !out.empty();
}

// --swap-size's "WxH": each from 64, at most 3840 wide and 2160 high.
bool parse_size(const std::wstring& text, std::uint32_t& width, std::uint32_t& height) {
  const std::size_t x = text.find(L'x');
  if (x == std::wstring::npos || x == 0 || x + 1 >= text.size() || text.size() > 9) return false;
  std::uint32_t parts[2] = {0, 0};
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i == x) continue;
    if (text[i] < L'0' || text[i] > L'9') return false;
    std::uint32_t& part = parts[i < x ? 0 : 1];
    part = part * 10 + static_cast<std::uint32_t>(text[i] - L'0');
  }
  if (parts[0] < 64 || parts[0] > 3840 || parts[1] < 64 || parts[1] > 2160) return false;
  width = parts[0];
  height = parts[1];
  return true;
}

// --- Command line -------------------------------------------------------------------------------------------------------
struct Parser {
  int argc;
  wchar_t** argv;
  int i = 1;
  std::wstring name;
  std::wstring inline_value;
  bool has_inline = false;
  std::string& error;

  bool value(std::wstring& out) {
    if (has_inline) {
      out = inline_value;
      return true;
    }
    if (i + 1 < argc) {
      out = argv[++i];
      return true;
    }
    error = "missing value for " + narrow(name);
    return false;
  }
  template <class T>
  bool number(T& out) {
    std::wstring v;
    if (!value(v)) return false;
    wchar_t* end = nullptr;
    const unsigned long long n = std::wcstoull(v.c_str(), &end, 0);
    if (v.empty() || end == nullptr || *end != L'\0' || v[0] == L'-') {
      error = "not a number for " + narrow(name) + ": " + narrow(v);
      return false;
    }
    out = static_cast<T>(n);
    return true;
  }
  // A list of numbers separated by commas: "300,420".
  template <class T>
  bool numbers(std::vector<T>& out) {
    std::wstring v;
    if (!value(v)) return false;
    out.clear();
    std::size_t start = 0;
    for (std::size_t i = 0; i <= v.size(); ++i) {
      if (i < v.size() && v[i] != L',') continue;
      const std::wstring part = v.substr(start, i - start);
      wchar_t* end = nullptr;
      const unsigned long long n = std::wcstoull(part.c_str(), &end, 0);
      if (part.empty() || end == nullptr || *end != L'\0' || part[0] == L'-') {
        error = "not a list of numbers for " + narrow(name) + ": " + narrow(v);
        return false;
      }
      out.push_back(static_cast<T>(n));
      start = i + 1;
    }
    return true;
  }
  template <class E, std::size_t N>
  bool choice(E& out, const wchar_t* const (&names)[N], const E (&values)[N]) {
    std::wstring v;
    if (!value(v)) return false;
    for (std::size_t k = 0; k < N; ++k) {
      if (v == names[k]) {
        out = values[k];
        return true;
      }
    }
    error = "unknown value for " + narrow(name) + ": " + narrow(v);
    return false;
  }
};

}  // namespace

std::uint32_t game_keys() noexcept { return g_game_keys.load(std::memory_order_relaxed); }
std::uint32_t game_chars() noexcept { return g_game_chars.load(std::memory_order_relaxed); }

bool parse_options(int argc, wchar_t** argv, Options& o, std::string& error) {
  Parser p{argc, argv, 1, {}, {}, false, error};
  for (; p.i < argc; ++p.i) {
    std::wstring arg = argv[p.i];
    if (arg.rfind(L"--", 0) != 0) {
      error = "unexpected argument: " + narrow(arg);
      return false;
    }
    const std::size_t eq = arg.find(L'=');
    p.has_inline = eq != std::wstring::npos;
    p.inline_value = p.has_inline ? arg.substr(eq + 1) : std::wstring{};
    p.name = p.has_inline ? arg.substr(0, eq) : arg;
    const std::wstring& n = p.name;
    bool ok = true;
    if (n == L"--frames") ok = p.number(o.frames);
    else if (n == L"--forever") o.forever = true;
    else if (n == L"--fps") ok = p.number(o.fps);
    else if (n == L"--no-window") o.window = false;
    else if (n == L"--boot-delay-ms") ok = p.number(o.boot_delay_ms);
    else if (n == L"--statics-delay-ms") ok = p.number(o.statics_delay_ms);
    else if (n == L"--field-mirror") o.field_mirror = true;
    else if (n == L"--pass-time") o.pass_time = true;
    else if (n == L"--clock-rate") ok = p.number(o.clock_rate);
    else if (n == L"--slot-by-state") o.slot_by_state = true;
    else if (n == L"--first-slot") ok = p.number(o.first_slot);
    else if (n == L"--play-time-s") ok = p.number(o.play_time_s);
    else if (n == L"--slot-copy") o.slot_copy = true;
    else if (n == L"--walk-from-s") ok = p.number(o.walk_from_s);
    else if (n == L"--walk-to-s") ok = p.number(o.walk_to_s);
    else if (n == L"--hit-at-s") ok = p.number(o.hit_at_s);
    else if (n == L"--heal-at-s") ok = p.number(o.heal_at_s);
    else if (n == L"--screen-from-s") ok = p.number(o.screen_from_s);
    else if (n == L"--screen-to-s") ok = p.number(o.screen_to_s);
    else if (n == L"--kill-at-s") ok = p.number(o.kill_at_s);
    else if (n == L"--block-at-s") ok = p.number(o.block_at_s);
    else if (n == L"--die-at-s") ok = p.number(o.die_at_s);
    else if (n == L"--wide-sheet") o.wide_sheet = true;
    else if (n == L"--retitle-at-s") ok = p.number(o.retitle_at_s);
    else if (n == L"--retitle-for-s") ok = p.number(o.retitle_for_s);
    else if (n == L"--day-book") o.day_book = true;
    else if (n == L"--flag-set") o.flag_set = true;
    else if (n == L"--title-frames") ok = p.number(o.title_frames);
    else if (n == L"--load-frames") ok = p.number(o.load_frames);
    else if (n == L"--stay-title") o.stay_title = true;
    else if (n == L"--reload-every") ok = p.number(o.reload_every);
    else if (n == L"--actors") ok = p.number(o.actors);
    else if (n == L"--online") o.online = true;
    else if (n == L"--online-at") ok = p.number(o.online_at);
    else if (n == L"--hitch-every") ok = p.number(o.hitch_every);
    else if (n == L"--hitch-ms") ok = p.number(o.hitch_ms);
    else if (n == L"--stall-at") ok = p.number(o.stall_at);
    else if (n == L"--stall-ms") ok = p.number(o.stall_ms);
    else if (n == L"--move-every") ok = p.number(o.move_every);
    else if (n == L"--move-mode") {
      static const wchar_t* const names[] = {L"poison", L"free", L"keep"};
      static const MoveMode values[] = {MoveMode::Poison, MoveMode::Free, MoveMode::Keep};
      ok = p.choice(o.move_mode, names, values);
    } else if (n == L"--shift-groups") ok = p.number(o.shift_groups);
    else if (n == L"--register") {
      static const wchar_t* const names[] = {L"normal", L"fault", L"ignore"};
      static const RegisterMode values[] = {RegisterMode::Normal, RegisterMode::Fault, RegisterMode::Ignore};
      ok = p.choice(o.register_mode, names, values);
    } else if (n == L"--crash") {
      static const wchar_t* const names[] = {L"av-read",        L"av-write",   L"av-exec",  L"div-zero", L"illegal",
                                             L"stack-overflow", L"breakpoint", L"fastfail", L"terminate"};
      static const Crash values[] = {Crash::AvRead,        Crash::AvWrite,    Crash::AvExec,   Crash::DivZero,
                                     Crash::Illegal,       Crash::StackOverflow, Crash::Breakpoint, Crash::FastFail,
                                     Crash::Terminate};
      ok = p.choice(o.crash, names, values);
    } else if (n == L"--crash-at") ok = p.number(o.crash_at);
    else if (n == L"--crash-thread") {
      static const wchar_t* const names[] = {L"game", L"worker"};
      static const bool values[] = {false, true};
      ok = p.choice(o.crash_on_worker, names, values);
    } else if (n == L"--handled-every") ok = p.number(o.handled_every);
    else if (n == L"--handled-kind") {
      static const wchar_t* const names[] = {L"seh", L"cxx"};
      static const Handled values[] = {Handled::Seh, Handled::Cxx};
      ok = p.choice(o.handled_kind, names, values);
    } else if (n == L"--game-filter") o.game_filter = true;
    else if (n == L"--game-filter-at") ok = p.number(o.game_filter_at);
    else if (n == L"--game-filter-result") {
      static const wchar_t* const names[] = {L"execute", L"continue"};
      static const bool values[] = {false, true};
      ok = p.choice(o.game_filter_continue, names, values);
    } else if (n == L"--load") {
      std::wstring v;
      ok = p.value(v);
      DllLoad load;
      static const wchar_t* const moments[] = {L"boot", L"window", L"ready", L"title", L"world"};
      static const LoadMoment values[] = {LoadMoment::Boot, LoadMoment::Window, LoadMoment::Ready, LoadMoment::Title,
                                          LoadMoment::World};
      load.path = v;
      const std::size_t colon = v.find(L':');
      for (std::size_t k = 0; ok && colon != std::wstring::npos && k < 5; ++k) {
        if (v.compare(0, colon, moments[k]) == 0) {
          load.moment = values[k];
          load.path = v.substr(colon + 1);
        }
      }
      if (ok) o.loads.push_back(load);
    } else if (n == L"--selftest-task") o.selftest_task = true;
    else if (n == L"--selftest-abi") {
      static const wchar_t* const names[] = {L"published", L"swapped"};
      static const bool values[] = {false, true};
      ok = p.choice(o.selftest_swapped, names, values);
    } else if (n == L"--selftest-object-size") ok = p.number(o.selftest_object_size);
    else if (n == L"--selftest-group") ok = p.number(o.selftest_group);
    else if (n == L"--press-pause-at") ok = p.number(o.press_pause_at);
    else if (n == L"--status-file") ok = p.value(o.status_file);
    else if (n == L"--d3d12") o.d3d12 = true;
    else if (n == L"--grace-take-pause") o.grace_take_pause = true;
    else if (n == L"--grace-lan") o.grace_lan = true;
    else if (n == L"--press-f11-at") ok = p.numbers(o.press_f11_at);
    else if (n == L"--press-keys") {
      std::wstring list;
      ok = p.value(list) && parse_keys(list, o.press_keys);
    }
    else if (n == L"--calm") o.calm = true;
    else if (n == L"--type-at") {
      // F:TEXT,F:TEXT: invented words of the tests' own, of letters, digits, spaces and . ? ! ' only.
      std::wstring list;
      ok = p.value(list);
      std::size_t at = 0;
      while (ok && at < list.size()) {
        const std::size_t end = std::min(list.find(L',', at), list.size());
        const std::wstring item = list.substr(at, end - at);
        const std::size_t colon = item.find(L':');
        ok = colon != std::wstring::npos && colon != 0 && colon + 1 < item.size();
        Options::Typed t;
        for (std::size_t i = 0; ok && i < colon; ++i) {
          ok = item[i] >= L'0' && item[i] <= L'9';
          t.frame = t.frame * 10 + static_cast<std::uint64_t>(item[i] - L'0');
        }
        t.text = ok ? item.substr(colon + 1) : std::wstring();
        for (const wchar_t c : t.text) {
          ok = ok && ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L' ' ||
                      c == L'.' || c == L'?' || c == L'!' || c == L'\'');
        }
        if (ok) o.type_at.push_back(std::move(t));
        at = end + 1;
      }
    }
    else if (n == L"--grace-report") ok = p.numbers(o.grace_report_at);
    else if (n == L"--grace-copy-at") ok = p.numbers(o.grace_copy_at);
    else if (n == L"--pad-chord-at") ok = p.numbers(o.pad_chord_at);
    else if (n == L"--lock-on-at") {
      ok = p.numbers(o.lock_on_at);
      if (ok && (o.lock_on_at.size() > 2 || (o.lock_on_at.size() == 2 && o.lock_on_at[1] <= o.lock_on_at[0]))) {
        error = "--lock-on-at is F or F,G with G after F";
        ok = false;
      }
    }
    else if (n == L"--grace-probe") {
      std::vector<std::uint32_t> v;
      ok = p.numbers(v);
      if (ok && v.size() != 7 && v.size() != 8) {
        error = "--grace-probe is x0,y0,x1,y1,r,g,b or x0,y0,x1,y1,r,g,b,tolerance";
        ok = false;
      }
      if (ok && o.grace_probes.size() == Options::kProbes) {
        error = "--grace-probe is given at most three times";
        ok = false;
      }
      if (ok) {
        Options::Probe probe;
        for (int k = 0; k < 4; ++k) probe.rect[k] = v[static_cast<std::size_t>(k)];
        for (int k = 0; k < 3; ++k) probe.colour[k] = v[static_cast<std::size_t>(4 + k)];
        if (v.size() == 8) probe.tolerance = v[7];
        o.grace_probes.push_back(probe);
      }
    }
    else if (n == L"--backdrop") o.backdrop = true;
    else if (n == L"--swap-size") {
      std::wstring v;
      ok = p.value(v) && parse_size(v, o.swap_width, o.swap_height);
      if (!ok && error.empty()) error = "--swap-size is WxH, each from 64 to 3840 wide and 2160 high";
    }
    else if (n == L"--swap-format") {
      ok = p.number(o.swap_format);
      if (ok && o.swap_format != 28 && o.swap_format != 24) {
        error = "--swap-format is 28 (R8G8B8A8_UNORM) or 24 (R10G10B10A2_UNORM)";
        ok = false;
      }
    }
    else if (n == L"--shot-at") ok = p.numbers(o.shot_at);
    else if (n == L"--shot-dir") ok = p.value(o.shot_dir);
    else if (n == L"--cursor-once") o.cursor_once = true;
    else if (n == L"--exit-code") ok = p.number(o.exit_code);
    else if (n == L"--describe") o.describe = true;
    else if (n == L"--verbose") o.verbose = true;
    else {
      error = "unknown option: " + narrow(n);
      return false;
    }
    if (!ok) return false;
  }
  if (o.actors > FG::kMaxNear - 1) {
    error = "--actors is at most 47";
    return false;
  }
  if (o.shift_groups > 8) {
    error = "--shift-groups is at most 8";
    return false;
  }
  if (o.selftest_object_size > 0x1000 || o.selftest_object_size % 8 != 0) {
    error = "--selftest-object-size is a multiple of 8, at most 0x1000";
    return false;
  }
  if (o.handled_every != 0 && !handled_supported(o.handled_kind)) {
    error = "--handled-kind seh needs the clang-cl build";
    return false;
  }
  const bool preview = o.backdrop || o.swap_width != 960 || o.swap_height != 540 || o.swap_format != 28 ||
                       !o.shot_at.empty() || !o.shot_dir.empty();
  if (preview && !o.d3d12) {
    error = "--backdrop, --swap-size, --swap-format, --shot-at and --shot-dir need --d3d12";
    return false;
  }
  if (o.shot_at.empty() != o.shot_dir.empty()) {
    error = "--shot-at and --shot-dir go together";
    return false;
  }
  return true;
}

const Options& options() { return g_options; }
void set_options(const Options& opts) { g_options = opts; }
DWORD game_thread_id() { return g_game_thread; }

void status_open(const std::wstring& file) {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  g_stdout = (out == INVALID_HANDLE_VALUE) ? nullptr : out;
  if (!file.empty()) {
    g_file = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  }
}

// Each line ends with t=<milliseconds since the program started>.
void status(const char* fmt, ...) {
  char line[1536];  // the grace report's line is the longest (present.cpp)
  int n = std::snprintf(line, sizeof line, "FAKEGAME ");
  va_list args;
  va_start(args, fmt);
  const int m = std::vsnprintf(line + n, sizeof line - static_cast<std::size_t>(n) - 40, fmt, args);
  va_end(args);
  n += m < 0 ? 0 : (m > static_cast<int>(sizeof line) - n - 40 ? static_cast<int>(sizeof line) - n - 40 : m);
  n += std::snprintf(line + n, 40, " t=%.1f", (now_s() - g_started) * 1000.0);
  line[n++] = '\n';
  AcquireSRWLockExclusive(&g_status_lock);
  DWORD written = 0;
  if (g_stdout != nullptr) WriteFile(g_stdout, line, static_cast<DWORD>(n), &written, nullptr);
  if (g_file != INVALID_HANDLE_VALUE) WriteFile(g_file, line, static_cast<DWORD>(n), &written, nullptr);
  ReleaseSRWLockExclusive(&g_status_lock);
}

void status_raw(const char* text, std::size_t size) {
  AcquireSRWLockExclusive(&g_status_lock);
  DWORD written = 0;
  if (g_stdout != nullptr) WriteFile(g_stdout, text, static_cast<DWORD>(size), &written, nullptr);
  if (g_file != INVALID_HANDLE_VALUE) WriteFile(g_file, text, static_cast<DWORD>(size), &written, nullptr);
  ReleaseSRWLockExclusive(&g_status_lock);
}

void status_flush() {
  if (g_stdout != nullptr) FlushFileBuffers(g_stdout);
  if (g_file != INVALID_HANDLE_VALUE) FlushFileBuffers(g_file);
}

namespace {

void print_usage() {
  static const char* const kUsage[] = {
      "usage: fakegame [options]   (README.md describes each)",
      "  --frames N  --forever  --fps N  --no-window  --boot-delay-ms N  --statics-delay-ms N  --field-mirror  --pass-time  --flag-set  --clock-rate N  --day-book  --slot-by-state  --slot-copy  --screen-from-s N  --screen-to-s N  --kill-at-s N  --block-at-s N  --die-at-s N  --wide-sheet  --first-slot N  --play-time-s N  --retitle-at-s N  --retitle-for-s N",
      "  --walk-from-s N  --walk-to-s N  --hit-at-s N  --heal-at-s N",
      "  --title-frames N  --load-frames N  --stay-title  --reload-every N  --actors N  --online  --online-at F",
      "  --hitch-every N  --hitch-ms M  --stall-at F  --stall-ms M  --move-every N  --move-mode poison|free|keep",
      "  --shift-groups N  --register normal|fault|ignore",
      "  --crash av-read|av-write|av-exec|div-zero|illegal|stack-overflow|breakpoint|fastfail|terminate",
      "  --crash-at F  --crash-thread game|worker  --handled-every N  --handled-kind seh|cxx",
      "  --game-filter  --game-filter-at F  --game-filter-result execute|continue",
      "  --load [boot|window|ready|title|world:]PATH  --selftest-task  --selftest-abi published|swapped",
      "  --selftest-object-size N  --selftest-group N  --press-pause-at F  --status-file PATH  --exit-code N",
      "  --d3d12  --press-f11-at F,F...  --grace-report F,F...  --grace-probe x0,y0,x1,y1,r,g,b[,tol] (up to 3 times)",
      "  --grace-take-pause  --grace-lan  --press-keys F:f5|next|prior|up|down|left|right|return|escape|t|tab|f11,...",
      "  --calm  --type-at F:TEXT,...  --grace-copy-at F,...  --pad-chord-at F,...  --lock-on-at F[,G]",
      "  --backdrop  --swap-size WxH  --swap-format 28|24  --shot-at F,F...  --shot-dir DIR  --cursor-once",
      "  --describe  --verbose"};
  for (const char* line : kUsage) status("usage %s", line);
}

// The launcher's spike folder for GraceUI's observer (ELDENNET_SPIKE_DIR, S0-E and S4): whether the variable is set
// and whether the folder it names exists, never the path.
void report_spike_dir() {
  wchar_t dir[1024] = {};
  const DWORD n = GetEnvironmentVariableW(L"ELDENNET_SPIKE_DIR", dir, 1024);
  const bool set = n > 0 && n < 1024;
  const DWORD attributes = set ? GetFileAttributesW(dir) : INVALID_FILE_ATTRIBUTES;
  const bool exists = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  status("spike_dir set=%d exists=%d", set ? 1 : 0, exists ? 1 : 0);
}

int run(HINSTANCE instance) {
  const Options& o = g_options;
  timeBeginPeriod(1);
  status("boot pid=%lu toolchain=%s product=fakegame version=3.14.15.0", GetCurrentProcessId(), kToolchain);
  report_spike_dir();
  load_dlls(LoadMoment::Boot);
  if (o.grace_take_pause) grace_take_pause();
  if (o.grace_lan) grace_lan();
  for (const std::uint64_t at : o.grace_copy_at) {
    if (at == 0) grace_copy(0);  // at boot, before the window: before EldenNet asks for GraceUI
  }
  if (o.selftest_task) selftest_start(g_game_thread);
  if (o.game_filter) install_game_filter();
  if (o.boot_delay_ms != 0) Sleep(o.boot_delay_ms);

  HWND window = nullptr;
  if (o.window) {
    window = make_window(instance);
    status("window hwnd=%p visible=%d", static_cast<void*>(window), window != nullptr && IsWindowVisible(window) ? 1 : 0);
    if (window == nullptr) return 3;
  } else {
    status("window none");
  }
  if (o.d3d12 && window != nullptr && !present_start(window)) return 4;
  world_publish_hinstance(instance);
  (void)fg_get_rockbound();  // a singleton asked for before it is set: the idiom's log path runs once (fd4_unset=1)
  load_dlls(LoadMoment::Window);
  if (o.statics_delay_ms != 0) wait_pumping(o.statics_delay_ms / 1000.0, window != nullptr);
  world_create_singletons();
  world_register_tasks();
  status("ready standin=%u groups=%u", standin_group(), group_count());
  load_dlls(LoadMoment::Ready);

  const double period = o.fps == 0 ? 0.0 : 1.0 / o.fps;
  double prev = now_s();
  double max_ms = 0.0;
  double sum_ms = 0.0;
  std::uint32_t hitches = 0;
  std::uint64_t frame = 0;
  bool saw_title = false;
  bool saw_world = false;
  while (!g_quit && (o.forever || frame < o.frames)) {
    ++frame;
    const double start = now_s();
    const double dt_s = start - prev;
    prev = start;
    if (frame > 1) {
      max_ms = dt_s * 1000.0 > max_ms ? dt_s * 1000.0 : max_ms;
      sum_ms += dt_s * 1000.0;
    }
    if (window != nullptr) pump();
    if (o.stall_at == frame) {
      status("stall frame=%llu ms=%u", static_cast<unsigned long long>(frame), o.stall_ms);
      status_flush();
      if (o.stall_ms == 0) {
        for (;;) Sleep(1000);
      }
      Sleep(o.stall_ms);
    }
    if (o.hitch_every != 0 && frame % o.hitch_every == 0) {
      Sleep(o.hitch_ms);
      ++hitches;
    }
    if (o.game_filter_at == frame) install_game_filter();
    if (o.press_pause_at != 0 && frame == o.press_pause_at) {
      if (o.d3d12 && window != nullptr) {
        press_key_into(window, VK_PAUSE, 0x45);  // into the window GraceUI filters (EldenNet's Mark on its hotkey)
      } else {
        press_pause();
      }
    }
    for (const std::uint64_t at : o.press_f11_at) {
      if (at == frame && window != nullptr) press_key_into(window, VK_F11, 0x57);
    }
    for (const Options::KeyPress& k : o.press_keys) {
      if (k.frame == frame && window != nullptr) press_key_into(window, k.vk, k.scan);
    }
    for (const Options::Typed& t : o.type_at) {
      if (t.frame == frame && window != nullptr) type_into(window, t.text);
    }
    for (const std::uint64_t at : o.grace_copy_at) {
      if (at == frame) grace_copy(frame);
    }
    if (!o.pad_chord_at.empty()) pad_chord_frame(frame);
    if (o.crash != Crash::None && frame == o.crash_at) {
      if (o.crash_on_worker) {
        crash_on_worker(o.crash);
      } else {
        crash_now(o.crash);
      }
    }
    if (o.handled_every != 0 && frame % o.handled_every == 0) {
      const bool ok = handled_exception(o.handled_kind);
      if (o.verbose) status("handled frame=%llu ok=%d", static_cast<unsigned long long>(frame), ok ? 1 : 0);
    }
    world_frame_begin(frame);
    if (!saw_title && world_phase() == Phase::Title) {
      saw_title = true;
      load_dlls(LoadMoment::Title);
    }
    if (!saw_world && world_phase() == Phase::World) {
      saw_world = true;
      load_dlls(LoadMoment::World);
    }
    hub_run_frame(frame, static_cast<float>(dt_s > 0.25 ? 0.25 : dt_s));
    if (o.d3d12) present_frame();
    for (const std::uint64_t at : o.grace_report_at) {
      if (at == frame) grace_report(frame);
    }
    for (const std::uint64_t at : o.shot_at) {
      if (at == frame) shot(frame);
    }
    if (period > 0.0) {
      const double until = start + period;
      for (double t = now_s(); t < until; t = now_s()) {
        const double left_ms = (until - t) * 1000.0;
        Sleep(left_ms > 2.0 ? static_cast<DWORD>(left_ms - 1.0) : 0);
      }
    }
  }

  const HubStats hs = hub_stats();
  const WorldStats ws = world_stats();
  if (o.selftest_task) {
    const SelftestStats st = selftest_stats();
    status("selftest registered=%d faulted=%d code=0x%08X attach=%llu runs=%llu queries=%llu wrong_thread=%llu "
           "wrong_group=%llu body_ok=%d",
           st.registered ? 1 : 0, st.faulted ? 1 : 0, st.fault_code, static_cast<unsigned long long>(st.attach),
           static_cast<unsigned long long>(st.runs), static_cast<unsigned long long>(st.queries),
           static_cast<unsigned long long>(st.wrong_thread), static_cast<unsigned long long>(st.wrong_group),
           st.body_ok ? 1 : 0);
  }
  status("end frames=%llu phase=%s builtin_runs=%llu foreign_tasks=%u foreign_runs=%llu foreign_us_max=%llu "
         "live=%llu rejected=%u ignored=%u moves=%u loads=%u poisoned=%u freed=%u hitches=%u handled=%u fd4_unset=%u "
         "frame_ms_max=%.1f frame_ms_mean=%.2f exit=%u",
         static_cast<unsigned long long>(frame), phase_name(world_phase()),
         static_cast<unsigned long long>(hs.builtin_runs), hs.foreign_tasks,
         static_cast<unsigned long long>(hs.foreign_runs), static_cast<unsigned long long>(hs.foreign_us_max),
         static_cast<unsigned long long>(hs.live), hs.rejected, hs.ignored, ws.moves, ws.loads, ws.poisoned, ws.freed, hitches, handled_count(), ws.fd4_unset,
         max_ms,
         frame > 1 ? sum_ms / static_cast<double>(frame - 1) : 0.0, o.exit_code);
  status_flush();
  timeEndPeriod(1);
  return static_cast<int>(o.exit_code);
}

}  // namespace
}  // namespace fakegame

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
  using namespace fakegame;
  g_started = now_s();
  g_game_thread = GetCurrentThreadId();
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  Options opts;
  std::string error;
  const bool parsed = argv != nullptr && parse_options(argc, argv, opts, error);
  if (argv != nullptr) LocalFree(argv);
  if (!parsed) {
    set_options(opts);
    status_open(opts.status_file);
    if (error == "unknown option: --help") {
      print_usage();
      return 0;
    }
    status("error %s", error.c_str());
    status_flush();
    return 2;
  }
  set_options(opts);
  status_open(opts.status_file);
  if (opts.describe) return describe(instance);
  return run(instance);
}
