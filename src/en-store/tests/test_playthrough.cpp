// The store's requests on one playthrough file (playthrough.hpp; MEM §8.6, §8.7): the event log with its frozen
// audiences and pages, the counters that never go back, conversations, jobs, journal and lived versions with their
// watermark, facts, and the queries that read each persona's own rows. Ids start past 2^33 and go past 2^53.
#include <doctest.h>

#include <string>
#include <vector>

#include "support.hpp"

using namespace en::store;
using namespace en::store::test;
namespace proto = en::proto;
using proto::How;

namespace {

constexpr std::int64_t kBig = (std::int64_t{1} << 53) + 11;

proto::db::Append said(std::int64_t id, std::string kind, proto::Pt t, std::int64_t conv, std::string text,
                       std::vector<proto::AudienceRow> aud) {
  proto::db::Append a;
  a.event = event(id, std::move(kind), t, std::move(aud));
  a.event.conv_id = conv;
  a.event.text = std::move(text);
  a.event.subject = a.event.kind == "PlayerSaid" ? "player" : "persona:kale";
  return a;
}

proto::ConversationRow conversation(std::int64_t id, std::string persona, proto::Pt started) {
  proto::ConversationRow c;
  c.id = id;
  c.persona = std::move(persona);
  c.stage = "at_the_church";
  c.started_pt = started;
  c.persona_hash = "ph";
  c.kb_hash = "kh";
  return c;
}

}  // namespace

TEST_CASE("playthrough: an event is stored with its frozen audience, and each persona reads only its own row") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  REQUIRE(pt.db.is_open());
  proto::db::Append a;
  a.event = event(kFirstEvent, "PersonaAttacked", 5000, {{"kale", How::Direct}, {"lantern_keeper", How::Witnessed}});
  a.event.subject = "persona:kale";
  a.event.object = "player";
  a.event.conf = 0.95;
  a.event.place_id = 62001;
  a.facts = {proto::PersonaFacts{"kale", {proto::FactRow{"times_attacked", "1", kFirstEvent, 5000}}}};
  REQUIRE(append(pt, a));
  CHECK(pt.next_event_id == kFirstEvent + 1);
  CHECK(meta_get(pt.db, "next_event_id").value_or(std::nullopt) == number_text(kFirstEvent + 1));
  auto kale = events::seen_by(pt.db, 1, "kale", 0, std::nullopt);
  REQUIRE(kale);
  REQUIRE(kale->size() == 1);
  const proto::StoredEvent& got = (*kale)[0];
  CHECK(got.id == kFirstEvent);
  CHECK(got.kind == "PersonaAttacked");
  CHECK(got.conf == doctest::Approx(0.95));
  CHECK(got.place_id == 62001);
  CHECK(got.epoch == pt.epoch);
  CHECK(got.audience == std::vector<proto::AudienceRow>{{"kale", How::Direct}});  // never the other's row
  auto keeper = events::seen_by(pt.db, 1, "lantern_keeper", 0, std::nullopt);
  REQUIRE(keeper);
  REQUIRE(keeper->size() == 1);
  CHECK((*keeper)[0].audience == std::vector<proto::AudienceRow>{{"lantern_keeper", How::Witnessed}});
  auto far = events::seen_by(pt.db, 1, "boc", 0, std::nullopt);  // not in the audience: nothing
  REQUIRE(far);
  CHECK(far->empty());
  auto other_journey = events::seen_by(pt.db, 2, "kale", 0, std::nullopt);
  REQUIRE(other_journey);
  CHECK(other_journey->empty());
  // A lower id never takes the counter back.
  REQUIRE(append(pt, proto::db::Append{event(kFirstEvent - 5, "PlaceEntered", 5100), {}}));
  CHECK(pt.next_event_id == kFirstEvent + 1);
}

TEST_CASE("playthrough: an NpcSaid's pages: its first at the append, each later one with its end and pt") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  REQUIRE(conv_open(pt, conversation(kBig, "kale", 1000)));
  REQUIRE(append(pt, said(kBig, "NpcSaid", 2000, kBig, "Hm.", {{"kale", How::Direct}})));
  REQUIRE(append_page(pt, proto::db::AppendPage{kBig, "It keeps.", 4500}));
  REQUIRE(append_page(pt, proto::db::AppendPage{kBig, "So do I.", 7000}));
  CHECK(test::value(pt.db, "SELECT text FROM events WHERE id = " + number_text(kBig)) == "Hm. It keeps. So do I.");
  CHECK(test::value(pt.db, "SELECT t_play_end_ms FROM events WHERE id = " + number_text(kBig)) == "7000");
  CHECK(test::value(pt.db, "SELECT json_extract(payload, '$.pages') FROM events WHERE id = " + number_text(kBig)) ==
        "[{\"end\":3,\"pt\":2000},{\"end\":13,\"pt\":4500},{\"end\":22,\"pt\":7000}]");
  // Only an NpcSaid takes a page.
  REQUIRE(append(pt, said(kBig + 1, "PlayerSaid", 8000, kBig, "And you?", {{"kale", How::Direct}})));
  const auto bad = append_page(pt, proto::db::AppendPage{kBig + 1, "more", 9000});
  REQUIRE_FALSE(bad);
  CHECK(bad.error() == Fault::Invalid);
}

TEST_CASE("playthrough: the schema's caps refuse an event, and nothing of it stays") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  auto long_text = said(kFirstEvent, "PlayerSaid", 1000, 1, std::string(601, 'a'), {});
  long_text.event.conv_id.reset();
  const auto t = append(pt, long_text);
  REQUIRE_FALSE(t);
  CHECK(t.error() == Fault::Constraint);
  proto::db::Append big;
  big.event = event(kFirstEvent + 1, "PlaceEntered", 1000);
  big.event.payload = "{\"x\":\"" + std::string(2100, 'b') + "\"}";
  const auto p = append(pt, big);
  REQUIRE_FALSE(p);
  CHECK(p.error() == Fault::Constraint);
  big.event.payload = "{not json";
  REQUIRE_FALSE(append(pt, big));
  proto::db::Append no_kind;
  no_kind.event = event(kFirstEvent + 2, "", 1000);
  const auto k = append(pt, no_kind);
  REQUIRE_FALSE(k);
  CHECK(k.error() == Fault::Invalid);
}

TEST_CASE("playthrough: a JourneyChanged into a later journey starts its epoch, and its event belongs to it") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  const std::int64_t first_epoch = pt.epoch;
  REQUIRE(append(pt, proto::db::Append{event(kFirstEvent, "PlaceEntered", 1000), {}}));
  proto::db::Append ng;
  ng.event = event(kFirstEvent + 1, "JourneyChanged", 9000000, {}, 2);
  ng.event.payload = "{\"ng_lvl\":1}";
  REQUIRE(append(pt, ng));
  CHECK(pt.journey == 2);
  CHECK(pt.epoch > first_epoch);
  CHECK(test::value(pt.db, "SELECT epoch FROM events WHERE id = " + number_text(kFirstEvent + 1)) ==
        number_text(pt.epoch));
  CHECK(test::value(pt.db, "SELECT reason || ':' || journey || ':' || start_pt FROM epochs WHERE epoch = " +
                               number_text(pt.epoch)) == "journey:2:9000000");
  CHECK(meta_get(pt.db, "current_journey").value_or(std::nullopt) == "2");
}

TEST_CASE("playthrough: conversations open and close, their ids past 2^53 and the counter kept") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  REQUIRE(conv_open(pt, conversation(kBig, "kale", 1000)));
  CHECK(pt.next_conv_id == kBig + 1);
  proto::ConversationRow end = conversation(kBig, "kale", 1000);
  end.ended_pt = 9000;
  end.end_reason = "closed";
  end.player_lines = 2;
  end.committed_lines = 5;
  end.shown_context = "[{\"c\":1}]";
  REQUIRE(conv_close(pt, end));
  CHECK(test::value(pt.db, "SELECT ended_pt || ':' || end_reason || ':' || committed_lines FROM conversations") ==
        "9000:closed:5");
  CHECK_FALSE(conv_close(pt, conversation(kBig + 7, "kale", 1)));  // no such conversation
  CHECK_FALSE(conv_open(pt, conversation(kBig, "kale", 1000)));    // its id is taken
}

TEST_CASE("playthrough: a journal version commits with its watermark, the job done and its facts, in one go") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  proto::MemoryJob job;
  job.persona = "kale";
  job.cause = proto::JobCause::Conversation;
  job.conv_id = 1;
  job.upto_event_id = kFirstEvent + 3;
  job.created_pt = 10000;
  auto id = upsert_job(pt, job);
  REQUIRE(id);
  CHECK(*id > 0);
  job.id = *id;
  job.attempts = 1;
  job.state = proto::JobState::Running;
  REQUIRE(upsert_job(pt, job));
  auto open = open_jobs(pt.db);
  REQUIRE(open);
  REQUIRE(open->size() == 1);
  CHECK((*open)[0].state == proto::JobState::Running);
  CHECK((*open)[0].cause == proto::JobCause::Conversation);
  CHECK((*open)[0].upto_event_id == kFirstEvent + 3);
  proto::db::CommitJournal c;
  c.version.persona = "kale";
  c.version.version = 1;
  c.version.as_of_pt = 9000;
  c.version.from_event_id = kFirstEvent;
  c.version.upto_event_id = kFirstEvent + 3;
  c.version.summary = "I met a newcomer.";
  c.version.keepsakes = "[{\"id\":\"k1\",\"basis\":\"said\"}]";
  c.version.told_name = "Wren";
  c.version.told_name_verified = true;
  c.version.disposition = 1;
  c.version.model_fp = "m";
  c.version.prompt_hash = "p";
  c.version.persona_hash = "h";
  c.job = *id;
  c.facts = {proto::FactRow{"verified:great_runes>=1", "1", std::nullopt, 9000}};
  REQUIRE(commit_journal(pt, c));
  auto none = open_jobs(pt.db);
  REQUIRE(none);
  CHECK(none->empty());
  auto b = bundle(pt, proto::db::Bundle{1, "kale"});
  REQUIRE(b);
  REQUIRE(b->journal);
  CHECK(b->journal->summary == "I met a newcomer.");
  CHECK(b->journal->told_name == "Wren");
  CHECK(b->journal->told_name_verified);
  CHECK(b->journal->upto_event_id == kFirstEvent + 3);
  REQUIRE(b->facts.size() == 1);
  CHECK(b->facts[0].key == "verified:great_runes>=1");
  CHECK(test::value(pt.db, "SELECT watermark FROM memory_progress WHERE persona = 'kale'") ==
        number_text(kFirstEvent + 3));
  // Version 1 again is refused, and version 2 is the current one once written.
  const auto dup = commit_journal(pt, c);
  REQUIRE_FALSE(dup);
  CHECK(dup.error() == Fault::Constraint);
  c.version.version = 2;
  c.version.summary = "Later.";
  c.job.reset();
  REQUIRE(commit_journal(pt, c));
  CHECK(bundle(pt, proto::db::Bundle{1, "kale"})->journal->version == 2);
}

TEST_CASE("playthrough: the bundle's tail is the last six direct lines of the last ended conversation") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  REQUIRE(conv_open(pt, conversation(1, "kale", 1000)));
  std::int64_t id = kFirstEvent;
  for (int i = 0; i < 8; ++i) {
    const bool player = i % 2 == 0;
    REQUIRE(append(pt, said(id++, player ? "PlayerSaid" : "NpcSaid", 1000 + 100 * i, 1, "line " + std::to_string(i),
                            {{"kale", How::Direct}, {"lantern_keeper", How::Overheard}})));
  }
  // An open conversation is no tail yet.
  CHECK_FALSE(bundle(pt, proto::db::Bundle{1, "kale"})->prev_conv);
  proto::ConversationRow end = conversation(1, "kale", 1000);
  end.ended_pt = 2000;
  REQUIRE(conv_close(pt, end));
  auto b = bundle(pt, proto::db::Bundle{1, "kale"});
  REQUIRE(b);
  REQUIRE(b->prev_conv == 1);
  REQUIRE(b->prev_tail.size() == kTailLines);
  CHECK(b->prev_tail.front().text == "line 2");
  CHECK(b->prev_tail.back().text == "line 7");
  CHECK(b->pending_count == 8);  // no journal yet: every line is above the watermark (0)
  // The Lantern-Keeper only overheard: it has no tail of Kalé's conversation, but its lines are pending for it.
  auto k = bundle(pt, proto::db::Bundle{1, "lantern_keeper"});
  REQUIRE(k);
  CHECK_FALSE(k->prev_conv);
  CHECK(k->pending_count == 8);
}

TEST_CASE("playthrough: a job's input is the persona's own evidence above the watermark, with its conversations") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  REQUIRE(conv_open(pt, conversation(3, "kale", 1000)));
  std::int64_t id = kFirstEvent;
  REQUIRE(append(pt, said(id++, "PlayerSaid", 1100, 3, "Evening.", {{"kale", How::Direct}})));
  REQUIRE(append(pt, said(id++, "NpcSaid", 1200, 3, "Evening to you.", {{"kale", How::Direct}})));
  REQUIRE(
      append(pt, proto::db::Append{event(id++, "PersonaAttacked", 1300, {{"lantern_keeper", How::Witnessed}}), {}}));
  REQUIRE(append(pt, said(id++, "PlayerSaid", 1400, 3, "Sorry.", {{"kale", How::Direct}})));
  proto::MemoryJob job;
  job.persona = "kale";
  job.upto_event_id = kFirstEvent + 2;  // the first three events: the attack is not Kalé's
  job.created_pt = 1500;
  auto jid = upsert_job(pt, job);
  REQUIRE(jid);
  auto in = job_input(pt, proto::db::JobInput{*jid});
  REQUIRE(in);
  CHECK(in->watermark == 0);
  REQUIRE(in->evidence.size() == 2);
  CHECK(in->evidence[0].text == "Evening.");
  CHECK(in->evidence[1].kind == "NpcSaid");
  REQUIRE(in->conversations.size() == 1);
  CHECK(in->conversations[0].id == 3);
  CHECK_FALSE(job_input(pt, proto::db::JobInput{*jid + 99}));
}

TEST_CASE("playthrough: lived versions, the facts replaced whole by a refold, and every row for the fold") {
  TempDir dir;
  Playthrough pt = make_playthrough(dir.path() / "pt.sqlite");
  proto::db::CommitLived l;
  l.version.persona = "kale";
  l.version.version = 1;
  l.version.journal_version = 3;
  l.version.as_of_pt = 12000;
  l.version.towards = "[{\"id\":\"t1\",\"who\":\"player\"}]";
  l.version.model_fp = "m";
  l.version.prompt_hash = "p";
  l.version.persona_hash = "h";
  REQUIRE(commit_lived(pt, l));
  auto li = lived_input(pt, proto::db::LivedInput{1, "kale"});
  REQUIRE(li);
  REQUIRE(li->lived);
  CHECK(li->lived->journal_version == 3);
  CHECK_FALSE(li->journal);
  REQUIRE(meta_set(pt.db, "facts_stale", "1"));
  // A fact an event gives, and a claim's proof, which none does (its src_event NULL).
  const proto::PersonaFacts before{"kale", {proto::FactRow{"seen_count", "1", kFirstEvent, 100},
                                            proto::FactRow{"verified:slew:godrick", "1", {}, 150}}};
  REQUIRE(append(pt, proto::db::Append{event(kFirstEvent, "PersonaSeen", 100, {{"kale", How::Direct}}), {before}}));
  REQUIRE(append(
      pt, proto::db::Append{
              event(kFirstEvent + 1, "PersonaAttacked", 200, {{"kale", How::Direct}, {"boc", How::Witnessed}}), {}}));
  proto::db::PutFacts refold;
  refold.personas = {proto::PersonaFacts{"kale", {proto::FactRow{"times_attacked", "1", kFirstEvent + 1, 200}}}};
  REQUIRE(put_facts(pt, refold));
  // seen_count is gone with the fold; the claim's proof, which no event gives, stays.
  CHECK(test::value(pt.db, "SELECT group_concat(key) FROM npc_facts ORDER BY key") ==
        "times_attacked,verified:slew:godrick");
  CHECK(meta_get(pt.db, "facts_stale").value_or(std::nullopt) == "0");
  auto all = fold_events(pt, proto::db::EventsForFold{1});
  REQUIRE(all);
  REQUIRE(all->events.size() == 2);
  CHECK(all->events[1].audience ==
        std::vector<proto::AudienceRow>{{"boc", How::Witnessed}, {"kale", How::Direct}});  // every row, by persona
}
