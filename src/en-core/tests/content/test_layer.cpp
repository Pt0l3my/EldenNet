// The player's own layer over a shipped persona (profile.hpp; NPC §4.5, §4.7, §4.8; S7.2.b): its path, its schema
// and caps (PL1), its text written and read back, its wording (PL4, PL5) and block 4 with it (WHY§472), the merge into
// the persona with the shipped record untouched, and the editor's check over the same load. Every text is invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../test_files.hpp"
#include "en/core/core.hpp"
#include "en/core/profile.hpp"
#include "en/core/speech_lint.hpp"

namespace en::core {
namespace {

ContentFile layer(std::string text, std::string path = "layers/persona-kale.toml") {
  return ContentFile{std::move(path), std::move(text), ContentOrigin::Override};
}

std::vector<LayerFinding> read(const ContentFile& f, std::optional<PersonaLayer>* out = nullptr) {
  std::vector<LayerFinding> found;
  auto l = read_layer(f, found);
  if (out != nullptr) *out = std::move(l);
  return found;
}

bool has(const std::vector<LayerFinding>& fs, std::string_view field, LayerProblem p, bool error, int item = -1) {
  return std::any_of(fs.begin(), fs.end(), [&](const LayerFinding& f) {
    return f.field == field && f.problem == p && f.error == error && f.item == item;
  });
}

constexpr std::string_view kHead = "schema = 1\ntarget = \"persona:kale\"\nrev = 2\n";

std::vector<ContentFile> with(const ContentFile& extra) {
  std::vector<ContentFile> files = builtin_content();
  files.push_back(extra);
  return files;
}

}  // namespace

TEST_CASE("layer: its path in the overrides folder, by the persona's id") {
  CHECK(layer_path("kale") == "layers/persona-kale.toml");
  CHECK(layer_persona("layers/persona-kale.toml") == "kale");
  CHECK_FALSE(layer_persona("layers/card-militia.toml"));
  CHECK_FALSE(layer_persona("layers/persona-Kale.toml"));
  CHECK_FALSE(layer_persona("personas/kale.toml"));
  CHECK_FALSE(layer_persona("layers/persona-.toml"));
  CHECK(record_hash("") == "cbf29ce484222325");
  CHECK(record_hash("a").size() == 16);
}

TEST_CASE("layer: a file a layer may hold reads whole; every field optional") {
  std::optional<PersonaLayer> l;
  const auto found = read(layer(std::string(kHead) +
                                "base_hash = \"0123456789abcdef\"\ndrafted = \"assisted\"\n"
                                "instructions = \"Hums under his breath when trade is slow.\"\n"
                                "subjects = [\"the price of lamp oil\", \"a good pair of boots\"]\n"
                                "register = \"formal\"\nreply_words_max = 25\n"),
                          &l);
  CHECK(found.empty());
  REQUIRE(l);
  CHECK(l->persona == "kale");
  CHECK(l->rev == 2);
  CHECK(l->base_hash == "0123456789abcdef");
  CHECK(l->assisted);
  CHECK(l->fields["instructions"] == "Hums under his breath when trade is slow.");
  CHECK(l->fields["subjects"].size() == 2);
  CHECK(l->fields["register"] == "formal");
  CHECK(l->fields["reply_words_max"] == 25);
  std::optional<PersonaLayer> bare;
  CHECK(read(layer(std::string(kHead)), &bare).empty());
  REQUIRE(bare);
  CHECK(bare->fields.empty());
  CHECK_FALSE(bare->assisted);
}

TEST_CASE("layer: PL1's schema and caps are errors, each by its field and item, never a value") {
  SUBCASE("a key a layer may not set") {
    const auto f = read(layer(std::string(kHead) + "public = { summary = \"x\" }\n"));
    CHECK(has(f, "", LayerProblem::UnknownKey, true));
  }
  SUBCASE("too long, too many, out of range, not a choice, empty") {
    const auto f = read(layer(std::string(kHead) +
                              "subjects = [\"one\", \"two\", \"three\", \"four\", \"five\", \"six\", \"seven\", \"eight\", "
                              "\"nine\", \"a b c d e f g h i j k\", \"\"]\nregister = \"shouting\"\nreply_words_max = 70\n"));
    CHECK(has(f, "subjects", LayerProblem::TooManyItems, true));
    CHECK(has(f, "subjects", LayerProblem::TooManyWords, true, 9));
    CHECK(has(f, "subjects", LayerProblem::Empty, true, 10));
    CHECK(has(f, "register", LayerProblem::NotAChoice, true));
    CHECK(has(f, "reply_words_max", LayerProblem::OutOfRange, true));
  }
  SUBCASE("instructions past 35 words or 210 characters") {
    std::string many;
    for (int i = 0; i < 36; ++i) many += "word ";
    CHECK(has(read(layer(std::string(kHead) + "instructions = \"" + many + "\"\n")), "instructions",
              LayerProblem::TooManyWords, true));
    CHECK(has(read(layer(std::string(kHead) + "instructions = \"" + std::string(211, 'x') + "\"\n")), "instructions",
              LayerProblem::TooManyChars, true));
  }
  SUBCASE("a newer schema, a wrong target, a bad revision, a hash or a provenance") {
    CHECK(has(read(layer("schema = 2\ntarget = \"persona:kale\"\n")), "", LayerProblem::NewerSchema, true));
    CHECK(has(read(layer("target = \"persona:kale\"\n")), "", LayerProblem::BadSchema, true));
    CHECK(has(read(layer("schema = 1\ntarget = \"persona:other\"\n")), "", LayerProblem::Target, true));
    CHECK(has(read(layer("schema = 1\ntarget = \"persona:kale\"\nrev = -1\n")), "", LayerProblem::WrongType, true));
    CHECK(has(read(layer("schema = 1\ntarget = \"persona:kale\"\nbase_hash = \"xyz\"\n")), "", LayerProblem::WrongType,
              true));
    CHECK(has(read(layer("schema = 1\ntarget = \"persona:kale\"\ndrafted = \"machine\"\n")), "", LayerProblem::WrongType,
              true));
  }
  SUBCASE("not TOML, refused before the parse, or past 8 KB") {
    CHECK(has(read(layer("schema = = 1\n")), "", LayerProblem::NotRead, true));
    CHECK(has(read(layer("schema = 1\ninstructions = \"\"\"\nx\n\"\"\"\n")), "", LayerProblem::NotRead, true));
    CHECK(has(read(layer(std::string(kHead) + "# " + std::string(8200, 'x') + "\n")), "", LayerProblem::NotRead, true));
  }
  SUBCASE("a path that names no persona's layer") {
    CHECK(has(read(layer(std::string(kHead), "layers/card-x.toml")), "", LayerProblem::Target, true));
  }
  // The words name the field and the cap, never the value.
  for (const LayerFinding& f : read(layer(std::string(kHead) + "subjects = [\"secret words here\", \"\"]\n"))) {
    CHECK(words(f).find("secret") == std::string::npos);
    CHECK(to_content_error(f).problem.find("secret") == std::string::npos);
  }
}

TEST_CASE("layer: its text, written by the editor, reads back as it was; one paragraph, escaped") {
  PersonaLayer l;
  l.file = "layers/persona-kale.toml";
  l.persona = "kale";
  l.rev = 7;
  l.base_hash = "00ff00ff00ff00ff";
  l.assisted = true;
  l.fields["instructions"] = "Says \"aye\" \\ never no,\nand hums.";
  l.fields["subjects"] = {"the road east", "weather, and whether it holds"};
  l.fields["register"] = "broken";
  l.fields["reply_words_max"] = 12;
  const std::string text = layer_text(l);
  CHECK(text.find("schema = 1\ntarget = \"persona:kale\"\nrev = 7\n") != std::string::npos);
  CHECK(text.find("drafted = \"assisted\"") != std::string::npos);
  std::optional<PersonaLayer> back;
  CHECK(read(layer(text), &back).empty());
  REQUIRE(back);
  PersonaLayer want = l;
  want.fields["instructions"] = "Says \"aye\" \\ never no, and hums.";  // the line break is a space
  CHECK(*back == want);
  // A name with an accent, as the player types it, stays as it is.
  l.fields["instructions"] = "Greets the Kal\xC3\xA9 of the roads warmly.";
  CHECK(read(layer(layer_text(l)), &back).empty());
  CHECK(back->fields["instructions"] == l.fields["instructions"]);
}

TEST_CASE("layer: the load merges it into its persona; the shipped record and the shipped lints stay as shipped") {
  Core core;
  const ContentFile l = layer(std::string(kHead) +
                              "instructions = \"Hums an old road song under his breath.\"\n"
                              "subjects = [\"an old road song\"]\nregister = \"formal\"\nreply_words_max = 20\n");
  REQUIRE(core.load_content(with(l), Realm::Game).empty());
  const Persona* kale = core.directory().pack().persona("kale");
  REQUIRE(kale != nullptr);
  REQUIRE(kale->layer);
  CHECK(kale->layer->rev == 2);
  CHECK(kale->reply_words_max == 20);
  CHECK(kale->reg == Register::Formal);
  CHECK(kale->resolved(0)["instructions"] == "Hums an old road song under his breath.");
  CHECK(kale->resolved(0)["register"] == "formal");
  CHECK_FALSE(kale->resolved(0, false).contains("instructions"));
  CHECK(kale->resolved(0, false)["register"] == "colloquial");
  CHECK_FALSE(kale->record.contains("instructions"));  // the shipped record, as shipped
  CHECK(kale->hash == record_hash([&] {
          for (const ContentFile& f : builtin_content()) {
            if (f.path == "personas/kale.toml") return f.text;
          }
          return std::string();
        }()));
  // The pilot's voice leaves little room (WHY§472): block 4 with the player's note passes its cap, a warning for the
  // player's own, and the layer still loads.
  const auto& lf = core.layer_findings();
  CHECK(has(lf, "instructions", LayerProblem::Block4, false));
  CHECK(std::any_of(core.content_warnings().begin(), core.content_warnings().end(),
                    [](const ContentError& e) { return e.file == "layers/persona-kale.toml"; }));
}

TEST_CASE("layer: wording warns, but template marks and a word of the safety list refuse the layer alone") {
  Core core;
  SUBCASE("warnings: a banned word, a quoted line, an effect word, a subject that is an order") {
    const ContentFile l = layer(std::string(kHead) +
                                "instructions = \"Calls every buyer \\\"friend\\\" and is gruff about the quest.\"\n"
                                "subjects = [\"tell me about the roads\"]\n");
    REQUIRE(core.load_content(with(l), Realm::Game).empty());
    const auto& f = core.layer_findings();
    CHECK(has(f, "instructions", LayerProblem::Banned, false));
    CHECK(has(f, "instructions", LayerProblem::Quote, false));
    CHECK(has(f, "instructions", LayerProblem::Effect, false));
    CHECK(has(f, "subjects", LayerProblem::NounPhrase, false, 0));
    CHECK(core.directory().pack().persona("kale")->layer);
  }
  SUBCASE("a template mark: refused, the shipped profile alone") {
    const ContentFile l = layer(std::string(kHead) + "instructions = \"Says {{ name }} twice.\"\nreply_words_max = 12\n");
    REQUIRE(core.load_content(with(l), Realm::Game).empty());
    CHECK(has(core.layer_findings(), "instructions", LayerProblem::Template, true));
    const Persona* kale = core.directory().pack().persona("kale");
    CHECK_FALSE(kale->layer);
    CHECK(kale->reply_words_max == 35);
    CHECK(std::any_of(core.content_warnings().begin(), core.content_warnings().end(), [](const ContentError& e) {
      return e.problem.find("refused: ") != std::string::npos;
    }));
  }
  SUBCASE("a schema error: refused, the pack loads") {
    const ContentFile l = layer(std::string(kHead) + "register = \"shouting\"\n");
    REQUIRE(core.load_content(with(l), Realm::Game).empty());
    CHECK(has(core.layer_findings(), "register", LayerProblem::NotAChoice, true));
    CHECK_FALSE(core.directory().pack().persona("kale")->layer);
  }
  SUBCASE("a layer whose persona the index lacks") {
    const ContentFile l = layer("schema = 1\ntarget = \"persona:nobody\"\n", "layers/persona-nobody.toml");
    REQUIRE(core.load_content(with(l), Realm::Game).empty());
    CHECK(has(core.layer_findings(), "", LayerProblem::Target, true));
  }
  SUBCASE("a shipped file in layers/ refuses the pack: only the overrides hold one") {
    ContentFile l = layer(std::string(kHead));
    l.origin = ContentOrigin::Shipped;
    CHECK_FALSE(core.load_content(with(l), Realm::Game).empty());
  }
}

TEST_CASE("layer: a word of the safety list is an error, by its fingerprint (O107)") {
  // The shipped list is empty: a test lexicon of our own holds one invented word's fingerprint.
  std::vector<ContentFile> files = builtin_content();
  const std::optional<std::string> fp = safety_fingerprint("zorblax");
  REQUIRE(fp);
  for (ContentFile& f : files) {
    if (f.path == "lexicon/safety.toml") f.text = "format = 1\nalways = [\"" + *fp + "\"]\ngentle = []\n";
  }
  const LayerCheck c =
      check_layer(files, layer(std::string(kHead) + "subjects = [\"the zorblax trade\", \"the road\"]\n"));
  CHECK_FALSE(c.ok());
  CHECK(has(c.findings, "subjects", LayerProblem::Safety, true, 0));
  CHECK_FALSE(c.layer);
}

TEST_CASE("layer: the editor's check runs the same load, and names the candidate's findings alone") {
  const std::vector<ContentFile> files = builtin_content();
  SUBCASE("a clean layer: ok, read and merged") {
    const LayerCheck c = check_layer(files, layer(std::string(kHead) + "subjects = [\"the road east\"]\n"));
    CHECK(c.ok());
    CHECK(c.findings.empty());
    REQUIRE(c.layer);
    CHECK(c.layer->fields["subjects"][0] == "the road east");
  }
  SUBCASE("an error and a warning together") {
    const LayerCheck c =
        check_layer(files, layer(std::string(kHead) + "instructions = \"Talks of the quest.\"\nreply_words_max = 9\n"));
    CHECK_FALSE(c.ok());
    CHECK(has(c.findings, "reply_words_max", LayerProblem::OutOfRange, true));
    CHECK_FALSE(c.layer);
  }
  SUBCASE("the candidate replaces the file of its path; another layer's findings are not its") {
    std::vector<ContentFile> more = files;
    more.push_back(layer(std::string(kHead) + "register = \"shouting\"\n"));
    more.push_back(layer("schema = 1\ntarget = \"persona:nobody\"\n", "layers/persona-nobody.toml"));
    const LayerCheck c = check_layer(more, layer(std::string(kHead) + "register = \"chant\"\n"));
    CHECK(c.ok());
    CHECK(c.findings.empty());
  }
  SUBCASE("content that does not load: one finding for the whole file") {
    std::vector<ContentFile> broken = files;
    std::erase_if(broken, [](const ContentFile& f) { return f.path == "identity/personas.toml"; });
    const LayerCheck c = check_layer(broken, layer(std::string(kHead)));
    REQUIRE(c.findings.size() == 1);
    CHECK(c.findings[0].problem == LayerProblem::ContentDown);
    CHECK(c.findings[0].field.empty());
  }
}

}  // namespace en::core
