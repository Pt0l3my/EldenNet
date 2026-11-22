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
//      that class (en-gal's probe::SlotSearch), again every minute while it finds none; several slots that hold one
//      object are copies of one address and name it; and a count of its class's objects in the image's own data;
//    - the member search (lodestar_trial.hpp): what the objects behind the statics hold, or point at, of an unnamed
//      lead's class;
//    - the clock search (lodestar_trial.hpp) over the accessor's statics that no lead has named: a manager with no type
//      information is known by how its fields behave;
//    - the rising-counter search (probe::RisingSearch) over the data's slots that point at the heap: the play time.
//    The candidates are the data's slots whose value is an address where Windows says memory may be read (most
//    values that could be an address are not one); the searches share 4 ms of each of T2's steps.
//    What they find is in the report as counts; only a manager a slot search names enters the draft table.
//    A search follows pointers that may be stale, so its reads, and T1's one read at each instance, ask first
//    (AskFirstCopier): the pages must be committed and readable, with no guard page among them, as VirtualQuery says;
//    only then are they copied, by ReadProcessMemory on our own process. So a search raises no exception in the game
//    and lifts no guard page (WHY§111, §160), whatever a pointer holds. What it will not read is counted.
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
#include <set>

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
constexpr std::size_t kMaxRisingCandidates = std::size_t{1} << 14;
constexpr std::uint64_t kSearchShareUs = 4'000;  // of each step of the reader's thread, for all the searches together

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

// What Windows says of the memory at an address, without reading it: memory that may be read, none (free, or only
// reserved), or memory that may not be read (a guard page, a page with no access, or a protection that is not one of
// the readable ones).
en::gal::probe::Liveness liveness(const std::optional<en::gal::probe::MemoryRegion>& r) noexcept {
  namespace probe = en::gal::probe;
  if (!r || !r->committed || r->size == 0) return probe::Liveness::NotCommitted;
  const std::uint32_t kind = r->protect & 0xFFu;
  const bool readable =
      (r->protect & (probe::kPageGuard | probe::kPageNoAccess)) == 0 &&
      (kind == probe::kPageReadOnly || kind == probe::kPageReadWrite || kind == probe::kPageWriteCopy ||
       kind == probe::kPageExecuteRead || kind == probe::kPageExecuteReadWrite || kind == probe::kPageExecuteWriteCopy);
  return readable ? probe::Liveness::Live : probe::Liveness::NoAccess;
}

// A copy that asks first. Every page of the range must be committed and readable, and none a guard page or a page
// with no access: VirtualQuery says so, and reads nothing. Only then is the range copied, by ReadProcessMemory on our
// own process, which fails quietly if the pages have changed in between. What it is asked, what it will not read
// (and why) and what failed all the same are counted.
class AskFirstCopier final : public en::gal::Copier {
 public:
  explicit AskFirstCopier(en::gal::NativeSys& sys) : sys_(sys) {}
  [[nodiscard]] en::gal::CopyOutcome copy(std::uint64_t address, std::span<std::byte> dst) noexcept override {
    namespace probe = en::gal::probe;
    asked_.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t end = address + dst.size();
    std::uint64_t at = address;
    for (int regions = 0; at < end; ++regions) {
      const std::optional<probe::MemoryRegion> r = regions < 4 ? sys_.region_at(at) : std::nullopt;
      const probe::Liveness l = liveness(r);
      if (l != probe::Liveness::Live) {
        (l == probe::Liveness::NotCommitted ? gone_ : access_).fetch_add(1, std::memory_order_relaxed);
        std::fill(dst.begin(), dst.end(), std::byte{0});
        return {false, 0xC0000005u};
      }
      at = r->base + r->size;
    }
    const en::gal::CopyOutcome done = sys_.copier_of(en::gal::CopyKind::Rpm).copy(address, dst);
    if (!done.copied) failed_.fetch_add(1, std::memory_order_relaxed);
    return done;
  }
  [[nodiscard]] std::uint64_t asked() const noexcept { return asked_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t refused_gone() const noexcept { return gone_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t refused_access() const noexcept { return access_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t copy_failed() const noexcept { return failed_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t refused() const noexcept { return refused_gone() + refused_access(); }

 private:
  en::gal::NativeSys& sys_;
  std::atomic<std::uint64_t> asked_{0};
  std::atomic<std::uint64_t> gone_{0};
  std::atomic<std::uint64_t> access_{0};
  std::atomic<std::uint64_t> failed_{0};
};

// The system layer as the searches see it: the game's own, with the copy that asks first. It is never asked to call
// anything.
class QuietSys final : public en::gal::SysLayer {
 public:
  explicit QuietSys(en::gal::NativeSys& sys) : sys_(sys), copier_(sys) {}
  [[nodiscard]] const AskFirstCopier& asks() const noexcept { return copier_; }
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
  [[nodiscard]] en::gal::Copier& copier() noexcept override { return copier_; }
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
  AskFirstCopier copier_;
};

// The trial's body on T2: the thread named for the crash filter, then the trial, then the searches T1 has handed
// over. The searches together get kSearchShareUs of a step, each an equal part of it, so that the trial keeps its
// pace whatever they have to go through; a step in which a search's part ran out is counted. T1 makes a search and
// publishes it here; T2 only steps it.
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
    auto* slots = slots_.load(std::memory_order_acquire);
    auto* members = members_.load(std::memory_order_acquire);
    auto* clock = clock_.load(std::memory_order_acquire);
    auto* rising = rising_.load(std::memory_order_acquire);
    const std::uint64_t active = (slots != nullptr ? 1u : 0u) + (members != nullptr ? 1u : 0u) +
                                 (clock != nullptr ? 1u : 0u) + (rising != nullptr ? 1u : 0u);
    if (active != 0) {
      const std::uint64_t part = kSearchShareUs / active;
      bool cut = false;
      // A search is stepped a few reads at a time until it has nothing to do just now (a call that copied nothing),
      // or its part of the step is over.
      const auto drive = [&](auto&& step) {
        const std::uint64_t start = quiet_.now_us();
        for (;;) {
          const std::uint32_t before = reader_.copies();
          const bool finished = step();
          if (finished || reader_.copies() == before || reader_.spent()) return;
          if (quiet_.now_us() - start >= part) {
            cut = true;
            return;
          }
        }
      };
      if (slots != nullptr) drive([&] { return slots->step(gt, reader_, 16); });
      if (members != nullptr) drive([&] { return members->step(gt, reader_, 16, frame.qpc_now_us); });
      if (clock != nullptr) drive([&] { return clock->step(gt, reader_, 8, frame.qpc_now_us); });
      if (rising != nullptr) drive([&] { return rising->step(gt, reader_, 16, frame.qpc_now_us); });
      search_steps_.fetch_add(1, std::memory_order_relaxed);
      if (cut) search_cut_.fetch_add(1, std::memory_order_relaxed);
    }
    steps_.fetch_add(1, std::memory_order_release);
  }

  // Steps T2 has finished: a search T1 took back two steps ago is no longer in T2's hands.
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t search_steps() const noexcept { return search_steps_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t search_cut() const noexcept { return search_cut_.load(std::memory_order_relaxed); }

  void set_slots(en::gal::probe::SlotSearch* search) noexcept { slots_.store(search, std::memory_order_release); }
  void set_members(lodestar::MemberSearch* search) noexcept { members_.store(search, std::memory_order_release); }
  void set_clock(lodestar::ClockSearch* search) noexcept { clock_.store(search, std::memory_order_release); }
  void set_rising(en::gal::probe::RisingSearch* search) noexcept { rising_.store(search, std::memory_order_release); }
  [[nodiscard]] const en::gal::GuardedReader& reader() const noexcept { return reader_; }
  [[nodiscard]] const AskFirstCopier& asks() const noexcept { return quiet_.asks(); }

 private:
  lodestar::Trial& trial_;
  QuietSys quiet_;
  en::gal::GuardedReader reader_;  // the searches', over QuietSys; the trial has its own, over the game's
  std::atomic<en::gal::probe::SlotSearch*> slots_{nullptr};
  std::atomic<lodestar::MemberSearch*> members_{nullptr};
  std::atomic<lodestar::ClockSearch*> clock_{nullptr};
  std::atomic<en::gal::probe::RisingSearch*> rising_{nullptr};
  std::atomic<std::uint64_t> steps_{0};
  std::atomic<std::uint64_t> search_steps_{0};
  std::atomic<std::uint64_t> search_cut_{0};
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
  explicit Round(Shell& s) : s_(s), folder_(s.paths.data + L"\\probe"), ask_first_(*s.sys) {}

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
    for (const lodestar::VariantFound& v : a.variants) {
      log(Line("read").word("stage", "variant").num("hits", v.hits).num("accepted", v.accepted)
              .num("statics", v.statics_found).num("known", v.known).num("agreeing", v.agreeing)
              .num("new", v.statics_found - v.known).num("shared", v.shared_class_objects));
    }

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
    for (const lodestar::OtherLead& o : pass.others) {
      if (o.held != 0) log(Line("read").word("stage", "other_lead").num("pass", held_.passes).num("held", o.held));
    }
  }

  void add_named() {
    if (trial_ == nullptr) return;
    for (const lodestar::ManagerFound& m : held_.managers) (void)trial_->add(m);
  }

  // One 8-byte read on T1, under the reader's plausibility rules and by the copy that asks first; nothing when it
  // is refused or the memory cannot be read.
  std::optional<std::uint64_t> read_word(std::uint64_t address) {
    if (en::gal::implausible(address, 8, 8, s_.sys->stacks())) return std::nullopt;
    std::array<std::byte, 8> v{};
    if (!ask_first_.copy(address, v).copied) return std::nullopt;
    std::uint64_t out = 0;
    std::memcpy(&out, v.data(), 8);
    return out;
  }

  // The data's slots whose value is an address where memory may be read just now, as Windows says without reading
  // it: the searches' candidates. One region is remembered, since neighbouring values often share one.
  en::gal::probe::HeapSlots live_slots(std::size_t max) {
    namespace probe = en::gal::probe;
    std::uint64_t from = 0, to = 0;
    probe::Liveness kind = probe::Liveness::NotCommitted;
    const probe::LiveFn live = [&](std::uint64_t v) {
      if (v >= from && v < to) return kind;
      const std::optional<probe::MemoryRegion> r = s_.sys->region_at(v);
      const probe::Liveness l = liveness(r);
      if (r && r->size != 0) {
        from = r->base;
        to = r->base + r->size;
        kind = l;
      }
      return l;
    };
    return probe::heap_slots(*s_.sys, *image_, max, &live);
  }

  [[nodiscard]] bool named_at(std::uint32_t rva) const {
    return std::any_of(held_.managers.begin(), held_.managers.end(), [&](const lodestar::ManagerFound& m) {
      return m.status == en::gal::derive::Status::Derived && m.rva == rva;
    });
  }

  // The searches by what an object holds: started once, when a save is loaded or kSearchAfterMs after the trial began.
  void start_searches() {
    searching_ = true;
    const std::uint64_t base = s_.sys->module_base();
    const std::vector<lodestar::StaticFound> statics = lodestar::all_statics(found_.accessor);
    // The clock: every static, of any of the accessor's shapes, that no lead has named.
    std::vector<std::uint32_t> unnamed, every;
    for (const lodestar::StaticFound& st : statics) {
      every.push_back(st.rva);
      if (!named_at(st.rva)) unnamed.push_back(st.rva);
    }
    clock_ = new lodestar::ClockSearch(std::move(unnamed), base, image_->size_of_image);  // lives for the process
    body_->set_clock(clock_);
    // The play time: every slot of the data that points at memory that may be read.
    en::gal::probe::HeapSlots slots = live_slots(kMaxSlotCandidates);
    search_.pointer_like = slots.pointer_like;
    search_.live = slots.slots.size();
    search_.not_committed = slots.not_committed;
    search_.no_access = slots.no_access;
    rising_ = new en::gal::probe::RisingSearch(std::move(slots), 0x100, kMaxRisingCandidates);
    body_->set_rising(rising_);
    // The members: what the objects behind the statics hold or point at, for every lead not named yet and every
    // lead the catalogue does not hold.
    std::vector<lodestar::Namer::LeadTable> tables;
    for (lodestar::Namer::LeadTable& t : namer_->lead_tables()) {
      const bool named = t.id != 0 && std::any_of(held_.managers.begin(), held_.managers.end(),
                                                  [&](const lodestar::ManagerFound& m) {
                                                    return m.id == t.id && m.status == en::gal::derive::Status::Derived;
                                                  });
      if (!named) tables.push_back(std::move(t));
    }
    const std::size_t member_tables = tables.size();
    if (!tables.empty()) {
      members_ = new lodestar::MemberSearch(std::move(every), std::move(tables), base, image_->size_of_image);
      body_->set_members(members_);
    }
    log(Line("read").word("stage", "searches").num("clock_candidates", clock_->found().candidates)
            .num("pointer_like", search_.pointer_like).num("live", search_.live)
            .num("not_committed", search_.not_committed).num("no_access", search_.no_access)
            .num("rising_candidates", rising_->candidates()).num("member_tables", member_tables)
            .num("ms", since_ms()));
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
    const std::uint64_t base = s_.sys->module_base();
    if (slot_search_ != nullptr) {
      if (slot_search_->checked() < slot_search_->candidates()) return;  // T2 is still at it
      body_->set_slots(nullptr);
      const auto found = slot_search_->found();
      const auto objects = slot_search_->found_objects();
      lodestar::SlotSearchResult result;
      result.slots.assign(found.begin(), found.end());
      result.objects = static_cast<std::uint32_t>(std::set<std::uint64_t>(objects.begin(), objects.end()).size());
      result.candidates = static_cast<std::uint32_t>(slot_search_->candidates());
      result.checked = static_cast<std::uint32_t>(slot_search_->checked());
      result.unreadable = static_cast<std::uint32_t>(slot_search_->unreadable());
      if (!objects.empty()) {
        // The object behind the first slot found: its table of methods, read once here.
        const std::optional<std::uint64_t> word = read_word(objects[0]);
        if (word && *word >= base && *word - base < image_->size_of_image) {
          result.vtable = static_cast<std::uint32_t>(*word - base);
        }
      }
      lodestar::set_by_search(held_, slot_id_, result);
      log(Line("read").word("stage", "search").num("id", slot_id_).num("candidates", result.candidates)
              .num("checked", result.checked).num("found", slot_search_->found_total()).num("objects", result.objects)
              .num("unreadable", result.unreadable).num("named", held_.named));
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
      // Named, or the search has found its class behind slots already (two objects: that is for the review).
      if (m.status == en::gal::derive::Status::Derived || m.search_found != 0) continue;
      const auto last = searched_at_.find(m.id);
      if (last != searched_at_.end() && since_ms() - last->second < kSearchAgainMs) continue;
      const std::vector<std::uint32_t> tables = namer_->vtables(m.id);
      if (tables.empty()) continue;  // no class of its name: nothing to look for
      if (last == searched_at_.end()) {
        // Before its first search: objects of its class in the image's own data (a read of the image, no more).
        std::uint64_t count = 0;
        std::uint32_t first = 0;
        for (const std::uint32_t t : tables) {
          const en::gal::probe::Holding h = en::gal::probe::slots_holding(*s_.sys, *image_, base + t, 1);
          if (count == 0 && !h.slots.empty()) first = h.slots.front();
          count += h.count;
        }
        lodestar::set_in_data(held_, m.id, static_cast<std::uint32_t>(count), first);
        log(Line("read").word("stage", "in_data").num("id", m.id).num("tables", tables.size()).num("found", count));
      }
      slot_set_ = new en::gal::VtableSet();  // the search keeps a reference to it: freed with it
      std::vector<std::uint64_t> absolute;
      for (const std::uint32_t t : tables) absolute.push_back(base + t);
      slot_set_->assign(std::move(absolute));
      slot_id_ = m.id;
      slot_search_ = new en::gal::probe::SlotSearch(live_slots(kMaxSlotCandidates), *slot_set_);
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
      r.risen = rising_->risen();
      for (const en::gal::probe::RisingSearch::Hit& h : rising_->hits()) {
        r.hits.push_back(lodestar::RisingHit{h.slot, h.offset, h.rose, h.still, h.other});
      }
      found_.rising = std::move(r);
    }
    if (searching_ && body_ != nullptr) {
      const AskFirstCopier& asks = body_->asks();
      search_.asked = asks.asked();
      search_.refused_gone = asks.refused_gone();
      search_.refused_access = asks.refused_access();
      search_.copy_failed = asks.copy_failed();
      search_.steps = body_->search_steps();
      search_.steps_cut = body_->search_cut();
      found_.search = search_;
    }
    if (members_ != nullptr) found_.members = members_->found();
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
                  .num("risen", rising_->risen())
                  .num("member_passes", members_ != nullptr ? members_->found().passes : 0)
                  .num("member_hits", members_ != nullptr ? members_->found().hits.size() : 0)
                  .num("search_faults", body_->reader().faults(en::gal::FaultKind::Copy))
                  .num("asked", body_->asks().asked()).num("refused_gone", body_->asks().refused_gone())
                  .num("refused_access", body_->asks().refused_access())
                  .num("copy_failed", body_->asks().copy_failed()).num("search_steps", body_->search_steps())
                  .num("search_cut", body_->search_cut()));
        }
      }
    }
  }

  Shell& s_;
  std::wstring folder_;
  AskFirstCopier ask_first_;  // T1's own, for its one read at each instance
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
  lodestar::MemberSearch* members_ = nullptr;
  lodestar::SearchFound search_;
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
