// The launcher's dialogs (ARCH §4: it has no main window; notices and errors use TaskDialogIndirect), or, with
// --no-dialogs, lines that say what was shown and which answer was taken.
#include "win.hpp"  // windows.h first

#include <commctrl.h>
#include <commdlg.h>

namespace en::launcher::win {

namespace {

bool g_dialogs = true;
bool g_yes = false;

using TaskDialogFn = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);

// comctl32's TaskDialogIndirect, from the system folder; the manifest selects version 6, which has it.
TaskDialogFn task_dialog() {
  HMODULE m = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (m == nullptr) return nullptr;
  // SAFETY: the export's documented signature.
  return reinterpret_cast<TaskDialogFn>(reinterpret_cast<void*>(GetProcAddress(m, "TaskDialogIndirect")));
}

constexpr int kAction = 100;

// One dialog: the title, the text, an optional action button besides Close. Answers whether the action was chosen;
// `verify` (a "Don't show again" box) is filled when given.
bool dialog(const std::string& title, const std::string& text, const std::string& action, bool warning, bool* verify) {
  const std::wstring wtitle = widen(title), wtext = widen(text), waction = widen(action);
  const auto fn = task_dialog();
  if (fn == nullptr) {
    const UINT type = (action.empty() ? MB_OK : MB_OKCANCEL) | (warning ? MB_ICONWARNING : MB_ICONINFORMATION);
    return MessageBoxW(nullptr, wtext.c_str(), L"EldenNet", type) == IDOK && !action.empty();
  }
  TASKDIALOGCONFIG c{};
  c.cbSize = sizeof c;
  c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
  c.pszWindowTitle = L"EldenNet";
  c.pszMainInstruction = wtitle.c_str();
  c.pszContent = wtext.c_str();
  c.pszMainIcon = warning ? TD_WARNING_ICON : TD_INFORMATION_ICON;
  TASKDIALOG_BUTTON button{kAction, waction.c_str()};
  if (!action.empty()) {
    c.pButtons = &button;
    c.cButtons = 1;
  }
  c.dwCommonButtons = TDCBF_CLOSE_BUTTON;
  const std::wstring wverify = L"Don't show again";
  if (verify != nullptr) c.pszVerificationText = wverify.c_str();
  int pressed = 0;
  BOOL checked = FALSE;
  if (FAILED(fn(&c, &pressed, nullptr, &checked))) return false;
  if (verify != nullptr) *verify = checked != FALSE;
  return pressed == kAction;
}

}  // namespace

void set_dialogs(bool enabled, bool answer_yes) {
  g_dialogs = enabled;
  g_yes = answer_yes;
}

bool ask(ErrorCode e, const std::string& text) {
  if (!g_dialogs) {
    say("error code=%s exit=%d asked=1 answer=%s", error_id(e).c_str(), exit_code(e), g_yes ? "yes" : "no");
    return g_yes;
  }
  return dialog(error_id(e), text, error_action(e), true, nullptr);
}

void tell_error(ErrorCode e, const std::string& text) {
  if (!g_dialogs) {
    say("error code=%s exit=%d asked=0", error_id(e).c_str(), exit_code(e));
    return;
  }
  (void)dialog(error_id(e), text, {}, true, nullptr);
}

void tell(const std::string& title, const std::string& text) {
  if (!g_dialogs) return;
  (void)dialog(title, text, {}, false, nullptr);
}

bool show_notices() {
  if (!g_dialogs) {
    say("notices version=%u shown=0", kNoticesVersion);
    return false;
  }
  std::string text;
  for (const Notice& n : notices()) text += n.title + "\n" + n.body + "\n\n";
  bool never_again = false;
  (void)dialog("Before you start", text, {}, false, &never_again);
  return never_again;
}

std::optional<fs::path> pick_game(const std::string& exe_name) {
  if (!g_dialogs) return std::nullopt;
  wchar_t file[MAX_PATH] = {};
  const std::wstring name = widen(exe_name);
  std::wstring filter = name + L'\0' + name + L'\0' + L'\0';
  OPENFILENAMEW o{};
  o.lStructSize = sizeof o;
  o.lpstrFilter = filter.c_str();
  o.lpstrFile = file;
  o.nMaxFile = MAX_PATH;
  o.lpstrTitle = L"Pick the game's executable";
  o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&o)) return std::nullopt;
  return fs::path(file);
}

}  // namespace en::launcher::win
