# en-store: the SQLite files, their migrations, backups and the store's request loop on T9 (ARCH §2.2, §1.2; MEM §8 to
# §10). It depends on en-proto and SQLite alone (ARCH §2.3): never on en-core, en-gal, en-overlay or en-llm.
if(EN_BUILD_TESTS)
  file(GLOB EN_STORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_store_tests ${EN_STORE_TESTS})
  target_link_libraries(en_store_tests PRIVATE en_sqlite en_proto en_doctest)
  add_test(NAME en_store_tests COMMAND en_store_tests)
endif()
