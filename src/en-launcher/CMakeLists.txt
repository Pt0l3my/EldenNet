# en-launcher: eldennet-launcher.exe, our own launcher and crash watcher (ARCH §4, O5, O71; the first in-game build,
# part 7). Two parts:
# - en_launcher_core, portable and tested on Linux: Steam's KeyValues and the game's discovery, the save backups with
#   their BLAKE3 manifests, GraceUI's pre-flight (graceui.toml, with our own SHA-256), the appid file and the
#   launcher's state, launcher.toml, the command line, the texts, the watcher's records, the S0 spikes' tally, and the
#   runs' folders and the game's environment (S0-E, S4, and S0-A to S0-D), the S0 kind's tasks, words and the facts of
#   eldennet.dll's report of an S0 run (s0.hpp), and the console's bounded queue of lines (WHY§290).
# - eldennet-launcher (Windows builds): processes, injection, the watch block, the dev console and the Mark (src/win/).
# It depends on en-gal's offline part only (the PE reader, builds.toml's gate and the reports' vocabularies; ARCH §2.3),
# and through it on en-proto, whose watch block it shares with EldenNet in the game: never on en-core, en_shell or any
# other module of ours, which the link rules below refuse when the build is configured.
set(EN_LAUNCHER_CORE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/blake3.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sha256.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/graceui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/vdf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/steam.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/backup.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/appid.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/texts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/record.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/spikes.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/s0.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/observer_tasks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/exe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lines.cpp)
add_library(en_launcher_core STATIC ${EN_LAUNCHER_CORE_SOURCES})
target_include_directories(en_launcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_launcher_core PUBLIC en_gal en_proto en_third_party)

if(WIN32)
  enable_language(RC)
  file(GLOB EN_LAUNCHER_WIN_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/win/*.cpp)
  # The manifest: the resource script embeds launcher.manifest as it stands, in both toolchains. lld-link's own merge
  # (/MANIFESTINPUT) rewrote its trustInfo with namespaced attributes (ms_asmv1:level), which Windows refuses with a
  # side-by-side error while Wine accepts them; so lld-link makes no manifest of its own (/MANIFEST:NO).
  set(EN_LAUNCHER_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/src/win/launcher.manifest)
  set(EN_LAUNCHER_MANIFEST_RC "1 24 \"${EN_LAUNCHER_MANIFEST}\"")
  set(_en_manifest_source "")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/win/launcher.rc.in ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc @ONLY)
  set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc APPEND PROPERTY OBJECT_DEPENDS ${EN_LAUNCHER_MANIFEST})
  add_executable(eldennet-launcher WIN32 ${EN_LAUNCHER_WIN_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc
    ${_en_manifest_source})
  # The report's vocabulary, with which the launcher lints the read-only round's report after its session (--read), is
  # en-gal's offline part's (lodestar.hpp), which en_launcher_core brings.
  target_link_libraries(eldennet-launcher PRIVATE en_launcher_core comdlg32 shell32 ole32 advapi32 iphlpapi)
  if(MSVC)
    target_link_options(eldennet-launcher PRIVATE /MANIFEST:NO)
  endif()
  target_compile_definitions(eldennet-launcher PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00
    EN_LAUNCHER_VERSION="${EN_VERSION_LABEL}")
  if(NOT MSVC)
    target_link_options(eldennet-launcher PRIVATE -municode)  # MinGW: the wide entry point, wWinMain
  endif()
endif()

# ARCH §2.3: the launcher and its core link en-gal's offline part, en-proto and the header-only libraries, and no other
# target of the build (cmake/link_rules.cmake). en_gal_game, the in-game part, is not among them either.
en_link_rule(en_launcher_core ALLOW en_gal en_proto en_third_party)
en_link_rule(eldennet-launcher ALLOW en_launcher_core en_gal en_proto en_third_party)

if(EN_BUILD_TESTS)
  file(GLOB EN_LAUNCHER_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  # en-gal's invented-image builder, for the gate's and the DLL check's tests.
  add_executable(en_launcher_tests ${EN_LAUNCHER_TESTS} ${PROJECT_SOURCE_DIR}/src/en-gal/tests/fake_image.cpp)
  target_include_directories(en_launcher_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/en-gal/tests)
  target_link_libraries(en_launcher_tests PRIVATE en_launcher_core en_doctest)
  add_test(NAME en_launcher_tests COMMAND en_launcher_tests)
  # tests/graceui-manifest.toml: the graceui.toml GraceUI's tools/release/manifest.py wrote (GraceUI D165; at 5702fae,
  # GraceUI 0.2.70) for an invented GraceUI folder of ours, compiled into the tests byte for byte, so that every build
  # reads the committed file itself, under Wine too, with no path to it.
  set(_en_fixture ${CMAKE_CURRENT_SOURCE_DIR}/tests/graceui-manifest.toml)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_en_fixture})
  file(READ ${_en_fixture} _en_bytes HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _en_bytes "${_en_bytes}")
  file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fixtures/graceui_manifest.inc CONTENT "${_en_bytes}\n" @ONLY)
  target_include_directories(en_launcher_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
endif()
