# en-overlay: EldenNet's display layer through GraceUI (part 12; ARCH §2.2, §2.3; GraceUI G15, D131 to D156). Two parts:
# - portable, in every build (tested on Linux too): the F11 view's lines (view.hpp) and the world-state claims
#   (world.hpp), on en-proto and graceui.h, which is plain C;
# - in the Windows builds: the connection (overlay.hpp), through graceui-sys alone, and the view drawn on Dear ImGui
#   1.93.0's core with no backend, as GraceUI's gu_imgui_core compiles it. No hook library, no renderer backend, no
#   hook (R12, GraceUI invariant 16), and never en-core or en-gal (ARCH §2.3: it can never reach game memory).
set(EN_OVERLAY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/view.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/world.cpp)
if(WIN32)
  list(APPEND EN_OVERLAY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay.cpp)
  # Dear ImGui's core, with GraceUI's gu_imgui_core's definitions (they change the library's structures, so they are
  # PUBLIC), compiled as published: third-party code, with its warnings off but for en_dll_module's two rules, which
  # Clang keeps as errors (no dynamic initialiser, no exit-time destructor: the DLL links it).
  add_library(en_imgui_core STATIC
    ${EN_IMGUI_DIR}/imgui.cpp
    ${EN_IMGUI_DIR}/imgui_draw.cpp
    ${EN_IMGUI_DIR}/imgui_tables.cpp
    ${EN_IMGUI_DIR}/imgui_widgets.cpp)
  target_include_directories(en_imgui_core SYSTEM PUBLIC ${EN_IMGUI_DIR})
  target_compile_definitions(en_imgui_core PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS IMGUI_DISABLE_FILE_FUNCTIONS
    IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS IMGUI_DISABLE_WIN32_FUNCTIONS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(en_imgui_core PRIVATE -Wno-everything -Werror=global-constructors
      -Werror=exit-time-destructors)
  elseif(MSVC)
    target_compile_options(en_imgui_core PRIVATE /w)
  else()
    target_compile_options(en_imgui_core PRIVATE -w)
  endif()
  # Its function-local statics without the runtime's thread-safe initialisation, whose lock and condition variable the
  # DLL would otherwise import: Dear ImGui is not thread-safe anyway, and one thread at a time uses its context (T1
  # makes it, then T6 alone draws; overlay.hpp).
  if(MSVC)
    target_compile_options(en_imgui_core PRIVATE /Zc:threadSafeInit-)
  else()
    target_compile_options(en_imgui_core PRIVATE -fno-threadsafe-statics)
  endif()
endif()

add_library(en_overlay STATIC ${EN_OVERLAY_SOURCES})
en_dll_module(en_overlay)
target_include_directories(en_overlay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_overlay PUBLIC en_proto en_graceui_sys)
if(WIN32)
  target_link_libraries(en_overlay PUBLIC en_imgui_core)
  target_compile_definitions(en_overlay PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
# ARCH §2.3: en-overlay links en-proto, graceui-sys, Dear ImGui's core and the header-only libraries, and nothing else
# of the build (cmake/link_rules.cmake): never en-core or en-gal.
en_link_rule(en_overlay ALLOW en_proto en_graceui_sys en_imgui_core en_third_party)

if(EN_BUILD_TESTS)
  set(EN_OVERLAY_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_world.cpp)
  if(WIN32)
    list(APPEND EN_OVERLAY_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_grace.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_overlay.cpp)
  endif()
  add_executable(en_overlay_tests ${EN_OVERLAY_TESTS})
  target_link_libraries(en_overlay_tests PRIVATE en_overlay en_doctest)
  if(WIN32)
    target_compile_definitions(en_overlay_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  endif()
  add_test(NAME en_overlay_tests COMMAND en_overlay_tests)
endif()
