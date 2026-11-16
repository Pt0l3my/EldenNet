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
  ${CMAKE_CURRENT_SOURCE_DIR}/src/toml_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/x86.cpp)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_x86.cpp)
  if(MSVC)
    list(APPEND EN_GAL_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reader.cpp)
  endif()
  add_executable(en_gal_tests ${EN_GAL_TESTS})
  target_link_libraries(en_gal_tests PRIVATE en_gal en_doctest)
  add_test(NAME en_gal_tests COMMAND en_gal_tests)

  # The decoder against LLVM's disassembler (tests/x86_objdump.py), in the Linux builds: every instruction of our own
  # binaries, where any disagreement is a finding, and 20,000 seeded random byte strings, assembled by the build's own
  # compiler, where three kinds that no compiler emits are counted apart (tests/x86_check.cpp says which).
  add_executable(en_gal_x86_check ${CMAKE_CURRENT_SOURCE_DIR}/tests/x86_check.cpp)
  target_link_libraries(en_gal_x86_check PRIVATE en_gal)
  if(NOT WIN32)
    find_package(Python3 COMPONENTS Interpreter)
    find_program(EN_LLVM_OBJDUMP NAMES llvm-objdump llvm-objdump-18 DOC "llvm-objdump, for the decoder's comparison")
    if(Python3_Interpreter_FOUND AND EN_LLVM_OBJDUMP)
      set(_en_x86 ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/x86_objdump.py)
      add_test(NAME en_gal_x86_objects
        COMMAND ${_en_x86} objects --checker $<TARGET_FILE:en_gal_x86_check> --objdump ${EN_LLVM_OBJDUMP}
          --work ${CMAKE_CURRENT_BINARY_DIR}/x86 $<TARGET_FILE:eldennet-engine> $<TARGET_FILE:en_gal_tests>)
      add_test(NAME en_gal_x86_random
        COMMAND ${_en_x86} random --checker $<TARGET_FILE:en_gal_x86_check> --objdump ${EN_LLVM_OBJDUMP}
          --assembler ${CMAKE_CXX_COMPILER} --work ${CMAKE_CURRENT_BINARY_DIR}/x86 --count 20000 --seed 1)
      set_tests_properties(en_gal_x86_objects en_gal_x86_random PROPERTIES TIMEOUT 600)
    else()
      message(STATUS "en-gal: the decoder's comparison with LLVM is skipped: it needs Python 3 and llvm-objdump")
    endif()
  endif()
endif()
