#pragma once
// R1's probe report (O89; plan §2; the owner's offline round R0c): what the probe build of eldennet.dll finds in game,
// in numbers and the leads' labels only, for the reviewing agent to turn into gal/ data. Written through en-gal's
// closed-vocabulary writer (report.hpp), kind "probe", schema 1, and checked by `eldennet-engine lint --report`. Its
// companion, r1-derive.json, is the derivation run over the live image (derive.json's schema 1, the vocabulary
// DataPiler's guard already holds), so this report holds only what the derivation does not:
// - `checks`: what R1 confirmed before its one call into the game, each item `confirmed`, `differs`, `missing` or
//   `not_run`, with the number found and the number expected (R0's or the draft layout's) where there is one: the
//   gate, the FD4 idiom against gal/patterns.toml's (its value the live hits, its expected R0's count), the name
//   layout, the registration and its registers, the group's index against the layout's, the task's shape, and each
//   singleton;
// - `hinstance`: the data slots holding the module's base (global_hinstance's slot);
// - `register`: the call, made only once every check passed and at the title (the first Mark), its registers and group;
// - `slots`: how often the game called each slot of our task's fake vtable (the run slot is the one called every
//   frame), and how much of our object the game's hub wrote;
// - `census`: the vtables the hub's own objects have, and the base classes they share, each with its derived classes'
//   slot counts and run slot (the task's base class);
// - `rising`: the data slots whose instance holds a 32-bit field rising with the clock (the play time's static, which
//   has no RTTI), with how often it rose and stood still;
// - `statics`: each RTTI-classed static's slot, found as the data slot whose instance has the class's vtable.
// It is in en-gal's offline part, beside derive.json's report, so that the launcher lints R1's two reports after an R1
// session (--probe) without en-core (ARCH §2.3). The probe build of eldennet.dll fills `Findings` in game
// (win/probe_r1.cpp); probe.hpp's searches, which find what it holds, are en_gal_game_probe's.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/gal/report.hpp"
#include "en/proto/expected.hpp"

namespace en::gal::probe {

enum class CheckStatus : std::uint8_t { Confirmed, Differs, Missing, NotRun };

struct Check {
  std::string_view item;                   // gate, fd4_idiom, name_layout, register, register_abi, group, task_shape,
                                           // singleton
  std::string label;                       // the leads' label, where the item takes one
  CheckStatus status = CheckStatus::NotRun;
  std::optional<std::uint64_t> value;      // what R1 found
  std::optional<std::uint64_t> expected;   // what R0 or the draft layout says
};

struct Base {
  std::uint32_t type_descriptor = 0;  // RVA
  std::uint32_t found = 0;            // of the census's classes, how many list it among their bases
  std::uint32_t derived = 0;          // classes deriving from it in the image
  std::uint32_t slots_min = 0;
  std::uint32_t slots_max = 0;
  std::optional<std::uint32_t> run_slot;
};

struct Rising {
  std::uint32_t slot = 0;
  std::uint32_t offset = 0;
  std::uint32_t rose = 0;
  std::uint32_t still = 0;
  std::uint32_t other = 0;
};

struct Static {
  std::string label;
  std::uint64_t candidates = 0;
  std::uint64_t checked = 0;
  std::vector<std::uint32_t> found;  // slots (RVAs)
};

// The class objects' name search, as numbers only (derive.hpp's NameSlot): R1 run 1 named 9 of 217 (WHY§206).
struct NameSlot {
  std::uint32_t offset = 0;
  std::uint64_t null_ptr = 0, to_rdata = 0, to_data = 0, to_code = 0, to_other = 0, outside = 0;
  std::uint64_t plain = 0, qualified = 0, other_text = 0, inline_text = 0;
};

struct Names {
  std::uint64_t of = 0;         // class objects searched
  std::uint64_t agree = 0;      // named by the best layout
  std::uint64_t qualified = 0;  // of those, names joined by :: to a namespace
  std::optional<std::uint32_t> offset;  // the best layout's offset
  // Naming by the instances' RTTI (O92): statics tried, null, unread, not a known vtable, classed, leads named.
  std::uint64_t inst_statics = 0, inst_null = 0, inst_unread = 0, inst_unclassed = 0, inst_classed = 0,
                inst_matched = 0;
  std::vector<NameSlot> slots;
};

// R1 run 4 (WHY§209): register_task answered ok and the task never ran. After the call, on T1, the hub's reachable
// memory is searched for our task object's address, read-only and bounded: where it was stored, if anywhere.
struct HubHit {
  std::uint32_t depth = 0;            // pointers followed from the hub's instance
  std::vector<std::uint32_t> path;    // the offset read at each level, the last the word that held our address
  std::uint32_t near_group = 0;       // 32-bit words equal to the group's index within 64 bytes of it
};

struct HubSearch {
  std::uint64_t at_ms = 0;            // after the call
  std::uint64_t nodes = 0;            // blocks read
  std::uint64_t read_bytes = 0;
  std::uint64_t faults = 0;           // copies that faulted
  std::uint64_t refused_reads = 0;    // addresses the plausibility rules refused
  std::uint64_t found = 0;            // words equal to our object's address
  std::uint64_t inside = 0;           // words pointing inside our object, past its start
  std::vector<HubHit> hits;      // the first eight
};

struct Findings {
  std::string_view stage = "start";  // start, derived, waiting, registered, running, refused, failed
  std::uint64_t elapsed_ms = 0;      // since T1's start
  bool gate_passed = false;
  std::uint32_t derive_passes = 0;
  std::uint64_t derive_ms = 0;
  bool derive_written = false;
  std::vector<Check> checks;
  std::vector<std::uint32_t> hinstance;
  std::uint64_t hinstance_count = 0;
  // The call.
  std::string_view register_status = "not_called";  // not_called, waiting_for_mark, refused, ok, faulted
  std::uint32_t callee = 0;                          // RVA
  std::array<std::uint8_t, 3> registers{};           // task, instance, group: 1 rcx, 2 rdx, 8 r8, 9 r9
  std::uint32_t group_index = 0;
  std::uint32_t object_size = 0;
  std::uint32_t vtable_slots = 0;
  // On the game thread.
  std::uint64_t first_tick_ms = 0;  // from the call to the first tick
  std::uint64_t invocations = 0;
  std::vector<std::pair<std::uint32_t, std::uint64_t>> slot_calls;  // slot, calls (those called)
  std::uint32_t object_written = 0;   // one past the last byte of our object the hub wrote
  std::uint32_t object_nonzero = 0;   // bytes it holds that are not zero
  bool census_done = false;
  std::uint64_t census_copies = 0;
  std::uint64_t census_faults = 0;
  std::uint64_t census_objects = 0;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> census_vtables;  // RVA, objects
  std::vector<Base> bases;
  std::uint64_t rising_candidates = 0;
  std::uint32_t rising_passes = 0;
  std::vector<Rising> rising;
  std::vector<Static> statics;
  std::uint64_t copy_faults = 0;
  std::uint64_t stack_refused = 0;
  std::uint64_t allocations = 0;  // on the game thread after its first tick (R6)
  Names names;
  std::vector<HubSearch> hub_search;
};

[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Findings& f);

}  // namespace en::gal::probe
