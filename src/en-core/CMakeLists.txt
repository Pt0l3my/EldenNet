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
add_library(en_core STATIC ${EN_CORE_SOURCES} ${_en_builtin})
en_dll_module(en_core)
target_include_directories(en_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_core PUBLIC en_proto en_augur en_prompt)

if(EN_BUILD_TESTS)
  file(GLOB_RECURSE EN_CORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_core_tests ${EN_CORE_TESTS})
  target_link_libraries(en_core_tests PRIVATE en_core en_doctest)
  add_test(NAME en_core_tests COMMAND en_core_tests)
endif()
