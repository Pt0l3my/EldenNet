// The commits and v0.2's event log in memory (event_log.hpp; NPC §8.7; WHY§364 (7)).
#include "en/core/event_log.hpp"

#include <algorithm>
#include <array>

namespace en::core {

const char* to_string(EndReason r) noexcept {
  constexpr std::array<const char*, 12> kNames = {"closed",      "walked_away", "danger",   "travel",
                                                  "death",       "dormant",     "npc_state", "native_talk",
                                                  "idle",        "retarget",    "degraded", "fault"};
  const auto i = static_cast<std::size_t>(r);
  return i < kNames.size() ? kNames[i] : "fault";
}

void MemoryLog::apply(const Commit& c) {
  if (c.kind == Commit::Kind::NpcPage || c.kind == Commit::Kind::CutOff) {
    // Grows (or marks) the reply's NpcSaid; a page whose NpcSaid has left the ring is lost with it.
    for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
      if (it->kind != Commit::Kind::NpcSaid || it->conv != c.conv || it->reply != c.reply) continue;
      if (c.kind == Commit::Kind::CutOff) {
        it->cut_off = true;
      } else {
        if (!it->text.empty() && !c.text.empty()) it->text += ' ';
        it->text += c.text;
        ++it->pages;
      }
      return;
    }
    return;
  }
  LoggedEvent e;
  e.id = next_++;
  e.kind = c.kind;
  e.conv = c.conv;
  e.reply = c.reply;
  e.persona = c.persona;
  e.text = c.text;
  e.pages = c.kind == Commit::Kind::NpcSaid ? 1 : 0;
  e.reason = c.reason;
  e.player_lines = c.player_lines;
  e.committed = c.committed;
  e.at = c.at;
  events_.push_back(std::move(e));
  if (events_.size() > kCapacity) {
    events_.pop_front();
    ++dropped_;
  }
}

std::vector<LoggedEvent> MemoryLog::last_lines(std::string_view persona, proto::ConvId before, std::size_t n) const {
  // The newest conversation with the persona before `before`, and its last n lines.
  proto::ConvId last = 0;
  for (const LoggedEvent& e : events_) {
    if (e.persona == persona && e.conv != before && e.conv < before && e.conv > last) last = e.conv;
  }
  std::vector<LoggedEvent> out;
  if (last == 0) return out;
  for (const LoggedEvent& e : events_) {
    if (e.conv == last && (e.kind == Commit::Kind::PlayerSaid || e.kind == Commit::Kind::NpcSaid)) out.push_back(e);
  }
  if (out.size() > n) out.erase(out.begin(), out.end() - static_cast<std::ptrdiff_t>(n));
  return out;
}

}  // namespace en::core
