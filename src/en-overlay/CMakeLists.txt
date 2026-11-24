# en-overlay: EldenNet's display layer through GraceUI (part 12; ARCH §2.2, §2.3; GraceUI G15, D131 to D156). Two parts:
# - portable, in every build (tested on Linux too): the F11 view's lines (view.hpp), the world-state claims
#   (world.hpp), the conversation's surfaces laid out as draw lists (surfaces.hpp) and EldenNet's space (space.hpp:
#   its pages, generated from pages/space.toml into src/space_data.cpp by tools/pages/build.py, its topics and its one
#   command path; characters.hpp, the Characters pages and the profile editor), on en-proto, nlohmann/json and
#   graceui.h, plain C;
# - in the Windows builds: the connection (overlay.hpp), through graceui-sys alone, and the view drawn on Dear ImGui
#   1.93.0's core with no backend, as GraceUI's gu_imgui_core compiles it. No hook library, no renderer backend, no
#   hook (R12, GraceUI invariant 16), and never en-core or en-gal (ARCH §2.3: it can never reach game memory).
set(EN_OVERLAY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/view.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/world.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/space.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/space_data.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/surfaces.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/characters.cpp)
if(WIN32)
  list(APPEND EN_OVERLAY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/chat.cpp)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_world.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_space.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_surfaces.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_characters.cpp)
  if(WIN32)
    list(APPEND EN_OVERLAY_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_grace.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_overlay.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_chat.cpp)
  endif()
  add_executable(en_overlay_tests ${EN_OVERLAY_TESTS})
  target_link_libraries(en_overlay_tests PRIVATE en_overlay en_doctest)
  if(WIN32)
    target_compile_definitions(en_overlay_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  endif()
  add_test(NAME en_overlay_tests COMMAND en_overlay_tests)
  # The space's fixture states (tests/pages/<state>/topics.json) are the bodies en_overlay_tests builds from each
  # state's invented facts; the tests compare them, and EN_UPDATE_FIXTURES=1 writes them anew.
  # Only the Linux builds compare them: the Windows builds' tests run under Wine, away from the source tree.
  target_compile_definitions(en_overlay_tests PRIVATE EN_PAGES_FIXTURES="${PROJECT_SOURCE_DIR}/tests/pages")
endif()

# The space's source and what it builds (PAGES §2; GraceUI eldennet-changes §19): tools/pages/build.py --check fails
# when pages.json, settings.json or src/space_data.cpp is not what pages/space.toml builds; GraceUI's own page lint,
# from a GraceUI checkout whose three files the lint runs on are the pinned commit's (EN_GRACEUI_SOURCE), checks the
# rest (tests/pages/lint.py). grace-dev, from a GraceUI build at the same commit (EN_GRACE_DEV), checks each fixture
# state's folder as register_pages does and dumps each page's in-game structure against tests/pages/golden.
if(EN_BUILD_TESTS)
  find_package(Python3 3.11 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME en_pages_build COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/pages/build.py --check)
    set(EN_GRACEUI_SOURCE "" CACHE PATH "A GraceUI checkout at the pinned commit (c53f414), for its page lint")
    set(EN_GRACE_DEV "" CACHE FILEPATH "grace-dev from a Linux build of GraceUI at the pinned commit (c53f414)")
    if(EN_GRACEUI_SOURCE)
      add_test(NAME en_pages_lint COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tests/pages/lint.py
        --graceui ${EN_GRACEUI_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/pages/space.toml)
    else()
      message(STATUS "en_pages_lint is left out: EN_GRACEUI_SOURCE names no GraceUI checkout")
    endif()
    if(EN_GRACE_DEV AND NOT CMAKE_CROSSCOMPILING)
      add_test(NAME en_pages_preview COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tests/pages/preview.py
        --grace-dev ${EN_GRACE_DEV} --pages ${CMAKE_CURRENT_SOURCE_DIR}/pages --fixtures ${PROJECT_SOURCE_DIR}/tests/pages
        --work ${CMAKE_CURRENT_BINARY_DIR}/pages-preview)
    else()
      message(STATUS "en_pages_preview is left out: EN_GRACE_DEV names no grace-dev (a Linux build's)")
    endif()
  endif()
endif()
