// The tester's task list: see tasks.hpp.
#include "en/launcher/tasks.hpp"

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
  for (std::size_t i = 0; i < tasks_.size(); ++i) {
    const Task& t = tasks_[i];
    TaskRow r;
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

std::vector<Task> read_tasks() {
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
