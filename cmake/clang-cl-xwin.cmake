# Cross-compile for Windows x64 with MSVC's ABI on Linux: clang-cl and lld-link from LLVM 18, against the MSVC CRT and
# the Windows SDK that xwin splats outside every repository (O60, ARCH §2.6). cmake/xwin.lock pins their versions, and
# this file refuses a splat that differs. Microsoft's licence covers those files: they are never committed or copied
# into a repository. CMakeLists.txt adds the project's own warning and conformance flags (/W4 /permissive- /utf-8).
#
#   xwin --accept-license --manifest-version 17 --channel release --arch x86_64 --variant desktop \
#        --sdk-version 10.0.26100 --crt-version 14.43.17.13 splat --output ~/.cache/xwin
#   cmake -S . -B build-cl -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-xwin.cmake -DEN_WITH_CURL=OFF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER clang-cl-18)
set(CMAKE_CXX_COMPILER clang-cl-18)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_LINKER lld-link-18)
set(CMAKE_AR llvm-lib-18)
set(CMAKE_RC_COMPILER llvm-rc-18)
set(CMAKE_MT llvm-mt-18)
set(CMAKE_OBJDUMP llvm-objdump-18)  # en-llm's check that no test program imports winhttp.dll by name

# The splat: ~/.cache/xwin unless XWIN_ROOT says otherwise, in the environment or the cache. try_compile's projects
# read this file again, and get the same root.
if(DEFINED ENV{XWIN_ROOT})
  set(_en_xwin_root "$ENV{XWIN_ROOT}")
else()
  set(_en_xwin_root "$ENV{HOME}/.cache/xwin")
endif()
set(XWIN_ROOT "${_en_xwin_root}" CACHE PATH "The xwin splat of the MSVC CRT and the Windows SDK (cmake/xwin.lock)")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XWIN_ROOT)

# The pin: the splat's CRT (its crtversion.h) and SDK (the folder xwin names after it) must be the lock's.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/xwin.lock" _en_lock REGEX "^(crt|sdk) = \"[0-9.]+\"")
foreach(_en_line IN LISTS _en_lock)
  if(_en_line MATCHES "^(crt|sdk) = \"([0-9.]+)\"")
    set(_en_pin_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endif()
endforeach()
string(REGEX MATCH "^[0-9]+\\.[0-9]+" _en_pin_crt_minor "${_en_pin_crt}")
set(_en_found_crt "none")
if(EXISTS "${XWIN_ROOT}/crt/include/crtversion.h")
  file(STRINGS "${XWIN_ROOT}/crt/include/crtversion.h" _en_crt REGEX "^#define _VC_CRT_(MAJOR|MINOR)_VERSION [0-9]+")
  list(TRANSFORM _en_crt REPLACE "^#define _VC_CRT_[A-Z]+_VERSION ([0-9]+).*" "\\1")
  list(JOIN _en_crt "." _en_found_crt)
endif()
if(NOT _en_found_crt STREQUAL _en_pin_crt_minor OR NOT IS_DIRECTORY "${XWIN_ROOT}/sdk/include/${_en_pin_sdk}")
  message(FATAL_ERROR "XWIN_ROOT (${XWIN_ROOT}) is not the splat that cmake/xwin.lock pins: CRT ${_en_pin_crt} and "
    "SDK ${_en_pin_sdk} (its CRT: ${_en_found_crt}). Splat them with the command at the top of this file.")
endif()

# The CRT's and SDK's headers as system headers, for the compiler of the CRT's own toolset (14.43 is 19.43), and
# nothing from the environment (%INCLUDE% and %LIB%). Paths in debug information and __FILE__ are relative to the
# source tree (ARCH §2.6).
string(REGEX REPLACE "^14\\.([0-9]+).*" "19.\\1" _en_msc_version "${_en_pin_crt}")
get_filename_component(_en_source_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_en_cl_flags "/X -fms-compatibility-version=${_en_msc_version} /clang:-ffile-prefix-map=${_en_source_root}/=")
foreach(_en_dir IN ITEMS crt/include sdk/include/ucrt sdk/include/shared sdk/include/um)
  string(APPEND _en_cl_flags " -imsvc${XWIN_ROOT}/${_en_dir}")
endforeach()
set(_en_link_flags "/lldignoreenv")
foreach(_en_dir IN ITEMS crt/lib/x86_64 sdk/lib/um/x86_64 sdk/lib/ucrt/x86_64)
  string(APPEND _en_link_flags " /libpath:${XWIN_ROOT}/${_en_dir}")
endforeach()

# ARCH §2.6. The static CRT (/MT) in every configuration, so that no program needs the Visual C++ runtime (and the
# splat holds no debug CRT). /EHsc: C++ exceptions on, which R3's ffi_guard depends on; /WX: no warning passes;
# /Brepro: no timestamps; release builds with ThinLTO and line tables, and a PDB beside each binary, which each binary
# names by its file name alone (/pdbaltpath:%_PDB%), so no build machine's folder is carried in it (WHY§118); no
# incremental link. CMake adds /O2 /Ob2 /DNDEBUG to release builds itself.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
set(CMAKE_C_FLAGS_INIT "${_en_cl_flags} /WX /Brepro")
set(CMAKE_CXX_FLAGS_INIT "${_en_cl_flags} /EHsc /WX /Brepro")
set(CMAKE_C_FLAGS_RELEASE_INIT "-flto=thin /Z7 -gline-tables-only")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-flto=thin /Z7 -gline-tables-only")
foreach(_en_kind IN ITEMS EXE SHARED MODULE)
  set(CMAKE_${_en_kind}_LINKER_FLAGS_INIT "${_en_link_flags} /Brepro /INCREMENTAL:NO")
  set(CMAKE_${_en_kind}_LINKER_FLAGS_RELEASE_INIT "/DEBUG /pdbaltpath:%_PDB%")
endforeach()
