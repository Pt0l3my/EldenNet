#pragma once
// The tester's panel for the runs of GraceUI's observer (S0-E's present-hook half and S4, input; roadmap S10.1.g,
// S11.1.a; the owner's request of 2026-11-23): the steps of the s0e-s4 package's runs as task lists, data in the S0
// kind's way (s0.hpp), shown by the launcher's own panel beside the game (tasks.hpp, win/panel.cpp), never anything
// drawn in the game. `s0e` is runs A1 to A4, `s0e_alone` A0 (GraceUI alone: no eldennet.dll, so nothing of EldenNet's
// ends a task there, and the list starts with one of the tester's), `s4` B. Every task of the tester's says "press
// Pause" and stamps a Mark, so that the spike's files hold when each step was done; the panel's button is the
// fallback, since a click moves the pointer out of the game, which matters where GraceUI's input is measured.
#include <string>
#include <vector>

#include "en/launcher/tasks.hpp"
#include "en/proto/watch.hpp"

namespace en::launcher {

// The tasks of a run of GraceUI's observer, in the order the tester does them; empty for any other spike.
[[nodiscard]] std::vector<Task> observer_tasks(proto::watch::Spike spike);
// Who says a task's line on the console: "Spike S0-E: ", "Spike S0-E (GraceUI alone): ", "Spike S4: ".
[[nodiscard]] std::string observer_who(proto::watch::Spike spike);

}  // namespace en::launcher
