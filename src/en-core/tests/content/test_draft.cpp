// The "draft with AI" helper's prompt and reply (profile.hpp; NPC §4.7, §4.8's draft helper tests, the mock model's
// replies written here): the prompt holds the notes, the kind and the player's current text and nothing else; a reply's
// sentence naming someone neither the notes nor the current text hold is dropped without a mark; a safety hit discards
// the draft; caps are kept at a sentence's end. Every text is invented.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/core/core.hpp"
#include "en/core/profile.hpp"
#include "en/core/speech_lint.hpp"

namespace en::core {
namespace {

std::vector<ContentFile> others() {
  std::vector<ContentFile> out;
  for (ContentFile& f : builtin_content()) {
    if (f.path.rfind("prompts/", 0) == 0) out.push_back(std::move(f));
  }
  return out;
}

Lexicon lexicon(std::vector<std::string> safety = {}) {
  Lexicon l;
  for (const std::string& w : safety) l.safety_always.push_back(*safety_fingerprint(w));
  std::sort(l.safety_always.begin(), l.safety_always.end());
  return l;
}

std::string joined(const std::vector<proto::Message>& ms) {
  std::string out;
  for (const proto::Message& m : ms) out += m.joined() + "\n";
  return out;
}

}  // namespace

TEST_CASE("draft: the prompt is ours, from the pack, and reads the notes, the kind and the current text alone") {
  DraftInput in;
  in.notes = "a ferryman who hums and counts coins twice";
  in.kind_phrase = "a named person";
  auto m = draft_messages(others(), in);
  REQUIRE(m);
  REQUIRE(m->size() == 2);
  CHECK((*m)[0].role == proto::Role::System);
  CHECK((*m)[1].role == proto::Role::User);
  const std::string text = joined(*m);
  CHECK(text.find("Return JSON only.") != std::string::npos);
  CHECK(text.find("a ferryman who hums and counts coins twice") != std::string::npos);
  CHECK(text.find("Kind: a named person.") != std::string::npos);
  CHECK(text.find("current text, to improve") == std::string::npos);  // nothing to revise
  in.current = {{"instructions", "Counts every coin twice."}, {"register", "formal"}};
  m = draft_messages(others(), in);
  REQUIRE(m);
  const std::string revising = joined(*m);
  CHECK(revising.find("current text, to improve: {\"instructions\":\"Counts every coin twice.\"}") != std::string::npos);
  CHECK(revising.find("formal") == std::string::npos);  // the drafted fields alone
  // Notes past 400 characters are cut.
  in.notes = std::string(500, 'n');
  m = draft_messages(others(), in);
  REQUIRE(m);
  CHECK(joined(*m).find(std::string(401, 'n')) == std::string::npos);
  CHECK_FALSE(draft_messages({}, in));
  CHECK(draft_schema()["additionalProperties"] == false);
}

TEST_CASE("draft: a reply read into proposals; unknown fields dropped, caps kept") {
  DraftInput in;
  in.notes = "a ferryman who hums";
  const Lexicon lex = lexicon();
  DraftResult r = read_draft(
      "Here it is:\n```json\n{\"instructions\": \"Hums low over the oars. Counts coins twice.\", "
      "\"subjects\": [\"the river's moods\", \"coins and their weight\"], \"identity\": \"x\"}\n```",
      in, lex);
  CHECK_FALSE(r.unreadable);
  CHECK(r.draft == nlohmann::json{{"instructions", "Hums low over the oars. Counts coins twice."},
                                  {"subjects", {"the river's moods", "coins and their weight"}}});
  // Past 35 words: cut at a sentence's end.
  std::string longer = "Hums low over the oars.";
  for (int i = 0; i < 8; ++i) longer += " He counts every coin twice.";
  r = read_draft("{\"instructions\": \"" + longer + "\"}", in, lex);
  CHECK(r.cut == 1);
  const std::string kept = r.draft["instructions"].get<std::string>();
  CHECK(kept.back() == '.');
  CHECK(kept.size() < longer.size());
  // Subjects past ten words or past the eighth are left out.
  r = read_draft("{\"subjects\": [\"a b c d e f g h i j k\", \"one\", \"two\", \"three\", \"four\", \"five\", \"six\", "
                 "\"seven\", \"eight\", \"nine\"]}",
                 in, lex);
  CHECK(r.draft["subjects"].size() == 8);
  CHECK(r.cut == 2);
  CHECK(read_draft("no json here", in, lex).unreadable);
  CHECK(read_draft("[1, 2]", in, lex).unreadable);
}

TEST_CASE("draft: a sentence naming someone the writer did not is dropped without a mark; theirs stay") {
  DraftInput in;
  in.notes = "a ferryman on the Grey Water who owes Old Tam money";
  const Lexicon lex = lexicon();
  const DraftResult r = read_draft(
      "{\"instructions\": \"Rows the Grey Water at dusk. Swears by Saint Morwen of the Deep. I'd say he owes Old Tam.\", "
      "\"subjects\": [\"debts to Old Tam\", \"the tolls of Hollowmere\"]}",
      in, lex);
  CHECK(r.dropped == 2);
  CHECK(r.draft["instructions"] == "Rows the Grey Water at dusk. I'd say he owes Old Tam.");
  CHECK(r.draft["subjects"] == nlohmann::json{"debts to Old Tam"});
  // A name in the player's current text counts as theirs.
  in.current = {{"instructions", "Fears the Hollowmere tolls."}};
  CHECK(read_draft("{\"subjects\": [\"the tolls of Hollowmere\"]}", in, lex).dropped == 0);
}

TEST_CASE("draft: a word of the safety list discards the whole draft") {
  DraftInput in;
  in.notes = "a ferryman";
  const DraftResult r =
      read_draft("{\"instructions\": \"Rows at dusk.\", \"subjects\": [\"zorblax and rivers\"]}", in, lexicon({"zorblax"}));
  CHECK(r.discarded);
  CHECK(r.draft.empty());
}

}  // namespace en::core
