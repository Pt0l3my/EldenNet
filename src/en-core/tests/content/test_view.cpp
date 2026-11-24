// The Content packs page's projection of each load (content_view.hpp; PAGES §4.6.9): the shipped cast whatever the
// overrides hold, each override file's state and problems by line in the content layer's words (never a value of a
// file), what disable.toml turns off in the content in use, the files the shell left out, and the reloads, newest
// first. Every override text and id is invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/core/content_view.hpp"
#include "en/core/core.hpp"

using namespace en::core;
namespace pr = en::proto;

namespace {

std::vector<ContentFile> with(std::vector<ContentFile> extra) {
  std::vector<ContentFile> files = builtin_content();
  for (ContentFile& f : extra) files.push_back(std::move(f));
  return files;
}

ContentFile over(std::string path, std::string text) {
  return ContentFile{std::move(path), std::move(text), ContentOrigin::Override};
}

std::string shipped_text(std::string_view path) {
  for (const ContentFile& f : builtin_content()) {
    if (f.path == path) return f.text;
  }
  return {};
}

const pr::OverrideFile* row(const pr::ContentView& v, std::string_view path) {
  for (const pr::OverrideFile& f : v.overrides) {
    if (f.path == path) return &f;
  }
  return nullptr;
}

// Every word of the view, for the checks that no value of a file reached it.
std::string all_words(const pr::ContentView& v) {
  std::string out;
  for (const auto* list : {&v.overrides, &v.left_out}) {
    for (const pr::OverrideFile& f : *list) {
      out += f.path + "\n";
      for (const pr::ContentProblem& p : f.problems) out += p.words + "\n";
    }
  }
  for (const std::string& id : v.off_personas) out += id + "\n";
  return out;
}

}  // namespace

TEST_CASE("content view: the shipped content alone: the realm's cast, knowledge and rules, one load that passed") {
  Core core;
  pr::ContentView v;
  const std::vector<ContentFile> files = builtin_content();
  const std::vector<ContentError> errors = core.load_content(files, Realm::Game);
  REQUIRE(errors.empty());
  record_load(v, files, errors, core, Realm::Game, 6'200, pr::ReloadLayer::All);
  CHECK(v.revision == 1);
  CHECK(v.loaded);
  CHECK(v.personas == 1);  // the pilot: the test persona never counts in the game's realm
  CHECK(v.cards == 0);
  CHECK(v.knowledge);
  CHECK(v.rules);
  CHECK(v.overrides.empty());
  CHECK(v.off_personas.empty());
  REQUIRE(v.reloads.size() == 1);
  CHECK(v.reloads[0] == pr::ContentReload{6'200, pr::ReloadLayer::All, true, false, 0, 0});

  // In the tests' realm the test persona is of the cast too.
  pr::ContentView eval;
  record_load(eval, files, errors, core, Realm::Eval, 0, pr::ReloadLayer::All);
  CHECK(eval.personas == 2);
  // The shipped index is counted whatever the overrides put in its place.
  CHECK(shipped_index(with({over("identity/personas.toml", "schema = 1\n")})).size() == 2);
  CHECK(shipped_index(std::vector<ContentFile>{over("identity/personas.toml", "schema = 1\n")}).empty());
}

TEST_CASE("content view: the overrides as loaded: warnings by line, a layer refused alone, what is turned off") {
  Core core;
  pr::ContentView v;
  const std::vector<ContentFile> files =
      with({over("disable.toml", "personas = [\"lantern_keeper\"]\ncards = [\"invented_card\"]\n"),
            over("layers/persona-kale.toml", "schema = 1\ntarget = \"persona:kale\"\nrev = 1\nreply_words_max = 5\n"),
            over("notes/readme.md", "Invented notes of a player's own.\n")});
  const std::vector<ContentError> errors = core.load_content(files, Realm::Game);
  REQUIRE(errors.empty());
  record_load(v, files, errors, core, Realm::Game, 1'000, pr::ReloadLayer::All);
  CHECK(v.loaded);
  REQUIRE(v.overrides.size() == 3);  // by path
  const pr::OverrideFile* disable = row(v, "disable.toml");
  REQUIRE(disable != nullptr);
  CHECK(disable->state == pr::OverrideState::Loaded);
  REQUIRE(disable->problems.size() == 1);
  CHECK(disable->problems[0] ==
        pr::ContentProblem{2, false, "cards[0]: names no shipped card, so nothing is turned off"});
  const pr::OverrideFile* layer = row(v, "layers/persona-kale.toml");
  REQUIRE(layer != nullptr);
  CHECK(layer->state == pr::OverrideState::Refused);  // a layer's errors refuse it alone: the load passed
  REQUIRE_FALSE(layer->problems.empty());
  CHECK(layer->problems[0].error);
  CHECK(layer->problems[0].line == 0);
  CHECK(layer->problems[0].words.find("reply_words_max: refused: ") == 0);
  CHECK(row(v, "notes/readme.md")->state == pr::OverrideState::Loaded);
  CHECK(row(v, "notes/readme.md")->problems.empty());
  CHECK(v.off_personas == std::vector<std::string>{"lantern_keeper"});
  CHECK(v.off_cards.empty());  // v0.2 ships no card: the id is a warning
  CHECK(v.reloads[0].ok);
  CHECK(v.reloads[0].warnings == core.content_warnings().size());
  CHECK(all_words(v).find("invented_card") == std::string::npos);
}

TEST_CASE("content view: a refused load: the file and line, the rest waiting, the content before it in use") {
  Core core;
  pr::ContentView v;
  const std::vector<ContentFile> first = with({over("disable.toml", "personas = [\"lantern_keeper\"]\n")});
  REQUIRE(core.load_content(first, Realm::Game).empty());
  record_load(v, first, {}, core, Realm::Game, 1'000, pr::ReloadLayer::All);

  // A record of the player's own with an unknown key: the load is refused, and its value never shown.
  const std::vector<ContentFile> second =
      with({over("disable.toml", "personas = [\"lantern_keeper\"]\n"),
            over("personas/kale.toml", "colour = \"crimsonhue\"\n" + shipped_text("personas/kale.toml"))});
  const std::vector<ContentError> errors = core.load_content(second, Realm::Game);
  REQUIRE_FALSE(errors.empty());
  record_load(v, second, errors, core, Realm::Game, 90'000, pr::ReloadLayer::Overrides);
  CHECK(v.revision == 2);
  CHECK(v.loaded);  // the first load's content stays in use
  const pr::OverrideFile* refused = row(v, "personas/kale.toml");
  REQUIRE(refused != nullptr);
  CHECK(refused->state == pr::OverrideState::Refused);
  REQUIRE_FALSE(refused->problems.empty());
  CHECK(refused->problems[0].line == 1);
  CHECK(refused->problems[0].error);
  CHECK(refused->problems[0].words.find("colour") != std::string::npos);  // the key's path, never its value
  CHECK(row(v, "disable.toml")->state == pr::OverrideState::Waiting);
  CHECK(v.off_personas == std::vector<std::string>{"lantern_keeper"});  // the content in use, the first load's
  REQUIRE(v.reloads.size() == 2);
  CHECK(v.reloads[0].at_ms == 90'000);  // newest first
  CHECK(v.reloads[0].layer == pr::ReloadLayer::Overrides);
  CHECK_FALSE(v.reloads[0].ok);
  CHECK(v.reloads[0].earlier);
  CHECK(v.reloads[0].errors == errors.size());
  CHECK(v.reloads[1].ok);
  CHECK(all_words(v).find("crimsonhue") == std::string::npos);

  // A start refused: nothing in use, and the reload says so.
  Core fresh;
  pr::ContentView none;
  const std::vector<ContentError> bad = fresh.load_content(second, Realm::Game);
  record_load(none, second, bad, fresh, Realm::Game, 500, pr::ReloadLayer::All);
  CHECK_FALSE(none.loaded);
  CHECK_FALSE(none.reloads[0].earlier);
  CHECK(none.off_personas.empty());
  CHECK(none.personas == 1);  // the shipped cast, whatever was refused
}

TEST_CASE("content view: problems on no file of the folder, an unfit path, the left-out files, the reloads' cap") {
  Core core;
  pr::ContentView v;
  // A shipped file's error and the count past the content layer's list go on the row of the rest, its path empty; a
  // path that is not fit to show is never on a row (PV5).
  const std::vector<ContentFile> files = with({over("C:/Users/someone/disable.toml", "x = 1\n")});
  const std::vector<ContentError> errors = {ContentError{"personas/kale.toml", 4, "voice: must be a table"},
                                            ContentError{"C:/Users/someone/disable.toml", 1, "(top): is empty"},
                                            ContentError{"", 0, "and 3 more problems"}};
  record_load(v, files, errors, core, Realm::Game, 0, pr::ReloadLayer::All);
  REQUIRE(v.overrides.size() == 1);
  CHECK(v.overrides[0].path.empty());
  CHECK(v.overrides[0].state == pr::OverrideState::Refused);
  CHECK(v.overrides[0].problems.size() == 3);
  CHECK(all_words(v).find("someone") == std::string::npos);

  record_left_out(v, std::vector<ContentError>{ContentError{"drafts/notes.toml", 0, "is past the 256 files read"},
                                               ContentError{"../x.toml", 0, "is a link, which is never followed"}});
  REQUIRE(v.left_out.size() == 2);
  CHECK(v.left_out[0].path == "drafts/notes.toml");
  CHECK(v.left_out[0].state == pr::OverrideState::LeftOut);
  CHECK(v.left_out[1].path.empty());
  CHECK(v.revision == 2);

  for (int i = 0; i < 30; ++i) record_load(v, files, {}, core, Realm::Game, 1'000u + i, pr::ReloadLayer::Overrides);
  CHECK(v.reloads.size() == pr::kContentReloadsMax);
  CHECK(v.reloads.front().at_ms == 1'029);
}
