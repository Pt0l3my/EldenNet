// BLAKE3 against its published test vectors; Steam's KeyValues; finding the game in invented Steam folders.
#include <doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "en/launcher/blake3.hpp"
#include "en/launcher/steam.hpp"
#include "en/launcher/vdf.hpp"
#include "support.hpp"

using namespace en::launcher;
using en::launcher::test::TempDir;
using en::launcher::test::write_file;

namespace {

// The vectors' input: byte i is i mod 251.
std::vector<std::byte> vector_input(std::size_t n) {
  std::vector<std::byte> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::byte>(i % 251);
  return v;
}

std::string hash_in_pieces(const std::vector<std::byte>& input, std::size_t piece) {
  Blake3 h;
  for (std::size_t at = 0; at < input.size(); at += piece) {
    h.update(std::span<const std::byte>(input).subspan(at, std::min(piece, input.size() - at)));
  }
  return to_hex(h.finish());
}

}  // namespace

TEST_CASE("BLAKE3: the published test vectors, whole and in pieces of every awkward size") {
  // The first 32 bytes of the official test vectors' hashes (BLAKE3's test_vectors.json).
  const std::vector<std::pair<std::size_t, const char*>> vectors = {
      {0, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"},
      {1, "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213"},
      {63, "e9bc37a594daad83be9470df7f7b3798297c3d834ce80ba85d6e207627b7db7b"},
      {64, "4eed7141ea4a5cd4b788606bd23f46e212af9cacebacdc7d1f4c6dc7f2511b98"},
      {65, "de1e5fa0be70df6d2be8fffd0e99ceaa8eb6e8c93a63f2d8d1c30ecb6b263dee"},
      {1023, "10108970eeda3eb932baac1428c7a2163b0e924c9a9e25b35bba72b28f70bd11"},
      {1024, "42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af7"},
      {1025, "d00278ae47eb27b34faecf67b4fe263f82d5412916c1ffd97c8cb7fb814b8444"},
      {2048, "e776b6028c7cd22a4d0ba182a8bf62205d2ef576467e838ed6f2529b85fba24a"},
      {2049, "5f4d72f40d7a5f82b15ca2b2e44b1de3c2ef86c426c95c1af0b6879522563030"},
      {3072, "b98cb0ff3623be03326b373de6b9095218513e64f1ee2edd2525c7ad1e5cffd2"},
      {3073, "7124b49501012f81cc7f11ca069ec9226cecb8a2c850cfe644e327d22d3e1cd3"},
      {8193, "bab6c09cb8ce8cf459261398d2e7aef35700bf488116ceb94a36d0f5f1b7bc3b"},
      {16384, "f875d6646de28985646f34ee13be9a576fd515f76b5b0a26bb324735041ddde4"},
      {31744, "62b6960e1a44bcc1eb1a611a8d6235b6b4b78f32e7abc4fb4c6cdcce94895c47"},
      {102400, "bc3e3d41a1146b069abffad3c0d44860cf664390afce4d9661f7902e7943e085"},
  };
  for (const auto& [len, want] : vectors) {
    CAPTURE(len);
    const auto input = vector_input(len);
    for (const std::size_t piece : {std::size_t{1} << 20, std::size_t{1}, std::size_t{63}, std::size_t{64},
                                    std::size_t{1000}, std::size_t{1024}, std::size_t{4097}}) {
      CAPTURE(piece);
      CHECK(hash_in_pieces(input, piece) == want);
    }
  }
}

TEST_CASE("BLAKE3: a file's digest and size") {
  TempDir dir;
  const auto input = vector_input(102400);
  write_file(dir.path() / "f.bin", std::string(reinterpret_cast<const char*>(input.data()), input.size()));
  const auto d = digest_file(dir.path() / "f.bin");
  REQUIRE(d.has_value());
  CHECK(d->size == 102400);
  CHECK(to_hex(d->blake3) == "bc3e3d41a1146b069abffad3c0d44860cf664390afce4d9661f7902e7943e085");
  CHECK_FALSE(digest_file(dir.path() / "absent.bin").has_value());
}

TEST_CASE("KeyValues: objects, quoted and bare tokens, escapes, comments and conditions") {
  const auto root = vdf::parse(
      "\xEF\xBB\xBF// a comment\n"
      "\"Outer\"\n{\n"
      "  \"path\"  \"C:\\\\Games\\\\Invented \\\"Lib\\\"\"\n"
      "  bare   value   [$WIN32]\n"
      "  \"inner\" { \"k\" \"v\" } [$OSX]\n"
      "  \"tab\" \"a\\tb\\nc\"\n"
      "}\n");
  REQUIRE(root.has_value());
  const vdf::Node* outer = vdf::find(*root, "outer");  // keys ignore ASCII case
  REQUIRE(outer != nullptr);
  CHECK(outer->object);
  CHECK(vdf::find(*outer, "PATH")->value == "C:\\Games\\Invented \"Lib\"");
  CHECK(vdf::find(*outer, "bare")->value == "value");
  CHECK(vdf::find(*vdf::find(*outer, "inner"), "k")->value == "v");
  CHECK(vdf::find(*outer, "tab")->value == "a\tb\nc");
  CHECK(vdf::find(*outer, "missing") == nullptr);
}

TEST_CASE("KeyValues: malformed text is refused with its line and a fixed problem") {
  const auto unclosed = vdf::parse("\"a\"\n{\n\"b\" \"c");
  REQUIRE_FALSE(unclosed.has_value());
  CHECK(unclosed.error().line == 3);
  CHECK(std::string(unclosed.error().problem) == "a quoted string is not closed");
  CHECK(std::string(vdf::parse("\"a\" { \"b\" \"c\"").error().problem) == "an object is not closed");
  CHECK(std::string(vdf::parse("}").error().problem) == "a closing brace with no object open");
  CHECK(std::string(vdf::parse("\"a\"").error().problem) == "a value or an object was expected after a key");
  CHECK(std::string(vdf::parse("{ }").error().problem) == "a key was expected");
  std::string deep;
  for (int i = 0; i < 17; ++i) deep += "k {";
  CHECK(std::string(vdf::parse(deep).error().problem) == "objects nested too deep");
  CHECK_FALSE(vdf::parse(std::string(vdf::kMaxText + 1, ' ')).has_value());
}

namespace {

// An invented Steam folder: libraryfolders.vdf naming the Steam folder and two libraries, the app's manifest in the
// second, and the executable where the GameSpec says. App id and names invented.
const GameSpec kSpec{4242420, {"Bin", "fake_game.exe"}};

void make_steam(const std::filesystem::path& steam, const std::filesystem::path& lib1,
                const std::filesystem::path& lib2, bool new_form) {
  const auto esc = [](const std::filesystem::path& p) {
    std::string s = path_to_utf8(p), out;
    for (const char c : s) {
      if (c == '\\')
        out += "\\\\";
      else
        out.push_back(c);
    }
    return out;
  };
  std::string vdf;
  if (new_form) {
    vdf = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + esc(steam) +
          "\"\n\t\t\"apps\" { \"111\" \"5\" }\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" + esc(lib1) +
          "\"\n\t}\n\t\"2\"\n\t{\n\t\t\"path\"\t\t\"" + esc(lib2) + "\"\n\t}\n}\n";
  } else {
    vdf = "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\"123\"\n\t\"ContentStatsID\"\t\"-456\"\n\t\"1\"\t\"" +
          esc(lib1) + "\"\n\t\"2\"\t\"" + esc(lib2) + "\"\n}\n";
  }
  write_file(steam / "steamapps" / "libraryfolders.vdf", vdf);
  write_file(lib2 / "steamapps" / "appmanifest_4242420.acf",
             "\"AppState\"\n{\n\t\"appid\"\t\t\"4242420\"\n\t\"installdir\"\t\t\"Invented Game\"\n}\n");
  write_file(lib2 / "steamapps" / "common" / "Invented Game" / "Bin" / "fake_game.exe", "MZ");
}

}  // namespace

TEST_CASE("Steam: the game found in a library listed in either form of libraryfolders.vdf") {
  for (const bool new_form : {true, false}) {
    CAPTURE(new_form);
    TempDir dir;
    const auto steam = dir.path() / "Steam";
    const auto lib1 = dir.path() / "Lib One";
    const auto lib2 = dir.path() / "Lib Two";
    std::filesystem::create_directories(lib1);
    make_steam(steam, lib1, lib2, new_form);
    const SteamScan scan = find_in_steam(steam, kSpec);
    CHECK(scan.libraries.size() == 3);  // the Steam folder once, though the newer form lists it too
    CHECK(scan.manifests == 1);
    REQUIRE(scan.exe.has_value());
    CHECK(*scan.exe == lib2 / "steamapps" / "common" / "Invented Game" / "Bin" / "fake_game.exe");
  }
}

TEST_CASE("Steam: no list, no manifest, a manifest with a path for its folder, a missing executable") {
  TempDir dir;
  const auto steam = dir.path() / "Steam";
  std::filesystem::create_directories(steam);
  SteamScan scan = find_in_steam(steam, kSpec);
  CHECK_FALSE(scan.exe.has_value());
  CHECK(scan.notes == std::vector<std::string>{"no library list"});

  write_file(steam / "steamapps" / "appmanifest_4242420.acf", "\"AppState\" { \"installdir\" \"..\" }");
  scan = find_in_steam(steam, kSpec);
  CHECK(scan.manifests == 1);
  CHECK_FALSE(scan.exe.has_value());
  CHECK(scan.notes.back() == "an app manifest without a usable install folder");

  write_file(steam / "steamapps" / "appmanifest_4242420.acf", "\"AppState\" { \"installdir\" \"a\\\\b\" }");
  CHECK_FALSE(find_in_steam(steam, kSpec).exe.has_value());

  write_file(steam / "steamapps" / "appmanifest_4242420.acf", "\"AppState\" { \"installdir\" \"Gone\" }");
  scan = find_in_steam(steam, kSpec);
  CHECK(scan.notes.back() == "an app manifest whose executable is missing");

  write_file(steam / "steamapps" / "libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" ");
  CHECK(find_in_steam(steam, kSpec).notes.front() == "the library list cannot be read");
}

TEST_CASE("--game: the executable, its folder, or the install folder above it") {
  TempDir dir;
  const auto install = dir.path() / "Invented Game";
  const auto exe = install / "Bin" / "fake_game.exe";
  write_file(exe, "MZ");
  CHECK(resolve_game_path(exe, kSpec) == exe);
  CHECK(resolve_game_path(install / "Bin", kSpec) == exe);
  CHECK(resolve_game_path(install, kSpec) == exe);
  CHECK_FALSE(resolve_game_path(dir.path(), kSpec).has_value());
  CHECK_FALSE(resolve_game_path(dir.path() / "nothing", kSpec).has_value());
}

TEST_CASE("paths through UTF-8 and back") {
  const std::string text =
      "C:/Spiele/\xC3\x9C"
      "ber/x.exe";
  CHECK(path_to_utf8(path_from_utf8(text)) == text);
  CHECK(elden_ring().appid == 1245620);
  CHECK(elden_ring().exe_name() == "eldenring.exe");
}
