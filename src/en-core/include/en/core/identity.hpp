#pragma once
// Identity (NPC §2; O45): which persona a character of the snapshot is, matched by id to its data. A character binds
// by its NpcParam row (`npc_params`), else by a non-zero entity id (`entity_ids`, the disambiguator for overhauls that
// swap rows); nothing is matched by name. The directory joins that index to the content pack and works out each
// persona's active stage (NPC §3.2) from v0.1's predicates (predicates.hpp).
//
// A `test_only` persona is never put in the game's index (NPC §2.1): in `Realm::Game`, the realm the DLL builds, its
// rows are left out whatever they hold, an override's included, so it never resolves in game. `Realm::Eval` (the
// self-test, the eval and the tests) keeps them. The tests bind the test persona to an invented character of the fake
// game through an overrides folder of their own (DESIGN §11.4; WHY§364 (1): tests/scenarios/talk/overrides/), whose
// index gives the persona that character's row and keeps it `test_only`; so the binding lives in a test fixture, and
// holds only in the Eval realm.
//
// v0.2 reads no text and no params, so `expect_name_id` is not checked (row_ok, NPC §2.3) and the name is the
// record's `display_name_fallback` (the runtime NpcName wins once text is read, §2.7).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "en/augur/rules.hpp"  // PredicateId
#include "en/core/content.hpp"
#include "en/core/predicates.hpp"
#include "en/proto/snapshot.hpp"

namespace en::core {

enum class Realm : std::uint8_t {
  Game,  // the DLL in the game: test_only personas never resolve
  Eval,  // the self-test, the eval and the tests: they do
};

// How a character resolved to a persona (NPC §2.3's paths, as v0.2 has them).
struct Binding {
  std::size_t persona = 0;  // the persona's index in the directory's pack
  std::string form;         // the row's form: default, summon, ...
  bool verified = false;    // the row (or entity id) is verified
  enum class Path : std::uint8_t { Param, Entity } path = Path::Param;

  friend bool operator==(const Binding&, const Binding&) = default;
};

class IdentityIndex {
 public:
  IdentityIndex() = default;
  [[nodiscard]] static IdentityIndex build(const ContentPack& pack, Realm realm);

  // 1. the exact NpcParam row; 2. a non-zero entity id. Nothing else binds a persona.
  [[nodiscard]] std::optional<Binding> resolve(const proto::Chr& chr) const;
  [[nodiscard]] Realm realm() const noexcept { return realm_; }
  [[nodiscard]] std::size_t rows() const noexcept { return by_param_.size() + by_entity_.size(); }

 private:
  Realm realm_ = Realm::Game;
  std::unordered_map<std::uint32_t, Binding> by_param_;
  std::unordered_map<std::uint32_t, Binding> by_entity_;
};

// What the core knows of its personas: the content pack, the index built from it for a realm, and each stage's `when`
// compiled. Built once per content load; the conversation reads it.
class Directory {
 public:
  Directory() = default;
  [[nodiscard]] static Directory build(ContentPack pack, Realm realm, const Predicates& predicates);

  [[nodiscard]] const ContentPack& pack() const noexcept { return pack_; }
  [[nodiscard]] const IdentityIndex& index() const noexcept { return index_; }
  [[nodiscard]] std::size_t size() const noexcept { return pack_.personas.size(); }
  [[nodiscard]] const Persona& persona(std::size_t i) const { return pack_.personas[i]; }
  [[nodiscard]] std::optional<std::size_t> find(std::string_view id) const noexcept;

  // NPC §3.2: the last stage in file order whose `when` holds. A `when` v0.1's predicates cannot evaluate (flag(),
  // boss(), stage() and region() are refused until the catalogue) is Unknown: such a stage is inactive if it unlocks or
  // shows anything of its own, and active if it only restricts. The base stage's `when` is "true".
  [[nodiscard]] std::size_t active_stage(std::size_t persona, const Predicates& predicates,
                                         const PredicateFacts& facts) const;

 private:
  ContentPack pack_;
  IdentityIndex index_;
  std::vector<std::vector<std::optional<augur::PredicateId>>> when_;  // per persona, per stage: none when Unknown
};

}  // namespace en::core
