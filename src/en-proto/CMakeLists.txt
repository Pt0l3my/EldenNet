# en-proto: the messages that cross a thread or the adapter boundary, as plain data with their JSON forms, the game
# access layer's types (Expected, the fixed-capacity containers, the snapshot, the lifecycle, the commands, the token
# and the backend interface), and the guard on every entry from foreign code (ARCH §2.2, §2.3; R1, R3). No I/O, no
# threads, no clocks, no Win32.
file(GLOB EN_PROTO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(en_proto STATIC ${EN_PROTO_SOURCES})
en_dll_module(en_proto)
target_include_directories(en_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_proto PUBLIC en_third_party)

if(EN_BUILD_TESTS)
  file(GLOB EN_PROTO_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_proto_tests ${EN_PROTO_TESTS})
  target_link_libraries(en_proto_tests PRIVATE en_proto en_doctest)
  add_test(NAME en_proto_tests COMMAND en_proto_tests)

  # R1's compile-fail tests (ARCH §10.1): each case is the control source built with one forbidden use of the game
  # thread's token, and CTest expects its build to fail. The control, with none, must build. They compile only (no
  # link), so they run in every build, the cross builds included, and one at a time: each runs the build tool in this
  # build tree.
  set(en_proto_cf_source ${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/game_thread.cpp)
  foreach(en_case IN ITEMS CONTROL COPY COPY_FROM_REFERENCE ASSIGN MAKE MAKE_BRACED)
    string(TOLOWER ${en_case} en_name)
    add_library(en_proto_cf_${en_name} OBJECT EXCLUDE_FROM_ALL ${en_proto_cf_source})
    target_link_libraries(en_proto_cf_${en_name} PRIVATE en_proto)
    add_test(NAME en_proto_compile_fail_${en_name}
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target en_proto_cf_${en_name})
    set_tests_properties(en_proto_compile_fail_${en_name} PROPERTIES RESOURCE_LOCK en_build_tree)
    if(NOT en_case STREQUAL "CONTROL")
      target_compile_definitions(en_proto_cf_${en_name} PRIVATE EN_CF_${en_case})
      set_tests_properties(en_proto_compile_fail_${en_name} PROPERTIES WILL_FAIL TRUE)
    endif()
  endforeach()
endif()
