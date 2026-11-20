// The F11 view's lines (view.hpp): our words and numbers only, whatever the status holds.
#include <doctest.h>

#include <cstring>
#include <string>

#include "en/overlay/view.hpp"

using namespace en::overlay;

namespace {

std::string all_text(const ViewLines& lines) {
  std::string out;
  for (const ViewLine& l : lines) out += std::string(l.text.data()) + "\n";
  return out;
}

bool only_ours(const ViewLines& lines) {
  for (const ViewLine& l : lines) {
    for (const char* p = l.text.data(); *p != '\0'; ++p) {
      const char c = *p;
      const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      std::strchr(" _.+-/:,()?'", c) != nullptr;
      if (!ok) return false;
    }
  }
  return true;
}

ViewStatus sample() {
  ViewStatus s;
  s.version = {0, 1, 0};
  s.label = "0.1.0-spike.6";
  s.init = "ready";
  s.items_resolved = 23;
  s.items_total = 23;
  s.passes = 1;
  s.phase = "in_world";
  s.level = "L2";
  s.gate_open = true;
  s.gate_why = "none";
  s.dormant = false;
  s.snapshots = 1234;
  s.task_p99_us = 85;
  s.read_mode = "full";
  s.have_health = true;
  s.health.of(en::proto::Group::Session).state = en::proto::GroupState::Ok;
  s.health.of(en::proto::Group::Camera).state = en::proto::GroupState::Tripped;
  s.health.of(en::proto::Group::Camera).faults = 3;
  s.tripped = 1u << static_cast<unsigned>(en::proto::Group::Camera);
  s.copies = 99;
  s.fault_names = {"implausible", "stack", "copy", "budget", "wrong_thread", "injected"};
  s.faults = {0, 0, 2, 0, 0, 0};
  s.marks = 2;
  s.mark_source = "eldennet";
  s.graceui_version = (0u << 22) | (1u << 12) | 0u;
  (void)s.changes.try_push_back(ViewChange{"phase", "title", 1000});
  (void)s.changes.try_push_back(ViewChange{"gate_opened", "open", 4000});
  s.now_ms = 16300;
  return s;
}

}  // namespace

TEST_CASE("view: the ribbon first, then the start, the lifecycle, the groups, the faults, the Marks, the changes") {
  ViewLines lines;
  view_lines(sample(), lines);
  const std::string text = all_text(lines);
  REQUIRE(lines.size() >= 10);
  CHECK(lines[0].heading);
  CHECK(std::string(lines[0].text.data()) == kRibbonText);
  CHECK(text.find("EldenNet 0.1.0  build 0.1.0-spike.6") != std::string::npos);
  CHECK(text.find("init ready   discovery 23/23 items, 1 pass") != std::string::npos);
  CHECK(text.find("phase in_world   level L2   banner none") != std::string::npos);
  CHECK(text.find("gate open (none)   dormant no   badge no") != std::string::npos);
  CHECK(text.find("snapshots 1234   task p99 85 us   reads full") != std::string::npos);
  CHECK(text.find("session ok") != std::string::npos);
  CHECK(text.find("camera tripped(3)") != std::string::npos);
  CHECK(text.find("tripped camera") != std::string::npos);
  CHECK(text.find("copies 99   faults implausible 0 stack 0 copy 2") != std::string::npos);
  CHECK(text.find("marks 2 (eldennet)   GraceUI 0.1.0") != std::string::npos);
  // The newest change first, with its age.
  const std::size_t opened = text.find("12.3 s ago  gate_opened open");
  const std::size_t title = text.find("15.3 s ago  phase title");
  CHECK(opened != std::string::npos);
  CHECK(title != std::string::npos);
  CHECK(opened < title);
  CHECK(only_ours(lines));
  for (const ViewLine& l : lines) CHECK(std::strlen(l.text.data()) < kViewLineBytes);
}

TEST_CASE("view: anything that is not a word of ours is written as '?', and a long line is cut") {
  ViewStatus s;
  s.phase = "in world: Lantern\xC3\xA9";  // a word that is not one of ours (it never is): never written as it stands
  s.label = "C:\\Users\\someone";
  s.banner = nullptr;
  const std::string long_word(400, 'x');
  s.init = long_word.c_str();
  ViewLines lines;
  view_lines(s, lines);
  const std::string text = all_text(lines);
  CHECK(text.find("Lantern") != std::string::npos);  // letters stay letters,
  CHECK(text.find("in world") == std::string::npos);  // but a space, a colon,
  CHECK(text.find(':') == std::string::npos);         // a path's separators
  CHECK(text.find('\\') == std::string::npos);
  CHECK(text.find("\xC3") == std::string::npos);      // and anything outside ASCII become '?'
  CHECK(text.find("banner none") != std::string::npos);
  CHECK(text.find("groups (no snapshot yet)") != std::string::npos);
  CHECK(text.find("none yet") != std::string::npos);
  for (const ViewLine& l : lines) CHECK(std::strlen(l.text.data()) < kViewLineBytes);
  CHECK(text.find(std::string(kViewLineBytes, 'x')) == std::string::npos);
  CHECK(only_ours(lines));
}

TEST_CASE("view: eight changes fill the list within its lines") {
  ViewStatus s = sample();
  s.changes.clear();
  for (int i = 0; i < 8; ++i) (void)s.changes.try_push_back(ViewChange{"phase", "loading", 100u * static_cast<unsigned>(i)});
  ViewLines lines;
  view_lines(s, lines);
  CHECK(lines.size() <= kViewLines);
  CHECK(std::string(lines[lines.size() - 1].text.data()).find("phase loading") != std::string::npos);
}

TEST_CASE("view: put_word") {
  char out[8] = {};
  CHECK(put_word("in_world", out, 8) == 8);
  CHECK(std::string(out, 8) == "in_world");
  CHECK(put_word(nullptr, out, 8) == 4);
  CHECK(std::string(out, 4) == "none");
  CHECK(put_word("a b", out, 8) == 3);
  CHECK(std::string(out, 3) == "a?b");
}
