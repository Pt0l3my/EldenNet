// Tools › Workshop's keeping (workshop.hpp): the newest exchange as sent and answered, scrubbed and bounded; events in
// the golden form with the session's handle names; unknown characters with their TOML, which en-core's index takes; the
// world view, the known sets and the kinds built from the shipped content; and the name's mask. Every key, name and
// text here is invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/augur/events.hpp"
#include "en/core/content.hpp"
#include "en/core/core.hpp"
#include "en/shell/workshop.hpp"

using namespace en::shell;
namespace proto = en::proto;
namespace core = en::core;
namespace augur = en::augur;

namespace {

constexpr std::string_view kKey = "lantern-key-of-the-tests";  // invented, and shaped like no real key
constexpr std::string_view kName = "Ondrel";

// The DLL's scrub, as the tests make it: the key's value, then the name, matched whole.
std::string scrub(std::string_view text) {
  std::string out(text);
  for (std::size_t at = out.find(kKey); at != std::string::npos; at = out.find(kKey, at)) {
    out.replace(at, kKey.size(), "redacted");
  }
  return mask_name(out, kName);
}

proto::LlmRequest chat_request(proto::RequestId id, std::vector<proto::Message> messages) {
  proto::ChatRequest c;
  c.slot = "dialogue";
  c.messages = std::move(messages);
  c.max_tokens = 140;
  c.stop = {"\n\n", "Tarnished:"};
  return proto::LlmRequest::chat(id, std::move(c));
}

proto::LlmEvent ev(proto::EventKind kind, proto::RequestId id) {
  proto::LlmEvent e;
  e.kind = kind;
  e.id = id;
  return e;
}

void load(core::Core& c) { REQUIRE(c.load_content(core::builtin_content(), core::Realm::Game).empty()); }

}  // namespace

TEST_CASE("workshop: the name's mask takes the whole name alone, in either case, never inside a word") {
  CHECK(mask_name("Ondrel came; ondrel left.", "Ondrel") ==
        "[the Tarnished's name] came; [the Tarnished's name] left.");
  CHECK(mask_name("Ondrelith and Bondrel", "Ondrel") == "Ondrelith and Bondrel");
  CHECK(mask_name("Ondrel", "Ondrel") == "[the Tarnished's name]");
  CHECK(mask_name("said \"Ondrel!\"", "Ondrel") == "said \"[the Tarnished's name]!\"");
  CHECK(mask_name("Ondrelé", "Ondrel") == "Ondrelé");  // a byte of a multi-byte character is a word's
  CHECK(mask_name("nothing here", "") == "nothing here");
}

TEST_CASE("workshop: the newest exchange, scrubbed and bounded; another request's events are not its own") {
  Workshop w;
  w.set_scrub(scrub);
  CHECK_FALSE(w.view().exchange.any);
  w.sent(chat_request(7, {proto::Message::system("You keep the lamps of the invented harbour."),
                          proto::Message::user("I am Ondrel. My key is " + std::string(kKey) + ".")}),
         proto::CallPurpose::Reply, 1000);
  const proto::WorkshopExchange& x = w.view().exchange;
  REQUIRE(x.any);
  CHECK(x.id == 7);
  CHECK(x.slot == "dialogue");
  CHECK(x.purpose == "reply");
  REQUIRE(x.messages.size() == 2);
  CHECK(x.messages[0].role == "system");
  CHECK(x.messages[1].text == "I am [the Tarnished's name]. My key is redacted.");
  CHECK(x.max_tokens == 140);
  CHECK(x.temperature == -1);
  CHECK(x.stops == 2);
  CHECK_FALSE(x.started);

  proto::LlmEvent started = ev(proto::EventKind::Started, 7);
  started.provider = "lantern-box";
  started.model = "invented-lantern-a";
  w.event(started, 1100);
  CHECK(x.started);
  CHECK(x.provider == "lantern-box");
  // A key split between two pieces of the stream is still scrubbed: the whole of what came is.
  proto::LlmEvent d1 = ev(proto::EventKind::Delta, 7);
  d1.text = "Your key, " + std::string(kKey.substr(0, 10));
  proto::LlmEvent d2 = ev(proto::EventKind::Delta, 7);
  d2.text = std::string(kKey.substr(10)) + ", stays with you.";
  w.event(d1, 1200);
  w.event(d2, 1250);
  CHECK(x.text == "Your key, redacted, stays with you.");
  proto::LlmEvent other = ev(proto::EventKind::Delta, 8);
  other.text = "not this exchange's";
  w.event(other, 1260);
  CHECK(x.text.find("not this") == std::string::npos);

  proto::LlmEvent end = ev(proto::EventKind::Ended, 7);
  proto::ChatResult r;
  r.text = "Welcome, Ondrel.";
  r.raw_text = "<think>The lamp keeper greets Ondrel.</think>Welcome, Ondrel.";
  r.finish_reason = "stop";
  r.provider = "lantern-box";
  r.model = "C:\\models\\invented.gguf";  // shaped like a path: left out (PV5)
  r.served_model = "invented-lantern-a";
  r.usage.prompt_tokens = 812;
  r.usage.completion_tokens = 9;
  r.first_token = std::chrono::milliseconds(140);
  r.total = std::chrono::milliseconds(610);
  r.attempts.push_back(proto::AttemptInfo{});
  r.message = "a server's words, never kept";
  end.chat = r;
  w.event(end, 1700);
  CHECK(x.ended);
  CHECK(x.text == "Welcome, [the Tarnished's name].");
  CHECK(x.raw == "<think>The lamp keeper greets [the Tarnished's name].</think>Welcome, [the Tarnished's name].");
  CHECK(x.model.empty());
  CHECK(x.served_model == "invented-lantern-a");
  CHECK(x.tokens_in == 812);
  CHECK(x.first_token_ms == 140);
  CHECK(x.total_ms == 610);
  CHECK(x.attempts == 1);
  // After the end nothing more is taken.
  w.event(d1, 1800);
  CHECK(x.text == "Welcome, [the Tarnished's name].");

  // The next request replaces it; a decision call is no chat and is not kept.
  w.sent(proto::LlmRequest::decision(9, proto::DecisionCall{}), proto::CallPurpose::Reply, 2000);
  CHECK(w.view().exchange.id == 7);
  w.sent(chat_request(10, {proto::Message::user("A draft's notes")}), proto::CallPurpose::ProfileDraft, 2100);
  CHECK(w.view().exchange.id == 10);
  CHECK(w.view().exchange.purpose == "profile draft");
  CHECK(w.view().exchange.text.empty());
}

TEST_CASE("workshop: a prompt past its bound is cut at a character's start, and the bytes left out are counted") {
  Workshop w;
  std::string big(proto::kWorkshopMessagesMax - 1, 'a');
  big += "é and more";  // the cut would fall inside é
  w.sent(chat_request(3, {proto::Message::system(big), proto::Message::user("left out whole")}),
         proto::CallPurpose::Reply, 0);
  const proto::WorkshopExchange& x = w.view().exchange;
  REQUIRE(x.messages.size() == 2);
  CHECK(x.messages[0].text.size() == proto::kWorkshopMessagesMax - 1);
  CHECK(x.messages[1].text.empty());
  CHECK(x.messages_cut == big.size() - (proto::kWorkshopMessagesMax - 1) + std::string("left out whole").size());
  std::size_t kept = 0;
  for (const auto& m : x.messages) kept += m.text.size();
  CHECK(kept <= proto::kWorkshopMessagesMax);
}

TEST_CASE("workshop: an event's row is its golden line, its handles the session's names, counted") {
  Workshop w;
  augur::Event e;
  e.kind = augur::Struck{augur::ActorRef::mute(proto::ChrHandle{0xFFFF00AA}), augur::ActorRef::player(), true, false,
                         1, 0.4f, 0};
  e.event_class = augur::Class::K;
  e.source = augur::SourceId::SnapshotDiff;
  e.t_mono_ms = 5000;
  const proto::WorkshopEvent row = w.event_row(e, 4000);
  CHECK(row.t_ms == 4000);
  CHECK(row.kind == "Struck");
  CHECK(row.cls == "K");
  CHECK(row.source == "snapshot_diff");
  CHECK(row.line.find("\"kind\":\"Struck\"") != std::string::npos);
  CHECK(row.line.find("mute#h1") != std::string::npos);
  CHECK(row.line.find("ffff00aa") == std::string::npos);
  CHECK(row.line.find("FFFF00AA") == std::string::npos);
  (void)w.event_row(e, 4100);
  CHECK(w.view().events.seen == 2);
  w.drops(3, 4);
  CHECK(w.view().events.dropped_t == 3);
  CHECK(w.view().events.dropped_k == 4);
  w.clear();
  CHECK(w.view().events.seen == 0);
}

TEST_CASE("workshop: unknown characters, by row and entity, the least recently seen giving way; their TOML loads") {
  core::Core c;
  load(c);
  Workshop w;
  proto::WorldSnapshot s;
  proto::Chr kale;  // the pilot's row (identity/personas.toml): bound, so not unknown
  kale.param = 32000000;
  proto::Chr wolf;
  wolf.param = 12345600;
  wolf.dist_m = 14;
  proto::Chr guard;
  guard.param = 7700;
  guard.entity = 1043300800;
  guard.dist_m = 30;
  REQUIRE(s.chars.try_push_back(kale));
  REQUIRE(s.chars.try_push_back(wolf));
  REQUIRE(s.chars.try_push_back(guard));
  w.see(s, c.directory().index(), 100);
  s.chars.clear();
  wolf.dist_m = 6;
  REQUIRE(s.chars.try_push_back(wolf));
  w.see(s, c.directory().index(), 200);
  const auto& rows = w.view().unknown;
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].param == 12345600);  // the most recently seen first
  CHECK(rows[0].seen == 2);
  CHECK(rows[0].nearest_m == 6);
  CHECK(rows[0].first_ms == 100);
  CHECK(rows[0].last_ms == 200);
  CHECK(rows[1].entity == 1043300800);
  CHECK(rows[1].toml.find("entity_ids = [{ id = 1043300800, verified = false }]") != std::string::npos);
  CHECK(rows[0].toml.find("entity_ids") == std::string::npos);

  // The TOML is a row en-core's index takes, with a record of its own (an invented one).
  const std::string index = "schema = 1\n" + rows[1].toml;
  const std::string record = "id = \"new_7700\"\nschema = 1\ndrafted = \"hand\"\nreview = \"ci\"\n"
                             "display_name_fallback = \"Invented\"\nregister = \"colloquial\"\npronouns = \"they\"\n"
                             "deflections = [\"Not now.\", \"Ask another.\", \"That stays with me.\"]\n"
                             "[[stages]]\nid = \"base\"\nwhen = \"true\"\nroad = \"You keep an invented pier.\"\n"
                             "at = \"limgrave\"\nstate = \"awake\"\n";
  const std::vector<core::ContentFile> files = {
      core::ContentFile{"identity/personas.toml", index, core::ContentOrigin::Shipped},
      core::ContentFile{"personas/new_7700.toml", record, core::ContentOrigin::Shipped}};
  const auto pack = core::load_content(files);
  std::string errors;
  if (!pack) {
    for (const auto& e : pack.error()) errors += core::describe(e) + "\n";
  }
  INFO(errors);
  REQUIRE(pack.has_value());
  REQUIRE(pack->rows.size() == 1);
  CHECK(pack->rows[0].id == "new_7700");

  // At most kWorkshopUnknownMax: the least recently seen go first.
  for (std::uint32_t i = 0; i < 70; ++i) {
    proto::WorldSnapshot more;
    proto::Chr ch;
    ch.param = 500000 + i;
    REQUIRE(more.chars.try_push_back(ch));
    w.see(more, c.directory().index(), 1000 + i);
  }
  CHECK(w.view().unknown.size() == proto::kWorkshopUnknownMax);
  CHECK(w.view().unknown.front().param == 500069);
  CHECK(std::none_of(w.view().unknown.begin(), w.view().unknown.end(),
                     [](const auto& u) { return u.param == 12345600; }));
}

TEST_CASE("workshop: the world view, the known sets and the kinds, from the shipped content") {
  core::Core c;
  load(c);
  const proto::WorkshopWorld world = Workshop::world_of(c);
  CHECK(world.flags.size() == c.knowledge().catalogue().flags.size());
  CHECK_FALSE(world.flags.empty());
  CHECK_FALSE(world.have_frame);
  // No frame yet: every alias reads Unknown, and none of the shipped leads is verified (MEM §6.4).
  CHECK(std::all_of(world.flags.begin(), world.flags.end(), [](const auto& f) { return f.reads == "U"; }));
  CHECK(std::none_of(world.flags.begin(), world.flags.end(), [](const auto& f) { return f.verified_here; }));
  const auto godrick = std::find_if(world.flags.begin(), world.flags.end(),
                                    [](const auto& f) { return f.alias == "boss.godrick"; });
  REQUIRE(godrick != world.flags.end());
  CHECK(godrick->id == 9101);
  CHECK(godrick->kind == "outcome");
  CHECK(world.stages.size() == c.knowledge().stages().size());
  CHECK(std::all_of(world.stages.begin(), world.stages.end(), [](const auto& s) { return s.value == "U"; }));
  CHECK(world.tracks.size() == c.knowledge().tracks().size());

  const proto::WorkshopKnownSets known = Workshop::known_of(c);
  CHECK(known.cut == 0);
  for (const auto& row : known.rows) {
    CHECK_FALSE(row.persona.empty());
    CHECK_FALSE(row.stage.empty());
  }

  const proto::WorkshopKinds kinds = Workshop::kinds_of(c);
  CHECK(kinds.kinds.size() == augur::kKindCount);
  const auto named = [&](std::string_view n) {
    return std::find_if(kinds.kinds.begin(), kinds.kinds.end(), [&](const auto& k) { return k.name == n; });
  };
  const auto struck = named("Struck");
  REQUIRE(struck != kinds.kinds.end());
  CHECK_FALSE(struck->built_by.empty());
  CHECK(struck->fields.find("frac_total (number)") != std::string::npos);
  CHECK(struck->placeholders.rfind("subject", 0) == 0);
  const auto said = named("NpcSaid");
  REQUIRE(said != kinds.kinds.end());
  CHECK(said->built_by.empty());
  CHECK_FALSE(said->waits_for.empty());
  std::size_t counted = 0;
  for (const auto& k : kinds.kinds) counted += k.rules;
  CHECK(counted == kinds.rules.size());
  CHECK_FALSE(kinds.rules.empty());
  for (const auto& r : kinds.rules) {
    CHECK_FALSE(r.id.empty());
    CHECK(r.where.find(':') != std::string::npos);
    CHECK(r.detector != "?");
  }
}
