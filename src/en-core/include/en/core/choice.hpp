#pragma once
// The rollback choice's panel (MEM §9.1, O144; WHY§690): what the core shows while the store waits for the player's
// answer, made from the store's summary (ids and numbers, never chat text) in our own words. The words are drafts for
// the owner (src/en-core/README.md lists them).
//
// - The save's play time and journey against what EldenNet's memory holds.
// - The things that matter after the cut, newest first: a conversation, a line addressed to the Tarnished, something
//   a profiled NPC saw, a memory the memory job wrote, a later journey; each with who (a persona's name as the content
//   gives it, or "someone you know"), and when (in play time).
// - The quest stages the cut would undo, by whose story they are, never a stage's id or line (SP1, SP5).
// - The counts in one line, and the three answers.
//
// There is never an answer of EldenNet's own: the panel stays until the player answers, and one closed without an
// answer (danger, a load, the player's exit) is asked again at the next safe moment (core.cpp).
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "en/proto/store.hpp"
#include "en/proto/ui.hpp"

namespace en::core {

// A persona's name for the panel, by its profile id: none where the content has no such persona.
using NameOf = std::function<std::optional<std::string>(std::string_view persona)>;

// Play time as the panel writes it: "0:42" (hours and minutes), "12:05".
[[nodiscard]] std::string play_time_words(proto::Pt ms);
[[nodiscard]] proto::ChoicePanel choice_panel(const proto::db::RollbackSummary& s, std::uint32_t id,
                                              const NameOf& name_of);

inline constexpr std::string_view kChoiceTitle = "An older save";
inline constexpr std::string_view kChoiceLead =
    "You loaded a save from earlier than what EldenNet remembers. What should it do with what came after?";
inline constexpr std::string_view kChoiceForget = "Forget what came after";
inline constexpr std::string_view kChoiceKeep = "Keep everything";
inline constexpr std::string_view kChoiceNew = "This is a new character";
// How long a panel closed without an answer waits before it is asked again, at the next safe moment after it.
inline constexpr std::uint64_t kChoiceAgainMs = 10000;

}  // namespace en::core
