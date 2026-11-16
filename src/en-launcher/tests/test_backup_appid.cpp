// Save backups with invented save folders and files; the appid file and the launcher's record of it; launcher.toml.
#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>

#include "en/launcher/appid.hpp"
#include "en/launcher/backup.hpp"
#include "en/launcher/blake3.hpp"
#include "en/launcher/config.hpp"
#include "en/launcher/steam.hpp"
#include "support.hpp"

using namespace en::launcher;
using en::launcher::test::read_file;
using en::launcher::test::TempDir;
using en::launcher::test::write_file;
namespace fs = std::filesystem;

namespace {

// Two invented save folders (17-digit names), one with both files, one with the first only; and decoys: a folder that
// is no SteamID, a file at the top.
constexpr const char* kIdA = "76500000000000042";
constexpr const char* kIdB = "76500000000000077";

BackupSpec spec_in(const TempDir& dir, const std::string& stamp, std::size_t keep = 10) {
  return BackupSpec{dir.path() / "Saves", dir.path() / "Backups", {"SLOT0.sav", "SLOT0.sav.bak"}, keep, stamp};
}

void make_saves(const TempDir& dir, const std::string& a = "save-a", const std::string& b = "save-b") {
  write_file(dir.path() / "Saves" / kIdA / "SLOT0.sav", a);
  write_file(dir.path() / "Saves" / kIdA / "SLOT0.sav.bak", a + "-bak");
  write_file(dir.path() / "Saves" / kIdB / "SLOT0.sav", b);
  write_file(dir.path() / "Saves" / "not-an-id" / "SLOT0.sav", "decoy");
  write_file(dir.path() / "Saves" / "GraphicsConfig.xml", "decoy");
}

std::map<std::string, std::string> listing(const fs::path& root) {
  std::map<std::string, std::string> out;
  for (const auto& e : fs::recursive_directory_iterator(root)) {
    out[path_to_utf8(fs::relative(e.path(), root))] = e.is_regular_file() ? read_file(e.path()) : "<dir>";
  }
  return out;
}

}  // namespace

TEST_CASE("backups: each SteamID folder's files copied with a manifest of sizes and BLAKE3 hashes") {
  TempDir dir;
  make_saves(dir);
  const auto before = listing(dir.path() / "Saves");
  const BackupReport r = back_up_saves(spec_in(dir, "20261116-101500"));
  CHECK(r.ok());
  CHECK(r.folders == 2);
  CHECK(r.made == 2);
  CHECK(r.unchanged == 0);
  const fs::path a = dir.path() / "Backups" / kIdA / "20261116-101500";
  CHECK(read_file(a / "SLOT0.sav") == "save-a");
  CHECK(read_file(a / "SLOT0.sav.bak") == "save-a-bak");
  CHECK_FALSE(fs::exists(a / "SLOT0.sav.tmp"));
  const std::string manifest = read_file(a / "manifest.toml");
  Blake3 h;
  const std::string body = "save-a";
  h.update(std::as_bytes(std::span<const char>(body.data(), body.size())));
  CHECK(manifest.find("name = \"SLOT0.sav\"\nsize = 6\nblake3 = \"" + to_hex(h.finish()) + "\"") != std::string::npos);
  CHECK(manifest.find("made = \"20261116-101500\"") != std::string::npos);
  CHECK(fs::exists(dir.path() / "Backups" / kIdB / "20261116-101500" / "SLOT0.sav"));
  CHECK_FALSE(fs::exists(dir.path() / "Backups" / kIdB / "20261116-101500" / "SLOT0.sav.bak"));
  CHECK_FALSE(fs::exists(dir.path() / "Backups" / "not-an-id"));
  // Nothing under the saves was written, moved or renamed.
  CHECK(listing(dir.path() / "Saves") == before);
}

TEST_CASE("backups: no duplicate of an unchanged save; a changed one is a new backup; one second, two backups") {
  TempDir dir;
  make_saves(dir);
  REQUIRE(back_up_saves(spec_in(dir, "20261116-101500")).made == 2);
  BackupReport r = back_up_saves(spec_in(dir, "20261116-111500"));
  CHECK(r.made == 0);
  CHECK(r.unchanged == 2);
  CHECK_FALSE(fs::exists(dir.path() / "Backups" / kIdA / "20261116-111500"));

  write_file(dir.path() / "Saves" / kIdA / "SLOT0.sav", "save-a, later");
  r = back_up_saves(spec_in(dir, "20261116-111500"));
  CHECK(r.made == 1);
  CHECK(r.unchanged == 1);
  CHECK(read_file(dir.path() / "Backups" / kIdA / "20261116-111500" / "SLOT0.sav") == "save-a, later");

  write_file(dir.path() / "Saves" / kIdA / "SLOT0.sav", "save-a, later still");
  r = back_up_saves(spec_in(dir, "20261116-111500"));
  CHECK(r.made == 1);
  CHECK(fs::exists(dir.path() / "Backups" / kIdA / "20261116-111500-2" / "SLOT0.sav"));
  // A file that disappears from the save folder makes the next backup differ too.
  fs::remove(dir.path() / "Saves" / kIdA / "SLOT0.sav.bak");
  CHECK(back_up_saves(spec_in(dir, "20261116-121500")).made == 1);
}

TEST_CASE("backups: the rotation keeps the newest, and deletes only after a new backup") {
  TempDir dir;
  for (int i = 0; i < 6; ++i) {
    make_saves(dir, "save-a-" + std::to_string(i), "save-b");
    const BackupReport r = back_up_saves(spec_in(dir, "2026111" + std::to_string(i) + "-101500", 3));
    CHECK(r.ok());
    CHECK(r.removed == (i >= 3 ? 1u : 0u));
  }
  std::vector<std::string> kept;
  for (const auto& e : fs::directory_iterator(dir.path() / "Backups" / kIdA))
    kept.push_back(path_to_utf8(e.path().filename()));
  std::sort(kept.begin(), kept.end());
  CHECK(kept == std::vector<std::string>{"20261113-101500", "20261114-101500", "20261115-101500"});
  // B never changed: its one backup stays, and nothing of it is deleted.
  CHECK(fs::exists(dir.path() / "Backups" / kIdB / "20261110-101500"));
  // A folder that is no backup is never touched by the rotation.
  write_file(dir.path() / "Backups" / kIdA / "notes" / "keep.txt", "mine");
  make_saves(dir, "save-a-9");
  (void)back_up_saves(spec_in(dir, "20261119-101500", 3));
  CHECK(fs::exists(dir.path() / "Backups" / kIdA / "notes" / "keep.txt"));
}

TEST_CASE("backups: a folder with none of the files, no save root, and a backup folder that cannot be made") {
  TempDir dir;
  fs::create_directories(dir.path() / "Saves" / kIdA);
  BackupReport r = back_up_saves(spec_in(dir, "20261116-101500"));
  CHECK(r.ok());
  CHECK(r.empty == 1);
  CHECK(r.made == 0);
  TempDir none;
  CHECK(back_up_saves(spec_in(none, "20261116-101500")).ok());

  TempDir bad;
  make_saves(bad);
  write_file(bad.path() / "Backups", "a file where the folder should be");
  r = back_up_saves(spec_in(bad, "20261116-101500"));
  CHECK_FALSE(r.ok());
  CHECK(r.problems.front() == "a backup folder cannot be made");
  CHECK(r.made == 0);
}

TEST_CASE("backups: names") {
  CHECK(is_backup_name("20261116-101500"));
  CHECK(is_backup_name("20261116-101500-2"));
  CHECK(is_backup_name("20261116-101500-99"));
  CHECK_FALSE(is_backup_name("20261116-101500-1"));
  CHECK_FALSE(is_backup_name("20261116-101500-02"));
  CHECK_FALSE(is_backup_name("20261116_101500"));
  CHECK_FALSE(is_backup_name("notes"));
  CHECK(is_steam_id_folder("76500000000000042"));
  CHECK_FALSE(is_steam_id_folder("7650000000000004"));
  CHECK_FALSE(is_steam_id_folder("7650000000000004x"));
  CHECK(elden_ring_save_files().size() == 2);
}

TEST_CASE("appid file: created when absent, used when it holds the id, left alone when it holds anything else") {
  TempDir dir;
  AppidPrepared p = prepare_appid_file(dir.path(), 4242420, "20261116-101500");
  CHECK(p.step == AppidStep::Created);
  REQUIRE(p.record.has_value());
  CHECK(p.record->created_by_us);
  CHECK(read_file(dir.path() / "steam_appid.txt") == "4242420");
  // A second start finds it there: used, not ours (the first start's record says it is).
  CHECK(prepare_appid_file(dir.path(), 4242420, "x").step == AppidStep::Used);
  CHECK(remove_if_ours(*p.record, 4242420) == AppidRemoval::Removed);
  CHECK_FALSE(fs::exists(dir.path() / "steam_appid.txt"));
  CHECK(remove_if_ours(*p.record, 4242420) == AppidRemoval::Gone);

  write_file(dir.path() / "steam_appid.txt", " 4242420\r\n");
  CHECK(prepare_appid_file(dir.path(), 4242420, "x").step == AppidStep::Used);
  write_file(dir.path() / "steam_appid.txt", "999");
  const AppidPrepared foreign = prepare_appid_file(dir.path(), 4242420, "x");
  CHECK(foreign.step == AppidStep::Foreign);
  CHECK(read_file(dir.path() / "steam_appid.txt") == "999");

  // A record of ours whose file now holds something else, and one that is not ours: neither is deleted.
  CHECK(remove_if_ours(AppidRecord{dir.path() / "steam_appid.txt", true, "x"}, 4242420) == AppidRemoval::Changed);
  write_file(dir.path() / "steam_appid.txt", "4242420");
  CHECK(remove_if_ours(AppidRecord{dir.path() / "steam_appid.txt", false, "x"}, 4242420) == AppidRemoval::NotOurs);
  CHECK(fs::exists(dir.path() / "steam_appid.txt"));

  CHECK(prepare_appid_file(dir.path() / "missing-folder", 4242420, "x").step == AppidStep::Unwritable);
}

TEST_CASE("appid file: classify") {
  CHECK(classify(std::nullopt, 7) == AppidFile::Absent);
  CHECK(classify(std::string("7"), 7) == AppidFile::Ours);
  CHECK(classify(std::string("\t7 \n"), 7) == AppidFile::Ours);
  CHECK(classify(std::string("77"), 7) == AppidFile::Other);
  CHECK(classify(std::string(""), 7) == AppidFile::Other);
}

TEST_CASE("launcher-state.toml: the record round trips, odd paths included; anything unreadable is no record") {
  const AppidRecord rec{path_from_utf8("C:\\Games\\Invented \"Quoted\" \xC3\x9C\\steam_appid.txt"), true,
                        "20261116-101500"};
  const std::string text = state_text(rec);
  const auto back = read_state(text);
  REQUIRE(back.has_value());
  CHECK(*back == rec);
  CHECK_FALSE(read_state(state_text(std::nullopt)).has_value());
  CHECK_FALSE(read_state("[appid]\nfile = 3\n").has_value());
  CHECK_FALSE(read_state("[appid]\nfile = \"x\"\ncreated_by_us = true\n").has_value());
  CHECK_FALSE(read_state("[!]").has_value());
  CHECK_FALSE(read_state("not toml at all [").has_value());
}

TEST_CASE("launcher.toml: defaults, a round trip, and problems named without quoting") {
  CHECK(read_config("").config == LauncherConfig{});
  LauncherConfig c;
  c.game = path_from_utf8("C:\\Invented\\Game\\fake_game.exe");
  c.appid_method = AppidMethod::Env;
  c.remove_after = RemoveAfter::Exit;
  c.notices_seen = 1;
  const ConfigRead back = read_config(config_text(c));
  CHECK(back.problems.empty());
  CHECK(back.config == c);

  const ConfigRead bad =
      read_config("appid_method = \"carrier pigeon\"\nremove_after = 3\nnotices_seen = -1\nfoo = 1\n");
  CHECK(bad.config == LauncherConfig{});
  CHECK(bad.problems.size() == 4);
  for (const auto& p : bad.problems) CHECK(p.find("pigeon") == std::string::npos);
  CHECK(read_config("[!]\n").problems.front() == "launcher.toml cannot be read safely: the defaults are used");
  CHECK(read_config("game = \n").problems.size() == 1);
}
