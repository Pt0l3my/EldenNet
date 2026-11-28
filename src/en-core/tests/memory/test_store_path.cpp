// The store in the core (store_path.hpp; ARCH §9; MEM §1.5, §8.6, §9.1): the core on a simulated clock against
// en-store's StoreCore on a temporary folder, each step's requests answered in order as T9 would. The world waits for
// the open; ids come from the file; speech goes out in its step and other writes within 250 ms; a lock at open is
// retried and then leaves memory off; the rollback choice holds named chat and the writes until the player answers;
// memory off opens nothing. All data is invented.
#include <doctest.h>

#include <atomic>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "../snapshots.hpp"
#include "../test_files.hpp"
#include "en/core/core.hpp"
#include "en/core/facts.hpp"
#include "en/core/stubs.hpp"
#include "en/store/db.hpp"
#include "en/store/files.hpp"
#include "en/store/store.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;
namespace db = proto::db;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t kAccount = 76561190000000003ull;  // invented

class TempDir {
 public:
  TempDir() {
    static std::atomic<unsigned> n{0};
    path_ = fs::temp_directory_path() /
            ("en-core-store-" + std::to_string(std::random_device{}()) + "-" + std::to_string(n.fetch_add(1)));
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  [[nodiscard]] const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

proto::WorldSnapshot with_keeper(proto::WorldSnapshot s) {
  s.player->handle = proto::ChrHandle{1};
  s.player->hp = proto::Meter{400, 400};
  s.ui.hud = proto::HudState::Default;
  s.ui.subtitles_on = true;
  proto::Chr c;
  c.handle = proto::ChrHandle{0x500};
  c.param = 81100;
  c.dist_m = 2.0f;
  c.bearing_deg = 3.0f;
  c.hp = proto::Meter{100, 100};
  (void)s.chars.try_push_back(c);
  return s;
}

store::StoreCfg config(const fs::path& saves) {
  store::StoreCfg c;
  c.saves = saves;
  c.build = "test-build";
  c.wall = [] { return std::int64_t{1764201600}; };
  c.mono_ms = [] { return std::uint64_t{0}; };
  c.free_bytes = [](const fs::path&) -> std::optional<std::uint64_t> { return 10ull << 30; };
  c.busy_timeout_ms = 20;
  return c;
}

// A session: the core, the stubs' speech, and the store answering every request in order. `play` is the save's play
// time at the first snapshot.
struct Session {
  explicit Session(const fs::path& saves, proto::Pt play = 600000, bool attach = true)
      : store(config(saves)), play0(play) {
    REQUIRE(core.load_content(content_with_overrides(), Realm::Eval).empty());
    if (attach) (void)feed(CoreInput{CoreInput::Account{kAccount}});
  }
  stubs::StubSpeech speech;
  Core core{speech.parts()};
  store::StoreCore store;
  bool answering = true;  // the store answers at once; off, requests wait in `pending`
  std::vector<proto::DbRequest> pending;
  std::vector<proto::DbRequest> sent;  // every request, in order
  std::vector<std::vector<proto::DbRequest>> per_step;
  std::vector<proto::Toast> toasts;
  Mono t = 0;
  proto::Pt play0;
  Outputs last;

  Outputs feed(CoreInput in) {
    Outputs o = core.step(std::move(in), t);
    per_step.push_back(o.db);
    for (const proto::Toast& x : o.ui.toasts) {
      if (std::none_of(toasts.begin(), toasts.end(), [&](const proto::Toast& y) { return y.id == x.id; })) {
        toasts.push_back(x);
      }
    }
    for (proto::DbRequest& r : o.db) {
      sent.push_back(r);
      pending.push_back(r);
    }
    if (answering) answer();
    last = o;
    return o;
  }
  void answer() {
    while (!pending.empty()) {
      std::vector<proto::DbRequest> now = std::move(pending);
      pending.clear();
      for (const proto::DbRequest& r : now) (void)feed_result(store.handle(r));
    }
  }
  Outputs feed_result(proto::DbResult r) { return feed(CoreInput{std::move(r)}); }
  proto::WorldSnapshot snap() const {
    return with_keeper(test::world(t, static_cast<std::uint64_t>(play0 + static_cast<proto::Pt>(t))));
  }
  void world(int snapshots) {
    for (int i = 0; i < snapshots; ++i) {
      t += 250;
      const Outputs o = feed(in(snap()));
      if (o.ui.page && o.ui.page->kind == proto::PageKind::Speech) {
        t += 10;
        (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::PageShown{o.ui.page->id, t}}});
      }
    }
  }
  void boot() {
    (void)feed(in(data_ready()));
    (void)feed(CoreInput{proto::NetHealth{true}});
    (void)feed(in(playthrough_ready()));
  }
  // The talk key, a line, and its reply shown.
  void talk(const std::string& line, const std::string& reply) {
    (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
    const Outputs o = feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{line}}});
    REQUIRE(o.requests.size() == 1);
    proto::LlmEvent d;
    d.kind = proto::EventKind::Delta;
    d.id = o.requests[0].id;
    d.text = reply;
    (void)feed(CoreInput{d});
    proto::LlmEvent e;
    e.kind = proto::EventKind::Ended;
    e.id = d.id;
    e.chat = proto::ChatResult{};
    (void)feed(CoreInput{e});
    world(20);
  }
  void leave() {
    t += 250;
    (void)feed(in(test::title(t)));
  }
  [[nodiscard]] bool toasted(std::string_view text) const {
    return std::any_of(toasts.begin(), toasts.end(), [&](const proto::Toast& x) { return x.text == text; });
  }
};

fs::path live_of(const fs::path& saves, const proto::PlaythroughKey& k) {
  return store::save_folder(saves, k.steamid64).live(k.slot, k.fp8);
}

proto::PlaythroughKey the_key() { return playthrough_key(kAccount, 3, "Vessel", 5, 2); }

std::string one(const store::Db& d, const std::string& sql) {
  auto s = store::Stmt::prepare(d, sql);
  if (!s) return "<error>";
  auto r = s->step();
  if (!r || !*r || s->null(0)) return "";
  return s->text(0);
}

}  // namespace

TEST_CASE("store path: the key is MEM §8.3's: the name hashed, never kept; a gift unread is 0xFF") {
  const proto::PlaythroughKey k = the_key();
  CHECK(k.fp8.size() == 8);
  CHECK(k.name_hash.size() == 64);
  CHECK(k.name_hash.find("Vessel") == std::string::npos);
  CHECK(playthrough_key(kAccount, 3, "Vessel", 5, 2) == k);  // stable
  CHECK(playthrough_key(kAccount, 4, "Vessel", 5, 2).fp8 != k.fp8);
  CHECK(playthrough_key(kAccount, 3, "Vessel", 5, 0xFF).fp8 != k.fp8);
  CHECK(playthrough_key(kAccount, 3, "Vessel", 5, 0xFF).name_hash == k.name_hash);
  CHECK(playthrough_key(kAccount + 1, 3, "Vessel", 5, 2).fp8 != k.fp8);
}

TEST_CASE("store path: the world waits for Opened; speech goes out in its step; the file holds the conversation") {
  TempDir dir;
  Session s(dir.path() / "saves");
  s.answering = false;
  s.boot();
  s.world(4);
  CHECK(s.core.state().phase == Phase::Playthrough);  // Open sent, no answer yet
  REQUIRE(!s.sent.empty());
  CHECK(std::holds_alternative<db::Open>(s.sent.front().op));
  CHECK(std::get<db::Open>(s.sent.front().op).key == the_key());
  s.answering = true;
  s.answer();
  CHECK(s.core.state().phase == Phase::InWorld);
  CHECK(s.core.store().mode() == StorePath::Mode::Open);
  s.world(30);
  REQUIRE(s.core.state().gate_open);
  s.talk("Is the lamp yours?", "Hm. It keeps me as I keep it.");
  // The Tarnished's line went out in the step that committed it.
  bool same_step = false;
  for (const auto& step : s.per_step) {
    for (const proto::DbRequest& r : step) {
      if (const auto* a = std::get_if<db::Append>(&r.op); a != nullptr && a->event.kind == "PlayerSaid") {
        same_step = true;
        CHECK(a->event.text == "Is the lamp yours?");
        CHECK(a->event.conv_id.has_value());
      }
    }
  }
  CHECK(same_step);
  s.t += 250;
  (void)s.feed(in(loading(s.t)));  // travel ends the conversation
  s.world(4);
  // The meeting is folded live, in RAM and into the facts written with it (below).
  REQUIRE(s.core.store().facts().count("lantern_keeper") == 1);
  CHECK(fact_number(s.core.store().facts().at("lantern_keeper"), fact::kMetCount) == std::optional<std::int64_t>(1));
  s.leave();
  CHECK(s.core.store().mode() == StorePath::Mode::Off);
  auto d = store::Db::open(live_of(dir.path() / "saves", the_key()), store::FileKind::Playthrough, false);
  REQUIRE(d.has_value());
  CHECK(one(*d, "SELECT count(*) FROM conversations") == "1");
  CHECK(one(*d, "SELECT persona || ':' || end_reason FROM conversations") == "lantern_keeper:travel");
  CHECK(one(*d, "SELECT text FROM events WHERE kind = 'PlayerSaid'") == "Is the lamp yours?");
  CHECK(one(*d, "SELECT text FROM events WHERE kind = 'NpcSaid'") == "Hm. It keeps me as I keep it.");
  CHECK(one(*d, "SELECT json_array_length(payload, '$.pages') FROM events WHERE kind = 'NpcSaid'") == "2");
  // The end carries the core's own word, its counts and the stage. The persona perceived it directly, so its meeting
  // is folded (the audience rows are en-store's own to read: MEM §1.3's grep).
  CHECK(one(*d, "SELECT json_extract(payload, '$.reason') FROM events WHERE kind = 'ConversationEnded'") == "travel");
  CHECK(one(*d, "SELECT json_extract(payload, '$.stage') FROM events WHERE kind = 'ConversationEnded'") != "");
  CHECK(one(*d, "SELECT value FROM npc_facts WHERE persona = 'lantern_keeper' AND key = 'met_count'") == "1");
}

TEST_CASE("store path: other writes are coalesced and go out within 250 ms; speech takes the queue with it") {
  StorePath p;
  p.attach(kAccount);
  p.open(the_key(), 600000, 1, 0);
  std::vector<proto::DbRequest> out;
  p.pump(0, out);
  (void)p.on_result(proto::DbResult{out[0].seq, db::Opened{}});
  out.clear();
  p.pump(0, out);
  (void)p.on_result(proto::DbResult{out[0].seq, db::FoldEvents{1, {}}});
  REQUIRE(p.mode() == StorePath::Mode::Open);
  auto pack = load_content(content_with_overrides());
  REQUIRE(pack.has_value());
  Predicates predicates;
  const Directory cast = Directory::build(std::move(*pack), Realm::Eval, predicates);
  augur::Event place;
  place.kind = augur::PlaceEntered{6100, std::nullopt};
  place.event_class = augur::Class::P;
  place.t_play_ms = 610000;
  out.clear();
  p.write({place}, cast, nullptr, 100);
  p.pump(100, out);
  CHECK(out.empty());  // 100 ms after the last flush: held
  p.pump(249, out);
  CHECK(out.empty());
  p.pump(250, out);
  REQUIRE(out.size() == 1);  // at 250 ms it goes
  CHECK(std::get<db::Append>(out[0].op).event.payload == R"({"place_id":6100,"prev":null})");
  // A later place, then speech in the same step: both go at once, in order.
  out.clear();
  p.write({place}, cast, nullptr, 300);
  Commit started;
  started.kind = Commit::Kind::Started;
  started.conv = 1;
  started.persona = "lantern_keeper";
  p.commit(started, nullptr, cast, nullptr, 611000);
  p.pump(300, out);
  REQUIRE(out.size() == 3);
  CHECK(std::holds_alternative<db::Append>(out[0].op));
  CHECK(std::holds_alternative<db::ConvOpen>(out[1].op));
  CHECK(std::get<db::ConvOpen>(out[1].op).row.id == 1);
  CHECK(std::holds_alternative<db::Bundle>(out[2].op));  // MEM §4.1: the persona's bundle, for the first turn
}

TEST_CASE("store path: a lock at open is retried three times, then the world goes on with memory off and a toast") {
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  {
    Session first(saves);
    first.boot();
    first.world(10);
    first.leave();
  }
  auto other = store::Db::open(live_of(saves, the_key()), store::FileKind::Playthrough, false);
  REQUIRE(other.has_value());
  REQUIRE(store::exec(*other, "BEGIN IMMEDIATE").has_value());
  Session s(saves);
  s.boot();
  s.world(2);
  CHECK(s.core.state().phase == Phase::Playthrough);  // the first Busy: a retry 1 s later
  s.world(20);
  CHECK(s.core.state().phase == Phase::InWorld);
  CHECK(s.core.store().mode() == StorePath::Mode::Down);
  std::size_t opens = 0;
  for (const proto::DbRequest& r : s.sent) opens += std::holds_alternative<db::Open>(r.op) ? 1 : 0;
  CHECK(opens == 1 + StorePath::kBusyRetries);
  CHECK(s.toasted(memory_off_text(db::Fault::Busy)));
  REQUIRE(store::exec(*other, "ROLLBACK").has_value());
}

TEST_CASE("store path: the choice holds named chat with a toast and the writes; Forget trims, then lifts them") {
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  {
    Session first(saves, 2 * 3600000);  // two hours in
    first.boot();
    first.world(40);
    first.talk("Is the lamp yours?", "It keeps me as I keep it.");
    first.leave();
  }
  Session s(saves, 600000);  // an older save: ten minutes in
  s.boot();
  s.world(40);
  CHECK(s.core.state().phase == Phase::InWorld);  // the world goes on (barks would run)
  REQUIRE(s.core.store().mode() == StorePath::Mode::Choice);
  REQUIRE(s.core.store().choice().has_value());
  CHECK(s.core.store().choice()->turns >= 2);
  CHECK(s.core.store().choice()->save_pt < s.core.store().choice()->file_pt);
  const Outputs o = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
  CHECK(o.ui.chat.view != proto::BoxView::Open);
  s.world(1);
  CHECK(s.toasted(kChoicePending));
  s.t += 250;
  (void)s.feed(in(loading(s.t)));  // a P event of this session, held
  s.world(4);
  CHECK(s.core.store().held() >= 1);
  (void)s.feed(CoreInput{CoreInput::MemoryChoice{db::Choice::Forget}});
  CHECK(s.core.store().mode() == StorePath::Mode::Open);
  CHECK(s.core.store().held() == 0);
  s.leave();
  auto d = store::Db::open(live_of(saves, the_key()), store::FileKind::Playthrough, false);
  REQUIRE(d.has_value());
  CHECK(one(*d, "SELECT count(*) FROM conversations") == "0");  // the future is forgotten
  CHECK(one(*d, "SELECT count(*) FROM events WHERE t_play_ms >= 3600000") == "0");
  CHECK(one(*d, "SELECT count(*) FROM events WHERE t_play_ms < 3600000") != "0");  // this session's, lifted
}

TEST_CASE("store path: memory off opens nothing, and the world follows at once") {
  TempDir dir;
  Session s(dir.path() / "saves");
  Behaviour b = s.core.behaviour();
  b.features.memory = false;
  REQUIRE_FALSE(s.core.set_behaviour(b).has_value());
  s.boot();
  s.world(4);
  CHECK(s.core.state().phase == Phase::InWorld);
  CHECK(s.sent.empty());
  CHECK(s.core.store().mode() == StorePath::Mode::Off);
  Session none(dir.path() / "other", 600000, false);  // no account: the same
  none.boot();
  none.world(4);
  CHECK(none.core.state().phase == Phase::InWorld);
  CHECK(none.sent.empty());
}

TEST_CASE("store path: a tripped store leaves memory off for the session, with one toast naming the cause") {
  TempDir dir;
  Session s(dir.path() / "saves");
  s.boot();
  s.world(40);
  REQUIRE(s.core.store().mode() == StorePath::Mode::Open);
  (void)s.feed_result(proto::DbResult{999, db::Tripped{db::Fault::Full}});
  CHECK(s.core.store().mode() == StorePath::Mode::Down);
  s.world(2);
  CHECK(s.toasted(memory_off_text(db::Fault::Full)));
  const std::size_t n = s.sent.size();
  s.t += 250;
  (void)s.feed(in(loading(s.t)));
  s.world(4);
  CHECK(s.sent.size() == n);  // nothing more is written this session
}

TEST_CASE("store path: StageChanged carries the stages' ids; JourneyChanged starts the next journey's facts") {
  TempDir dir;
  StorePath p;
  p.attach(kAccount);
  p.open(the_key(), 600000, 1, 0);
  std::vector<proto::DbRequest> out;
  p.pump(0, out);
  REQUIRE(out.size() == 1);
  db::Opened opened;
  opened.next_event_id = 41;
  (void)p.on_result(proto::DbResult{out[0].seq, opened});
  out.clear();
  p.pump(1, out);
  REQUIRE(out.size() == 1);
  REQUIRE(std::holds_alternative<db::EventsForFold>(out[0].op));
  (void)p.on_result(proto::DbResult{out[0].seq, db::FoldEvents{1, {}}});
  REQUIRE(p.mode() == StorePath::Mode::Open);
  auto pack = load_content(content_with_overrides());
  REQUIRE(pack.has_value());
  Predicates predicates;
  const Directory cast = Directory::build(std::move(*pack), Realm::Eval, predicates);
  const auto keeper = cast.find("lantern_keeper");
  REQUIRE(keeper.has_value());
  augur::Event ev;
  ev.kind = augur::StageChanged{augur::PersonaId{static_cast<std::uint32_t>(*keeper)}, 0, 0};
  ev.event_class = augur::Class::P;
  ev.t_play_ms = 700000;
  ev.subject = augur::ActorRef{augur::ActorRef::Kind::Persona, {}, static_cast<std::uint32_t>(*keeper), 0};
  augur::Event ng;
  ng.kind = augur::JourneyChanged{1};
  ng.event_class = augur::Class::P;
  ng.journey = 1;
  ng.t_play_ms = 800000;
  p.write({ev, ng}, cast, nullptr, 2);
  out.clear();
  p.pump(300, out);
  REQUIRE(out.size() == 2);
  const auto& stage = std::get<db::Append>(out[0].op).event;
  CHECK(stage.id == 41);
  CHECK(stage.kind == "StageChanged");
  const std::string base = cast.persona(*keeper).stages.front().id;
  CHECK(stage.payload == R"({"from":")" + base + R"(","to":")" + base + R"("})");
  CHECK(stage.subject == "persona:lantern_keeper");
  const auto& journey = std::get<db::Append>(out[1].op).event;
  CHECK(journey.id == 42);
  CHECK(journey.journey == 2);
  CHECK(p.journey() == 2);
  CHECK(p.facts().empty());
}
