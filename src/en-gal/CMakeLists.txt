# en-gal: the game access layer (ARCH §6). For now it holds GuardedReader's guarded copy alone (ARCH §6.5, DESIGN R4):
# one structured exception handler around a copy, which needs an MSVC-like compiler's __try (the top-level
# CMakeLists.txt adds this module for one only). It is a module the DLL links, and it knows nothing of the game.
file(GLOB EN_GAL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(en_gal STATIC ${EN_GAL_SOURCES})
en_dll_module(en_gal)
target_include_directories(en_gal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(EN_BUILD_TESTS)
  file(GLOB EN_GAL_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_gal_tests ${EN_GAL_TESTS})
  target_link_libraries(en_gal_tests PRIVATE en_gal en_doctest)
  add_test(NAME en_gal_tests COMMAND en_gal_tests)
endif()
