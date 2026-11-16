# vehprobe.dll: our own stand-in for a crash reporter with a vectored exception handler (vehprobe.cpp; S0-D, O91).
# Windows builds only, with the tests on; the fake game's harness (tests/fakegame) loads it into the fake game.
if(NOT WIN32 OR NOT EN_BUILD_TESTS)
  return()
endif()

add_library(vehprobe SHARED vehprobe.cpp)
set_target_properties(vehprobe PROPERTIES PREFIX "" OUTPUT_NAME vehprobe)
en_dll_module(vehprobe)
