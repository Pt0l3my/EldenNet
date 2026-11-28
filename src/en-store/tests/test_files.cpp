// The playthrough files on disk (files.hpp; MEM §8.1 to §8.3): names, a key's lookup (the live file, a soft match
// renamed, an archived file restored, a new file with the slot's other character archived), the rotating backups, the
// migrations' last two, and a corrupt file set aside for the newest backup. Every file and key is invented.
#include <doctest.h>

#include <string>

#include "en/store/files.hpp"
#include "en/store/migrate.hpp"
#include "support.hpp"

using namespace en::store;
using en::store::test::TempDir;
namespace fs = std::filesystem;

namespace {

constexpr std::int64_t kWall = 1764201600;  // 2025-11-27T00:00:00Z

en::proto::PlaythroughKey key(std::uint8_t slot, std::string fp8, std::string name_hash, std::uint8_t archetype,
                              std::uint8_t gift) {
  en::proto::PlaythroughKey k;
  k.steamid64 = 76561190000000001ull;  // an invented account
  k.slot = slot;
  k.fp8 = std::move(fp8);
  k.name_hash = std::move(name_hash);
  k.archetype = archetype;
  k.gift = gift;
  return k;
}

// A playthrough file at `p` with its schema and the key's meta, and one row of its own to tell it apart.
void make_file(const fs::path& p, const en::proto::PlaythroughKey& k, const std::string& tag) {
  auto db = Db::open(p, FileKind::Playthrough, true);
  REQUIRE(db);
  REQUIRE(migrate(*db, playthrough_migrations(), {}));
  REQUIRE(meta_set(*db, "fp8", k.fp8));
  REQUIRE(meta_set(*db, "name_hash", k.name_hash));
  REQUIRE(meta_set(*db, "archetype", number_text(k.archetype)));
  REQUIRE(meta_set(*db, "gift", number_text(k.gift)));
  REQUIRE(meta_set(*db, "tag", tag));
}

std::string tag_of(const fs::path& p) {
  auto db = Db::open(p, FileKind::Playthrough, false);
  if (!db) return "<none>";
  return meta_get(*db, "tag").value_or(std::nullopt).value_or("<none>");
}

}  // namespace

TEST_CASE("files: names, folders and UTC stamps, with no clock and no locale") {
  const SaveFolder f = save_folder(fs::path("saves"), 76561190000000001ull);
  CHECK(f.dir == fs::path("saves") / "76561190000000001");
  CHECK(f.live(3, "0a1b2c3d").filename() == "pt-s3-0a1b2c3d.sqlite");
  CHECK(f.backups().filename() == "backups");
  CHECK(f.archive().filename() == "archive");
  CHECK(utc_stamp(0) == "19700101T000000Z");
  CHECK(utc_stamp(kWall) == "20251127T000000Z");
  CHECK(utc_stamp(951782400) == "20000229T000000Z");
  CHECK(utc_stamp(4102444799) == "20991231T235959Z");
}

TEST_CASE("files: the soft match is the name's hash and the archetype, with gifts equal or unread on either side") {
  FileIdentity file{"aaaaaaaa", "n1", 3, 0xFF};
  CHECK(soft_match(file, key(1, "bbbbbbbb", "n1", 3, 5)));
  file.gift = 5;
  CHECK(soft_match(file, key(1, "bbbbbbbb", "n1", 3, 5)));
  CHECK(soft_match(file, key(1, "bbbbbbbb", "n1", 3, 0xFF)));
  CHECK_FALSE(soft_match(file, key(1, "bbbbbbbb", "n1", 3, 6)));
  CHECK_FALSE(soft_match(file, key(1, "bbbbbbbb", "n2", 3, 5)));
  CHECK_FALSE(soft_match(file, key(1, "bbbbbbbb", "n1", 4, 5)));
  file.name_hash.clear();
  CHECK_FALSE(soft_match(file, key(1, "bbbbbbbb", "", 3, 5)));
}

TEST_CASE("files: a key finds its live file; else a new one is made, and the slot's other character is archived") {
  TempDir dir;
  const SaveFolder f = save_folder(dir.path(), 76561190000000001ull);
  const auto a = key(2, "aaaaaaaa", "na", 3, 1);
  auto first = locate(f, a, kWall);
  REQUIRE(first);
  CHECK(first->create);
  CHECK_FALSE(first->archived_other);
  make_file(first->path, a, "first");
  auto again = locate(f, a, kWall);
  REQUIRE(again);
  CHECK_FALSE(again->create);
  CHECK(again->path == first->path);
  // Another character in the same slot: a new file, the first archived (never deleted).
  const auto b = key(2, "bbbbbbbb", "nb", 7, 1);
  auto second = locate(f, b, kWall);
  REQUIRE(second);
  CHECK(second->create);
  CHECK(second->archived_other);
  CHECK_FALSE(fs::exists(first->path));
  CHECK(fs::exists(f.archive() / "pt-s2-aaaaaaaa-20251127T000000Z.sqlite"));
  make_file(second->path, b, "second");
  // The first character again: its archived file comes back, and the second goes to the archive.
  auto back = locate(f, a, kWall + 60);
  REQUIRE(back);
  CHECK(back->restored_archive);
  CHECK(back->archived_other);
  CHECK(tag_of(back->path) == "first");
  CHECK(fs::exists(f.archive() / "pt-s2-bbbbbbbb-20251127T000100Z.sqlite"));
  // Another slot is never touched.
  const auto c = key(5, "cccccccc", "nc", 1, 1);
  auto other_slot = locate(f, c, kWall);
  REQUIRE(other_slot);
  CHECK_FALSE(other_slot->archived_other);
  CHECK(fs::exists(back->path));
}

TEST_CASE("files: the same character under a new fingerprint is renamed while closed, with no WAL left behind") {
  TempDir dir;
  const SaveFolder f = save_folder(dir.path(), 76561190000000001ull);
  const auto old = key(4, "aaaaaaaa", "n1", 3, 0xFF);  // the gift was not read then
  REQUIRE(locate(f, old, kWall));
  make_file(f.live(4, "aaaaaaaa"), old, "mine");
  const auto now = key(4, "dddddddd", "n1", 3, 9);  // the layout reads it now: a new fingerprint
  auto r = locate(f, now, kWall);
  REQUIRE(r);
  CHECK(r->renamed);
  CHECK_FALSE(r->create);
  CHECK(r->path == f.live(4, "dddddddd"));
  CHECK_FALSE(fs::exists(f.live(4, "aaaaaaaa")));
  CHECK_FALSE(fs::exists(fs::path(f.live(4, "aaaaaaaa")).concat("-wal")));
  CHECK(tag_of(r->path) == "mine");
}

TEST_CASE("files: three rotating backups, the newest in .1; the migrations' last two; their count and times") {
  TempDir dir;
  const SaveFolder f = save_folder(dir.path(), 76561190000000001ull);
  const auto k = key(1, "eeeeeeee", "n", 1, 1);
  REQUIRE(locate(f, k, kWall));
  make_file(f.live(1, "eeeeeeee"), k, "v0");
  auto db = Db::open(f.live(1, "eeeeeeee"), FileKind::Playthrough, false);
  REQUIRE(db);
  for (int i = 1; i <= 4; ++i) {
    REQUIRE(meta_set(*db, "tag", "v" + std::to_string(i)));
    REQUIRE(rotate_backup(*db, f, 1, "eeeeeeee"));
  }
  CHECK(tag_of(f.backups() / "pt-s1-eeeeeeee.1.sqlite") == "v4");
  CHECK(tag_of(f.backups() / "pt-s1-eeeeeeee.2.sqlite") == "v3");
  CHECK(tag_of(f.backups() / "pt-s1-eeeeeeee.3.sqlite") == "v2");
  CHECK_FALSE(fs::exists(f.backups() / "pt-s1-eeeeeeee.4.sqlite"));
  const BackupInfo info = backups_of(f, 1, "eeeeeeee");
  CHECK(info.count == 3);
  CHECK(info.walls.size() == 3);
  CHECK(newest_backup(f, 1, "eeeeeeee") == f.backups() / "pt-s1-eeeeeeee.1.sqlite");
  for (std::uint32_t v = 1; v <= 3; ++v) {
    const auto p = pre_migration_backup(f, 1, "eeeeeeee", v, v + 1);
    REQUIRE(backup_into(*db, p));
  }
  std::size_t pre = 0;
  for (const auto& e : fs::directory_iterator(f.backups())) {
    if (e.path().filename().string().find(".pre-") != std::string::npos) ++pre;
  }
  CHECK(pre == 2);
  CHECK(fs::exists(f.backups() / "pt-s1-eeeeeeee.pre-3-4.sqlite"));
}

TEST_CASE("files: a corrupt file set aside, and the newest backup in its place") {
  TempDir dir;
  const SaveFolder f = save_folder(dir.path(), 76561190000000001ull);
  const auto k = key(0, "ffffffff", "n", 1, 1);
  REQUIRE(locate(f, k, kWall));
  make_file(f.live(0, "ffffffff"), k, "good");
  {
    auto db = Db::open(f.live(0, "ffffffff"), FileKind::Playthrough, false);
    REQUIRE(db);
    REQUIRE(rotate_backup(*db, f, 0, "ffffffff"));
  }
  test::write_file(f.live(0, "ffffffff"), std::string(8192, '\x7f'));
  CHECK_FALSE(Db::open(f.live(0, "ffffffff"), FileKind::Playthrough, false));
  REQUIRE(set_aside(f.live(0, "ffffffff"), kWall));
  CHECK(fs::exists(fs::path(f.live(0, "ffffffff")).concat(".corrupt-20251127T000000Z")));
  REQUIRE(restore_newest(f, 0, "ffffffff"));
  CHECK(tag_of(f.live(0, "ffffffff")) == "good");
  // A set-aside file is never taken for the slot's other character.
  auto again = locate(f, k, kWall);
  REQUIRE(again);
  CHECK_FALSE(again->archived_other);
}
