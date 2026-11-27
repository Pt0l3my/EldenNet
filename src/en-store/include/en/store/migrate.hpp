#pragma once
// The migration runner (MEM §10): numbered SQL files built into the program, applied forward only, one transaction
// each, `PRAGMA user_version` the file's version and `meta.schema_min_reader` the oldest build that may read it. A
// backup of the file is taken before the first migration that changes it.
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/store/db.hpp"

namespace en::store {

struct Migration {
  std::uint32_t n = 0;    // 1, 2, ... with no gap
  std::string_view name;  // the file's name, for the tests' messages
  std::string_view sql;
};

// A text file built in (cmake/embed_files.cmake): its name and text.
struct EmbeddedSql {
  std::string_view name;
  std::string_view text;
};
[[nodiscard]] std::span<const EmbeddedSql> playthrough_sql() noexcept;  // src/migrations/pt/*.sql, in name order

// The playthrough file's migrations, from the built-in files, in order: each file's number is its name's first four
// digits (NNNN_name.sql). A file that breaks the numbering is left out, and the tests refuse it.
[[nodiscard]] std::vector<Migration> playthrough_migrations();

// The lint on a migration's text (MEM §10): no transaction control and no journal mode inside it. Answers the first
// word refused: BEGIN, COMMIT, END, ROLLBACK, SAVEPOINT or RELEASE starting a statement (a trigger's own BEGIN and END
// are allowed), or journal_mode anywhere; nothing when it is clean. Comments and quoted strings are skipped, so a word
// inside them is never refused.
[[nodiscard]] std::string_view migration_lint(std::string_view sql);

struct MigrateOutcome {
  std::uint32_t from = 0, to = 0;
  bool newer = false;  // the file is newer than this build and was opened read-compatible (schema_min_reader ≤ top)
  bool backed_up = false;
};

// Brings the file to the set's top version. A newer file opens only when its `schema_min_reader` is at most the top
// (else TooNew). Before the first migration, `backup` (a path that must not exist) receives a copy of the file; a
// failed migration leaves the file at its previous version. `backup` empty: no copy (a new, empty file).
[[nodiscard]] Result<MigrateOutcome> migrate(const Db& db, std::span<const Migration> set,
                                             const std::filesystem::path& backup);

// The top version of a set (0 for none).
[[nodiscard]] std::uint32_t top_of(std::span<const Migration> set) noexcept;

// The version this build writes as `schema_min_reader` (MEM §10's promise: v1.x migrations are additive, so it stays
// at v1.0's).
inline constexpr std::uint32_t kSchemaMinReader = 1;

}  // namespace en::store
