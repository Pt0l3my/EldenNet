# EldenNet: our own engine core (O84), built outside the game first (roadmap S8.1.a).
cmake_minimum_required(VERSION 3.20)
project(EldenNet VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EN_BUILD_TESTS "Build the unit tests" ON)
# The version the programs report: eldennet-launcher's log, eldennet-engine version, eldennet.dll's version resource and
# log. A spike package is labelled 0.1.0-spike.N (the plan's part 10; tools/spikepkg).
set(EN_VERSION_LABEL "${PROJECT_VERSION}" CACHE STRING "The build's version label")
if(NOT EN_VERSION_LABEL MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+(-[a-z]+(\\.[0-9]+)?)?$")
  message(FATAL_ERROR "EN_VERSION_LABEL is a version such as 0.1.0 or 0.1.0-spike.1")
endif()
option(EN_WITH_CURL "Build the libcurl transport (Linux and test builds)" ON)

# Header-only libraries (THIRD_PARTY.md). inja and doctest are vendored with their licences; nlohmann/json and toml++
# are downloaded at configure time from their original projects, pinned by SHA-256, into the build tree, so that the
# repository holds no third-party author's contact details (the privacy sweep's rule).
set(EN_DEPS ${CMAKE_BINARY_DIR}/_deps)
function(en_fetch url path sha256)
  if(NOT EXISTS ${EN_DEPS}/${path})
    file(DOWNLOAD ${url} ${EN_DEPS}/${path} EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS st)
    list(GET st 0 code)
    if(NOT code EQUAL 0)
      message(FATAL_ERROR "Could not download ${url}: ${st}")
    endif()
  endif()
endfunction()
en_fetch(https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
  nlohmann/json.hpp 9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6)
en_fetch(https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT
  nlohmann/LICENSE.MIT 86b998c792894ccb911a1cb7994f7a9652894e7a094c0b5e45be2f553f45cf14)
en_fetch(https://raw.githubusercontent.com/marzer/tomlplusplus/v3.4.0/toml.hpp
  tomlplusplus/toml.hpp 6b5172ad4dd6519aec67b919181fa7a38a2234131e5b2afa232dfe444819783e)
en_fetch(https://raw.githubusercontent.com/marzer/tomlplusplus/v3.4.0/LICENSE
  tomlplusplus/LICENSE 529bc3900a9571e49db285b0df432397e70b881cc3bf48de6667ae74ff4b06d8)

# Dear ImGui 1.93.0's core, for the F11 view (en-overlay; DESIGN O25, ARCH §2.3): the very pin GraceUI uses (its
# cmake/third_party.cmake at GraceUI 3f1352b, unchanged since f51bcc7): tag v1.93.0's commit, from its own project,
# each file by its SHA-256, the core alone and no backend. Only the Windows builds build en-overlay's connection and
# the view, so only they fetch it.
if(WIN32)
  set(EN_IMGUI_COMMIT c1aaac65e896a45f81f47a4d68f9959e0f86c859)  # ocornut/imgui, tag v1.93.0
  set(EN_IMGUI_DIR ${EN_DEPS}/imgui-1.93.0)
  foreach(pin IN ITEMS
      "LICENSE.txt|173506a2d6f7fb67990d257fb2507f188690eca39060c39469ae7bef43aae2a3"
      "imconfig.h|fa91f606da783669ffd485f3f9e0cd0d92cddeac459efce4c2b9327cce7a3233"
      "imgui.h|314462301948fbd5673f5ead6f9a53acf54767cbee57e186e69f8f8b022c7094"
      "imgui.cpp|9542082a1eaeffcab7b3d8069e707839ce1714d167005e9f273ac9f656b415ed"
      "imgui_draw.cpp|fff3bb04ca11e32041144e83a37b89ec92f4d5c62d681eb7ef29ee6cd7663e84"
      "imgui_internal.h|61bc50172d907cc931159cdce48fbff1db616cda33251c9c3469e316056939e6"
      "imgui_tables.cpp|36635c1cf2ad1b7c88cdced456cceba3a1d02266d78a63d77e9cce92f4d48d61"
      "imgui_widgets.cpp|d2d388f711688f6a5ce1ae3b30168c1bf4f954ead40235b0b2b9667452850c93"
      "imstb_rectpack.h|889b396795202d1457560a797a7242e96f6f132d4b88ca2d69be58bf05e1771f"
      "imstb_textedit.h|a985f5fa0ed97353d493b497961e9eef52082edcd045cf6954b69990ec9d0741"
      "imstb_truetype.h|c51a0f7e7ea760f2366bd3752635ec58e21fccfec4a832501639990ba6ce0528")
    string(REPLACE "|" ";" pin "${pin}")
    list(GET pin 0 _en_path)
    list(GET pin 1 _en_sha256)
    en_fetch(https://raw.githubusercontent.com/ocornut/imgui/${EN_IMGUI_COMMIT}/${_en_path}
      imgui-1.93.0/${_en_path} ${_en_sha256})
  endforeach()
endif()

# GraceUI's graceui-sys (its C helper, C++ wrapper and headless Dear ImGui helper) and the ABI header it includes,
# vendored unmodified from GraceUI at 3f1352b with GraceUI's licence (third_party/graceui-sys, THIRD_PARTY.md), and
# graceui_test.h, the test build's exports, which the fake game reads in the tests alone (from GraceUI at 2131839,
# GraceUI 0.2.1, since its test state gained D175's fields). Each file is checked against
# its SHA-256 here, so that an edit is caught when the build is configured.
foreach(pin IN ITEMS
    "LICENSE|8cdd091310968c1af65a292fad858fb4a13509d829e4d1e5594f3a391028a4e0"
    "graceui.h|c652405a182283c08041a46f89df9a6f08035ce24e72ed30f3343b64ff1d74e6"
    "graceui_sys.h|b76946029472afb2e7006cfc57da6463c50b23dcf41144927a4a1e1822d6f7a7"
    "graceui_sys.hpp|b718314c6d4b0c8b2112c9658e53a328d7da5e45c1a8af4bee1d262ee8835250"
    "graceui_imgui.hpp|b7b66dd74af0cf8600f407447d70df8dd7e1f8aa0df887924922bbd9e2f33c02"
    "graceui_test.h|bce62f08c8a5abd939c231d106cc05b920059046f08ba2070dc94a670cc32ff4")
  string(REPLACE "|" ";" pin "${pin}")
  list(GET pin 0 _en_path)
  list(GET pin 1 _en_sha256)
  file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/third_party/graceui-sys/${_en_path} _en_have)
  if(NOT _en_have STREQUAL _en_sha256)
    message(FATAL_ERROR "third_party/graceui-sys/${_en_path} is not GraceUI 3f1352b's (SHA-256 ${_en_have})")
  endif()
endforeach()
add_library(en_graceui_sys INTERFACE)
target_include_directories(en_graceui_sys SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/graceui-sys)

add_library(en_third_party INTERFACE)
target_include_directories(en_third_party SYSTEM INTERFACE
  ${EN_DEPS}
  ${EN_DEPS}/tomlplusplus
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/inja)
# toml++ without exceptions, in every module alike: en-gal may not throw (ARCH §2.4), so a parse answers its error. One
# setting for all: toml++'s internal reader interface has a virtual function only without exceptions, under the same
# name either way, so two settings in one program are two different vtables the linker merges into one (it crashed
# eldennet-engine's probe-exe, the first program to link en-gal and en-llm, in GCC's build).
target_compile_definitions(en_third_party INTERFACE TOML_EXCEPTIONS=0)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
include(cmake/sanitizers.cmake)  # EN_SANITIZE and EN_FUZZ, for Clang on Linux (ARCH §12.1)
include(cmake/embed_files.cmake)  # en_embed_files: text files built in (Augur's rule files)
include(cmake/link_rules.cmake)  # en_link_rule: ARCH §2.3's rules on the link graph, checked at configure time

# The modules the DLL links hold no static object with a dynamic initialiser (it would run under the loader lock) or
# with a destructor that runs at exit (it would run at DLL_PROCESS_DETACH): ARCH §3.1 and §1.7. Clang checks both.
function(en_dll_module target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE -Werror=exit-time-destructors -Werror=global-constructors)
  endif()
endfunction()

if(EN_BUILD_TESTS)
  enable_testing()
  add_library(en_doctest INTERFACE)
  target_include_directories(en_doctest SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest)
endif()

add_subdirectory(src/en-prompt)
add_subdirectory(src/en-proto)
add_subdirectory(src/en-llm)
add_subdirectory(src/en-augur)  # Augur, the event engine (part 11): below en-core, game-neutral
add_subdirectory(src/en-core)   # the core's v0.1 slice: the lifecycle, the gate, L1, L2 and Dormant (part 5)
# en-gal's portable part builds with every compiler; its SEH-guarded copy (__try/__except, ARCH §2.6) only with an
# MSVC-like one, the clang-cl build (cmake/clang-cl-xwin.cmake): src/en-gal/CMakeLists.txt chooses.
add_subdirectory(src/en-gal)
# en-overlay (part 12): GraceUI's ABI 1 through graceui-sys. Its pure part (the F11 view's lines, the world-state
# claims) builds everywhere; the connection, the notices, the hotkeys and the view on Dear ImGui's core in the Windows
# builds.
add_subdirectory(src/en-overlay)
add_subdirectory(src/eldennet)  # eldennet.dll (part 6) and its portable shell, en_shell, which en-tools' lint reads
add_subdirectory(src/en-tools)
add_subdirectory(src/en-launcher)  # eldennet-launcher and its crash watcher (part 7)
if(EN_FUZZ)
  add_subdirectory(fuzz)
endif()
add_subdirectory(tests/galtest)   # the GAL's test DLL, which the harness loads into the fake game (clang-cl)
add_subdirectory(tests/fakegame)  # the fake game and its Wine harness (Windows builds with the tests)
add_subdirectory(tests/vehprobe)  # the vectored-handler DLL the harness loads into it

# The link rules' own test (tests/linkrules): each case configures a small project of interface targets with the same
# cmake/link_rules.cmake; `clean` must configure, and each other case must stop with the chain that reaches en-core's
# stand-in. The real rules (src/en-launcher/CMakeLists.txt) are checked whenever this build is configured.
if(EN_BUILD_TESTS)
  set(_en_rules_cases
    "clean|"
    "through|launcher reaches shell \\(launcher -> shell\\)"
    "deep|launcher reaches core \\(launcher -> launcher_core -> gal -> core\\)"
    "link_only|launcher reaches core \\(launcher -> launcher_core -> core\\)"
    "condition|launcher reaches core \\(launcher -> core\\)"
    "alias|launcher reaches core \\(launcher -> core\\)")
  foreach(_en_rules_case IN LISTS _en_rules_cases)
    string(REGEX REPLACE "\\|.*" "" _en_case "${_en_rules_case}")
    string(REGEX REPLACE "^[^|]*\\|" "" _en_expect "${_en_rules_case}")
    # CMake wraps a long FATAL_ERROR message at a space, so a space in the pattern matches any run of white space.
    string(REPLACE " " "[ \t\r\n]+" _en_expect "${_en_expect}")
    add_test(NAME en_link_rules.${_en_case}
      COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/tests/linkrules
        -B ${CMAKE_CURRENT_BINARY_DIR}/linkrules/${_en_case} -DEN_CASE=${_en_case})
    if(_en_expect)
      set_tests_properties(en_link_rules.${_en_case} PROPERTIES PASS_REGULAR_EXPRESSION "${_en_expect}")
    else()
      set_tests_properties(en_link_rules.${_en_case} PROPERTIES FAIL_REGULAR_EXPRESSION "reaches")
    endif()
  endforeach()
endif()
