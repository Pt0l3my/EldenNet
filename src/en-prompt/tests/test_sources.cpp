// Template sources (ARCH §2.3): the same templates give the same messages and the same errors from a folder and from
// memory, the memory source keeps to the name rules, and rendering from memory opens no file.
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "support.hpp"

using namespace en_test;

namespace {

using Templates = std::map<std::string, std::string>;
using Entry = prompt::TemplateSource::Entry;

std::shared_ptr<const prompt::MemorySource> in_memory(const Templates& templates) {
  return std::make_shared<const prompt::MemorySource>(templates);
}

void write_all(const TempTree& tree, const Templates& templates) {
  for (const auto& [name, text] : templates) tree.write(name, text);
}

std::string described(const prompt::TemplateError& error) {
  std::string out = "error at " + prompt::to_string(error.site()) + ": " + error.message();
  for (const prompt::Finding& finding : error.findings()) out += "\n  finding " + prompt::to_string(finding);
  for (const prompt::Site& caller : error.trace()) out += "\n  rendered from " + prompt::to_string(caller);
  return out;
}

// What a render gives, written out: its messages with their roles, parts and breakpoints, or its error with the
// place, the findings and the calls that led there.
std::string outcome(const prompt::Environment& environment, const std::string& name, const json& context) {
  try {
    return prompt::to_json(environment.render(name, context)).dump();
  } catch (const prompt::TemplateError& error) {
    return described(error);
  }
}

std::string inline_outcome(const prompt::Environment& environment, const std::string& source, const json& context) {
  try {
    return prompt::to_json(environment.render_source(source, context)).dump();
  } catch (const prompt::TemplateError& error) {
    return described(error);
  }
}

std::string lint_outcome(const prompt::Environment& environment, const std::string& name, const json* context) {
  std::string out;
  for (const prompt::Finding& finding : environment.lint(name, context)) out += prompt::to_string(finding) + "\n";
  return out;
}

// Templates for everything a source decides: files of each extension, named in full or bare, with '/' or '\'; a
// folder of numbered parts beside what is no part; render_template and render_subcomponent; cache breakpoints; and
// the errors of names, folders, composition and loading.
Templates twin_templates() {
  Templates templates{
      {"main.j2",
       "[ system ]\nFrame for {{ speaker }}.\n[ cache ]\n[ user ]\n{{ render_template(\"registers/calm\") }}\n"
       "{{ render_template(\"registers\\\\calm.md\") }}"},
      {"registers/calm.md", "Speak slowly, {{ speaker }}."},
      {"registers/notes.txt", "not a template"},
      {"note.prompt", "[ user ]\n{{ render_template(\"line\", object(\"words\", \"Well met\", \"loud\", true)) }}"},
      {"line.j2", "{{ speaker }} says {{ quote(words) }}{% if loud %}!{% endif %}"},
      {"dialogue/10_task.j2", "Part ten."},
      {"dialogue/9_scene.j2", "Part nine: {{ render_template(\"line\", object(\"words\", \"Hello\", \"loud\", false)) }}"},
      {"dialogue/2_frame.md", "[ system ]\nPart two.\n[ cache ]\n[ user ]"},
      {"dialogue/05_contract.md", "Part five."},
      {"dialogue/05b_world.j2", "Part five b."},
      {"dialogue/README.txt", "not a template"},
      {"dialogue/.draft.j2", "{{ never read"},
      {"dialogue/drafts/10_old.j2", "A folder inside a folder is not one of its parts."},
      {"bio/10_summary.j2", "Summary for {{ render_mode }}."},
      {"bio/20_private.j2", "{% if render_mode == \"self\" %}Private thoughts.{% endif %}"},
      {"profile.j2",
       "[ user ]\n{{ render_subcomponent(\"bio\", \"observed\") }}\n---\n{{ render_subcomponent(\"bio\", \"self\") }}"},
      {"split.j2", "[ user ]\nFirst stretch.\n[ cache ]\nSecond stretch.\n[ assistant ]\nA reply."},
      {"leak.j2", "[ user ]\n{% set secret = \"kept\" %}{{ render_template(\"peek\") }}"},
      {"peek.j2", "{{ secret }}"},
      {"cycle_a.j2", "[ user ]\n{{ render_template(\"cycle_b\") }}"},
      {"cycle_b.j2", "{{ render_template(\"cycle_a\") }}"},
      {"loop/10_again.j2", "{{ render_template(\"loop\") }}"},
      {"missing.j2", "[ user ]\nA first line.\n{{ render_template(\"nowhere\") }}"},
      {"twice.j2", "a"},
      {"twice.md", "b"},
      {"ambiguous.j2", "[ user ]\n{{ render_template(\"twice\") }}"},
      {"not_folder.j2", "[ user ]\n{{ render_subcomponent(\"profile.j2\") }}"},
      {"unnumbered/10_summary.j2", "[ user ]\nA summary."},
      {"unnumbered/notes.j2", "Loose notes."},
      {"unnumbered/zz.j2", "More loose notes."},
      {"odd/10summary.j2", "[ user ]\nA summary."},
      {"empty/README.txt", "nothing to render"},
      {"unknown.j2", "[ user ]\n{{ shout(speaker) }}"},
      {"arity.j2", "[ user ]\n{{ at(speaker) }}\n{{ round(1.5, 1, 2) }}"},
      {"include.j2", "[ user ]\n{% include \"line.j2\" %}"},
      {"unclosed.j2", "[ user ]\n{# a comment that never closes"},
      {"stray.j2", "Text before any marker."},
      {"deep.j2", "[ user ]\n{{ render_template(\"chain/t1\") }}"},
  };
  for (int i = 1; i <= 20; ++i) {
    templates["chain/t" + std::to_string(i) + ".j2"] =
        i == 20 ? std::string("the end") : "{{ render_template(\"chain/t" + std::to_string(i + 1) + "\") }}";
  }
  return templates;
}

// Changes the current folder, and changes it back.
class CurrentFolder {
 public:
  explicit CurrentFolder(const std::filesystem::path& folder) : saved_(std::filesystem::current_path()) {
    std::filesystem::current_path(folder);
  }
  ~CurrentFolder() {
    std::error_code error;
    std::filesystem::current_path(saved_, error);
  }
  CurrentFolder(const CurrentFolder&) = delete;
  CurrentFolder& operator=(const CurrentFolder&) = delete;

 private:
  std::filesystem::path saved_;
};

}  // namespace

TEST_CASE("the same templates render identically from a folder and from memory") {
  const Templates templates = twin_templates();
  TempTree tree;
  write_all(tree, templates);
  const prompt::Environment folder(tree.root());
  const prompt::Environment memory(in_memory(templates));
  const json orla = {{"speaker", "Orla"}};

  std::vector<std::string> names;
  for (const auto& entry : templates) names.push_back(entry.first);
  for (const char* name : {"main", "note", "registers\\calm.md", "registers", "dialogue/", "dialogue\\05b_world.j2",
                           "dialogue/drafts", "twice", "nowhere", "../main", "a//b"}) {
    names.emplace_back(name);
  }
  for (const std::string& name : names) {
    CAPTURE(name);
    CHECK(outcome(memory, name, orla) == outcome(folder, name, orla));
    CHECK(lint_outcome(memory, name, &orla) == lint_outcome(folder, name, &orla));
    CHECK(lint_outcome(memory, name, nullptr) == lint_outcome(folder, name, nullptr));
  }
  const std::string source = "[ user ]\n{{ render_template(\"registers/calm\") }} {{ render_subcomponent(\"bio\", \"self\") }}";
  CHECK(inline_outcome(memory, source, orla) == inline_outcome(folder, source, orla));

  // And they are the right ones, so the two do not agree by failing alike.
  const auto main = memory.render("main", orla);
  REQUIRE(main.size() == 2);
  CHECK(main[0].role == Role::system);
  CHECK(main[0].text() == "Frame for Orla.");
  CHECK(main[0].cache());
  CHECK(main[1].text() == "Speak slowly, Orla.\nSpeak slowly, Orla.");
  CHECK(memory.render("note", orla)[0].text() == "Orla says \"Well met\"!");
  CHECK(memory.render_source(source, orla)[0].text() == "Speak slowly, Orla. Summary for self.\nPrivate thoughts.");

  const auto dialogue = memory.render("dialogue", orla);
  REQUIRE(dialogue.size() == 2);
  CHECK(dialogue[0].text() == "Part two.");
  CHECK(dialogue[0].cache());
  CHECK(dialogue[1].text() == "Part five.\nPart five b.\nPart nine: Orla says \"Hello\"\nPart ten.");
  CHECK(memory.render("profile", orla)[0].text() == "Summary for observed.\n---\nSummary for self.\nPrivate thoughts.");

  const auto split = memory.render("split", orla);
  REQUIRE(split.size() == 2);
  REQUIRE(split[0].parts.size() == 2);
  CHECK(split[0].parts[0].text == "First stretch.");
  CHECK(split[0].parts[0].cache);
  CHECK(split[0].parts[1].text == "\nSecond stretch.");
  CHECK(split[1].role == Role::assistant);

  const auto leak = error_of([&] { memory.render("leak", orla); });
  CHECK(leak.site().template_name == "peek.j2");
  CHECK(says(leak, "unknown variable 'secret'"));
  REQUIRE(leak.trace().size() == 1);
  CHECK(leak.trace()[0].template_name == "leak.j2");
  CHECK(says(error_of([&] { memory.render("cycle_a", orla); }), "a render cycle: cycle_a.j2 -> cycle_b.j2 -> cycle_a.j2"));
  CHECK(says(error_of([&] { memory.render("loop", orla); }), "a render cycle: loop -> loop/10_again.j2 -> loop"));
  CHECK(says(error_of([&] { memory.render("deep", orla); }), "renders nest deeper than 16 levels"));
  const auto missing = error_of([&] { memory.render("missing", orla); });
  CHECK(missing.site().line == 3);
  CHECK(says(missing, "no template named 'nowhere'"));
  CHECK(says(error_of([&] { memory.render("ambiguous", orla); }), "'twice' is ambiguous: twice.j2 and twice.md both exist"));
  CHECK(says(error_of([&] { memory.render("not_folder", orla); }), "takes a folder of numbered parts"));
  // Of two parts without a number, the first by name is named, from either source.
  const auto unnumbered = error_of([&] { memory.render("unnumbered", orla); });
  CHECK(unnumbered.site().template_name == "unnumbered/notes.j2");
  CHECK(says(unnumbered, "has no number"));
  CHECK(says(error_of([&] { memory.render("odd", orla); }), "needs '_' after its number"));
  CHECK(says(error_of([&] { memory.render("empty", orla); }), "the folder has no numbered parts"));
  CHECK(says(error_of([&] { memory.render("registers/notes.txt", orla); }), "is not a template file"));
  CHECK(says(error_of([&] { memory.render("unknown", orla); }), "unknown function 'shout'"));
  CHECK(error_of([&] { memory.render("arity", orla); }).findings().size() == 2);
  CHECK(says(error_of([&] { memory.render("include", orla); }), "write {{ render_template(\"line.j2\") }}"));
  CHECK(memory.lint("dialogue", &orla).empty());
  CHECK(has_rule(memory.lint("missing"), "missing-template"));
  CHECK(has_rule(memory.lint("empty"), "folder"));
}

TEST_CASE("the memory source refuses the names the folder source refuses") {
  TempTree tree;
  tree.write("outside.j2", "a secret");
  const std::string main = "[ user ]\n{{ render_template(name) }}";
  tree.write("inner/main.j2", main);
  const prompt::Environment folder(tree.root() / "inner");
  const prompt::Environment memory(in_memory({{"main.j2", main}}));
  for (const char* name : {"../outside", "/etc/hosts", "a//b", "C:/x", "..", ".", "./main.j2", "bio/./10_a.j2",
                           "inner/../../outside", "", "/", "a b.j2", "what?.j2", "\\\\host\\share.j2",
                           "caf\xC3\xA9.j2"}) {
    CAPTURE(name);
    const auto by_folder = error_of([&] { folder.render("main.j2", json{{"name", name}}); });
    const auto by_memory = error_of([&] { memory.render("main.j2", json{{"name", name}}); });
    CHECK(described(by_memory) == described(by_folder));
    CHECK(by_folder.site().line == 2);
    // As a key, the name is refused when the source is made, in the same words.
    const auto as_key = error_of([&] { const prompt::MemorySource source({{name, "[ user ]\nA line."}}); });
    CHECK(as_key.message() == by_folder.message());
    CHECK(as_key.site().template_name == name);
  }
}

TEST_CASE("a memory source holds each name once, in its checked form, as a file or a folder") {
  const auto twice =
      error_of([] { const prompt::MemorySource source({{"registers/calm.md", "a"}, {"registers\\calm.md", "b"}}); });
  CHECK(twice.message() == "'registers/calm.md' and 'registers\\calm.md' name the same template");
  CHECK(twice.site().template_name == "registers\\calm.md");
  CHECK(says(error_of([] { const prompt::MemorySource source({{"a.j2", "a"}, {"a.j2/", "b"}}); }),
             "name the same template"));
  const auto both = error_of([] { const prompt::MemorySource source({{"bio.j2", "a"}, {"bio.j2/10_part.j2", "b"}}); });
  CHECK(both.message() == "'bio.j2' is both a template and a folder");
  CHECK(both.site().template_name == "bio.j2");

  const prompt::MemorySource source(
      {{"registers\\calm.md", "[ user ]\nCalm."}, {"registers/deep/10_x.j2", "x"}, {"notes.txt", "n"}});
  CHECK(source.find("registers/calm.md") == Entry::file);
  CHECK(source.find("registers\\calm.md") == Entry::none);  // a source is asked in the checked form only
  CHECK(source.find("registers") == Entry::folder);
  CHECK(source.find("registers/deep") == Entry::folder);
  CHECK(source.find("registers/calm") == Entry::none);
  CHECK(source.files("registers") == std::vector<std::string>{"calm.md"});
  CHECK(source.files("registers/deep") == std::vector<std::string>{"10_x.j2"});
  CHECK_FALSE(source.files("nowhere").has_value());
  CHECK(source.read("registers/calm.md") == "[ user ]\nCalm.");
  CHECK_FALSE(source.read("registers").has_value());
  CHECK_FALSE(source.leads_outside("registers/calm.md"));

  const prompt::Environment environment(std::make_shared<const prompt::MemorySource>(source));
  CHECK(environment.render("registers\\calm", json::object())[0].text() == "Calm.");
  CHECK(says(error_of([&] { environment.render("notes.txt", json::object()); }), "is not a template file"));
}

TEST_CASE("rendering from memory opens no file") {
  // Names that exist nowhere on disk.
  const Templates templates{
      {"held-in-memory/main.j2",
       "[ system ]\n{{ render_template(\"held-in-memory/registers/calm\") }}\n[ cache ]\n"
       "{{ render_subcomponent(\"held-in-memory/bio\", \"self\") }}"},
      {"held-in-memory/registers/calm.md", "Speak slowly."},
      {"held-in-memory/bio/10_summary.j2", "[ user ]\nSummary for {{ render_mode }}."},
      {"held-in-memory/bio/20_more.j2", "More."},
  };
  const auto renders_from_memory = [&](const prompt::Environment& environment) {
    const auto messages = environment.render("held-in-memory/main", json::object());
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].text() == "Speak slowly.");
    CHECK(messages[0].cache());
    CHECK(messages[1].text() == "Summary for self.\nMore.");
    CHECK(environment.lint("held-in-memory/main").empty());
    CHECK(environment
              .render_source("[ user ]\n{{ render_template(\"held-in-memory/registers/calm.md\") }}", json::object())[0]
              .text() == "Speak slowly.");
  };

  // From an empty current folder, where a look on disk would find nothing.
  TempTree empty;
  {
    const CurrentFolder here(empty.root());
    renders_from_memory(prompt::Environment(in_memory(templates)));
  }
  CHECK(std::filesystem::is_empty(empty.root()));

  // Among files of the same names with another text, which a read from disk would show.
  TempTree decoys;
  for (const auto& entry : templates) decoys.write(entry.first, "[ user ]\nRead from disk.");
  {
    const CurrentFolder here(decoys.root());
    const prompt::Environment disk(std::filesystem::path("."));
    CHECK(disk.render("held-in-memory/main", json::object())[0].text() == "Read from disk.");
    renders_from_memory(prompt::Environment(in_memory(templates)));
  }
}

TEST_CASE("a folder source answers only for names in the checked form, inside its root") {
  TempTree tree;
  tree.write("outside.j2", "a secret");
  tree.write("inner/main.j2", "[ user ]\nInside.");
  tree.write("inner/parts/10_a.j2", "a");
  const prompt::FolderSource source(tree.root() / "inner");
  CHECK(source.root() == tree.root() / "inner");
  CHECK(source.find("main.j2") == Entry::file);
  CHECK(source.find("parts") == Entry::folder);
  CHECK(source.find("main") == Entry::none);
  CHECK(source.read("main.j2") == "[ user ]\nInside.");
  CHECK(source.files("parts") == std::vector<std::string>{"10_a.j2"});
  CHECK_FALSE(source.leads_outside("parts/10_a.j2"));
  // Even when it is asked directly, it never lists or reads anything outside its root.
  for (const char* name : {"../outside.j2", "parts/../main.j2", "/main.j2", "parts\\10_a.j2", "parts/", "", "."}) {
    CAPTURE(name);
    CHECK(source.find(name) == Entry::none);
    CHECK_FALSE(source.read(name).has_value());
    CHECK_FALSE(source.files(name).has_value());
    CHECK(source.leads_outside(name));
  }
  // A link is seen for what it leads to, and one that leads out is neither read nor listed.
  std::error_code error;
  std::filesystem::create_symlink(tree.root() / "outside.j2", tree.root() / "inner" / "link.j2", error);
  if (!error) {
    CHECK(source.find("link.j2") == Entry::file);
    CHECK(source.leads_outside("link.j2"));
    CHECK_FALSE(source.read("link.j2").has_value());
  }
  std::filesystem::create_directory_symlink(tree.root(), tree.root() / "inner" / "up", error);
  if (!error) {
    CHECK(source.find("up") == Entry::folder);
    CHECK(source.leads_outside("up"));
    CHECK_FALSE(source.files("up").has_value());
  }
}

TEST_CASE("an environment needs a source, and a folder source a folder") {
  CHECK_THROWS_AS(prompt::Environment(std::shared_ptr<const prompt::TemplateSource>()), std::invalid_argument);
  TempTree tree;
  tree.write("file.j2", "x");
  CHECK_THROWS_AS(prompt::Environment(tree.root() / "missing"), std::invalid_argument);
  CHECK_THROWS_AS(prompt::FolderSource(tree.root() / "file.j2"), std::invalid_argument);
  const prompt::Environment environment(tree.root());
  CHECK(environment.source().find("file.j2") == Entry::file);  // the folder's own source
}
