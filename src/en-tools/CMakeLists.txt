# en-tools: the command-line host that runs the engine outside the game.
# en_tools_scn: the scenario DSL and the golden runner (part 11), which the command and its tests share.
file(GLOB EN_TOOLS_LIB_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp)
add_library(en_tools_scn STATIC ${EN_TOOLS_LIB_SOURCES})
target_include_directories(en_tools_scn PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(en_tools_scn PUBLIC en_core en_shell en_gal)
file(GLOB EN_TOOLS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(eldennet-engine ${EN_TOOLS_SOURCES})
target_link_libraries(eldennet-engine PRIVATE en_prompt en_llm en_gal en_shell en_tools_scn)  # en_shell: the lint's recording vocabulary
target_compile_definitions(eldennet-engine PRIVATE EN_ENGINE_VERSION="${EN_VERSION_LABEL}")

# probe-exe and lint --report as DataPiler's job runs them (tests/cli_test.py), on the derivation tests' invented program,
# in the Linux builds (the Windows builds run the same tool under Wine on the fake game: tests/fakegame).
if(EN_BUILD_TESTS AND NOT WIN32)
  set(_en_gal_tests ${PROJECT_SOURCE_DIR}/src/en-gal/tests)
  add_executable(en_tools_write_sample ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_sample.cpp
    ${_en_gal_tests}/sample_program.cpp ${_en_gal_tests}/fake_program.cpp ${_en_gal_tests}/fake_image.cpp)
  target_include_directories(en_tools_write_sample PRIVATE ${_en_gal_tests})
  target_link_libraries(en_tools_write_sample PRIVATE en_gal)
  # Invented recordings written as eldennet.dll's core thread writes them, for rec-stats, replay and spike-report.
  add_executable(en_tools_write_recording ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_recording.cpp)
  target_link_libraries(en_tools_write_recording PRIVATE en_shell en_core)
  # The scenario DSL and the golden runner (part 11), and each route's golden check as CI runs it.
  add_executable(en_tools_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_eval.cpp)
  target_link_libraries(en_tools_tests PRIVATE en_tools_scn en_doctest)
  target_compile_definitions(en_tools_tests PRIVATE EN_TOOLS_SCENARIOS="${PROJECT_SOURCE_DIR}/tests/scenarios"
    EN_TOOLS_GOLDEN="${PROJECT_SOURCE_DIR}/tests/golden" EN_TOOLS_WORK="${CMAKE_CURRENT_BINARY_DIR}/scn")
  add_test(NAME en_tools_tests COMMAND en_tools_tests)
  file(GLOB _en_routes CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/scenarios/*.scn)
  foreach(_en_route IN LISTS _en_routes)
    get_filename_component(_en_name ${_en_route} NAME_WE)
    add_test(NAME en_golden.${_en_name} COMMAND eldennet-engine golden check ${_en_route})
  endforeach()
  # The eval (DESIGN 14, ARCH 12.1; WHY 364 (10)): each conversation scenario through `eval --llm mock` and, where it has
  # one, `--llm recorded`. On demand, never in the default run: `ctest -C eval -L eval` (BUILD.md). The reports hold
  # model text and stay in the build tree.
  file(GLOB _en_talks CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/scenarios/talk/*.toml)
  foreach(_en_talk IN LISTS _en_talks)
    get_filename_component(_en_name ${_en_talk} NAME_WLE)
    if(_en_name MATCHES "\\.mock$")
      continue()
    endif()
    if(NOT EXISTS ${PROJECT_SOURCE_DIR}/tests/scenarios/talk/${_en_name}.mock.toml)
      continue()
    endif()
    add_test(NAME en_eval.${_en_name}.mock CONFIGURATIONS eval
      COMMAND eldennet-engine eval ${_en_talk} --llm mock --report ${CMAKE_CURRENT_BINARY_DIR}/eval-reports/${_en_name}.mock.json)
    set_tests_properties(en_eval.${_en_name}.mock PROPERTIES LABELS eval)
    if(EXISTS ${PROJECT_SOURCE_DIR}/tests/scenarios/talk/${_en_name}.llm.jsonl)
      add_test(NAME en_eval.${_en_name}.recorded CONFIGURATIONS eval
        COMMAND eldennet-engine eval ${_en_talk} --llm recorded
          --report ${CMAKE_CURRENT_BINARY_DIR}/eval-reports/${_en_name}.recorded.json)
      set_tests_properties(en_eval.${_en_name}.recorded PROPERTIES LABELS eval)
    endif()
  endforeach()
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/eval-reports)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME en_tools_cli
      COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.py --engine $<TARGET_FILE:eldennet-engine>
        --writer $<TARGET_FILE:en_tools_write_sample> --work ${CMAKE_CURRENT_BINARY_DIR}/cli
        --gal ${PROJECT_SOURCE_DIR}/gal --recordings $<TARGET_FILE:en_tools_write_recording>)
  endif()
endif()
