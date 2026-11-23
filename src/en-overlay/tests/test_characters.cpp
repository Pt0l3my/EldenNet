// The Characters pages and the profile editor in EldenNet's space (characters.hpp, space.hpp; PAGES §4.6.4, NPC §4.7,
// S7.2.b; GraceUI's D215 to D218 and D221), in every build: the page data (the master list, the two families, the
// editor block, the slow and sensitive marks), the bodies from invented facts (the list, a general profile of first-meeting
// fields alone, the editor's document over the player's layer), the commands checked at the one entry point and handed
// to the DLL, their answers later with their reply handles (a check's messages, a refused save, a conflict, a save's
// chosen row, a draft's proposals), what is published and when, and the spoiler golden: nothing a projection does not
// publish reaches a view (PAGES §3; GraceUI's invariants 1 and 2).
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "characters_fixture.hpp"
#include "en/overlay/space.hpp"

using namespace en::overlay;
using nlohmann::json;

namespace {

std::string cmd(std::string_view name, const json& args, int id = 9) {
  return json{{"v", 1}, {"t", "cmd"}, {"space", "eldennet"}, {"id", id}, {"name", name}, {"args", args}}.dump();
}

std::string sub(std::string_view topic) {
  return json{{"v", 1}, {"t", "sub"}, {"space", "eldennet"}, {"id", 0}, {"topic", topic}}.dump();
}

std::string unsub(std::string_view topic) {
  return json{{"v", 1}, {"t", "unsub"}, {"space", "eldennet"}, {"id", 0}, {"topic", topic}}.dump();
}

SpaceFacts facts_with(const CharacterFacts* c) {
  SpaceFacts f;
  f.characters = c;
  return f;
}

json doc_of(const json& body, std::string_view id) {
  for (const json& f : body["fields"]) {
    if (f["id"] == id) return f;
  }
  return json();
}

// Every string a body holds, for the spoiler golden.
void strings_of(const json& j, std::vector<std::string>& out) {
  if (j.is_string()) out.push_back(j.get<std::string>());
  if (j.is_array() || j.is_object()) {
    for (const json& v : j) strings_of(v, out);
  }
}

}  // namespace

TEST_CASE("characters: the page data: Named NPCs' master list, its families, the editor and the marks") {
  const json pages = json::parse(space_pages());
  const json* page = nullptr;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "characters.named") page = &p;
  }
  REQUIRE(page != nullptr);
  CHECK((*page)["increment"] == "0.2");
  CHECK((*page)["master"]["topic"] == kNamedList);
  CHECK((*page)["master"]["detail"] == kNamedFamily);
  CHECK((*page)["master"]["key"] == "id");
  const json& tab = (*page)["tabs"][0];
  CHECK(tab["sections"][0]["blocks"][0]["topic"] == kNamedFamily);
  const json& editor = tab["sections"][1]["blocks"][0];
  CHECK(editor["kind"] == "editor");
  CHECK(editor["topic"] == kProfilesFamily);
  CHECK(editor["editor"]["save"] == "profile.save");
  CHECK(editor["editor"]["draft"] == "profile.draft");
  CHECK(editor["editor"]["notes_max"] == 400);
  int families = 0;
  for (const json& t : pages["topics"]) families += t.value("family", false) ? 1 : 0;
  CHECK(families == 2);
  CHECK(pages["slow_commands"] == json{"profile.draft"});
  CHECK(pages["sensitive_commands"] == json{"providers.add", "providers.remove", "providers.key"});
  CHECK(pages["minor"] == 8);
  for (const char* c : {"profile.check", "profile.save", "profile.reset", "profile.restore", "profile.draft"}) {
    CHECK(std::find(pages["commands"].begin(), pages["commands"].end(), c) != pages["commands"].end());
  }
  // Sensitive settings: the developer view and the recordings, and nothing else (S11.4.e).
  std::vector<std::string> sensitive;
  const json description = json::parse(settings_description());
  for (const json& s : description["settings"]) {
    if (s.value("sensitive", false)) sensitive.push_back(s["key"]);
  }
  CHECK(sensitive == std::vector<std::string>{"debug.panel", "debug.record_sessions"});
}

TEST_CASE("characters: the list and a general profile hold what a first meeting shows, in our labels") {
  const CharacterFacts* f = test::characters_fixture("title");
  const json list = Characters::list_body(f);
  REQUIRE(list["rows"].size() == 2);
  CHECK(list["rows"][0] == json{{"id", "persona.ferryman"}, {"name", "Old Bram"}, {"role", "a ferryman on the grey river"}});
  CHECK(list["columns"][0]["id"] == "name");
  const json general = Characters::general_body(f, "persona.ferryman");
  REQUIRE(general["items"].size() == 5);
  CHECK(general["items"][0]["label"] == "Who they appear to be");
  CHECK(general["items"][2]["value"] == "the river's moods; coins and what they buy; the weather over the water");
  CHECK(general["items"][3]["value"] == "Everyday, with old idiom");
  CHECK(general["items"][4]["value"] == "Speaks of news that touches them or you");
  // The weaver has no news temperament: absent, never a placeholder.
  CHECK(Characters::general_body(f, "persona.weaver")["items"].size() == 4);
  // Before the content loads, an empty list; a key no row has, no body.
  CHECK(Characters::list_body(nullptr)["rows"].empty());
  CHECK(Characters::general_body(f, "persona.nobody").empty());
  CHECK(Characters::document_body(nullptr, "persona.ferryman").empty());
}

TEST_CASE("characters: the editor's document: the four fields, null where left to the shipped profile, its state") {
  const CharacterFacts* f = test::characters_fixture("title");
  const json none = Characters::document_body(f, "persona.ferryman");
  CHECK(none["target"] == "persona.ferryman");
  CHECK(none["rev"] == 0);
  CHECK(none["state"]["word"] == "Not written yet");
  CHECK(none["draft_help"].get<std::string>().find("cloud-one") != std::string::npos);
  CHECK_FALSE(none.contains("draft_blocked"));
  REQUIRE(none["fields"].size() == 4);
  for (const json& field : none["fields"]) {
    CHECK(field["value"].is_null());
    CHECK(field["nullable"] == true);
  }
  CHECK(doc_of(none, "subjects")["reference"] ==
        json{"the river's moods", "coins and what they buy", "the weather over the water"});
  CHECK(doc_of(none, "register")["reference"] == "Everyday, with old idiom");
  CHECK(doc_of(none, "register")["choices"].size() == 5);
  CHECK(doc_of(none, "reply_words_max")["reference"] == "30");
  CHECK(doc_of(none, "instructions")["max_words"] == 35);
  const json mine = Characters::document_body(f, "persona.weaver");
  CHECK(mine["rev"] == 3);
  CHECK(mine["state"]["word"] == "Saved, with warnings");
  CHECK(mine["state"]["tone"] == "ember");
  CHECK(doc_of(mine, "instructions")["value"].is_string());
  CHECK(doc_of(mine, "subjects")["value"].size() == 2);
  CHECK(doc_of(mine, "register")["value"].is_null());
  const json world = Characters::document_body(test::characters_fixture("world"), "persona.weaver");
  CHECK(world["state"]["word"] == "Updated since you wrote this");
  CHECK(world["draft_blocked"] == "Drafts wait until EldenNet may use the network.");
  CHECK_FALSE(world.contains("draft_help"));
  // A newer file is shown read-only, every field locked; a refused one says so.
  CharacterFacts odd = *f;
  odd.rows[0].newer = true;
  odd.rows[1].refused = true;
  const json newer = Characters::document_body(&odd, "persona.ferryman");
  CHECK(newer["state"]["word"] == "Newer file");
  for (const json& field : newer["fields"]) CHECK(field.contains("locked"));
  CHECK(Characters::document_body(&odd, "persona.weaver")["state"]["word"] == "Not loaded");
}

TEST_CASE("characters: the spoiler golden: nothing but the projected fields reaches a view (PAGES §3, SP1)") {
  // A row whose layer's fields hold a key the editor never shows, as a refused file's might: it reaches no body.
  CharacterFacts f = *test::characters_fixture("title");
  f.rows[1].fields["private_secret"] = "the hidden canary text";
  f.rows[1].fields["stages"] = json::array({"another canary"});
  std::vector<std::string> seen;
  strings_of(Characters::list_body(&f), seen);
  for (const ProfileRow& r : f.rows) {
    strings_of(Characters::general_body(&f, r.key), seen);
    strings_of(Characters::document_body(&f, r.key), seen);
  }
  CHECK(std::none_of(seen.begin(), seen.end(), [](const std::string& s) { return s.find("canary") != std::string::npos; }));
  // Each document holds exactly the four fields a layer may set; each general profile its five labels at most.
  const json doc = Characters::document_body(&f, "persona.weaver");
  std::vector<std::string> ids;
  for (const json& field : doc["fields"]) ids.push_back(field["id"]);
  CHECK(ids == std::vector<std::string>{"instructions", "subjects", "register", "reply_words_max"});
}

TEST_CASE("characters: the commands, checked at the one entry point, then handed to the DLL") {
  Space s;
  const SpaceFacts f = facts_with(test::characters_fixture("title"));
  const json good_doc = {{"instructions", "Hums."}, {"subjects", json::array({"the river", json{{"text", "boats"}, {"key", "k1"}}})},
                         {"register", nullptr}, {"reply_words_max", 20}};
  SUBCASE("refused at once, each by its code") {
    const auto error_of = [&](std::string_view name, const json& args) {
      const std::string r = s.command(cmd(name, args), f, 77);
      return r.empty() ? std::string("deferred") : json::parse(r).value("error", std::string("ok"));
    };
    CHECK(error_of("profile.save", {{"target", "persona.nobody"}, {"rev", 0}, {"doc", good_doc}}) == "invalid_value");
    CHECK(error_of("profile.save", {{"target", "persona.ferryman"}, {"rev", -1}, {"doc", good_doc}}) == "invalid_value");
    CHECK(error_of("profile.save", {{"target", "persona.ferryman"}, {"rev", 0}, {"doc", {{"identity", "x"}}}}) ==
          "invalid_value");
    CHECK(error_of("profile.check", {{"target", "persona.ferryman"}, {"rev", 0}, {"doc", {{"instructions", 4}}}}) ==
          "invalid_value");
    CHECK(error_of("profile.check", {{"target", "persona.ferryman"}, {"rev", 0}, {"doc", {{"subjects", {1, 2}}}}}) ==
          "invalid_value");
    CHECK(error_of("profile.reset", {{"target", "persona.weaver"}, {"all", true}}) == "confirm_required");
    CHECK(error_of("profile.reset", {{"target", "persona.ferryman"}, {"all", true}, {"confirm", true}}) == "not_now");
    CHECK(error_of("profile.draft", {{"target", "persona.ferryman"}, {"notes", std::string(401, 'n')}}) ==
          "invalid_value");
    CHECK(error_of("profile.draft", {{"target", "persona.ferryman"}, {"notes", "   "}}) == "invalid_value");
    CHECK(error_of("profile.draft", {{"target", "persona.ferryman"}}) == "invalid_value");
    CHECK(s.take_profile_requests().empty());
    // Without a reply handle (the two-argument form) nothing can be answered later: refused as internal.
    CHECK(json::parse(s.command(cmd("profile.restore", {{"target", "persona.ferryman"}}), f))["error"] == "internal");
    // A draft while the helper may not run: refused in the page's own words.
    const SpaceFacts blocked = facts_with(test::characters_fixture("world"));
    const json r = json::parse(s.command(cmd("profile.draft", {{"target", "persona.ferryman"}, {"notes", "a ferryman"}}),
                                         blocked, 5));
    CHECK(r["error"] == "not_now");
    CHECK(r["detail"] == "Drafts wait until EldenNet may use the network.");
  }
  SUBCASE("taken: no answer now, a request for the DLL with its reply handle") {
    CHECK(s.command(cmd("profile.save", {{"target", "persona.ferryman"}, {"rev", 0}, {"doc", good_doc}}, 11), f, 501)
              .empty());
    CHECK(s.command(cmd("profile.draft", {{"target", "persona.weaver"}, {"notes", "nets and knots"}}, 12), f, 502).empty());
    CHECK(s.command(cmd("profile.reset", {{"target", "persona.weaver"}, {"all", true}, {"confirm", true}}, 13), f, 503)
              .empty());
    const std::vector<ProfileRequest> rs = s.take_profile_requests();
    REQUIRE(rs.size() == 3);
    CHECK(rs[0].kind == ProfileRequest::Kind::Save);
    CHECK(rs[0].reply == 501);
    CHECK(rs[0].id == 11);
    CHECK(rs[0].key == "persona.ferryman");
    CHECK(rs[0].doc["subjects"] == json{"the river", "boats"});  // an item's text; its key is the view's
    CHECK(rs[0].doc["register"].is_null());
    CHECK(rs[1].kind == ProfileRequest::Kind::Draft);
    CHECK(rs[1].notes == "nets and knots");
    CHECK(rs[2].kind == ProfileRequest::Kind::Reset);
    const std::vector<SpaceNote> notes = s.take_notes();
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].topic == "profile.save");
    CHECK(notes[0].key == "persona.ferryman");
    CHECK(notes[0].result == "taken");
  }
}

TEST_CASE("characters: answers come later with their reply handles: messages, a conflict, the chosen row, a draft") {
  Space s;
  const SpaceFacts f = facts_with(test::characters_fixture("title"));
  REQUIRE(s.command(cmd("profile.save", {{"target", "persona.ferryman"}, {"rev", 0}, {"doc", json::object()}}, 21), f, 601)
              .empty());
  REQUIRE(s.command(cmd("profile.check", {{"target", "persona.ferryman"}, {"rev", 0}, {"doc", json::object()}}, 22), f, 602)
              .empty());
  REQUIRE(s.command(cmd("profile.draft", {{"target", "persona.ferryman"}, {"notes", "a ferryman"}}, 23), f, 603).empty());
  const std::vector<ProfileRequest> rs = s.take_profile_requests();
  REQUIRE(rs.size() == 3);
  ProfileAnswer saved;
  saved.chosen = "persona.ferryman";
  s.answer_profile(rs[0], saved);
  ProfileAnswer checked;
  checked.messages = {ProfileMessage{"subjects", 1, false, "Write a subject as a short phrase."},
                      ProfileMessage{"", -1, true, "This file could not be read."}};
  s.answer_profile(rs[1], checked);
  ProfileAnswer drafted;
  drafted.draft = {{"instructions", "Poles slowly."}};
  drafted.notice = "A sentence was left out.";
  s.answer_profile(rs[2], drafted);
  const auto answers = s.take_answers();
  REQUIRE(answers.size() == 3);
  CHECK(answers[0].first == 601);
  const json a0 = json::parse(answers[0].second);
  CHECK(a0 == json{{"v", 1}, {"t", "result"}, {"space", "eldennet"}, {"id", 21}, {"ok", true},
                   {"body", {{"chosen", "persona.ferryman"}}}});
  const json a1 = json::parse(answers[1].second);
  CHECK(a1["body"]["messages"][0] == json{{"field", "subjects"}, {"item", 1}, {"level", "warning"},
                                          {"words", "Write a subject as a short phrase."}});
  CHECK_FALSE(a1["body"]["messages"][1].contains("field"));  // about the whole file
  CHECK(a1["body"]["messages"][1]["level"] == "error");
  const json a2 = json::parse(answers[2].second);
  CHECK(a2["body"]["draft"]["instructions"] == "Poles slowly.");
  CHECK(a2["body"]["notice"] == "A sentence was left out.");
  CHECK(s.take_answers().empty());
  // A conflict and a refused save: the code, the words, the messages; never a chosen row on an error.
  REQUIRE(s.command(cmd("profile.save", {{"target", "persona.weaver"}, {"rev", 2}, {"doc", json::object()}}, 24), f, 604)
              .empty());
  const ProfileRequest late = s.take_profile_requests().at(0);
  ProfileAnswer conflict;
  conflict.error = "conflict";
  conflict.detail = "Changed elsewhere since you opened it.";
  conflict.chosen = "persona.weaver";
  s.answer_profile(late, conflict);
  const json c = json::parse(s.take_answers().at(0).second);
  CHECK(c["error"] == "conflict");
  CHECK(c["detail"] == "Changed elsewhere since you opened it.");
  CHECK_FALSE(c.contains("body"));
}

TEST_CASE("characters: the list and each member watched are published on change, nothing unwatched") {
  Space s;
  const CharacterFacts* title = test::characters_fixture("title");
  SpaceFacts f = facts_with(title);
  std::vector<std::string> frames;
  s.collect(f, 0, frames);
  CHECK(frames.empty());
  s.watched(sub(kNamedList));
  s.watched(sub("profiles/persona.weaver"));
  s.watched(sub("named/persona.nobody"));  // no such row: no body, nothing published
  s.collect(f, 10, frames);
  REQUIRE(frames.size() == 2);
  std::vector<std::string> topics;
  for (const std::string& t : frames) topics.push_back(json::parse(t)["topic"]);
  std::sort(topics.begin(), topics.end());
  CHECK(topics == std::vector<std::string>{"named.list", "profiles/persona.weaver"});
  frames.clear();
  s.collect(f, 20, frames);
  CHECK(frames.empty());  // nothing changed
  // A save moves the weaver's layer on: only its member is published again.
  CharacterFacts next = *title;
  next.revision = 9;
  next.rows[1].rev = 4;
  f.characters = &next;
  s.collect(f, 30, frames);
  REQUIRE(frames.size() == 1);
  CHECK(json::parse(frames[0])["body"]["rev"] == 4);
  frames.clear();
  s.unwatched(unsub("profiles/persona.weaver"));
  next.rows[1].rev = 5;
  s.collect(f, 40, frames);
  CHECK(frames.empty());
  // Before the content loads, the list is published empty.
  Space before;
  before.watched(sub(kNamedList));
  frames.clear();
  before.collect(SpaceFacts{}, 0, frames);
  REQUIRE(frames.size() == 1);
  CHECK(json::parse(frames[0])["body"]["rows"].empty());
}
