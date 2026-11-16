# libFuzzer's targets (ARCH §12.1's fuzz stage), built with EN_FUZZ (cmake/sanitizers.cmake). Each has its folder: the
# target's source, a dictionary for libFuzzer's -dict and a seed corpus of invented inputs, which also keeps, as a
# regression seed, the input of every finding once it is fixed. CTest runs each corpus once; `en_fuzz` builds them all.
add_custom_target(en_fuzz)

function(en_fuzz_target name)
  set(dir ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  add_executable(en_fuzz_${name} ${dir}/fuzz_${name}.cpp)
  target_link_libraries(en_fuzz_${name} PRIVATE ${ARGN})
  target_link_options(en_fuzz_${name} PRIVATE -fsanitize=fuzzer)
  add_dependencies(en_fuzz en_fuzz_${name})
  if(EN_BUILD_TESTS)
    add_test(NAME en_fuzz_${name}_corpus COMMAND en_fuzz_${name} -runs=0 ${dir}/corpus)
  endif()
endfunction()

en_fuzz_target(sse en_llm)         # en-llm's SSE line parser and stream assembler
en_fuzz_target(json en_proto)      # en-proto's parse_json with its caps, and read_json
en_fuzz_target(game en_proto)      # en-proto's JSON forms of the game messages: snapshot, lifecycle, command, feed
en_fuzz_target(template en_prompt) # en-prompt's template compile from a memory source
