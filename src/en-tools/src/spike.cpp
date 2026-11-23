// eldennet-engine rec-stats, replay and spike-report: the first in-game build's reports and recording tools (the plan's
// part 9). Each reads files the owner's PC wrote (eldennet.dll's recordings, the launcher's spike logs, both numbers
// and fixed words only) and writes a numbers-only report through en-gal's writer, which `lint --report` checks.
//
//   eldennet-engine rec-stats <recording> [--out FILE]
//   eldennet-engine replay <recording> [--out FILE]
//   eldennet-engine spike-report s0a|s0b|s0c|s0d [--spikes DIR] [--recordings DIR] [--log FILE]...
//                                [--recording FILE]... [--run FILE]... [--need N] [--session-s S] [--out FILE]
//
// - rec-stats: what a recording holds (README.md lists it): its lines by kind, its length, the snapshots' rate, gaps,
//   late and stalled intervals, the task's build times and its C13 p99, the faults and trips by group, the lifecycle
//   messages, the core's changes, the gate's time open, the banners, the commands and the Marks.
// - replay: the recording's own inputs (its lifecycle messages, banners and snapshots, in their order) stepped through
//   en-core's v0.1 slice again, and the changes it answers compared with those the recording holds: the same core on
//   the same inputs must answer the same, or the core or the recorder has a defect.
// - spike-report: DESIGN §16.1's pass rules for S0-A to S0-D on the table-based start (since 2026-11-23) over the
//   launcher's spike logs, eldennet.dll's reports of the runs (eldennet-spike.json, en_shell's spikerun.hpp) and the
//   recordings, each rule `pass`, `fail` or `not_measured` (what no file measures, such as "no online menu", the
//   owner's to observe, or what a start that stopped short of its reading could not give), and the spike's verdict:
//   `pass` when every measured rule passes, `incomplete` when one could not be measured from the files given, else
//   `fail`. A spike whose start waits for something (the start's layout not complete yet) says so, and is never failed
//   for it. `--spikes` takes the spikes folder whole (its logs, and each run's folder with its report), `--recordings`
//   the recordings folder, each recording joined to its run by the game's process id in its name.
//
// Exit codes: 0 the report was written (rec-stats, replay: and the replay agrees; spike-report: its verdict is pass);
// 1 replay disagrees, or the spike's verdict is fail or incomplete; 2 a usage error, or a file that cannot be read, is
// too large, or is refused by the lint (each problem on standard error, never quoting what was there); 5 the report
// was refused by its own writer (a defect of ours).
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/core/core.hpp"
#include "en/gal/report.hpp"
#include "en/proto/watch.hpp"
#include "en/augur/events.hpp"
#include "en/shell/record.hpp"
#include "en/shell/spikerun.hpp"
#include "golden_run.hpp"

namespace {

namespace report = en::gal::report;
using report::Key;
using report::Type;
using en::shell::RecordedLine;
using Json = nlohmann::json;

constexpr std::uintmax_t kMaxLogBytes = 16ull << 20;

// ---- The reports' vocabulary: one key table, three kinds ----

constexpr std::array<std::string_view, 7> kKinds = {"header", "snap", "life", "banner", "change", "cmd", "mark"};
constexpr std::array<std::string_view, 4> kRegions = {"ww", "jp", "other", "none"};
constexpr std::array<std::string_view, 9> kSpikes = {"none", "s0a",       "s0b", "s0c", "s0d",
                                                     "s0d-crash", "s0e", "s0e_alone", "s4"};
constexpr std::array<std::string_view, 11> kGroups = {"session", "world", "combat", "player", "flags", "text",
                                                      "ui",      "place", "camera", "params", "status"};
constexpr std::array<std::string_view, 6> kChanges = {"phase", "gate_opened", "gate_closed", "level", "banner", "badge"};
constexpr std::array<std::string_view, 9> kBanners = {"unknown_build",    "anticheat",        "not_started",
                                                      "discovery_failed", "register_faulted", "inert",
                                                      "session_failed",   "task_tripped",     "spike_run"};
static_assert(kSpikes.size() == en::proto::watch::kSpikeCount);
static_assert(kBanners.size() == static_cast<std::size_t>(en::core::BannerReason::SpikeRun) + 1);
constexpr std::array<std::string_view, 7> kPhases = {"gate",       "banner",   "ready",  "title",
                                                     "playthrough", "in_world", "loading"};
constexpr std::array<std::string_view, 3> kLevels = {"starting", "l1", "l2"};
constexpr std::array<std::string_view, 3> kVerdicts = {"pass", "fail", "incomplete"};
constexpr std::array<std::string_view, 3> kStatuses = {"pass", "fail", "not_measured"};
constexpr std::array<std::string_view, 6> kModes = {"suspended", "window", "title", "watch", "vanilla", "unknown"};
constexpr std::array<std::string_view, 20> kRules = {
    "runs_per_mode", "ready_or_l1",   "no_crash",           "both_modes",        "session_length",
    "faults",        "no_trip",       "task_p99_us",        "gate_closed_unless_solo", "connections",
    "online_menu",   "faults_caught", "breaker_tripped",    "crashes_recorded",  "recording",
    "spike_log",     "table_checked", "table_unchanged",    "no_call",           "ready_start"};
// What a spike waits for when its start stopped short of its reading (en_shell's spikerun.hpp: SpikeWaits).
constexpr std::array<std::string_view, 6> kWaits = {"none", "layout", "files", "build", "anticheat", "not_started"};

constexpr std::array<Key, 73> kKeys = {{
    {"header", Type::Object},
    {"game_version", Type::Array, Type::Unsigned},
    {"en_version", Type::Array, Type::Unsigned},
    {"region", Type::Word, Type::Object, kRegions},
    {"spike", Type::Word, Type::Object, kSpikes},
    {"probe_build", Type::Boolean},
    {"files", Type::Unsigned},
    {"lines", Type::Unsigned},
    {"kinds", Type::Array, Type::Object},
    {"kind", Type::Word, Type::Object, kKinds},
    {"count", Type::Unsigned},
    {"duration_ms", Type::Unsigned},
    {"snapshots", Type::Object},
    {"gaps", Type::Unsigned},
    {"late", Type::Unsigned},
    {"stalls", Type::Unsigned},
    {"interval_max_ms", Type::Unsigned},
    {"rate_mhz", Type::Unsigned},
    {"chars_max", Type::Unsigned},
    {"flag_frames", Type::Unsigned},
    {"task", Type::Object},
    {"build_us_p50", Type::Unsigned},
    {"build_us_p99", Type::Unsigned},
    {"build_us_max", Type::Unsigned},
    {"p99_us_last", Type::Unsigned},
    {"p99_us_max", Type::Unsigned},
    {"groups", Type::Array, Type::Object},
    {"group", Type::Word, Type::Object, kGroups},
    {"faults", Type::Unsigned},
    {"tripped", Type::Boolean},
    {"ok", Type::Unsigned},
    {"life", Type::Object},
    {"data_ready", Type::Unsigned},
    {"playthrough_ready", Type::Unsigned},
    {"group_tripped", Type::Unsigned},
    {"changes", Type::Array, Type::Object},
    {"what", Type::Word, Type::Object, kChanges},
    {"gate", Type::Object},
    {"opened", Type::Unsigned},
    {"closed", Type::Unsigned},
    {"open_ms", Type::Unsigned},
    {"banners", Type::Array, Type::Object},
    {"reason", Type::Word, Type::Object, kBanners},
    {"commands", Type::Unsigned},
    {"marks", Type::Unsigned},
    // replay
    {"recorded", Type::Unsigned},
    {"replayed", Type::Unsigned},
    {"agree", Type::Boolean},
    {"first_difference", Type::Unsigned},
    {"phases", Type::Array, Type::Object},
    {"phase", Type::Word, Type::Object, kPhases},
    {"entered", Type::Unsigned},
    {"final", Type::Object},
    {"level", Type::Word, Type::Object, kLevels},
    {"gate_open", Type::Boolean},
    {"playthroughs", Type::Unsigned},
    // spike
    {"verdict", Type::Word, Type::Object, kVerdicts},
    {"rules", Type::Array, Type::Object},
    {"rule", Type::Word, Type::Object, kRules},
    {"status", Type::Word, Type::Object, kStatuses},
    {"value", Type::Unsigned},
    {"limit", Type::Unsigned},
    {"mode", Type::Word, Type::Object, kModes},
    {"modes", Type::Array, Type::Object},
    {"runs", Type::Unsigned},
    {"ready", Type::Unsigned},
    {"banner", Type::Unsigned},
    {"inert", Type::Unsigned},
    {"crashes", Type::Unsigned},
    {"timeouts", Type::Unsigned},
    {"other", Type::Unsigned},
    {"items_min", Type::Unsigned},
    {"items_max", Type::Unsigned},
}};
constexpr std::array<Key, 13> kMoreKeys = {{
    // replay: Augur's events (part 11)
    {"events", Type::Unsigned},
    {"event_kinds", Type::Array, Type::Object},
    {"event", Type::Word, Type::Object, en::augur::kKindNames},
    {"items_total", Type::Unsigned},
    {"passes", Type::Array, Type::Unsigned},
    {"ready_ms_p50", Type::Unsigned},
    {"ready_ms_max", Type::Unsigned},
    {"records", Type::Unsigned},
    {"filter_records", Type::Unsigned},
    {"connections_max", Type::Unsigned},
    {"sessions", Type::Unsigned},
    {"recordings", Type::Unsigned},
    {"logs", Type::Unsigned},
}};

std::array<Key, kKeys.size() + kMoreKeys.size()> all_keys() {
  std::array<Key, kKeys.size() + kMoreKeys.size()> out{};
  std::copy(kKeys.begin(), kKeys.end(), out.begin());
  std::copy(kMoreKeys.begin(), kMoreKeys.end(), out.begin() + kKeys.size());
  return out;
}

const std::array<Key, kKeys.size() + kMoreKeys.size()>& keys() {
  static const auto table = all_keys();
  return table;
}

// The spike report's own keys since its schema 2 (2026-11-23, the table-based start): eldennet.dll's reports of the
// runs (`run_reports`), what the spike waits for, the build's table, S0-D's forced faults and the reader's steady
// counts.
constexpr std::array<Key, 13> kSpikeKeys = {{
    {"waits", Type::Word, Type::Object, kWaits},
    {"run_reports", Type::Unsigned},
    {"table_items", Type::Unsigned},
    {"table_refused", Type::Unsigned},
    {"table_changed", Type::Unsigned},
    {"trial", Type::Object},
    {"forced", Type::Unsigned},
    {"caught", Type::Unsigned},
    {"steady", Type::Object},
    {"torn", Type::Unsigned},
    {"held", Type::Unsigned},
    {"unconfirmed", Type::Unsigned},
    {"reading_ms", Type::Unsigned},
}};

std::array<Key, kKeys.size() + kMoreKeys.size() + kSpikeKeys.size()> all_spike_keys() {
  std::array<Key, kKeys.size() + kMoreKeys.size() + kSpikeKeys.size()> out{};
  const auto& base = keys();
  std::copy(base.begin(), base.end(), out.begin());
  std::copy(kSpikeKeys.begin(), kSpikeKeys.end(), out.begin() + base.size());
  return out;
}

const std::array<Key, kKeys.size() + kMoreKeys.size() + kSpikeKeys.size()>& spike_keys() {
  static const auto table = all_spike_keys();
  return table;
}

}  // namespace

// The three kinds, for lint --report too (lint.cpp).
const report::Vocabulary& rec_stats_vocabulary() {
  static const report::Vocabulary v{"rec_stats", 1, keys()};
  return v;
}
const report::Vocabulary& replay_vocabulary() {
  static const report::Vocabulary v{"replay", 1, keys()};
  return v;
}
// Schema 2 since 2026-11-23: the table-based start's rules and keys (a new key or word is a new schema, WHY§188).
const report::Vocabulary& spike_vocabulary() {
  static const report::Vocabulary v{"spike", 2, spike_keys()};
  return v;
}

namespace {

// ---- Reading ----

std::optional<std::string> read_file(const std::string& path, std::uintmax_t max_bytes) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size > max_bytes) return std::nullopt;
  std::ifstream in(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  if (!in || (size != 0 && !in.read(text.data(), static_cast<std::streamsize>(size)))) return std::nullopt;
  return text;
}

template <class F>
void each_line(std::string_view text, F&& f) {
  std::size_t at = 0;
  while (at < text.size()) {
    std::size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (!line.empty()) f(line);
    at = end + 1;
  }
}

// A recording: the golden runner's loader (lint first, then each line read back).
std::optional<std::vector<RecordedLine>> load_recording(const std::string& path, const char* cmd) {
  return en::tools::load_recording(path, cmd);
}

std::uint64_t percentile(std::vector<std::uint64_t> v, unsigned per_mille) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const std::size_t i = std::min(v.size() - 1, (v.size() * per_mille + 999) / 1000 - (per_mille == 0 ? 0 : 1));
  return v[i];
}

// The report to --out, or (`print`) to standard output without it.
bool write_out(const std::optional<std::string>& out, const en::proto::Expected<std::string, report::Problem>& text,
               const char* cmd, bool print = true) {
  if (!text) {
    std::fprintf(stderr, "%s: the report was refused by its writer: %s\n", cmd, report::describe(text.error()).c_str());
    return false;
  }
  if (!out) {
    if (print) std::fputs(text->c_str(), stdout);
    return true;
  }
  std::ofstream f(*out + ".part", std::ios::binary | std::ios::trunc);
  f << *text;
  f.close();
  std::error_code ec;
  std::filesystem::rename(*out + ".part", *out, ec);
  if (!f || ec) {
    std::fprintf(stderr, "%s: the report cannot be written\n", cmd);
    return false;
  }
  return true;
}

// ---- rec-stats ----

struct Stats {
  RecordedLine::Kind first = RecordedLine::Kind::Header;
  en::shell::RecordingHeader header;
  bool have_header = false;
  std::uint64_t files = 0;
  std::uint64_t lines = 0;
  std::array<std::uint64_t, 7> kinds{};
  std::uint64_t t_first = 0, t_last = 0;
  std::uint64_t snaps = 0, gaps = 0, late = 0, stalls = 0, interval_max = 0, chars_max = 0, flag_frames = 0;
  std::uint64_t snap_first = 0, snap_last = 0;
  std::vector<std::uint64_t> build_us;
  std::uint64_t p99_last = 0, p99_max = 0;
  std::array<std::uint64_t, en::proto::kGroupCount> faults{};
  std::array<std::uint64_t, en::proto::kGroupCount> ok{};
  std::array<bool, en::proto::kGroupCount> tripped{};
  std::uint64_t data_ready = 0, playthrough_ready = 0, group_tripped = 0;
  // The groups the reading started with (the first DataReady's caps; every group when the recording has none): a
  // group that was never on (discovery did not find it) shows as tripped in every snapshot, and is no trip of the
  // session's.
  std::uint16_t live = 0xFFFF;
  bool live_known = false;
  std::array<std::uint64_t, 6> changes{};
  std::uint64_t opened = 0, closed = 0, open_ms = 0;
  std::optional<std::uint64_t> open_since;
  std::array<std::uint64_t, kBanners.size()> banners{};
  std::uint64_t commands = 0, marks = 0;
};

Stats stats_of(const std::vector<RecordedLine>& lines) {
  Stats s;
  std::optional<std::uint64_t> prev_snap;
  for (const RecordedLine& l : lines) {
    ++s.lines;
    ++s.kinds[static_cast<std::size_t>(l.kind)];
    if (s.lines == 1) s.t_first = l.t;
    s.t_last = std::max(s.t_last, l.t);
    switch (l.kind) {
      case RecordedLine::Kind::Header:
        ++s.files;
        if (!s.have_header) s.header = l.header;
        s.have_header = true;
        break;
      case RecordedLine::Kind::Snap: {
        const en::proto::WorldSnapshot& w = *l.snap;
        ++s.snaps;
        if (s.snaps == 1) s.snap_first = w.mono_ms;
        s.snap_last = w.mono_ms;
        s.gaps += w.gap ? 1 : 0;
        if (prev_snap && w.mono_ms >= *prev_snap) {
          const std::uint64_t d = w.mono_ms - *prev_snap;
          s.interval_max = std::max(s.interval_max, d);
          s.late += d > 1000 ? 1 : 0;
          s.stalls += d > 5000 ? 1 : 0;
        }
        prev_snap = w.mono_ms;
        s.chars_max = std::max<std::uint64_t>(s.chars_max, l.chars);
        s.flag_frames += l.flag_frame ? 1 : 0;
        s.build_us.push_back(w.health.task_us_last);
        s.p99_last = w.health.task_us_p99;
        s.p99_max = std::max<std::uint64_t>(s.p99_max, w.health.task_us_p99);
        for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
          s.faults[g] += w.health.groups[g].faults;
          s.ok[g] += w.health.groups[g].state == en::proto::GroupState::Ok ? 1 : 0;
          s.tripped[g] = s.tripped[g] || w.health.groups[g].state == en::proto::GroupState::Tripped;
        }
        break;
      }
      case RecordedLine::Kind::Life:
        if (const auto* d = std::get_if<en::proto::Lifecycle::DataReady>(&l.life.payload)) {
          ++s.data_ready;
          if (!s.live_known) s.live = static_cast<std::uint16_t>(d->caps.groups);
          s.live_known = true;
        }
        if (std::holds_alternative<en::proto::Lifecycle::PlaythroughReady>(l.life.payload)) ++s.playthrough_ready;
        if (const auto* g = std::get_if<en::proto::Lifecycle::GroupTripped>(&l.life.payload)) {
          ++s.group_tripped;
          s.tripped[static_cast<std::size_t>(g->group)] = true;
        }
        break;
      case RecordedLine::Kind::Banner:
        ++s.banners[static_cast<std::size_t>(l.banner)];
        break;
      case RecordedLine::Kind::Change:
        ++s.changes[static_cast<std::size_t>(l.change.what)];
        if (l.change.what == en::core::Change::What::GateOpened) {
          ++s.opened;
          s.open_since = l.change.at;
        } else if (l.change.what == en::core::Change::What::GateClosed) {
          ++s.closed;
          if (s.open_since && l.change.at >= *s.open_since) s.open_ms += l.change.at - *s.open_since;
          s.open_since.reset();
        }
        break;
      case RecordedLine::Kind::Cmd:
        ++s.commands;
        break;
      case RecordedLine::Kind::Mark:
        ++s.marks;
        break;
    }
  }
  if (s.open_since && s.snap_last >= *s.open_since) s.open_ms += s.snap_last - *s.open_since;  // open at the end
  return s;
}

void write_stats_body(report::Writer& w, const Stats& s) {
  w.begin_object("header");
  w.begin_array("game_version");
  for (const auto v : s.header.game_version) w.number(v);
  w.end_array();
  w.begin_array("en_version");
  for (const auto v : s.header.en_version) w.number(v);
  w.end_array();
  w.word("region", s.header.region);
  w.word("spike", s.header.spike);
  w.boolean("probe_build", s.header.probe_build);
  w.number("files", s.files);
  w.end_object();
  w.number("lines", s.lines);
  w.begin_array("kinds");
  for (std::size_t k = 0; k < s.kinds.size(); ++k) {
    w.begin_object();
    w.word("kind", kKinds[k]);
    w.number("count", s.kinds[k]);
    w.end_object();
  }
  w.end_array();
  const std::uint64_t span = s.snaps > 1 ? s.snap_last - s.snap_first : 0;
  w.number("duration_ms", s.t_last >= s.t_first ? s.t_last - s.t_first : 0);
  w.begin_object("snapshots");
  w.number("count", s.snaps);
  w.number("gaps", s.gaps);
  w.number("late", s.late);
  w.number("stalls", s.stalls);
  w.number("interval_max_ms", s.interval_max);
  w.number("rate_mhz", span == 0 ? 0 : (s.snaps - 1) * 1'000'000 / span);
  w.number("chars_max", s.chars_max);
  w.number("flag_frames", s.flag_frames);
  w.end_object();
  w.begin_object("task");
  w.number("build_us_p50", percentile(s.build_us, 500));
  w.number("build_us_p99", percentile(s.build_us, 990));
  w.number("build_us_max", percentile(s.build_us, 1000));
  w.number("p99_us_last", s.p99_last);
  w.number("p99_us_max", s.p99_max);
  w.end_object();
  w.begin_array("groups");
  std::uint64_t total = 0;
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    total += s.faults[g];
    w.begin_object();
    w.word("group", kGroups[g]);
    w.number("faults", s.faults[g]);
    w.boolean("tripped", s.tripped[g]);
    w.number("ok", s.ok[g]);
    w.end_object();
  }
  w.end_array();
  w.number("faults", total);
  w.begin_object("life");
  w.number("data_ready", s.data_ready);
  w.number("playthrough_ready", s.playthrough_ready);
  w.number("group_tripped", s.group_tripped);
  w.end_object();
  w.begin_array("changes");
  for (std::size_t c = 0; c < s.changes.size(); ++c) {
    w.begin_object();
    w.word("what", kChanges[c]);
    w.number("count", s.changes[c]);
    w.end_object();
  }
  w.end_array();
  w.begin_object("gate");
  w.number("opened", s.opened);
  w.number("closed", s.closed);
  w.number("open_ms", s.open_ms);
  w.end_object();
  w.begin_array("banners");
  for (std::size_t b = 0; b < s.banners.size(); ++b) {
    if (s.banners[b] == 0) continue;
    w.begin_object();
    w.word("reason", kBanners[b]);
    w.number("count", s.banners[b]);
    w.end_object();
  }
  w.end_array();
  w.number("commands", s.commands);
  w.number("marks", s.marks);
}

// The groups the session tripped: of those it started with (S0-B's no_trip, S0-D's breaker), as Stats::live says.
std::uint64_t session_trips(const Stats& st) {
  std::uint64_t trips = 0;
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) trips += st.tripped[g] && ((st.live >> g) & 1u) != 0 ? 1 : 0;
  return trips;
}

struct Common {
  std::vector<std::string> positional;
  std::vector<std::string> logs;
  std::vector<std::string> recordings;
  std::vector<std::string> runs;           // spike-report: eldennet.dll's reports of runs (eldennet-spike.json)
  std::optional<std::string> spikes_dir;   // spike-report: the spikes folder, its logs and its runs' folders
  std::optional<std::string> recordings_dir;  // spike-report: the recordings folder, each joined to its run
  std::optional<std::string> out;
  std::optional<std::string> events;  // replay: Augur's events, in the golden files' form
  std::uint32_t need = 20;
  std::uint32_t session_s = 7200;  // spike-report s0b: the session's length to reach (DESIGN §16.1: two hours)
};

std::optional<Common> parse(int argc, char** argv, const char* cmd, bool spike) {
  Common c;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--out" && more && !c.out) {
      c.out = argv[++i];
    } else if (!spike && a == "--events" && more && !c.events && std::string_view(cmd) == "replay") {
      c.events = argv[++i];
    } else if (spike && a == "--log" && more) {
      c.logs.emplace_back(argv[++i]);
    } else if (spike && a == "--recording" && more) {
      c.recordings.emplace_back(argv[++i]);
    } else if (spike && a == "--run" && more) {
      c.runs.emplace_back(argv[++i]);
    } else if (spike && a == "--spikes" && more && !c.spikes_dir) {
      c.spikes_dir = argv[++i];
    } else if (spike && a == "--recordings" && more && !c.recordings_dir) {
      c.recordings_dir = argv[++i];
    } else if (spike && a == "--session-s" && more) {
      const std::string v = argv[++i];
      if (v.empty() || v.size() > 5 || v.find_first_not_of("0123456789") != std::string::npos || std::stoul(v) == 0) {
        std::fprintf(stderr, "%s: --session-s is 1 to 99999 seconds\n", cmd);
        return std::nullopt;
      }
      c.session_s = static_cast<std::uint32_t>(std::stoul(v));
    } else if (spike && a == "--need" && more) {
      const std::string v = argv[++i];
      if (v.empty() || v.size() > 4 || v.find_first_not_of("0123456789") != std::string::npos || std::stoul(v) == 0) {
        std::fprintf(stderr, "%s: --need is 1 to 9999 runs\n", cmd);
        return std::nullopt;
      }
      c.need = static_cast<std::uint32_t>(std::stoul(v));
    } else if (a.rfind("--", 0) == 0) {
      std::fprintf(stderr, "%s: an unknown option, or one without its value\n", cmd);
      return std::nullopt;
    } else {
      c.positional.push_back(a);
    }
  }
  return c;
}

}  // namespace

int run_rec_stats(int argc, char** argv) {
  const auto c = parse(argc, argv, "rec-stats", false);
  if (!c || c->positional.size() != 1) {
    std::fputs("usage: eldennet-engine rec-stats <recording> [--out FILE]\n", stderr);
    return 2;
  }
  const auto lines = load_recording(c->positional[0], "rec-stats");
  if (!lines) return 2;
  const Stats s = stats_of(*lines);
  report::Writer w(rec_stats_vocabulary());
  write_stats_body(w, s);
  return write_out(c->out, w.finish(), "rec-stats") ? 0 : 5;
}

// ---- replay ----

int run_replay(int argc, char** argv) {
  const auto c = parse(argc, argv, "replay", false);
  if (!c || c->positional.size() != 1) {
    std::fputs("usage: eldennet-engine replay <recording> [--out FILE] [--events FILE]\n", stderr);
    return 2;
  }
  const auto lines = load_recording(c->positional[0], "replay");
  if (!lines) return 2;
  en::core::Core core;
  std::vector<en::core::Change> recorded;
  std::vector<en::core::Change> replayed;
  std::array<std::uint64_t, 7> entered{};
  std::uint64_t snaps = 0, life = 0, banners = 0;
  std::uint64_t now = 0;
  en::core::State last;
  en::tools::Run augur_run;  // Augur's events, which en-core's step answers too (part 11)
  if (core.augur().rules()) augur_run.rules_hash = core.augur().rules()->hash();
  std::array<std::uint64_t, en::augur::kKindCount> by_kind{};
  const auto step = [&](en::core::CoreInput in) {
    en::core::Outputs out = core.step(std::move(in), now);
    last = out.state;
    for (const en::core::Change& ch : out.changes) {
      replayed.push_back(ch);
      if (ch.what == en::core::Change::What::Phase && ch.to < entered.size()) ++entered[ch.to];
    }
    for (en::augur::Event& e : out.events) {
      ++by_kind[e.kind.index()];
      augur_run.events.push_back(std::move(e));
    }
  };
  for (const RecordedLine& l : *lines) {
    now = std::max(now, l.t);  // a snapshot's line carries its own time, the others the core's step's
    switch (l.kind) {
      case RecordedLine::Kind::Snap:
        ++snaps;
        step(en::core::CoreInput{std::make_unique<en::proto::WorldSnapshot>(*l.snap)});
        break;
      case RecordedLine::Kind::Life:
        ++life;
        step(en::core::CoreInput{l.life});
        break;
      case RecordedLine::Kind::Banner:
        ++banners;
        step(en::core::CoreInput{en::core::BootBanner{l.banner, {}}});
        break;
      case RecordedLine::Kind::Change:
        recorded.push_back(l.change);
        break;
      default:
        break;
    }
  }
  // The same changes, in the same order (what, from and to; their times are the step's, which a replay cannot
  // reproduce to the millisecond).
  std::optional<std::size_t> differs;
  for (std::size_t i = 0; i < std::max(recorded.size(), replayed.size()) && !differs; ++i) {
    if (i >= recorded.size() || i >= replayed.size() || recorded[i].what != replayed[i].what ||
        recorded[i].from != replayed[i].from || recorded[i].to != replayed[i].to) {
      differs = i;
    }
  }
  report::Writer r(replay_vocabulary());
  r.begin_array("kinds");
  for (const auto& [kind, count] : {std::pair<std::string_view, std::uint64_t>{"snap", snaps}, {"life", life},
                                    {"banner", banners}}) {
    r.begin_object();
    r.word("kind", kind);
    r.number("count", count);
    r.end_object();
  }
  r.end_array();
  r.number("recorded", recorded.size());
  r.number("replayed", replayed.size());
  r.boolean("agree", !differs);
  if (differs) r.number("first_difference", *differs);
  r.begin_array("phases");
  for (std::size_t p = 0; p < entered.size(); ++p) {
    r.begin_object();
    r.word("phase", kPhases[p]);
    r.number("entered", entered[p]);
    r.end_object();
  }
  r.end_array();
  r.number("events", augur_run.events.size());
  r.begin_array("event_kinds");
  for (std::size_t k = 0; k < by_kind.size(); ++k) {
    if (by_kind[k] == 0) continue;
    r.begin_object();
    r.word("event", en::augur::kKindNames[k]);
    r.number("count", by_kind[k]);
    r.end_object();
  }
  r.end_array();
  r.begin_object("final");
  r.word("phase", kPhases[static_cast<std::size_t>(last.phase)]);
  r.word("level", kLevels[static_cast<std::size_t>(last.level)]);
  r.boolean("gate_open", last.gate_open);
  r.number("playthroughs", last.playthroughs);
  r.end_object();
  if (!write_out(c->out, r.finish(), "replay")) return 5;
  if (c->events) {
    // The golden files' form (handles h1, h2, ...), so that a recording's events can become a route's golden file.
    std::ofstream f(*c->events, std::ios::binary | std::ios::trunc);
    if (!f || !(f << en::tools::golden_text("recording", "recording", augur_run))) {
      std::fputs("replay: the events file cannot be written\n", stderr);
      return 2;
    }
  }
  std::fprintf(stderr, "replay: %s: %zu changes recorded, %zu replayed, %zu events\n", differs ? "disagrees" : "agrees",
               recorded.size(), replayed.size(), augur_run.events.size());
  return differs ? 1 : 0;
}

// ---- spike-report ----

namespace {

enum class Status : std::uint8_t { Pass, Fail, NotMeasured };

struct Rule {
  std::string_view rule;
  Status status = Status::NotMeasured;
  std::optional<std::uint64_t> value;
  std::optional<std::uint64_t> limit;
  std::optional<std::string_view> mode;
};

Rule at_least(std::string_view rule, std::uint64_t value, std::uint64_t limit) {
  return Rule{rule, value >= limit ? Status::Pass : Status::Fail, value, limit, {}};
}
Rule at_most(std::string_view rule, std::uint64_t value, std::uint64_t limit) {
  return Rule{rule, value <= limit ? Status::Pass : Status::Fail, value, limit, {}};
}
Rule unmeasured(std::string_view rule) { return Rule{rule, Status::NotMeasured, {}, {}, {}}; }

// One run of a spike log, as the launcher writes it (en-launcher's record.cpp: to_json(RunResult)).
struct Run {
  std::string_view mode = "unknown";
  std::string outcome;
  std::string end;
  bool record_kept = false;
  bool crash_filter = false;
  std::uint64_t items_resolved = 0, items_total = 0, passes = 0, ready_ms = 0, connections_max = 0;
};

struct Logs {
  std::vector<Run> runs;
  std::uint64_t files = 0;
};

std::string_view mode_word(const std::string& m) {
  for (const std::string_view w : kModes) {
    if (w == m) return w;
  }
  return "unknown";
}

std::optional<Logs> load_logs(const std::vector<std::string>& paths, std::string_view spike) {
  Logs out;
  for (const std::string& path : paths) {
    const auto text = read_file(path, kMaxLogBytes);
    if (!text) {
      std::fputs("spike-report: a spike log cannot be read, or is larger than 16 MiB\n", stderr);
      return std::nullopt;
    }
    bool bad = false;
    each_line(*text, [&](std::string_view line) {
      const Json j = Json::parse(line, nullptr, false);
      if (j.is_discarded() || !j.is_object() || !j.contains("spike") || !j["spike"].is_string()) {
        bad = true;
        return;
      }
      if (j["spike"].get<std::string>() != spike || j.contains("tally")) return;  // another spike's, or the tally
      Run r;
      r.mode = mode_word(j.value("mode", std::string("unknown")));
      r.outcome = j.value("outcome", std::string());
      r.end = j.value("end", std::string());
      r.record_kept = j.value("record_kept", false);
      r.crash_filter = j.value("crash_filter", false);
      r.connections_max = j.value("connections_max", std::uint64_t{0});
      if (const auto st = j.find("status"); st != j.end() && st->is_object()) {
        r.items_resolved = st->value("items_resolved", std::uint64_t{0});
        r.items_total = st->value("items_total", std::uint64_t{0});
        r.passes = st->value("discovery_passes", std::uint64_t{0});
        r.ready_ms = st->value("ready_ms", std::uint64_t{0});
      }
      out.runs.push_back(std::move(r));
    });
    if (bad) {
      std::fputs("spike-report: a spike log holds a line that is not one of the launcher's\n", stderr);
      return std::nullopt;
    }
    ++out.files;
  }
  return out;
}

bool crashed(const Run& r) { return r.end == "crash"; }

struct ModeTally {
  std::string_view mode;
  std::uint64_t runs = 0, ready = 0, banner = 0, inert = 0, crashes = 0, timeouts = 0, other = 0;
  std::uint64_t items_min = 0, items_max = 0, items_total = 0;
  std::array<std::uint64_t, 5> passes{};  // runs whose discovery took 1, 2, 3, 4 passes (0: none)
  std::vector<std::uint64_t> ready_ms;
};

std::vector<ModeTally> by_mode(const Logs& logs) {
  std::vector<ModeTally> out;
  for (const Run& r : logs.runs) {
    auto it = std::find_if(out.begin(), out.end(), [&](const ModeTally& m) { return m.mode == r.mode; });
    if (it == out.end()) {
      ModeTally fresh;
      fresh.mode = r.mode;
      out.push_back(std::move(fresh));
      it = out.end() - 1;
      it->items_min = r.items_resolved;
    }
    ModeTally& m = *it;
    ++m.runs;
    if (r.outcome == "ready") {
      ++m.ready;
    } else if (r.outcome == "banner") {
      ++m.banner;
    } else if (r.outcome == "inert") {
      ++m.inert;
    } else if (r.outcome == "timeout") {
      ++m.timeouts;
    } else {
      ++m.other;
    }
    m.crashes += crashed(r) ? 1 : 0;
    m.items_min = std::min(m.items_min, r.items_resolved);
    m.items_max = std::max(m.items_max, r.items_resolved);
    m.items_total = std::max(m.items_total, r.items_total);
    ++m.passes[std::min<std::uint64_t>(r.passes, 4)];
    if (r.outcome == "ready") m.ready_ms.push_back(r.ready_ms);
  }
  return out;
}

bool solo(const en::proto::WorldSnapshot& s) {
  return s.session.role == en::proto::MultiplayRole::Host && s.session.is_my_world &&
         s.session.lobby == en::proto::LobbyState::None && s.session.player_set_len == 1 && !s.session.anticheat;
}

// The gate open on a snapshot that is not solo and not closed before the next one: the core closes the gate on the
// first such snapshot (C4), so its change follows that snapshot's line in the recording.
std::uint64_t gate_violations(const std::vector<RecordedLine>& lines) {
  bool open = false;
  bool pending = false;
  std::uint64_t violations = 0;
  for (const RecordedLine& l : lines) {
    if (l.kind == RecordedLine::Kind::Change) {
      if (l.change.what == en::core::Change::What::GateOpened) open = true;
      if (l.change.what == en::core::Change::What::GateClosed) {
        open = false;
        pending = false;
      }
    } else if (l.kind == RecordedLine::Kind::Snap) {
      violations += pending ? 1 : 0;
      pending = open && l.snap->player && !solo(*l.snap);
    }
  }
  return violations + (pending ? 1 : 0);
}

void write_rule(report::Writer& w, const Rule& r) {
  w.begin_object();
  w.word("rule", r.rule);
  if (r.mode) w.word("mode", *r.mode);
  w.word("status", kStatuses[static_cast<std::size_t>(r.status)]);
  if (r.value) w.number("value", *r.value);
  if (r.limit) w.number("limit", *r.limit);
  w.end_object();
  std::printf("spike-report: rule=%.*s%s%.*s status=%.*s", static_cast<int>(r.rule.size()), r.rule.data(),
              r.mode ? " mode=" : "", r.mode ? static_cast<int>(r.mode->size()) : 0, r.mode ? r.mode->data() : "",
              static_cast<int>(kStatuses[static_cast<std::size_t>(r.status)].size()),
              kStatuses[static_cast<std::size_t>(r.status)].data());
  if (r.value) std::printf(" value=%llu", static_cast<unsigned long long>(*r.value));
  if (r.limit) std::printf(" limit=%llu", static_cast<unsigned long long>(*r.limit));
  std::printf("\n");
}

// ---- eldennet.dll's reports of the runs (spike_run, en_shell's spikerun.hpp) ----

constexpr std::uintmax_t kMaxRunReportBytes = 64u << 10;

struct RunReport {
  std::string spike;         // the run's word
  std::uint32_t folder = 0;  // spikes\<word>-<folder>; 0 when given by --run
  std::uint64_t pid = 0;
  std::string state = "none", waits = "none";
  std::uint64_t getter_calls = 0;
  std::uint64_t table_items = 0, table_refused = 0, again_refused = 0, changed = 0;
  bool rechecked = false;
  bool trial = false;
  std::uint64_t forced = 0, caught = 0;
  bool inject = false;  // a group was named for the reader's injected faults
  bool reading = false;
  std::uint64_t reading_ms = 0, snapshots = 0, torn = 0, held = 0, unconfirmed = 0, tripped = 0, p99_us = 0;
};

std::uint64_t num_of(const Json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_number_unsigned() ? it->get<std::uint64_t>() : 0;
}
bool flag_of(const Json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_boolean() && it->get<bool>();
}
std::string word_of(const Json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string("none");
}
Json part_of(const Json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_object() ? *it : Json::object();
}

// One report: read, linted against its own vocabulary (it is the DLL's, numbers and words only), then its numbers.
std::optional<RunReport> load_run(const std::string& path, std::uint32_t folder) {
  const auto text = read_file(path, kMaxRunReportBytes);
  if (!text) {
    std::fputs("spike-report: a run's report cannot be read, or is larger than 64 KiB\n", stderr);
    return std::nullopt;
  }
  const std::array<const report::Vocabulary*, 1> known = {&en::shell::spike_run_vocabulary()};
  if (const auto ok = report::lint(*text, known); !ok) {
    std::fputs("spike-report: a run's report is refused by the lint:\n", stderr);
    for (const report::Problem& p : ok.error()) std::fprintf(stderr, "  %s\n", report::describe(p).c_str());
    return std::nullopt;
  }
  const Json j = Json::parse(*text, nullptr, false);
  RunReport r;
  r.folder = folder;
  r.spike = word_of(j, "spike");
  r.pid = num_of(j, "pid");
  r.state = word_of(j, "state");
  r.waits = word_of(j, "waits");
  r.getter_calls = num_of(part_of(j, "discovery"), "getter_calls");
  const Json table = part_of(j, "table");
  r.table_items = num_of(table, "items");
  r.table_refused = num_of(table, "refused");
  r.rechecked = flag_of(table, "rechecked");
  r.again_refused = num_of(table, "again_refused");
  r.changed = num_of(table, "changed");
  const Json trial = part_of(j, "trial");
  r.trial = flag_of(trial, "ran");
  r.forced = num_of(trial, "seh_forced") + num_of(trial, "rpm_forced");
  r.caught = num_of(trial, "seh_caught") + num_of(trial, "rpm_caught");
  r.inject = word_of(part_of(j, "inject"), "group") != "none";
  const Json reading = part_of(j, "reading");
  r.reading = flag_of(reading, "started");
  r.reading_ms = num_of(reading, "ms");
  r.snapshots = num_of(reading, "snapshots");
  r.torn = num_of(reading, "torn");
  r.held = num_of(reading, "held");
  r.unconfirmed = num_of(reading, "unconfirmed");
  r.p99_us = num_of(reading, "p99_us");
  if (const auto it = reading.find("tripped"); it != reading.end() && it->is_array()) r.tripped = it->size();
  return r;
}

// A name `<word>-<n>`, n of 1 to 9 digits: the word and n, or nothing.
std::optional<std::pair<std::string, std::uint32_t>> run_folder_name(const std::string& name) {
  const std::size_t dash = name.rfind('-');
  if (dash == std::string::npos || dash == 0 || dash + 1 >= name.size() || name.size() - dash - 1 > 9) return {};
  const std::string digits = name.substr(dash + 1);
  if (digits.find_first_not_of("0123456789") != std::string::npos) return {};
  const std::string word = name.substr(0, dash);
  if (std::find(kSpikes.begin(), kSpikes.end(), word) == kSpikes.end()) return {};
  return std::make_pair(word, static_cast<std::uint32_t>(std::stoul(digits)));
}

// A recording's name, `<date>-<time>-<pid>-<n>.ensnap.jsonl`: its process id, or nothing.
std::optional<std::uint64_t> recording_pid(const std::string& name) {
  constexpr std::string_view kExt = ".ensnap.jsonl";
  if (name.size() <= kExt.size() || name.compare(name.size() - kExt.size(), kExt.size(), kExt) != 0) return {};
  std::vector<std::string> parts;
  std::size_t at = 0;
  const std::string stem = name.substr(0, name.size() - kExt.size());
  while (at <= stem.size()) {
    const std::size_t dash = std::min(stem.find('-', at), stem.size());
    parts.push_back(stem.substr(at, dash - at));
    at = dash + 1;
  }
  if (parts.size() != 4) return {};
  for (const std::string& p : parts) {
    if (p.empty() || p.size() > 10 || p.find_first_not_of("0123456789") != std::string::npos) return {};
  }
  return std::stoull(parts[2]);
}

}  // namespace

int run_spike_report(int argc, char** argv) {
  auto c = parse(argc, argv, "spike-report", true);
  const auto usage = [] {
    std::fputs("usage: eldennet-engine spike-report s0a|s0b|s0c|s0d [--spikes DIR] [--recordings DIR] [--log FILE]... "
               "[--recording FILE]... [--run FILE]... [--need N] [--session-s S] [--out FILE]\n",
               stderr);
    return 2;
  };
  if (!c || c->positional.size() != 1) return usage();
  const std::string spike = c->positional[0];
  if (spike != "s0a" && spike != "s0b" && spike != "s0c" && spike != "s0d") return usage();
  namespace fs = std::filesystem;

  // The spikes folder whole: its logs (the launcher's, `<word>-<stamp>.jsonl`), and each run's folder with
  // eldennet.dll's report of the run. Sorted, so that the report is the same for the same folder.
  std::vector<std::pair<std::string, std::uint32_t>> run_files;  // a report's path, and its folder's number
  for (const std::string& path : c->runs) run_files.emplace_back(path, 0);
  if (c->spikes_dir) {
    std::error_code ec;
    if (!fs::is_directory(*c->spikes_dir, ec)) {
      std::fputs("spike-report: --spikes names no folder\n", stderr);
      return 2;
    }
    std::vector<fs::directory_entry> entries;
    for (fs::directory_iterator it(*c->spikes_dir, ec), end; !ec && it != end; it.increment(ec)) entries.push_back(*it);
    std::sort(entries.begin(), entries.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) { return a.path() < b.path(); });
    for (const fs::directory_entry& e : entries) {
      const std::string name = e.path().filename().string();
      if (e.is_regular_file(ec) && e.path().extension() == ".jsonl") {
        c->logs.push_back(e.path().string());
      } else if (e.is_directory(ec)) {
        const auto folder = run_folder_name(name);
        const fs::path file = e.path() / "eldennet-spike.json";
        if (folder && (folder->first == spike || (spike == "s0d" && folder->first == "s0d-crash")) &&
            fs::is_regular_file(file, ec)) {
          run_files.emplace_back(file.string(), folder->second);
        }
      }
    }
  }
  std::vector<RunReport> runs;
  for (const auto& [path, folder] : run_files) {
    auto r = load_run(path, folder);
    if (!r) return 2;
    if (r->spike == spike) runs.push_back(std::move(*r));  // S0-D's crashes have their logs, not reports, counted
  }
  // The recordings folder: each recording joined to a run of this spike by the game's process id in its name (and its
  // header's spike, read below); with no report of a run, every recording of this spike.
  if (c->recordings_dir) {
    std::error_code ec;
    std::vector<std::string> names;
    for (fs::directory_iterator it(*c->recordings_dir, ec), end; !ec && it != end; it.increment(ec)) {
      if (it->is_regular_file(ec)) names.push_back(it->path().filename().string());
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
      const auto pid = recording_pid(name);
      if (!pid) continue;
      const bool joined = runs.empty() || std::any_of(runs.begin(), runs.end(), [&](const RunReport& r) {
                            return r.pid == *pid;
                          });
      if (joined) c->recordings.push_back((fs::path(*c->recordings_dir) / name).string());
    }
  }

  // S0-D's forced crashes are a log of their own (--spike s0d-crash); its other logs are the session's.
  const auto logs = load_logs(c->logs, spike);
  const auto crash_logs = spike == "s0d" ? load_logs(c->logs, "s0d-crash") : std::optional<Logs>(Logs{});
  if (!logs || !crash_logs) return 2;
  std::vector<std::vector<RecordedLine>> recordings;
  for (const std::string& path : c->recordings) {
    auto lines = load_recording(path, "spike-report");
    if (!lines) return 2;
    // A recording of another spike's session (the recordings folder holds every session's) is left out.
    if (c->recordings_dir && !lines->empty() && lines->front().kind == RecordedLine::Kind::Header &&
        lines->front().header.spike != spike) {
      continue;
    }
    recordings.push_back(std::move(*lines));
  }
  std::vector<Stats> stats;
  for (const auto& r : recordings) stats.push_back(stats_of(r));

  // What the runs' reports say: the start reached its reading in one of them, or what it waited for.
  const bool any_ready = std::any_of(runs.begin(), runs.end(), [](const RunReport& r) { return r.state == "ready"; });
  std::string_view waits = "none";
  if (!any_ready) {
    for (const RunReport& r : runs) {
      if (r.waits != "none") {
        const auto it = std::find(kWaits.begin(), kWaits.end(), r.waits);
        waits = it != kWaits.end() ? *it : std::string_view("none");
        break;
      }
    }
  }
  const bool waiting = waits != "none";

  std::vector<Rule> rules;
  std::vector<ModeTally> modes;
  std::uint64_t connections = 0;
  for (const Run& r : logs->runs) connections = std::max(connections, r.connections_max);
  const std::uint64_t log_crashes = static_cast<std::uint64_t>(
      std::count_if(logs->runs.begin(), logs->runs.end(), [](const Run& r) { return crashed(r); }));
  const auto session_crash = [&] {
    return logs->runs.empty() ? unmeasured("no_crash") : at_most("no_crash", log_crashes, 0);
  };
  std::uint64_t trial_forced = 0, trial_caught = 0, torn = 0, held = 0, unconfirmed = 0, reading_ms = 0;
  std::uint64_t table_items = 0, table_refused = 0, table_changed = 0;
  bool trial_ran = false;
  for (const RunReport& r : runs) {
    trial_ran = trial_ran || r.trial;
    trial_forced += r.forced;
    trial_caught += r.caught;
    torn += r.torn;
    held += r.held;
    unconfirmed += r.unconfirmed;
    reading_ms = std::max(reading_ms, r.reading_ms);
    table_items = std::max(table_items, r.table_items);
    table_refused += r.table_refused;
    table_changed += r.changed;
  }
  if (spike == "s0a") {
    // DESIGN §16.1: 20/20 launches per mode, else L1 with no crash; early (suspended) and late (the window, the title).
    modes = by_mode(*logs);
    bool early = false, late = false;
    for (const ModeTally& m : modes) {
      early = early || m.mode == "suspended";
      late = late || m.mode == "window" || m.mode == "title";
      Rule n = at_least("runs_per_mode", m.runs, c->need);
      n.mode = m.mode;
      Rule ok = at_least("ready_or_l1", m.ready + m.banner + m.inert, m.runs);
      ok.mode = m.mode;
      Rule nc = at_most("no_crash", m.crashes, 0);
      nc.mode = m.mode;
      rules.insert(rules.end(), {n, ok, nc});
    }
    // Both loads are asked for; a mode the logs do not hold yet is not measured, not failed.
    Rule both = at_least("both_modes", (early ? 1 : 0) + (late ? 1 : 0), 2);
    if (both.status == Status::Fail) both.status = Status::NotMeasured;
    rules.push_back(both);
    if (logs->runs.empty()) rules.push_back(unmeasured("spike_log"));
    // Since 2026-11-22 (proposal 0005 §15): the ids of the build's table, each passing its checks in every run, and the
    // image untouched where the table points (the checks made again once the start stopped agree), with no call into
    // the game. Each run's report gives them; a run with no report counts against them, a layout with no table is
    // not measured.
    const auto tabled = static_cast<std::uint64_t>(
        std::count_if(runs.begin(), runs.end(), [](const RunReport& r) { return r.table_items != 0; }));
    if (tabled == 0) {
      rules.push_back(unmeasured("table_checked"));
      rules.push_back(unmeasured("table_unchanged"));
    } else {
      const std::uint64_t expected = std::max<std::uint64_t>(tabled, logs->runs.size());
      rules.push_back(at_least("table_checked", static_cast<std::uint64_t>(std::count_if(
                                                    runs.begin(), runs.end(),
                                                    [](const RunReport& r) {
                                                      return r.table_items != 0 && r.table_refused == 0;
                                                    })),
                               expected));
      rules.push_back(at_least("table_unchanged", static_cast<std::uint64_t>(std::count_if(
                                                      runs.begin(), runs.end(),
                                                      [](const RunReport& r) {
                                                        return r.table_items != 0 && r.rechecked &&
                                                               r.again_refused == 0 && r.changed == 0;
                                                      })),
                               expected));
    }
    if (runs.empty()) {
      rules.push_back(unmeasured("no_call"));
    } else {
      std::uint64_t calls = 0;
      for (const RunReport& r : runs) calls += r.getter_calls;
      rules.push_back(at_most("no_call", calls, 0));
    }
  } else if (spike == "s0b") {
    // Since 2026-11-22 (O101): the reader on EldenNet's own thread for 2 h; 0 faults confirmed (a fault counts when it
    // comes twice running); p99 ≤ 0.5 ms. It needs a ready start: one that stopped short of its reading for want of
    // something waits for it, and nothing below is measured.
    if (any_ready) {
      rules.push_back(Rule{"ready_start", Status::Pass, 1, 1, {}});
    } else if (waiting) {
      rules.push_back(unmeasured("ready_start"));
    } else if (!runs.empty()) {
      rules.push_back(Rule{"ready_start", Status::Fail, 0, 1, {}});
    } else {
      // Logs and recordings alone (no run's report): a recording whose reader sent DataReady had a ready start.
      std::uint64_t ready = 0;
      for (const Stats& st : stats) ready += st.data_ready;
      rules.push_back(stats.empty() ? unmeasured("ready_start")
                                    : at_least("ready_start", std::min<std::uint64_t>(ready, 1), 1));
    }
    if (waiting) {
      for (const std::string_view rule : {"session_length", "faults", "no_trip", "task_p99_us"}) {
        rules.push_back(unmeasured(rule));
      }
    } else if (!stats.empty()) {
      std::uint64_t duration = 0, faults = 0, trips = 0, p99 = 0;
      for (const Stats& st : stats) {
        duration += st.snaps > 1 ? st.snap_last - st.snap_first : 0;
        for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) faults += st.faults[g];
        trips += session_trips(st);
        p99 = std::max(p99, st.p99_last);
      }
      rules.push_back(at_least("session_length", duration / 1000, c->session_s));
      rules.push_back(at_most("faults", faults, 0));
      rules.push_back(at_most("no_trip", trips, 0));
      rules.push_back(at_most("task_p99_us", p99, 500));
    } else if (any_ready) {
      // The runs' reports alone: the reader's own length, trips and p99; its confirmed faults are the recording's.
      std::uint64_t trips = 0, p99 = 0;
      for (const RunReport& r : runs) {
        trips += r.tripped;
        p99 = std::max(p99, r.p99_us);
      }
      rules.push_back(at_least("session_length", reading_ms / 1000, c->session_s));
      rules.push_back(unmeasured("faults"));
      rules.push_back(at_most("no_trip", trips, 0));
      rules.push_back(at_most("task_p99_us", p99, 500));
    } else {
      rules.push_back(unmeasured("recording"));
    }
    rules.push_back(session_crash());
  } else if (spike == "s0c") {
    // The gate closed in every session but a solo one of one's own; no connection beyond this PC; no online menu. The
    // gate is read from the recordings' snapshots, which a start that stopped short of its reading has none of.
    std::uint64_t snaps = 0;
    for (const Stats& st : stats) snaps += st.snaps;
    if (snaps != 0) {
      std::uint64_t violations = 0;
      for (const auto& r : recordings) violations += gate_violations(r);
      rules.push_back(at_most("gate_closed_unless_solo", violations, 0));
    } else if (waiting) {
      rules.push_back(unmeasured("gate_closed_unless_solo"));
    } else {
      rules.push_back(unmeasured(stats.empty() ? "recording" : "gate_closed_unless_solo"));
    }
    rules.push_back(logs->runs.empty() ? unmeasured("connections") : at_most("connections", connections, 0));
    rules.push_back(session_crash());
    rules.push_back(unmeasured("online_menu"));  // the owner's observation
  } else {
    // 20/20 caught; breakers trip; every forced crash recorded by the watcher. The faults caught are the forced ones of
    // the start's trial (eldennet.dll's own page, by each copy primitive) and the reader's injected ones in the
    // recordings; a breaker trips only on the reader's, so a start that stopped short of its reading waits for it.
    std::uint64_t faults = 0, trips = 0;
    for (const Stats& st : stats) {
      for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) faults += st.faults[g];
      trips += session_trips(st);
    }
    for (const RunReport& r : runs) trips = std::max(trips, r.tripped);
    if (stats.empty() && !trial_ran) {
      rules.push_back(unmeasured("recording"));
    } else {
      rules.push_back(at_least("faults_caught", faults + trial_caught, c->need));
    }
    const bool injected = std::any_of(runs.begin(), runs.end(), [](const RunReport& r) { return r.inject; });
    if (trips != 0) {
      rules.push_back(at_least("breaker_tripped", trips, 1));
    } else if (waiting || (!runs.empty() && !injected)) {
      rules.push_back(unmeasured("breaker_tripped"));
    } else if (!stats.empty() || any_ready) {
      rules.push_back(at_least("breaker_tripped", trips, 1));
    } else {
      rules.push_back(unmeasured("breaker_tripped"));
    }
    rules.push_back(session_crash());
    if (crash_logs->runs.empty()) {
      rules.push_back(unmeasured("crashes_recorded"));
    } else {
      const auto recorded = static_cast<std::uint64_t>(std::count_if(
          crash_logs->runs.begin(), crash_logs->runs.end(), [](const Run& r) { return crashed(r) && r.record_kept; }));
      rules.push_back(at_least("crashes_recorded", recorded, crash_logs->runs.size()));
    }
  }
  bool failed = false, unmeasured_rule = false;
  for (const Rule& r : rules) {
    failed = failed || r.status == Status::Fail;
    unmeasured_rule = unmeasured_rule || (r.status == Status::NotMeasured && r.rule != "online_menu");
  }
  const std::string_view verdict = failed ? "fail" : unmeasured_rule ? "incomplete" : "pass";

  report::Writer w(spike_vocabulary());
  w.word("spike", spike);
  w.word("verdict", verdict);
  w.word("waits", waits);
  w.number("logs", logs->files);
  w.number("recordings", recordings.size());
  w.number("run_reports", runs.size());
  w.begin_array("rules");
  for (const Rule& r : rules) write_rule(w, r);
  w.end_array();
  if (!modes.empty()) {
    w.begin_array("modes");
    for (const ModeTally& m : modes) {
      w.begin_object();
      w.word("mode", m.mode);
      w.number("runs", m.runs);
      w.number("ready", m.ready);
      w.number("banner", m.banner);
      w.number("inert", m.inert);
      w.number("crashes", m.crashes);
      w.number("timeouts", m.timeouts);
      w.number("other", m.other);
      w.number("items_min", m.items_min);
      w.number("items_max", m.items_max);
      w.number("items_total", m.items_total);
      w.begin_array("passes");
      for (const std::uint64_t p : m.passes) w.number(p);
      w.end_array();
      w.number("ready_ms_p50", percentile(m.ready_ms, 500));
      w.number("ready_ms_max", percentile(m.ready_ms, 1000));
      w.end_object();
    }
    w.end_array();
  }
  if (!runs.empty()) {
    w.number("table_items", table_items);
    w.number("table_refused", table_refused);
    w.number("table_changed", table_changed);
  }
  if (spike != "s0a") {
    w.number("sessions", logs->runs.size());
    w.number("connections_max", connections);
  }
  if (spike == "s0b" && !runs.empty()) {
    w.number("reading_ms", reading_ms);
    w.begin_object("steady");
    w.number("torn", torn);
    w.number("held", held);
    w.number("unconfirmed", unconfirmed);
    w.end_object();
  }
  if (spike == "s0d") {
    w.number("crashes", static_cast<std::uint64_t>(crash_logs->runs.size()));
    w.number("records", static_cast<std::uint64_t>(std::count_if(crash_logs->runs.begin(), crash_logs->runs.end(),
                                                                 [](const Run& r) { return r.record_kept; })));
    w.number("filter_records", static_cast<std::uint64_t>(std::count_if(
                                   crash_logs->runs.begin(), crash_logs->runs.end(),
                                   [](const Run& r) { return r.crash_filter; })));
    w.begin_object("trial");
    w.number("forced", trial_forced);
    w.number("caught", trial_caught);
    w.end_object();
  }
  if (!stats.empty()) {
    // The gate's timeline over every recording (S0-C), and the first recording's numbers in full (S0-B's session; the
    // rules above cover every recording).
    std::uint64_t opened = 0, closed = 0, open_ms = 0;
    for (const Stats& st : stats) {
      opened += st.opened;
      closed += st.closed;
      open_ms += st.open_ms;
    }
    w.begin_object("gate");
    w.number("opened", opened);
    w.number("closed", closed);
    w.number("open_ms", open_ms);
    w.end_object();
    w.begin_object("snapshots");
    w.number("count", stats.front().snaps);
    w.number("gaps", stats.front().gaps);
    w.number("late", stats.front().late);
    w.number("stalls", stats.front().stalls);
    w.number("interval_max_ms", stats.front().interval_max);
    w.end_object();
    w.begin_object("task");
    w.number("build_us_p50", percentile(stats.front().build_us, 500));
    w.number("build_us_p99", percentile(stats.front().build_us, 990));
    w.number("build_us_max", percentile(stats.front().build_us, 1000));
    w.number("p99_us_last", stats.front().p99_last);
    w.number("p99_us_max", stats.front().p99_max);
    w.end_object();
  }
  if (!write_out(c->out, w.finish(), "spike-report", false)) return 5;
  if (waiting) {
    // The owner's words: why the spike could not run whole, which is never counted against it.
    const char* why = waits == "layout"       ? "the start's layout is not complete yet"
                      : waits == "files"      ? "gal's files are missing or were refused"
                      : waits == "build"      ? "the game's build is not one EldenNet knows"
                      : waits == "anticheat"  ? "anti-cheat was running"
                                              : "the game did not finish starting";
    std::printf("spike-report: %s waits=%.*s: %s, so what needs EldenNet's reading was not measured; nothing is "
                "counted against it\n",
                spike.c_str(), static_cast<int>(waits.size()), waits.data(), why);
  }
  std::printf("spike-report: %s verdict=%.*s rules=%zu runs=%zu\n", spike.c_str(), static_cast<int>(verdict.size()),
              verdict.data(), rules.size(), runs.size());
  return verdict == "pass" ? 0 : 1;
}
