// The tester's panel (the owner's request, 2026-11-22): a window of the launcher's own that lists a round's tasks in
// order, each with its timer, the done ones greyed. It stays on top of the game's window (windowed or borderless; an
// exclusive full-screen game covers it) and never takes the keyboard: the tester confirms a task with the Pause key
// (the launcher's system hotkey) or by clicking the panel's button. It is the launcher's own process and plain
// Windows drawing: nothing of it is in the game.
#include "win.hpp"

#include <algorithm>

namespace en::launcher::win {

namespace {

constexpr wchar_t kClass[] = L"EldenNetTaskPanel";
constexpr int kWidth = 430;   // at 96 dots an inch
constexpr int kMargin = 12;
constexpr int kGap = 6;
constexpr int kHeader = 30;
// The panel's top below GraceUI's notices at the game's top right (its D134: a 12 px margin, one-line cards of about
// 38 px with a 6 px gap), so that two notices, GraceUI's and EldenNet's banner, stay readable (the owner, 2026-11-23:
// the panel covered GraceUI's notice in run A5; WHY§517). At 96 dots an inch, from the work area's top.
constexpr int kTop = 72;
constexpr int kRight = 16;

constexpr COLORREF kBack = RGB(24, 24, 28);
constexpr COLORREF kNowBack = RGB(44, 56, 84);
constexpr COLORREF kText = RGB(236, 236, 236);
constexpr COLORREF kDim = RGB(168, 168, 172);
constexpr COLORREF kDone = RGB(110, 110, 114);
constexpr COLORREF kAccent = RGB(255, 204, 102);
constexpr COLORREF kButton = RGB(70, 96, 150);

LRESULT CALLBACK panel_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<TaskPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;  // a click never takes the keyboard from the game
    case WM_NCHITTEST: {
      // The header drags the window; the rest is the panel's own (its button).
      POINT p{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
      ScreenToClient(hwnd, &p);
      return self != nullptr && p.y < self->header_height() ? HTCAPTION : HTCLIENT;
    }
    case WM_LBUTTONUP:
      if (self != nullptr) {
        const POINT p{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        if (self->in_button(p.x, p.y)) PostThreadMessageW(GetCurrentThreadId(), kPanelConfirm, 0, 0);
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      const HDC dc = BeginPaint(hwnd, &ps);
      if (self != nullptr) self->paint(dc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

HFONT make_font(int points, int dpi, bool bold) {
  return CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

int text_height(HDC dc, HFONT font, const std::wstring& text, int width) {
  if (text.empty()) return 0;
  const HGDIOBJ old = SelectObject(dc, font);
  RECT r{0, 0, width, 0};
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
  SelectObject(dc, old);
  return r.bottom;
}

void draw_text(HDC dc, HFONT font, COLORREF colour, const std::wstring& text, RECT r, UINT format) {
  if (text.empty()) return;
  const HGDIOBJ old = SelectObject(dc, font);
  SetTextColor(dc, colour);
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, format | DT_NOPREFIX);
  SelectObject(dc, old);
}

void fill(HDC dc, const RECT& r, COLORREF colour) {
  const HBRUSH brush = CreateSolidBrush(colour);
  FillRect(dc, &r, brush);
  DeleteObject(brush);
}

// The row's sign once done: a tick, drawn (a font may lack the sign). `box` is a square.
void draw_tick(HDC dc, const RECT& box, COLORREF colour) {
  const int w = box.right - box.left;
  const HPEN pen = CreatePen(PS_SOLID, std::max(2, w / 6), colour);
  const HGDIOBJ old = SelectObject(dc, pen);
  MoveToEx(dc, box.left + w / 8, box.top + w / 2, nullptr);
  LineTo(dc, box.left + (w * 3) / 8, box.top + (w * 6) / 8);
  LineTo(dc, box.left + (w * 7) / 8, box.top + w / 5);
  SelectObject(dc, old);
  DeleteObject(pen);
}

// The row's sign while under way: an arrow to the right, drawn.
void draw_arrow(HDC dc, const RECT& box, COLORREF colour) {
  const int w = box.right - box.left;
  const POINT points[3] = {{box.left + w / 5, box.top + w / 8},
                           {box.left + (w * 4) / 5, box.top + w / 2},
                           {box.left + w / 5, box.top + (w * 7) / 8}};
  const HBRUSH brush = CreateSolidBrush(colour);
  const HPEN pen = CreatePen(PS_SOLID, 1, colour);
  const HGDIOBJ old_brush = SelectObject(dc, brush);
  const HGDIOBJ old_pen = SelectObject(dc, pen);
  Polygon(dc, points, 3);
  SelectObject(dc, old_pen);
  SelectObject(dc, old_brush);
  DeleteObject(pen);
  DeleteObject(brush);
}

}  // namespace

TaskPanel::~TaskPanel() { close(); }

bool TaskPanel::open(HWND beside) {
  if (hwnd_ != nullptr) return true;
  WNDCLASSW wc{};
  wc.lpfnWndProc = panel_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kClass;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&wc);  // a second registration fails harmlessly
  const HDC screen = GetDC(nullptr);
  dpi_ = screen != nullptr ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
  if (screen != nullptr) ReleaseDC(nullptr, screen);
  if (dpi_ <= 0) dpi_ = 96;
  title_font_ = make_font(10, dpi_, true);
  text_font_ = make_font(9, dpi_, false);
  // The top right corner of the work area of the monitor the game's window is on (the primary one before it shows).
  RECT area{0, 0, 1280, 720};
  MONITORINFO mi{};
  mi.cbSize = sizeof mi;
  const HMONITOR monitor = beside != nullptr ? MonitorFromWindow(beside, MONITOR_DEFAULTTOPRIMARY)
                                             : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  if (monitor != nullptr && GetMonitorInfoW(monitor, &mi)) area = mi.rcWork;
  const int width = MulDiv(kWidth, dpi_, 96);
  hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kClass, L"EldenNet: the round's tasks",
                          WS_POPUP | WS_BORDER, area.right - width - MulDiv(kRight, dpi_, 96),
                          area.top + MulDiv(kTop, dpi_, 96), width, MulDiv(200, dpi_, 96), nullptr, nullptr,
                          GetModuleHandleW(nullptr), nullptr);
  if (hwnd_ == nullptr) return false;
  SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  return true;
}

void TaskPanel::close() {
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
  hwnd_ = nullptr;
  if (title_font_ != nullptr) DeleteObject(title_font_);
  if (text_font_ != nullptr) DeleteObject(text_font_);
  title_font_ = nullptr;
  text_font_ = nullptr;
}

int TaskPanel::header_height() const noexcept { return MulDiv(kHeader, dpi_, 96); }

bool TaskPanel::in_button(int x, int y) const noexcept {
  return button_.right > button_.left && x >= button_.left && x < button_.right && y >= button_.top &&
         y < button_.bottom;
}

void TaskPanel::show(const std::vector<TaskRow>& rows, std::string_view footer, bool key_works) {
  if (hwnd_ == nullptr) return;
  // What the panel says, as one text: it is painted again only when that changes (once a second while a timer runs).
  std::string said(footer);
  said += key_works ? "|k" : "|c";
  for (const TaskRow& r : rows) {
    said += '|';
    said += r.title;
    said += static_cast<char>('0' + static_cast<int>(r.state));
    said += timer_text(r);
    said += button_text(r, key_works);
  }
  if (said == said_) return;
  said_ = std::move(said);
  rows_ = rows;
  footer_ = widen(footer);
  key_works_ = key_works;
  // Its height follows what it holds.
  const HDC dc = GetDC(hwnd_);
  const int height = layout(dc, nullptr);
  ReleaseDC(hwnd_, dc);
  RECT now{};
  GetWindowRect(hwnd_, &now);
  if (now.bottom - now.top != height + 2) {
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, now.right - now.left, height + 2,
                 SWP_NOMOVE | SWP_NOACTIVATE);
  } else {
    // Still on top: another window may have taken the place.
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void TaskPanel::paint(HDC dc) { (void)layout(dc, dc); }

// Lays the panel out and, with `draw`, paints it; answers its height. One routine for both, so that they agree.
int TaskPanel::layout(HDC measure, HDC draw) {
  RECT client{};
  GetClientRect(hwnd_, &client);
  const int margin = MulDiv(kMargin, dpi_, 96);
  const int gap = MulDiv(kGap, dpi_, 96);
  const int timer_width = MulDiv(86, dpi_, 96);
  const int left = margin + MulDiv(18, dpi_, 96);
  const int right = client.right - margin;
  if (draw != nullptr) {
    fill(draw, client, kBack);
    SetBkMode(draw, TRANSPARENT);
  }
  int y = 0;
  {
    const RECT header{margin, 0, right, header_height()};
    if (draw != nullptr) {
      draw_text(draw, title_font_, kAccent, L"EldenNet: the round's tasks", header,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    y = header_height();
  }
  std::size_t number = 0;
  for (const TaskRow& row : rows_) {
    ++number;
    const bool now = row.state == TaskState::Now;
    const bool done = row.state == TaskState::Done;
    const std::wstring title = widen(row.title);
    // A done task shows what the round said of it, where it said anything; a task to come or under way, its detail.
    const std::wstring detail = done ? widen(gathered_text(row)) : widen(row.detail);
    const std::wstring timer = widen(timer_text(row));
    const int title_height = std::max(text_height(measure, title_font_, title, right - left - timer_width),
                                      MulDiv(18, dpi_, 96));
    const int detail_height = text_height(measure, text_font_, detail, right - left);
    const int height = gap + title_height + (detail_height != 0 ? detail_height + gap / 2 : 0) + gap;
    if (draw != nullptr) {
      const RECT band{0, y, client.right, y + height};
      if (now) fill(draw, band, kNowBack);
      const COLORREF colour = done ? kDone : now ? kText : kDim;
      // The row's sign: a number while it is to come, an arrow while it is under way, a tick once done.
      const int side = MulDiv(12, dpi_, 96);
      const int sign_top = y + gap + std::max(0, (MulDiv(18, dpi_, 96) - side) / 2);
      const RECT sign{margin, sign_top, margin + side, sign_top + side};
      if (done) {
        draw_tick(draw, sign, kDone);
      } else if (now) {
        draw_arrow(draw, sign, kAccent);
      } else {
        draw_text(draw, title_font_, colour, std::to_wstring(number),
                  RECT{margin, y + gap, left, y + gap + title_height}, DT_SINGLELINE | DT_LEFT);
      }
      draw_text(draw, title_font_, colour, title, RECT{left, y + gap, right - timer_width, y + gap + title_height},
                DT_WORDBREAK | DT_LEFT);
      draw_text(draw, title_font_, now ? kAccent : colour, timer,
                RECT{right - timer_width, y + gap, right, y + gap + title_height}, DT_SINGLELINE | DT_RIGHT);
      draw_text(draw, text_font_, done && row.gathered == proto::watch::kNotGathered ? kText : colour, detail,
                RECT{left, y + gap + title_height + gap / 2, right, y + height}, DT_WORDBREAK | DT_LEFT);
    }
    y += height;
  }
  // The button, where the task under way is the tester's to confirm, in that task's own words; and the footer.
  std::wstring button;
  for (const TaskRow& r : rows_) {
    if (r.state == TaskState::Now) button = widen(button_text(r, key_works_));
  }
  button_ = RECT{0, 0, 0, 0};
  if (!button.empty()) {
    y += gap;
    const int height = MulDiv(28, dpi_, 96);
    button_ = RECT{margin, y, right, y + height};
    if (draw != nullptr) {
      fill(draw, button_, kButton);
      draw_text(draw, title_font_, kText, button, button_, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    y += height;
  }
  if (!footer_.empty()) {
    y += gap;
    const int height = text_height(measure, text_font_, footer_, right - margin);
    if (draw != nullptr) {
      draw_text(draw, text_font_, kDim, footer_, RECT{margin, y, right, y + height}, DT_WORDBREAK | DT_LEFT);
    }
    y += height;
  }
  return y + margin;
}

}  // namespace en::launcher::win
