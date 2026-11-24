#pragma once
// The tester's task list (the owner's request, 2026-11-22): what a round asks of the person at the game, in order,
// each task with its timer, the done ones greyed, and one key to confirm a task. The launcher shows it in a window
// of its own beside the game (win/panel.cpp) and stamps a Mark at each confirmation that the round is to know of.
// This file is the list itself: no window, no clock of its own, no key.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace en::launcher {

enum class TaskKind : std::uint8_t {
  Round,  // done when the round itself reaches its next step (the launcher's read step)
  Wait,   // done when its time is up
  Do,     // done when the tester presses the key
  End,    // the last thing to do, which ends the round itself (quit the game): nothing confirms it, and the panel
          // offers no button for it; it is done when the round's watch ends
};

struct Task {
  TaskKind kind = TaskKind::Do;
  std::string title;
  std::string detail;
  std::uint32_t seconds = 0;  // Wait: how long. Round and Do: 0
  bool mark = false;          // Do: its confirmation is a Mark for the round. Wait: the end of its time is one
  // Do: what the panel's button says for this task before ": press Pause, or click here" (the owner, 2026-11-22: the
  // button is to know the task). "Done" where none is given.
  std::string button;
};

enum class TaskState : std::uint8_t { Later, Now, Done };

struct TaskRow {
  std::string title;
  std::string detail;
  TaskKind kind = TaskKind::Do;
  TaskState state = TaskState::Later;
  std::uint32_t elapsed_s = 0;  // Now: since it began. Done: how long it took
  std::uint32_t seconds = 0;    // a Wait's length
  std::string button;           // a Do task's word for the panel's button; empty for every other kind
};

class TaskList {
 public:
  // What a call changed: whether another task is under way now, and whether a Mark is to be stamped for it.
  struct Change {
    bool changed = false;
    bool mark = false;
    std::size_t task = 0;  // the task that was done
  };

  explicit TaskList(std::vector<Task> tasks);

  // The tests' pace: every Wait lasts `pace_s`, and a Do task confirms itself `pace_s` after it began. 0: none.
  void set_pace(std::uint32_t pace_s) noexcept { pace_s_ = pace_s; }

  void start(std::uint64_t now_ms) noexcept;         // the first task begins
  Change round_step(std::uint64_t now_ms) noexcept;  // the round reached its next step: a Round task under way is done
  Change tick(std::uint64_t now_ms) noexcept;        // a Wait whose time is up is done (and, paced, a Do)
  Change confirm(std::uint64_t now_ms) noexcept;     // the tester's key: a Do task under way is done
  Change end(std::uint64_t now_ms) noexcept;         // the round's watch ended: an End task under way is done

  [[nodiscard]] bool started() const noexcept { return started_; }
  [[nodiscard]] bool finished() const noexcept { return started_ && current_ >= tasks_.size(); }
  [[nodiscard]] std::size_t current() const noexcept { return current_; }
  [[nodiscard]] std::size_t size() const noexcept { return tasks_.size(); }
  [[nodiscard]] std::vector<TaskRow> rows(std::uint64_t now_ms) const;

 private:
  Change next(std::uint64_t now_ms, bool mark) noexcept;
  [[nodiscard]] std::uint32_t length(const Task& t) const noexcept;

  std::vector<Task> tasks_;
  std::vector<std::uint32_t> took_s_;  // by task, once done
  std::size_t current_ = 0;
  std::uint64_t began_ms_ = 0;         // the task under way
  std::uint32_t pace_s_ = 0;
  bool started_ = false;
};

// Seconds as a clock reads them: "0:07", "3:05", "61:00".
[[nodiscard]] std::string clock_text(std::uint32_t seconds);

// What the panel's button says for the row under way, or nothing where there is no button: only a task of the
// tester's has one. `key_works`: the Pause key confirms; else only the click does.
[[nodiscard]] std::string button_text(const TaskRow& row, bool key_works);

// A row's timer as the panel shows it: a Wait under way counts down ("2:41 left"), anything else under way counts up
// ("0:12"); a done row says how long it took; a later Wait says how long it will be; a later Do or Round nothing.
[[nodiscard]] std::string timer_text(const TaskRow& row);

// The read-only round's tasks (--read), in the order the tester does them.
// `plan`: watch.hpp's kPlan*. 0: the rounds up to the seventeenth; 1: the character survey.
[[nodiscard]] std::vector<Task> read_tasks(std::uint32_t plan = 0);

}  // namespace en::launcher
