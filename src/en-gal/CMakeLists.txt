# en-gal: the game access layer (ARCH §6). Its portable, offline part builds with every compiler: the PE32+ reader
# (pe), the byte patterns and their matcher (patterns), gal/builds.toml with the version gate (builds) and the layouts
# (layout); en-launcher and en-tools use it off game too (ARCH §2.3). Its guarded copy (reader.cpp) is C++ with
# structured exception handling (__try/__except, ARCH §2.6), which only an MSVC-like compiler has: the clang-cl build
# (cmake/clang-cl-xwin.cmake) adds it, and GCC's, Clang's and MinGW-w64's builds leave it out. It is a module the DLL
# links, and it knows nothing of the game.
set(EN_GAL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/patterns.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/builds.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/layout.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/toml_reader.cpp)
if(MSVC)
  list(APPEND EN_GAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/reader.cpp)
else()
  message(STATUS "en-gal: its SEH-guarded copy (__try/__except) is built by an MSVC-like compiler only, and this "
    "build's is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}; cmake/clang-cl-xwin.cmake builds it")
endif()
add_library(en_gal STATIC ${EN_GAL_SOURCES})
en_dll_module(en_gal)
target_include_directories(en_gal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_gal PUBLIC en_proto PRIVATE en_third_party)
# toml++ without exceptions, here only: en-gal may not throw (ARCH §2.4), so a parse answers its error instead. toml++
# keeps either setting in an ABI namespace of its own, so en-llm, which builds it with exceptions, links beside it.
target_compile_definitions(en_gal PRIVATE TOML_EXCEPTIONS=0)

if(EN_BUILD_TESTS)
  set(EN_GAL_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_patterns.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_builds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_layout.cpp)
  if(MSVC)
    list(APPEND EN_GAL_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reader.cpp)
  endif()
  add_executable(en_gal_tests ${EN_GAL_TESTS})
  target_link_libraries(en_gal_tests PRIVATE en_gal en_doctest)
  add_test(NAME en_gal_tests COMMAND en_gal_tests)
endif()
