// The rollback choice's panel (choice.hpp; MEM §9.1, O144; WHY§690).
#include "en/core/choice.hpp"

#include <algorithm>

namespace en::core {

namespace db = proto::db;

namespace {

std::string who_of(const std::string& persona, const NameOf& name_of) {
  if (persona.empty()) return "someone";
  if (name_of) {
    if (std::optional<std::string> n = name_of(persona); n && !n->empty()) return *n;
  }
  return "someone you know";
}

std::string capital(std::string s) {
  if (!s.empty() && s.front() >= 'a' && s.front() <= 'z') s.front() = static_cast<char>(s.front() - 'a' + 'A');
  return s;
}

// "2 h 14 min", "45 min", "under a minute".
std::string span_words(proto::Pt ms) {
  const std::uint64_t minutes = ms / 60000;
  if (minutes == 0) return "under a minute";
  const std::uint64_t h = minutes / 60;
  const std::uint64_t m = minutes % 60;
  if (h == 0) return std::to_string(m) + " min";
  if (m == 0) return std::to_string(h) + " h";
  return std::to_string(h) + " h " + std::to_string(m) + " min";
}

std::string plural(std::uint32_t n, std::string_view one, std::string_view many) {
  return std::to_string(n) + " " + std::string(n == 1 ? one : many);
}

}  // namespace

std::string play_time_words(proto::Pt ms) {
  const std::uint64_t minutes = ms / 60000;
  const std::uint64_t m = minutes % 60;
  return std::to_string(minutes / 60) + ":" + (m < 10 ? "0" : "") + std::to_string(m);
}

proto::ChoicePanel choice_panel(const db::RollbackSummary& s, std::uint32_t id, const NameOf& name_of) {
  using proto::ChoiceBullet;
  using proto::ChoiceTone;
  proto::ChoicePanel c;
  c.id = id;
  c.title = std::string(kChoiceTitle);
  if (s.save_journey < s.file_journey) {
    c.lead = "This save is from an earlier journey than what EldenNet remembers for this character. What should the "
             "people you have met remember?";
  } else {
    c.lead = "This save is " + span_words(s.file_pt > s.save_pt ? s.file_pt - s.save_pt : 0) +
             " older than what EldenNet remembers for this character. What should the people you have met remember?";
  }
  const auto at = [](proto::Pt pt) { return "at " + play_time_words(pt) + " of play"; };
  c.bullets.push_back(ChoiceBullet{ChoiceTone::Save, "This save: journey " + std::to_string(s.save_journey) + ", " +
                                                         play_time_words(s.save_pt) + " of play."});
  c.bullets.push_back(ChoiceBullet{ChoiceTone::Memory, "EldenNet remembers up to journey " +
                                                           std::to_string(s.file_journey) + ", " +
                                                           play_time_words(s.file_pt) + " of play."});
  for (const db::RollbackItem& item : s.last) {
    const std::string who = who_of(item.who, name_of);
    std::string text;
    switch (item.kind) {
      case db::RollbackItem::Kind::Turn: text = "A conversation with " + who + ", " + at(item.pt) + "."; break;
      case db::RollbackItem::Kind::Addressed: text = capital(who) + " spoke to you, " + at(item.pt) + "."; break;
      case db::RollbackItem::Kind::Witnessed:
        text = capital(who) + " saw something happen, " + at(item.pt) + ".";
        break;
      case db::RollbackItem::Kind::Memory: text = capital(who) + "'s memory of it, written " + at(item.pt) + "."; break;
      case db::RollbackItem::Kind::Stage: continue;  // listed with the stages below
      case db::RollbackItem::Kind::Journey: text = "A later journey, begun " + at(item.pt) + "."; break;
    }
    c.bullets.push_back(ChoiceBullet{ChoiceTone::Undone, std::move(text)});
  }
  for (const db::StageUndone& st : s.stages_undone) {
    c.bullets.push_back(ChoiceBullet{ChoiceTone::Stage, capital(who_of(st.persona, name_of)) +
                                                            "'s story would go back to where it stood before " +
                                                            play_time_words(st.pt) + " of play."});
  }
  std::vector<std::string> parts;
  if (s.turns != 0) parts.push_back(plural(s.turns, "line of conversation", "lines of conversation"));
  if (s.addressed != 0) parts.push_back(plural(s.addressed, "line spoken to you", "lines spoken to you"));
  if (s.witnessed != 0) parts.push_back(plural(s.witnessed, "thing seen", "things seen"));
  if (s.memories != 0) parts.push_back(plural(s.memories, "memory", "memories"));
  if (s.stages != 0) parts.push_back(plural(s.stages, "quest stage", "quest stages"));
  if (s.journeys != 0) parts.push_back(plural(s.journeys, "later journey", "later journeys"));
  if (!parts.empty()) {
    std::string all = "After this save, in all: ";
    for (std::size_t i = 0; i < parts.size(); ++i) {
      if (i != 0) all += i + 1 == parts.size() ? " and " : ", ";
      all += parts[i];
    }
    c.bullets.push_back(ChoiceBullet{ChoiceTone::Note, all + "."});
  }
  c.bullets.push_back(ChoiceBullet{ChoiceTone::Note, "Forget: what came after this save is forgotten, as if it had "
                                                     "not happened yet. Keep: everything stays, and this save goes on "
                                                     "from here. New character: EldenNet starts afresh for this save "
                                                     "and keeps the old memories aside."});
  c.answers = {std::string(kChoiceForget), std::string(kChoiceKeep), std::string(kChoiceNew)};
  return c;
}

}  // namespace en::core
