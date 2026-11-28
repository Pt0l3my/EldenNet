# Cross-compile for Windows x64 with MinGW-w64. The runtime (libstdc++, libgcc) is linked statically, so the
# programs need no MinGW DLLs on a player's PC; Wine runs the tests here (S8.1.g).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)  # SQLite's amalgamation, the one C target (cmake/sqlite.cmake)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static")
