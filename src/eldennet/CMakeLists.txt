# eldennet: the DLL the launcher loads into the game (part 6 of the first in-game build; ARCH §2.2, §3): DllMain and the
# instance marker, T1's init machine, the threads v0.1 needs (T6 the core, T10 the watcher, T11 the log writer), the
# scrubbed logger, the numbers-only recorder, the watch block's status and the crash filter, and, in a probe build, R1.
# Two parts:
# - en_shell, portable and tested on Linux and under Wine in every build: settings.toml, the log lines, the channels,
#   the recorder's lines, R1's probe report and the safety checks' file names. en-tools links it for the lint's
#   vocabularies.
# - eldennet.dll (win/), the clang-cl build's alone (NativeSys needs structured exception handling): the Win32 shell
#   around en-core and en-gal. With the tests or EN_PROBE on, the same DLL is also built with the probe option
#   (probe/eldennet.dll), which runs R1.
add_library(en_shell STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/names.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/record.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/probe_report.cpp)
en_dll_module(en_shell)
target_include_directories(en_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_shell PUBLIC en_core en_gal en_proto PRIVATE en_third_party)

if(EN_BUILD_TESTS)
  add_executable(en_shell_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_record.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_probe_report.cpp)
  target_link_libraries(en_shell_tests PRIVATE en_shell en_doctest)
  add_test(NAME en_shell_tests COMMAND en_shell_tests)
endif()

if(NOT MSVC)
  message(STATUS "eldennet: the DLL is built by an MSVC-like compiler only (NativeSys's structured exception "
    "handling); cmake/clang-cl-xwin.cmake builds it")
  return()
endif()

enable_language(RC)
set(EN_DLL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/win/exports.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/boot.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/platform.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/logger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/block.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/watcher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/alloc_count.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/shell.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/win/probe_r1.cpp)
# One DLL per flavour: `release` links en_gal_game, `probe` en_gal_game_probe and brings EN_PROBE (ARCH §2.5).
# Each has its own version resource, configured from win/eldennet.rc.in with the build's label (and, for the probe
# build, SpecialBuild "probe"); the label is in the log's boot line too.
function(en_dll target gal special)
  set(EN_RC_FLAGS 0x0)
  set(EN_RC_SPECIAL "")
  if(special)
    set(EN_RC_FLAGS 0x20)
    set(EN_RC_SPECIAL "\n      VALUE \"SpecialBuild\", \"probe\"")
  endif()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/win/eldennet.rc.in ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc @ONLY)
  add_library(${target} SHARED ${EN_DLL_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
  en_dll_module(${target})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME eldennet)
  target_compile_definitions(${target} PRIVATE EN_DLL_VERSION_LABEL="${EN_VERSION_LABEL}")
  target_link_libraries(${target} PRIVATE en_shell ${gal} en_core en_gal en_proto en_third_party)
endfunction()
en_dll(eldennet en_gal_game OFF)
if(EN_BUILD_TESTS OR EN_PROBE)
  en_dll(eldennet_probe en_gal_game_probe ON)
  set_target_properties(eldennet_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/probe
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/probe ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/probe)
endif()

# ARCH §12.1's checks on the DLL: the greps (DllMain's body, the empty detach branch, no hook library) over win/, and
# the built DLL's exports and imports against the allowlists beside this file.
if(EN_BUILD_TESTS AND CMAKE_OBJDUMP)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME eldennet_dll_checks
      COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/dll_checks.py
        --source ${CMAKE_CURRENT_SOURCE_DIR} --objdump ${CMAKE_OBJDUMP}
        --dll $<TARGET_FILE:eldennet> --dll $<TARGET_FILE:eldennet_probe>)
  endif()
endif()
