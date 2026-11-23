// The tester's task list (tasks.hpp): the order, the timers, the key, the Marks.
#include <doctest.h>

#include "en/launcher/tasks.hpp"

using namespace en::launcher;

namespace {
std::vector<Task> sample() {
  return {
      {TaskKind::Round, "wait", "the round reads", 0, false, ""},
      {TaskKind::Do, "load", "press the key in the world", 0, false, ""},
      {TaskKind::Wait, "walk", "", 30, false, ""},
      {TaskKind::Do, "act", "then press the key", 0, true, "I have done it"},
      {TaskKind::End, "quit", "", 0, false, ""},
  };
}
}  // namespace

TEST_CASE("tasks: nothing is under way before the start, and the key does nothing then") {
  TaskList list(sample());
  CHECK_FALSE(list.started());
  CHECK_FALSE(list.finished());
  CHECK_FALSE(list.confirm(1000).changed);
  CHECK_FALSE(list.tick(1000).changed);
  CHECK_FALSE(list.round_step(1000).changed);
  const auto rows = list.rows(1000);
  REQUIRE(rows.size() == 5);
  for (const TaskRow& r : rows) CHECK(r.state == TaskState::Later);
}

TEST_CASE("tasks: each kind of task is done its own way, in order, and only a marked task asks for a Mark") {
  TaskList list(sample());
  list.start(10'000);
  CHECK(list.started());
  CHECK(list.current() == 0);
  // The round's own task: the key and the clock do nothing; the round's step does.
  CHECK_FALSE(list.confirm(11'000).changed);
  CHECK_FALSE(list.tick(500'000).changed);
  auto c = list.round_step(70'000);
  CHECK(c.changed);
  CHECK_FALSE(c.mark);
  CHECK(c.task == 0);
  CHECK(list.current() == 1);
  // The tester's task: the clock and the round do nothing; the key does. No Mark for this one.
  CHECK_FALSE(list.tick(900'000).changed);
  CHECK_FALSE(list.round_step(71'000).changed);
  c = list.confirm(95'000);
  CHECK(c.changed);
  CHECK_FALSE(c.mark);
  // The wait: the key does nothing; it is done when its 30 s are up, not before.
  CHECK_FALSE(list.confirm(100'000).changed);
  CHECK_FALSE(list.tick(124'999).changed);
  c = list.tick(125'000);
  CHECK(c.changed);
  CHECK_FALSE(c.mark);
  CHECK(list.current() == 3);
  // The marked task: its confirmation asks for a Mark.
  c = list.confirm(140'000);
  CHECK(c.changed);
  CHECK(c.mark);
  CHECK(c.task == 3);
  CHECK_FALSE(list.finished());
  // The last task ends the round itself: neither the key, the clock nor the round's step does it; the watch's end
  // does.
  CHECK_FALSE(list.confirm(150'000).changed);
  CHECK_FALSE(list.tick(900'000).changed);
  CHECK_FALSE(list.round_step(150'000).changed);
  CHECK(list.current() == 4);
  c = list.end(160'000);
  CHECK(c.changed);
  CHECK_FALSE(c.mark);
  CHECK(list.finished());
  CHECK_FALSE(list.end(161'000).changed);
  CHECK_FALSE(list.confirm(161'000).changed);
}

TEST_CASE("tasks: the panel's button speaks for the task under way, and only a task of the tester's has one") {
  TaskList list(sample());
  list.start(0);
  // The round's own task: no button.
  CHECK(button_text(list.rows(1'000)[0], true).empty());
  (void)list.round_step(10'000);
  // A task of the tester's with no word of its own: "Done". Without the key, only the click.
  CHECK(button_text(list.rows(11'000)[1], true) == "Done: press Pause, or click here");
  CHECK(button_text(list.rows(11'000)[1], false) == "Done: click here");
  // A row that is not under way has no button, whatever its kind.
  CHECK(button_text(list.rows(11'000)[3], true).empty());
  (void)list.confirm(20'000);
  CHECK(button_text(list.rows(21'000)[2], true).empty());  // a wait
  (void)list.tick(50'000);
  // The task's own word.
  CHECK(button_text(list.rows(51'000)[3], true) == "I have done it: press Pause, or click here");
  (void)list.confirm(60'000);
  // The last task, which is to quit the game: nothing to press, so no button (the owner, 2026-11-22).
  const TaskRow last = list.rows(61'000)[4];
  CHECK(last.state == TaskState::Now);
  CHECK(last.kind == TaskKind::End);
  CHECK(button_text(last, true).empty());
  CHECK(button_text(last, false).empty());
  CHECK(timer_text(last) == "0:01");
}

TEST_CASE("tasks: the rows say what is done, what is under way and for how long") {
  TaskList list(sample());
  list.start(0);
  (void)list.round_step(61'000);
  (void)list.confirm(73'000);
  const auto rows = list.rows(73'000 + 12'400);
  REQUIRE(rows.size() == 5);
  CHECK(rows[0].state == TaskState::Done);
  CHECK(rows[0].elapsed_s == 61);
  CHECK(rows[1].state == TaskState::Done);
  CHECK(rows[1].elapsed_s == 12);
  CHECK(rows[2].state == TaskState::Now);
  CHECK(rows[2].elapsed_s == 12);
  CHECK(rows[2].seconds == 30);
  CHECK(rows[3].state == TaskState::Later);
  CHECK(rows[3].title == "act");
  CHECK(rows[3].detail == "then press the key");
  // The timers, as the panel shows them.
  CHECK(timer_text(rows[0]) == "1:01");
  CHECK(timer_text(rows[2]) == "0:18 left");
  CHECK(timer_text(rows[3]).empty());
  TaskRow later_wait = rows[2];
  later_wait.state = TaskState::Later;
  CHECK(timer_text(later_wait) == "0:30");
  TaskRow doing = rows[3];
  doing.state = TaskState::Now;
  doing.elapsed_s = 7;
  CHECK(timer_text(doing) == "0:07");
  // A wait past its time (the list was not ticked) reads nothing left, never a negative time.
  TaskRow over = rows[2];
  over.elapsed_s = 45;
  CHECK(timer_text(over) == "0:00 left");
  CHECK(clock_text(0) == "0:00");
  CHECK(clock_text(185) == "3:05");
  CHECK(clock_text(3660) == "61:00");
}

TEST_CASE("tasks: with a pace every wait is that long and the tester's tasks confirm themselves, Marks included") {
  TaskList list(sample());
  list.set_pace(5);
  list.start(0);
  CHECK_FALSE(list.tick(100'000).changed);  // the round's task still waits for the round
  (void)list.round_step(10'000);
  CHECK_FALSE(list.tick(14'999).changed);
  auto c = list.tick(15'000);  // "load": itself, no Mark
  CHECK(c.changed);
  CHECK_FALSE(c.mark);
  CHECK(list.rows(15'000)[2].seconds == 5);
  c = list.tick(20'000);  // the wait, five seconds
  CHECK(c.changed);
  c = list.tick(25'000);  // "act": itself, with its Mark
  CHECK(c.changed);
  CHECK(c.mark);
  CHECK(c.task == 3);
  CHECK_FALSE(list.tick(900'000).changed);  // the last task waits for the watch's end, paced or not
  CHECK(list.end(30'000).changed);
  CHECK(list.finished());
}

TEST_CASE("tasks: the read-only round's list starts with the round's own task and marks each change of the game's state") {
  const std::vector<Task> tasks = read_tasks();
  REQUIRE(tasks.size() == 11);
  CHECK(tasks[0].kind == TaskKind::Round);
  CHECK(tasks[1].kind == TaskKind::Wait);  // the first look at the title, before the save is loaded
  std::size_t marks = 0, waits = 0, seconds = 0;
  for (const Task& t : tasks) {
    CHECK_FALSE(t.title.empty());
    CHECK_FALSE(t.detail.empty());
    CHECK(t.title.size() <= 40);
    CHECK(t.detail.size() <= 180);
    if (t.mark) {
      ++marks;
      CHECK(t.kind == TaskKind::Do);
    }
    if (t.kind == TaskKind::Wait) {
      ++waits;
      seconds += t.seconds;
      CHECK(t.seconds >= 45);
    } else {
      CHECK(t.seconds == 0);
    }
  }
  // Four marks: in the world, at the title again, in the world again, and one thing the game remembers. Each is
  // followed by a wait, in which the round reads the game in that state.
  CHECK(marks == 4);
  CHECK(waits == 5);
  CHECK(seconds == 285);
  for (std::size_t i = 0; i + 1 < tasks.size(); ++i) {
    if (tasks[i].mark) CHECK(tasks[i + 1].kind == TaskKind::Wait);
  }
  // The last task is to quit the game: it has no button and nothing confirms it.
  CHECK(tasks.back().kind == TaskKind::End);
  CHECK_FALSE(tasks.back().mark);
  CHECK(tasks.back().detail.find("nothing more to press") != std::string::npos);
  // Each task of the tester's has its own word for the button; no other kind has one.
  for (const Task& t : tasks) CHECK((t.kind == TaskKind::Do) == !t.button.empty());
}
