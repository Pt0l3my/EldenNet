// The game's gate and the DLL's check, on invented images (en-gal's FakeImage builder) written to files.
#include <doctest.h>

#include <string>

#include "en/launcher/exe.hpp"
#include "fake_image.hpp"
#include "support.hpp"

using namespace en::launcher;
using en::launcher::test::TempDir;
using en::launcher::test::write_file;
namespace g = en::gal;

namespace {

g::test::BuiltImage invented(std::u16string product) {
  g::test::FakeImageSpec spec;
  spec.sections.push_back(g::test::FakeSection{".text", g::test::kCode, std::vector<std::uint8_t>(64, 0xCC), 0});
  g::test::FakeVersion v;
  v.product_name = std::move(product);
  spec.version = v;
  return g::test::build(spec);
}

void write_image(const std::filesystem::path& p, const g::test::BuiltImage& img) {
  write_file(p, std::string(reinterpret_cast<const char*>(img.file.data()), img.file.size()));
}

std::string builds_for(std::uint32_t stamp, std::uint32_t size) {
  return "schema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\napp = \"4.5.6\"\nfile_version = \"9.8.7.0\"\n"
         "product_name = \"INVENTED GAME\"\nregion = \"ww\"\ntime_date_stamp = " +
         std::to_string(stamp) + "\nsize_of_image = " + std::to_string(size) +
         "\nlayout = \"layouts/invented.toml\"\nstatus = \"supported\"\n";
}

}  // namespace

TEST_CASE("exe: a DLL is ours when its ProductName reads EldenNet") {
  TempDir dir;
  write_image(dir.path() / "ours.dll", invented(u"EldenNet"));
  write_image(dir.path() / "other.dll", invented(u"Someone Else"));
  write_file(dir.path() / "junk.dll", "not a PE image");
  CHECK(is_ours(dir.path() / "ours.dll"));
  CHECK_FALSE(is_ours(dir.path() / "other.dll"));
  CHECK_FALSE(is_ours(dir.path() / "junk.dll"));
  CHECK_FALSE(is_ours(dir.path() / "absent.dll"));
  CHECK(product_name(dir.path() / "other.dll") == std::string("Someone Else"));
}

TEST_CASE("exe: the game's build through builds.toml: passed, unknown, no builds, unreadable") {
  TempDir dir;
  const auto img = invented(u"INVENTED GAME");
  write_image(dir.path() / "game.exe", img);
  write_file(dir.path() / "builds.toml", builds_for(0x12345678, img.size_of_image));
  GateOutcome g = gate_game(dir.path() / "game.exe", dir.path() / "builds.toml");
  CHECK(g.kind == GateOutcome::Kind::Passed);
  CHECK(g.key == "9.8.7.0-ww");
  CHECK(g.version == "9.8.7.0");

  write_file(dir.path() / "builds.toml", builds_for(0x12345679, img.size_of_image));
  g = gate_game(dir.path() / "game.exe", dir.path() / "builds.toml");
  CHECK(g.kind == GateOutcome::Kind::Unknown);
  CHECK(g.why == "build fingerprint differs (TimeDateStamp)");
  CHECK(g.version == "9.8.7.0");

  g = gate_game(dir.path() / "game.exe", dir.path() / "missing.toml");
  CHECK(g.kind == GateOutcome::Kind::NoBuilds);
  write_file(dir.path() / "bad.toml", "schema = 1\n");
  CHECK(gate_game(dir.path() / "game.exe", dir.path() / "bad.toml").why == "builds.toml is refused");

  write_file(dir.path() / "junk.exe", "MZ but nothing more");
  CHECK(gate_game(dir.path() / "junk.exe", dir.path() / "builds.toml").kind == GateOutcome::Kind::Unreadable);
  CHECK(std::string(to_string(GateOutcome::Kind::NoBuilds)) == "no_builds");
}
