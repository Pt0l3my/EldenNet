// The names of the game messages' enums (snapshot.hpp, backend.hpp), as their JSON forms and logs write them.
#include "en/proto/backend.hpp"
#include "en/proto/snapshot.hpp"

namespace en::proto {

const char* to_string(MultiplayRole v) noexcept {
  switch (v) {
    case MultiplayRole::Unknown: return "Unknown";
    case MultiplayRole::Host: return "Host";
    case MultiplayRole::Cooperator: return "Cooperator";
    case MultiplayRole::Invader: return "Invader";
    case MultiplayRole::Other: return "Other";
  }
  return "Unknown";
}

const char* to_string(LobbyState v) noexcept {
  switch (v) {
    case LobbyState::Unknown: return "Unknown";
    case LobbyState::None: return "None";
    case LobbyState::Hosting: return "Hosting";
    case LobbyState::Joined: return "Joined";
    case LobbyState::Other: return "Other";
  }
  return "Unknown";
}

const char* to_string(Screen v) noexcept {
  switch (v) {
    case Screen::Unknown: return "Unknown";
    case Screen::InGame: return "InGame";
    case Screen::Loading: return "Loading";
    case Screen::Title: return "Title";
  }
  return "Unknown";
}

const char* to_string(HudState v) noexcept {
  switch (v) {
    case HudState::Unknown: return "Unknown";
    case HudState::HideAll: return "HideAll";
    case HudState::ShowAll: return "ShowAll";
    case HudState::PopupMenu: return "PopupMenu";
    case HudState::Default: return "Default";
  }
  return "Unknown";
}

const char* to_string(BodyType v) noexcept {
  switch (v) {
    case BodyType::Unknown: return "Unknown";
    case BodyType::A: return "A";
    case BodyType::B: return "B";
  }
  return "Unknown";
}

const char* to_string(Status v) noexcept {
  switch (v) {
    case Status::Frostbite: return "Frostbite";
    case Status::Poison: return "Poison";
    case Status::ScarletRot: return "ScarletRot";
    case Status::Sleep: return "Sleep";
    case Status::Hemorrhage: return "Hemorrhage";
    case Status::Madness: return "Madness";
    case Status::DeathBlight: return "DeathBlight";
  }
  return "Frostbite";
}

const char* to_string(ParamKind v) noexcept {
  switch (v) {
    case ParamKind::NpcParam: return "NpcParam";
    case ParamKind::BonfireWarpParam: return "BonfireWarpParam";
    case ParamKind::WorldMapPlaceNameParam: return "WorldMapPlaceNameParam";
    case ParamKind::SpEffectParam: return "SpEffectParam";
  }
  return "NpcParam";
}

const char* to_string(Group v) noexcept {
  switch (v) {
    case Group::Session: return "session";
    case Group::World: return "world";
    case Group::Combat: return "combat";
    case Group::Player: return "player";
    case Group::Flags: return "flags";
    case Group::Text: return "text";
    case Group::Ui: return "ui";
    case Group::Place: return "place";
    case Group::Camera: return "camera";
    case Group::Params: return "params";
    case Group::Status: return "status";
  }
  return "session";
}

const char* to_string(GroupState v) noexcept {
  switch (v) {
    case GroupState::Untested: return "Untested";
    case GroupState::Ok: return "Ok";
    case GroupState::Tripped: return "Tripped";
  }
  return "Untested";
}

const char* to_string(ReadError v) noexcept {
  switch (v) {
    case ReadError::Inert: return "Inert";
    case ReadError::Loading: return "Loading";
    case ReadError::BudgetSpent: return "BudgetSpent";
    case ReadError::Faulted: return "Faulted";
    case ReadError::Ended: return "Ended";
    case ReadError::Unsteady: return "Unsteady";
  }
  return "Inert";
}

const char* to_string(ApplyError v) noexcept {
  switch (v) {
    case ApplyError::Inert: return "Inert";
    case ApplyError::Invalid: return "Invalid";
    case ApplyError::Unsupported: return "Unsupported";
  }
  return "Inert";
}

}  // namespace en::proto
