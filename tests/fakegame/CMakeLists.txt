# The fake game (README.md): our own Windows program that stands in for a game in EldenNet's tests, and its harness,
# which runs it under Wine on Xvfb and checks each behaviour from outside. Windows builds only (clang-cl with xwin,
# and MinGW-w64), with the tests on. Nothing in it is a game's.
if(NOT WIN32 OR NOT EN_BUILD_TESTS)
  return()
endif()
enable_language(RC)

add_executable(fakegame WIN32
  asm.cpp describe.cpp faults.cpp hub.cpp main.cpp selftest.cpp world.cpp fakegame.rc)
target_link_libraries(fakegame PRIVATE en_third_party user32 gdi32 shell32 winmm version)

# --- The harness: CTest runs each case of harness.py (its CASES) under Wine on Xvfb, or directly on Windows ------------
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
  message(STATUS "The fake game's harness is skipped: no Python 3")
  return()
endif()
if(CMAKE_HOST_WIN32)
  set(_en_runner_args --native)
else()
  find_program(EN_WINE NAMES wine64 wine HINTS /usr/lib/wine /usr/lib64/wine
    DOC "wine64, for the Windows tests under Wine (the fake game's harness)")
  find_program(EN_XVFB_RUN NAMES xvfb-run DOC "xvfb-run, for the Windows tests that open a window")
  set(EN_WINE_PREFIX "${CMAKE_BINARY_DIR}/wineprefix" CACHE PATH
    "The Wine prefix the Windows tests run in (made by the first test; several hundred MB)")
  if(NOT EN_WINE OR NOT EN_XVFB_RUN)
    message(STATUS "The fake game's harness is skipped: it needs wine64 and xvfb-run (EN_WINE, EN_XVFB_RUN)")
    return()
  endif()
  set(_en_runner_args --wine ${EN_WINE} --xvfb-run ${EN_XVFB_RUN} --prefix ${EN_WINE_PREFIX})
endif()
if(MSVC)
  set(_en_toolchain clang-cl)
else()
  set(_en_toolchain mingw)
endif()

# Paths a case may name besides the fake game's own ({exe}, {vehprobe}): a later part that adds chain cases (the DLL,
# the launcher) appends its target here, as `--set <name>=$<TARGET_FILE:<target>>` and the name to _en_have, and
# harness.py lists a case that needs a path only when it is given.
set(_en_have "")
set(_en_set "")
# The derivation's offline round (part 3): the Windows eldennet-engine.exe, run under Wine on fakegame.exe with the fake
# game's leads (derive-leads.toml), against what --describe says.
if(TARGET eldennet-engine)
  list(APPEND _en_have engine leads)
  list(APPEND _en_set --set engine=$<TARGET_FILE:eldennet-engine> --set leads=${CMAKE_CURRENT_SOURCE_DIR}/derive-leads.toml)
endif()

# The GAL in game (part 4): galtest.dll, loaded into fakegame.exe, discovers the fake game's facts from gal/ files the
# harness writes from probe-exe's derive.json and gal-layout.toml.in, registers its task and reads every group.
if(TARGET galtest AND TARGET eldennet-engine)
  list(APPEND _en_have galtest gallayout)
  list(APPEND _en_set --set galtest=$<TARGET_FILE:galtest> --set gallayout=${CMAKE_CURRENT_SOURCE_DIR}/gal-layout.toml.in)
endif()

set(_en_harness ${CMAKE_CURRENT_SOURCE_DIR}/harness.py)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_en_harness})
list(JOIN _en_have "," _en_have_list)
execute_process(COMMAND ${Python3_EXECUTABLE} -I ${_en_harness} list --toolchain ${_en_toolchain}
  --have "${_en_have_list}"
  OUTPUT_VARIABLE _en_cases RESULT_VARIABLE _en_listed OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _en_listed EQUAL 0)
  message(FATAL_ERROR "harness.py could not list its cases")
endif()
string(REPLACE "\n" ";" _en_cases "${_en_cases}")

# One Wine prefix serves every test: the first test makes it (or checks it), and no two tests run in it at once.
add_test(NAME fakegame.prefix COMMAND ${Python3_EXECUTABLE} -I ${_en_harness} setup ${_en_runner_args})
set_tests_properties(fakegame.prefix PROPERTIES FIXTURES_SETUP en_wine_prefix RESOURCE_LOCK en_wine TIMEOUT 300)
foreach(_en_case IN LISTS _en_cases)
  add_test(NAME fakegame.${_en_case}
    COMMAND ${Python3_EXECUTABLE} -I ${_en_harness} run ${_en_case} --toolchain ${_en_toolchain}
      --exe $<TARGET_FILE:fakegame> --vehprobe $<TARGET_FILE:vehprobe> ${_en_set}
      --work ${CMAKE_CURRENT_BINARY_DIR}/harness ${_en_runner_args})
  set_tests_properties(fakegame.${_en_case} PROPERTIES
    FIXTURES_REQUIRED en_wine_prefix RESOURCE_LOCK en_wine TIMEOUT 240)
endforeach()
