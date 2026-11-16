# en-tools: the command-line host that runs the engine outside the game.
file(GLOB EN_TOOLS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(eldennet-engine ${EN_TOOLS_SOURCES})
target_link_libraries(eldennet-engine PRIVATE en_prompt en_llm en_gal)
