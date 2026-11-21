# The sanitizers and libFuzzer, for the Linux builds with Clang (ARCH §12.1: the test and fuzz stages).
#
# EN_SANITIZE: "address;undefined" (AddressSanitizer with UndefinedBehaviorSanitizer, which halts at its first report
# as AddressSanitizer does), or "thread" (ThreadSanitizer). Every target is compiled and linked with them, and each
# sanitizer ends a program that it reported on with a failing status, so CTest fails on any report.
# EN_FUZZ: the libFuzzer targets in fuzz/, which need AddressSanitizer and UndefinedBehaviorSanitizer: every module is
# compiled with libFuzzer's coverage, and each target links libFuzzer (fuzz/CMakeLists.txt).
set(EN_SANITIZE "" CACHE STRING "Sanitizers for the Linux builds with Clang: address;undefined, or thread")
option(EN_FUZZ "Build the libFuzzer targets in fuzz/, with address;undefined (Clang on Linux)" OFF)

set(en_sanitizers ${EN_SANITIZE})
if(EN_FUZZ AND NOT en_sanitizers)
  set(en_sanitizers address undefined)
endif()

if(en_sanitizers)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "EN_SANITIZE and EN_FUZZ are for Clang on Linux")
  endif()
  foreach(sanitizer IN LISTS en_sanitizers)
    if(NOT sanitizer MATCHES "^(address|undefined|thread)$")
      message(FATAL_ERROR "EN_SANITIZE takes address;undefined or thread, not '${sanitizer}'")
    endif()
  endforeach()
  if("thread" IN_LIST en_sanitizers AND "address" IN_LIST en_sanitizers)
    message(FATAL_ERROR "ThreadSanitizer and AddressSanitizer cannot share a build: EN_SANITIZE takes one of them")
  endif()
  if(EN_FUZZ AND NOT ("address" IN_LIST en_sanitizers AND "undefined" IN_LIST en_sanitizers))
    message(FATAL_ERROR "EN_FUZZ builds with address;undefined, so EN_SANITIZE must be empty or address;undefined")
  endif()

  list(JOIN en_sanitizers "," en_sanitize_flag)
  # Frame pointers and debug information, so that every report names the file and line of each frame.
  add_compile_options(-fsanitize=${en_sanitize_flag} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${en_sanitize_flag})
  if("undefined" IN_LIST en_sanitizers)
    add_compile_options(-fno-sanitize-recover=all)  # UBSan halts at its first report instead of going on
  endif()
  if("thread" IN_LIST en_sanitizers)
    # TSan's static runtime defines every form of operator new, strongly, and is linked whole, so a program that
    # replaces them, as the standard allows and en-llm's tests do for their allocation faults, could not link. With the
    # shared runtime the program's own forms take precedence; the programs find it in Clang's runtime folder.
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-runtime-dir
                    OUTPUT_VARIABLE en_runtime_dir OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    add_link_options(-shared-libsan -Wl,-rpath,${en_runtime_dir})
  endif()
  if(EN_FUZZ)
    add_compile_options(-fsanitize=fuzzer-no-link)  # coverage for libFuzzer, in every module the targets link
    message(STATUS "EldenNet: -fsanitize=${en_sanitize_flag}, with libFuzzer's coverage and fuzz/")
  else()
    message(STATUS "EldenNet: -fsanitize=${en_sanitize_flag}")
  endif()
endif()
