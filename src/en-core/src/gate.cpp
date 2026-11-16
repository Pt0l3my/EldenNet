// The offline and multiplayer gate (gate.hpp; ARCH §5.1).
#include "en/core/gate.hpp"

namespace en::core {

const char* to_string(Unclean why) noexcept {
  switch (why) {
    case Unclean::None:
      return "none";
    case Unclean::Gap:
      return "gap";
    case Unclean::SessionFailed:
      return "session_failed";
    case Unclean::AntiCheat:
      return "anticheat";
    case Unclean::NotSolo:
      return "not_solo";
    case Unclean::Lobby:
      return "lobby";
    case Unclean::Loading:
      return "loading";
  }
  return "unknown";
}

Verdict judge(const proto::WorldSnapshot& s, bool session_failed) noexcept {
  using proto::LobbyState;
  using proto::MultiplayRole;
  if (s.gap) return {false, Unclean::Gap};
  const bool session_ok = !session_failed && s.health.of(proto::Group::Session).state == proto::GroupState::Ok;
  if (!session_ok) return {false, Unclean::SessionFailed};
  if (s.session.anticheat) return {false, Unclean::AntiCheat};
  const bool in_world = s.life.main_player;
  if (in_world) {
    const bool solo_world = s.session.role == MultiplayRole::Host && s.session.is_my_world &&
                            s.session.lobby == LobbyState::None && s.session.player_set_len == 1;
    return solo_world ? Verdict{true, Unclean::None} : Verdict{false, Unclean::NotSolo};
  }
  if (s.life.screen != proto::Screen::Title) return {false, Unclean::Loading};
  if (s.session.lobby != LobbyState::None) return {false, Unclean::Lobby};
  return {true, Unclean::None};
}

bool Gate::step(const proto::WorldSnapshot& s) noexcept {
  const bool was = open_;
  if (s.health.of(proto::Group::Session).state == proto::GroupState::Tripped) session_failed_ = true;
  last_ = judge(s, session_failed_);
  // A gap between two processed snapshots longer than 1,000 ms, or a clock that went back, starts the hold again.
  const bool continuous = last_ms_ && s.mono_ms >= *last_ms_ && s.mono_ms - *last_ms_ <= kGateMaxGapMs;
  last_ms_ = s.mono_ms;
  if (!last_.clean) {
    open_ = false;  // at once, on this snapshot (C4)
    clean_since_.reset();
  } else {
    if (!clean_since_ || !continuous) clean_since_ = s.mono_ms;
    if (!open_ && s.mono_ms - *clean_since_ >= kGateHoldMs) open_ = true;
  }
  return open_ != was;
}

bool Gate::session_failed_now() noexcept {
  const bool was = open_;
  session_failed_ = true;
  last_ = {false, Unclean::SessionFailed};
  open_ = false;
  clean_since_.reset();
  return open_ != was;
}

}  // namespace en::core
