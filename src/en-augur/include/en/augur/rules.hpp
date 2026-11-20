#pragma once
// The rule files (ARCH §8.5, DESIGN §6.3): their schema, the load-time validation and the compiled `requires`.
//
// RuleSet::load reads every file of a set, checks every rule against the schema, the catalogue, the signal registry and
// the payload builders, and compiles each `requires` through the PredicateCompiler en-core gives it (the predicates
// are en-core's, DESIGN §10.4; Augur only holds their ids). Any error refuses the whole set, so a reload keeps the old
// one (Augur::set_rules), and each error names its file and line (a toast, DESIGN §11.4). Unknown keys are refused.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/augur/detectors.hpp"
#include "en/augur/events.hpp"
#include "en/augur/signals.hpp"
#include "en/augur/types.hpp"
#include "en/proto/expected.hpp"

namespace en::augur {

// A compiled `requires`. 0 is "true", which needs no evaluation.
using PredicateId = std::uint32_t;
inline constexpr PredicateId kAlwaysTrue = 0;

// en-core's predicate DSL (DESIGN §10.4), as Augur's `requires` may use it: world() and knows() are refused here
// (MEM §12.10, W1), and every alias must be a catalogue alias. An expression is at most 1 KB and nests at most 32 deep
// (ARCH §10.2). The error is a fixed text or names a word of the expression's own grammar, never more.
class PredicateCompiler {
 public:
  [[nodiscard]] virtual proto::Expected<PredicateId, std::string> compile(std::string_view expr) const = 0;

 protected:
  ~PredicateCompiler() = default;
};

// The payload builders registered in code (ARCH §8.5: `payload` names one). Each builds one kind and offers the
// placeholders a key may use; "subject" is always offered.
enum class PayloadId : std::uint8_t {
  Place,
  BossStart,
  BossOutcome,
  TalkStart,
  TalkEnd,
  PlayerDied,
  Mount,
  Struck,
  LowHp,
  Healed,
  Engaged,
  Disengaged,
  AllyDied,
  Crit,
  Journey,
  DataReady,
  Playthrough,
  LoadingStarted,
  LoadingEnded,
  Title,
  Gate,
  Group,
  Ladder,
};

struct PayloadInfo {
  PayloadId id;
  std::string_view name;
  std::string_view kind;  // the catalogue's name of the kind it builds
  std::array<std::string_view, 2> placeholders;  // besides "subject"; empty slots are ""
};
[[nodiscard]] std::span<const PayloadInfo> payloads() noexcept;
[[nodiscard]] const PayloadInfo& payload_info(PayloadId id) noexcept;

// Why a catalogue kind has no builder in this build: the version and source it waits for (the README's table).
[[nodiscard]] std::string_view waits_for(std::size_t kind) noexcept;

// A rule's subjects (ARCH §8.5's `subjects`).
enum : std::uint8_t { kKindGeneric = 1, kKindPersona = 2, kKindPlayer = 4, kKindWorld = 8 };

struct Rule {
  std::shared_ptr<const std::string> id;
  std::size_t index = 0;  // in the set, in file order
  std::size_t kind = 0;   // EventKind's index
  std::uint8_t subjects = 0;
  bool alive = false;      // only while the subject is not dead
  bool in_combat = false;  // only while the subject is in combat (the Tarnished: §8.7; a character: engaged ≥ 0.6)
  SignalId signal = SignalId::HpFrac;
  DetectorSpec detect;
  Mono debounce_ms = 0;
  PayloadId payload = PayloadId::Place;
  Class cls = Class::T;
  std::string key;  // the template, with {placeholders}; empty: none
  std::uint32_t ttl_s = 0, merge_ms = 0, dedup_s = 0, cooldown_s = 0;
  float confidence = 1.0f;
  Radius radius = Radius::None;
  InterruptSet interrupts = InterruptSet::None;
  PredicateId requires_id = kAlwaysTrue;
  std::uint8_t priority = 50;
  std::string file;
  std::size_t line = 0;
};

struct RuleFile {
  std::string name;  // shown in errors: the file's name, no folder
  std::string text;
  bool user = false;  // a player's own file (augur.allow_custom_rules)
};

struct RuleError {
  std::string file;
  std::size_t line = 0;  // 1-based; 0 for the whole file
  std::string problem;
};
[[nodiscard]] std::string describe(const RuleError& e);  // "<file>:<line>: <problem>"

struct RuleLimits {
  bool allow_custom = false;               // augur.allow_custom_rules: off in v1 (DESIGN §6.3)
  std::size_t max_file_bytes = 64 * 1024;  // ARCH §8.5
  std::size_t max_rules = 256;             // impl
  std::size_t max_errors = 64;
};

class RuleSet {
 public:
  [[nodiscard]] static proto::Expected<std::shared_ptr<const RuleSet>, std::vector<RuleError>> load(
      std::span<const RuleFile> files, const PredicateCompiler& predicates, const RuleLimits& limits = {});

  [[nodiscard]] std::span<const Rule> rules() const noexcept { return rules_; }
  // FNV-1a 64 over each file's name and text, in order: the golden files' rules_hash.
  [[nodiscard]] std::uint64_t hash() const noexcept { return hash_; }

 private:
  std::vector<Rule> rules_;
  std::uint64_t hash_ = 0;
};

// FNV-1a 64 (the golden files' hashes), and its text form "fnv1a64:<16 hex digits>".
[[nodiscard]] std::uint64_t fnv1a64(std::string_view bytes, std::uint64_t h = 0xcbf29ce484222325ull) noexcept;
[[nodiscard]] std::string hash_text(std::uint64_t h);

}  // namespace en::augur
