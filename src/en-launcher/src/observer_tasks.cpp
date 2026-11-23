// The tester's panel for the runs of GraceUI's observer (observer_tasks.hpp): the s0e-s4 package's steps as data.
#include "en/launcher/observer_tasks.hpp"

namespace en::launcher {

namespace w = proto::watch;

namespace {

Task title_look(bool eldennet) {
  return {TaskKind::Do, "Look at the title screen",
          eldennet ? "GraceUI's notice \"GraceUI spike: recording\" and EldenNet's banner (\"Spike run: EldenNet makes "
                     "no call into the game\") should both show. Note whether they do, then press Pause."
                   : "GraceUI's notice \"GraceUI spike: recording\" should show, and no banner of EldenNet's: it is not "
                     "loaded in this run. Note whether it does, then press Pause.",
          0, true, "I have looked"};
}
Task load_save(const char* where) {
  return {TaskKind::Do, "Load your save", where, 0, true, "I am in the world"};
}
Task menu() {
  return {TaskKind::Do, "Open GraceUI's menu",
          "Press F5: GraceUI's menu opens, with GraceUI's own pointer (a gold-edged arrow). Look round it for a few "
          "seconds, press F5 again to close it, then press Pause.",
          0, true, "Menu opened and closed"};
}
Task overlay() {
  return {TaskKind::Do, "The Steam overlay",
          "If the Steam overlay shows in this game, press Shift+Tab to open it and again to close it, and note whether "
          "GraceUI's notice is still drawn afterwards; then press Pause. If it does not show, just press Pause.",
          0, true, "Done, or no overlay"};
}
Task play() {
  return {TaskKind::Wait, "Play for two minutes", "Play as you usually do; the panel counts the time down.", 120, false,
          ""};
}
Task quit() {
  return {TaskKind::End, "Quit the game", "That was the last task: quit from the game's own menu.", 0, false, ""};
}

}  // namespace

std::vector<Task> observer_tasks(w::Spike spike) {
  using K = TaskKind;
  switch (spike) {
    case w::Spike::S0e:  // runs A1 to A4
      return {title_look(true), load_save("When you stand in the world, press Pause."), menu(),
              {K::Do, "Open EldenNet's view",
               "Press F11: EldenNet's view opens. Press F11 again to close it, then press Pause.", 0, true,
               "View opened and closed"},
              overlay(), play(), quit()};
    case w::Spike::S0eAlone:  // run A0: no eldennet.dll, so the list starts with a task of the tester's
      return {title_look(false), load_save("When you stand in the world, press Pause."), menu(), overlay(), play(),
              quit()};
    case w::Spike::S4:  // run B
      return {load_save("Stand somewhere safe, away from enemies, then press Pause."),
              {K::Do, "The typing test",
               "Press F9: the typing test opens in the middle of the screen and takes the keyboard and the mouse. For "
               "60 seconds, type anything (W, A, S and D among it, some held down) and move the mouse about; what you "
               "type shows in the test's own field only. It closes by itself after 60 seconds (holding Esc for a "
               "second stops it early). Note whether the Tarnished moved or the camera turned while it was open, "
               "then press Pause.",
               0, true, "The test has closed"},
              {K::Do, "Walk into the menu",
               "Hold W so the Tarnished walks, press F5 while still holding it, let W go, then press F5 again: the "
               "Tarnished must stand still once the menu is open, and must not start walking again when it closes. "
               "Then press Pause.",
               0, true, "Done"},
              {K::Do, "Only if you have a pad",
               "With a pad connected: open GraceUI's menu with F5, then press B twice; the Tarnished must not dodge or "
               "roll. Without a pad, just press Pause.",
               0, true, "Done, or no pad"},
              {K::Do, "Menu size, once",
               "Press F5, and on the Menu page set Menu size to 1.50 (the left and right arrow keys change it); close "
               "the menu and open it again; then set it back to 1.25 (R resets it) and close the menu. Then press "
               "Pause.",
               0, true, "Done"},
              quit()};
    default:
      return {};
  }
}

std::string observer_who(w::Spike spike) {
  switch (spike) {
    case w::Spike::S0e: return "Spike S0-E: ";
    case w::Spike::S0eAlone: return "Spike S0-E (GraceUI alone): ";
    case w::Spike::S4: return "Spike S4: ";
    default: return "Spike: ";
  }
}

}  // namespace en::launcher
