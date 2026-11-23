// eldennet.dll's report of an S0 run (spikerun.hpp).
#include "en/shell/spikerun.hpp"

namespace en::shell {

namespace report = gal::report;
using report::Key;
using report::Type;

namespace {

constexpr std::array<std::string_view, 4> kRegions = {"ww", "jp", "other", "none"};
constexpr std::array<std::string_view, 9> kSpikes = {"none", "s0a",       "s0b", "s0c", "s0d",
                                                     "s0d-crash", "s0e", "s0e_alone", "s4"};
static_assert(kSpikes.size() == proto::watch::kSpikeCount);
// T1's terminal states, and none before one.
constexpr std::array<std::string_view, 5> kStates = {"none", "ready", "inert", "banner", "exit"};
constexpr std::array<std::string_view, 10> kBanners = {"none",          "unknown_build",    "anticheat",
                                                       "not_started",   "discovery_failed", "register_faulted",
                                                       "inert",         "session_failed",   "task_tripped",
                                                       "spike_run"};
static_assert(kBanners.size() == static_cast<std::size_t>(core::BannerReason::SpikeRun) + 2);
constexpr std::array<std::string_view, 6> kWaits = {"none", "layout", "files", "build", "anticheat", "not_started"};
constexpr std::array<std::string_view, 12> kGroups = {"session", "world", "combat", "player", "flags", "text",
                                                      "ui",      "place", "camera", "params", "status", "none"};
static_assert(kGroups.size() == proto::kGroupCount + 1);

constexpr std::array<Key, 47> kKeys = {{
    {"spike", Type::Word, Type::Object, kSpikes},
    {"pid", Type::Unsigned},
    {"probe_build", Type::Boolean},
    {"game_version", Type::Array, Type::Unsigned},
    {"region", Type::Word, Type::Object, kRegions},
    {"en_version", Type::Array, Type::Unsigned},
    {"state", Type::Word, Type::Object, kStates},
    {"banner", Type::Word, Type::Object, kBanners},
    {"waits", Type::Word, Type::Object, kWaits},
    {"start_ms", Type::Unsigned},
    {"session_ms", Type::Unsigned},
    {"writes", Type::Unsigned},
    // discovery
    {"discovery", Type::Object},
    {"ran", Type::Boolean},
    {"ready", Type::Boolean},
    {"leads", Type::Boolean},
    {"passes", Type::Unsigned},
    {"items", Type::Unsigned},
    {"of", Type::Unsigned},
    {"getter_calls", Type::Unsigned},
    {"groups_on", Type::Unsigned},
    // table
    {"table", Type::Object},
    {"refused", Type::Unsigned},
    {"rechecked", Type::Boolean},
    {"entries", Type::Unsigned},
    {"again_refused", Type::Unsigned},
    {"changed", Type::Unsigned},
    {"witnesses", Type::Unsigned},
    // trial
    {"trial", Type::Object},
    {"seh_forced", Type::Unsigned},
    {"seh_caught", Type::Unsigned},
    {"rpm_forced", Type::Unsigned},
    {"rpm_caught", Type::Unsigned},
    // inject
    {"inject", Type::Object},
    {"group", Type::Word, Type::Object, kGroups},
    {"every", Type::Unsigned},
    {"after_s", Type::Unsigned},
    // reading
    {"reading", Type::Object},
    {"started", Type::Boolean},
    {"ms", Type::Unsigned},
    {"snapshots", Type::Unsigned},
    {"torn", Type::Unsigned},
    {"held", Type::Unsigned},
    {"unconfirmed", Type::Unsigned},
    {"copy_faults", Type::Unsigned},
    {"injected_faults", Type::Unsigned},
    {"tripped", Type::Array, Type::Word, kGroups},
}};
constexpr std::array<Key, 1> kMoreKeys = {{
    {"p99_us", Type::Unsigned},
}};

constexpr std::array<Key, kKeys.size() + kMoreKeys.size()> all_keys() {
  std::array<Key, kKeys.size() + kMoreKeys.size()> out{};
  for (std::size_t i = 0; i < kKeys.size(); ++i) out[i] = kKeys[i];
  for (std::size_t i = 0; i < kMoreKeys.size(); ++i) out[kKeys.size() + i] = kMoreKeys[i];
  return out;
}
constexpr auto kAll = all_keys();
constexpr report::Vocabulary kVocabulary{"spike_run", 1, kAll};

std::string_view state_word(proto::watch::InitState s) noexcept {
  using proto::watch::InitState;
  switch (s) {
    case InitState::Ready: return "ready";
    case InitState::Inert: return "inert";
    case InitState::Banner: return "banner";
    case InitState::Exit: return "exit";
    default: return "none";
  }
}

std::string_view spike_word(proto::watch::Spike s) noexcept {
  const auto i = static_cast<std::size_t>(s);
  return i < kSpikes.size() ? kSpikes[i] : kSpikes[0];
}

std::string_view group_word(proto::Group g) noexcept {
  const auto i = static_cast<std::size_t>(g);
  return i < proto::kGroupCount ? kGroups[i] : kGroups[proto::kGroupCount];
}

}  // namespace

std::string_view to_string(SpikeWaits w) noexcept {
  const auto i = static_cast<std::size_t>(w);
  return i < kWaits.size() ? kWaits[i] : kWaits[0];
}

const report::Vocabulary& spike_run_vocabulary() noexcept { return kVocabulary; }

std::string spike_run_text(const SpikeRun& r) {
  report::Writer w(kVocabulary);
  w.word("spike", spike_word(r.spike));
  w.number("pid", r.pid);
  w.boolean("probe_build", r.probe_build);
  w.begin_array("game_version");
  for (const auto v : r.game_version) w.number(v);
  w.end_array();
  w.word("region", r.region);
  w.begin_array("en_version");
  for (const auto v : r.en_version) w.number(v);
  w.end_array();
  w.word("state", state_word(r.state));
  w.word("banner", r.banner ? kBanners[static_cast<std::size_t>(*r.banner) + 1] : kBanners[0]);
  w.word("waits", to_string(r.waits));
  w.number("start_ms", r.start_ms);
  w.number("session_ms", r.session_ms);
  w.number("writes", r.writes);

  w.begin_object("discovery");
  w.boolean("ran", r.discovery.ran);
  w.boolean("ready", r.discovery.ready);
  w.boolean("leads", r.discovery.leads);
  w.number("passes", r.discovery.passes);
  w.number("items", r.discovery.items);
  w.number("of", r.discovery.of);
  w.number("getter_calls", r.discovery.getter_calls);
  w.number("groups_on", r.discovery.groups_on);
  w.end_object();

  w.begin_object("table");
  w.number("items", r.table.items);
  w.number("refused", r.table.refused);
  w.boolean("rechecked", r.table.rechecked);
  w.number("entries", r.table.entries);
  w.number("again_refused", r.table.again_refused);
  w.number("changed", r.table.changed);
  w.number("witnesses", r.table.witnesses);
  w.end_object();

  w.begin_object("trial");
  w.boolean("ran", r.trial.ran);
  w.number("seh_forced", r.trial.seh_forced);
  w.number("seh_caught", r.trial.seh_caught);
  w.number("rpm_forced", r.trial.rpm_forced);
  w.number("rpm_caught", r.trial.rpm_caught);
  w.end_object();

  w.begin_object("inject");
  w.word("group", r.inject.group ? group_word(*r.inject.group) : kGroups[proto::kGroupCount]);
  w.number("every", r.inject.every);
  w.number("after_s", r.inject.after_s);
  w.end_object();

  w.begin_object("reading");
  w.boolean("started", r.reading.started);
  w.number("ms", r.reading.ms);
  w.number("snapshots", r.reading.snapshots);
  w.number("torn", r.reading.torn);
  w.number("held", r.reading.held);
  w.number("unconfirmed", r.reading.unconfirmed);
  w.number("copy_faults", r.reading.copy_faults);
  w.number("injected_faults", r.reading.injected_faults);
  w.begin_array("tripped");
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    if ((r.reading.tripped & (std::uint64_t{1} << g)) != 0) w.word(kGroups[g]);
  }
  w.end_array();
  w.number("p99_us", r.reading.p99_us);
  w.end_object();

  auto out = w.finish();
  return out ? std::move(*out) : std::string();
}

}  // namespace en::shell
