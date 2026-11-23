// Identity (identity.hpp; NPC §2.1, §2.3, §3.2; O45): a character binds by its NpcParam row or its entity id, never by
// name; a test_only persona never resolves in the game's realm, whatever an override gives it; the active stage. All
// data is invented.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/core/identity.hpp"
#include "../test_files.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

proto::Chr chr(std::uint32_t param, std::uint32_t entity = 0) {
  proto::Chr c;
  c.handle = proto::ChrHandle{0x9000 + param};
  c.param = param;
  c.entity = entity;
  return c;
}

ContentPack pack_from(std::vector<ContentFile> files) {
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  return std::move(*pack);
}

constexpr const char* kIndex = R"toml(schema = 1
[[persona]]
id = "wren"
file = "personas/wren.toml"
npc_params = [ { param = 4100, form = "default", verified = true }, { param = 4101, form = "summon" } ]
entity_ids = [ { id = 77001, verified = false } ]
)toml";

constexpr const char* kWren = R"toml(id = "wren"
schema = 1
drafted = "hand"
review = "ci"
display_name_fallback = "Wren"
register = "colloquial"
pronouns = "she"
deflections = ["Not today.", "Ask the wind.", "That is mine to keep."]
[[stages]]
id = "base"
when = "true"
road = "You mend nets by the ford."
at = "limgrave"
state = "awake"
[[stages]]
id = "rich"
when = "runes >= 2"
gist = "The Tarnished has done well."
[[stages]]
id = "hidden"
when = "flag('wren.left')"
gist = "Shows something of its own: inactive while its flag is unknown."
[[stages]]
id = "quiet"
when = "flag('wren.sulks')"
chat = "refuse"
)toml";

}  // namespace

TEST_CASE("a test_only persona never resolves in the game's realm, an override's binding included") {
  const ContentPack shipped = pack_from(builtin_content());
  CHECK(IdentityIndex::build(shipped, Realm::Game).rows() == 1);  // the pilot's lead row alone
  const ContentPack bound = pack_from(content_with_overrides());
  const IdentityIndex game = IdentityIndex::build(bound, Realm::Game);
  CHECK(game.rows() == 0);
  CHECK_FALSE(game.resolve(chr(81100)).has_value());
  const IdentityIndex eval = IdentityIndex::build(bound, Realm::Eval);
  const auto b = eval.resolve(chr(81100));
  REQUIRE(b.has_value());
  CHECK(bound.personas[b->persona].id == "lantern_keeper");
  CHECK(b->path == Binding::Path::Param);
  CHECK(b->verified);
}

TEST_CASE("a character binds by its row, else by a non-zero entity id; nothing else binds") {
  const ContentPack pack = pack_from({ContentFile{"identity/personas.toml", kIndex, ContentOrigin::Shipped},
                                      ContentFile{"personas/wren.toml", kWren, ContentOrigin::Shipped}});
  const IdentityIndex ix = IdentityIndex::build(pack, Realm::Game);
  const auto by_row = ix.resolve(chr(4100));
  REQUIRE(by_row);
  CHECK(by_row->form == "default");
  const auto summon = ix.resolve(chr(4101));
  REQUIRE(summon);
  CHECK(summon->form == "summon");
  CHECK_FALSE(summon->verified);
  const auto by_entity = ix.resolve(chr(5555, 77001));  // an overhaul swapped the row, kept the placement
  REQUIRE(by_entity);
  CHECK(by_entity->path == Binding::Path::Entity);
  CHECK_FALSE(ix.resolve(chr(5555)).has_value());
  CHECK_FALSE(ix.resolve(chr(0, 0)).has_value());
}

TEST_CASE("the active stage: the last whose when holds; an unknown when hides, never reveals") {
  const ContentPack pack = pack_from({ContentFile{"identity/personas.toml", kIndex, ContentOrigin::Shipped},
                                      ContentFile{"personas/wren.toml", kWren, ContentOrigin::Shipped}});
  Predicates predicates;
  const Directory d = Directory::build(pack, Realm::Game, predicates);
  REQUIRE(d.size() == 1);
  CHECK(d.find("wren") == 0u);
  CHECK_FALSE(d.find("nobody").has_value());
  // `quiet` only restricts, so its unknown flag leaves it active: the persona refuses (an unverified flag may hide).
  PredicateFacts poor;
  poor.runes = 0;
  CHECK(d.active_stage(0, predicates, poor) == 3);
  CHECK(d.persona(0).chat_at(3) == ChatMode::Refuse);
  // `hidden` shows a gist of its own, so its unknown flag leaves it inactive; `rich` holds with two runes.
  CHECK_FALSE(d.persona(0).stages[3].shows);
  CHECK(d.persona(0).stages[2].shows);
}
