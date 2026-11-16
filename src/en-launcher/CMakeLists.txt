# en-launcher: eldennet-launcher.exe, our own launcher and crash watcher (ARCH §4, O5, O71; the first in-game build,
# part 7). Two parts:
# - en_launcher_core, portable and tested on Linux: Steam's KeyValues and the game's discovery, the save backups with
#   their BLAKE3 manifests, the appid file and the launcher's state, launcher.toml, the command line, the texts, the
#   watcher's records and the S0 spikes' tally.
# - eldennet-launcher (Windows builds): processes, injection, the watch block, the dev console and the Mark (src/win/).
# It depends on en-gal's offline part only (the PE reader and builds.toml's gate; ARCH §2.3), and through it on
# en-proto, whose watch block it shares with EldenNet in the game.
set(EN_LAUNCHER_CORE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/blake3.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/vdf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/steam.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/backup.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/appid.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/texts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/record.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/exe.cpp)
add_library(en_launcher_core STATIC ${EN_LAUNCHER_CORE_SOURCES})
target_include_directories(en_launcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_launcher_core PUBLIC en_gal en_proto en_third_party)

if(WIN32)
  enable_language(RC)
  file(GLOB EN_LAUNCHER_WIN_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/win/*.cpp)
  # The manifest: lld-link embeds a .manifest source itself (CMake passes /MANIFESTINPUT); for MinGW, the resource
  # script carries it.
  set(EN_LAUNCHER_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/src/win/launcher.manifest)
  if(MSVC)
    set(EN_LAUNCHER_MANIFEST_RC "")
    set(_en_manifest_source ${EN_LAUNCHER_MANIFEST})
  else()
    set(EN_LAUNCHER_MANIFEST_RC "1 24 \"${EN_LAUNCHER_MANIFEST}\"")
    set(_en_manifest_source "")
  endif()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/win/launcher.rc.in ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc @ONLY)
  set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc APPEND PROPERTY OBJECT_DEPENDS ${EN_LAUNCHER_MANIFEST})
  add_executable(eldennet-launcher WIN32 ${EN_LAUNCHER_WIN_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc
    ${_en_manifest_source})
  target_link_libraries(eldennet-launcher PRIVATE en_launcher_core comdlg32 shell32 ole32 advapi32 iphlpapi)
  target_compile_definitions(eldennet-launcher PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00
    EN_LAUNCHER_VERSION="${PROJECT_VERSION}")
  if(NOT MSVC)
    target_link_options(eldennet-launcher PRIVATE -municode)  # MinGW: the wide entry point, wWinMain
  endif()
endif()

if(EN_BUILD_TESTS)
  file(GLOB EN_LAUNCHER_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  # en-gal's invented-image builder, for the gate's and the DLL check's tests.
  add_executable(en_launcher_tests ${EN_LAUNCHER_TESTS} ${PROJECT_SOURCE_DIR}/src/en-gal/tests/fake_image.cpp)
  target_include_directories(en_launcher_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/en-gal/tests)
  target_link_libraries(en_launcher_tests PRIVATE en_launcher_core en_doctest)
  add_test(NAME en_launcher_tests COMMAND en_launcher_tests)
endif()
