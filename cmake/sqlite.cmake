# SQLite 3.54.1's amalgamation (O146; MEM §8.5, ARCH §2.3): public domain, from SQLite's own site, pinned by the
# SHA3-256 its download page publishes and by our own SHA-256 of the verified file, fetched at configure time into the
# build tree and never into the repository. Only en-store links it (ARCH §2.3), through its C API.
#
# It is the one C target of the build: the amalgamation is compiled with its warnings off (third-party code, not held
# to our warnings), with the build's runtime (the static CRT in the Windows builds) and the options below, which
# en_store_tests asserts one by one (MEM §8.5: FTS5, which the reserved `memories_fts` needs).
enable_language(C)

set(EN_SQLITE_VERSION 3.54.1)
set(EN_SQLITE_ZIP sqlite-amalgamation-3540100.zip)
set(EN_SQLITE_SHA3_256 92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30)
set(EN_SQLITE_SHA256 0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9)
set(EN_SQLITE_DIR ${EN_DEPS}/sqlite-3540100)

if(NOT EXISTS ${EN_SQLITE_DIR}/sqlite3.c OR NOT EXISTS ${EN_SQLITE_DIR}/sqlite3.h)
  set(_en_zip ${EN_DEPS}/${EN_SQLITE_ZIP})
  if(EXISTS ${_en_zip})
    file(SHA3_256 ${_en_zip} _en_have)
    if(NOT _en_have STREQUAL EN_SQLITE_SHA3_256)
      file(REMOVE ${_en_zip})
    endif()
  endif()
  if(NOT EXISTS ${_en_zip})
    file(DOWNLOAD https://www.sqlite.org/2026/${EN_SQLITE_ZIP} ${_en_zip} TLS_VERIFY ON STATUS _en_st)
    list(GET _en_st 0 _en_code)
    if(NOT _en_code EQUAL 0)
      file(REMOVE ${_en_zip})
      message(FATAL_ERROR "Could not download SQLite's amalgamation: ${_en_st}")
    endif()
  endif()
  file(SHA3_256 ${_en_zip} _en_sha3)
  file(SHA256 ${_en_zip} _en_sha256)
  if(NOT _en_sha3 STREQUAL EN_SQLITE_SHA3_256 OR NOT _en_sha256 STREQUAL EN_SQLITE_SHA256)
    file(REMOVE ${_en_zip})
    message(FATAL_ERROR
      "${EN_SQLITE_ZIP} does not have its pinned hashes (SHA3-256 ${_en_sha3}, SHA-256 ${_en_sha256})")
  endif()
  set(_en_tmp ${EN_DEPS}/sqlite-extract)
  file(REMOVE_RECURSE ${_en_tmp})
  file(ARCHIVE_EXTRACT INPUT ${_en_zip} DESTINATION ${_en_tmp}
    PATTERNS sqlite-amalgamation-3540100/sqlite3.c sqlite-amalgamation-3540100/sqlite3.h)
  file(MAKE_DIRECTORY ${EN_SQLITE_DIR})
  file(RENAME ${_en_tmp}/sqlite-amalgamation-3540100/sqlite3.c ${EN_SQLITE_DIR}/sqlite3.c)
  file(RENAME ${_en_tmp}/sqlite-amalgamation-3540100/sqlite3.h ${EN_SQLITE_DIR}/sqlite3.h)
  file(REMOVE_RECURSE ${_en_tmp})
endif()

add_library(en_sqlite STATIC ${EN_SQLITE_DIR}/sqlite3.c)
target_include_directories(en_sqlite SYSTEM PUBLIC ${EN_SQLITE_DIR})
# The options (ours, beside MEM §8.5's FTS5): no extension loading inside the game's process; no double-quoted string
# literals; the schema untrusted; foreign keys on and WAL's NORMAL sync by default (the store sets both pragmas too);
# no memory statistics, deprecated interfaces or shared cache; LIKE never matches a blob. The serialised threading mode
# stays the default: T9 alone owns each connection, and a test's second connection is safe.
target_compile_definitions(en_sqlite PUBLIC
  SQLITE_ENABLE_FTS5=1
  SQLITE_OMIT_LOAD_EXTENSION=1
  SQLITE_DQS=0
  SQLITE_TRUSTED_SCHEMA=0
  SQLITE_DEFAULT_FOREIGN_KEYS=1
  SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
  SQLITE_DEFAULT_MEMSTATUS=0
  SQLITE_OMIT_DEPRECATED=1
  SQLITE_OMIT_SHARED_CACHE=1
  SQLITE_LIKE_DOESNT_MATCH_BLOBS=1
  SQLITE_THREADSAFE=1)
if(MSVC)
  target_compile_options(en_sqlite PRIVATE /w)
else()
  target_compile_options(en_sqlite PRIVATE -w)
  set_target_properties(en_sqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(en_sqlite PUBLIC Threads::Threads m)
endif()
