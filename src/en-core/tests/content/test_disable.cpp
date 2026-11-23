// The overrides' disable.toml (ARCH §11.6, DESIGN §11.4; roadmap S11.3.g): shipped personas turned off by id, checked
// like any content (an unknown id is a warning, never an error), and the core's index built without them. The ids
// that name nothing are invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/core.hpp"
#include "en/core/identity.hpp"
#include "../test_files.hpp"

using namespace en::core;
using namespace en::core::test;

namespace {

std::vector<ContentFile> shipped_with_disable(const std::string& text) {
  std::vector<ContentFile> files = builtin_content();
  files.push_back(ContentFile{std::string(kDisablePath), text, ContentOrigin::Override});
  return files;
}

bool mentions(const std::vector<ContentError>& errors, std::string_view needle) {
  return std::any_of(errors.begin(), errors.end(), [&](const ContentError& e) {
    return describe(e).find(needle) != std::string::npos;
  });
}

}  // namespace

TEST_CASE("disable.toml turns a shipped persona off: no record, no index row, no directory entry") {
  const auto plain = load_content(builtin_content());
  REQUIRE(plain.has_value());
  REQUIRE(plain->persona("kale") != nullptr);
  CHECK(plain->disabled_personas.empty());
  CHECK(IdentityIndex::build(*plain, Realm::Game).rows() == 1);  // the pilot's lead row

  const auto files = shipped_with_disable("personas = [\"kale\", \"kale\"]\n");
  const auto pack = load_content(files);
  REQUIRE(pack.has_value());
  CHECK(pack->persona("kale") == nullptr);
  REQUIRE(pack->row("kale") != nullptr);
  CHECK_FALSE(pack->row("kale")->enabled);
  CHECK(pack->disabled_personas == std::vector<std::string>{"kale"});  // once, whatever the file repeats
  CHECK(pack->warnings.empty());
  CHECK(pack->persona("lantern_keeper") != nullptr);  // the rest untouched
  CHECK(IdentityIndex::build(*pack, Realm::Game).rows() == 0);
  CHECK(pack->hash != plain->hash);  // a change of what is turned off is a change of the content
  CHECK(std::none_of(pack->others.begin(), pack->others.end(),
                     [](const ContentFile& f) { return f.path == kDisablePath; }));

  // Through the core: the pack loads, the directory has no pilot, and nothing is warned.
  Core core;
  CHECK(core.load_content(files, Realm::Game).empty());
  CHECK_FALSE(core.directory().find("kale").has_value());
  CHECK(core.directory().find("lantern_keeper").has_value());
  CHECK(core.content_warnings().empty());
  CHECK(core.directory().pack().disabled_personas == std::vector<std::string>{"kale"});
}

TEST_CASE("disable.toml: an id that names nothing is a warning, never an error, and never quoted back") {
  const auto files = shipped_with_disable(
      "personas = [\"no_such_one\", \"lantern_keeper\"]\ncards = [\"invented_card\"]\nkb = [\"rumour.invented\"]\n"
      "characters = []\ntriggers = [\"idle\"]\nrules = [\"some_rule\"]\n");
  const auto pack = load_content(files);
  REQUIRE(pack.has_value());
  CHECK(pack->disabled_personas == std::vector<std::string>{"lantern_keeper"});
  CHECK(pack->persona("kale") != nullptr);
  REQUIRE(pack->warnings.size() == 5);  // the persona, the card, and kb, triggers and rules (characters is empty)
  CHECK(describe(pack->warnings[0]) == "disable.toml:1: personas[0]: names no persona of the index, so nothing is "
                                       "turned off");
  CHECK(describe(pack->warnings[1]) == "disable.toml:2: cards[0]: names no shipped card, so nothing is turned off");
  CHECK(mentions(pack->warnings, "kb: not read in this version"));
  CHECK(mentions(pack->warnings, "triggers: not read in this version"));
  CHECK(mentions(pack->warnings, "rules: not read in this version"));
  for (const char* id : {"no_such_one", "invented_card", "rumour.invented", "idle", "some_rule"}) {
    CHECK_FALSE(mentions(pack->warnings, id));
  }
  // The core keeps them as the load's warnings, before any of block 2's, and the pack loads.
  Core core;
  CHECK(core.load_content(files, Realm::Eval).empty());
  CHECK(core.content_warnings().size() == 5);
  CHECK(core.directory().find("kale").has_value());
  CHECK_FALSE(core.directory().find("lantern_keeper").has_value());
}

TEST_CASE("disable.toml is checked as any content: an unknown key or a wrong type refuses the pack") {
  for (const char* text : {"colours = [\"crimsonhue\"]\n", "personas = \"kale\"\n", "personas = [1]\n", "personas = [\n",
                           "[personas]\nkale = true\n"}) {
    CAPTURE(text);
    const auto pack = load_content(shipped_with_disable(text));
    REQUIRE_FALSE(pack.has_value());
    CHECK(mentions(pack.error(), "disable.toml"));
    CHECK_FALSE(mentions(pack.error(), "crimsonhue"));
  }
  // The old content stays in the core.
  Core core;
  REQUIRE(core.load_content(builtin_content(), Realm::Game).empty());
  CHECK_FALSE(core.load_content(shipped_with_disable("personas = 3\n"), Realm::Game).empty());
  CHECK(core.directory().find("kale").has_value());
  // An empty file turns nothing off.
  const auto empty = load_content(shipped_with_disable(""));
  REQUIRE(empty.has_value());
  CHECK(empty->disabled_personas.empty());
  CHECK(empty->warnings.empty());
}
