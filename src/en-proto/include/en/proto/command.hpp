#pragma once
// What the core asks of the game task (DESIGN §2.3; channel C4). In v1 every command only configures what the task
// reads: the flags it watches, the characters it follows every frame, and the text and param rows it looks up. None
// writes game memory or calls the game (R2). Freeze, NativeLine and Action are reserved behind build options that are
// off in v1, and are not here.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

namespace en::proto {

struct GameCommand {
  // The flag ids the task reads into each FlagFrame, bit i for ids[i], at most kMaxWatchFlags of them (MEM §6.2). The
  // list is shared, never copied: the task keeps the last copy until the core takes it back through C5, so it never
  // frees memory on the game thread. A null list watches nothing.
  struct SetWatchFlags {
    std::uint32_t version = 0;
    std::shared_ptr<const std::vector<std::uint32_t>> ids;

    friend bool operator==(const SetWatchFlags& a, const SetWatchFlags& b) {
      if (a.version != b.version || (a.ids == nullptr) != (b.ids == nullptr)) return false;
      return a.ids == nullptr || *a.ids == *b.ids;
    }
  };
  // The characters whose heads the render feed carries every frame (ARCH §7.4); empty stops it.
  struct SetTracked {
    FixedVector<ChrHandle, 8> handles;

    friend constexpr bool operator==(const SetTracked&, const SetTracked&) = default;
  };
  // Text look-ups in one FMG, answered in WorldSnapshot::new_text.
  struct ResolveText {
    std::uint16_t fmg = 0;
    FixedVector<std::uint32_t, 8> ids;

    friend constexpr bool operator==(const ResolveText&, const ResolveText&) = default;
  };
  // Row look-ups in one param, answered in WorldSnapshot::new_params. DESIGN §2.3's sketch has only the ids; the param
  // is here because the core asks for rows of three params (NpcParam for a character's row, and the grace and
  // place-name rows), and their ids overlap.
  struct ResolveParams {
    ParamKind param = ParamKind::NpcParam;
    FixedVector<std::uint32_t, 16> ids;

    friend constexpr bool operator==(const ResolveParams&, const ResolveParams&) = default;
  };
  struct Noop {
    friend constexpr bool operator==(const Noop&, const Noop&) = default;
  };

  // Noop comes first: a default command does nothing, and only Noop's default constructor is needed while this class
  // is still being defined (the others' default member initializers are not usable until it is complete).
  std::variant<Noop, SetWatchFlags, SetTracked, ResolveText, ResolveParams> payload;

  friend bool operator==(const GameCommand&, const GameCommand&) = default;
};

// JSON forms (game_json.cpp): exactly one of {"set_watch_flags": {...}}, {"set_tracked": {...}},
// {"resolve_text": {...}}, {"resolve_params": {...}} and {"noop": {}}. A watch list is an array of ids, or null.
void to_json(nlohmann::json& j, const GameCommand::SetWatchFlags& v);
void from_json(const nlohmann::json& j, GameCommand::SetWatchFlags& v);
void to_json(nlohmann::json& j, const GameCommand::SetTracked& v);
void from_json(const nlohmann::json& j, GameCommand::SetTracked& v);
void to_json(nlohmann::json& j, const GameCommand::ResolveText& v);
void from_json(const nlohmann::json& j, GameCommand::ResolveText& v);
void to_json(nlohmann::json& j, const GameCommand::ResolveParams& v);
void from_json(const nlohmann::json& j, GameCommand::ResolveParams& v);
void to_json(nlohmann::json& j, const GameCommand& v);
void from_json(const nlohmann::json& j, GameCommand& v);

}  // namespace en::proto
