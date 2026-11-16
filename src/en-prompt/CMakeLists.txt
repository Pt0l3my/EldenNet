# en-prompt: the template engine (inja, with SkyrimNet's template rules; NPC §7.1).
file(GLOB EN_PROMPT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(en_prompt STATIC ${EN_PROMPT_SOURCES})
target_include_directories(en_prompt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_prompt PUBLIC en_third_party)
# One environment may render on several threads: its cache is guarded and each thread keeps its own render stack.
find_package(Threads REQUIRED)
target_link_libraries(en_prompt PUBLIC Threads::Threads)

if(EN_BUILD_TESTS)
  file(GLOB EN_PROMPT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_prompt_tests ${EN_PROMPT_TESTS})
  target_link_libraries(en_prompt_tests PRIVATE en_prompt en_doctest)
  add_test(NAME en_prompt_tests COMMAND en_prompt_tests)
endif()
