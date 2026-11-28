// What a persona may know about an event, and the store's form of an audience (audience.hpp; MEM §1.3).
#include "en/augur/audience.hpp"

namespace en::augur {

namespace {

bool reserved(std::string_view kind) noexcept {
  return kind == "NpcThought" || kind == "DecisionMade" || kind == "NpcIntent";
}

}  // namespace

std::optional<Visible<proto::StoredEvent>> visible_to(const proto::StoredEvent& ev, std::string_view persona,
                                                      const VisCtx& ctx) noexcept {
  if (ev.journey != ctx.journey || reserved(ev.kind) || persona.empty()) return std::nullopt;
  for (const proto::AudienceRow& a : ev.audience) {
    if (a.persona == persona) return Visible<proto::StoredEvent>(ev, a.how);
  }
  return std::nullopt;
}

std::optional<Visible<Event>> visible_to(const Event& ev, PersonaId persona, const VisCtx& ctx) noexcept {
  if (static_cast<std::uint32_t>(ev.journey) + 1 != ctx.journey) return std::nullopt;
  if (std::holds_alternative<NpcThought>(ev.kind) || std::holds_alternative<DecisionMade>(ev.kind)) {
    return std::nullopt;
  }
  for (const AudienceEntry& a : ev.audience) {
    if (a.persona == persona) return Visible<Event>(ev, a.how);
  }
  return std::nullopt;
}

std::vector<proto::AudienceRow> stored_audience(const Event& ev, const PersonaNamer& id_of) {
  std::vector<proto::AudienceRow> out;
  out.reserve(ev.audience.size());
  for (const AudienceEntry& a : ev.audience) {
    if (auto id = id_of(a.persona)) out.push_back(proto::AudienceRow{std::move(*id), a.how});
  }
  return out;
}

void freeze_into(proto::StoredEvent& out, const Event& ev, const PersonaNamer& id_of) {
  out.audience = stored_audience(ev, id_of);
}

std::vector<std::pair<std::string, How>> perceivers(const proto::StoredEvent& ev, const VisCtx& ctx) {
  std::vector<std::pair<std::string, How>> out;
  if (ev.journey != ctx.journey || reserved(ev.kind)) return out;
  out.reserve(ev.audience.size());
  for (const proto::AudienceRow& a : ev.audience) {
    if (!a.persona.empty()) out.emplace_back(a.persona, a.how);
  }
  return out;
}

std::size_t audience_size(const Event& ev) noexcept { return ev.audience.size(); }

}  // namespace en::augur
