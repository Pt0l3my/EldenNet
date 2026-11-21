#pragma once
// The game thread's token (R1; ARCH §6.4, DESIGN §2.3). Every read of a game object takes a `const GameThread&`, and
// only the code running on the game thread holds one: the task's trampoline issues a token for each tick, and the
// replay harness issues one for its own loop. The token is empty, cannot be copied or made by anyone but issue(), and
// is only moved.
//
// C++ cannot limit a public function to two files, so a CI grep allows `GameThread::issue` in exactly
// src/en-gal/src/task.cpp and src/en-tools/src/replay.cpp (ARCH §10.1, R1), besides the tests of the token and of the
// backends; compile-fail tests (tests/compile_fail/) check that a copy, or a token made any other way, does not build.
// C++ has no Send either: the type alone does not keep a token, or a reference to it, on the thread that issued it.
// Debug and test builds also check, at each read, that it runs on the issuing thread (en-gal's reader).

namespace en::proto {

class GameThread {
 public:
  GameThread(GameThread&&) noexcept = default;
  GameThread(const GameThread&) = delete;
  GameThread& operator=(const GameThread&) = delete;
  GameThread& operator=(GameThread&&) = delete;
  ~GameThread() = default;

  // Only the task's trampoline (en-gal) and the replay harness (en-tools) call this; CI enforces it.
  [[nodiscard]] static GameThread issue() noexcept { return GameThread{}; }

 private:
  GameThread() noexcept = default;
};

}  // namespace en::proto
