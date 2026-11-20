# en-augur: Augur, the event engine (ARCH §8; the first in-game build's part 11). Sources, the world model, signals,
# detectors, rules, events, audience resolution, the K store and the bus. Pure and game-neutral: no I/O, no clock, no
# thread; below en-core in the graph (en-core supplies AugurCtx and WorldFacts). The DLL links it (en_dll_module).
file(GLOB EN_AUGUR_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(en_augur STATIC ${EN_AUGUR_SOURCES})
en_dll_module(en_augur)
target_include_directories(en_augur PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_augur PUBLIC en_proto)

if(EN_BUILD_TESTS)
  file(GLOB EN_AUGUR_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp)
  add_executable(en_augur_tests ${EN_AUGUR_TESTS})
  target_include_directories(en_augur_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(en_augur_tests PRIVATE en_augur en_doctest)
  # The shipped rule files, read by the tests as the loader reads them.
  target_compile_definitions(en_augur_tests PRIVATE EN_AUGUR_CONTENT="${PROJECT_SOURCE_DIR}/content/eldenring/augur")
  add_test(NAME en_augur_tests COMMAND en_augur_tests)

  # The microbenchmark of Augur::step on a busy synthetic step (ARCH §8.10): built with the tests, run by hand.
  add_executable(en_augur_bench ${CMAKE_CURRENT_SOURCE_DIR}/tests/bench.cpp)
  target_include_directories(en_augur_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(en_augur_bench PRIVATE en_augur)
  target_compile_definitions(en_augur_bench PRIVATE EN_AUGUR_CONTENT="${PROJECT_SOURCE_DIR}/content/eldenring/augur")
endif()
