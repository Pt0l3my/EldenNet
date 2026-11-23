// en_tools_write_recording: writes invented recordings as eldennet.dll's core thread writes them (src/eldennet/win/
// shell.cpp: a banner's, a lifecycle message's or a snapshot's line, then the core stepped on it and its changes'
// lines, through en_shell's own writers and en-core's own slice), for cli_test.py to run rec-stats, replay and
// spike-report on. Every number is invented; there is no game in them.
//
//   en_tools_write_recording <dir>   writes <dir>/s0b.ensnap.jsonl (a solo session of 7,300 s at 1 Hz, no fault),
//                                    <dir>/s0b-slow.ensnap.jsonl (the same, 600 µs at p99 and one fault),
//                                    <dir>/s0c.ensnap.jsonl (solo, a co-op visit that closes the gate, solo again
//                                    until it opens again),
//                                    <dir>/s0d.ensnap.jsonl (25 faults injected into camera, then its breaker trips),
//                                    and in <dir>/runs/ eldennet.dll's reports of S0 runs (en_shell's spikerun.hpp),
//                                    each as spike.cpp writes it: <spike>-<case>.json (run_reports below)
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "en/core/core.hpp"
#include "en/shell/record.hpp"
#include "en/shell/spikerun.hpp"

namespace {

namespace proto = en::proto;

struct Session {
  std::ofstream out;
  en::core::Core core;
  std::uint64_t now = 1'000'000;

  explicit Session(const std::string& path, std::string_view spike) : out(path, std::ios::binary) {
    en::shell::RecordingHeader h;
    h.game_version = {9, 8, 7, 0};
    h.region = "ww";
    h.en_version = {0, 1, 0};
    h.spike = spike;
    h.file = 1;
    out << en::shell::header_line(h, now);
  }
  void step(en::core::CoreInput in) {
    en::core::Outputs o = core.step(std::move(in), now);
    for (const auto& c : o.game) out << en::shell::command_line(c, now);
    for (const auto& c : o.changes) out << en::shell::change_line(c, now);
  }
  void life(const proto::Lifecycle& l) {
    out << en::shell::lifecycle_line(l, now);
    step(en::core::CoreInput{l});
  }
  void snap(const proto::WorldSnapshot& s) {
    out << en::shell::snapshot_line(s, s.mono_ms);
    step(en::core::CoreInput{std::make_unique<proto::WorldSnapshot>(s)});
  }
};

proto::WorldSnapshot base(std::uint64_t seq, std::uint64_t mono) {
  proto::WorldSnapshot s;
  s.seq = seq;
  s.mono_ms = mono;
  for (auto& g : s.health.groups) g.state = proto::GroupState::Ok;
  s.health.task_us_last = 120 + static_cast<std::uint32_t>(seq % 7) * 10;
  s.health.task_us_p99 = 310;
  return s;
}

void title(proto::WorldSnapshot& s) {
  s.life.screen = proto::Screen::Title;
  s.session.lobby = proto::LobbyState::None;
}

void world(proto::WorldSnapshot& s, std::uint64_t play_ms) {
  s.life.main_player = true;
  s.life.screen = proto::Screen::InGame;
  s.life.load_counter = 3;
  s.session.role = proto::MultiplayRole::Host;
  s.session.is_my_world = true;
  s.session.lobby = proto::LobbyState::None;
  s.session.player_set_len = 1;
  for (auto& g : s.health.groups) g.phase_b = true;
  proto::Player p;
  p.level = 42;
  p.archetype = 3;
  p.save_slot = 2;
  p.play_time_ms = play_ms;
  s.player = p;
}

// The start every session shares: DataReady, the title, the playthrough's identity (a name of 7 letters, invented).
std::uint64_t start(Session& r) {
  proto::Lifecycle::DataReady d;
  d.caps.groups = proto::FlagSetTraits<proto::GroupSet>::all;
  r.life(proto::Lifecycle{d});
  std::uint64_t seq = 0;
  for (int i = 0; i < 4; ++i) {
    r.now += 1000;
    auto s = base(++seq, r.now);
    title(s);
    r.snap(s);
  }
  proto::Lifecycle::PlaythroughReady p;
  p.slot = 2;
  p.archetype = 3;
  p.play_time_ms = 500'000;
  (void)p.name.try_assign("Inventa");
  r.life(proto::Lifecycle{p});
  return seq;
}

void solo_session(const std::string& path, bool slow) {
  Session r(path, "s0b");
  std::uint64_t seq = start(r);
  for (int i = 0; i < 7300; ++i) {
    r.now += 1000;
    auto s = base(++seq, r.now);
    world(s, 500'000 + static_cast<std::uint64_t>(i) * 1000);
    if (slow) s.health.task_us_p99 = 600;
    if (slow && i == 4000) s.health.groups[static_cast<std::size_t>(proto::Group::Camera)].faults = 1;
    r.snap(s);
  }
}

void coop_session(const std::string& path) {
  Session r(path, "s0c");
  std::uint64_t seq = start(r);
  for (int i = 0; i < 70; ++i) {
    r.now += 250;
    auto s = base(++seq, r.now);
    world(s, 500'000 + static_cast<std::uint64_t>(i) * 250);
    if (i >= 30 && i < 40) {  // a cooperator joins for 2.5 s: another world, a lobby, two players
      s.session.role = proto::MultiplayRole::Cooperator;
      s.session.is_my_world = false;
      s.session.lobby = proto::LobbyState::Joined;
      s.session.player_set_len = 2;
    }
    r.snap(s);
  }
  r.out << en::shell::mark_line(1, r.now);
}

void fault_session(const std::string& path) {
  Session r(path, "s0d");
  std::uint64_t seq = start(r);
  const auto camera = static_cast<std::size_t>(proto::Group::Camera);
  for (int i = 0; i < 80; ++i) {
    r.now += 250;
    auto s = base(++seq, r.now);
    world(s, 500'000 + static_cast<std::uint64_t>(i) * 250);
    if (i >= 20 && i < 45) s.health.groups[camera].faults = 1;
    if (i >= 45) s.health.groups[camera].state = proto::GroupState::Tripped;
    if (i == 45) r.life(proto::Lifecycle{proto::Lifecycle::GroupTripped{proto::Group::Camera}});
    r.snap(s);
  }
}

// eldennet.dll's reports of S0 runs, as spike.cpp fills them: a ready start with its table checked twice, one with an
// id refused, one with an id that moved when checked again; S0-B's session read for 7,300 s (its process id joins it to
// a recording named for it), and stopped at the discovery banner because the start's layout is not complete; S0-C's
// the same; S0-D's ready with its trial and camera tripped, and stopped short with its trial alone.
void write_run(const std::string& path, const en::shell::SpikeRun& r) {
  std::ofstream(path, std::ios::binary) << en::shell::spike_run_text(r);
}

void run_reports(const std::string& dir) {
  std::filesystem::create_directories(dir);
  namespace w = proto::watch;
  using en::shell::SpikeRun;
  using en::shell::SpikeWaits;
  const auto base = [](w::Spike spike, std::uint32_t pid) {
    SpikeRun r;
    r.spike = spike;
    r.pid = pid;
    r.probe_build = true;
    r.game_version = {9, 8, 7, 0};
    r.region = "ww";
    r.en_version = {0, 1, 0};
    r.writes = 1;
    r.discovery = {true, true, true, 1, 15, 16, 0, 11};
    r.table = {15, 0, true, 15, 0, 0, 60};
    return r;
  };
  const auto ready = [&](w::Spike spike, std::uint32_t pid) {
    SpikeRun r = base(spike, pid);
    r.state = w::InitState::Ready;
    r.start_ms = 9100;
    r.reading.started = true;
    return r;
  };
  const auto waits = [&](w::Spike spike, std::uint32_t pid) {
    SpikeRun r = base(spike, pid);
    r.state = w::InitState::Banner;
    r.banner = en::core::BannerReason::DiscoveryFailed;
    r.waits = SpikeWaits::Layout;
    r.discovery = {true, false, true, 4, 13, 16, 0, 2};
    r.table = {14, 0, true, 14, 0, 0, 56};
    r.start_ms = 41000;
    return r;
  };
  write_run(dir + "/s0a-ready.json", ready(w::Spike::S0a, 1001));
  write_run(dir + "/s0a-waits.json", waits(w::Spike::S0a, 1002));
  SpikeRun refused = ready(w::Spike::S0a, 1003);
  refused.table.refused = 1;
  refused.table.again_refused = 1;
  write_run(dir + "/s0a-refused.json", refused);
  SpikeRun changed = ready(w::Spike::S0a, 1004);
  changed.table.changed = 1;
  write_run(dir + "/s0a-changed.json", changed);
  SpikeRun b = ready(w::Spike::S0b, 4242);
  b.reading.ms = 7'300'000;
  b.reading.snapshots = 73'000;
  b.reading.torn = 2;
  b.reading.held = 3;
  b.reading.unconfirmed = 1;
  b.reading.p99_us = 310;
  b.session_ms = 7'310'000;
  write_run(dir + "/s0b-ready.json", b);
  write_run(dir + "/s0b-waits.json", waits(w::Spike::S0b, 4243));
  write_run(dir + "/s0c-waits.json", waits(w::Spike::S0c, 4244));
  SpikeRun d = ready(w::Spike::S0d, 4245);
  d.trial = {true, 20, 20, 20, 20};
  d.inject.group = proto::Group::Camera;
  d.inject.every = 1;
  d.inject.after_s = 2;
  d.reading.injected_faults = 9;
  d.reading.tripped = std::uint64_t{1} << static_cast<unsigned>(proto::Group::Camera);
  write_run(dir + "/s0d-ready.json", d);
  SpikeRun dw = waits(w::Spike::S0d, 4246);
  dw.trial = {true, 20, 20, 20, 20};
  write_run(dir + "/s0d-waits.json", dw);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fputs("usage: en_tools_write_recording <dir>\n", stderr);
    return 2;
  }
  const std::string dir = argv[1];
  solo_session(dir + "/s0b.ensnap.jsonl", false);
  solo_session(dir + "/s0b-slow.ensnap.jsonl", true);
  coop_session(dir + "/s0c.ensnap.jsonl");
  fault_session(dir + "/s0d.ensnap.jsonl");
  run_reports(dir + "/runs");
  return 0;
}
