// eldennet-engine rec-stats, replay and spike-report: the first in-game build's reports and recording tools (the plan's
// part 9). Each reads files the owner's PC wrote (eldennet.dll's recordings, the launcher's spike logs, both numbers
// and fixed words only) and writes a numbers-only report through en-gal's writer, which `lint --report` checks.
//
//   eldennet-engine rec-stats <recording> [--out FILE]
//   eldennet-engine replay <recording> [--out FILE]
//   eldennet-engine spike-report s0a|s0b|s0c|s0d [--log FILE]... [--recording FILE]... [--need N] [--out FILE]
//
// - rec-stats: what a recording holds (README.md lists it): its lines by kind, its length, the snapshots' rate, gaps,
//   late and stalled intervals, the task's build times and its C13 p99, the faults and trips by group, the lifecycle
//   messages, the core's changes, the gate's time open, the banners, the commands and the Marks.
// - replay: the recording's own inputs (its lifecycle messages, banners and snapshots, in their order) stepped through
//   en-core's v0.1 slice again, and the changes it answers compared with those the recording holds: the same core on
//   the same inputs must answer the same, or the core or the recorder has a defect.
// - spike-report: DESIGN §16.1's pass rules for S0-A to S0-D over the launcher's spike logs and the recordings, each
//   rule `pass`, `fail` or `not_measured` (what no file measures, such as "no online menu", the owner's to observe),
//   and the spike's verdict: `pass` when every measured rule passes, `incomplete` when one could not be measured from
//   the files given, else `fail`.
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
#include "en/shell/record.hpp"

namespace {

namespace report = en::gal::report;
using report::Key;
using report::Type;
using en::shell::RecordedLine;
using Json = nlohmann::json;

constexpr std::uintmax_t kMaxRecordingBytes = 512ull << 20;
constexpr std::uintmax_t kMaxLogBytes = 16ull << 20;

// ---- The reports' vocabulary: one key table, three kinds ----

constexpr std::array<std::string_view, 7> kKinds = {"header", "snap", "life", "banner", "change", "cmd", "mark"};
constexpr std::array<std::string_view, 4> kRegions = {"ww", "jp", "other", "none"};
constexpr std::array<std::string_view, 6> kSpikes = {"none", "s0a", "s0b", "s0c", "s0d", "s0d-crash"};
constexpr std::array<std::string_view, 11> kGroups = {"session", "world", "combat", "player", "flags", "text",
                                                      "ui",      "place", "camera", "params", "status"};
constexpr std::array<std::string_view, 6> kChanges = {"phase", "gate_opened", "gate_closed", "level", "banner", "badge"};
constexpr std::array<std::string_view, 8> kBanners = {"unknown_build",    "anticheat",        "not_started",
                                                      "discovery_failed", "register_faulted", "inert",
                                                      "session_failed",   "task_tripped"};
constexpr std::array<std::string_view, 7> kPhases = {"gate",       "banner",   "ready",  "title",
                                                     "playthrough", "in_world", "loading"};
constexpr std::array<std::string_view, 3> kLevels = {"starting", "l1", "l2"};
constexpr std::array<std::string_view, 3> kVerdicts = {"pass", "fail", "incomplete"};
constexpr std::array<std::string_view, 3> kStatuses = {"pass", "fail", "not_measured"};
constexpr std::array<std::string_view, 6> kModes = {"suspended", "window", "title", "watch", "vanilla", "unknown"};
constexpr std::array<std::string_view, 16> kRules = {
    "runs_per_mode", "ready_or_l1",   "no_crash",           "both_modes",        "session_length",
    "faults",        "no_trip",       "task_p99_us",        "gate_closed_unless_solo", "connections",
    "online_menu",   "faults_caught", "breaker_tripped",    "crashes_recorded",  "recording",
    "spike_log"};

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
constexpr std::array<Key, 10> kMoreKeys = {{
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
const report::Vocabulary& spike_vocabulary() {
  static const report::Vocabulary v{"spike", 1, keys()};
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

// A recording, linted first (the same check as lint --recording), then read back line by line.
std::optional<std::vector<RecordedLine>> load_recording(const std::string& path, const char* cmd) {
  const auto text = read_file(path, kMaxRecordingBytes);
  if (!text) {
    std::fprintf(stderr, "%s: a recording cannot be read, or is larger than 512 MiB\n", cmd);
    return std::nullopt;
  }
  const report::Vocabulary* kinds[] = {&en::shell::recording_vocabulary()};
  if (const auto linted = report::lint_lines(*text, kinds); !linted) {
    for (const auto& p : linted.error()) std::fprintf(stderr, "%s: %s\n", cmd, report::describe(p).c_str());
    std::fprintf(stderr, "%s: the recording is refused by the lint\n", cmd);
    return std::nullopt;
  }
  std::vector<RecordedLine> out;
  bool bad = false;
  each_line(*text, [&](std::string_view line) {
    auto r = en::shell::read_recorded_line(line);
    if (!r) {
      bad = true;
      return;
    }
    out.push_back(std::move(*r));
  });
  if (bad) {
    std::fprintf(stderr, "%s: a line of the recording could not be read back\n", cmd);
    return std::nullopt;
  }
  return out;
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
  std::array<std::uint64_t, 6> changes{};
  std::uint64_t opened = 0, closed = 0, open_ms = 0;
  std::optional<std::uint64_t> open_since;
  std::array<std::uint64_t, 8> banners{};
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
        if (std::holds_alternative<en::proto::Lifecycle::DataReady>(l.life.payload)) ++s.data_ready;
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

struct Common {
  std::vector<std::string> positional;
  std::vector<std::string> logs;
  std::vector<std::string> recordings;
  std::optional<std::string> out;
  std::uint32_t need = 20;
};

std::optional<Common> parse(int argc, char** argv, const char* cmd, bool spike) {
  Common c;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--out" && more && !c.out) {
      c.out = argv[++i];
    } else if (spike && a == "--log" && more) {
      c.logs.emplace_back(argv[++i]);
    } else if (spike && a == "--recording" && more) {
      c.recordings.emplace_back(argv[++i]);
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
    std::fputs("usage: eldennet-engine replay <recording> [--out FILE]\n", stderr);
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
  const auto step = [&](en::core::CoreInput in) {
    en::core::Outputs out = core.step(std::move(in), now);
    last = out.state;
    for (const en::core::Change& ch : out.changes) {
      replayed.push_back(ch);
      if (ch.what == en::core::Change::What::Phase && ch.to < entered.size()) ++entered[ch.to];
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
  r.begin_object("final");
  r.word("phase", kPhases[static_cast<std::size_t>(last.phase)]);
  r.word("level", kLevels[static_cast<std::size_t>(last.level)]);
  r.boolean("gate_open", last.gate_open);
  r.number("playthroughs", last.playthroughs);
  r.end_object();
  if (!write_out(c->out, r.finish(), "replay")) return 5;
  std::fprintf(stderr, "replay: %s: %zu changes recorded, %zu replayed\n", differs ? "disagrees" : "agrees",
               recorded.size(), replayed.size());
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

}  // namespace

int run_spike_report(int argc, char** argv) {
  const auto c = parse(argc, argv, "spike-report", true);
  const auto usage = [] {
    std::fputs("usage: eldennet-engine spike-report s0a|s0b|s0c|s0d [--log FILE]... [--recording FILE]... "
               "[--need N] [--out FILE]\n",
               stderr);
    return 2;
  };
  if (!c || c->positional.size() != 1) return usage();
  const std::string spike = c->positional[0];
  if (spike != "s0a" && spike != "s0b" && spike != "s0c" && spike != "s0d") return usage();
  // S0-D's forced crashes are a log of their own (--spike s0d-crash); its other logs are the session's.
  const auto logs = load_logs(c->logs, spike);
  const auto crash_logs = spike == "s0d" ? load_logs(c->logs, "s0d-crash") : std::optional<Logs>(Logs{});
  if (!logs || !crash_logs) return 2;
  std::vector<std::vector<RecordedLine>> recordings;
  for (const std::string& path : c->recordings) {
    auto lines = load_recording(path, "spike-report");
    if (!lines) return 2;
    recordings.push_back(std::move(*lines));
  }
  std::vector<Stats> stats;
  for (const auto& r : recordings) stats.push_back(stats_of(r));

  std::vector<Rule> rules;
  std::vector<ModeTally> modes;
  std::uint64_t connections = 0;
  for (const Run& r : logs->runs) connections = std::max(connections, r.connections_max);
  const std::uint64_t log_crashes = static_cast<std::uint64_t>(
      std::count_if(logs->runs.begin(), logs->runs.end(), [](const Run& r) { return crashed(r); }));
  const auto session_crash = [&] {
    return logs->runs.empty() ? unmeasured("no_crash") : at_most("no_crash", log_crashes, 0);
  };
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
  } else if (spike == "s0b") {
    // 0 faults; p99 ≤ 0.5 ms; over two hours of play.
    if (stats.empty()) {
      rules.push_back(unmeasured("recording"));
    } else {
      std::uint64_t duration = 0, faults = 0, trips = 0, p99 = 0;
      for (const Stats& s : stats) {
        duration += s.snaps > 1 ? s.snap_last - s.snap_first : 0;
        for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
          faults += s.faults[g];
          trips += s.tripped[g] ? 1 : 0;
        }
        p99 = std::max(p99, s.p99_last);
      }
      rules.push_back(at_least("session_length", duration / 1000, 7200));
      rules.push_back(at_most("faults", faults, 0));
      rules.push_back(at_most("no_trip", trips, 0));
      rules.push_back(at_most("task_p99_us", p99, 500));
    }
    rules.push_back(session_crash());
  } else if (spike == "s0c") {
    // The gate closed in every session but a solo one of one's own; no connection beyond this PC; no online menu.
    if (stats.empty()) {
      rules.push_back(unmeasured("recording"));
    } else {
      std::uint64_t violations = 0;
      for (const auto& r : recordings) violations += gate_violations(r);
      rules.push_back(at_most("gate_closed_unless_solo", violations, 0));
    }
    rules.push_back(logs->runs.empty() ? unmeasured("connections") : at_most("connections", connections, 0));
    rules.push_back(session_crash());
    rules.push_back(unmeasured("online_menu"));  // the owner's observation
  } else {
    // 20/20 caught; breakers trip; every forced crash recorded by the watcher.
    if (stats.empty()) {
      rules.push_back(unmeasured("recording"));
    } else {
      std::uint64_t faults = 0, trips = 0;
      for (const Stats& s : stats) {
        for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
          faults += s.faults[g];
          trips += s.tripped[g] ? 1 : 0;
        }
      }
      rules.push_back(at_least("faults_caught", faults, c->need));
      rules.push_back(at_least("breaker_tripped", trips, 1));
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
  w.number("logs", logs->files);
  w.number("recordings", recordings.size());
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
  if (spike != "s0a") {
    w.number("sessions", logs->runs.size());
    w.number("connections_max", connections);
  }
  if (spike == "s0d") {
    w.number("crashes", static_cast<std::uint64_t>(crash_logs->runs.size()));
    w.number("records", static_cast<std::uint64_t>(std::count_if(crash_logs->runs.begin(), crash_logs->runs.end(),
                                                                 [](const Run& r) { return r.record_kept; })));
    w.number("filter_records", static_cast<std::uint64_t>(std::count_if(
                                   crash_logs->runs.begin(), crash_logs->runs.end(),
                                   [](const Run& r) { return r.crash_filter; })));
  }
  if (!stats.empty()) {
    // The gate's timeline over every recording (S0-C), and the first recording's numbers in full (S0-B's session; the
    // rules above cover every recording).
    std::uint64_t opened = 0, closed = 0, open_ms = 0;
    for (const Stats& s : stats) {
      opened += s.opened;
      closed += s.closed;
      open_ms += s.open_ms;
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
  std::printf("spike-report: %s verdict=%.*s rules=%zu\n", spike.c_str(), static_cast<int>(verdict.size()),
              verdict.data(), rules.size());
  return verdict == "pass" ? 0 : 1;
}
