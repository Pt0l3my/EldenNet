#pragma once
// Snapshots for the core's tests: the session and lifecycle fields the slice reads, with invented values.
#include <cstdint>
#include <memory>

#include "en/core/core.hpp"
#include "en/proto/snapshot.hpp"

namespace en::core::test {

inline proto::ReadHealth healthy(bool phase_b = true) {
  proto::ReadHealth h;
  for (auto& g : h.groups) {
    g.state = proto::GroupState::Ok;
    g.phase_b = phase_b;
  }
  return h;
}

// A solo world of one's own, the main player present.
inline proto::WorldSnapshot world(Mono ms, std::uint64_t play_ms = 0, std::uint32_t counter = 7) {
  proto::WorldSnapshot s;
  s.mono_ms = ms;
  s.health = healthy();
  s.session.role = proto::MultiplayRole::Host;
  s.session.is_my_world = true;
  s.session.lobby = proto::LobbyState::None;
  s.session.player_set_len = 1;
  s.life.main_player = true;
  s.life.screen = proto::Screen::InGame;
  s.life.load_counter = counter;
  proto::Player p;
  p.save_slot = 3;
  p.archetype = 5;
  p.play_time_ms = play_ms == 0 ? 600000 + ms : play_ms;
  s.player = p;
  return s;
}

inline proto::WorldSnapshot title(Mono ms) {
  proto::WorldSnapshot s;
  s.mono_ms = ms;
  s.health = healthy(false);
  s.session.lobby = proto::LobbyState::None;
  s.life.screen = proto::Screen::Title;
  return s;
}

inline proto::WorldSnapshot loading(Mono ms, std::uint32_t counter = 8) {
  proto::WorldSnapshot s;
  s.mono_ms = ms;
  s.health = healthy();
  s.life.screen = proto::Screen::Loading;
  s.life.load_counter = counter;
  return s;
}

inline CoreInput in(const proto::WorldSnapshot& s) { return CoreInput{std::make_unique<proto::WorldSnapshot>(s)}; }
inline CoreInput in(const proto::Lifecycle& l) { return CoreInput{l}; }

inline proto::Lifecycle data_ready() {
  proto::Capabilities caps;
  caps.groups = proto::FlagSetTraits<proto::GroupSet>::all;
  return proto::Lifecycle{proto::Lifecycle::DataReady{proto::BuildKey{0}, caps}};
}

inline proto::Lifecycle playthrough_ready(std::uint8_t slot = 3, const char* name = "Vessel",
                                          std::uint8_t archetype = 5) {
  proto::Lifecycle::PlaythroughReady p;
  p.slot = slot;
  (void)p.name.try_assign(name);
  p.archetype = archetype;
  p.keepsake = 2;
  p.play_time_ms = 600000;
  return proto::Lifecycle{p};
}

inline proto::Lifecycle tripped(proto::Group g) { return proto::Lifecycle{proto::Lifecycle::GroupTripped{g}}; }

}  // namespace en::core::test
