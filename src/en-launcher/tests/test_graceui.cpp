// SHA-256 against NIST's examples, and GraceUI's pre-flight on invented GraceUI folders (graceui.hpp): every file
// matching graceui.toml, a graceui.toml as GraceUI's own manifest.py writes it (D165), and each way it can fail.
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
  f.manifest("0.2.50", "[1, 7]",
             Folder::entry("graceui.dll", kDll) + Folder::entry("fonts/NotoSans-Regular.ttf", kFont));
  const GraceReport r = check_graceui(f.grace);
  CHECK(r.result == GraceCheck::Ok);
  CHECK(r.version == std::array<std::uint16_t, 3>{0, 2, 50});
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
    f.manifest("0.2.50", "[1, 7]",
               Folder::entry("graceui.dll", kDll) + Folder::entry("fonts/NotoSans-Regular.ttf", kFont));
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
    f.manifest("0.2.49", "[1, 7]", Folder::entry("graceui.dll", kDll));
    r = check_graceui(f.grace);
    CHECK(r.result == GraceCheck::TooOld);
    CHECK(graceui_problem(r) == "this GraceUI is version 0.2.49, and EldenNet needs 0.2.50 or newer");
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

// graceui-manifest.toml (beside this file, compiled in by CMakeLists.txt) is what GraceUI's own writer,
// tools/release/manifest.py at GraceUI 7fa3042 (D165), wrote for an invented GraceUI folder of these three files:
//   python3 -I tools/release/manifest.py write FOLDER --released 2026-11-25
// Its form is the one every GraceUI build now lays out (`GraceUI/` beside `bin/`): a comment, `graceui`, `abi`,
// `protocol`, `caps` (empty), `released`, a blank line, then `[files]` in the order of their names.
namespace {

const unsigned char kManifestFixture[] = {
#include "graceui_manifest.inc"
};
const std::string kFixtureDll = "MZ an invented graceui.dll for EldenNet's launcher tests, not a DLL\n";
const std::string kFixtureLicence = "An invented licence for EldenNet's launcher tests.\n";
const std::string kFixtureNotice = "An invented notice for EldenNet's launcher tests.\n";

std::string manifest_fixture() {
  return std::string(reinterpret_cast<const char*>(kManifestFixture), sizeof kManifestFixture);
}

}  // namespace

TEST_CASE("GraceUI's pre-flight: a graceui.toml written by GraceUI's own manifest.py (D165)") {
  const std::string text = manifest_fixture();
  REQUIRE(text.find("protocol = [") != std::string::npos);  // the committed fixture, D165's keys and all
  REQUIRE(text.find("caps = []") != std::string::npos);
  REQUIRE(text.find("released = \"") != std::string::npos);
  auto m = read_graceui_toml(text);
  REQUIRE(m);
  CHECK(m->version == std::array<std::uint16_t, 3>{0, 2, 53});  // GraceUI 7fa3042's manifest.py, above EldenNet's floor
  CHECK(m->abi_major == 1);
  CHECK(m->abi_minor == 8);
  REQUIRE(m->files.size() == 3);
  CHECK(m->files[0].name == "LICENSE.txt");
  CHECK(m->files[1].name == "NOTICE.txt");
  CHECK(m->files[2].name == "graceui.dll");
  CHECK(m->files[2].size == kFixtureDll.size());
  CHECK(to_hex(m->files[2].sha256) == sha_hex(kFixtureDll));

  // The folder it was written for, checked as the launcher checks Game\GraceUI\ before the game starts.
  Folder f;
  f.put("graceui.dll", kFixtureDll);
  f.put("LICENSE.txt", kFixtureLicence);
  f.put("NOTICE.txt", kFixtureNotice);
  write_file(f.grace / "graceui.toml", text);
  const GraceReport r = check_graceui(f.grace);
  CHECK(r.result == GraceCheck::Ok);
  CHECK(r.files == 3);
  CHECK(r.checked == 3);
  f.put("NOTICE.txt", "An invented notice for EldenNet's launcher tests!\n");
  CHECK(check_graceui(f.grace).result == GraceCheck::HashDiffers);
}

TEST_CASE("graceui.toml: D165's protocol, caps and released are GraceUI's, never a reason to fail") {
  // EldenNet's pre-flight reads the version, the ABI and the files (ARCH §4.3); the bridge protocol is not used in
  // game in v0.1 and the release date names nothing it checks, so the three are accepted whatever they hold.
  const std::string files = "[files]\n\"graceui.dll\" = { size = 3, sha256 = \"" + std::string(64, 'a') + "\" }\n";
  const auto reads = [&](const std::string& head) { return static_cast<bool>(read_graceui_toml(head + files)); };
  const std::string base = "graceui = \"0.6.2\"\nabi = [1, 4]\n";
  CHECK(reads(base));
  CHECK(reads(base + "protocol = [1, 4]\ncaps = [\"monitor.decisions\", \"playthrough\"]\n" +
              "released = \"2027-03-01\"\n"));
  CHECK(reads(base + "protocol = [2, 0]\ncaps = []\nreleased = \"2026-11-20\"\n"));
  CHECK(reads(base + "protocol = \"1.4\"\ncaps = \"none\"\nreleased = 20261120\n"));
  CHECK(reads("# GraceUI's version file (DESIGN \u00a72.7, D52): a comment with a character outside ASCII\n" + base +
              "\n"));
}

TEST_CASE("E12: GraceUI's pre-flight, in E08's manner") {
  CHECK(error_id(ErrorCode::E12) == "E12");
  const std::string text = error_text(ErrorCode::E12, "graceui.dll does not match graceui.toml");
  CHECK(text.find("missing or damaged: graceui.dll does not match graceui.toml. The game starts") != std::string::npos);
  CHECK(error_action(ErrorCode::E12).empty());
}

TEST_CASE("E13: a test package's GraceUI against the game's, by graceui.toml") {
  Folder pkg, game;
  const std::string files = Folder::entry("graceui.dll", kDll) + Folder::entry("fonts/face.ttf", kFont);
  pkg.manifest("0.2.27", "[1, 6]", files);
  const auto want = read_graceui_manifest(pkg.grace / "graceui.toml");
  REQUIRE(want);
  CHECK(!read_graceui_manifest(game.grace / "graceui.toml"));  // no GraceUI in the game's folder
  const auto have_of = [&](const std::string& version, const std::string& abi, const std::string& listed) {
    game.manifest(version, abi, listed);
    const auto have = read_graceui_manifest(game.grace / "graceui.toml");
    REQUIRE(have);
    return graceui_differs(*want, *have);
  };
  CHECK(have_of("0.2.27", "[1, 6]", files).empty());
  CHECK(have_of("0.2.27", "[1, 6]", Folder::entry("fonts/face.ttf", kFont) + Folder::entry("graceui.dll", kDll))
            .empty());  // the files' order is not compared
  CHECK(have_of("0.2.24", "[1, 6]", files) == "the game's GraceUI is version 0.2.24, and this package carries 0.2.27");
  CHECK(have_of("0.2.27", "[1, 5]", files) == "the game's GraceUI speaks ABI 1.5, and this package's 1.6");
  const std::string changed = Folder::entry("graceui.dll", kDll + "x") + Folder::entry("fonts/face.ttf", kFont);
  CHECK(have_of("0.2.27", "[1, 6]", changed) == "both are version 0.2.27, but their graceui.dll differ");
  CHECK(have_of("0.2.27", "[1, 6]", Folder::entry("graceui.dll", kDll)) ==
        "both are version 0.2.27, but the game's lacks fonts/face.ttf");
  CHECK(have_of("0.2.27", "[1, 6]", files + Folder::entry("grace-standin.dll", kDll)) ==
        "both are version 0.2.27, but the game's has grace-standin.dll besides");
  // E13 names what differs and what to do, offers "Start anyway", and stops with its own exit code otherwise.
  const std::string text =
      error_text(ErrorCode::E13, "the game's GraceUI is version 0.2.24, and this package carries 0.2.27");
  CHECK(text.find("not the one this test package carries: the game's GraceUI is version 0.2.24") != std::string::npos);
  CHECK(text.find("copy the GraceUI folder from this package into the game's folder") != std::string::npos);
  CHECK(error_action(ErrorCode::E13) == "Start anyway");
  CHECK(exit_code(ErrorCode::E13) == 23);
}
