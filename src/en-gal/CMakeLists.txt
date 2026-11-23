# en-gal: the game access layer (ARCH §6). Three libraries:
# - en_gal, its offline part, which builds with every compiler: the PE32+ reader (pe), the byte patterns and their
#   matcher (patterns), gal/builds.toml with the version gate (builds), the layouts (layout), and the derivation kit
#   (x86, rtti, fd4, derive, report). en-launcher and en-tools use it off game (ARCH §2.3). With an MSVC-like
#   compiler it also holds the guarded copy (reader.cpp: structured exception handling, __try/__except, ARCH §2.6),
#   which the clang-cl build (cmake/clang-cl-xwin.cmake) adds and GCC's, Clang's and MinGW-w64's leave out.
# - en_gal_game, the part that runs in the game process (part 4 of the first in-game build): GuardedReader, the
#   groups and their breakers, discovery over the live image, ErBackend's reads, the task, all portable and tested on
#   Linux over the tests' FakeSys; and, with an MSVC-like compiler, NativeSys (sys/), the platform seam in game.
# - en_gal_game_probe, the same with the probe option (EN_PROBE, ARCH §2.5, §6.10: the invariant searches, slot
#   counts and fault injection). It is built for the tests and the test DLL, and when EN_PROBE is on; a release build
#   links en_gal_game, and CI's `options` stage refuses EN_PROBE there (ARCH §12.1).
# Every module the DLL links keeps en_dll_module's rules, and none knows the game.
option(EN_PROBE "Build the probe option of en-gal (the read-only in-game round; never in a release)" OFF)

set(EN_GAL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/patterns.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/builds.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ids.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/layout.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/resolve.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/toml_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/x86.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/report.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/image_bytes.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/code.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cut.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rtti.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shape.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fd4.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/recipes.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/derive.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/derive_leads.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/derive_report.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lodestar.cpp)
if(MSVC)
  list(APPEND EN_GAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/reader.cpp)
else()
  message(STATUS "en-gal: its SEH-guarded copy (__try/__except) and NativeSys are built by an MSVC-like compiler only, "
    "and this build's is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}; cmake/clang-cl-xwin.cmake builds them")
endif()
add_library(en_gal STATIC ${EN_GAL_SOURCES})
en_dll_module(en_gal)
target_include_directories(en_gal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_gal PUBLIC en_proto PRIVATE en_third_party)
# toml++ is built without exceptions (en_third_party, CMakeLists.txt): en-gal may not throw (ARCH §2.4), so a parse
# answers its error instead.

set(EN_GAL_GAME_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/guarded.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/roles.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/discovery.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/backend.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/readers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/task.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lodestar_trial.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lodestar_chars.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lodestar_parts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/probe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/probe_memory.cpp)
if(MSVC)
  # NativeSys, the platform seam in game (sys/): structured exception handling and Win32, clang-cl only.
  list(APPEND EN_GAL_GAME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sys/native_sys.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sys/register.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sys/trampoline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sys/boot.cpp)  # eldennet.dll's load and its crash filter's look-up (part 6)
endif()
function(en_gal_game target probe)
  add_library(${target} STATIC ${EN_GAL_GAME_SOURCES})
  en_dll_module(${target})
  target_link_libraries(${target} PUBLIC en_gal en_proto)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  # PUBLIC: a class's layout depends on it (guarded.hpp), so every user of the headers must see the same value.
  target_compile_definitions(${target} PUBLIC EN_PROBE=${probe})
endfunction()
en_gal_game(en_gal_game 0)
if(EN_BUILD_TESTS OR EN_PROBE)
  en_gal_game(en_gal_game_probe 1)
endif()

if(EN_BUILD_TESTS)
  set(EN_GAL_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_patterns.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_builds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_x86.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/sample_program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_derive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_resolve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shape.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_recipes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lodestar.cpp
    # The in-game part, on FakeSys (part 4)
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/alloc_count.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_sys.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_world.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_guarded.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_discovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_discovery_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lodestar_game.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_probe.cpp)
  if(MSVC)
    list(APPEND EN_GAL_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reader.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_sys.cpp)
  endif()
  add_executable(en_gal_tests ${EN_GAL_TESTS})
  # gal/ids.toml as the repository holds it, compiled into the tests byte for byte (tests/gal_ids_text.hpp.in), so that
  # the catalogue EldenNet ships is the one the tests read, under Wine too.
  file(READ ${PROJECT_SOURCE_DIR}/gal/ids.toml EN_GAL_IDS_TOML)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/gal_ids_text.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/gen/gal_ids_text.hpp @ONLY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/gal/ids.toml)
  target_include_directories(en_gal_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen)
  target_link_libraries(en_gal_tests PRIVATE en_gal_game_probe en_gal en_doctest)
  target_include_directories(en_gal_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)  # the derivation's internals
  add_test(NAME en_gal_tests COMMAND en_gal_tests)

  # The decoder against LLVM's disassembler (tests/x86_objdump.py), in the Linux builds: every instruction of our own
  # binaries, where any disagreement is a finding, and 20,000 seeded random byte strings, assembled by the build's own
  # compiler, where three kinds that no compiler emits are counted apart (tests/x86_check.cpp says which).
  add_executable(en_gal_x86_check ${CMAKE_CURRENT_SOURCE_DIR}/tests/x86_check.cpp)
  target_link_libraries(en_gal_x86_check PRIVATE en_gal)
  if(NOT WIN32)
    find_package(Python3 COMPONENTS Interpreter)
    find_program(EN_LLVM_OBJDUMP NAMES llvm-objdump llvm-objdump-18 DOC "llvm-objdump, for the decoder's comparison")
    if(Python3_Interpreter_FOUND AND EN_LLVM_OBJDUMP)
      set(_en_x86 ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/x86_objdump.py)
      add_test(NAME en_gal_x86_objects
        COMMAND ${_en_x86} objects --checker $<TARGET_FILE:en_gal_x86_check> --objdump ${EN_LLVM_OBJDUMP}
          --work ${CMAKE_CURRENT_BINARY_DIR}/x86 $<TARGET_FILE:eldennet-engine> $<TARGET_FILE:en_gal_tests>)
      add_test(NAME en_gal_x86_random
        COMMAND ${_en_x86} random --checker $<TARGET_FILE:en_gal_x86_check> --objdump ${EN_LLVM_OBJDUMP}
          --assembler ${CMAKE_CXX_COMPILER} --work ${CMAKE_CURRENT_BINARY_DIR}/x86 --count 20000 --seed 1)
      set_tests_properties(en_gal_x86_objects en_gal_x86_random PROPERTIES TIMEOUT 600)
    else()
      message(STATUS "en-gal: the decoder's comparison with LLVM is skipped: it needs Python 3 and llvm-objdump")
    endif()
  endif()
endif()
