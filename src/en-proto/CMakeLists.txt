# en-proto: the messages that cross a thread or the adapter boundary, as plain data with their JSON forms, and the
# guard on every entry from foreign code (ARCH §2.2, §2.3; R3). No I/O, no threads, no clocks, no Win32.
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
endif()
