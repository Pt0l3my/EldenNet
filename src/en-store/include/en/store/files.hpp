#pragma once
// The playthrough files on disk (MEM §8.1 to §8.3): where each lives, which one a character's key finds, the rotating
// backups, the archive of an older character in a reused slot, and a corrupt file set aside. Paths are made here and
// never leave en-store: no result, log line or page names one (PV rules).
//
//   <saves>\<SteamID64>\pt-s<slot>-<fp8>.sqlite (+ -wal, -shm)        one playthrough, every journey
//   <saves>\<SteamID64>\backups\pt-s<slot>-<fp8>.<1..3>.sqlite        at each playthrough start, and before a forget
//   <saves>\<SteamID64>\backups\pt-s<slot>-<fp8>.pre-<from>-<to>.sqlite   before a migration: the last 2
//   <saves>\<SteamID64>\archive\pt-s<slot>-<fp8>-<utc>.sqlite         an older character in a reused slot
//   <live>.corrupt-<utc>                                              a file that read as corrupt, set aside
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "en/proto/store.hpp"
#include "en/store/db.hpp"

namespace en::store {

struct SaveFolder {
  std::filesystem::path dir;  // <saves>\<SteamID64>
  [[nodiscard]] std::filesystem::path backups() const { return dir / "backups"; }
  [[nodiscard]] std::filesystem::path archive() const { return dir / "archive"; }
  [[nodiscard]] std::filesystem::path live(std::uint8_t slot, std::string_view fp8) const;
};

[[nodiscard]] SaveFolder save_folder(const std::filesystem::path& saves, std::uint64_t steamid64);
// "pt-s<slot>-<fp8>"
[[nodiscard]] std::string file_stem(std::uint8_t slot, std::string_view fp8);
// A wall time (seconds since the epoch) as file names write it: YYYYMMDDTHHMMSSZ, in UTC.
[[nodiscard]] std::string utc_stamp(std::int64_t wall);

// What a key's lookup found (MEM §8.3), in its order:
// 1. the live file `pt-s<slot>-<fp8>`;
// 2. a live file in the same slot whose meta matches on the name's hash and the archetype, with the gifts equal or
//    0xFF on either side: renamed to the new fp8 while closed, after a TRUNCATE checkpoint, so no -wal is left;
// 3. an archived file with this fp8: restored, any other live file in the slot archived;
// 4. otherwise a new file, any other live file in the slot archived, never deleted.
struct Located {
  std::filesystem::path path;
  bool create = false;            // 4: no file yet
  bool renamed = false;           // 2
  bool restored_archive = false;  // 3
  bool archived_other = false;    // 3 or 4: another character's file moved to the archive
};
[[nodiscard]] Result<Located> locate(const SaveFolder& folder, const proto::PlaythroughKey& key, std::int64_t wall);

// The meta a soft match reads (MEM §8.3): never the name, only its hash.
struct FileIdentity {
  std::string fp8, name_hash;
  std::optional<std::uint8_t> archetype, gift;
};
[[nodiscard]] Result<FileIdentity> identity_of(const Db& db);

// MEM §8.3's soft match: the name's hash and the archetype equal; the gifts equal, or 0xFF on either side.
[[nodiscard]] bool soft_match(const FileIdentity& file, const proto::PlaythroughKey& key) noexcept;

// The rotating backups (MEM §8.1): `.3` dropped, `.2` → `.3`, `.1` → `.2`, then a consistent copy of the open file into
// `.1`. A playthrough start's and a forget's alike.
[[nodiscard]] Result<void> rotate_backup(const Db& db, const SaveFolder& folder, std::uint8_t slot,
                                         std::string_view fp8);
// The path for the backup before a migration from `from` to `to`, the older ones of the file beyond the last
// `keep - 1` removed first, so that the copy made makes `keep` (MEM §8.1: the last 2).
[[nodiscard]] std::filesystem::path pre_migration_backup(const SaveFolder& folder, std::uint8_t slot,
                                                         std::string_view fp8, std::uint32_t from, std::uint32_t to,
                                                         std::size_t keep = 2);
// The newest rotating backup of this file, if any (`.1`, else `.2`, else `.3`).
[[nodiscard]] std::optional<std::filesystem::path> newest_backup(const SaveFolder& folder, std::uint8_t slot,
                                                                 std::string_view fp8);
// The backups' count and times, for the Memories page's section (no path; PAGES §4.6.5).
struct BackupInfo {
  std::size_t count = 0;
  std::vector<std::int64_t> walls;  // newest first, seconds since the epoch
};
[[nodiscard]] BackupInfo backups_of(const SaveFolder& folder, std::uint8_t slot, std::string_view fp8);

// A file that read as corrupt, renamed aside with its -wal and -shm (`*.corrupt-<utc>`), so the next open restores the
// newest backup (MEM §8.6).
[[nodiscard]] bool set_aside(const std::filesystem::path& live, std::int64_t wall);
// The newest backup copied into the live file's place; false when there is none or it cannot be copied.
[[nodiscard]] bool restore_newest(const SaveFolder& folder, std::uint8_t slot, std::string_view fp8);
// A live file moved to the archive while closed: its WAL emptied into it first (a TRUNCATE checkpoint).
[[nodiscard]] bool archive_file(const SaveFolder& folder, const std::filesystem::path& live, std::int64_t wall);

// MEM §8.6: under 200 MB free at open is a warning.
inline constexpr std::uint64_t kLowDiskBytes = 200ull * 1024 * 1024;

}  // namespace en::store
