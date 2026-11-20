// The F11 view's lines (view.hpp): words of ours and numbers, built into fixed buffers with no allocation.
#include "en/overlay/view.hpp"

#include <cstdio>

namespace en::overlay {

namespace {

bool word_char(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
         c == '+' || c == '-';
}

// One line, written piece by piece: this file's own text, a word of ours (checked again), or a number.
class Builder {
 public:
  explicit Builder(ViewLine& line) : line_(line) {}

  Builder& text(const char* t) noexcept {
    for (; t != nullptr && *t != '\0'; ++t) put(*t);
    return *this;
  }
  Builder& word(const char* w) noexcept {
    const std::size_t room = kViewLineBytes - 1 - n_;
    n_ += put_word(w, line_.text.data() + n_, room);
    line_.text[n_] = '\0';
    return *this;
  }
  Builder& num(std::uint64_t v) noexcept {
    char digits[24];
    const int k = std::snprintf(digits, sizeof digits, "%llu", static_cast<unsigned long long>(v));
    for (int i = 0; i < k; ++i) put(digits[i]);
    return *this;
  }
  Builder& version(std::uint32_t packed) noexcept {  // GRACEUI_VERSION_PACK: 10, 10 and 12 bits
    return num(packed >> 22).text(".").num((packed >> 12) & 0x3FFu).text(".").num(packed & 0xFFFu);
  }
  Builder& yes_no(bool v) noexcept { return text(v ? "yes" : "no"); }
  // Tenths of a second, for ages: "12.3".
  Builder& seconds(std::uint64_t ms) noexcept { return num(ms / 1000).text(".").num((ms % 1000) / 100); }

 private:
  void put(char c) noexcept {
    if (n_ + 1 >= kViewLineBytes) return;
    line_.text[n_++] = c;
    line_.text[n_] = '\0';
  }

  ViewLine& line_;
  std::size_t n_ = 0;
};

const char* state_word(proto::GroupState s) noexcept {
  switch (s) {
    case proto::GroupState::Ok: return "ok";
    case proto::GroupState::Tripped: return "tripped";
    case proto::GroupState::Untested: break;
  }
  return "untested";
}

Builder line(ViewLines& out, bool heading = false) noexcept {
  ViewLine fresh;
  fresh.heading = heading;
  if (!out.try_push_back(fresh)) {
    // Full: the last line is written over (kViewLines is sized so that this never happens).
    out[out.size() - 1] = fresh;
  }
  return Builder(out[out.size() - 1]);
}

}  // namespace

std::size_t put_word(const char* word, char* out, std::size_t max) noexcept {
  const char* w = word == nullptr || *word == '\0' ? "none" : word;
  std::size_t n = 0;
  for (; *w != '\0' && n < max; ++w) out[n++] = word_char(*w) ? *w : '?';
  return n;
}

void view_lines(const ViewStatus& s, ViewLines& out) noexcept {
  out.clear();
  line(out, true).text(kRibbonText);
  line(out)
      .text("EldenNet ")
      .num(s.version[0])
      .text(".")
      .num(s.version[1])
      .text(".")
      .num(s.version[2])
      .text("  build ")
      .word(s.label)
      .text(s.probe_build ? "  (probe build)" : "");
  line(out, true).text("Start and lifecycle");
  line(out)
      .text("  init ")
      .word(s.init)
      .text("   discovery ")
      .num(s.items_resolved)
      .text("/")
      .num(s.items_total)
      .text(" items, ")
      .num(s.passes)
      .text(s.passes == 1 ? " pass" : " passes");
  line(out).text("  phase ").word(s.phase).text("   level ").word(s.level).text("   banner ").word(s.banner);
  line(out)
      .text("  gate ")
      .text(s.gate_open ? "open" : "closed")
      .text(" (")
      .word(s.gate_why)
      .text(")   dormant ")
      .yes_no(s.dormant)
      .text("   badge ")
      .yes_no(s.badge);
  line(out, true).text("Task and read groups");
  line(out)
      .text("  snapshots ")
      .num(s.snapshots)
      .text("   task p99 ")
      .num(s.task_p99_us)
      .text(" us   reads ")
      .word(s.read_mode)
      .text(s.throttled ? " (throttled)" : "");
  if (!s.have_health) {
    line(out).text("  groups (no snapshot yet)");
  } else {
    // Three lines of groups, four to a line: each "name state", with its hard faults this tick when there were any.
    for (std::size_t from = 0; from < proto::kGroupCount; from += 4) {
      Builder b = line(out);
      b.text(" ");
      for (std::size_t g = from; g < from + 4 && g < proto::kGroupCount; ++g) {
        const proto::GroupHealth& h = s.health.groups[g];
        b.text(" ").word(proto::to_string(static_cast<proto::Group>(g))).text(" ").text(state_word(h.state));
        if (h.faults != 0) b.text("(").num(h.faults).text(")");
      }
    }
  }
  {
    Builder b = line(out);
    b.text("  tripped");
    bool any = false;
    for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
      if ((s.tripped >> g & 1u) != 0) {
        b.text(" ").word(proto::to_string(static_cast<proto::Group>(g)));
        any = true;
      }
    }
    if (!any) b.text(" none");
  }
  {
    Builder b = line(out);
    b.text("  copies ").num(s.copies).text("   faults");
    for (std::size_t k = 0; k < kViewFaultKinds; ++k) {
      if (s.fault_names[k] != nullptr) b.text(" ").word(s.fault_names[k]).text(" ").num(s.faults[k]);
    }
  }
  line(out, true).text("Marks and GraceUI");
  line(out)
      .text("  marks ")
      .num(s.marks)
      .text(" (")
      .word(s.mark_source)
      .text(")   GraceUI ")
      .version(s.graceui_version);
  line(out, true).text("Last changes");
  if (s.changes.size() == 0) {
    line(out).text("  none yet");
  }
  for (std::size_t i = s.changes.size(); i-- > 0;) {  // the newest first
    const ViewChange& c = s.changes[i];
    line(out)
        .text("  ")
        .seconds(s.now_ms >= c.at_ms ? s.now_ms - c.at_ms : 0)
        .text(" s ago  ")
        .word(c.what)
        .text(" ")
        .word(c.to);
  }
}

}  // namespace en::overlay
