// GraceUI through en-overlay (dll.hpp; part 12; ARCH §1.4, §3.2's Connect and ConnectB; GraceUI eldennet-changes §14 to
// §18): T1's connect, then T6's part after each step of the core.
//
// - T1: find GraceUI and connect (`connect_overlay`), one log line either way. A failed connect leaves EldenNet
//   headless at L2 at most, nothing drawn (ARCH §10.3, "Overlay failed to install"); the launcher's hint the next time
//   it runs is its GraceUI pre-flight.
// - T6, once the link is published: the banner (ARCH §5.4, §10.3's texts, en-core's banner_text) and the pause badge
//   as notices; EldenNet's claims as a world-state provider (en-overlay's world.hpp); the Mark's hotkey on Pause when
//   the launcher's console holds Pause or debug.panel is on, and the hand-over of the Marks' ring (watch.hpp's
//   MarkSource); and the F11 view (debug.panel), whose status is filled here from the core, the task and the block.
#include <optional>
#include <string>

#include "dll.hpp"
#include "en/gal/guarded.hpp"
#include "en/overlay/overlay.hpp"
#include "en/overlay/world.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

namespace w = en::proto::watch;
using en::shell::Line;
using en::shell::LogLevel;

struct OverlayLink {
  en::overlay::Overlay* overlay = nullptr;
  // T6's own, from here on.
  bool banner_known = false;
  std::optional<en::core::BannerReason> banner;
  bool badge = false;
  std::uint32_t world = 0xFFFFFFFFu;
  bool mark_asked = false;
  bool marks_ours = false;
  bool lost = false;
  GraceUiStatus refused = GRACEUI_OK;
};

namespace {

// The core's newest changes, for the view (T6 alone): a ring of the last kViewChanges.
constinit std::array<en::overlay::ViewChange, en::overlay::kViewChanges> g_changes{};
constinit std::size_t g_changes_seen = 0;

const char* how_word(std::uint32_t how) noexcept {
  switch (how) {
    case GRACEUI_SYS_BY_NAME: return "name";
    case GRACEUI_SYS_BY_SCAN: return "scan";
    case GRACEUI_SYS_BY_PATH: return "path";
    default: return "none";
  }
}

std::string version_word(std::uint32_t v) {
  return std::to_string(GRACEUI_VERSION_MAJOR(v)) + "." + std::to_string(GRACEUI_VERSION_MINOR(v)) + "." +
         std::to_string(GRACEUI_VERSION_PATCH(v));
}

std::string build_word(const Shell& s) {
  if (!s.game_version_known) return "unknown";
  const auto& v = s.game_version;
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." + std::to_string(v[3]);
}

en::overlay::WorldInputs::Where where_of(en::core::Phase p) noexcept {
  using Where = en::overlay::WorldInputs::Where;
  switch (p) {
    case en::core::Phase::Title: return Where::Title;
    case en::core::Phase::Loading: return Where::Loading;
    case en::core::Phase::Playthrough:
    case en::core::Phase::InWorld: return Where::World;
    case en::core::Phase::Banner: return Where::Stopped;
    case en::core::Phase::Gate:
    case en::core::Phase::Ready: break;
  }
  return Where::Starting;
}

void fill_view(const Shell& s, const en::core::Core& core, const en::proto::WorldSnapshot* latest,
               std::uint64_t tripped, const en::overlay::Overlay& o, en::overlay::ViewStatus& v) {
  const en::core::State& cs = core.state();
  v.version = kVersion;
  v.label = kVersionLabel.data();  // a string literal (EN_DLL_VERSION_LABEL), NUL-terminated
  v.probe_build = kProbeBuild;
  v.init = w::to_string(block::init_state());
  const block::Items items = block::items_seen();
  v.items_resolved = items.resolved;
  v.items_total = items.total;
  v.passes = items.passes;
  v.phase = en::core::to_string(cs.phase);
  v.level = en::core::to_string(cs.level);
  v.banner = cs.banner ? en::core::to_string(*cs.banner) : "none";
  v.gate_open = cs.gate_open;
  v.gate_why = en::core::to_string(cs.gate_why);
  v.dormant = cs.dormant;
  v.badge = cs.badge;
  if (const en::gal::GalTask* task = s.task_set.load(std::memory_order_acquire) ? s.task : nullptr) {
    const en::gal::TaskStatus& st = task->status();
    const en::gal::GuardedReader& reader = task->backend().reader();
    v.snapshots = st.snapshots.load(std::memory_order_relaxed);
    v.task_p99_us = static_cast<std::uint32_t>(st.task_us.percentile_us(990));
    v.read_mode = en::gal::to_string(st.mode.load(std::memory_order_relaxed));
    v.throttled = st.throttled.load(std::memory_order_relaxed);
    v.copies = reader.total_copies();
    static_assert(en::gal::kFaultKinds == en::overlay::kViewFaultKinds);
    for (std::size_t k = 0; k < en::gal::kFaultKinds; ++k) {
      const auto kind = static_cast<en::gal::FaultKind>(k);
      v.fault_names[k] = en::gal::to_string(kind);
      v.faults[k] = reader.faults(kind);
    }
  }
  v.have_health = latest != nullptr;
  if (latest != nullptr) v.health = latest->health;
  v.tripped = tripped;
  v.marks = block::marks();
  v.mark_source = w::to_string(block::mark_source());
  v.graceui_version = o.graceui_version();
  v.changes.clear();
  const std::size_t n = g_changes_seen < en::overlay::kViewChanges ? g_changes_seen : en::overlay::kViewChanges;
  for (std::size_t i = g_changes_seen - n; i < g_changes_seen; ++i) {
    (void)v.changes.try_push_back(g_changes[i % en::overlay::kViewChanges]);
  }
  v.now_ms = mono_ms();
}

}  // namespace

void connect_overlay(Shell& s, bool banner_path) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (!s.settings.overlay_connect) {
      log(Line("connect").flag("skipped", true).word("why", "off"));
      return;
    }
    const en::overlay::Found found = en::overlay::find();
    en::overlay::Options options;
    options.version = kVersion;
    options.view = s.settings.debug_panel;
    // The Mark's hotkey: in a development session (debug.panel), or when the launcher's console holds Pause, which
    // then hands the Marks over to it (later launchers' consoles are taken up on T6).
    options.mark = s.settings.debug_panel || block::mark_source() == w::MarkSource::Launcher;
    en::overlay::Report r;
    en::overlay::Overlay* o = en::overlay::Overlay::connect(found.api, options, r);
    Line l("connect");
    l.flag("ok", o != nullptr).word("outcome", en::overlay::to_string(r.outcome));
    l.word("path", banner_path ? "banner" : "main").word("how", how_word(found.how)).num("looks", found.attempts);
    l.num("look_ms", found.ms).flag("installed", found.installed).word("graceui", version_word(r.graceui_version));
    l.num("status", r.status).num("reason", r.reason).word("view", r.view).flag("mark", r.mark).flag("world", r.world);
    log(l, o != nullptr ? LogLevel::Info : LogLevel::Warn);
    if (o == nullptr) return;  // headless: nothing can be drawn this session (ARCH §10.3)
    auto* link = new OverlayLink();  // lives for the process (R14)
    link->overlay = o;
    s.overlay.store(link, std::memory_order_release);
    SetEvent(s.wake);  // T6 shows what it has at once
  });
}

void overlay_change(const char* what, const char* to, std::uint64_t at_ms) noexcept {
  g_changes[g_changes_seen % en::overlay::kViewChanges] = en::overlay::ViewChange{what, to, at_ms};
  ++g_changes_seen;
}

void overlay_tick(Shell& s, const en::core::Core& core, const en::proto::WorldSnapshot* latest, std::uint64_t tripped,
                  std::uint64_t now) noexcept {
  OverlayLink* link = s.overlay.load(std::memory_order_acquire);
  if (link == nullptr || link->lost) return;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    en::overlay::Overlay& o = *link->overlay;
    const en::core::State& cs = core.state();

    // The banner, replaced in place when its reason changes (it stays once shown: en-core keeps it), and the badge.
    if (!link->banner_known || cs.banner != link->banner) {
      const std::string text =
          cs.banner ? en::core::banner_text(*cs.banner, build_word(s), core.banner_detail()) : std::string();
      const GraceUiStatus st = o.banner(text, GRACEUI_TONE_WARNING);
      if (cs.banner || link->banner_known) {
        log(Line("notice").word("what", "banner").word("reason", cs.banner ? en::core::to_string(*cs.banner) : "none")
                .num("status", st),
            st == GRACEUI_OK ? LogLevel::Info : LogLevel::Warn);
      }
      link->banner_known = true;
      link->banner = cs.banner;
    }
    if (cs.badge != link->badge) {
      const GraceUiStatus st = o.badge(cs.badge ? en::core::kBadgeMultiplayer : std::string_view());
      log(Line("notice").word("what", "badge").flag("up", cs.badge).num("status", st));
      link->badge = cs.badge;
    }
    if (o.notices_only()) return;  // the anti-cheat's notice-only mode: nothing else works (D88)

    // EldenNet's claims as a world-state provider.
    en::overlay::WorldInputs in;
    in.where = where_of(cs.phase);
    in.multiplayer = cs.badge || cs.gate_why == en::core::Unclean::NotSolo || cs.gate_why == en::core::Unclean::Lobby;
    in.anticheat = s.anticheat.load(std::memory_order_relaxed) || (latest != nullptr && latest->session.anticheat);
    in.latest = latest;
    const std::uint32_t flags = en::overlay::world_flags(in);
    if (flags != link->world) {
      const GraceUiStatus st = o.world(flags);
      log(Line("world").hex("flags", flags).num("status", st));
      link->world = flags;
    }

    // The Mark: GraceUI's hotkey on Pause, once a console holds Pause (or debug.panel asked for it at connect), and the
    // ring asked for until it is EldenNet's.
    if (!o.mark_registered() && block::mark_source() == w::MarkSource::Launcher) {
      const GraceUiStatus st = o.register_mark();
      log(Line("mark_hotkey").num("status", st));
    }
    if (o.mark_registered() && !link->marks_ours) {
      link->marks_ours = block::take_marks();
      if (!link->mark_asked || link->marks_ours) {
        log(Line("marks").word("source", w::to_string(block::mark_source())));
      }
      link->mark_asked = true;
    }

    // The view and the hotkeys' presses.
    en::overlay::ViewStatus view;
    if (o.view_ready()) fill_view(s, core, latest, tripped, o, view);
    const en::overlay::Overlay::Step step = o.step(view);
    for (std::uint32_t i = 0; i < step.marks; ++i) {
      if (block::add_own_mark(now) == 0) log(Line("mark").flag("taken", false).word("why", "not_handed_over"));
    }
    if (step.view_presses != 0) log(Line("view").num("presses", step.view_presses).flag("open", o.view_open()));
    if (step.refused != GRACEUI_OK && step.refused != link->refused) {
      log(Line("view").word("refused", "submit_or_panel").num("status", step.refused), LogLevel::Warn);
    }
    link->refused = step.refused;
    if (step.lost) {
      link->lost = true;
      log(Line("overlay").flag("lost", true), LogLevel::Warn);
    }
  });
}

}  // namespace eldennet
