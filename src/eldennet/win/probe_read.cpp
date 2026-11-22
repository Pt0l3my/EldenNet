// The read-only round (Lodestar's step L5; proposal 0005 §6.1's `instance_class`, §6.2, §10; O92, O101), in a probe
// build of eldennet.dll with `probe = { round = "read" }`. It calls nothing of the game, registers nothing with it and
// writes nothing to it. On T1, after the window and `probe.settle_s` seconds:
// 1. The finder over the live image (en-gal's lodestar.hpp, with gal/leads.toml, recipes.toml, ids.toml and
//    builds.toml): the accessor's shape and every static its sites load, the leads' classes, the groups' table.
// 2. The managers named by their instances (O92): for each of those statics, the instance it holds and that
//    instance's first word, read once through the guarded copy under the reader's plausibility rules, and the word's
//    class by the image's own type information. Again every 10 s for as long as a lead is unnamed: a manager the
//    game makes only once a save is loaded is null at the title. What is named once stays named. The image's type
//    information is read once (lodestar::Namer); a later pass reads the 217 slots and one word at each instance.
// 3. The reader's trial (lodestar_trial.hpp): T2, a thread of our own as in a v1 start (boot.cpp), reads each named
//    manager's slot and its instance's first word about ten times a second, and counts what it saw.
// 4. Every 2 s, probe\lodestar-found.json (report kind `lodestar`, schema 2: numbers, fixed words and the catalogue's
//    labels) and, when the names change, probe\lodestar-table.toml: the round's findings as a build's table, each
//    entry checked against the image and a lead until its review.
// 5. Once a save is loaded (the characters' manager has an instance), or three minutes after the trial began, the
//    searches by what an object holds (step L6's first), each read-only, on T2 within its own budget:
//    - for a lead that has a class of its name and still no static, the search of the data's slots for an object of
//      that class (en-gal's probe::SlotSearch), again every minute while it finds none;
//    - the clock search (lodestar_trial.hpp) over the accessor's statics that no lead has named: a manager with no type
//      information, such as the world's clock, is known by how its fields behave;
//    - the rising-counter search (probe::RisingSearch) over the data's slots that point at the heap: the play time.
//    What they find is in the report as counts; only a manager a slot search names enters the draft table.
//    A search follows pointers that may be stale, so its reads, and T1's one read at each instance, go through
//    ReadProcessMemory on our own process (QuietSys), which raises no exception in the game whatever the address.
//    The trial reads as v1 will, through the settings' copy primitive.
// T1 stays as the round's reporter for the session. No file carries a name, a byte or a text of the game (O75).
#include "dll.hpp"

#if EN_PROBE

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/discovery.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/lodestar_trial.hpp"
#include "en/gal/native_sys.hpp"
#include "en/gal/probe.hpp"
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

constexpr std::uint64_t kNameEveryMs = 10'000;  // between naming passes, for as long as a lead is unnamed
constexpr std::uint64_t kReportEveryMs = 2'000;
constexpr std::uint64_t kFirstReadingMs = 30'000;
constexpr std::uint64_t kSearchAfterMs = 180'000;  // the searches start then, if no save was loaded sooner
constexpr std::uint64_t kSearchAgainMs = 60'000;   // a slot search that found nothing is made again
constexpr std::size_t kMaxSlotCandidates = std::size_t{1} << 20;

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

// The system layer as the searches see it: the game's own, with the copy primitive that raises no exception. It is
// never asked to call anything.
class QuietSys final : public en::gal::SysLayer {
 public:
  explicit QuietSys(en::gal::NativeSys& sys) : sys_(sys) {}
  [[nodiscard]] std::uint64_t module_base() const noexcept override { return sys_.module_base(); }
  [[nodiscard]] const en::gal::ByteSource& image() const noexcept override { return sys_.image(); }
  [[nodiscard]] en::proto::Expected<std::uint64_t, en::gal::SysError> call_getter(std::uint64_t,
                                                                                 std::uint64_t) noexcept override {
    return en::proto::Unexpected{en::gal::SysError::Unsupported};
  }
  [[nodiscard]] en::proto::Expected<std::uint64_t, en::gal::SysError> register_task(
      std::uint64_t, std::uint64_t, const en::gal::TaskShape&, en::gal::TaskBody&) noexcept override {
    return en::proto::Unexpected{en::gal::SysError::Unsupported};
  }
  [[nodiscard]] en::gal::Copier& copier() noexcept override { return sys_.copier_of(en::gal::CopyKind::Rpm); }
  [[nodiscard]] en::gal::StackRanges& stacks() noexcept override { return sys_.stacks(); }
  void note_own_stack() noexcept override { sys_.note_own_stack(); }
  [[nodiscard]] std::uint64_t now_us() const noexcept override { return sys_.now_us(); }
  [[nodiscard]] std::uint32_t thread_id() const noexcept override { return sys_.thread_id(); }
  [[nodiscard]] std::uint64_t slot_calls(std::uint32_t) const noexcept override { return 0; }
  [[nodiscard]] std::uint64_t task_object() const noexcept override { return 0; }
  bool forget_stale_stacks(std::uint64_t address, std::uint64_t len) noexcept override {
    return sys_.forget_stale_stacks(address, len);
  }

 private:
  en::gal::NativeSys& sys_;
};

// The trial's body on T2: the thread named for the crash filter, then the trial, then the searches T1 has handed
// over, each a step within a budget of its own. T1 makes a search and publishes it here; T2 only steps it.
class Body final : public en::gal::TaskBody {
 public:
  Body(en::gal::NativeSys& sys, lodestar::Trial& trial)
      : trial_(trial), quiet_(sys), reader_(quiet_, en::gal::ReadBudget{2048, 1u << 20}) {}
  void run(const en::proto::GameThread& gt, const en::gal::FrameCtx& frame) override {
    if (!named_) {
      named_ = true;
      block::reader_thread(GetCurrentThreadId());
      reader_.bind_thread();
    }
    trial_.run(gt, frame);
    reader_.begin_tick();
    if (auto* slots = slots_.load(std::memory_order_acquire)) (void)slots->step(gt, reader_, 1024);
    if (auto* clock = clock_.load(std::memory_order_acquire)) (void)clock->step(gt, reader_, 32, frame.qpc_now_us);
    if (auto* rising = rising_.load(std::memory_order_acquire)) (void)rising->step(gt, reader_, 48, frame.qpc_now_us);
    steps_.fetch_add(1, std::memory_order_release);
  }

  // Steps T2 has finished: a search T1 took back two steps ago is no longer in T2's hands.
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_.load(std::memory_order_acquire); }

  void set_slots(en::gal::probe::SlotSearch* search) noexcept { slots_.store(search, std::memory_order_release); }
  void set_clock(lodestar::ClockSearch* search) noexcept { clock_.store(search, std::memory_order_release); }
  void set_rising(en::gal::probe::RisingSearch* search) noexcept { rising_.store(search, std::memory_order_release); }
  [[nodiscard]] const en::gal::GuardedReader& reader() const noexcept { return reader_; }

 private:
  lodestar::Trial& trial_;
  QuietSys quiet_;
  en::gal::GuardedReader reader_;  // the searches', over QuietSys; the trial has its own, over the game's
  std::atomic<en::gal::probe::SlotSearch*> slots_{nullptr};
  std::atomic<lodestar::ClockSearch*> clock_{nullptr};
  std::atomic<en::gal::probe::RisingSearch*> rising_{nullptr};
  std::atomic<std::uint64_t> steps_{0};
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
    found_ = lodestar::find(*image_, s_.sys->image(), 0, leads_, *recipes, &*builds, &ids_);
    const lodestar::AccessorFound& a = found_.accessor;
    log(Line("read").word("stage", "found").flag("known_build", found_.image.known_build).flag("recipe", a.recipe)
            .num("hits", a.hits).num("accepted", a.accepted).num("statics", a.statics_found)
            .num("class_objects", a.class_objects).num("classes", found_.classes.size()).num("ms", since_ms() - t0));

    // ---- 2. The managers, by their instances: the first pass ----
    namer_ = new lodestar::Namer(*image_, s_.sys->image(), leads_, ids_);  // lives for the process
    name_pass();
    provenance_ = en::gal::TableProvenance{std::string(kVersionLabel), std::string(64, '0'), "l5", today(),
                                           {"gal/leads.toml", "gal/recipes.toml"}};
    write(true);
    block::install_filter();  // the window is up: the game's C runtime's filter is in, ours goes over it (WHY§172)

    // ---- 3. The reader's trial, on T2 ----
    state(w::InitState::Reader);
    trial_ = new lodestar::Trial(*s_.sys, s_.sys->module_base());  // lives for the process
    // The characters' manager exists only with a save loaded: its instance tells the trial's readings' stage.
    trial_->set_stage(en::gal::roles::info(en::gal::roles::Item::Characters).id);
    add_named();
    body_ = new Body(*s_.sys, *trial_);
    auto* body = body_;
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
    trial_at_ms_ = since_ms();
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
    const lodestar::ReadWord word = [this](std::uint64_t address) { return read_word(address); };
    const lodestar::InstancesFound pass = namer_->pass(s_.sys->image(), found_.accessor, word);
    lodestar::merge(held_, pass);
    last_pass_ms_ = since_ms();
    // For the launcher's console: how many of the managers asked for are named, and in how many passes.
    block::items(static_cast<std::uint32_t>(held_.named), static_cast<std::uint32_t>(held_.managers.size()),
                 held_.passes);
    log(Line("read").word("stage", "named").num("pass", held_.passes).num("tried", pass.tried).num("null", pass.null)
            .num("unread", pass.unread).num("in_image", pass.in_image).num("unclassed", pass.unclassed)
            .num("classed", pass.classed)
            .num("named", held_.named).num("ambiguous", held_.ambiguous).num("of", held_.managers.size())
            .num("unlisted", held_.unlisted).num("ms", last_pass_ms_ - t));
  }

  void add_named() {
    if (trial_ == nullptr) return;
    for (const lodestar::ManagerFound& m : held_.managers) (void)trial_->add(m);
  }

  // One 8-byte read on T1, under the reader's plausibility rules and by the copy that raises no exception; nothing
  // when it is refused or the memory cannot be read.
  std::optional<std::uint64_t> read_word(std::uint64_t address) const {
    if (en::gal::implausible(address, 8, 8, s_.sys->stacks())) return std::nullopt;
    std::array<std::byte, 8> v{};
    if (!s_.sys->copier_of(en::gal::CopyKind::Rpm).copy(address, v).copied) return std::nullopt;
    std::uint64_t out = 0;
    std::memcpy(&out, v.data(), 8);
    return out;
  }

  // The searches by what an object holds: started once, when a save is loaded or kSearchAfterMs after the trial began.
  void start_searches() {
    searching_ = true;
    const std::uint64_t base = s_.sys->module_base();
    // The clock: every static of the accessor's that no lead has named.
    std::vector<std::uint32_t> unnamed;
    for (const lodestar::StaticFound& st : found_.accessor.statics) {
      const bool named = std::any_of(held_.managers.begin(), held_.managers.end(), [&](const lodestar::ManagerFound& m) {
        return m.status == en::gal::derive::Status::Derived && m.rva == st.rva;
      });
      if (!named) unnamed.push_back(st.rva);
    }
    clock_ = new lodestar::ClockSearch(std::move(unnamed), base, image_->size_of_image);  // lives for the process
    body_->set_clock(clock_);
    // The play time: every slot of the data that points at the heap.
    rising_ = new en::gal::probe::RisingSearch(en::gal::probe::heap_slots(*s_.sys, *image_, 1u << 16), 0x100, 1u << 14);
    body_->set_rising(rising_);
    log(Line("read").word("stage", "searches").num("clock_candidates", clock_->found().candidates)
            .num("rising_candidates", rising_->candidates()).num("ms", since_ms()));
  }

  // The slot search for one lead that has a class of its name and no static yet: made here, stepped on T2, read back
  // here when it has checked every candidate. One at a time; a lead whose search found nothing is tried again later.
  void slot_searches() {
    // A search taken back from T2 is freed once T2 has finished two more steps: it cannot be inside it any more.
    if (retired_ != nullptr) {
      if (body_->steps() < retired_step_ + 2) return;
      delete retired_;
      delete retired_set_;
      retired_ = nullptr;
      retired_set_ = nullptr;
    }
    if (slot_search_ != nullptr) {
      if (slot_search_->checked() < slot_search_->candidates()) return;  // T2 is still at it
      body_->set_slots(nullptr);
      const auto found = slot_search_->found();
      std::uint32_t vtable = 0;
      if (!found.empty()) {
        // The object behind the first slot found: its table of methods, read once here.
        std::array<std::byte, 8> raw{};
        std::uint64_t instance = 0;
        if (s_.sys->image().read(found[0], raw)) std::memcpy(&instance, raw.data(), 8);
        const std::optional<std::uint64_t> word = instance != 0 ? read_word(instance) : std::nullopt;
        const std::uint64_t base = s_.sys->module_base();
        if (word && *word >= base && *word - base < image_->size_of_image) vtable = static_cast<std::uint32_t>(*word - base);
      }
      lodestar::set_by_search(held_, slot_id_, found.empty() ? 0 : found[0], vtable,
                              static_cast<std::uint32_t>(found.size()));
      log(Line("read").word("stage", "search").num("id", slot_id_).num("candidates", slot_search_->candidates())
              .num("found", found.size()).num("unreadable", slot_search_->unreadable()).num("named", held_.named));
      block::items(static_cast<std::uint32_t>(held_.named), static_cast<std::uint32_t>(held_.managers.size()),
                   held_.passes);
      add_named();
      // T2 may be inside its last step of it: it is freed two of T2's steps from now (above).
      retired_ = slot_search_;
      retired_set_ = slot_set_;
      retired_step_ = body_->steps();
      slot_search_ = nullptr;
      slot_set_ = nullptr;
      searched_at_[slot_id_] = since_ms();
      return;
    }
    for (const lodestar::ManagerFound& m : held_.managers) {
      if (m.status == en::gal::derive::Status::Derived || m.search_found > 1) continue;
      const auto last = searched_at_.find(m.id);
      if (last != searched_at_.end() && since_ms() - last->second < kSearchAgainMs) continue;
      const std::vector<std::uint32_t> tables = namer_->vtables(m.id);
      if (tables.empty()) continue;  // no class of its name: nothing to look for
      slot_set_ = new en::gal::VtableSet();  // the search keeps a reference to it: freed with it
      std::vector<std::uint64_t> absolute;
      for (const std::uint32_t t : tables) absolute.push_back(s_.sys->module_base() + t);
      slot_set_->assign(std::move(absolute));
      slot_id_ = m.id;
      slot_search_ = new en::gal::probe::SlotSearch(*s_.sys, *image_, *slot_set_, kMaxSlotCandidates);
      body_->set_slots(slot_search_);
      return;
    }
  }

  // The report, and the table when the names changed (or `table` asks).
  void write(bool table) {
    found_.instances = held_;
    if (trial_ != nullptr) found_.trial = trial_->found();
    if (clock_ != nullptr) found_.clock = clock_->found();
    if (rising_ != nullptr) {
      lodestar::RisingFound r;
      r.candidates = rising_->candidates();
      r.passes = rising_->passes();
      r.unreadable = rising_->unreadable();
      for (const en::gal::probe::RisingSearch::Hit& h : rising_->hits()) {
        r.hits.push_back(lodestar::RisingHit{h.slot, h.offset, h.rose, h.still, h.other});
      }
      found_.rising = std::move(r);
    }
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
    std::uint64_t wrote = since_ms();
    for (;;) {
      Sleep(500);
      // The searches' hand-overs twice a second; the naming, the files and the lines at their own paces.
      if (trial_ != nullptr && body_ != nullptr) {
        if (!searching_ && (trial_->found().world_readings != 0 || since_ms() - trial_at_ms_ >= kSearchAfterMs)) {
          start_searches();
        }
        if (searching_) slot_searches();
      }
      if (since_ms() - wrote < kReportEveryMs) continue;
      wrote = since_ms();
      const bool unnamed = held_.named < held_.managers.size();
      if (unnamed && since_ms() - last_pass_ms_ >= kNameEveryMs) {
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
                .num("faults", faults).num("world", t.world_readings).num("stage_changes", t.stage_changes)
                .num("p99_us", t.reading_p99_us).num("max_us", t.reading_max_us));
        if (clock_ != nullptr && rising_ != nullptr) {
          const lodestar::ClockFound c = clock_->found();
          log(Line("read").word("stage", "held").num("clock_passes", c.passes).num("clock_hits", c.hits.size())
                  .num("clocks", static_cast<std::uint64_t>(
                                     std::count_if(c.hits.begin(), c.hits.end(), [](const auto& h) { return h.has_hour; })))
                  .num("rising_passes", rising_->passes()).num("rising_hits", rising_->hits().size())
                  .num("search_faults", body_->reader().faults(en::gal::FaultKind::Copy)));
        }
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
  lodestar::Namer* namer_ = nullptr;
  lodestar::Trial* trial_ = nullptr;
  Body* body_ = nullptr;
  lodestar::ClockSearch* clock_ = nullptr;
  en::gal::probe::RisingSearch* rising_ = nullptr;
  en::gal::probe::SlotSearch* slot_search_ = nullptr;  // the one T2 is stepping, if any
  en::gal::VtableSet* slot_set_ = nullptr;
  en::gal::probe::SlotSearch* retired_ = nullptr;      // the one taken back from T2, to be freed
  en::gal::VtableSet* retired_set_ = nullptr;
  std::uint64_t retired_step_ = 0;
  std::uint32_t slot_id_ = 0;
  std::map<std::uint32_t, std::uint64_t> searched_at_;  // by id: when its last slot search ended
  bool searching_ = false;
  std::uint64_t trial_at_ms_ = 0;
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
