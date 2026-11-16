# The fake game: our own Windows program that stands in for a game in EldenNet's tests. Windows builds only (clang-cl
# with xwin, and MinGW-w64), with the tests on. Nothing in it is a game's.
if(NOT WIN32 OR NOT EN_BUILD_TESTS)
  return()
endif()
enable_language(RC)

add_executable(fakegame WIN32
  asm.cpp describe.cpp faults.cpp hub.cpp main.cpp selftest.cpp world.cpp fakegame.rc)
target_link_libraries(fakegame PRIVATE en_third_party user32 gdi32 shell32 winmm version)
