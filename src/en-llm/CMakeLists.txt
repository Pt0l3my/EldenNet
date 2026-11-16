# en-llm: the model client, job slots and decision calls (ARCH §11, §11.8).
file(GLOB EN_LLM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(en_llm STATIC ${EN_LLM_SOURCES})
en_dll_module(en_llm)
target_include_directories(en_llm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_llm PUBLIC en_third_party)
if(EN_WITH_CURL)
  find_package(CURL REQUIRED)
  target_link_libraries(en_llm PRIVATE CURL::libcurl)
  target_compile_definitions(en_llm PUBLIC EN_WITH_CURL=1)
endif()
# No winhttp here: the WinHTTP transport loads the system's own winhttp.dll by its full path (transport.hpp).
find_package(Threads REQUIRED)
target_link_libraries(en_llm PUBLIC Threads::Threads)

if(EN_BUILD_TESTS)
  file(GLOB EN_LLM_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_llm_tests ${EN_LLM_TESTS})
  target_link_libraries(en_llm_tests PRIVATE en_llm en_doctest)
  if(WIN32)
    target_link_libraries(en_llm_tests PRIVATE ws2_32)  # the tests' local server (local_server.cpp)
  endif()
  if(WIN32 AND CMAKE_OBJDUMP)
    # The build fails if the tests' import table names winhttp.dll: an import by name would let a winhttp.dll in the
    # game's folder stand in for the system's (transport.hpp).
    set(EN_LLM_IMPORT_CHECK ${CMAKE_CURRENT_BINARY_DIR}/check_imports.cmake)
    file(WRITE ${EN_LLM_IMPORT_CHECK} [=[
execute_process(COMMAND "${OBJDUMP}" -p "${BINARY}" OUTPUT_VARIABLE dump RESULT_VARIABLE failed)
if(failed)
  message(FATAL_ERROR "objdump could not read ${BINARY}")
endif()
string(TOLOWER "${dump}" dump)
if(dump MATCHES "dll name: winhttp\\.dll")
  message(FATAL_ERROR "${BINARY} imports winhttp.dll: the WinHTTP transport must load it by its full path")
endif()
]=])
    add_custom_command(TARGET en_llm_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -DOBJDUMP=${CMAKE_OBJDUMP} -DBINARY=$<TARGET_FILE:en_llm_tests> -P ${EN_LLM_IMPORT_CHECK}
      VERBATIM)
  endif()
  add_test(NAME en_llm_tests COMMAND en_llm_tests)
endif()
