#pragma once
// The content predicate DSL (DESIGN §10.4) as v0.1 compiles and evaluates it for Augur's `requires` (the predicates
// and aliases row of DESIGN's test table: Augur's `requires` at v0.1, the personas' `when` at v0.2).
//
//   expr    := and ("||" and)*          and := unary ("&&" unary)*          unary := "!" unary | primary
//   primary := "(" expr ")" | "true" | "false" | ("runes_held" | "journey") op number | name "(" ... ")"
//   op      := ">=" | ">" | "<=" | "<" | "==" | "!="
//
// `runes_held` is the Tarnished's runes held, `journey` the journey (ng_lvl), both at the last processed snapshot.
// `runes >= n` has one meaning everywhere, MEM §6.6's: the great runes, through the catalogue's `runes.ge<n>` aliases
// (WHY§474); v0.1's predicates read no alias, so it is refused here as flag() is, and a persona's `when` that uses it is
// Unknown in the directory (identity.hpp) and evaluated by the knowledge (knowledge.hpp). Every call is refused at
// v0.1: world() and knows() are never Augur's (MEM §12.10, W1); flag(), boss() and stage() name catalogue aliases,
// which v0.1's predicates do not read (every alias is unknown); region() names a key of regions.toml, which they do not
// load. An expression is at most 1 KB and nests at most 32 deep (ARCH §10.2). An error is a fixed text with a column,
// never a quote of the expression.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "en/augur/rules.hpp"
#include "en/proto/expected.hpp"

namespace en::core {

// What v0.1's predicates read.
struct PredicateFacts {
  std::uint64_t runes_held = 0;
  std::uint8_t journey = 0;
};

class Predicates final : public augur::PredicateCompiler {
 public:
  [[nodiscard]] proto::Expected<augur::PredicateId, std::string> compile(std::string_view expr) const override;
  // An unknown id is false (a defect: the loader only hands out ids this compiler made).
  [[nodiscard]] bool eval(augur::PredicateId id, const PredicateFacts& facts) const noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return programs_.size(); }

  struct Op {
    enum class Code : std::uint8_t { True, False, Cmp, Not, And, Or };
    Code code = Code::True;
    std::uint8_t var = 0;  // 0 runes_held, 1 journey
    std::uint8_t cmp = 0;  // 0 >=, 1 >, 2 <=, 3 <, 4 ==, 5 !=
    std::uint64_t n = 0;
  };

 private:
  // compile is const in Augur's interface: the programs it makes are kept here, ids from 1.
  mutable std::vector<std::vector<Op>> programs_;
};

}  // namespace en::core
