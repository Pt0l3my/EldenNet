// eldennet.dll's report of an S0 run (spikerun.hpp): a full report and a stopped start's both lint clean against their
// own vocabulary, carry the words the launcher and spike-report read, and nothing else; the S0 spikes and sessions.
#include <doctest.h>

#include <array>
#include <string>

#include "en/gal/report.hpp"
#include "en/shell/spikerun.hpp"

using namespace en::shell;
namespace proto = en::proto;
namespace w = en::proto::watch;

namespace {

void lints(const std::string& text) {
  INFO(text.substr(0, 400));
  REQUIRE_FALSE(text.empty());
  const std::array<const en::gal::report::Vocabulary*, 1> known = {&spike_run_vocabulary()};
  const auto ok = en::gal::report::lint(text, known);
  if (!ok) {
    for (const auto& p : ok.error()) INFO(en::gal::report::describe(p));
  }
  CHECK(ok.has_value());
}

bool holds(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("spike run: a ready start's report, with the table checked twice, the trial and the reader") {
  SpikeRun r;
  r.spike = w::Spike::S0d;
  r.pid = 4242;
  r.probe_build = true;
  r.game_version = {3, 14, 15, 0};
  r.region = "ww";
  r.en_version = {0, 1, 0};
  r.state = w::InitState::Ready;
  r.start_ms = 2100;
  r.session_ms = 62000;
  r.writes = 7;
  r.discovery = {true, true, true, 1, 15, 15, 0, 11};
  r.table = {15, 0, true, 15, 0, 0, 62};
  r.trial = {true, 20, 20, 20, 20};
  r.inject.group = proto::Group::Camera;
  r.inject.every = 1;
  r.inject.after_s = 2;
  r.reading.started = true;
  r.reading.ms = 59000;
  r.reading.snapshots = 590;
  r.reading.torn = 1;
  r.reading.held = 2;
  r.reading.unconfirmed = 3;
  r.reading.copy_faults = 0;
  r.reading.injected_faults = 9;
  r.reading.tripped = std::uint64_t{1} << static_cast<unsigned>(proto::Group::Camera);
  r.reading.p99_us = 180;
  const std::string text = spike_run_text(r);
  lints(text);
  CHECK(holds(text, "\"report\": \"spike_run\""));
  CHECK(holds(text, "\"spike\": \"s0d\""));
  CHECK(holds(text, "\"state\": \"ready\""));
  CHECK(holds(text, "\"banner\": \"none\""));
  CHECK(holds(text, "\"waits\": \"none\""));
  CHECK(holds(text, "\"group\": \"camera\""));
  CHECK(holds(text, "\"camera\""));
  CHECK(holds(text, "\"pid\": 4242"));
  CHECK_FALSE(holds(text, "\\"));
}

TEST_CASE("spike run: a start stopped at the discovery banner waits for its layout") {
  SpikeRun r;
  r.spike = w::Spike::S0b;
  r.state = w::InitState::Banner;
  r.banner = en::core::BannerReason::DiscoveryFailed;
  r.waits = SpikeWaits::Layout;
  r.discovery.ran = true;
  r.table.items = 14;
  const std::string text = spike_run_text(r);
  lints(text);
  CHECK(holds(text, "\"banner\": \"discovery_failed\""));
  CHECK(holds(text, "\"waits\": \"layout\""));
  CHECK(holds(text, "\"group\": \"none\""));
  CHECK(holds(text, "\"tripped\": []"));
  for (SpikeWaits wt : {SpikeWaits::None, SpikeWaits::Layout, SpikeWaits::Files, SpikeWaits::Build,
                        SpikeWaits::Anticheat, SpikeWaits::NotStarted}) {
    r.waits = wt;
    lints(spike_run_text(r));
  }
  CHECK(to_string(SpikeWaits::NotStarted) == "not_started");
}

TEST_CASE("spike run: which spikes are S0's, and which of them are sessions") {
  CHECK(s0_spike(w::Spike::S0a));
  CHECK(s0_spike(w::Spike::S0dCrash));
  CHECK_FALSE(s0_spike(w::Spike::S0e));
  CHECK_FALSE(s0_spike(w::Spike::None));
  CHECK(s0_session(w::Spike::S0b));
  CHECK(s0_session(w::Spike::S0c));
  CHECK(s0_session(w::Spike::S0d));
  CHECK_FALSE(s0_session(w::Spike::S0a));
  CHECK_FALSE(s0_session(w::Spike::S0dCrash));
}
