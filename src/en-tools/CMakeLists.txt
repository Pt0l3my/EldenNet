# en-tools: the command-line host that runs the engine outside the game.
file(GLOB EN_TOOLS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(eldennet-engine ${EN_TOOLS_SOURCES})
target_link_libraries(eldennet-engine PRIVATE en_prompt en_llm en_gal en_shell)  # en_shell: the lint's vocabularies

# probe-exe and lint --report as DataPiler's job runs them (tests/cli_test.py), on the derivation tests' invented program,
# in the Linux builds (the Windows builds run the same tool under Wine on the fake game: tests/fakegame).
if(EN_BUILD_TESTS AND NOT WIN32)
  set(_en_gal_tests ${PROJECT_SOURCE_DIR}/src/en-gal/tests)
  add_executable(en_tools_write_sample ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_sample.cpp
    ${_en_gal_tests}/sample_program.cpp ${_en_gal_tests}/fake_program.cpp ${_en_gal_tests}/fake_image.cpp)
  target_include_directories(en_tools_write_sample PRIVATE ${_en_gal_tests})
  target_link_libraries(en_tools_write_sample PRIVATE en_gal)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME en_tools_cli
      COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.py --engine $<TARGET_FILE:eldennet-engine>
        --writer $<TARGET_FILE:en_tools_write_sample> --work ${CMAKE_CURRENT_BINARY_DIR}/cli)
  endif()
endif()
