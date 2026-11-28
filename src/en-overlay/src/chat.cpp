// The conversation's surfaces through GraceUI (chat.hpp), Windows only.
#include "en/overlay/chat.hpp"

#include <algorithm>
#include <cstring>

#include "graceui_imgui.hpp"
#include "imgui.h"
#include "imgui_internal.h"  // the input line's state (its caret and selection), and its focus

namespace en::overlay {

namespace {

constexpr float kEditorWidth = 2048.0f;  // the editor's own window: wide enough that nothing scrolls it
constexpr float kEditorHeight = 64.0f;

// NPC §8.5's characters (code points, not bytes): a line past them loses what was typed last, before the caret.
int on_edit(ImGuiInputTextCallbackData* d) {
  const std::size_t max = *static_cast<const std::size_t*>(d->UserData);
  std::size_t n = surfaces::chars(std::string_view(d->Buf, static_cast<std::size_t>(d->BufTextLen)));
  while (n > max && d->CursorPos > 0) {
    int at = d->CursorPos - 1;
    while (at > 0 && (static_cast<unsigned char>(d->Buf[at]) & 0xC0) == 0x80) --at;
    d->DeleteChars(at, d->CursorPos - at);
    --n;
  }
  return 0;
}

const char* not_now_word(std::uint32_t why) noexcept {
  switch (why) {
    case GRACEUI_NOT_NOW_NOT_READY: return "not_ready";
    case GRACEUI_NOT_NOW_DANGER: return "danger";
    case GRACEUI_NOT_NOW_LOADING: return "loading";
    case GRACEUI_NOT_NOW_GAME_MENU: return "game_menu";
    default: return "not_now";
  }
}

proto::UiEvent close_event(proto::CloseReason reason, std::string draft, std::string detail = {}) {
  return proto::UiEvent{proto::UiEvent::Close{reason, std::move(draft), std::move(detail)}};
}

}  // namespace

// ---- The editor ----

Editor::Editor() {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ctx_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctx_);
  ImGuiIO& io = ImGui::GetIO();
  graceui::imgui::prepare(io);
  io.ConfigInputTrickleEventQueue = false;  // one event a frame already
  io.Fonts->AddFontDefaultBitmap();
  // Baked here and never registered with GraceUI: the editor is never drawn (the box draws from GraceUI's glyphs).
  ImFontAtlasBuildMain(io.Fonts);
  if (ImTextureData* atlas = io.Fonts->TexData) {
    atlas->SetTexID(static_cast<ImTextureID>(1));
    atlas->SetStatus(ImTextureStatus_OK);
  }
  ImGui::SetCurrentContext(previous);
}

Editor::~Editor() {
  if (ctx_ != nullptr) ImGui::DestroyContext(ctx_);
}

void Editor::frame(std::uint64_t now) {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGui::SetCurrentContext(ctx_);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(kEditorWidth, kEditorHeight);
  // Real time between frames, so that Dear ImGui's own key repeat follows a held key as its Win32 backend's would.
  const std::uint64_t since = last_ != 0 && now > last_ ? now - last_ : 16;
  io.DeltaTime = std::clamp(static_cast<float>(since) / 1000.0f, 0.001f, 0.5f);
  last_ = now;
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(kEditorWidth, kEditorHeight));
  ImGui::Begin("eldennet-chat-line", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollWithMouse);
  if (focus_) ImGui::SetKeyboardFocusHere();
  ImGui::SetNextItemWidth(kEditorWidth - 32.0f);
  (void)ImGui::InputText("##line", buf_.data(), max_bytes_ + 1, ImGuiInputTextFlags_CallbackEdit, &on_edit,
                         &max_chars_);
  id_ = ImGui::GetItemID();
  ImGui::End();
  ImGui::EndFrame();
  const int before[3] = {cursor_, select_start_, select_end_};
  if (ImGuiInputTextState* st = ImGui::GetInputTextState(id_)) {
    focus_ = false;
    if (to_end_) {
      // Activated by focus, Dear ImGui selects the whole line: a kept draft is to be typed on, not replaced.
      to_end_ = false;
      st->SetSelection(st->TextLen, st->TextLen);
    }
    cursor_ = st->GetCursorPos();
    select_start_ = st->GetSelectionStart();
    select_end_ = st->GetSelectionEnd();
  }
  if (before[0] != cursor_ || before[1] != select_start_ || before[2] != select_end_) ++edits_;
  ImGui::SetCurrentContext(previous);
}

void Editor::set(std::string_view text, std::size_t max_chars, std::size_t max_bytes) {
  max_bytes_ = std::clamp<std::size_t>(max_bytes, 1, proto::kLineMaxBytes);
  max_chars_ = std::max<std::size_t>(max_chars, 1);
  // The draft within the limits, cut at a character's start.
  std::size_t n = std::min(text.size(), max_bytes_);
  while (n > 0 && n < text.size() && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80) --n;
  std::string_view kept = text.substr(0, n);
  while (surfaces::chars(kept) > max_chars_) {
    std::size_t at = kept.size() - 1;
    while (at > 0 && (static_cast<unsigned char>(kept[at]) & 0xC0) == 0x80) --at;
    kept = kept.substr(0, at);
  }
  std::memcpy(buf_.data(), kept.data(), kept.size());
  buf_[kept.size()] = '\0';
  cursor_ = select_start_ = select_end_ = static_cast<int>(kept.size());
  ++edits_;
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGui::SetCurrentContext(ctx_);
  ImGuiInputTextState* st = ImGui::GetInputTextState(id_);
  if (st != nullptr) st->ReloadUserBufAndMoveToEnd();
  ImGui::SetCurrentContext(previous);
  focus_ = st == nullptr;
  to_end_ = st == nullptr;
  const std::uint64_t now = last_ + 16;
  for (int i = 0; i < 4 && (focus_ || i == 0); ++i) frame(now);
}

void Editor::feed(const GraceUiEvent& e, std::uint64_t now) {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGui::SetCurrentContext(ctx_);
  if (ImGui::GetInputTextState(id_) == nullptr) {
    focus_ = true;  // the field lost its focus (it never should): taken again before the event, the caret kept
  }
  (void)graceui::imgui::feed(ImGui::GetIO(), e);
  ImGui::SetCurrentContext(previous);
  const std::string before(buf_.data());
  frame(now);
  if (before != buf_.data()) ++edits_;
}

std::string Editor::text() const { return std::string(buf_.data()); }

surfaces::EditView Editor::view() const noexcept {
  surfaces::EditView v;
  v.text = std::string_view(buf_.data());
  v.cursor = static_cast<std::size_t>(std::max(0, cursor_));
  v.select_start = static_cast<std::size_t>(std::max(0, select_start_));
  v.select_end = static_cast<std::size_t>(std::max(0, select_end_));
  return v;
}

// ---- The surfaces ----

Surfaces::Surfaces(const graceui::Api& api, GraceUiConsumer me) : api_(api), me_(me) {}

Surfaces::~Surfaces() = default;

std::unique_ptr<Surfaces> Surfaces::make(const graceui::Api& api, GraceUiConsumer me, const Fonts& fonts,
                                         const GraceUiConnectAnswer& answer, SurfacesReport& report) {
  report = SurfacesReport{};
  for (const char* cap : {GRACEUI_CAP_PANELS, GRACEUI_CAP_TEXTURES, GRACEUI_CAP_HOTKEYS, GRACEUI_CAP_INPUT,
                          GRACEUI_CAP_HOLDER_EVENTS, GRACEUI_CAP_DISPLAY, GRACEUI_CAP_FACES}) {
    if (!api.has_cap(cap)) {
      report.state = "no_caps";
      return nullptr;
    }
  }
  std::unique_ptr<Surfaces> s(new Surfaces(api, me));
  // A white texture of EldenNet's own for the solid shapes, in its registration window (before any submit).
  static constexpr std::array<std::uint32_t, 16> kWhite = {
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
  GraceUiTextureDesc white{};
  white.size = sizeof white;
  white.format = GRACEUI_TEXTURE_RGBA8;
  white.width = 4;
  white.height = 4;
  white.row_bytes = 16;
  white.pixels = kWhite.data();
  if (api.register_texture(me, white, s->white_) != GRACEUI_OK || s->white_ == 0) {
    report.state = "texture";
    return nullptr;
  }
  // EldenNet's faces: GraceUI's own fonts, scaled with the display, before warm-up (§33 item 2), of the type GraceUI's
  // Plain text asks for at connect (O134, D268; an older GraceUI, without the cap, leaves plain_text as graceui-sys
  // zeroed it). Only that type's files are read and registered: a change of Plain text in the session waits for the
  // next launch. After warm-up they answer too_late, and the surfaces draw GraceUI's notices' face (face 1) instead.
  s->plain_cap_ = api.has_cap(GRACEUI_CAP_PLAIN_TEXT);
  s->type_ = surfaces::type_at_connect(s->plain_cap_, answer.plain_text);
  s->plain_now_ = s->type_ == surfaces::Type::Plain;
  report.plain_cap = s->plain_cap_;
  report.type = s->type_;
  std::string_view last_file;
  std::string bytes;  // the last file read: Plain text's Noto Sans Regular serves the text and the ui
  for (const surfaces::FaceFile& f : surfaces::faces_of(s->type_)) {
    if (f.file != last_file) {
      bytes = fonts ? fonts(f.file) : std::string();
      last_file = f.file;
      if (!bytes.empty()) ++report.files;
    }
    std::uint32_t& id = f.role == surfaces::Role::Text     ? s->text_face_
                        : f.role == surfaces::Role::Ui     ? s->ui_face_
                        : f.role == surfaces::Role::Italic ? s->italic_face_
                                                           : s->name_face_;
    if (bytes.empty()) continue;
    GraceUiFaceDesc d{};
    d.size = sizeof d;
    d.pixel_height = static_cast<std::uint32_t>(f.px);
    d.data = bytes.data();
    d.data_length = bytes.size();
    d.flags = GRACEUI_FACE_SCALED;
    report.face_status = api.register_face(me, d, id);
    if (report.face_status == GRACEUI_OK && id != 0) {
      ++report.faces;
    } else {
      id = 0;
    }
  }
  static_assert(GRACEUI_FACES_ELDENNET >= 3, "the surfaces' three faces are within what GraceUI gives EldenNet (D139)");
  report.italic = s->italic_face_ != 0;
  const auto file_of = [&](surfaces::Role role) -> std::string_view {
    for (const surfaces::FaceFile& f : surfaces::faces_of(s->type_)) {
      if (f.role == role) return surfaces::face_name(f.file);
    }
    return {};
  };
  using surfaces::Role;
  report.text = s->text_face_ != 0 ? file_of(Role::Text) : std::string_view("graceui");
  report.ui = s->ui_face_ != 0 ? file_of(Role::Ui) : std::string_view("graceui");
  report.name = s->name_face_ != 0 ? file_of(Role::Name) : report.text;
  report.tarnished = s->italic_face_ != 0 ? file_of(Role::Italic) : report.text;
  // The hotkeys (§48, §49): stable action ids, players' names; the chord with the talk key where GraceUI has chords.
  const GraceUiApi1* t = api.table();
  GraceUiHotkeyDesc talk{};
  talk.size = sizeof talk;
  talk.key = kKeyTalk;
  talk.pad_chord = api.has_cap(GRACEUI_CAP_PAD_CHORDS) ? kTalkChord : 0u;
  talk.action = kActionTalk.data();
  talk.action_length = kActionTalk.size();
  talk.name = kNameTalk.data();
  talk.name_length = kNameTalk.size();
  GraceUiStatus st = GRACEUI_API1_COVERS(t, register_hotkey) ? t->register_hotkey(me, &talk, &s->talk_)
                                                              : GRACEUI_NOT_YET;
  report.chord = talk.pad_chord != 0 ? "asked" : "no_cap";
  if (st != GRACEUI_OK && st != GRACEUI_UNBOUND && talk.pad_chord != 0) {
    talk.pad_chord = 0;  // the key alone, then
    report.chord = "refused";
    st = t->register_hotkey(me, &talk, &s->talk_);
  }
  if ((st != GRACEUI_OK && st != GRACEUI_UNBOUND) || s->talk_ == 0) {
    report.state = "hotkey";
    return nullptr;
  }
  report.talk = st == GRACEUI_OK ? "bound" : "unbound";
  if (const GraceUiStatus k = api.register_hotkey(me, kActionSkip, kNameSkip, kKeySkip, 0, s->skip_);
      k != GRACEUI_OK && k != GRACEUI_UNBOUND) {
    s->skip_ = 0;  // no skip key: the pages hold their time
  }
  // GraceUI's wait handle (ABI 1.5, D212): T6 waits on it, with its 100 ms as the fallback.
  if (api.has_cap(GRACEUI_CAP_WAKE) && GRACEUI_API1_COVERS(t, wake_event)) {
    void* event = nullptr;
    if (t->wake_event(me, &event) == GRACEUI_OK && event != nullptr) s->wake_ = event;
  }
  report.wake = s->wake_ != nullptr;
  s->hotkey_by_ = api.has_cap(GRACEUI_CAP_HOTKEY_BY);  // ABI 1.7 (D243): what pressed a hotkey
  if (answer.display_width != 0 && answer.display_height != 0 && answer.ui_scale_default > 0) {
    s->display_.width = static_cast<float>(answer.display_width);
    s->display_.height = static_cast<float>(answer.display_height);
    s->display_.content_scale = static_cast<float>(answer.display_height) / 1080.0f * answer.ui_scale /
                                answer.ui_scale_default;
  }
  s->describe_talk();
  report.state = "ready";
  return s;
}

void Surfaces::describe_talk() {
  const GraceUiApi1* t = api_.table();
  if (talk_ == 0 || !api_.has_cap(GRACEUI_CAP_HOTKEY_BINDINGS) || !GRACEUI_API1_COVERS(t, describe_hotkey)) return;
  GraceUiHotkeyBinding b{};
  b.size = sizeof b;
  if (t->describe_hotkey(me_, talk_, &b) != GRACEUI_OK) return;
  b.key_text[sizeof b.key_text - 1] = '\0';
  b.chord_text[sizeof b.chord_text - 1] = '\0';
  if ((b.bound & GRACEUI_HOTKEY_KEY_BOUND) != 0) {
    key_words_ = b.key_text;
    talk_vk_ = b.key;
  } else if ((b.bound & GRACEUI_HOTKEY_CHORD_BOUND) != 0) {
    key_words_ = b.chord_text;
    talk_vk_ = 0;
  } else {
    key_words_.clear();  // unbound: the hint names no key
    talk_vk_ = 0;
  }
}

bool Surfaces::take(const GraceUiEvent& e, std::uint64_t now, std::vector<proto::UiEvent>& out) {
  switch (e.kind) {
    case GRACEUI_EVENT_HOTKEY:
      if (e.hotkey_id != 0 && e.hotkey_id == talk_) {
        // What pressed it (ABI 1.7, D243): the chord alone opens by the chord, and arms no swallow (it types nothing);
        // the key, or both, by the key. 0, as an older GraceUI writes, is taken as the key, as before.
        const std::uint32_t by = hotkey_by_ ? e.hotkey_by : 0u;
        const bool chord = (by & GRACEUI_HOTKEY_BY_KEY) == 0 && (by & GRACEUI_HOTKEY_BY_CHORD) != 0;
        pressed_ = now;
        pressed_by_key_ = by == 0 || (by & GRACEUI_HOTKEY_BY_KEY) != 0;
        out.push_back(proto::UiEvent{proto::UiEvent::OpenChat{chord ? proto::OpenVia::PadChord
                                                                    : proto::OpenVia::TalkKey}});
        return true;
      }
      if (e.hotkey_id != 0 && e.hotkey_id == skip_) {
        const auto& page = lower_.current();
        if (page && page->kind == proto::PageKind::Speech) {
          out.push_back(proto::UiEvent{proto::UiEvent::Skip{page->id}});
        }
        return true;
      }
      return false;
    case GRACEUI_EVENT_HOTKEY_CHANGED:
      if (e.hotkey_id != 0 && e.hotkey_id == talk_) describe_talk();
      return e.hotkey_id != 0 && (e.hotkey_id == talk_ || e.hotkey_id == skip_);
    case GRACEUI_EVENT_DISPLAY:
      display_.width = static_cast<float>(e.display_width);
      display_.height = static_cast<float>(e.display_height);
      display_.content_scale = e.content_scale > 0 ? e.content_scale : 1.0f;
      display_.faces_scale = e.faces_scale;
      // Plain text as the player has it now (D268): noted for the log, and no face registered or changed for it; the
      // faces chosen at connect stay until the next launch (O134; GraceUI's invariant 11).
      if (plain_cap_ && (e.plain_text == 1) != plain_now_) {
        plain_now_ = e.plain_text == 1;
        ++plain_changes_;
      }
      return false;  // the overlay's other parts may read it too
    case GRACEUI_EVENT_KEY:
    case GRACEUI_EVENT_CHARACTER:
    case GRACEUI_EVENT_POINTER:
    case GRACEUI_EVENT_PAD:
    case GRACEUI_EVENT_INPUT_ENDED: {
      if (choice_holding_) {
        take_choice(e, out);
        return true;
      }
      if (!holding_) return true;  // after the box's own release: GraceUI's `released` end, or a late event
      const surfaces::InputMap::Mapped m = input_.map(e, now);
      if (m.swallowed) ++counts_.swallowed;
      using A = surfaces::InputMap::Action;
      switch (m.action) {
        case A::None: break;
        case A::Edit: editor_.feed(e, now); break;
        case A::Submit: {
          const std::string line = editor_.text();
          if (!line.empty()) {
            out.push_back(proto::UiEvent{proto::UiEvent::Submit{line}});
            editor_.clear();
          }
          break;
        }
        case A::Escape: release(&out, proto::CloseReason::Escape); break;
        case A::Skip: {
          const auto& page = lower_.current();
          if (page && page->kind == proto::PageKind::Speech) {
            out.push_back(proto::UiEvent{proto::UiEvent::Skip{page->id}});
          }
          break;
        }
        case A::Ended:
          holding_ = false;
          caret_ = {};
          if (m.reason) out.push_back(close_event(*m.reason, editor_.text()));
          break;
      }
      return true;
    }
    default:
      return false;
  }
}

void Surfaces::release(std::vector<proto::UiEvent>* out, std::optional<proto::CloseReason> reason) {
  if (!holding_) return;
  (void)api_.release_input(me_);
  holding_ = false;
  caret_ = {};
  if (out != nullptr && reason) out->push_back(close_event(*reason, editor_.text()));
}

void Surfaces::fault() noexcept {
  // First, then the report (NPC §5.5: ffi_guard releases input first).
  if (holding_ || choice_holding_) (void)api_.release_input(me_);
  holding_ = false;
  choice_holding_ = false;
  faulted_ = true;
}

void Surfaces::release_choice() {
  if (!choice_holding_) return;
  (void)api_.release_input(me_);
  choice_holding_ = false;
}

void Surfaces::take_choice(const GraceUiEvent& e, std::vector<proto::UiEvent>& out) {
  const surfaces::ChoiceInput::Mapped m = surfaces::ChoiceInput::map(e);
  using A = surfaces::ChoiceInput::Action;
  const std::size_t n = last_choice_ ? last_choice_->answers.size() : 0;
  switch (m.action) {
    case A::None: break;
    case A::Previous:
      if (n != 0 && choice_focus_ > 0) {
        --choice_focus_;
        choice_dirty_ = true;
      }
      break;
    case A::Next:
      if (n != 0 && choice_focus_ + 1 < n) {
        ++choice_focus_;
        choice_dirty_ = true;
      }
      break;
    case A::Answer:
      if (last_choice_ && choice_focus_ < n) {
        out.push_back(proto::UiEvent{proto::UiEvent::Choose{last_choice_->id,
                                                            static_cast<proto::ChoiceAnswer>(choice_focus_)}});
      }
      break;
    case A::Ended:
      choice_holding_ = false;
      hide(choice_);
      if (m.reason && last_choice_) {
        out.push_back(proto::UiEvent{proto::UiEvent::ChoiceClosed{last_choice_->id, *m.reason, {}}});
      }
      last_choice_.reset();
      break;
  }
}

void Surfaces::show_choice(const proto::UiModel& model, std::uint64_t now, std::vector<proto::UiEvent>& out) {
  if (!model.choice) {
    release_choice();
    hide(choice_);
    last_choice_.reset();
    choice_asked_ = 0;
    return;
  }
  const proto::ChoicePanel& c = *model.choice;
  if (choice_.id == 0 || (choice_for_ != display_ && !choice_holding_)) {
    Panel made;
    made.desc = surfaces::choice_panel(display_);
    if (api_.create_panel(me_, made.desc, made.id) != GRACEUI_OK) {
      ++counts_.refused;
      if (choice_.id == 0) {
        if (choice_asked_ != c.id) {
          choice_asked_ = c.id;
          out.push_back(proto::UiEvent{proto::UiEvent::ChoiceClosed{c.id, proto::CloseReason::NotNow, "not_ready"}});
        }
        return;
      }
    } else {
      hide(choice_);
      choice_ = std::move(made);
      choice_for_ = display_;
    }
  }
  const bool fresh = !last_choice_ || last_choice_->id != c.id;
  if (fresh) {
    choice_focus_ = 0;  // "Forget what came after" has the focus as it opens
    choice_dirty_ = true;
  }
  if (fresh || choice_dirty_ || *last_choice_ != c || !choice_.shown) {
    choice_.painter.reset(choice_.desc.width, choice_.desc.height, white_, 0.5f, 0.5f);
    surfaces::paint_choice(choice_.painter, faces_, display_, c, choice_focus_);
    if (submit(choice_, now, true)) {
      last_choice_ = c;
      choice_dirty_ = false;
    }
  }
  if (choice_holding_ || choice_asked_ == c.id) return;
  choice_asked_ = c.id;
  if (!choice_.shown) {
    out.push_back(proto::UiEvent{proto::UiEvent::ChoiceClosed{c.id, proto::CloseReason::NotNow, "not_ready"}});
    return;
  }
  // The modal layer's holder, the keyboard and the pad with the holder's events (D176): EldenNet moves its own focus.
  GraceUiInputRequest req{};
  req.size = sizeof req;
  req.kind = GRACEUI_HOLDER_MODAL;
  req.wants = GRACEUI_WANT_KEYBOARD | GRACEUI_WANT_PAD;
  req.panel = choice_.id;
  req.flags = GRACEUI_INPUT_EVENTS;
  GraceUiInputAnswer a{};
  const GraceUiStatus st = api_.request_input(me_, req, a);
  if (st == GRACEUI_OK && a.answer == GRACEUI_INPUT_GRANTED) {
    choice_holding_ = true;
    return;
  }
  hide(choice_);
  last_choice_.reset();
  if (st == GRACEUI_OK && a.answer == GRACEUI_INPUT_REFUSED) {
    a.holder[sizeof a.holder - 1] = '\0';
    out.push_back(
        proto::UiEvent{proto::UiEvent::ChoiceClosed{c.id, proto::CloseReason::Refused, std::string(a.holder)}});
  } else {
    out.push_back(proto::UiEvent{proto::UiEvent::ChoiceClosed{c.id, proto::CloseReason::NotNow,
                                                              st == GRACEUI_OK ? not_now_word(a.why) : "not_now"}});
  }
}

std::optional<Surfaces::Opened> Surfaces::take_opened() noexcept {
  std::optional<Opened> o = opened_;
  opened_.reset();
  return o;
}

bool Surfaces::due(const Panel& p, std::uint64_t now) const noexcept {
  return !p.submitted || now >= p.last_submit + surfaces::kSubmitEveryMs;
}

void Surfaces::hide(Panel& p) {
  if (p.id != 0 && p.shown) (void)api_.hide_panel(me_, p.id);
  p.shown = false;
}

bool Surfaces::submit(Panel& p, std::uint64_t now, bool force) {
  if (!force && !due(p, now)) return false;
  if (p.painter.empty()) {
    hide(p);  // nothing to draw: GraceUI refuses a list with no command
    return false;
  }
  const GraceUiStatus st = api_.submit(me_, p.id, p.painter.view());
  if (st != GRACEUI_OK) {
    ++counts_.refused;
    return false;
  }
  ++counts_.submits;
  p.submitted = true;
  p.last_submit = now;
  if (!p.shown) {
    // Shown once it holds a list, so it never shows empty.
    if (api_.show_panel(me_, p.id) == GRACEUI_OK) {
      p.shown = true;
    } else {
      ++counts_.refused;
    }
  }
  return true;
}

bool Surfaces::panels(std::uint64_t) {
  if (!display_.known()) return false;
  if (page_.id != 0 && display_ == made_for_) return true;
  if (page_.id != 0 && holding_) return true;  // a new display waits until the box gives input back
  // GraceUI's panels cannot move or grow: a new set for a new display, while the mod's room lasts (16 panels).
  Panel page;
  Panel box;
  Panel hint;
  page.desc = surfaces::page_panel(display_);
  box.desc = surfaces::box_panel(display_);
  hint.desc = surfaces::hint_panel(display_);
  if (api_.create_panel(me_, page.desc, page.id) != GRACEUI_OK ||
      api_.create_panel(me_, box.desc, box.id) != GRACEUI_OK ||
      api_.create_panel(me_, hint.desc, hint.id) != GRACEUI_OK) {
    ++counts_.refused;
    return page_.id != 0;  // the old set, if there is one
  }
  hide(page_);
  hide(box_);
  hide(hint_);
  page_ = std::move(page);
  box_ = std::move(box);
  hint_ = std::move(hint);
  made_for_ = display_;
  last_box_.reset();
  last_hint_.reset();
  return true;
}

bool Surfaces::faces() {
  const auto read = [&](std::uint32_t id, surfaces::Face& out) {
    GraceUiGlyphTable t{};
    if (id != 0 && api_.glyph_table(me_, id, t) == GRACEUI_OK && t.glyph_count != 0) {
      out = surfaces::Face::of(t);
      return true;
    }
    if (api_.glyph_table(me_, 1, t) == GRACEUI_OK && t.glyph_count != 0) {  // GraceUI's notices' face
      out = surfaces::Face::of(t);
      return true;
    }
    return false;
  };
  // Read each show: a table is GraceUI's, valid while its generation is current, and a list names the generation.
  const bool text = read(text_face_, faces_.text);
  const bool ui = read(ui_face_, faces_.ui);
  // The italic and the name line's have no stand-in: without its own table each takes the text's (Faces::tarnished,
  // Faces::name_line).
  const auto own = [&](std::uint32_t id) {
    GraceUiGlyphTable t{};
    return id != 0 && api_.glyph_table(me_, id, t) == GRACEUI_OK && t.glyph_count != 0 ? surfaces::Face::of(t)
                                                                                       : surfaces::Face{};
  };
  faces_.italic = own(italic_face_);
  faces_.name = own(name_face_);
  return text && ui && faces_.usable();
}

void Surfaces::caret(const surfaces::Caret& c) {
  const GraceUiApi1* t = api_.table();
  if (!holding_ || !GRACEUI_API1_COVERS(t, set_caret)) return;
  GraceUiCaret in{};
  in.size = sizeof in;
  in.panel = box_.id;
  in.x = c.x;
  in.y = c.y;
  in.height = c.height;
  (void)t->set_caret(me_, &in);
}

std::vector<proto::UiEvent> Surfaces::show(const proto::UiModel& model, std::uint64_t now) {
  std::vector<proto::UiEvent> out;
  if (faulted_) {
    faulted_ = false;
    out.push_back(close_event(proto::CloseReason::Fault, editor_.text()));
  }
  // The toasts: each id posted once, let go when no longer listed (NPC §10.5's policy is the core's).
  for (const surfaces::ToastBook::Call& c : toasts_.update(model.toasts)) {
    const GraceUiStatus st = api_.notice(me_, c.text, c.tone, c.key, 0);
    if (!c.text.empty() && st == GRACEUI_OK) ++counts_.toasts;
  }
  const proto::ChatBox& box = model.chat;
  if (!panels(now) || !faces()) {
    // Nothing can be drawn yet (GraceUI's warm-up, or no display): a box the model opens cannot take input.
    if (box.view == proto::BoxView::Open && !asked_) {
      asked_ = true;
      out.push_back(close_event(proto::CloseReason::NotNow, std::string(), "not_ready"));
    }
    if (box.view != proto::BoxView::Open) asked_ = false;
    if (model.choice && choice_asked_ != model.choice->id) {
      choice_asked_ = model.choice->id;
      out.push_back(proto::UiEvent{proto::UiEvent::ChoiceClosed{model.choice->id, proto::CloseReason::NotNow,
                                                                "not_ready"}});
    }
    return out;
  }

  // ---- The rollback choice (MEM §9.1): its panel on the modal layer while the model holds it.
  show_choice(model, now, out);

  // ---- The lower third.
  std::optional<proto::PageView> lower;
  if (model.page && model.page->surface == proto::Surface::LowerThird) lower = model.page;
  lower_.update(lower, now);
  if (lower_.empty(now)) {
    hide(page_);
    (void)lower_.submitted(now);  // the page that left is let go
  } else if ((lower_.moving(now) || !page_.shown) && due(page_, now)) {
    page_.painter.reset(page_.desc.width, page_.desc.height, white_, 0.5f, 0.5f);
    for (const surfaces::LowerThird::Drawn& d : lower_.frame(now)) {
      surfaces::paint_page(page_.painter, faces_, display_, d.page, d.alpha);
    }
    if (submit(page_, now, false)) {
      if (const auto shown = lower_.submitted(now)) {
        out.push_back(proto::UiEvent{*shown});
        ++counts_.shown;
      }
    }
  }

  // ---- The box.
  const proto::PageView* strip_page =
      model.page && model.page->surface == proto::Surface::HistoryStrip ? &*model.page : nullptr;
  strip_.update(strip_page != nullptr ? std::optional<proto::PageView>(*strip_page) : std::nullopt, now);
  if (box.view == proto::BoxView::Closed) {
    release(&out, proto::CloseReason::Shut);  // the model closed it (danger, an ending): its draft goes back
    asked_ = false;
    hide(box_);
    last_box_.reset();
  } else {
    bool opening = false;
    if (box.view == proto::BoxView::Open) {
      opening = !holding_ && !asked_;
      if (holding_ && box.conv != box_conv_) {
        editor_.set(box.draft, box.max_chars, box.max_bytes);  // another conversation in the same box
        box_conv_ = box.conv;
      }
    } else {
      release(&out, proto::CloseReason::Shut);  // the strip alone holds no input
      asked_ = false;
    }
    if (opening) {
      editor_.set(box.draft, box.max_chars, box.max_bytes);
      box_conv_ = box.conv;
      scroll_ = 0;
      // The talk key's own character is swallowed if it follows the open (NPC §5.1); the chord types nothing.
      input_.open(now, pressed_ != 0 && now <= pressed_ + 1000 && pressed_by_key_ ? talk_vk_ : 0u);
    }
    const auto strip_frame = strip_.frame(now);
    const bool changed = opening || !last_box_ || *last_box_ != box || editor_.edits() != box_edits_ ||
                         strip_.moving(now) || !box_.shown;
    if (changed && (opening || due(box_, now))) {
      box_.painter.reset(box_.desc.width, box_.desc.height, white_, 0.5f, 0.5f);
      const surfaces::EditView edit = box.view == proto::BoxView::Open ? editor_.view() : surfaces::EditView{};
      const float strip_alpha = strip_frame.empty() ? 0.0f : strip_frame.back().alpha;
      caret_ = surfaces::paint_box(box_.painter, faces_, display_, box, edit, strip_page, strip_alpha, scroll_);
      if (submit(box_, now, opening)) {
        last_box_ = box;
        box_edits_ = editor_.edits();
        if (strip_page != nullptr) {
          if (const auto shown = strip_.submitted(now)) {
            out.push_back(proto::UiEvent{*shown});
            ++counts_.shown;
          }
        }
        caret(caret_);
      }
    }
    if (opening) {
      asked_ = true;
      if (!box_.shown) {
        out.push_back(close_event(proto::CloseReason::NotNow, std::string(), "not_ready"));
      } else {
        // The box's own panel, shown, takes the keyboard and the pad, with the holder's events (D176). Not the mouse
        // (O113, the owner's direction of 2026-11-23): no pointer shows, and the mouse stays the game's, so the camera
        // still turns while the player types.
        GraceUiInputRequest req{};
        req.size = sizeof req;
        req.kind = GRACEUI_HOLDER_PANEL;
        req.wants = GRACEUI_WANT_KEYBOARD | GRACEUI_WANT_PAD;
        req.panel = box_.id;
        req.flags = GRACEUI_INPUT_EVENTS;
        GraceUiInputAnswer a{};
        const GraceUiStatus st = api_.request_input(me_, req, a);
        if (st == GRACEUI_OK && a.answer == GRACEUI_INPUT_GRANTED) {
          holding_ = true;
          opened_ = Opened{pressed_, box_.last_submit};
          caret(caret_);
        } else {
          hide(box_);
          last_box_.reset();
          if (st == GRACEUI_OK && a.answer == GRACEUI_INPUT_REFUSED) {
            a.holder[sizeof a.holder - 1] = '\0';
            out.push_back(close_event(proto::CloseReason::Refused, std::string(), std::string(a.holder)));
          } else {
            out.push_back(close_event(proto::CloseReason::NotNow, std::string(),
                                      st == GRACEUI_OK ? not_now_word(a.why) : "not_now"));
          }
        }
      }
    }
  }

  // ---- The hint and its marker (not while the box is open).
  if (model.hint && box.view != proto::BoxView::Open) {
    const bool changed = !last_hint_ || *last_hint_ != *model.hint || last_hint_words_ != key_words_ || !hint_.shown;
    if (changed && due(hint_, now)) {
      hint_.painter.reset(hint_.desc.width, hint_.desc.height, white_, 0.5f, 0.5f);
      surfaces::paint_hint(hint_.painter, faces_, display_, *model.hint, key_words_);
      if (submit(hint_, now, false)) {
        last_hint_ = model.hint;
        last_hint_words_ = key_words_;
      }
    }
  } else {
    hide(hint_);
    last_hint_.reset();
  }
  return out;
}

}  // namespace en::overlay
