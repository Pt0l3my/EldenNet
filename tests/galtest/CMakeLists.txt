# galtest.dll: our own test DLL for the game-access layer in game (part 4 of the first in-game build). Loaded into the
# fake game (tests/fakegame) by its --load option, it does what EldenNet's T1 and task will do, in miniature: the gate,
# discovery from gal/ data the harness derives with probe-exe, the one registration, the task's reads, the probe
# option's searches and fault injection; and prints what it found as GALTEST lines, which harness.py's gal cases check
# against `fakegame --describe`. clang-cl only (NativeSys needs structured exception handling); never shipped.
if(NOT MSVC OR NOT EN_BUILD_TESTS)
  return()
endif()
add_library(galtest SHARED galtest.cpp)
target_link_libraries(galtest PRIVATE en_gal_game_probe)
en_dll_module(galtest)
