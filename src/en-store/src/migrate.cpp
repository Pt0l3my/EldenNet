// The migration runner (migrate.hpp; MEM §10).
#include "en/store/migrate.hpp"

#include <array>
#include <cctype>

namespace en::store {

std::vector<Migration> playthrough_migrations() {
  std::vector<Migration> out;
  for (const EmbeddedSql& f : playthrough_sql()) {
    if (f.name.size() < 6 || f.name[4] != '_') continue;
    std::uint32_t n = 0;
    bool digits = true;
    for (std::size_t i = 0; i < 4; ++i) {
      if (!std::isdigit(static_cast<unsigned char>(f.name[i]))) digits = false;
      n = n * 10 + static_cast<std::uint32_t>(f.name[i] - '0');
    }
    if (!digits || n != out.size() + 1) continue;
    out.push_back(Migration{n, f.name, f.text});
  }
  return out;
}

std::uint32_t top_of(std::span<const Migration> set) noexcept { return set.empty() ? 0 : set.back().n; }

std::string_view migration_lint(std::string_view sql) {
  // Transaction control as a statement of its own: the first word of a top-level statement. A trigger's body (CREATE
  // TRIGGER ... BEGIN ... END) holds statements of its own, and its BEGIN and END are not transaction control.
  static constexpr std::array<std::string_view, 6> kStatements = {"BEGIN", "COMMIT", "END", "ROLLBACK", "SAVEPOINT",
                                                                  "RELEASE"};
  const auto word_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
  bool first = true;          // the next word starts a top-level statement
  bool trigger = false;       // the statement is a CREATE TRIGGER
  int body = 0;               // inside a trigger's BEGIN ... END
  bool create = false;
  std::size_t i = 0;
  while (i < sql.size()) {
    const char c = sql[i];
    if (c == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {  // a line comment
      while (i < sql.size() && sql[i] != '\n') ++i;
      continue;
    }
    if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {  // a block comment
      const std::size_t end = sql.find("*/", i + 2);
      i = end == std::string_view::npos ? sql.size() : end + 2;
      continue;
    }
    if (c == '\'' || c == '"' || c == '`' || c == '[') {  // a quoted string or name: its quote doubled inside
      const char close = c == '[' ? ']' : c;
      ++i;
      while (i < sql.size()) {
        if (sql[i] == close) {
          if (close != ']' && i + 1 < sql.size() && sql[i + 1] == close) {
            i += 2;
            continue;
          }
          ++i;
          break;
        }
        ++i;
      }
      first = false;
      continue;
    }
    if (c == ';') {
      if (body == 0) {
        first = true;
        trigger = false;
        create = false;
      }
      ++i;
      continue;
    }
    if (word_char(c)) {
      const std::size_t start = i;
      while (i < sql.size() && word_char(sql[i])) ++i;
      std::string up(sql.substr(start, i - start));
      for (char& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      if (up == "JOURNAL_MODE") return "journal_mode";
      if (first && body == 0) {
        for (const std::string_view w : kStatements) {
          if (up == w) return w;
        }
        create = up == "CREATE";
      } else if (create && up == "TRIGGER") {
        trigger = true;
      } else if (trigger && up == "BEGIN") {
        ++body;
      } else if (body > 0 && up == "END") {
        --body;
      }
      first = false;
      continue;
    }
    ++i;
  }
  return {};
}

Result<MigrateOutcome> migrate(const Db& db, std::span<const Migration> set, const std::filesystem::path& backup) {
  auto cur = user_version(db);
  if (!cur) return Unexpected{cur.error()};
  MigrateOutcome out;
  out.from = *cur;
  out.to = *cur;
  const std::uint32_t top = top_of(set);
  if (*cur > top) {
    // A newer file: opened only if this build may read it (MEM §10).
    auto min = meta_get(db, "schema_min_reader");
    if (!min) return Unexpected{min.error()};
    const auto n = *min ? number_of(**min) : std::nullopt;
    if (!n || *n < 0 || static_cast<std::uint64_t>(*n) > top) return Unexpected{Fault::TooNew};
    out.newer = true;
    return out;
  }
  if (*cur == top) return out;
  if (*cur > 0 && !backup.empty()) {
    if (auto r = backup_into(db, backup); !r) return Unexpected{r.error()};
    out.backed_up = true;
  }
  for (const Migration& m : set) {
    if (m.n <= *cur) continue;
    auto tx = Transaction::exclusive(db);
    if (!tx) return Unexpected{tx.error()};
    if (auto r = execute_batch(db, m.sql); !r) return Unexpected{r.error()};
    if (auto r = set_user_version(db, m.n); !r) return Unexpected{r.error()};
    if (m.n == 1) {
      if (auto r = meta_set(db, "app", "eldennet.playthrough"); !r) return Unexpected{r.error()};
      if (auto r = meta_set(db, "schema_min_reader", number_text(kSchemaMinReader)); !r) return Unexpected{r.error()};
    }
    if (auto r = tx->commit(); !r) return Unexpected{r.error()};
    out.to = m.n;
  }
  return out;
}

}  // namespace en::store
