// The tester's task list (tasks.hpp): the order, the timers, the key, the Marks.
#include <doctest.h>

#include "en/launcher/tasks.hpp"
#include "en/proto/watch.hpp"

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

TEST_CASE("tasks: the survey's list (plan 1): one character, a stand, a look around, a kill; five marks") {
  const std::vector<Task> tasks = read_tasks(1);
  REQUIRE(tasks.size() == 7);
  CHECK(tasks[0].kind == TaskKind::Round);
  CHECK(tasks[1].kind == TaskKind::Do);    // the load: the first mark, in the world
  CHECK(tasks[2].kind == TaskKind::Wait);  // standing: marked at its end
  CHECK(tasks[3].kind == TaskKind::Wait);  // looking around: marked at its end
  CHECK(tasks[4].kind == TaskKind::Do);    // the kill
  CHECK(tasks[5].kind == TaskKind::Wait);
  CHECK(tasks[6].kind == TaskKind::End);
  std::size_t marks = 0, seconds = 0;
  for (const Task& t : tasks) {
    CHECK_FALSE(t.title.empty());
    CHECK_FALSE(t.detail.empty());
    CHECK(t.title.size() <= 40);
    CHECK(t.detail.size() <= 180);
    marks += t.mark ? 1 : 0;
    seconds += t.seconds;
    if (t.kind == TaskKind::Do) CHECK_FALSE(t.button.empty());
  }
  CHECK(marks == 5);
  CHECK(seconds == 100);
  // A plan the launcher does not know is the rounds' list, as its option refuses any other number first.
  CHECK(read_tasks(0).size() == 14);
  CHECK(read_tasks().size() == 14);
}

TEST_CASE("tasks: the sheet's list (plan 2): a stand, a ride or a run, the tester's own death; five marks") {
  const std::vector<Task> tasks = read_tasks(2);
  REQUIRE(tasks.size() == 7);
  CHECK(tasks[0].kind == TaskKind::Round);
  CHECK(tasks[1].kind == TaskKind::Do);    // the load, somewhere quiet
  CHECK(tasks[2].kind == TaskKind::Wait);  // standing, on foot
  CHECK(tasks[3].kind == TaskKind::Wait);  // the ride or the run
  CHECK(tasks[4].kind == TaskKind::Do);    // the death, confirmed once the tester is back
  CHECK(tasks[5].kind == TaskKind::Wait);
  CHECK(tasks[6].kind == TaskKind::End);
  std::size_t marks = 0, seconds = 0;
  for (const Task& t : tasks) {
    CHECK_FALSE(t.title.empty());
    CHECK_FALSE(t.detail.empty());
    CHECK(t.title.size() <= 40);
    CHECK(t.detail.size() <= 180);
    marks += t.mark ? 1 : 0;
    seconds += t.seconds;
    if (t.kind == TaskKind::Do) CHECK_FALSE(t.button.empty());
  }
  CHECK(marks == 5);
  CHECK(seconds == 100);  // no standing within sight of enemies, and little standing at all (the nineteenth's lesson)
  // A run in which the tester is attacked or killed early still counts, and the steps say so themselves.
  CHECK(tasks[4].detail.find("counts too") != std::string::npos);
  CHECK(tasks[6].detail.find("send the files") != std::string::npos);
}

TEST_CASE("tasks: a done task says what the round gathered, and the run's own line comes once all are judged") {
  namespace w = en::proto::watch;
  TaskList list(read_tasks(2));
  list.start(0);
  const auto rows = list.rows(0);
  // The marks number the tasks that stamp one, in order: the load 1, the stand 2, the ride 3, the death 4, the
  // last stand 5; the round's own task and the quit stamp none.
  CHECK(rows[0].mark == 0);
  CHECK(rows[1].mark == 1);
  CHECK(rows[3].mark == 3);
  CHECK(rows[5].mark == 5);
  CHECK(rows[6].mark == 0);
  CHECK(gathered_footer(rows).empty());
  (void)list.round_step(1000);
  (void)list.confirm(2000);     // the load is done
  (void)list.tick(2000 + 20000);  // the stand
  std::uint32_t word = w::with_gathered(0, 1, w::kGathered);
  list.set_gathered(word);
  auto now = list.rows(25000);
  CHECK(now[1].state == TaskState::Done);
  CHECK(now[1].gathered == w::kGathered);
  CHECK(gathered_text(now[1]) == "Got it.");
  CHECK(now[2].state == TaskState::Done);
  CHECK(gathered_text(now[2]).empty());  // done, and the round has not said yet
  CHECK(gathered_text(now[3]).empty());  // under way: nothing, whatever the word holds
  CHECK(gathered_footer(now).empty());   // not every marked task is done and judged
  // All done: one task missed.
  (void)list.tick(22000 + 60000);
  (void)list.confirm(90000);
  (void)list.tick(90000 + 20000);
  word = w::with_gathered(word, 2, w::kGathered);
  word = w::with_gathered(word, 3, w::kNotGathered);
  word = w::with_gathered(word, 4, w::kGathered);
  list.set_gathered(word);
  now = list.rows(110000);
  CHECK(now[5].state == TaskState::Done);
  CHECK(gathered_footer(now).empty());  // the last stand is not judged yet
  word = w::with_gathered(word, 5, w::kGathered);
  list.set_gathered(word);
  now = list.rows(110000);
  CHECK(gathered_text(now[3]) == "Not gathered.");
  CHECK(gathered_footer(now) ==
        "Task 4 was not gathered: send the files all the same; run again only if you have the time.");
  list.set_gathered(w::with_gathered(word, 5, w::kNotGathered));
  CHECK(gathered_footer(list.rows(110000)) ==
        "Tasks 4 and 6 were not gathered: send the files all the same; run again only if you have the time.");
  list.set_gathered(w::with_gathered(word, 3, w::kGathered));
  CHECK(gathered_footer(list.rows(110000)) == "This run is usable: send it.");
  // A round that judges nothing (the earlier plans) leaves the rows and the line as they were.
  TaskList old(read_tasks(1));
  old.start(0);
  CHECK(gathered_footer(old.rows(0)).empty());
}

TEST_CASE("tasks: the read-only round's list starts with the round's own task and marks each change it asks for") {
  const std::vector<Task> tasks = read_tasks();
  REQUIRE(tasks.size() == 14);
  CHECK(tasks[0].kind == TaskKind::Round);
  CHECK(tasks[1].kind == TaskKind::Wait);  // the first look at the title, before a save is loaded
  std::size_t marks = 0, waits = 0, seconds = 0, marked_waits = 0;
  for (const Task& t : tasks) {
    CHECK_FALSE(t.title.empty());
    CHECK_FALSE(t.detail.empty());
    CHECK(t.title.size() <= 40);
    CHECK(t.detail.size() <= 180);
    if (t.mark) {
      ++marks;
      CHECK((t.kind == TaskKind::Do || t.kind == TaskKind::Wait));
      marked_waits += t.kind == TaskKind::Wait ? 1 : 0;
    }
    if (t.kind == TaskKind::Wait) {
      ++waits;
      seconds += t.seconds;
      CHECK(t.seconds >= 10);
    } else {
      CHECK(t.seconds == 0);
    }
  }
  // Eight marks. Three for the game's states (the first world, the title again, the second world); then the parts
  // watch's phases: the ends of standing, walking and standing again (marked waits), the hit and the heal.
  CHECK(marks == 8);
  CHECK(marked_waits == 3);
  CHECK(waits == 7);
  CHECK(seconds == 245);
  // The two loads are two characters: the last of the list, then the usual one, so that Continue is the usual one's.
  CHECK(tasks[2].title.find("last character") != std::string::npos);
  CHECK(tasks[6].title.find("usual character") != std::string::npos);
  // The second world's phases, in the order the parts watch counts them.
  CHECK(tasks[7].title == "Stand still");
  CHECK(tasks[7].mark);
  CHECK(tasks[7].seconds == 90);  // the character search's look comes first and may take 40 s
  CHECK(tasks[8].title == "Walk about");
  CHECK(tasks[9].title == "Stand still again");
  CHECK(tasks[10].title == "Take one hit");
  CHECK(tasks[11].title == "Heal");
  // The last task is to quit the game: it has no button and nothing confirms it.
  CHECK(tasks.back().kind == TaskKind::End);
  CHECK_FALSE(tasks.back().mark);
  CHECK(tasks.back().detail.find("nothing more to press") != std::string::npos);
  // Each task of the tester's has its own word for the button; no other kind has one.
  for (const Task& t : tasks) CHECK((t.kind == TaskKind::Do) == !t.button.empty());
}

TEST_CASE("tasks: a marked wait stamps its mark when its time is up") {
  TaskList list({{TaskKind::Wait, "stand", "d", 5, true, ""}, {TaskKind::Wait, "rest", "d", 5, false, ""},
                 {TaskKind::End, "quit", "d", 0, false, ""}});
  list.start(0);
  CHECK_FALSE(list.tick(4'000).changed);
  TaskList::Change c = list.tick(5'000);
  CHECK(c.changed);
  CHECK(c.mark);
  c = list.tick(10'000);
  CHECK(c.changed);
  CHECK_FALSE(c.mark);
}
