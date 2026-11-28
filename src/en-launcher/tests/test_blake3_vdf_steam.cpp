// A file's BLAKE3 digest (the hash's own vectors are en-proto's tests); Steam's KeyValues; finding the game in
// invented Steam folders.
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

}  // namespace

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
