#pragma once
// The synthetic scenario DSL (ARCH §8.12, DESIGN §6.8): `tests/scenarios/*.scn`, parsed by en-tools into the inputs
// en-core's step takes (lifecycle messages and snapshots on a fixed period) and the expectations the golden runner
// checks. Our own form of §8.12's example for what v0.1 can say: no personas, cards, flags, speech or barks yet, so
// `set_flag`, `say` and `expect bark` are refused with the version they wait for. Every value is invented.
//
//   route <name>                 the golden file's name: tests/golden/<name>.events.jsonl
//   seed <n>  step <ms>  end <ms>
//   at <ms> <verb> ...           applied at the first snapshot at or after <ms>
//   expect event|no|count <n> <Kind> [field=value ...] [within <a>..<b>]
//   expect audience <Kind> [field=value ...] = [<persona> ...]
//
// Verbs: spawn <name> team=<n> pos=x,y,z [hp=c/m] [param=<n>] [offscreen] (spawn player hp=.. pos=.. sets the
// Tarnished's), despawn, move <who> to=x,y,z [over=<ms>], hit <a> -> <b> dmg=<n>, heal <who> amount=<n>,
// kill <who> [by=<a>], lock <name>, unlock, throw <who> <n>, ride on|off, place <id>|none, banner <n>|none,
// boss <name> on|off, talk on|off, ng <n>, session solo|coop, title, load [over=<ms>] [revive], wait,
// drop_snapshots <n>.
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "en/augur/events.hpp"
#include "en/core/core.hpp"
#include "en/proto/expected.hpp"

namespace en::tools {

struct ScnError {
  std::size_t line = 0;
  std::string problem;  // never a quote of the file beyond a word of its own grammar
};

struct Expect {
  enum class What : std::uint8_t { Event, No, Count, Audience };
  What what = What::Event;
  std::size_t line = 0;
  std::string kind;
  std::vector<std::pair<std::string, std::string>> fields;
  std::optional<std::pair<std::uint64_t, std::uint64_t>> within;
  std::size_t count = 0;
  std::vector<std::uint32_t> audience;
};

struct Action {
  std::uint64_t at = 0;
  std::size_t line = 0;
  std::string verb;
  std::vector<std::string> args;  // the words after the verb
};

struct Scenario {
  std::string route;
  std::uint64_t seed = 0;
  std::uint64_t step = 250;
  std::uint64_t end = 0;
  std::vector<Action> actions;
  std::vector<Expect> expects;
};

[[nodiscard]] proto::Expected<Scenario, ScnError> parse_scn(std::string_view text);

// One input of en-core's step, at its time.
struct Step {
  std::uint64_t now = 0;
  core::CoreInput input;
};

struct Simulated {
  std::vector<Step> steps;
  std::map<std::uint64_t, std::string> names;  // handle → the scenario's name for it
};

// The scenario played: the lifecycle messages at 0, then a snapshot every `step` ms to `end`. `drop_percent` drops
// that share of the snapshots at random from `seed` (the backpressure tests), on top of the scenario's own drops.
[[nodiscard]] proto::Expected<Simulated, ScnError> simulate(const Scenario& s, unsigned drop_percent = 0,
                                                            std::uint64_t drop_seed = 0);

// The expectations against the events, each failure as a line ("scn: line 12: expected ... ").
[[nodiscard]] std::vector<std::string> check_expects(const Scenario& s, const std::vector<augur::Event>& events,
                                                     const std::map<std::uint64_t, std::string>& names);

}  // namespace en::tools
