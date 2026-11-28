# en-store: the SQLite files, their migrations, backups and the store's request loop on T9 (ARCH §2.2, §1.2; MEM §8 to
# §10). It depends on en-proto and SQLite alone (ARCH §2.3): never on en-core, en-gal, en-overlay or en-llm. The DLL
# links it (en_dll_module: no static object with a dynamic initialiser or an exit-time destructor).
#
# The migrations (src/migrations/pt/NNNN_name.sql) are built in (cmake/embed_files.cmake), so no file is read for them
# at run time, in game or under Wine.
file(GLOB EN_STORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB EN_STORE_PT_SQL CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/migrations/pt/*.sql)
list(SORT EN_STORE_PT_SQL)
set(_en_pt_sql ${CMAKE_CURRENT_BINARY_DIR}/pt_migrations.cpp)
en_embed_files(${_en_pt_sql} INCLUDE en/store/migrate.hpp NAMESPACE en::store TYPE EmbeddedSql
  FUNCTION playthrough_sql FILES ${EN_STORE_PT_SQL})
add_library(en_store STATIC ${EN_STORE_SOURCES} ${_en_pt_sql})
en_dll_module(en_store)
target_include_directories(en_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(en_store PUBLIC en_proto PRIVATE en_sqlite)
# The system's threads (ARCH §2.3: the standard library's threads), which SQLite links on Linux, are no module of ours.
en_link_rule(en_store ALLOW en_proto en_sqlite en_third_party Threads::Threads)

if(EN_BUILD_TESTS)
  file(GLOB EN_STORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_executable(en_store_tests ${EN_STORE_TESTS})
  target_include_directories(en_store_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(en_store_tests PRIVATE en_store en_sqlite en_proto en_doctest)
  add_test(NAME en_store_tests COMMAND en_store_tests)
endif()
