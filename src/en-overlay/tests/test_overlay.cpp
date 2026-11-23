// en-overlay's connection against the fake GraceUI (fake_grace.hpp; the Windows builds): connect's answers, the banner
// and the badge as notices, the world-state claims, the F11 view on Dear ImGui's core (its atlas in the registration
// window, a hidden panel, F11 through poll, lists that pass GraceUI's checks), the Mark's hotkey, and either hotkey
// registered unbound (D164).
#include <doctest.h>

#include <memory>
#include <string>

#include "en/overlay/overlay.hpp"
#include "fake_grace.hpp"

using namespace en::overlay;

namespace {

Options options(bool view, bool mark = false) {
  Options o;
  o.version = {0, 1, 0};
  o.view = view;
  o.mark = mark;
  return o;
}

ViewStatus status() {
  ViewStatus s;
  s.version = {0, 1, 0};
  s.init = "ready";
  s.phase = "title";
  s.level = "L2";
  s.snapshots = 42;
  return s;
}

std::unique_ptr<Overlay> connect(fake::Grace& g, const Options& o, Report& r) {
  return std::unique_ptr<Overlay>(Overlay::connect(graceui::Api(g.api()), o, r));
}

}  // namespace

TEST_CASE("overlay: connect as eldennet, with EldenNet's floor and the caps it uses") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(false), r);
  REQUIRE(o != nullptr);
  CHECK(r.outcome == Outcome::Connected);
  CHECK(g.id == "eldennet");
  CHECK(g.name == "EldenNet");
  CHECK(g.floor == GRACEUI_VERSION_PACK(0, 2, 22));  // GraceUI 0.2.22: the editor's chosen row (D218), on 1.5's wait
  CHECK(g.mod_version == GRACEUI_VERSION_PACK(0, 1, 0));
  CHECK(g.mod_caps == "notices world-state hotkeys");
  CHECK(std::string(r.view) == "off");
  CHECK(r.world);
  CHECK(r.hotkeys);
  CHECK(g.panels.empty());
  CHECK(g.hotkeys.empty());
  CHECK(g.textures.empty());
  CHECK_FALSE(o->notices_only());
}

TEST_CASE("overlay: a failed connect leaves EldenNet headless, each answer named") {
  struct Case {
    GraceUiStatus status;
    std::uint32_t reason;
    Outcome outcome;
  };
  for (const Case c : {Case{GRACEUI_TOO_OLD, 0, Outcome::TooOld}, Case{GRACEUI_DUPLICATE_ID, 0, Outcome::DuplicateId},
                       Case{GRACEUI_REFUSED, GRACEUI_REFUSED_CAPS, Outcome::Refused},
                       Case{GRACEUI_INACTIVE, GRACEUI_INACTIVE_HOST_FAILED, Outcome::Inactive},
                       Case{GRACEUI_INACTIVE, GRACEUI_INACTIVE_RENDER_DEAD, Outcome::Inactive}}) {
    fake::Grace g;
    g.connect_status = c.status;
    g.connect_reason = c.reason;
    Report r;
    auto o = connect(g, options(true), r);
    CHECK(o == nullptr);
    CHECK(r.outcome == c.outcome);
    CHECK(r.reason == c.reason);
    CHECK(g.notice_calls.empty());
  }
  Report r;
  CHECK(Overlay::connect(graceui::Api(), options(true), r) == nullptr);
  CHECK(r.outcome == Outcome::Absent);
  // An older table without connect's slot answers not_yet, and nothing is called.
  fake::Grace g;
  g.table_size = 16;
  CHECK(connect(g, options(true), r) == nullptr);
  CHECK(r.outcome == Outcome::NotYet);
  CHECK(g.connects == 0);
  CHECK(std::string(to_string(Outcome::NoticesOnly)) == "notices_only");
}

TEST_CASE("overlay: the banner is one notice under EldenNet's key, replaced in place and withdrawn by an empty text") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(false), r);
  REQUIRE(o != nullptr);
  CHECK(o->banner("EldenNet inactive: game build 3.14.15.0 not supported yet") == GRACEUI_OK);
  CHECK(o->banner("EldenNet inactive: game build 3.14.15.0 not supported yet") == GRACEUI_OK);  // the same: no call
  CHECK(g.notice_calls.size() == 1);
  CHECK(g.notices.at(kBannerKey) == "EldenNet inactive: game build 3.14.15.0 not supported yet");
  CHECK(o->banner("EldenNet inactive: the game did not finish starting") == GRACEUI_OK);
  CHECK(g.notices.size() == 1);
  CHECK(g.notices.at(kBannerKey) == "EldenNet inactive: the game did not finish starting");
  CHECK(o->badge("EldenNet paused (multiplayer)") == GRACEUI_OK);
  CHECK(g.notices.at(kBadgeKey) == "EldenNet paused (multiplayer)");
  CHECK(o->badge("") == GRACEUI_OK);
  CHECK(g.notices.count(kBadgeKey) == 0);
  CHECK(o->badge("") == GRACEUI_OK);  // nothing showing: nothing called
  CHECK(o->banner("") == GRACEUI_OK);
  CHECK(g.notices.empty());
  CHECK(g.notice_calls.size() == 5);
  // A text past 512 bytes is cut to fit, never refused.
  CHECK(o->banner(std::string(600, 'a')) == GRACEUI_OK);
  CHECK(g.notices.at(kBannerKey).size() == GRACEUI_NOTICE_TEXT_MAX);
}

TEST_CASE("overlay: the anti-cheat's notice-only mode shows the banner and nothing else") {
  fake::Grace g;
  g.connect_status = GRACEUI_INACTIVE;
  g.connect_reason = GRACEUI_INACTIVE_ANTI_CHEAT;
  Report r;
  auto o = connect(g, options(true, true), r);
  REQUIRE(o != nullptr);
  CHECK(r.outcome == Outcome::NoticesOnly);
  CHECK(o->notices_only());
  CHECK(std::string(r.view) == "no_caps");
  CHECK_FALSE(r.mark);
  CHECK(g.panels.empty());
  CHECK(g.textures.empty());
  CHECK(g.hotkeys.empty());
  CHECK(o->banner("EldenNet inactive: Easy Anti-Cheat is running.") == GRACEUI_OK);
  CHECK(g.notices.at(kBannerKey) == "EldenNet inactive: Easy Anti-Cheat is running.");
  CHECK(o->world(GRACEUI_WORLD_ANTI_CHEAT) == GRACEUI_NOT_YET);  // no provider in this mode
  CHECK(g.world_calls.empty());
  const Overlay::Step s = o->step(status());
  CHECK(s.marks == 0);
  CHECK_FALSE(s.lost);
  CHECK(g.polls == 0);
}

TEST_CASE("overlay: the world-state claims, sent when they change") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(false), r);
  REQUIRE(o != nullptr);
  CHECK(o->world(0) == GRACEUI_OK);  // the first claim makes EldenNet a provider, even with none set
  CHECK(o->world(0) == GRACEUI_OK);
  CHECK(o->world(GRACEUI_WORLD_TITLE) == GRACEUI_OK);
  CHECK(o->world(GRACEUI_WORLD_TITLE) == GRACEUI_OK);
  CHECK(o->world(GRACEUI_WORLD_DANGER | GRACEUI_WORLD_MULTIPLAYER) == GRACEUI_OK);
  REQUIRE(g.world_calls.size() == 3);
  CHECK(g.world_calls[1] == GRACEUI_WORLD_TITLE);
  // A GraceUI without the cap: no claim is made.
  fake::Grace h;
  h.caps = {"notices", "hotkeys"};
  auto p = connect(h, options(false), r);
  REQUIRE(p != nullptr);
  CHECK_FALSE(r.world);
  CHECK(p->world(GRACEUI_WORLD_TITLE) == GRACEUI_NOT_YET);
  CHECK(h.world_calls.empty());
}

TEST_CASE("overlay: the F11 view: its atlas before any submit, a hidden panel, F11, then lists GraceUI accepts") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(true), r);
  REQUIRE(o != nullptr);
  CHECK(std::string(r.view) == "ready");
  CHECK(g.mod_caps == "notices world-state hotkeys panels textures");
  REQUIRE(g.textures.size() == 1);  // Dear ImGui's atlas, in the registration window
  CHECK_FALSE(g.window_closed);
  REQUIRE(g.panels.size() == 1);
  const std::uint64_t panel = g.panel_of();
  const fake::Panel& p = g.panels.at(panel);
  CHECK_FALSE(p.shown);
  CHECK(p.desc.layer == GRACEUI_LAYER_PANEL);
  CHECK(p.desc.anchor == GRACEUI_ANCHOR_TOP_LEFT);
  CHECK(p.desc.width == kViewWidth);
  CHECK(p.desc.height == kViewHeight);
  CHECK(p.desc.flags == 0);  // it takes no input (D142)
  REQUIRE(g.hotkeys.size() == 1);
  CHECK(g.hotkeys[0].action == "debug-view");
  CHECK(g.hotkeys[0].key == kKeyF11);
  CHECK(o->view_ready());

  // Nothing pressed: polled, nothing submitted, still hidden.
  Overlay::Step s = o->step(status());
  CHECK(g.polls >= 1);
  CHECK(g.submits == 0);
  CHECK_FALSE(o->view_open());

  // F11 once: one list submitted, then shown.
  g.press("debug-view");
  s = o->step(status());
  CHECK(s.view_presses == 1);
  CHECK(s.refused == GRACEUI_OK);
  CHECK(o->view_open());
  CHECK(p.shown);
  CHECK(p.lists == 1);
  CHECK(g.refused_lists == 0);
  CHECK(g.last_refusal.empty());
  CHECK(p.last_vertices > 100);  // the ground, the ribbon and the text
  CHECK(g.texture_too_late == 0);

  // Open: a list every step.
  s = o->step(status());
  s = o->step(status());
  CHECK(p.lists == 3);
  CHECK(g.refused_lists == 0);

  // F11 again: hidden; nothing more submitted.
  g.press("debug-view");
  s = o->step(status());
  CHECK_FALSE(o->view_open());
  CHECK_FALSE(p.shown);
  const std::uint64_t lists = p.lists;
  s = o->step(status());
  CHECK(p.lists == lists);

  // Two presses since the last look: an even count leaves it as it was (D155); three toggle it.
  g.press("debug-view", 2);
  s = o->step(status());
  CHECK(s.view_presses == 2);
  CHECK_FALSE(o->view_open());
  g.press("debug-view", 3);
  s = o->step(status());
  CHECK(o->view_open());
}

TEST_CASE("overlay: a view that cannot be made leaves the connection as it is") {
  SUBCASE("a GraceUI without panels") {
    fake::Grace g;
    g.caps = {"notices", "world-state", "hotkeys"};
    Report r;
    auto o = connect(g, options(true), r);
    REQUIRE(o != nullptr);
    CHECK(std::string(r.view) == "no_caps");
    CHECK(g.textures.empty());
    CHECK_FALSE(o->view_ready());
    CHECK(o->banner("text") == GRACEUI_OK);
  }
  SUBCASE("the atlas refused") {
    fake::Grace g;
    g.refuse_textures = true;
    Report r;
    auto o = connect(g, options(true), r);
    REQUIRE(o != nullptr);
    CHECK(std::string(r.view) == "atlas");
    CHECK(g.panels.empty());
    g.press("debug-view");
    (void)o->step(status());
    CHECK(g.submits == 0);
  }
}

TEST_CASE("overlay: the Mark's hotkey on Pause, at once or later, its presses counted") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(true, true), r);
  REQUIRE(o != nullptr);
  CHECK(r.mark);
  REQUIRE(g.hotkeys.size() == 2);
  CHECK(g.hotkeys[1].action == "mark");
  CHECK(g.hotkeys[1].key == kKeyPause);
  CHECK(o->register_mark() == GRACEUI_OK);  // once only
  CHECK(g.hotkeys.size() == 2);
  g.press("mark");
  g.press("mark", 2);
  const Overlay::Step s = o->step(status());
  CHECK(s.marks == 3);
  CHECK(s.view_presses == 0);
  CHECK_FALSE(o->view_open());

  fake::Grace h;
  auto p = connect(h, options(false), r);
  REQUIRE(p != nullptr);
  CHECK_FALSE(p->mark_registered());
  CHECK(p->register_mark() == GRACEUI_OK);
  CHECK(p->mark_registered());
  h.press("mark");
  CHECK(p->step(status()).marks == 1);
}

TEST_CASE("overlay: poll passes over the display's event and any kind it does not use, and goes on") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(true, true), r);
  REQUIRE(o != nullptr);
  g.display();  // GraceUI 0.2.2's, after warm-up (D180)
  g.press("mark");
  GraceUiEvent later{};
  later.size = sizeof later;
  later.kind = 77;  // a kind of a later GraceUI's
  g.events.push_back(later);
  g.display();
  g.press("debug-view");
  const Overlay::Step s = o->step(status());
  CHECK(s.marks == 1);
  CHECK(s.view_presses == 1);
  CHECK(o->view_open());
  CHECK(g.events.empty());  // everything waiting was taken
  // Asked at the vendored header's size, ABI 1.6's.
  CHECK(g.poll_sizes.count(static_cast<std::uint32_t>(sizeof(GraceUiEvent))) == 1);

  // A GraceUI without the holder's events is asked at 0.1's size, and never gives the display's.
  fake::Grace h;
  h.caps = {"notices", "panels", "textures", "world-state", "hotkeys"};
  auto p = connect(h, options(false, true), r);
  REQUIRE(p != nullptr);
  h.display();
  h.press("mark");
  CHECK(p->step(status()).marks == 1);
  CHECK(h.poll_sizes.count(16u) == 1);
  CHECK(h.events.size() == 1);  // the display's, which an event of 0.1's size never gets
}

TEST_CASE("overlay: a hotkey another mod took registers unbound, with its id (GraceUI D164)") {
  SUBCASE("F11: the view is made, nothing opens it until the player binds F11") {
    fake::Grace g;
    g.taken_keys = {kKeyF11};
    Report r;
    auto o = connect(g, options(true), r);
    REQUIRE(o != nullptr);
    CHECK(std::string(r.view) == "unbound");
    REQUIRE(g.panels.size() == 1);
    REQUIRE(g.hotkeys.size() == 1);
    CHECK_FALSE(g.hotkeys[0].bound);
    CHECK(o->view_ready());
    CHECK_FALSE(o->view_bound());
    (void)o->step(status());
    CHECK_FALSE(o->view_open());
    // Bound later on GraceUI's Hotkeys page, its presses come under the same id: the view opens.
    g.press("debug-view");
    (void)o->step(status());
    CHECK(o->view_open());
  }
  SUBCASE("Pause: the Mark is registered and unbound, so the launcher keeps Pause") {
    fake::Grace g;
    g.taken_keys = {kKeyPause};
    Report r;
    auto o = connect(g, options(true, true), r);
    REQUIRE(o != nullptr);
    CHECK(std::string(r.view) == "ready");
    CHECK(o->view_bound());
    CHECK(r.mark);
    CHECK_FALSE(r.mark_bound);
    CHECK(o->mark_registered());
    CHECK_FALSE(o->mark_bound());
    REQUIRE(g.hotkeys.size() == 2);
    CHECK_FALSE(g.hotkeys[1].bound);
    CHECK(o->register_mark() == GRACEUI_UNBOUND);  // asked again: the same answer, GraceUI not called
    CHECK(g.hotkeys.size() == 2);

    fake::Grace h;
    h.taken_keys = {kKeyPause};
    auto later = connect(h, options(false), r);
    REQUIRE(later != nullptr);
    CHECK_FALSE(r.mark);
    CHECK(later->register_mark() == GRACEUI_UNBOUND);
    CHECK(later->mark_registered());
    CHECK_FALSE(later->mark_bound());
  }
  SUBCASE("both free: bound") {
    fake::Grace g;
    Report r;
    auto o = connect(g, options(true, true), r);
    REQUIRE(o != nullptr);
    CHECK(o->view_bound());
    CHECK(r.mark_bound);
    CHECK(o->mark_bound());
    CHECK(o->register_mark() == GRACEUI_OK);
  }
}

TEST_CASE("overlay: GraceUI gone: lost once, nothing more is called") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(true), r);
  REQUIRE(o != nullptr);
  g.poll_status = GRACEUI_NOT_CONNECTED;
  CHECK(o->step(status()).lost);
  const std::uint32_t polls = g.polls;
  CHECK(o->step(status()).lost);
  CHECK(g.polls == polls);
  CHECK(o->banner("text") == GRACEUI_NOT_CONNECTED);
  CHECK(g.notice_calls.empty());
}

// ---- EldenNet's space through ABI 1's protocol slots (D169, eldennet-changes §23).

namespace {

Options with_space(std::uint32_t views = GRACEUI_VIEW_GAME | GRACEUI_VIEW_BROWSER) {
  Options o = options(true, false);
  o.space_views = views;
  return o;
}

nlohmann::json parsed(const std::string& text) { return nlohmann::json::parse(text); }

}  // namespace

TEST_CASE("overlay: the space registered once at connect, for both views, at its increment") {
  fake::Grace g;
  Report r;
  auto o = connect(g, with_space(), r);
  REQUIRE(o != nullptr);
  CHECK(g.mod_caps == "notices world-state hotkeys panels textures pages");
  CHECK(r.pages == GRACEUI_OK);
  CHECK(o->pages());
  CHECK(g.register_calls == 1);
  CHECK(g.space_views == (GRACEUI_VIEW_GAME | GRACEUI_VIEW_BROWSER));
  CHECK(g.space_version == GRACEUI_VERSION_PACK(0, 1, 0));
  CHECK(g.space_increment == std::string(kIncrement));  // 0.2 since the AI pages
  CHECK(g.space_pages == std::string(space_pages()));
  CHECK(g.space_settings == std::string(settings_description()));
  CHECK(std::string(r.view) == "ready");  // the rest as before

  // The banner path's: the game's view alone (PAGES §4.4).
  fake::Grace h;
  auto p = connect(h, with_space(GRACEUI_VIEW_GAME), r);
  REQUIRE(p != nullptr);
  CHECK(h.space_views == GRACEUI_VIEW_GAME);

  // Without the pages cap (a GraceUI before 0.2), and in the notice-only mode, nothing is registered.
  fake::Grace old;
  old.caps = {"notices", "panels", "textures", "world-state", "hotkeys"};
  auto q = connect(old, with_space(), r);
  REQUIRE(q != nullptr);
  CHECK(r.pages == GRACEUI_NOT_YET);
  CHECK_FALSE(q->pages());
  CHECK(old.register_calls == 0);
  fake::Grace ac;
  ac.connect_status = GRACEUI_INACTIVE;
  ac.connect_reason = GRACEUI_INACTIVE_ANTI_CHEAT;
  auto n = connect(ac, with_space(), r);
  REQUIRE(n != nullptr);
  CHECK(n->notices_only());
  CHECK(ac.register_calls == 0);
}

TEST_CASE("overlay: watched topics published, each command answered once at once, nothing while unwatched") {
  fake::Grace g;
  Report r;
  auto o = connect(g, with_space(), r);
  REQUIRE(o != nullptr);
  Space space;
  space.log_line("init state=ready", 10);
  SpaceFacts facts;
  facts.version = {0, 1, 0};
  facts.graceui = g.version;
  facts.init = en::proto::watch::InitState::Ready;
  facts.level = Level::Observing;
  facts.where = Where::Title;

  // Nothing watched: nothing published.
  Overlay::Step s = o->step(status(), &space, &facts, 1000);
  CHECK(s.published == 0);
  CHECK(g.published.empty());

  // The menu watches the settings, a browser the status and the logs, with the display's event among them.
  g.watch("settings");
  g.display();
  g.watch("status");
  g.watch("logs");
  g.press("debug-view");
  s = o->step(status(), &space, &facts, 1000);
  CHECK(s.view_presses == 1);  // hotkeys as before
  CHECK(s.published == 3);
  CHECK(s.page_refused == GRACEUI_OK);
  REQUIRE(g.published_of("settings").size() == 1);
  CHECK(parsed(g.published_of("settings")[0])["body"]["values"]["debug.log_level"] == "info");
  CHECK(parsed(g.published_of("status")[0])["body"]["word"] == "Dormant");
  CHECK(parsed(g.published_of("logs")[0])["rows"][0]["line"] == "init state=ready");
  CHECK(g.events.empty());

  // A command: answered at once, exactly once, through the space's one command path; its change published.
  const std::uint64_t ok = g.command("settings.set", R"({"key":"debug.log_level","value":"debug"})");
  const std::uint64_t bad = g.command("settings.set", R"({"key":"debug.log_level","value":"loud"})");
  const std::uint64_t unknown = g.command("voice.sample", "{}");
  s = o->step(status(), &space, &facts, 1100);
  CHECK(s.commands == 3);
  CHECK(g.answer_refused == 0);
  REQUIRE(g.answers.size() == 3);
  CHECK(parsed(g.answers[ok])["ok"] == true);
  CHECK(parsed(g.answers[bad])["error"] == "invalid_value");
  CHECK(parsed(g.answers[unknown])["error"] == "unknown_command");
  REQUIRE(g.published_of("settings").size() == 2);
  CHECK(parsed(g.published_of("settings")[1])["body"]["values"]["debug.log_level"] == "debug");
  const auto changes = space.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "debug.log_level");

  // Unwatched: nothing more, though it changes.
  g.unwatch("status");
  g.unwatch("settings");
  g.unwatch("logs");
  (void)o->step(status(), &space, &facts, 1200);
  const std::size_t before = g.published.size();
  facts.level = Level::Inactive;
  facts.stop = Stop::TaskTripped;
  facts.stop_text = "EldenNet stopped reading the game after an error. Your game is not affected.";
  space.log_line("tripped group=camera", 1300);
  (void)o->step(status(), &space, &facts, 5000);
  CHECK(g.published.size() == before);
  CHECK_FALSE(space.watching("status"));

  // A command with no space to answer it (a defect) is answered all the same: internal.
  const std::uint64_t orphan = g.command("settings.set", R"({"key":"debug.panel","value":true})");
  s = o->step(status());
  CHECK(s.commands == 1);
  CHECK(parsed(g.answers[orphan])["error"] == "internal");
}

TEST_CASE("overlay: the profile editor's commands answered later, once each, with their reply handles (S7.2.b)") {
  fake::Grace g;
  Report r;
  auto o = connect(g, with_space(), r);
  REQUIRE(o != nullptr);
  Space space;
  CharacterFacts characters;
  ProfileRow kale;
  kale.key = "persona.test";
  kale.name = "Invented";
  characters.rows = {kale};
  characters.draft_provider = "box";
  SpaceFacts facts;
  facts.characters = &characters;
  g.watch("profiles/persona.test");
  const std::uint64_t save = g.command("profile.save", R"({"target":"persona.test","rev":0,"doc":{}})");
  const std::uint64_t draft = g.command("profile.draft", R"({"target":"persona.test","notes":"a test"})");
  const std::uint64_t bad = g.command("profile.save", R"({"target":"persona.none","rev":0,"doc":{}})");
  Overlay::Step s = o->step(status(), &space, &facts, 1000);
  CHECK(s.commands == 3);
  // The refusal at once; the two taken wait for the DLL, answered by nobody yet.
  CHECK(g.answers.size() == 1);
  CHECK(parsed(g.answers[bad])["error"] == "invalid_value");
  CHECK(parsed(g.published_of("profiles/persona.test")[0])["body"]["rev"] == 0);
  std::vector<ProfileRequest> taken = space.take_profile_requests();
  REQUIRE(taken.size() == 2);
  ProfileAnswer saved;
  saved.chosen = "persona.test";
  space.answer_profile(taken[0], saved);
  CHECK(o->answer_later(space) == 1);  // the DLL's at once, not at the next step
  CHECK(parsed(g.answers[save])["body"]["chosen"] == "persona.test");
  ProfileAnswer drafted;
  drafted.draft = {{"instructions", "Invented words."}};
  space.answer_profile(taken[1], drafted);
  s = o->step(status(), &space, &facts, 1100);  // or at the next step, before anything is published
  CHECK(parsed(g.answers[draft])["body"]["draft"]["instructions"] == "Invented words.");
  CHECK(g.answer_refused == 0);
  CHECK(o->answer_later(space) == 0);  // each once
}

TEST_CASE("overlay: a space's event too large for the buffer is kept, never answered twice nor lost") {
  fake::Grace g;
  Report r;
  auto o = connect(g, with_space(), r);
  REQUIRE(o != nullptr);
  Space space;
  SpaceFacts facts;
  // A view's command whose frame is larger than the buffer: GraceUI answers full and keeps it; the step goes on, and
  // nothing is lost or answered twice (EldenNet's own buffer is GRACEUI_FRAME_FROM_PAGE_MAX, which always suffices).
  g.command("settings.set", R"({"key":"debug.panel","value":true,"pad":")" + std::string(20000, 'x') + R"("})");
  g.press("debug-view");
  const Overlay::Step s = o->step(status(), &space, &facts, 10);
  CHECK(s.commands == 0);
  CHECK(g.answers.empty());
  CHECK(g.events.size() == 2);  // the command, kept, and the hotkey behind it
}
