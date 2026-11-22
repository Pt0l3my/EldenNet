// The read-only round (Lodestar's step L5; proposal 0005 §6.1's `instance_class`, §6.2, §10; O92, O101), in a probe
// build of eldennet.dll with `probe = { round = "read" }`. It calls nothing of the game, registers nothing with it and
// writes nothing to it. On T1, after the window and `probe.settle_s` seconds:
// 1. The finder over the live image (en-gal's lodestar.hpp, with gal/leads.toml, recipes.toml, ids.toml and
//    builds.toml): the accessor's shape and every static its sites load, the leads' classes, the groups' table.
// 2. The managers named by their instances (O92): for each of those statics, the instance it holds and that
//    instance's first word, read once through the guarded copy under the reader's plausibility rules, and the word's
//    class by the image's own type information. Again every 10 s while a lead is unnamed, for at most ten minutes: a
//    manager the game makes only once a save is loaded is null at the title. What is named once stays named.
// 3. The reader's trial (lodestar_trial.hpp): T2, a thread of our own as in a v1 start (boot.cpp), reads each named
//    manager's slot and its instance's first word about ten times a second, and counts what it saw.
// 4. Every 2 s, probe\lodestar-found.json (report kind `lodestar`, schema 2: numbers, fixed words and the catalogue's
//    labels) and, when the names change, probe\lodestar-table.toml: the round's findings as a build's table, each
//    entry checked against the image and a lead until its review.
// T1 stays as the round's reporter for the session. No file carries a name, a byte or a text of the game (O75).
#include "dll.hpp"

#if EN_PROBE

#include <array>
#include <cstdio>
#include <cstring>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/lodestar_trial.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/task.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

namespace w = en::proto::watch;
namespace lodestar = en::gal::lodestar;
using en::core::BannerReason;
using en::shell::Line;
using en::shell::LogLevel;

namespace {

constexpr std::uint64_t kNameEveryMs = 10'000;  // between naming passes, while a lead is unnamed
constexpr std::uint32_t kMaxPasses = 60;        // ten minutes of them
constexpr std::uint64_t kReportEveryMs = 2'000;
constexpr std::uint64_t kFirstReadingMs = 30'000;

void state(w::InitState s) {
  block::state(s);
  log(Line("init").word("state", w::to_string(s)));
}

// T2 in this round: en-gal's ReaderLoop over the trial, for the life of the process (boot.cpp's reader, likewise).
struct ReaderStart {
  en::gal::SysLayer* sys = nullptr;
  en::gal::TaskBody* body = nullptr;
};

DWORD WINAPI trial_reader_main(void* arg) noexcept {
  return en::proto::ffi_guard(en::proto::Component::Task, DWORD{1}, [&]() -> DWORD {
    const ReaderStart start = *static_cast<const ReaderStart*>(arg);  // lives for the process
    en::gal::ReaderLoop loop(*start.sys, *start.body);
    for (;;) {
      const std::uint64_t wait_us = loop.step();
      Sleep(static_cast<DWORD>((wait_us + 999) / 1000));
    }
  });
}

// The trial's body on T2: the thread named for the crash filter, then the trial.
class Body final : public en::gal::TaskBody {
 public:
  explicit Body(lodestar::Trial& trial) : trial_(trial) {}
  void run(const en::proto::GameThread& gt, const en::gal::FrameCtx& frame) override {
    if (!named_) {
      named_ = true;
      block::reader_thread(GetCurrentThreadId());
    }
    trial_.run(gt, frame);
  }

 private:
  lodestar::Trial& trial_;
  bool named_ = false;
};

std::string today() {
  SYSTEMTIME t{};
  GetLocalTime(&t);  // the day, as the PC has it: a date and nothing else
  char text[16] = {};
  std::snprintf(text, sizeof text, "%04u-%02u-%02u", static_cast<unsigned>(t.wYear), static_cast<unsigned>(t.wMonth),
                static_cast<unsigned>(t.wDay));
  return text;
}

class Round {
 public:
  explicit Round(Shell& s) : s_(s), folder_(s.paths.data + L"\\probe") {}

  void run(const en::gal::Gated& gated) {
    (void)make_dirs(folder_);
    state(w::InitState::Discover);
    log(Line("read").word("stage", "settle").num("s", s_.settings.probe.settle_s));
    Sleep(s_.settings.probe.settle_s * 1000);

    // ---- gal/'s files: what to look for, how, and under which ids ----
    const std::optional<std::string> leads_text = read_text(s_.paths.gal + L"\\leads.toml", 64u << 10);
    const std::optional<std::string> recipes_text = read_text(s_.paths.gal + L"\\recipes.toml", 256u << 10);
    const std::optional<std::string> ids_text = read_text(s_.paths.gal + L"\\ids.toml", 256u << 10);
    const std::optional<std::string> builds_text = read_text(s_.paths.gal + L"\\builds.toml", 256u << 10);
    if (!leads_text || !recipes_text || !ids_text || !builds_text) return stop("read_files");
    auto leads = en::gal::derive::parse_leads(*leads_text, "leads.toml");
    auto recipes = en::gal::parse_recipes(*recipes_text, {});
    auto ids = en::gal::parse_ids(*ids_text);
    auto builds = en::gal::parse_builds(*builds_text);
    if (!leads || !recipes || !ids || !builds) {
      log(Line("read").word("stage", "files").flag("leads", leads.has_value()).flag("recipes", recipes.has_value())
              .flag("ids", ids.has_value()).flag("builds", builds.has_value()), LogLevel::Error);
      return stop("read_files");
    }
    leads_ = std::move(*leads);
    ids_ = std::move(*ids);
    image_ = &gated.image;

    // ---- 1. The finder, over the image as the game runs it ----
    const std::uint64_t t0 = since_ms();
    found_ = lodestar::find(*image_, s_.sys->image(), 0, leads_, *recipes, &*builds);
    const lodestar::AccessorFound& a = found_.accessor;
    log(Line("read").word("stage", "found").flag("known_build", found_.image.known_build).flag("recipe", a.recipe)
            .num("hits", a.hits).num("accepted", a.accepted).num("statics", a.statics_found)
            .num("class_objects", a.class_objects).num("classes", found_.classes.size()).num("ms", since_ms() - t0));

    // ---- 2. The managers, by their instances: the first pass ----
    name_pass();
    provenance_ = en::gal::TableProvenance{std::string(kVersionLabel), std::string(64, '0'), "l5", today(),
                                           {"gal/leads.toml", "gal/recipes.toml"}};
    write(true);
    block::install_filter();  // the window is up: the game's C runtime's filter is in, ours goes over it (WHY§172)

    // ---- 3. The reader's trial, on T2 ----
    state(w::InitState::Reader);
    trial_ = new lodestar::Trial(*s_.sys, s_.sys->module_base());  // lives for the process
    add_named();
    auto* body = new Body(*trial_);
    auto* start = new ReaderStart{s_.sys, body};
    // Made suspended, so that the stack table is made again with its stack in it and without T1's (boot.cpp; WHY§190).
    const HANDLE reader = CreateThread(nullptr, 0, &trial_reader_main, start, CREATE_SUSPENDED, nullptr);
    const std::size_t stacks = reader != nullptr ? s_.sys->rescan_stacks() : 0;
    const bool started = reader != nullptr && ResumeThread(reader) != static_cast<DWORD>(-1);
    if (reader != nullptr) CloseHandle(reader);
    log(Line("reader").flag("started", started).num("stacks", stacks).word("round", "read"));
    if (!started) return stop("reader");

    state(w::InitState::FirstReading);
    const std::uint64_t asked = since_ms();
    while (trial_->readings() == 0) {
      if (since_ms() - asked >= kFirstReadingMs) {
        log(Line("first_reading").word("state", "none").word("why", "no_reading"), LogLevel::Error);
        push_banner(s_, BannerReason::Inert, "no_reading");
        block::ready_ms(static_cast<std::uint32_t>(since_ms()));
        state(w::InitState::Inert);
        return report_loop();
      }
      Sleep(50);
    }
    log(Line("first_reading").word("state", "running").num("ms", since_ms() - asked));
    block::ready_ms(static_cast<std::uint32_t>(since_ms()));
    state(w::InitState::Ready);
    report_loop();
  }

 private:
  void stop(const char* detail) {
    log(Line("read").word("stage", "stopped").word("why", detail), LogLevel::Error);
    push_banner(s_, BannerReason::DiscoveryFailed, detail);
    block::ready_ms(static_cast<std::uint32_t>(since_ms()));
    state(w::InitState::Banner);
  }

  // One naming pass, folded into what the round holds. The one read outside the image is the instance's first word:
  // 8 bytes, through the crash-guarded copy, under the reader's plausibility rules (never a thread's stack).
  void name_pass() {
    const std::uint64_t t = since_ms();
    const lodestar::ReadWord read_word = [this](std::uint64_t address) -> std::optional<std::uint64_t> {
      if (en::gal::implausible(address, 8, 8, s_.sys->stacks())) return std::nullopt;
      std::array<std::byte, 8> v{};
      if (!s_.sys->copier().copy(address, v).copied) return std::nullopt;
      std::uint64_t out = 0;
      std::memcpy(&out, v.data(), 8);
      return out;
    };
    const lodestar::InstancesFound pass =
        lodestar::name_instances(*image_, s_.sys->image(), found_.accessor, leads_, ids_, read_word);
    lodestar::merge(held_, pass);
    last_pass_ms_ = since_ms();
    // For the launcher's console: how many of the managers asked for are named, and in how many passes.
    block::items(static_cast<std::uint32_t>(held_.named), static_cast<std::uint32_t>(held_.managers.size()),
                 held_.passes);
    log(Line("read").word("stage", "named").num("pass", held_.passes).num("tried", pass.tried).num("null", pass.null)
            .num("unread", pass.unread).num("unclassed", pass.unclassed).num("classed", pass.classed)
            .num("named", held_.named).num("ambiguous", held_.ambiguous).num("of", held_.managers.size())
            .num("unlisted", held_.unlisted).num("ms", last_pass_ms_ - t));
  }

  void add_named() {
    if (trial_ == nullptr) return;
    for (const lodestar::ManagerFound& m : held_.managers) (void)trial_->add(m);
  }

  // The report, and the table when the names changed (or `table` asks).
  void write(bool table) {
    found_.instances = held_;
    if (trial_ != nullptr) found_.trial = trial_->found();
    const auto text = lodestar::write_report(found_);
    if (!text) {
      log(Line("read").word("report", "refused"), LogLevel::Error);
    } else {
      (void)write_text(folder_ + L"\\lodestar-found.json", *text);
    }
    if (!table && held_.named == tabled_) return;
    tabled_ = held_.named;
    const lodestar::DraftTable draft = lodestar::draft_table(found_, ids_, provenance_, *image_, s_.sys->image());
    (void)write_text(folder_ + L"\\lodestar-table.toml", draft.text);
    log(Line("read").word("stage", "table").num("entries", draft.entries).num("left_out", draft.left_out)
            .num("witnesses_dropped", draft.witnesses_dropped));
  }

  // T1 as the round's reporter, for the session: the naming again while a lead is unnamed, the files every 2 s, and a
  // line of the trial's counts every 10 s.
  [[noreturn]] void report_loop() {
    std::uint64_t said = 0;
    for (;;) {
      Sleep(static_cast<DWORD>(kReportEveryMs));
      const bool unnamed = held_.named < held_.managers.size();
      if (unnamed && held_.passes < kMaxPasses && since_ms() - last_pass_ms_ >= kNameEveryMs) {
        name_pass();
        add_named();
      }
      write(false);
      if (trial_ != nullptr && since_ms() - said >= 10'000) {
        said = since_ms();
        const lodestar::TrialFound t = trial_->found();
        std::uint64_t same = 0, null = 0, other = 0, fault = 0, torn = 0, faults = 0;
        for (const lodestar::TrialManager& m : t.managers) {
          same += m.same;
          null += m.null;
          other += m.other;
          fault += m.fault;
          torn += m.torn;
        }
        for (const std::uint64_t f : t.faults) faults += f;
        log(Line("read").word("stage", "trial").num("passes", held_.passes).num("named", held_.named)
                .num("of", held_.managers.size()).num("managers", t.managers.size()).num("readings", t.readings)
                .num("same", same).num("null", null).num("other", other).num("fault", fault).num("torn", torn)
                .num("faults", faults).num("p99_us", t.reading_p99_us).num("max_us", t.reading_max_us));
      }
    }
  }

  Shell& s_;
  std::wstring folder_;
  const en::gal::ImageInfo* image_ = nullptr;  // the gate's, which lives for the process
  en::gal::derive::Leads leads_;
  en::gal::Catalogue ids_;
  lodestar::Found found_;
  lodestar::InstancesFound held_;
  en::gal::TableProvenance provenance_;
  lodestar::Trial* trial_ = nullptr;
  std::uint64_t last_pass_ms_ = 0;
  std::uint64_t tabled_ = ~std::uint64_t{0};
};

}  // namespace

void run_read(Shell& s, const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>* gated) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (gated == nullptr || !*gated) return;  // an unknown build took the banner path before this
    auto* round = new Round(s);                // lives for the process (R14): T1 stays its reporter
    round->run(**gated);
  });
}

}  // namespace eldennet

#else

namespace eldennet {
// A release build has no probe round.
void run_read(Shell&, const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>*) noexcept {}
}  // namespace eldennet

#endif  // EN_PROBE
