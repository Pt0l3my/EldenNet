#pragma once
// Tools › Workshop's invented facts (PAGES §4.6.8; en-proto's workshop.hpp) for the world's fixture state
// (tests/pages/world/topics.json, with Developer views on): a reply's exchange with the invented test persona, events
// in the golden form, flags, stages and a track of the shipped catalogue's shapes, a known set, two unknown characters,
// the recorder and a few kinds and rules. Every word, id and number is our own; no game text.
#include <string_view>

#include "en/proto/workshop.hpp"

namespace en::overlay::test {

namespace pr = en::proto;

inline pr::WorkshopView workshop_view() {
  pr::WorkshopView v;
  pr::WorkshopExchange& x = v.exchange;
  x.any = true;
  x.id = (std::uint64_t{1} << 48) + 3;
  x.at_ms = 1'204'100;
  x.slot = "dialogue";
  x.purpose = "reply";
  x.messages = {
      {"system", "You are the lantern keeper of an invented harbour. You speak plainly.", true},
      {"user", "As things stand now: the harbour's lamps are lit.\n\nThe Tarnished says: \"Are the lamps lit?\"",
       false},
  };
  x.max_tokens = 140;
  x.temperature = 0.8;
  x.stops = 2;
  x.started = true;
  x.ended = true;
  x.provider = "lantern-box";
  x.model = "invented-lantern-a";
  x.served_model = "invented-lantern-a-0425";
  x.text = "Every one of them, since dusk.";
  x.raw = "<think>The keeper is proud of the lamps.</think>Every one of them, since dusk.";
  x.finish = "stop";
  x.tokens_in = 812;
  x.tokens_out = 9;
  x.tokens_cached = 640;
  x.first_token_ms = 140;
  x.total_ms = 610;
  x.attempts = 1;
  v.events.seen = 3;
  v.events.dropped_t = 0;
  v.events.dropped_k = 1;
  pr::WorkshopWorld& w = v.world;
  w.have_frame = true;
  w.journey = 1;
  w.watch_version = 2;
  w.watched = 1;
  w.flags = {
      {"boss.lantern", 4100, "", 0, true, true, true, true, true, "T", "outcome", "unknown", 1},
      {"keeper.met", 4700, "keeper", 3, false, false, false, false, false, "U", "state", "reset", 0},
  };
  w.stages = {{"lamps_lit", "T", "boss('lantern')", "lantern", 1, 40},
              {"harbour_dark", "F", "!world('lamps_lit')", "", 0, 10}};
  w.tracks = {{"harbour", "lit"}, {"pier", ""}};
  v.known.personas = 1;
  v.known.rows = {{"lantern_keeper", "base", "w:lamps_lit", "fact", "near", true, "shown"},
                  {"lantern_keeper", "base", "q:harbour.lit", "rumour", "word", false, "offered"}};
  pr::WorkshopUnknown dog;
  dog.param = 12345600;
  dog.model = 2010;
  dog.kind = 1;
  dog.team = 6;
  dog.first_ms = 61'300;
  dog.last_ms = 1'203'900;
  dog.seen = 1180;
  dog.nearest_m = 6.5f;
  dog.toml = "[[persona]]\nid = \"new_12345600\"\nfile = \"personas/new_12345600.toml\"\n"
             "npc_params = [{ param = 12345600, form = \"default\", verified = false }]\nenabled = true\n";
  pr::WorkshopUnknown guard = dog;
  guard.param = 7700;
  guard.entity = 1043300800;
  guard.seen = 40;
  guard.nearest_m = 21;
  guard.last_ms = 1'190'000;
  guard.toml = "[[persona]]\nid = \"new_7700\"\nfile = \"personas/new_7700.toml\"\n"
               "npc_params = [{ param = 7700, form = \"default\", verified = false }]\n"
               "entity_ids = [{ id = 1043300800, verified = false }]\nenabled = true\n";
  v.unknown = {dog, guard};
  v.recording = {true, pr::RecordingWhy::AtStart, 1, 4180, 0, 13'770'000, 256ull << 20};
  v.kinds.kinds = {
      {"Struck", "Struck", "", "subject, attacker, victim",
       "att (actor or null), vic (actor), hard (bool), unseen (bool), count (u32), frac_total (number), "
       "dmg_type (int)",
       1},
      {"NpcSaid", "", "the Pipeline source: conversations and the call-out (v0.2 on)", "", "", 0},
  };
  v.kinds.rules = {{"struck.player", "Struck", "K", "hp_frac", "drop_over_window", "combat", "struck:{subject}", 8, 0,
                    "combat.toml:12"}};
  return v;
}

inline const pr::WorkshopView* workshop_fixture(std::string_view state) {
  static const pr::WorkshopView world = workshop_view();
  return state == "world" ? &world : nullptr;
}

// The world's events stream: three rows in the golden form (invented handles' names).
inline pr::WorkshopEvent workshop_event_of(std::uint64_t t_ms, std::string_view kind, std::string_view cls,
                                           std::string_view source, std::string_view line) {
  return pr::WorkshopEvent{t_ms, std::string(kind), std::string(cls), std::string(source), std::string(line)};
}

}  // namespace en::overlay::test
