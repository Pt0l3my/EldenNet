#pragma once
// Tools › Workshop's keeping (PAGES §4.6.8, SP8; en-proto's workshop.hpp), portable and tested on Linux: what the DLL
// keeps on T6 while Developer views are on, and what it builds from the core when a Workshop topic is watched. The DLL
// (win/workshop.cpp) feeds it each request it submits and each event of the model client (ai.cpp), each of Augur's
// events of a step (shell.cpp) and each snapshot, and empties it the moment Developer views go off. Nothing here is
// written anywhere: it is gone at the next start.
//
// Every text it keeps has gone through the DLL's scrub first (each configured key's value, anything shaped like a key,
// and the playthrough's name, matched whole: PV2), then through its bounds.
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "en/augur/events.hpp"
#include "en/core/core.hpp"
#include "en/core/identity.hpp"
#include "en/proto/calls.hpp"
#include "en/proto/llm.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/workshop.hpp"

namespace en::shell {

// What a text goes through before Workshop keeps it (the DLL's: keys, credential shapes, the character's name).
using WorkshopScrub = std::function<std::string(std::string_view)>;

// The playthrough's name, matched whole and in either case of its ASCII letters, replaced by kNameMask (PV2): a match
// counts only where neither its neighbour before nor after is a letter, a digit or a byte of a multi-byte character.
inline constexpr std::string_view kNameMask = "[the Tarnished's name]";
[[nodiscard]] std::string mask_name(std::string_view text, std::string_view name);

// An unknown character's row for identity/personas.toml (NPC §2.1, en-core's index schema), ids and numbers only, never
// a name: its id and record file to rename and write, its NpcParam row, and its entity id when it has one.
[[nodiscard]] std::string unknown_toml(std::uint32_t param, std::uint32_t entity);

// The session's names for the game's character handles (h1, h2, ... in order of first appearance, as the golden files
// name them), at most kWorkshopHandles; past them "h?". Forgotten with the rest.
inline constexpr std::size_t kWorkshopHandles = 4096;

class Workshop {
 public:
  void set_scrub(WorkshopScrub scrub) { scrub_ = std::move(scrub); }
  // Everything kept, gone (Developer views turned off, or a new playthrough's handles).
  void clear();

  // T6: a chat request the DLL submits, the conversation's or the profile editor's: the newest exchange from now on.
  void sent(const proto::LlmRequest& request, proto::CallPurpose purpose, std::uint64_t at_ms);
  // T6: an event of the model client's; one of another request than the newest exchange's is not taken.
  void event(const proto::LlmEvent& event, std::uint64_t at_ms);
  // T6: one of Augur's events of a step, as the events stream's row; counted.
  [[nodiscard]] proto::WorkshopEvent event_row(const augur::Event& ev, std::uint64_t t_ms);
  // T6: the bus's drops this session (en-augur's BusStats).
  void drops(std::uint64_t t, std::uint64_t k) noexcept;
  // T6: a snapshot's characters that bind no persona, kept by NpcParam row and entity id.
  void see(const proto::WorldSnapshot& snapshot, const core::IdentityIndex& index, std::uint64_t t_ms);

  [[nodiscard]] const proto::WorkshopView& view() const noexcept { return view_; }
  [[nodiscard]] proto::WorkshopView& view() noexcept { return view_; }

  // Built from the core when their topic is watched.
  [[nodiscard]] static proto::WorkshopWorld world_of(const core::Core& core);
  [[nodiscard]] static proto::WorkshopKnownSets known_of(const core::Core& core);
  [[nodiscard]] static proto::WorkshopKinds kinds_of(const core::Core& core);

 private:
  [[nodiscard]] std::string scrubbed(std::string_view text) const;
  [[nodiscard]] std::string name_of(proto::ChrHandle h);

  WorkshopScrub scrub_;
  proto::WorkshopView view_;
  std::map<std::uint64_t, std::uint32_t> handles_;
  std::string stream_;  // the newest exchange's reply as it streams, before its scrub (kWorkshopReplyMax at most)
};

}  // namespace en::shell
