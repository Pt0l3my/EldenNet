// GraceUI through en-overlay (dll.hpp; part 12; ARCH §1.4, §3.2's Connect and ConnectB; GraceUI eldennet-changes §14 to
// §18): T1's connect, then T6's part after each step of the core.
//
// - T1: find GraceUI and connect (`connect_overlay`), one log line either way. A failed connect leaves EldenNet
//   headless at L2 at most, nothing drawn (ARCH §10.3, "Overlay failed to install"); the launcher's hint the next time
//   it runs is its GraceUI pre-flight.
// - T6, once the link is published: the banner (ARCH §5.4, §10.3's texts, en-core's banner_text) and the pause badge
//   as notices; EldenNet's claims as a world-state provider (en-overlay's world.hpp); the Mark's hotkey on Pause when
//   the launcher's console holds Pause or debug.panel is on, and, once it is bound, the hand-over of the Marks' ring
//   (watch.hpp's MarkSource): registered unbound (GraceUI D164, another mod's hotkey took Pause first), the launcher's
//   Pause stays the Mark; the F11 view (debug.panel), whose status is filled here from the core, the task and the
//   block; and EldenNet's space (its pages, GraceUI D169; en-overlay's space.hpp): registered on T1 at the connect, and
//   on T6 its facts filled from the same sources, its events handled at each step, every command answered at once,
//   and what a command changed applied (the log's detail now) and handed to T11 for settings.toml after 300 ms of
//   quiet (D14). The space holds the log's lines from T1's first, through T11's tail, whether GraceUI answered or not.
#include <map>
#include <optional>
#include <string>

#include "dll.hpp"
#include "en/gal/guarded.hpp"
#include "en/overlay/overlay.hpp"
#include "en/overlay/space.hpp"
#include "en/overlay/world.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/proto/graceui_pin.hpp"

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
  bool mark_tried = false;  // the Mark's hotkey asked for once on T6 (a GraceUI without hotkeys answers not_yet)
  bool mark_asked = false;
  bool marks_ours = false;
  bool mark_kept_told = false;  // "the launcher keeps Pause" logged, for an unbound Mark
  bool lost = false;
  GraceUiStatus refused = GRACEUI_OK;
  GraceUiStatus page_refused = GRACEUI_OK;
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

// EldenNet's space and what its commands changed, T6's alone, made at its first wake and never destroyed (R14).
struct SpaceState {
  en::overlay::Space space;
  std::map<std::string, en::shell::SettingsEdit> edits;  // every edit of the session, by key
  std::uint64_t changed_ms = 0;                           // the last change, for D14's 300 ms of quiet
  bool unwritten = false;
};
constinit SpaceState* g_space = nullptr;

SpaceState& space_state(const Shell& s) {
  if (g_space == nullptr) {
    g_space = new SpaceState();
    // The values settings.toml gave at Settings (the defaults if it was refused).
    en::overlay::Space& sp = g_space->space;
    sp.set_value("debug.panel", s.settings.debug_panel);
    sp.set_value("debug.record_sessions", s.settings.record_sessions);
    sp.set_value("debug.log_level", en::shell::to_string(s.settings.log_level));
  }
  return *g_space;
}

en::overlay::Level level_of(en::core::Level l) noexcept {
  switch (l) {
    case en::core::Level::Starting: return en::overlay::Level::Starting;
    case en::core::Level::L1: return en::overlay::Level::Inactive;
    case en::core::Level::L2: return en::overlay::Level::Observing;
  }
  return en::overlay::Level::Starting;
}

en::overlay::Stop stop_of(const std::optional<en::core::BannerReason>& r) noexcept {
  using S = en::overlay::Stop;
  if (!r) return S::None;
  switch (*r) {
    case en::core::BannerReason::UnknownBuild: return S::UnknownBuild;
    case en::core::BannerReason::AntiCheat: return S::AntiCheat;
    case en::core::BannerReason::NotStarted: return S::NotStarted;
    case en::core::BannerReason::DiscoveryFailed: return S::DiscoveryFailed;
    case en::core::BannerReason::RegisterFaulted: return S::RegisterFaulted;
    case en::core::BannerReason::Inert: return S::Inert;
    case en::core::BannerReason::SessionFailed: return S::SessionFailed;
    case en::core::BannerReason::TaskTripped: return S::TaskTripped;
    case en::core::BannerReason::SpikeRun: return S::SpikeRun;
  }
  return S::None;
}

en::overlay::Where place_of(en::core::Phase p) noexcept {
  using W = en::overlay::Where;
  switch (p) {
    case en::core::Phase::Gate:
    case en::core::Phase::Ready: return W::Starting;
    case en::core::Phase::Banner: return W::Stopped;
    case en::core::Phase::Title: return W::Title;
    case en::core::Phase::Playthrough:
    case en::core::Phase::InWorld: return W::World;
    case en::core::Phase::Loading: return W::Loading;
  }
  return W::Starting;
}

en::overlay::Unclean unclean_of(en::core::Unclean u) noexcept {
  using U = en::overlay::Unclean;
  switch (u) {
    case en::core::Unclean::None: return U::None;
    case en::core::Unclean::Gap: return U::Gap;
    case en::core::Unclean::SessionFailed: return U::SessionFailed;
    case en::core::Unclean::AntiCheat: return U::AntiCheat;
    case en::core::Unclean::NotSolo: return U::NotSolo;
    case en::core::Unclean::Lobby: return U::Lobby;
    case en::core::Unclean::Loading: return U::Loading;
  }
  return U::Loading;
}

en::overlay::Reading reading_of(en::gal::TaskState t) noexcept {
  using R = en::overlay::Reading;
  switch (t) {
    case en::gal::TaskState::Registered: return R::Registered;
    case en::gal::TaskState::PhaseA: return R::PhaseA;
    case en::gal::TaskState::Running: return R::Running;
    case en::gal::TaskState::Inert: return R::Inert;
  }
  return R::None;
}

// What the space shows, from the core, the task, the block and the shell; `banner` keeps the banner's words alive.
void fill_space(const Shell& s, const en::core::Core& core, const en::proto::WorldSnapshot* latest,
                std::uint64_t tripped, std::uint32_t graceui, std::string& banner, en::overlay::SpaceFacts& f) {
  const en::core::State& cs = core.state();
  f.version = kVersion;
  f.graceui = graceui;
  f.floor = en::proto::graceui_pin::kFloor;
  f.init = block::init_state();
  f.level = level_of(cs.level);
  f.stop = stop_of(cs.banner);
  banner = cs.banner ? en::core::banner_text(*cs.banner, build_word(s), core.banner_detail()) : std::string();
  f.stop_text = banner;
  f.where = place_of(cs.phase);
  f.gate_open = cs.gate_open;
  f.gate_why = unclean_of(cs.gate_why);
  f.dormant = cs.dormant;
  f.badge = cs.badge;
  f.build_known = s.game_version_known;
  f.build = s.game_version;
  f.build_supported = s.build_supported;
  if (const en::gal::GalTask* task = s.task_set.load(std::memory_order_acquire) ? s.task : nullptr) {
    const en::gal::TaskStatus& st = task->status();
    f.reading = reading_of(st.state.load(std::memory_order_relaxed));
    f.snapshots = st.snapshots.load(std::memory_order_relaxed);
    f.task_p99_us = static_cast<std::uint32_t>(st.task_us.percentile_us(990));
  }
  f.have_health = latest != nullptr;
  if (latest != nullptr) f.health = latest->health;
  f.tripped = tripped;
  f.watcher = s.watcher_started.load(std::memory_order_relaxed);
  f.logger = s.logger_started;
  f.log_dropped = log_dropped();
  f.recording_asked = s.settings.record_sessions;
  f.recording = s.recording.load(std::memory_order_relaxed);
  f.recorded = s.recorded.load(std::memory_order_relaxed);
  f.launcher = block::attached();
  f.settings_refused = s.settings_refused;
  f.session_ms = since_ms();
}

const char* note_word(en::overlay::SpaceNote::What w) noexcept {
  switch (w) {
    case en::overlay::SpaceNote::What::Watched: return "watched";
    case en::overlay::SpaceNote::What::Unwatched: return "unwatched";
    case en::overlay::SpaceNote::What::Command: return "command";
  }
  return "none";
}

// What the commands changed: the log's detail at once (its effect "now"); the rest at the next launch. Each change
// joins the session's edits, which T11 writes once the pages have been quiet for 300 ms (D14).
void apply_changes(SpaceState& st, std::uint64_t now) {
  for (en::overlay::Space::Change& c : st.space.take_changes()) {
    en::shell::SettingsEdit e;
    e.key = c.key;
    if (c.value.is_boolean()) {
      e.flag = c.value.get<bool>();
    } else if (c.value.is_string()) {
      e.is_text = true;
      e.text = c.value.get<std::string>();
    } else {
      continue;  // no setting of this slice holds a number
    }
    if (c.key == "debug.log_level") {
      constexpr std::array<std::string_view, 4> kLevels = {"error", "warn", "info", "debug"};
      for (std::size_t i = 0; i < kLevels.size(); ++i) {
        if (e.text == kLevels[i]) set_log_level(static_cast<LogLevel>(i));
      }
    }
    log(Line("setting").word("key", c.key).word("effect", c.key == "debug.log_level" ? "now" : "next_launch"));
    st.edits[c.key] = std::move(e);
    st.changed_ms = now;
    st.unwritten = true;
  }
  if (st.unwritten && now - st.changed_ms >= 300) {
    std::vector<en::shell::SettingsEdit> all;
    for (const auto& [key, e] : st.edits) all.push_back(e);
    post_settings_write(std::move(all));
    st.unwritten = false;
  }
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
    // EldenNet's space for both views; on the banner path (ConnectB, a refused gate or anti-cheat) for the game's view
    // alone, so that the browser never gets it there (PAGES §4.4, GraceUI §7.2 item 1).
    options.space_views = banner_path ? GRACEUI_VIEW_GAME : GRACEUI_VIEW_GAME | GRACEUI_VIEW_BROWSER;
    en::overlay::Report r;
    en::overlay::Overlay* o = en::overlay::Overlay::connect(found.api, options, r);
    Line l("connect");
    l.flag("ok", o != nullptr).word("outcome", en::overlay::to_string(r.outcome));
    l.word("path", banner_path ? "banner" : "main").word("how", how_word(found.how)).num("looks", found.attempts);
    l.num("look_ms", found.ms).flag("installed", found.installed).word("graceui", version_word(r.graceui_version));
    l.num("status", r.status).num("reason", r.reason).word("view", r.view).flag("mark", r.mark);
    l.flag("mark_bound", r.mark_bound).flag("world", r.world).num("pages", r.pages);
    log(l, o != nullptr ? LogLevel::Info : LogLevel::Warn);
    if (o == nullptr) return;  // headless: nothing can be drawn this session (ARCH §10.3)
    // F11 taken first (D164): the view is made, but nothing opens it until the player binds its hotkey in GraceUI.
    if (o->view_ready() && !o->view_bound()) log(Line("view").word("hotkey", "unbound"), LogLevel::Warn);
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
  // The space's logs stream takes every line T11 wrote, from T1's first, whether GraceUI is there or not.
  SpaceState* space = nullptr;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    space = &space_state(s);
    en::shell::LogEntry e;
    while (take_log_tail(e)) space->space.log_line(std::string_view(e.text.data(), e.len), e.at_ms);
  });
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

    // The Mark: GraceUI's hotkey on Pause, once a console holds Pause (or debug.panel asked for it at connect), and,
    // bound, the ring asked for until it is EldenNet's. Unbound (D164), the launcher is never asked: its Pause stays the
    // Mark, and the session's Marks are its own.
    if (!o.mark_registered() && !link->mark_tried && block::mark_source() == w::MarkSource::Launcher) {
      link->mark_tried = true;
      const GraceUiStatus st = o.register_mark();
      log(Line("mark_hotkey").num("status", st).flag("bound", st == GRACEUI_OK),
          st == GRACEUI_OK || st == GRACEUI_UNBOUND ? LogLevel::Info : LogLevel::Warn);
    }
    if (o.mark_registered() && !o.mark_bound() && !link->mark_kept_told &&
        block::mark_source() == w::MarkSource::Launcher) {
      link->mark_kept_told = true;
      log(Line("marks").word("source", w::to_string(block::mark_source())).flag("bound", false));
    }
    if (o.mark_bound() && !link->marks_ours) {
      link->marks_ours = block::take_marks();
      if (!link->mark_asked || link->marks_ours) {
        log(Line("marks").word("source", w::to_string(block::mark_source())));
      }
      link->mark_asked = true;
    }

    // The view, the hotkeys' presses and the space's events: what the views watch published, each command answered.
    en::overlay::ViewStatus view;
    if (o.view_ready()) fill_view(s, core, latest, tripped, o, view);
    en::overlay::SpaceFacts facts;
    std::string banner_words;
    const bool pages = o.pages() && space != nullptr;
    if (pages) fill_space(s, core, latest, tripped, o.graceui_version(), banner_words, facts);
    const en::overlay::Overlay::Step step =
        pages ? o.step(view, &space->space, &facts, now) : o.step(view);
    if (pages) {
      for (const en::overlay::SpaceNote& n : space->space.take_notes()) {
        Line l("page");
        l.word("what", note_word(n.what)).word(n.what == en::overlay::SpaceNote::What::Command ? "name" : "topic",
                                                n.topic);
        if (!n.key.empty()) l.word("key", n.key);
        if (!n.result.empty()) l.word("result", n.result);
        log(l);
      }
      apply_changes(*space, now);
      if (step.page_refused != GRACEUI_OK && step.page_refused != link->page_refused) {
        log(Line("page").word("refused", "publish_or_answer").num("status", step.page_refused), LogLevel::Warn);
      }
      link->page_refused = step.page_refused;
    }
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
