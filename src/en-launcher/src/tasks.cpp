// The tester's task list: see tasks.hpp.
#include "en/launcher/tasks.hpp"

#include <utility>

namespace en::launcher {

TaskList::TaskList(std::vector<Task> tasks) : tasks_(std::move(tasks)), took_s_(tasks_.size(), 0) {}

std::uint32_t TaskList::length(const Task& t) const noexcept {
  if (t.kind == TaskKind::Round) return 0;
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
  if (t.kind == TaskKind::Round || wait == 0) return {};
  if (t.kind == TaskKind::Do && pace_s_ == 0) return {};  // a Do task waits for the tester's key
  if (now_ms - began_ms_ < std::uint64_t{wait} * 1000) return {};
  return next(now_ms, t.kind == TaskKind::Do && t.mark);
}

TaskList::Change TaskList::confirm(std::uint64_t now_ms) noexcept {
  if (!started_ || finished() || tasks_[current_].kind != TaskKind::Do) return {};
  return next(now_ms, tasks_[current_].mark);
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
  using K = TaskKind;
  return {
      {K::Round, "Wait at the title screen",
       "EldenNet is reading the game. Do not load your save yet: this row turns grey when it is time.", 0, false},
      {K::Do, "Load your save", "When you stand in the world, press Pause.", 0, false},
      {K::Wait, "Walk about",
       "Go near an NPC or an enemy, and open the map once. Leave items, doors and graces alone for now.", 180, false},
      {K::Do, "Do one thing the game remembers",
       "Take an item from a body or a chest, open a door or a chest, discover a grace, or buy something a merchant "
       "has only a few of. Then press Pause at once.",
       0, true},
      {K::Wait, "Walk about", "Leave items, doors and graces alone until the time is up.", 90, false},
      {K::Do, "Do a second such thing",
       "Another item, door, chest or purchase, then press Pause at once. If nothing is at hand, just press Pause.", 0,
       true},
      {K::Wait, "Walk about", "Leave items, doors and graces alone until the time is up.", 90, false},
      {K::Do, "Quit the game", "The round is done. Quit as you usually do.", 0, false},
  };
}

}  // namespace en::launcher
