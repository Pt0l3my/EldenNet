// SHA-256 against NIST's examples, and GraceUI's pre-flight on invented GraceUI folders (graceui.hpp): every file
// matching graceui.toml, and each way it can fail.
#include <doctest.h>

#include <cstring>
#include <string>

#include "en/launcher/blake3.hpp"
#include "en/launcher/graceui.hpp"
#include "en/launcher/sha256.hpp"
#include "en/launcher/texts.hpp"
#include "support.hpp"

using namespace en::launcher;
using en::launcher::test::TempDir;
using en::launcher::test::write_file;

namespace {

std::string sha_hex(std::string_view text, std::size_t piece = 0) {
  Sha256 h;
  const auto bytes = std::as_bytes(std::span<const char>(text.data(), text.size()));
  if (piece == 0) {
    h.update(bytes);
  } else {
    for (std::size_t i = 0; i < bytes.size(); i += piece) h.update(bytes.subspan(i, std::min(piece, bytes.size() - i)));
  }
  const auto d = h.finish();
  return to_hex(d);
}

// An invented GraceUI folder: its files' bytes are ours, and graceui.toml lists each with its size and SHA-256.
struct Folder {
  TempDir dir;
  std::filesystem::path grace;
  Folder() : grace(dir.path() / "Game" / "GraceUI") {}

  void put(const std::string& name, const std::string& bytes) {
    std::filesystem::path p = grace;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= name.size(); ++i) {
      if (i == name.size() || name[i] == '/') {
        p /= name.substr(start, i - start);
        start = i + 1;
      }
    }
    write_file(p, bytes);
  }
  static std::string entry(const std::string& name, const std::string& bytes) {
    return "\"" + name + "\" = { size = " + std::to_string(bytes.size()) + ", sha256 = \"" + sha_hex(bytes) + "\" }\n";
  }
  void manifest(const std::string& version, const std::string& abi, const std::string& files) {
    write_file(grace / "graceui.toml", "graceui = \"" + version + "\"\nabi = " + abi +
                                           "\nprotocol = [0, 1]\ncaps = []\nreleased = \"2026-11-20\"\n[files]\n" +
                                           files);
  }
};

const std::string kDll = std::string("MZ invented bytes, not a DLL\0\x01\x02", 32);
const std::string kFont = "an invented font file";

}  // namespace

TEST_CASE("SHA-256: NIST's examples, whole and in pieces") {
  CHECK(sha_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(sha_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const std::string two_blocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  CHECK(sha_hex(two_blocks) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  const std::string million(1000000, 'a');
  CHECK(sha_hex(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  for (const std::size_t piece : {1u, 3u, 55u, 56u, 63u, 64u, 65u, 127u}) {
    INFO(piece);
    CHECK(sha_hex(two_blocks, piece) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  }
  // Every length around the padding's edges agrees, whole and byte by byte.
  for (std::size_t n = 50; n < 140; ++n) {
    const std::string text(n, 'x');
    CHECK(sha_hex(text) == sha_hex(text, 1));
  }
  TempDir dir;
  write_file(dir.path() / "f.bin", "abc");
  const auto f = sha256_file(dir.path() / "f.bin");
  REQUIRE(f);
  CHECK(f->size == 3);
  CHECK(to_hex(f->sha256) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK_FALSE(sha256_file(dir.path() / "none.bin"));
}

TEST_CASE("GraceUI's pre-flight: every listed file matches, the ABI and the floor fit") {
  Folder f;
  f.put("graceui.dll", kDll);
  f.put("fonts/NotoSans-Regular.ttf", kFont);
  f.put("user/settings.toml", "not listed: GraceUI's own, at run time (D84)");
  f.manifest("0.1.0", "[1, 0]", Folder::entry("graceui.dll", kDll) + Folder::entry("fonts/NotoSans-Regular.ttf", kFont));
  const GraceReport r = check_graceui(f.grace);
  CHECK(r.result == GraceCheck::Ok);
  CHECK(r.version == std::array<std::uint16_t, 3>{0, 1, 0});
  CHECK(r.abi_major == 1);
  CHECK(r.files == 2);
  CHECK(r.checked == 2);
  CHECK(r.file.empty());
  // A newer minor of the same major, and a later version, fit too (D74).
  f.manifest("0.6.2", "[1, 4]", Folder::entry("graceui.dll", kDll));
  CHECK(check_graceui(f.grace).result == GraceCheck::Ok);
}

TEST_CASE("GraceUI's pre-flight: each way it fails, named without a path") {
  SUBCASE("no folder") {
    Folder f;
    CHECK(check_graceui(f.grace).result == GraceCheck::NoFolder);
    CHECK(graceui_problem(check_graceui(f.grace)) == "there is no GraceUI folder beside the game");
  }
  SUBCASE("no graceui.toml") {
    Folder f;
    f.put("graceui.dll", kDll);
    CHECK(check_graceui(f.grace).result == GraceCheck::NoManifest);
  }
  SUBCASE("a file missing, of another size, or another hash") {
    Folder f;
    f.put("graceui.dll", kDll);
    f.manifest("0.1.0", "[1, 0]", Folder::entry("graceui.dll", kDll) + Folder::entry("fonts/NotoSans-Regular.ttf", kFont));
    GraceReport r = check_graceui(f.grace);
    CHECK(r.result == GraceCheck::FileMissing);
    CHECK(r.file == "fonts/NotoSans-Regular.ttf");
    CHECK(r.checked == 0);  // in the order of their names: fonts/ before graceui.dll
    CHECK(graceui_problem(r) == "fonts/NotoSans-Regular.ttf is missing from GraceUI's folder");
    f.put("fonts/NotoSans-Regular.ttf", kFont + "!");
    r = check_graceui(f.grace);
    CHECK(r.result == GraceCheck::SizeDiffers);
    std::string same_size = kFont;
    same_size[0] = 'A';
    f.put("fonts/NotoSans-Regular.ttf", same_size);
    r = check_graceui(f.grace);
    CHECK(r.result == GraceCheck::HashDiffers);
    CHECK(graceui_problem(r) == "fonts/NotoSans-Regular.ttf does not match graceui.toml");
    CHECK(graceui_problem(r).find(f.dir.path().string()) == std::string::npos);
  }
  SUBCASE("another ABI major, or a version older than the floor") {
    Folder f;
    f.put("graceui.dll", kDll);
    f.manifest("0.1.0", "[2, 0]", Folder::entry("graceui.dll", kDll));
    GraceReport r = check_graceui(f.grace);
    CHECK(r.result == GraceCheck::AbiMajor);
    CHECK(graceui_problem(r) == "this GraceUI speaks ABI 2, and EldenNet needs ABI 1");
    f.manifest("0.0.9", "[1, 0]", Folder::entry("graceui.dll", kDll));
    r = check_graceui(f.grace);
    CHECK(r.result == GraceCheck::TooOld);
    CHECK(graceui_problem(r) == "this GraceUI is version 0.0.9, and EldenNet needs 0.1.0 or newer");
  }
}

TEST_CASE("graceui.toml: what it must hold, each refusal in fixed words") {
  const std::string dll = "\"graceui.dll\" = { size = 3, sha256 = \"" + std::string(64, 'a') + "\" }\n";
  const auto refused = [](const std::string& text) {
    auto m = read_graceui_toml(text);
    return m ? std::string() : m.error();
  };
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n[files]\n" + dll).empty());
  CHECK(refused("abi = [1, 0]\n[files]\n" + dll) == "graceui.toml names no version");
  CHECK(refused("graceui = \"0.1\"\nabi = [1, 0]\n[files]\n" + dll) == "graceui.toml names no version");
  CHECK(refused("graceui = \"0.1.0.4\"\nabi = [1, 0]\n[files]\n" + dll) == "graceui.toml names no version");
  CHECK(refused("graceui = \"1024.0.0\"\nabi = [1, 0]\n[files]\n" + dll) == "graceui.toml names no version");
  CHECK(refused("graceui = \"0.1.0\"\nabi = 1\n[files]\n" + dll) == "graceui.toml names no ABI");
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, -1]\n[files]\n" + dll) == "graceui.toml names no ABI");
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n") == "graceui.toml lists no files");
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n[files]\n\"web/a.js\" = { size = 3, sha256 = \"" +
                std::string(64, 'b') + "\" }\n") == "graceui.toml does not list graceui.dll");
  for (const char* outside : {"../graceui.dll", "/graceui.dll", "a//b", "C:/x", "a\\\\b", "./x", "a/", "web/../x"}) {
    INFO(outside);
    CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n[files]\n" + dll + "\"" + outside +
                  "\" = { size = 1, sha256 = \"" + std::string(64, 'c') + "\" }\n") ==
          "graceui.toml lists a file outside its folder");
  }
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n[files]\n\"graceui.dll\" = { size = 3, sha256 = \"abc\" }\n") ==
        "graceui.toml lists a file without its size and hash");
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n[files]\n\"graceui.dll\" = { sha256 = \"" + std::string(64, 'a') +
                "\" }\n") == "graceui.toml lists a file without its size and hash");
  CHECK(refused("graceui = \"0.1.0\"\nabi = [1, 0]\n[files]\n\"graceui.dll\" = 3\n") ==
        "graceui.toml lists a file without its size and hash");
  CHECK(refused("graceui = [") == "graceui.toml is not valid TOML");
  CHECK(refused(std::string(kGraceTomlMaxBytes + 1, ' ')) == "graceui.toml is too large");
  CHECK(std::string(to_string(GraceCheck::HashDiffers)) == "hash_differs");
}

TEST_CASE("E12: GraceUI's pre-flight, in E08's manner") {
  CHECK(error_id(ErrorCode::E12) == "E12");
  const std::string text = error_text(ErrorCode::E12, "graceui.dll does not match graceui.toml");
  CHECK(text.find("missing or damaged: graceui.dll does not match graceui.toml. The game starts") != std::string::npos);
  CHECK(error_action(ErrorCode::E12).empty());
}
