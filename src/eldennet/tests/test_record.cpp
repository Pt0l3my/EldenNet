// The recorder's lines (record.hpp): every kind written on one line through the closed vocabulary, linting clean
// alone and as a file; no name, text or param value in any of them; and a full snapshot within a line's cap.
#include <doctest.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "en/gal/report.hpp"
#include "en/shell/probe_report.hpp"
#include "en/shell/record.hpp"

using namespace en::shell;
namespace proto = en::proto;

namespace {

const std::array<const en::gal::report::Vocabulary*, 2> kKnown = {&recording_vocabulary(), &probe_vocabulary()};

void lints(const std::string& line) {
  INFO(line.substr(0, 300));
  REQUIRE_FALSE(line.empty());
  CHECK(line.back() == '\n');
  CHECK(line.find('\n') == line.size() - 1);
  const auto ok = en::gal::report::lint(line, kKnown);
  if (!ok) {
    for (const auto& p : ok.error()) INFO(en::gal::report::describe(p));
  }
  CHECK(ok.has_value());
}

std::unique_ptr<proto::WorldSnapshot> full_snapshot() {
  auto s = std::make_unique<proto::WorldSnapshot>();
  s->seq = 41;
  s->mono_ms = 123456;
  s->gap = true;
  s->build.index = 1;
  for (auto& g : s->health.groups) g = proto::GroupHealth{proto::GroupState::Ok, true, 2};
  s->health.task_us_p99 = 310;
  s->session = proto::Session{proto::MultiplayRole::Host, true, proto::LobbyState::None, 1, false, false};
  s->life = proto::Life{true, proto::Screen::InGame, 7};
  s->ui.hud = proto::HudState::Default;
  s->ui.banner = 3;
  s->ui.popup_talk_id = 99;
  proto::Player p;
  p.handle.raw = 0xFFFFFFFF15A00000ull;
  p.level = 120;
  p.hp = {1100, 1200};
  p.play_time_ms = 3'600'000;
  p.statuses = proto::bit_of(proto::Status::Poison);
  p.pos = {1.5f, -2.25f, 1234.567f};
  p.locked_on = proto::ChrHandle{5};
  s->player = p;
  s->place = proto::Place{6100100, {}, false, true};
  s->clock = proto::Clock{13, 37};
  (void)s->boss.bars.try_push_back(proto::BossBar{proto::ChrHandle{8}, 904, -10});
  for (std::uint32_t i = 0; i < proto::kMaxSnapshotChars; ++i) {
    proto::Chr c;
    c.handle.raw = 1000 + i;
    c.param = 81100 + i;
    c.hp = {500, 600};
    c.dist_m = 3.5f * static_cast<float>(i);
    c.bearing_deg = i % 2 == 0 ? std::numeric_limits<float>::quiet_NaN() : 12.25f;
    c.pos = {1.0f, 2.0f, 3.0f};
    (void)s->chars.try_push_back(c);
  }
  proto::FlagFrame f;
  f.watch_version = 9;
  for (std::size_t i = 0; i < proto::kMaxWatchFlags; i += 3) {
    f.bits.set(i);
    f.valid.set(i);
  }
  s->flags = f;
  proto::TextEntry named;
  named.fmg = 7;
  named.id = 1100;
  (void)named.text.emplace().try_assign("Invented Name");
  (void)s->new_text.try_push_back(named);
  proto::TextEntry missing;
  missing.fmg = 7;
  missing.id = 1103;
  (void)s->new_text.try_push_back(missing);
  proto::ParamRowInfo row;
  row.id = 81100;
  (void)row.fields.try_push_back(proto::ParamField{0, 424242});
  (void)s->new_params.try_push_back(row);
  return s;
}

}  // namespace

TEST_CASE("record: every kind of line lints clean, and a file of them too") {
  std::string file = header_line(RecordingHeader{{3, 14, 15, 0}, "ww", {0, 1, 0}, "s0b", true, 1}, 10);
  lints(file);
  const auto snap = full_snapshot();
  const std::string s = snapshot_line(*snap, 20);
  lints(s);
  CHECK(s.size() < en::gal::report::kMaxReportBytes);
  CHECK(s.find("bearing_deg\":12.25") != std::string::npos);
  file += s;
  proto::Lifecycle ready{proto::Lifecycle::DataReady{proto::BuildKey{1}, proto::Capabilities{en::proto::FlagSetTraits<proto::GroupSet>::all}}};
  proto::Lifecycle::PlaythroughReady pr;
  pr.slot = 2;
  (void)pr.name.try_assign("Tess Wayfarer");
  pr.play_time_ms = 1000;
  for (const proto::Lifecycle& l : {ready, proto::Lifecycle{pr}, proto::Lifecycle{proto::Lifecycle::GroupTripped{
                                                                     proto::Group::Camera}}}) {
    const std::string line = lifecycle_line(l, 30);
    lints(line);
    file += line;
  }
  for (const std::string& line :
       {banner_line(en::core::BannerReason::DiscoveryFailed, 40),
        change_line(en::core::Change{en::core::Change::What::GateClosed, 3, 0, 123}, 41), mark_line(1, 42),
        command_line(proto::GameCommand{proto::GameCommand::SetWatchFlags{
                         9, std::make_shared<const std::vector<std::uint32_t>>(std::vector<std::uint32_t>{1, 2})}},
                     43),
        command_line(proto::GameCommand{proto::GameCommand::SetTracked{}}, 44),
        command_line(proto::GameCommand{proto::GameCommand::ResolveText{}}, 45),
        command_line(proto::GameCommand{proto::GameCommand::ResolveParams{}}, 46),
        command_line(proto::GameCommand{}, 47)}) {
    lints(line);
    file += line;
  }
  CHECK(en::gal::report::lint_lines(file, kKnown).has_value());
  // Numbers only: neither the character's name, nor the text found, nor a param's value.
  CHECK(file.find("Tess") == std::string::npos);
  CHECK(file.find("Invented") == std::string::npos);
  CHECK(file.find("424242") == std::string::npos);
  CHECK(file.find("\"name_chars\":13") != std::string::npos);
}

TEST_CASE("record: an empty snapshot (the title) lints clean") {
  const proto::WorldSnapshot empty;
  lints(snapshot_line(empty, 1));
}
