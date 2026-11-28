// The tester's task list: see tasks.hpp.
#include "en/launcher/tasks.hpp"

#include "en/proto/watch.hpp"

#include <utility>

namespace en::launcher {

TaskList::TaskList(std::vector<Task> tasks) : tasks_(std::move(tasks)), took_s_(tasks_.size(), 0) {}

std::uint32_t TaskList::length(const Task& t) const noexcept {
  if (t.kind == TaskKind::Round || t.kind == TaskKind::End) return 0;
  if (pace_s_ != 0) return pace_s_;
  return t.kind == TaskKind::Wait ? t.seconds : 0;
}

void TaskList::start(std::uint64_t now_ms) noexcept {
  if (started_) return;
  started_ = true;
  current_ = 0;
  began_ms_ = now_ms;
}

TaskList::Change TaskList::next(std::uint64_t now_ms, bool mark) noexcept {
  Change c;
  c.changed = true;
  c.mark = mark;
  c.task = current_;
  took_s_[current_] = static_cast<std::uint32_t>((now_ms - began_ms_) / 1000);
  ++current_;
  began_ms_ = now_ms;
  return c;
}

TaskList::Change TaskList::round_step(std::uint64_t now_ms) noexcept {
  if (!started_ || finished() || tasks_[current_].kind != TaskKind::Round) return {};
  return next(now_ms, false);
}

TaskList::Change TaskList::tick(std::uint64_t now_ms) noexcept {
  if (!started_ || finished()) return {};
  const Task& t = tasks_[current_];
  const std::uint32_t wait = length(t);
  if (t.kind == TaskKind::Round || t.kind == TaskKind::End || wait == 0) return {};
  if (t.kind == TaskKind::Do && pace_s_ == 0) return {};  // a Do task waits for the tester's key
  if (now_ms - began_ms_ < std::uint64_t{wait} * 1000) return {};
  return next(now_ms, t.mark);  // a marked wait stamps its Mark when its time is up: the round's next phase begins
}

TaskList::Change TaskList::confirm(std::uint64_t now_ms) noexcept {
  if (!started_ || finished() || tasks_[current_].kind != TaskKind::Do) return {};
  return next(now_ms, tasks_[current_].mark);
}

TaskList::Change TaskList::end(std::uint64_t now_ms) noexcept {
  if (!started_ || finished() || tasks_[current_].kind != TaskKind::End) return {};
  return next(now_ms, false);
}

std::vector<TaskRow> TaskList::rows(std::uint64_t now_ms) const {
  std::vector<TaskRow> out;
  out.reserve(tasks_.size());
  std::uint32_t marks = 0;
  for (std::size_t i = 0; i < tasks_.size(); ++i) {
    const Task& t = tasks_[i];
    TaskRow r;
    if (t.mark && (t.kind == TaskKind::Do || t.kind == TaskKind::Wait)) r.mark = ++marks;
    r.title = t.title;
    r.detail = t.detail;
    r.kind = t.kind;
    r.seconds = t.kind == TaskKind::Wait ? length(t) : 0;
    if (t.kind == TaskKind::Do) r.button = t.button.empty() ? std::string("Done") : t.button;
    if (!started_ || i > current_) {
      r.state = TaskState::Later;
    } else if (i < current_) {
      r.state = TaskState::Done;
      r.elapsed_s = took_s_[i];
      r.gathered = proto::watch::gathered_of(gathered_, r.mark);
    } else {
      r.state = TaskState::Now;
      r.elapsed_s = static_cast<std::uint32_t>((now_ms - began_ms_) / 1000);
    }
    out.push_back(std::move(r));
  }
  return out;
}

std::string clock_text(std::uint32_t seconds) {
  std::string out = std::to_string(seconds / 60) + ":";
  const std::uint32_t s = seconds % 60;
  if (s < 10) out += "0";
  return out + std::to_string(s);
}

std::string button_text(const TaskRow& row, bool key_works) {
  if (row.state != TaskState::Now || row.kind != TaskKind::Do) return {};
  return (row.button.empty() ? std::string("Done") : row.button) +
         (key_works ? ": press Pause, or click here" : ": click here");
}

std::string timer_text(const TaskRow& row) {
  switch (row.state) {
    case TaskState::Done:
      return clock_text(row.elapsed_s);
    case TaskState::Now:
      if (row.kind == TaskKind::Wait) {
        return clock_text(row.seconds > row.elapsed_s ? row.seconds - row.elapsed_s : 0) + " left";
      }
      return clock_text(row.elapsed_s);
    case TaskState::Later:
      return row.kind == TaskKind::Wait ? clock_text(row.seconds) : std::string();
  }
  return {};
}

std::string gathered_text(const TaskRow& row) {
  if (row.state != TaskState::Done) return {};
  if (row.gathered == proto::watch::kGathered) return "Got it.";
  if (row.gathered == proto::watch::kNotGathered) return "Not gathered.";
  return {};
}

std::string gathered_footer(const std::vector<TaskRow>& rows, bool report_failed) {
  if (report_failed) {
    return "The round cannot write its report: the files will not hold this run. Finish or quit as you like, send "
           "the two logs, and tell us.";
  }
  std::vector<std::size_t> missed;
  std::size_t judged = 0;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].mark == 0) continue;
    if (rows[i].state != TaskState::Done || rows[i].gathered == proto::watch::kGatherUntold) return {};
    ++judged;
    if (rows[i].gathered == proto::watch::kNotGathered) missed.push_back(i + 1);
  }
  if (judged == 0) return {};
  if (missed.empty()) return "This run is usable: send it.";
  std::string which = missed.size() == 1 ? "Task " : "Tasks ";
  for (std::size_t k = 0; k < missed.size(); ++k) {
    if (k != 0) which += k + 1 == missed.size() ? " and " : ", ";
    which += std::to_string(missed[k]);
  }
  return which + (missed.size() == 1 ? " was" : " were") +
         " not gathered: send the files all the same; run again only if you have the time.";
}

namespace {

// The eighteenth read-only round, the character survey (the owner agreed to its reading and its tasks on
// 2026-11-24, O117 and O118): one character, near a few weak enemies. Standing, then looking around so that the
// characters near pass on and off the screen, then one of them killed. The waits are marked, so that the round
// knows when each ends; the load and the kill are the tester's to confirm. Five marks.
std::vector<Task> survey_tasks() {
  using K = TaskKind;
  return {
      {K::Round, "Wait at the title screen",
       "EldenNet is reading the game. Do not load a save yet: this row turns grey when it is time.", 0, false, ""},
      {K::Do, "Load your usual character",
       "Go to a place with a few weak enemies in sight, at a safe distance. When you stand there, press Pause, "
       "and let go of the controls.",
       0, true, "I stand near enemies"},
      {K::Wait, "Stand still", "Hands off the keyboard and the mouse: do not move or turn the camera.", 40, true, ""},
      {K::Wait, "Look around slowly",
       "Turn the camera all the way round, slowly, two or three times, so that the enemies pass on and off the "
       "screen. Do not walk.",
       40, true, ""},
      {K::Do, "Kill one weak enemy", "Any one will do. When it is dead, press Pause, and stay near it.", 0, true,
       "It is dead"},
      {K::Wait, "Stand still again", "Stay near the one you killed. Hands off the keyboard and the mouse.", 20,
       true, ""},
      {K::End, "Quit the game", "Quit as you usually do. The round's files are checked when the game has ended.", 0,
       false, ""},
  };
}

// The sheet's plan (the twentieth read-only round; the owner agreed to its tasks, 2026-11-24): one character,
// somewhere quiet and no standing near enemies. A short stand, then a ride or a run in one direction (the position
// moving, and the block as the tester crosses into another), then the tester's own death (a count that rises by
// one, and how the game marks the main player's death), and a short stand once they are back. Five marks.
std::vector<Task> sheet_tasks() {
  using K = TaskKind;
  return {
      {K::Round, "Wait at the title screen",
       "EldenNet is reading the game. Do not load a save yet: this row turns grey when it is time.", 0, false, ""},
      {K::Do, "Load your usual character",
       "Go somewhere quiet, with no enemy near, and get off your horse. When you stand there, press Pause, and "
       "let go of the controls.",
       0, true, "I stand somewhere quiet"},
      {K::Wait, "Stand still", "On foot. Hands off the keyboard and the mouse until the time is up.", 20, true, ""},
      {K::Wait, "Ride or run in one direction",
       "Mount if you like, and keep going one way until the time is up: the further the better. Go round enemies "
       "rather than through them.",
       60, true, ""},
      {K::Do, "Let an enemy kill you, once",
       "Find any enemy and let it kill you. Wait until you are back in the world and can move, then press Pause. "
       "If you died sooner than you meant to, that counts too: carry on.",
       0, true, "I died and I am back"},
      {K::Wait, "Stand still again", "Where you came back. Hands off the keyboard and the mouse.", 20, true, ""},
      {K::End, "Quit the game",
       "Quit as you usually do. Whatever happened on the way, send the files: the round's own line below says "
       "what it gathered.",
       0, false, ""},
  };
}

// The stay's plan (the twenty-first read-only round): one character, somewhere quiet. A short stand; the tester's
// own death (the count of deaths by its rise, and how the game marks the main player's death); then four minutes in
// the world as the tester likes, for the watch to see the clock go round beside EldenNet's own reader. Four marks.
std::vector<Task> stay_tasks() {
  using K = TaskKind;
  return {
      {K::Round, "Wait at the title screen",
       "EldenNet is reading the game. Do not load a save yet: this row turns grey when it is time.", 0, false, ""},
      {K::Do, "Load your usual character",
       "Go somewhere quiet, with no enemy near, and get off your horse. When you stand there, press Pause, and "
       "let go of the controls.",
       0, true, "I stand somewhere quiet"},
      {K::Wait, "Stand still", "On foot. Hands off the keyboard and the mouse until the time is up.", 20, true, ""},
      {K::Do, "Let an enemy kill you, once",
       "Find any enemy and let it kill you. Wait until you are back in the world and can move, then press Pause. "
       "If you died sooner than you meant to, that counts too: carry on.",
       0, true, "I died and I am back"},
      {K::Wait, "Stay in the world",
       "Play as you like, but stay in the world: no quitting, no long stop at a grace. Now and then sprint or "
       "roll, and use a spell or a skill: the round looks for focus and stamina.",
       240, true, ""},
      {K::End, "Quit the game",
       "Quit as you usually do. Whatever happened on the way, send the files: the round's own line below says "
       "what it gathered.",
       0, false, ""},
  };
}

// The twenty-second read-only round (plan 4, the tour; the owner agreed to its tasks on 2026-11-28, O151). Twelve
// marks, each a change of state the round's scan by phases sets against the others: the load at a grace; the
// game's menu open and closed; the end of the sprints and the spell (the reader's focus and stamina); mounted and on
// foot; another grace and back (two loading screens, and the place); locked on and released; the tester's own
// death, once; two minutes in the world. The four keys (T, Tab, V, B) are the engine's question of what the game
// does with them (S11.1.a): the tester presses them on foot and with the menu open, and says what happened.
std::vector<Task> tour_tasks() {
  using K = TaskKind;
  return {
      {K::Round, "Wait at the title screen",
       "EldenNet is reading the game. Do not load a save yet: this row turns grey when it is time.", 0, false, ""},
      {K::Do, "Load your usual character",
       "Go to a quiet grace of your choice and stand beside it, on foot. Do not sit down. When you stand there, "
       "press Pause, and let go of the controls.",
       0, true, "I stand at the grace"},
      {K::Wait, "Stand still", "On foot. Hands off the keyboard and the mouse until the time is up.", 20, false, ""},
      {K::Wait, "Press T, Tab, V and B",
       "Press each key three times, slowly: T, then Tab, then V, then B. Watch whether the game does anything at "
       "each. You tell us afterwards.",
       25, false, ""},
      {K::Do, "Open the game's menu",
       "Open the main menu (the one with Equipment and Status) and leave it open. Then press Pause.", 0, true,
       "The menu is open"},
      {K::Wait, "Keep the menu open",
       "With the menu open, press T, Tab, V and B again, three times each. Watch what each does. Leave the menu "
       "open.",
       25, false, ""},
      {K::Do, "Close the menu", "Close the game's menu, so that you stand in the world again. Then press Pause.", 0,
       true, "The menu is closed"},
      {K::Wait, "Sprint, roll and cast",
       "Sprint and roll, and use a spell or a weapon skill a few times, until the time is up.", 30, true, ""},
      {K::Do, "Mount your horse", "Call your horse and get on it. Then press Pause.", 0, true, "I am on my horse"},
      {K::Wait, "Stay mounted", "Ride about or stand, but stay on the horse until the time is up.", 15, false, ""},
      {K::Do, "Dismount", "Get off the horse. When you stand on foot, press Pause.", 0, true, "I am on foot"},
      {K::Do, "Travel to another grace",
       "Open the map and travel to any other grace. When you stand there after the loading screen, press Pause.",
       0, true, "I have arrived"},
      {K::Wait, "Stand still", "Stay where you arrived until the time is up.", 15, false, ""},
      {K::Do, "Travel back",
       "Travel back to the grace you started from. When you stand there after the loading screen, press Pause.", 0,
       true, "I am back"},
      {K::Wait, "Stand still", "Stay where you arrived until the time is up.", 15, false, ""},
      {K::Do, "Lock on to an enemy",
       "Find any enemy and lock on to it. Press Pause while you are locked on.", 0, true, "I am locked on"},
      {K::Wait, "Stay locked on",
       "Keep the lock until the time is up. If you die meanwhile, carry on with the tasks as they come.", 10, false,
       ""},
      {K::Do, "Release the lock", "Let go of the lock. Then press Pause.", 0, true, "The lock is released"},
      {K::Do, "Let an enemy kill you, once",
       "Let it kill you. Wait until you are back in the world and can move, then press Pause. After this, try not "
       "to die again.",
       0, true, "I died and I am back"},
      {K::Wait, "Stay in the world",
       "Play as you like, but stay in the world and try not to die: no quitting, no long stop at a grace.", 120,
       true, ""},
      {K::End, "Quit the game",
       "Quit as you usually do. Whatever happened on the way, send the files: the round's own line below says "
       "what it gathered.",
       0, false, ""},
  };
}

}  // namespace

std::vector<Task> read_tasks(std::uint32_t plan) {
  if (plan == 1) return survey_tasks();
  if (plan == 2) return sheet_tasks();
  if (plan == 3) return stay_tasks();
  if (plan == 4) return tour_tasks();
  // The fourteenth read-only round. First the save's slot, by the two numbers it must read: the last character of
  // the tester's list is loaded, then the usual one (the owner agreed, 2026-11-23, and the owner's save file gave
  // their slots). Then, with the usual character, the parts watch's phases: standing, walking, standing, one hit,
  // one heal. The waits of those phases are marked, so that the round knows when each ends; the hit and the heal
  // are the tester's to confirm.
  using K = TaskKind;
  return {
      {K::Round, "Wait at the title screen",
       "EldenNet is reading the game. Do not load a save yet: this row turns grey when it is time.", 0, false, ""},
      {K::Wait, "Stay at the title screen", "EldenNet takes its first look at the title. Do not load a save yet.",
       30, false, ""},
      {K::Do, "Load the last character in your list",
       "Choose Load Game, not Continue, and take the character at the bottom of the list. When you stand in the "
       "world, press Pause.",
       0, true, "I am in the world"},
      {K::Wait, "Stay in the world", "Anything will do. Stay in the world until the time is up.", 45, false, ""},
      {K::Do, "Quit to the title screen",
       "Use the game's own menu to quit to the title screen, not out of the game. When the title screen shows, "
       "press Pause.",
       0, true, "The title screen shows"},
      {K::Wait, "Stay at the title screen", "Do not load a save until the time is up.", 20, false, ""},
      {K::Do, "Load your usual character",
       "Choose Load Game and take the character you test with. When you stand in the world, press Pause, and "
       "then let go of the controls.",
       0, true, "I am in the world again"},
      {K::Wait, "Stand still", "Hands off the controls until the time is up: do not move, turn or open a menu.", 90,
       true, ""},
      {K::Wait, "Walk about", "Keep walking until the time is up, on foot: no riding, no fighting.", 30, true, ""},
      {K::Wait, "Stand still again", "Hands off the controls until the time is up.", 20, true, ""},
      {K::Do, "Take one hit",
       "Let an enemy hit you once, or take a small fall. When you have lost some health, press Pause. Do not heal "
       "yet.",
       0, true, "I have been hit"},
      {K::Do, "Heal", "Drink a flask, or heal another way. When your health is back up, press Pause.", 0, true,
       "I have healed"},
      {K::Wait, "Stand still", "A few seconds more, hands off the controls.", 10, false, ""},
      {K::End, "Quit the game", "The round is done: there is nothing more to press. Quit as you usually do.", 0, false,
       ""},
  };
}

}  // namespace en::launcher
