// The effect shell (dll.hpp; ARCH §1.2's T6, §1.3): the task's channels C1 to C5, and the core thread, which drains
// T1's banners, C2's lifecycle messages and C1's newest snapshot in that order, steps en-core's slice on each (and on
// a 100 ms tick, C16), sends its game commands on C4, and keeps the status block, the log and the recording.
// Nothing here runs on the game thread but Io's TaskIo calls, which never wait and never allocate (R6).
#include <deque>

#include "dll.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/record.hpp"

namespace eldennet {

namespace w = en::proto::watch;
using en::shell::Line;
using en::shell::LogLevel;

class Io final : public en::gal::TaskIo {
 public:
  explicit Io(Shell& s) : s_(s) {}

  bool pop_command(en::proto::GameCommand& out) noexcept override { return cmds.try_pop(out); }
  en::proto::WorldSnapshot* begin_snapshot() noexcept override { return snaps.begin_write(); }
  void end_snapshot(bool built) noexcept override {
    snaps.end_write(built);
    if (built) SetEvent(s_.wake);
  }
  bool push_lifecycle(const en::proto::Lifecycle& l) noexcept override {
    en::proto::Lifecycle copy = l;
    if (!life.try_push(copy)) {
      overflow.store(true, std::memory_order_relaxed);  // the core rebuilds the lifecycle from snapshots (C2)
      return false;
    }
    SetEvent(s_.wake);
    return true;
  }
  bool retire(std::shared_ptr<const std::vector<std::uint32_t>>& list) noexcept override {
    return retired.try_push(list);
  }
  void push_render(const en::proto::RenderFeed&) noexcept override {
    renders.fetch_add(1, std::memory_order_relaxed);  // no surface draws the render feed yet: counted only
  }

  en::shell::SnapSlot snaps;
  en::shell::SpscRing<en::proto::Lifecycle, 64> life;
  en::shell::SpscRing<en::proto::GameCommand, 64> cmds;
  en::shell::SpscRing<std::shared_ptr<const std::vector<std::uint32_t>>, 64> retired;
  std::atomic<bool> overflow{false};
  std::atomic<std::uint64_t> renders{0};

 private:
  Shell& s_;
};

namespace {

constinit std::atomic<Shell*> g_shell{nullptr};

// The recording (ARCH §7.5; debug.record_sessions): recordings\<date>-<time>-<pid>-<n>.ensnap.jsonl, plain JSON Lines
// of numbers-only records (record.hpp), rotated past 256 MB. Written by T6 alone.
class Recorder {
 public:
  explicit Recorder(const Shell& s) : s_(s) {}

  bool open() {
    if (folder_.empty()) {
      folder_ = s_.paths.data + L"\\recordings";
      SYSTEMTIME t{};
      GetLocalTime(&t);
      wchar_t stem[64];
      swprintf(stem, 64, L"%04u%02u%02u-%02u%02u%02u-%lu", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
               GetCurrentProcessId());
      stem_ = stem;
      if (!make_dirs(folder_)) return false;
    }
    ++file_;
    const std::wstring path = folder_ + L"\\" + stem_ + L"-" + std::to_wstring(file_) + L".ensnap.jsonl";
    f_ = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f_ == INVALID_HANDLE_VALUE) return false;
    bytes_ = 0;
    en::shell::RecordingHeader h;
    h.game_version = s_.game_version;
    h.region = s_.region;
    h.en_version = kVersion;
    h.spike = w::to_string(block::spike());
    h.probe_build = kProbeBuild;
    h.file = file_;
    put(en::shell::header_line(h, mono_ms()));
    log(Line("recording").flag("open", true).num("file", file_));
    return true;
  }

  void put(const std::string& line) {
    if (f_ == INVALID_HANDLE_VALUE) return;
    if (line.empty()) {
      ++refused_;
      return;
    }
    DWORD n = 0;
    WriteFile(f_, line.data(), static_cast<DWORD>(line.size()), &n, nullptr);
    bytes_ += n;
    ++lines_;
    if (bytes_ > en::shell::kRecordingRotateBytes) {
      CloseHandle(f_);
      f_ = INVALID_HANDLE_VALUE;
      (void)open();
    }
  }
  void flush() {
    if (f_ != INVALID_HANDLE_VALUE) FlushFileBuffers(f_);
  }
  [[nodiscard]] bool on() const noexcept { return f_ != INVALID_HANDLE_VALUE; }
  [[nodiscard]] std::uint64_t lines() const noexcept { return lines_; }
  [[nodiscard]] std::uint64_t refused() const noexcept { return refused_; }

 private:
  const Shell& s_;
  std::wstring folder_;
  std::wstring stem_;
  HANDLE f_ = INVALID_HANDLE_VALUE;
  std::uint64_t bytes_ = 0;
  std::uint64_t file_ = 0;
  std::uint64_t lines_ = 0;
  std::uint64_t refused_ = 0;
};

const char* change_word(const en::core::Change& c) {
  switch (c.what) {
    case en::core::Change::What::Phase: return en::core::to_string(static_cast<en::core::Phase>(c.to));
    case en::core::Change::What::Level: return en::core::to_string(static_cast<en::core::Level>(c.to));
    case en::core::Change::What::Banner: return en::core::to_string(static_cast<en::core::BannerReason>(c.to));
    case en::core::Change::What::GateClosed: return en::core::to_string(static_cast<en::core::Unclean>(c.from));
    case en::core::Change::What::GateOpened: return "open";
    case en::core::Change::What::Badge: return c.to != 0 ? "up" : "down";
  }
  return "none";
}

DWORD WINAPI core_main(void* param) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, DWORD{1}, [&]() -> DWORD {
    Shell& s = *static_cast<Shell*>(param);
    Io& io = *s.io;
    auto core = std::make_unique<en::core::Core>();
    auto latest = std::make_unique<en::proto::WorldSnapshot>();
    Recorder rec(s);
    if (s.settings.record_sessions && !rec.open()) log(Line("recording").flag("open", false), LogLevel::Warn);
    std::deque<en::proto::GameCommand> outbox;
    std::uint64_t last_tick = mono_ms();
    std::uint64_t last_status = 0;
    std::uint64_t last_flush = 0;
    std::uint32_t marks_seen = 0;
    std::uint64_t stall_snapshots = 0;
    std::uint64_t stall_since = 0;
    bool stalled = false;
    bool crashed = false;
    std::uint64_t tripped = 0;  // the groups whose breaker tripped (GroupTripped on C2), bit per group
    const auto step = [&](en::core::CoreInput in, std::uint64_t now) {
      en::core::Outputs out = core->step(std::move(in), now);
      for (auto& c : out.game) {
        if (rec.on()) rec.put(en::shell::command_line(c, now));
        outbox.push_back(std::move(c));
      }
      for (const en::core::Change& c : out.changes) {
        log(Line("change").word("what", en::core::to_string(c.what)).word("to", change_word(c)));
        if (rec.on()) rec.put(en::shell::change_line(c, now));
        overlay_change(en::core::to_string(c.what), change_word(c), c.at);  // the F11 view's list
      }
    };
    for (;;) {
      WaitForSingleObject(s.wake, 100);
      const std::uint64_t now = mono_ms();
      en::core::BootBanner banner;
      while (s.banners.try_pop(banner)) {
        if (rec.on()) rec.put(en::shell::banner_line(banner.reason, now));
        step(en::core::CoreInput{std::move(banner)}, now);
      }
      en::proto::Lifecycle life;
      while (io.life.try_pop(life)) {
        if (rec.on()) rec.put(en::shell::lifecycle_line(life, now));
        if (const auto* g = std::get_if<en::proto::Lifecycle::GroupTripped>(&life.payload)) {
          tripped |= 1ull << static_cast<unsigned>(g->group);
          log(Line("tripped").word("group", en::proto::to_string(g->group)), LogLevel::Warn);
        }
        step(en::core::CoreInput{life}, now);
      }
      if (io.snaps.take(*latest)) {
        // C14: the safety atomics, which the task does not read (ARCH §5.3), join the snapshot the core judges.
        latest->session.anticheat = latest->session.anticheat || s.anticheat.load(std::memory_order_relaxed);
        latest->session.ersc_loaded = latest->session.ersc_loaded || s.ersc.load(std::memory_order_relaxed);
        if (rec.on()) rec.put(en::shell::snapshot_line(*latest, latest->mono_ms));
        step(en::core::CoreInput{std::make_unique<en::proto::WorldSnapshot>(*latest)}, now);
      }
      if (now - last_tick >= 100) {
        last_tick = now;
        step(en::core::CoreInput{en::core::CoreInput::Tick{}}, now);
      }
      while (!outbox.empty()) {
        if (!io.cmds.try_push(outbox.front())) break;
        outbox.pop_front();
      }
      std::shared_ptr<const std::vector<std::uint32_t>> retired;
      while (io.retired.try_pop(retired)) retired.reset();  // freed here, never on the game thread (C5)
      // The Marks the launcher stamped (O90): into the recording on their own clock.
      for (const std::uint32_t n = block::marks(); marks_seen < n;) {
        ++marks_seen;
        log(Line("mark").num("n", marks_seen));
        if (rec.on()) rec.put(en::shell::mark_line(marks_seen, block::mark_ms(marks_seen)));
      }
      // The status block.
      const en::core::State& cs = core->state();
      block::core(static_cast<std::uint32_t>(cs.phase) + 1, static_cast<std::uint32_t>(cs.level) + 1,
                  cs.banner ? static_cast<std::uint32_t>(*cs.banner) + 1 : 0,
                  w::kFlagCore | (cs.gate_open ? w::kFlagGateOpen : 0u) | (cs.dormant ? w::kFlagDormant : 0u) |
                      (cs.badge ? w::kFlagBadge : 0u));
      const en::gal::GalTask* task = s.task_set.load(std::memory_order_acquire) ? s.task : nullptr;
      std::uint64_t snapshots = 0;
      if (task != nullptr) {
        const en::gal::TaskStatus& st = task->status();
        const en::gal::GuardedReader& reader = task->backend().reader();
        snapshots = st.snapshots.load(std::memory_order_relaxed);
        block::counts(snapshots,
                      reader.faults(en::gal::FaultKind::Copy) + reader.faults(en::gal::FaultKind::Injected),
                      tripped, static_cast<std::uint32_t>(st.task_us.percentile_us(990)));
        // The stall detector: a running task that has built no snapshot for 5 s (WHY§175: the gate does not act on
        // a silence; the log says it).
        if (stall_since == 0) stall_since = now;
        if (st.state.load(std::memory_order_relaxed) == en::gal::TaskState::Running) {
          if (snapshots != stall_snapshots) {
            if (stalled) log(Line("stall").flag("over", true).num("ms", now - stall_since));
            stalled = false;
            stall_snapshots = snapshots;
            stall_since = now;
          } else if (!stalled && now - stall_since >= 5000) {
            stalled = true;
            log(Line("stall").flag("over", false).num("snapshots", snapshots), LogLevel::Warn);
          }
        }
      }
      // GraceUI (overlay.cpp): the banner and the badge as notices, the world-state claims, the Mark and the F11 view.
      overlay_tick(s, *core, cs.snapshots > 0 ? latest.get() : nullptr, tripped, now);
      if (now - last_status >= 10'000) {
        last_status = now;
        Line l("status");
        l.word("phase", en::core::to_string(cs.phase)).word("level", en::core::to_string(cs.level));
        l.flag("gate", cs.gate_open).word("why", en::core::to_string(cs.gate_why)).flag("dormant", cs.dormant);
        l.flag("badge", cs.badge).num("snapshots", snapshots).num("renders", io.renders.load());
        l.num("recorded", rec.lines()).num("refused", rec.refused()).num("log_dropped", log_dropped());
        log(l);
        if (task != nullptr) {
          const en::gal::GuardedReader& reader = task->backend().reader();
          Line f("reads");
          f.num("copies", reader.total_copies());
          for (std::size_t k = 0; k < en::gal::kFaultKinds; ++k) {
            f.num(en::gal::to_string(static_cast<en::gal::FaultKind>(k)), reader.faults(static_cast<en::gal::FaultKind>(k)));
          }
          f.num("stale_stacks", reader.stale_stacks()).num("vtable_skips", task->backend().vtable_skips()).num("soft_world", task->backend().soft_skips(en::proto::Group::World));
          f.word("mode", en::gal::to_string(task->status().mode.load())).flag("throttled", task->status().throttled.load());
          log(f);
        }
      }
      if (rec.on() && now - last_flush >= 1000) {
        last_flush = now;
        rec.flush();
      }
      // S0-D's crash on request, honoured by a probe build only (watch.hpp).
      if (kProbeBuild && !crashed && block::crash_requested()) {
        crashed = true;
        log(Line("crash").word("requested", "launcher").word("thread", "core"), LogLevel::Warn);
        Sleep(200);  // the log writer's turn
        RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
      }
    }
  });
}

}  // namespace

Shell* shell() noexcept { return g_shell.load(std::memory_order_acquire); }

Shell& make_shell() {
  Shell* s = g_shell.load(std::memory_order_acquire);
  if (s == nullptr) {
    s = new Shell();  // lives for the process (R14)
    s->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_shell.store(s, std::memory_order_release);
  }
  return *s;
}

Io* make_io(Shell& s) { return new Io(s); }  // lives for the process (R14)

en::gal::TaskIo& task_io(Io& io) noexcept { return io; }

bool start_core(Shell& s) noexcept {
  HANDLE t = CreateThread(nullptr, 0, &core_main, &s, 0, nullptr);
  if (t == nullptr) return false;
  CloseHandle(t);
  return true;
}

void push_banner(Shell& s, en::core::BannerReason reason, std::string_view detail) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    en::core::BootBanner b{reason, std::string(detail)};
    if (s.banners.try_push(b)) SetEvent(s.wake);
  });
}

}  // namespace eldennet
