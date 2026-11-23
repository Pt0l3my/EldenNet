// The content pack's speech (speech_content.hpp; WHY§364, WHY§388): the shipped prompts and lexicon load with the pack,
// a pack whose prompts or lexicon fail is refused by the core with the old content kept, and block 2's content lint:
// an error for shipped content and a test persona, a warning for the player's own record, which still renders. Every
// persona and line here is invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/core/core.hpp"
#include "en/core/speech_content.hpp"
#include "../test_files.hpp"

using namespace en::core;
using namespace en::core::test;

namespace {

// An invented persona whose block 2 is far past its 120 tokens: its identity, role, summary and appearance near their
// schema caps (40, 20, 60 and 40 words).
constexpr const char* kWordy = R"toml(id = "heron"
schema = 1
drafted = "hand"
review = "ci"
display_name_fallback = "Heron"
register = "formal"
pronouns = "he"
deflections = ["Not today.", "Ask the wind.", "That is mine to keep."]
[public]
summary = "A tall and patient wader of the shallows who has fished the same bend of the slow brown river for more seasons than the oldest of the reed-cutters can remember, and who speaks to anyone who stands still long enough beside the water to hear him out, of the fish and the weather and the way the bank has moved."
appearance = "A long-legged old man in a grey coat patched at both elbows, his beard thin and white, a crooked wooden staff in one hand and a basket of woven willow on his back, his boots always wet to the knee."
role = "the old fisher of the river bend, who knows every eddy and every shallow from the weir to the ford"
[[stages]]
id = "base"
when = "true"
identity = "You are Heron, the old fisher of the slow river's bend, who has watched the water rise and fall for longer than anyone living, and who keeps count of every flood and every drought in notches on a post."
road = "You fish the river's bend."
at = "limgrave"
state = "awake"
)toml";

constexpr const char* kHeronRow = R"toml(
[[persona]]
id = "heron"
file = "personas/heron.toml"
npc_params = [ { param = 4200, form = "default", verified = true } ]
)toml";

// The shipped pack with an index that adds the heron, from `origin`, test_only or not.
std::vector<ContentFile> with_heron(ContentOrigin origin, bool test_only) {
  std::vector<ContentFile> files = builtin_content();
  std::string index = "schema = 1\n";
  index += kHeronRow;
  if (test_only) index += "test_only = true\n";
  files.push_back(ContentFile{"identity/personas.toml", index, ContentOrigin::Override});
  files.push_back(ContentFile{"personas/heron.toml", kWordy, origin});
  return files;
}

bool mentions(const std::vector<ContentError>& errors, const std::string& file, const std::string& words) {
  return std::any_of(errors.begin(), errors.end(), [&](const ContentError& e) {
    return e.file == file && e.problem.find(words) != std::string::npos;
  });
}

}  // namespace

TEST_CASE("the shipped pack's prompts and lexicon load with it, with no warning") {
  auto pack = load_content(builtin_content());
  REQUIRE(pack.has_value());
  auto speech = load_speech(*pack);
  REQUIRE_MESSAGE(speech.has_value(), (speech.has_value() ? std::string() : describe(speech.error().front())));
  REQUIRE(speech->prompt != nullptr);
  REQUIRE(speech->lint != nullptr);
  CHECK(speech->warnings.empty());
  CHECK_FALSE(speech->lint->lexicon().banned.empty());
  CHECK(speech->lint->lexicon().safety_always.empty());  // shipped with no entries: the owner's step (O107)
  // The shipped test persona's block 2 is within its cap at every stage.
  const Persona& keeper = *pack->persona("lantern_keeper");
  for (std::size_t i = 0; i < keeper.stages.size(); ++i) {
    const nlohmann::json record = keeper.resolved(i);
    DialogueContext c;
    c.persona = keeper.id;
    c.record = &record;
    c.name = keeper.display_name_fallback;
    const auto t = speech->prompt->block_tokens(c, Block::Identity);
    REQUIRE(t);
    CHECK(*t <= kBlocks[static_cast<std::size_t>(Block::Identity)].cap);
  }
}

TEST_CASE("a pack whose prompts or lexicon fail is refused, and the core keeps its old content") {
  Core core;
  REQUIRE(core.load_content(content_with_overrides(), Realm::Eval).empty());
  REQUIRE(core.directory().size() == 1);
  const std::uint64_t hash = core.directory().pack().hash;

  const auto refused = [&](std::vector<ContentFile> files) {
    const std::vector<ContentError> errors = core.load_content(files, Realm::Eval);
    CHECK(core.directory().pack().hash == hash);  // the old content stays
    return errors;
  };
  // A block's part missing.
  std::vector<ContentFile> files = content_with_overrides();
  files.erase(std::remove_if(files.begin(), files.end(),
                             [](const ContentFile& f) { return f.path.rfind("prompts/dialogue/11_", 0) == 0; }),
              files.end());
  std::vector<ContentError> e = refused(files);
  REQUIRE(e.size() == 1);
  CHECK(e[0].file == "prompts/");
  CHECK(e[0].problem.find("no part 11") != std::string::npos);
  // A template naming what the data does not hold, in the player's overrides.
  files = content_with_overrides();
  files.push_back(ContentFile{"prompts/dialogue/09_scene.j2", "[ user ]\n{{ scene.secret_weather }}", ContentOrigin::Override});
  e = refused(files);
  REQUIRE(e.size() == 1);
  CHECK(e[0].file == "prompts/");
  // The lexicon missing, or malformed.
  files = content_with_overrides();
  files.erase(std::remove_if(files.begin(), files.end(), [](const ContentFile& f) { return f.path == kSafetyPath; }),
              files.end());
  e = refused(files);
  REQUIRE(e.size() == 1);
  CHECK(e[0].file == "lexicon/safety.toml");
  CHECK(e[0].problem == "is missing");
  files = content_with_overrides();
  files.push_back(ContentFile{std::string(kBannedPath), "format = 2\n", ContentOrigin::Override});
  e = refused(files);
  REQUIRE(e.size() == 1);
  CHECK(e[0].file == "lexicon/banned.toml");
  // And a good pack loads again.
  CHECK(core.load_content(content_with_overrides(), Realm::Eval).empty());
}

TEST_CASE("block 2's content lint: an error for shipped content and a test persona, a warning for the player's own") {
  // Shipped: refused.
  auto pack = load_content(with_heron(ContentOrigin::Shipped, false));
  REQUIRE_MESSAGE(pack.has_value(), (pack.has_value() ? std::string() : describe(pack.error().front())));
  auto speech = load_speech(*pack);
  REQUIRE_FALSE(speech.has_value());
  REQUIRE(speech.error().size() == 1);
  CHECK(speech.error()[0].file == "personas/heron.toml");
  CHECK(speech.error()[0].line > 0);
  CHECK(mentions(speech.error(), "personas/heron.toml", "stages[0]: block 2"));
  CHECK(mentions(speech.error(), "personas/heron.toml", "past its cap of 120"));
  CHECK(speech.error()[0].problem.find("fisher") == std::string::npos);  // never a value of the file

  // A test persona in the player's overrides folder: refused too.
  pack = load_content(with_heron(ContentOrigin::Override, true));
  REQUIRE(pack.has_value());
  CHECK_FALSE(load_speech(*pack).has_value());

  // The player's own record: a warning, and the pack loads; the persona still renders, under the hard cap.
  Core core;
  CHECK(core.load_content(with_heron(ContentOrigin::Override, false), Realm::Game).empty());
  REQUIRE(core.content_warnings().size() == 1);
  CHECK(core.content_warnings()[0].file == "personas/heron.toml");
  CHECK(mentions(core.content_warnings(), "personas/heron.toml", "past its cap of 120"));
  REQUIRE(core.directory().find("heron").has_value());
  pack = load_content(with_heron(ContentOrigin::Override, false));
  REQUIRE(pack.has_value());
  speech = load_speech(*pack);
  REQUIRE(speech.has_value());
  REQUIRE(speech->warnings.size() == 1);
  const nlohmann::json record = pack->persona("heron")->resolved(0);
  DialogueContext c;
  c.persona = "heron";
  c.record = &record;
  c.name = "Heron";
  const auto r = speech->prompt->report(c);
  REQUIRE(r);
  CHECK(r->blocks[static_cast<std::size_t>(Block::Identity)].over_cap);
  CHECK(r->prompt.tokens <= kPromptHardCap);
  // A later load without it clears the warning.
  CHECK(core.load_content(builtin_content(), Realm::Game).empty());
  CHECK(core.content_warnings().empty());
}
