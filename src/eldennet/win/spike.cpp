// The S0 runs (DESIGN §16.1's S0-A to S0-D on the table-based start; roadmap S10.1.b): what eldennet.dll measures for
// them beyond any start, and its report, `eldennet-spike.json` in the run's folder (en_shell's spikerun.hpp).
//
// - T1, at Safety: the run's folder, from ELDENNET_SPIKE_DIR, accepted only when it is the launcher's folder for this
//   spike inside EldenNet's own spikes folder (`<data>\spikes\<word>-<n>`); anything else is ignored, and no report is
//   written.
// - T1, after Discover: discovery's counts and the build's table as discovery checked it.
// - T1, before the reader (S0-D, a probe build only): the trial, guarded copies forced to fault, each counted when the
//   guard caught it: by the SEH copy at a page of EldenNet's own that has no access, and by ReadProcessMemory at the
//   null page;
//   and the group the reader's own faults are injected into (settings.toml's probe table where it names one, else the
//   first optional group this start reads).
// - T1, at its terminal state and before it publishes it: the table checked again over the image as it is now (the
//   ids still where the table puts them, each witness still referring to its id: nothing of ours, or anyone's,
//   changed the game's code there), then the report, so that the launcher finds it once it sees the state.
// - T6, every 10 s from then on: the reader's counts, and the report again.
// Read-only towards the game: the trial reads EldenNet's own page, the recheck the game's image through the guarded
// view, and nothing is written but the report.
#include <algorithm>
#include <cstring>

#include "dll.hpp"
#include "en/gal/resolve.hpp"
#include "en/gal/roles.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

namespace w = en::proto::watch;
using en::shell::Line;
using en::shell::LogLevel;
using en::shell::SpikeWaits;

struct SpikeState {
  std::wstring path;  // the report's file
  en::shell::SpikeRun run;
  const en::gal::Discovery* discovery = nullptr;
  std::uint64_t reader_since_ms = 0;  // mono ms of the reader's first reading
  std::atomic<bool> stopped{false};   // T1 has written its report; T6 alone writes from then on
};

namespace {

constexpr std::uint32_t kTrialCopies = 20;  // per copy primitive: DESIGN §16.1's "20/20 caught"

std::wstring folded(std::wstring s) {
  for (wchar_t& c : s) {
    if (c == L'/') c = L'\\';
    if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
  }
  while (!s.empty() && s.back() == L'\\') s.pop_back();
  return s;
}

// The launcher's folder for this run of `spike`, or nothing: `<data>\spikes\<word>-<n>`, n of 1 to 9 digits.
bool our_folder(const std::wstring& dir, const std::wstring& data, w::Spike spike) {
  if (dir.empty() || data.empty()) return false;
  const std::wstring d = folded(dir);
  const std::wstring prefix = folded(data) + L"\\spikes\\";
  if (d.size() <= prefix.size() || d.compare(0, prefix.size(), prefix) != 0) return false;
  const std::wstring rest = d.substr(prefix.size());
  const std::string_view word = w::to_string(spike);
  if (rest.size() < word.size() + 2 || rest.size() > word.size() + 10) return false;
  for (std::size_t i = 0; i < word.size(); ++i) {
    if (rest[i] != static_cast<wchar_t>(word[i])) return false;
  }
  if (rest[word.size()] != L'-') return false;
  return std::all_of(rest.begin() + static_cast<std::ptrdiff_t>(word.size()) + 1, rest.end(),
                     [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

SpikeState* state_of(Shell& s) { return s.spike.load(std::memory_order_acquire); }

void write_report(SpikeState& st) {
  ++st.run.writes;
  st.run.session_ms = since_ms();
  const std::string text = en::shell::spike_run_text(st.run);
  const bool ok = !text.empty() && write_text(st.path, text);
  if (!ok || st.run.writes == 1) {
    log(Line("spike_report").flag("written", ok).num("writes", st.run.writes).word("waits",
        en::shell::to_string(st.run.waits)), ok ? LogLevel::Info : LogLevel::Warn);
  }
}

// The build's table checked again, over the image as it is now, against what discovery took from it.
void recheck(Shell& s, SpikeState& st) {
  const en::gal::Discovery* d = st.discovery;
  if (d == nullptr || d->layout().items().empty()) return;
  namespace roles = en::gal::roles;
  const en::gal::Library again(d->layout(), d->image(), s.sys->image());
  auto& t = st.run.table;
  t.rechecked = true;
  t.entries = static_cast<std::uint32_t>(again.states().size());
  t.again_refused = static_cast<std::uint32_t>(again.refused());
  t.witnesses = 0;
  for (const en::gal::ItemState& is : again.states()) t.witnesses += is.witnesses;
  t.changed = 0;
  const en::gal::DiscoveryReport& r = d->report();
  for (std::size_t k = 0; k < roles::kItemCount; ++k) {
    const roles::ItemInfo& info = roles::info(static_cast<roles::Item>(k));
    if (d->layout().find_item(info.id) == nullptr) continue;
    const en::gal::ItemResult& was = r.items[k];
    const bool was_ok = was.from_table && was.status == en::gal::ItemStatus::Resolved;
    const auto now = again.place(info.id, info.kind == roles::ItemKind::Function ? en::gal::IdKind::Function
                                                                                  : en::gal::IdKind::Static);
    if (was_ok != now.has_value() || (now && *now != was.rva)) ++t.changed;
  }
  for (std::size_t c = 0; c < roles::kClsCount; ++c) {
    const en::gal::ItemEntry* entry = d->layout().find_item(roles::label(static_cast<roles::Cls>(c)));
    if (entry == nullptr || entry->kind != en::gal::IdKind::Class) continue;
    std::vector<std::uint64_t> now;
    for (const std::uint32_t v : again.vtables(entry->id)) now.push_back(d->module_base() + v);
    std::sort(now.begin(), now.end());
    now.erase(std::unique(now.begin(), now.end()), now.end());
    const auto was = d->vtables(static_cast<roles::Cls>(c)).all();
    if (!std::equal(now.begin(), now.end(), was.begin(), was.end())) ++t.changed;
  }
}

}  // namespace

void spike_begin(Shell& s) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    const w::Spike spike = block::spike();
    if (!en::shell::s0_spike(spike)) return;
    wchar_t buf[1024];
    const DWORD n = GetEnvironmentVariableW(L"ELDENNET_SPIKE_DIR", buf, 1024);
    const std::wstring dir = n == 0 || n >= 1024 ? std::wstring() : std::wstring(buf, n);
    const bool ours = our_folder(dir, s.paths.data, spike);
    log(Line("spike").word("spike", w::to_string(spike)).flag("folder", !dir.empty()).flag("accepted", ours),
        ours ? LogLevel::Info : LogLevel::Warn);
    if (!ours) return;
    auto* st = new SpikeState();  // lives for the process (R14)
    st->path = dir + L"\\" + std::wstring(en::shell::kSpikeRunFile.begin(), en::shell::kSpikeRunFile.end());
    st->run.spike = spike;
    st->run.pid = GetCurrentProcessId();
    st->run.probe_build = kProbeBuild;
    st->run.game_version = s.game_version;
    st->run.region = s.region;
    st->run.en_version = kVersion;
    s.spike.store(st, std::memory_order_release);
  });
}

void spike_discovered(Shell& s, const en::gal::Discovery* d, std::uint32_t passes) noexcept {
  SpikeState* st = state_of(s);
  if (st == nullptr) return;
  auto& dr = st->run.discovery;
  dr.ran = true;
  dr.passes = passes;
  if (d == nullptr) return;  // the files were refused
  st->discovery = d;
  const en::gal::DiscoveryReport& r = d->report();
  dr.ready = r.ready;
  dr.leads = r.layout_has_leads;
  dr.getter_calls = static_cast<std::uint32_t>(r.getter_calls);
  dr.of = static_cast<std::uint32_t>(r.items.size());
  dr.items = 0;
  for (const en::gal::ItemResult& item : r.items) dr.items += item.status == en::gal::ItemStatus::Resolved ? 1 : 0;
  dr.groups_on = 0;
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    dr.groups_on += d->has(static_cast<en::proto::Group>(g)) ? 1 : 0;
  }
  st->run.table.items = r.table_items;
  st->run.table.refused = r.table_refused;
}

void spike_trial(Shell& s) noexcept {
  SpikeState* st = state_of(s);
  if (st == nullptr || st->run.spike != w::Spike::S0d || st->run.trial.ran || !kProbeBuild) return;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto& t = st->run.trial;
    t.ran = true;
    const std::uint64_t page = s.sys->no_access_page();
    std::array<std::byte, 8> buf{};
    std::uint32_t first_code = 0;
    for (std::uint32_t i = 0; i < kTrialCopies; ++i) {
      ++t.seh_forced;
      const en::gal::CopyOutcome o = s.sys->copier_of(en::gal::CopyKind::Seh).copy(page, buf);
      if (!o.copied) ++t.seh_caught;
      if (first_code == 0) first_code = o.code;
    }
    // ReadProcessMemory at the null page, which is never mapped, as S0-D's injection reads with that copy: Wine's
    // ReadProcessMemory on our own process copies a reserved page's zeros, where Windows' refuses it.
    for (std::uint32_t i = 0; i < kTrialCopies; ++i) {
      ++t.rpm_forced;
      if (!s.sys->copier_of(en::gal::CopyKind::Rpm).copy(0, buf).copied) ++t.rpm_caught;
    }
    log(Line("spike_trial").num("seh_forced", t.seh_forced).num("seh_caught", t.seh_caught)
            .num("rpm_forced", t.rpm_forced).num("rpm_caught", t.rpm_caught).hex("code", first_code));
  });
}

void spike_inject(Shell& s, const en::gal::Discovery& d) noexcept {
  SpikeState* st = state_of(s);
  if (st == nullptr || st->run.spike != w::Spike::S0d || !kProbeBuild) return;
  en::shell::ProbeSettings& p = s.settings.probe;
  if (p.fault_every == 0) {
    // No group named in settings.toml: the first optional group this start reads, every read of it faulting from
    // 2 s into the world, so that its faults come twice running and its breaker trips (steady reading counts a fault
    // only then).
    using G = en::proto::Group;
    for (const G g : {G::Camera, G::Place, G::Flags, G::Ui, G::Combat, G::Text, G::Params, G::Status}) {
      if (d.has(g)) {
        p.fault_group = g;
        p.fault_every = 1;
        break;
      }
    }
  }
  if (p.fault_every != 0) {
    st->run.inject.group = p.fault_group;
    st->run.inject.every = p.fault_every;
    st->run.inject.after_s = p.fault_after_s;
  }
  log(Line("spike_inject").word("group", p.fault_every != 0 ? en::proto::to_string(p.fault_group) : "none")
          .num("every", p.fault_every).num("after_s", p.fault_after_s));
}

void spike_reading(Shell& s) noexcept {
  if (SpikeState* st = state_of(s)) st->reader_since_ms = mono_ms();
}

void spike_stopped(Shell& s, w::InitState state, std::optional<en::core::BannerReason> banner,
                   SpikeWaits waits) noexcept {
  SpikeState* st = state_of(s);
  if (st == nullptr || st->stopped.load(std::memory_order_relaxed)) return;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    st->run.state = state;
    st->run.banner = banner;
    st->run.waits = waits;
    st->run.start_ms = since_ms();
    recheck(s, *st);
    st->run.reading.started = s.task_set.load(std::memory_order_acquire);
    log(Line("spike_stopped").word("state", w::to_string(state)).word("waits", en::shell::to_string(waits))
            .num("table", st->run.table.items).num("refused", st->run.table.refused)
            .flag("rechecked", st->run.table.rechecked).num("again_refused", st->run.table.again_refused)
            .num("changed", st->run.table.changed).num("witnesses", st->run.table.witnesses));
    write_report(*st);
  });
  st->stopped.store(true, std::memory_order_release);
}

void spike_tick(Shell& s, std::uint64_t tripped) noexcept {
  SpikeState* st = state_of(s);
  if (st == nullptr || !st->stopped.load(std::memory_order_acquire)) return;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto& r = st->run.reading;
    r.tripped = tripped;
    const en::gal::GalTask* task = s.task_set.load(std::memory_order_acquire) ? s.task : nullptr;
    if (task != nullptr) {
      const en::gal::TaskStatus& ts = task->status();
      const en::gal::GuardedReader& reader = task->backend().reader();
      r.started = true;
      r.ms = st->reader_since_ms != 0 ? mono_ms() - st->reader_since_ms : 0;
      r.snapshots = ts.snapshots.load(std::memory_order_relaxed);
      r.torn = ts.torn.load(std::memory_order_relaxed);
      r.held = ts.held.load(std::memory_order_relaxed);
      r.unconfirmed = ts.unconfirmed_faults.load(std::memory_order_relaxed);
      r.copy_faults = reader.faults(en::gal::FaultKind::Copy);
      r.injected_faults = reader.faults(en::gal::FaultKind::Injected);
      r.p99_us = static_cast<std::uint32_t>(ts.task_us.percentile_us(990));
    }
    write_report(*st);
  });
}

}  // namespace eldennet
