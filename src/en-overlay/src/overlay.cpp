// en-overlay's connection to GraceUI (overlay.hpp), Windows only.
#include "en/overlay/overlay.hpp"

#include <algorithm>
#include <cstring>

#include "en/proto/graceui_pin.hpp"
#include "imgui.h"

namespace en::overlay {

namespace {

namespace pin = en::proto::graceui_pin;

ImVec4 colour(const std::array<std::uint8_t, 3>& c) {
  return ImVec4(static_cast<float>(c[0]) / 255.0f, static_cast<float>(c[1]) / 255.0f,
                static_cast<float>(c[2]) / 255.0f, 1.0f);
}

std::uint32_t packed(const std::array<std::uint16_t, 3>& v) noexcept {
  return GRACEUI_VERSION_PACK(v[0], v[1], v[2]);
}

}  // namespace

Found find(std::uint32_t timeout_ms) noexcept {
  Found f;
  GraceUiSysFound sys{};
  sys.size = sizeof sys;
  const ULONGLONG start = GetTickCount64();
  // GraceUI's own rule for "not installed" (D164): the look ends at the first that finds nothing answering, no module
  // named graceui.dll and no GraceUI\graceui.dll in the game's folder.
  const GraceUiApi1* table = graceui_sys_find(GRACEUI_SYS_LOAD | GRACEUI_SYS_STOP_IF_ABSENT, timeout_ms, &sys);
  if (table != nullptr) f.api = graceui::Api(table);
  f.how = f.api ? sys.how : 0;
  f.attempts = sys.attempts;
  f.version = f.api ? sys.version : 0;
  f.installed = sys.installed != 0;  // graceui-sys's own word, set on its by-name look too since GraceUI c781a85 (D170)
  f.ms = GetTickCount64() - start;
  return f;
}

const char* to_string(Outcome o) noexcept {
  switch (o) {
    case Outcome::Connected: return "connected";
    case Outcome::NoticesOnly: return "notices_only";
    case Outcome::Absent: return "absent";
    case Outcome::TooOld: return "too_old";
    case Outcome::DuplicateId: return "duplicate_id";
    case Outcome::Inactive: return "inactive";
    case Outcome::Refused: return "refused";
    case Outcome::NotYet: return "not_yet";
  }
  return "absent";
}

Overlay::Overlay(const graceui::Api& api, GraceUiConsumer me, bool notices_only, std::uint32_t version)
    : api_(api), me_(me), notices_only_(notices_only), graceui_version_(version) {}

Overlay::~Overlay() {
  if (imgui_ != nullptr) ImGui::DestroyContext(imgui_);
}

Overlay* Overlay::connect(const graceui::Api& api, const Options& options, Report& report) {
  report = Report{};
  if (!api) return nullptr;
  report.graceui_version = api.version();
  // The caps EldenNet uses (D138): notices for the banner and the badge, world-state as a provider, hotkeys for F11 and
  // the Mark, for the view panels and textures (its atlas), and pages for its space (D169).
  std::string_view caps = "notices world-state hotkeys";
  if (options.view && options.space_views != 0) {
    caps = "notices world-state hotkeys panels textures pages";
  } else if (options.view) {
    caps = "notices world-state hotkeys panels textures";
  } else if (options.space_views != 0) {
    caps = "notices world-state hotkeys pages";
  }
  graceui::Mod mod;
  mod.id = kId;
  mod.name = kName;
  mod.version = packed(options.version);
  mod.floor = packed(pin::kFloor);
  mod.caps = caps;
  GraceUiConnectAnswer answer{};
  report.status = api.connect(mod, answer);
  report.reason = answer.reason;
  if (answer.graceui_version != 0) report.graceui_version = answer.graceui_version;
  report.floor = answer.floor;
  switch (report.status) {
    case GRACEUI_OK: report.outcome = Outcome::Connected; break;
    case GRACEUI_INACTIVE:
      report.outcome = answer.reason == GRACEUI_INACTIVE_ANTI_CHEAT && answer.consumer != 0 ? Outcome::NoticesOnly
                                                                                            : Outcome::Inactive;
      break;
    case GRACEUI_TOO_OLD: report.outcome = Outcome::TooOld; break;
    case GRACEUI_DUPLICATE_ID: report.outcome = Outcome::DuplicateId; break;
    case GRACEUI_NOT_YET: report.outcome = Outcome::NotYet; break;
    default: report.outcome = Outcome::Refused; break;
  }
  if (!usable(report.outcome)) return nullptr;
  auto* o = new Overlay(api, answer.consumer, report.outcome == Outcome::NoticesOnly, report.graceui_version);
  if (o->notices_only_) {
    report.view = options.view ? "no_caps" : "off";
    return o;  // in the notice-only mode nothing but notices works (D88)
  }
  o->caps_world_ = api.has_cap(GRACEUI_CAP_WORLD_STATE);
  o->caps_hotkeys_ = api.has_cap(GRACEUI_CAP_HOTKEYS);
  report.world = o->caps_world_;
  report.hotkeys = o->caps_hotkeys_;
  if (options.view) {
    const char* why = "ready";
    (void)o->make_view(why);
    report.view = why;
  }
  if (options.mark) {
    const GraceUiStatus mark = o->register_mark();  // GRACEUI_UNBOUND: registered, Pause taken first (D164)
    report.mark = o->mark_registered();
    report.mark_bound = mark == GRACEUI_OK;
  }
  // EldenNet's space, once, after connect (§23 item 1): its page data and settings description as the protocol's JSON,
  // at its release's increment (D173). A refusal leaves the rest of the connection as it is.
  if (options.space_views != 0 && api.has_cap(GRACEUI_CAP_PAGES)) {
    report.pages = api.register_pages(o->me_, options.space_views, packed(options.version), space_pages(),
                                      settings_description(), kIncrement);
    o->pages_ = report.pages == GRACEUI_OK;
  }
  return o;
}

// The view (D156): its own Dear ImGui, headless, its atlas registered as a texture before any submit, a hidden panel,
// then F11. A step that fails leaves the view off and the connection as it is.
bool Overlay::make_view(const char*& why) {
  if (!caps_hotkeys_ || !api_.has_cap(GRACEUI_CAP_PANELS) || !api_.has_cap(GRACEUI_CAP_TEXTURES)) {
    why = "no_caps";
    return false;
  }
  imgui_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(imgui_);
  ImGuiIO& io = ImGui::GetIO();
  graceui::imgui::prepare(io);
  io.Fonts->AddFontDefaultBitmap();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.WindowPadding = ImVec2(10.0f, 8.0f);
  style.ItemSpacing = ImVec2(6.0f, 3.0f);
  if (graceui::imgui::register_atlas(api_, me_, io) == 0) {
    why = "atlas";
    return false;
  }
  GraceUiPanelDesc desc{};
  desc.size = sizeof desc;
  desc.layer = GRACEUI_LAYER_PANEL;
  desc.anchor = GRACEUI_ANCHOR_TOP_LEFT;
  desc.x = kViewX;
  desc.y = kViewY;
  desc.width = kViewWidth;
  desc.height = kViewHeight;
  std::uint64_t panel = 0;
  if (api_.create_panel(me_, desc, panel) != GRACEUI_OK) {
    why = "panel";
    return false;
  }
  std::uint32_t hotkey = 0;
  const GraceUiStatus status =
      api_.register_hotkey(me_, kActionView, "Open EldenNet's debug view", kKeyF11, 0, hotkey);
  if ((status != GRACEUI_OK && status != GRACEUI_UNBOUND) || hotkey == 0) {
    why = "hotkey";
    return false;
  }
  // Unbound (D164): F11 was taken first, by another mod's hotkey or the menu key. The view is made all the same and
  // keeps its hotkey's id, so a binding the player makes later opens it; until then nothing does.
  panel_ = panel;
  view_hotkey_ = hotkey;
  view_bound_ = status == GRACEUI_OK;
  why = view_bound_ ? "ready" : "unbound";
  return true;
}

GraceUiStatus Overlay::register_mark() noexcept {
  if (mark_hotkey_ != 0) return mark_bound_ ? GRACEUI_OK : GRACEUI_UNBOUND;
  if (notices_only_ || !caps_hotkeys_) return GRACEUI_NOT_YET;
  std::uint32_t id = 0;
  const GraceUiStatus status = api_.register_hotkey(me_, kActionMark, "Mark (development)", kKeyPause, 0, id);
  if ((status == GRACEUI_OK || status == GRACEUI_UNBOUND) && id != 0) {
    mark_hotkey_ = id;
    mark_bound_ = status == GRACEUI_OK;
  }
  return status;
}

GraceUiStatus Overlay::notice(std::uint32_t key, std::string_view text, std::uint32_t tone,
                              std::array<char, GRACEUI_NOTICE_TEXT_MAX>& shown, std::uint32_t& shown_length) noexcept {
  if (lost_) return GRACEUI_NOT_CONNECTED;
  const std::size_t n = std::min<std::size_t>(text.size(), GRACEUI_NOTICE_TEXT_MAX);
  if (n == shown_length && (n == 0 || std::memcmp(text.data(), shown.data(), n) == 0)) return GRACEUI_OK;
  const GraceUiStatus status = api_.notice(me_, text.substr(0, n), tone, key, 0);
  if (status == GRACEUI_OK) {
    std::memcpy(shown.data(), text.data(), n);
    shown_length = static_cast<std::uint32_t>(n);
  } else if (status == GRACEUI_NOT_CONNECTED) {
    lost_ = true;
  }
  return status;
}

GraceUiStatus Overlay::banner(std::string_view text, std::uint32_t tone) noexcept {
  return notice(kBannerKey, text, tone, banner_, banner_length_);
}

GraceUiStatus Overlay::badge(std::string_view text) noexcept {
  return notice(kBadgeKey, text, GRACEUI_TONE_NORMAL, badge_, badge_length_);
}

GraceUiStatus Overlay::world(std::uint32_t flags) noexcept {
  if (lost_ || notices_only_ || !caps_world_) return GRACEUI_NOT_YET;
  if (world_sent_ && flags == world_) return GRACEUI_OK;
  const GraceUiStatus status = api_.set_world_state(me_, flags);
  if (status == GRACEUI_OK) {
    world_ = flags;
    world_sent_ = true;
  } else if (status == GRACEUI_NOT_CONNECTED || status == GRACEUI_INACTIVE) {
    lost_ = true;
  }
  return status;
}

// One frame of the view, in the panel's own pixels (D137, D156): a window filling the panel, the ribbon on its first
// line (PAGES SP8), headings in gold and the rest in ink.
bool Overlay::build(const ViewStatus& status) {
  ImGui::SetCurrentContext(imgui_);
  view_lines(status, lines_);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(kViewWidth, kViewHeight);
  io.DeltaTime = 0.1f;  // T6's frames, about ten a second
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(kViewWidth, kViewHeight));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, colour(kViewGround));
  ImGui::Begin("eldennet-debug-view", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
  const ImVec4 gold(0.86f, 0.74f, 0.45f, 1.0f);
  const ImVec4 ink(0.93f, 0.92f, 0.89f, 1.0f);
  for (std::size_t i = 0; i < lines_.size(); ++i) {
    const ViewLine& l = lines_[i];
    if (i == 0) {
      // The ribbon: a band across the panel behind the first line.
      const ImVec2 at = ImGui::GetCursorScreenPos();
      const float h = ImGui::GetTextLineHeight();
      ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f, at.y - 2.0f), ImVec2(kViewWidth, at.y + h + 2.0f),
                                                ImGui::GetColorU32(colour(kViewRibbon)));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, l.heading ? gold : ink);
    ImGui::TextUnformatted(l.text.data());
    ImGui::PopStyleColor();
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::Render();
  return list_.build(*ImGui::GetDrawData(), kViewWidth, kViewHeight);
}

Overlay::Step Overlay::step(const ViewStatus& status, Space* space, const SpaceFacts* facts, std::uint64_t now_ms) {
  Step out;
  if (lost_) {
    out.lost = true;
    return out;
  }
  if (notices_only_ || !caps_hotkeys_) return out;  // nothing to poll: no hotkey was registered
  GraceUiEvent event{};
  GraceUiStatus polled = GRACEUI_OK;
  bool toggle = false;
  const bool serving = pages_ && space != nullptr && facts != nullptr;
  // With a buffer for a space's event's frame (§23 item 2): GRACEUI_FRAME_FROM_PAGE_MAX bytes always suffice.
  while ((polled = api_.poll(me_, event, frame_.data(), frame_.size())) == GRACEUI_OK &&
         event.kind != GRACEUI_EVENT_NONE) {
    const std::string_view frame(frame_.data(), event.frame_length <= frame_.size() ? event.frame_length : 0);
    if (event.kind == GRACEUI_EVENT_WATCHED || event.kind == GRACEUI_EVENT_UNWATCHED) {
      if (!serving) continue;
      if (event.kind == GRACEUI_EVENT_WATCHED) {
        space->watched(frame);
      } else {
        space->unwatched(frame);
      }
      continue;
    }
    if (event.kind == GRACEUI_EVENT_COMMAND) {
      // Every command answered exactly once, at once (§23 item 4): through the space's one command path, or, with no
      // space to answer it (a defect), `internal`.
      const std::string result =
          serving ? space->command(frame, *facts)
                  : std::string(R"({"v":1,"t":"result","space":"eldennet","id":0,"error":"internal"})");
      const GraceUiStatus answered = api_.answer(me_, event.reply, result);
      ++out.commands;
      if (answered != GRACEUI_OK) out.page_refused = answered;
      continue;
    }
    // Every other kind but a hotkey's is passed over, whatever it is: the display's (GRACEUI_EVENT_DISPLAY, after
    // warm-up and each change, D180), a holder's and any later kind alike. Polling goes on until nothing waits.
    if (event.kind != GRACEUI_EVENT_HOTKEY) continue;
    if (event.hotkey_id != 0 && event.hotkey_id == view_hotkey_) {
      out.view_presses += event.count;
      if ((event.count & 1u) != 0) toggle = !toggle;
    } else if (event.hotkey_id != 0 && event.hotkey_id == mark_hotkey_) {
      out.marks += event.count;
    }
  }
  if (polled == GRACEUI_NOT_CONNECTED || polled == GRACEUI_INACTIVE) {
    lost_ = true;
    out.lost = true;
    return out;
  }
  // What the space has for its watched topics (§23 item 3): nothing for a topic no view watches.
  if (serving) {
    frames_.clear();
    space->collect(*facts, now_ms, frames_);
    for (const std::string& f : frames_) {
      const GraceUiStatus published = api_.publish(me_, f);
      if (published == GRACEUI_OK) {
        ++out.published;
      } else {
        out.page_refused = published;
      }
    }
  }
  if (toggle && view_ready()) {
    if (!open_) {
      // To open: one list submitted first, then the panel shown, so it never shows empty (§18 item 3).
      if (build(status)) {
        const GraceUiStatus s = api_.submit(me_, panel_, list_.view());
        if (s != GRACEUI_OK) out.refused = s;
      }
      const GraceUiStatus shown = api_.show_panel(me_, panel_);
      if (shown == GRACEUI_OK) {
        open_ = true;
      } else {
        out.refused = shown;
      }
    } else {
      const GraceUiStatus hidden = api_.hide_panel(me_, panel_);
      if (hidden != GRACEUI_OK) out.refused = hidden;
      open_ = false;
    }
    return out;
  }
  if (open_ && build(status) && !list_.empty()) {
    const GraceUiStatus s = api_.submit(me_, panel_, list_.view());
    if (s != GRACEUI_OK) out.refused = s;
  }
  return out;
}

}  // namespace en::overlay
