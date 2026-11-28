// MEM §8.2's SteamID discovery over invented listings (steamid.hpp): 17-digit folders only, one taken as it is,
// several by their newest save file (any big enough regular file, whatever its name), a change since the game started
// first, and none without a guess.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/shell/steamid.hpp"

using namespace en::shell;

namespace {

constexpr std::uint64_t kBig = kSaveFileMinBytes;
SaveFileMeta save(std::int64_t at, std::uint64_t bytes = kBig) { return SaveFileMeta{bytes, at, true}; }

}  // namespace

TEST_CASE("steamid: only folders of 17 decimal digits name an account") {
  CHECK(steamid_folder("76500000000000001") == 76500000000000001ull);
  CHECK_FALSE(steamid_folder("7650000000000001"));     // 16
  CHECK_FALSE(steamid_folder("765000000000000011"));   // 18
  CHECK_FALSE(steamid_folder("7650000000000000x"));
  CHECK_FALSE(steamid_folder("GraphicsConfig.xml"));
  CHECK_FALSE(steamid_folder("00000000000000000"));
  CHECK_FALSE(steamid_folder(""));
}

TEST_CASE("steamid: a save file is a regular file of at least 10 x 0x280000 bytes, whatever its name") {
  CHECK(save_file(save(1)));
  CHECK_FALSE(save_file(save(1, kBig - 1)));
  SaveFileMeta folder = save(1);
  folder.regular = false;
  CHECK_FALSE(save_file(folder));
}

TEST_CASE("steamid: one account folder is taken, with or without a save file in it") {
  const AccountPick p = pick_account({{"notes", {save(9)}}, {"76500000000000007", {}}});
  CHECK(p.steamid64 == 76500000000000007ull);
  CHECK(p.why == AccountWhy::One);
  CHECK(p.folders == 1);
}

TEST_CASE("steamid: several folders: the newest save file's, ties to the lower id, small files ignored") {
  const std::vector<AccountFolder> listing = {
      {"76500000000000003", {save(100), save(500, kBig - 1)}},
      {"76500000000000002", {save(200)}},
      {"76500000000000001", {save(200)}},
  };
  const AccountPick p = pick_account(listing);
  CHECK(p.steamid64 == 76500000000000001ull);
  CHECK(p.why == AccountWhy::Newest);
  CHECK(p.folders == 3);
  // The listing's order changes nothing.
  const std::vector<AccountFolder> reversed(listing.rbegin(), listing.rend());
  CHECK(pick_account(reversed).steamid64 == 76500000000000001ull);
}

TEST_CASE("steamid: a save file changed since the game started names its folder") {
  const std::vector<AccountFolder> listing = {{"76500000000000001", {save(100)}}, {"76500000000000002", {save(900)}}};
  const AccountPick p = pick_account(listing, 800);
  CHECK(p.steamid64 == 76500000000000002ull);
  CHECK(p.why == AccountWhy::Changed);
  CHECK(pick_account(listing, 1000).why == AccountWhy::Newest);
}

TEST_CASE("steamid: none, or several with no save file, is no account") {
  CHECK(pick_account({}).why == AccountWhy::None);
  CHECK(pick_account({{"saves", {save(1)}}}).steamid64 == 0);
  const AccountPick p = pick_account({{"76500000000000001", {save(1, 10)}}, {"76500000000000002", {}}});
  CHECK(p.steamid64 == 0);
  CHECK(p.why == AccountWhy::None);
  CHECK(p.folders == 2);
  CHECK(std::string(to_string(p.why)) == "none");
}
