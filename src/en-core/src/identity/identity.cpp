// Identity: the persona index and the directory (identity.hpp; NPC §2.1, §2.3, §3.2; O45).
#include "en/core/identity.hpp"

namespace en::core {

IdentityIndex IdentityIndex::build(const ContentPack& pack, Realm realm) {
  IdentityIndex ix;
  ix.realm_ = realm;
  for (const PersonaRow& row : pack.rows) {
    if (!row.enabled) continue;
    if (row.test_only && realm == Realm::Game) continue;  // never in the game's index (NPC §2.1)
    std::optional<std::size_t> at;
    for (std::size_t i = 0; i < pack.personas.size(); ++i) {
      if (pack.personas[i].id == row.id) at = i;
    }
    if (!at) continue;  // load_content refuses a pack where an enabled row has no record
    for (const NpcParamRow& p : row.npc_params) {
      ix.by_param_.emplace(p.param, Binding{*at, p.form, p.verified, Binding::Path::Param});
    }
    for (const EntityRow& e : row.entity_ids) {
      ix.by_entity_.emplace(e.id, Binding{*at, "default", e.verified, Binding::Path::Entity});
    }
  }
  return ix;
}

std::optional<Binding> IdentityIndex::resolve(const proto::Chr& chr) const {
  if (const auto it = by_param_.find(chr.param); chr.param != 0 && it != by_param_.end()) return it->second;
  if (const auto it = by_entity_.find(chr.entity); chr.entity != 0 && it != by_entity_.end()) return it->second;
  return std::nullopt;
}

Directory Directory::build(ContentPack pack, Realm realm, const Predicates& predicates) {
  Directory d;
  d.index_ = IdentityIndex::build(pack, realm);
  d.when_.reserve(pack.personas.size());
  for (const Persona& p : pack.personas) {
    std::vector<std::optional<augur::PredicateId>> stages;
    for (const PersonaStage& s : p.stages) {
      auto compiled = predicates.compile(s.when);
      if (compiled) {
        stages.emplace_back(*compiled);
      } else {
        stages.emplace_back(std::nullopt);
      }
    }
    d.when_.push_back(std::move(stages));
  }
  d.pack_ = std::move(pack);
  return d;
}

std::optional<std::size_t> Directory::find(std::string_view id) const noexcept {
  for (std::size_t i = 0; i < pack_.personas.size(); ++i) {
    if (pack_.personas[i].id == id) return i;
  }
  return std::nullopt;
}

std::size_t Directory::active_stage(std::size_t persona, const Predicates& predicates, const PredicateFacts& facts) const {
  const Persona& p = pack_.personas[persona];
  std::size_t active = 0;
  for (std::size_t i = 1; i < p.stages.size(); ++i) {
    const auto& when = when_[persona][i];
    if (when) {
      if (predicates.eval(*when, facts)) active = i;
    } else if (!p.stages[i].shows) {
      active = i;  // Unknown, and it only restricts: an unverified flag may hide, never reveal
    }
  }
  return active;
}

}  // namespace en::core
