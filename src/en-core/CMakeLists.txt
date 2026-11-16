# en-core: the engine's core (ARCH §2.2). Its v0.1 slice (the first in-game build, part 5): the lifecycle without the
# store (ARCH §9), the offline and multiplayer gate with its hysteresis (ARCH §5.1), and the ladder's L1 and L2 with
# Dormant (ARCH §6.8). Pure: no I/O, no threads, no clocks, no Win32 (ARCH §2.3). The DLL links it (en_dll_module).
file(GLOB EN_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(en_core STATIC ${EN_CORE_SOURCES})
en_dll_module(en_core)
target_include_directories(en_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_core PUBLIC en_proto)

if(EN_BUILD_TESTS)
  file(GLOB EN_CORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_core_tests ${EN_CORE_TESTS})
  target_link_libraries(en_core_tests PRIVATE en_core en_doctest)
  add_test(NAME en_core_tests COMMAND en_core_tests)
endif()
