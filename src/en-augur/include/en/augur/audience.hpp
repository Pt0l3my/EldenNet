#pragma once
// The one decision of what a persona may know about an event (MEM §1.3; ARCH §8.8; WHY§99), whether the event is in
// RAM (Augur's `Event`) or stored (en-proto's `StoredEvent`), and the store's form of a frozen audience. Audiences are
// Augur's: nothing outside en-augur reads an event's audience rows (a CI grep, since C++ has no module-private
// members); the core reads an event only through `visible_to`, and writes its audience through `stored_audience`.
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/augur/events.hpp"
#include "en/proto/store.hpp"

namespace en::augur {

// What the reader knows: the journey in view, as the store counts it (ng_lvl + 1, MEM §0).
struct VisCtx {
  std::uint16_t journey = 1;
};

// A view of an event as one persona perceived it: valid while the event lives. `visible_to` is its only maker.
template <class E>
class Visible {
 public:
  [[nodiscard]] const E& ev() const noexcept { return *ev_; }
  How how = How::Witnessed;

 private:
  Visible(const E& ev, How h) noexcept : how(h), ev_(&ev) {}
  friend std::optional<Visible<proto::StoredEvent>> visible_to(const proto::StoredEvent&, std::string_view,
                                                               const VisCtx&) noexcept;
  friend std::optional<Visible<Event>> visible_to(const Event&, PersonaId, const VisCtx&) noexcept;
  const E* ev_;
};

// MEM §1.3's rules, in order: the event is of the journey in view, else nothing (earlier journeys are never visible);
// the persona's own frozen row gives `how`, and no row gives nothing; the reserved kinds (NpcThought, DecisionMade,
// intents) give nothing in v1. There is no positional check, no recomputation and no line of sight. A stored event's
// persona is its profile id (the alias table's renames come with it, NPC §2.4); a RAM event's is the loaded content's
// index, and its journey Augur's ng_lvl.
[[nodiscard]] std::optional<Visible<proto::StoredEvent>> visible_to(const proto::StoredEvent& ev,
                                                                    std::string_view persona,
                                                                    const VisCtx& ctx) noexcept;
[[nodiscard]] std::optional<Visible<Event>> visible_to(const Event& ev, PersonaId persona, const VisCtx& ctx) noexcept;

// An event's frozen audience as the store keeps it: each row's persona by its profile id (`id_of` names a content
// index; a row it cannot name is left out), with its `how`, nearest first as Augur froze them.
using PersonaNamer = std::function<std::optional<std::string>(PersonaId)>;
[[nodiscard]] std::vector<proto::AudienceRow> stored_audience(const Event& ev, const PersonaNamer& id_of);

// The same, written into the stored event (`out`'s audience), so that the core writes an audience without naming the
// field (MEM §1.3's grep).
void freeze_into(proto::StoredEvent& out, const Event& ev, const PersonaNamer& id_of);

// Each row of a stored event as `visible_to` would answer it for its persona, in the event's order. For the facts' fold
// alone (MEM §2.2): live, as an event is written, and in recomputation, over the journey's events (EventsForFold). The
// fold moves each perceiver's own facts by its own `how` and keeps nothing of who else perceived the event.
[[nodiscard]] std::vector<std::pair<std::string, How>> perceivers(const proto::StoredEvent& ev, const VisCtx& ctx);

// The audience's size, for the F11 view's counts: how many perceived it, never who.
[[nodiscard]] std::size_t audience_size(const Event& ev) noexcept;

}  // namespace en::augur
