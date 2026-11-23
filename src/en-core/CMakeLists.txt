# en-core: the engine's core (ARCH §2.2). Its v0.1 slice (the first in-game build, part 5): the lifecycle without the
# store (ARCH §9), the offline and multiplayer gate with its hysteresis (ARCH §5.1), and the ladder's L1 and L2 with
# Dormant (ARCH §6.8). Pure: no I/O, no threads, no clocks, no Win32 (ARCH §2.3). The DLL links it (en_dll_module).
#
# Augur runs in every step (ARCH §8): en-core links en-augur, and builds in the shipped rule files
# (content/eldenring/augur/*.toml), read at configure time into builtin_rules.cpp (cmake/embed_files.cmake), since v0.1
# loads no content.
#
# The first conversation (WHY§364) adds to the core in folders, each one part's: src/speech/ and src/prompt/ (part A),
# src/content/, src/identity/ and src/conversation/ (part B), and their tests in tests/<folder>/. The globs below take
# every .cpp under src/ and tests/, so neither part edits this file to add one; en-prompt is linked already for A's
# renderer (ARCH §2.3 allows it), and the README says who owns which file.
file(GLOB_RECURSE EN_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB EN_AUGUR_RULE_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/content/eldenring/augur/*.toml)
list(SORT EN_AUGUR_RULE_FILES)
set(_en_builtin ${CMAKE_CURRENT_BINARY_DIR}/builtin_rules.cpp)
en_embed_files(${_en_builtin} INCLUDE en/core/core.hpp NAMESPACE en::core TYPE BuiltinFile FUNCTION builtin_rule_files
  FILES ${EN_AUGUR_RULE_FILES})
# The shipped persona index and records (part B), the dialogue prompt's templates and the lint's lexicon (part A), and
# the world files that quest and world awareness reads (S11.3.c: the flag catalogue, regions.toml, the world stages and
# tracks), built in as the rule files are: the shell adds the overrides folder's.
file(GLOB EN_CONTENT_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/content/eldenring/identity/*.toml
  ${PROJECT_SOURCE_DIR}/content/eldenring/personas/*.toml ${PROJECT_SOURCE_DIR}/content/eldenring/lexicon/*.toml
  ${PROJECT_SOURCE_DIR}/content/eldenring/*.toml ${PROJECT_SOURCE_DIR}/content/eldenring/flags/*.toml
  ${PROJECT_SOURCE_DIR}/content/eldenring/world/*.toml
  ${PROJECT_SOURCE_DIR}/content/eldenring/prompts/*/*.md ${PROJECT_SOURCE_DIR}/content/eldenring/prompts/*/*.j2)
list(SORT EN_CONTENT_FILES)
set(_en_content ${CMAKE_CURRENT_BINARY_DIR}/builtin_content.cpp)
en_embed_files(${_en_content} INCLUDE en/core/core.hpp NAMESPACE en::core TYPE BuiltinFile FUNCTION builtin_content_files
  BASE ${PROJECT_SOURCE_DIR}/content/eldenring FILES ${EN_CONTENT_FILES})
add_library(en_core STATIC ${EN_CORE_SOURCES} ${_en_builtin} ${_en_content})
en_dll_module(en_core)
target_include_directories(en_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_core PUBLIC en_proto en_augur en_prompt)

if(EN_BUILD_TESTS)
  file(GLOB_RECURSE EN_CORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  # The tests' own files, built in so that the Windows builds read no path under Wine: the overrides folders that bind
  # the test persona (part B) and the pilot (S11.3.c), the fixture flag states and the truth tables.
  file(GLOB_RECURSE EN_CORE_TEST_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/scenarios/talk/overrides/*.toml
    ${PROJECT_SOURCE_DIR}/tests/scenarios/talk/pilot-overrides/*.toml
    ${PROJECT_SOURCE_DIR}/tests/fixtures/flagstates/*.toml ${PROJECT_SOURCE_DIR}/tests/knowledge/*.toml)
  list(SORT EN_CORE_TEST_FILES)
  set(_en_test_files ${CMAKE_CURRENT_BINARY_DIR}/test_files.cpp)
  en_embed_files(${_en_test_files} INCLUDE en/core/core.hpp NAMESPACE en::core::test TYPE en::core::BuiltinFile
    FUNCTION test_files BASE ${PROJECT_SOURCE_DIR} FILES ${EN_CORE_TEST_FILES})
  add_executable(en_core_tests ${EN_CORE_TESTS} ${_en_test_files})
  target_link_libraries(en_core_tests PRIVATE en_core en_doctest)
  add_test(NAME en_core_tests COMMAND en_core_tests)
endif()
