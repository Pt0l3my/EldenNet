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
//    - the watch (lodestar_trial.hpp; from the moment the trial starts): once a second it follows the way to each
//      named manager's object, to each place the member search found an unnamed lead's object, and to the object
//      whose field rose by the clock, and counts how the first 512 bytes behave; the world's clock is also watched
//      for minutes and hours packed into bits and for a rising fraction;
//    - for the world's clock, the watch's wide ways: the object behind every static, every six seconds, for minutes
//      and hours packed into bits and for a rising fraction;
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
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "en/gal/backend.hpp"
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
// The fourteenth round's two loads (the record's note on the owner's save file): the last character of the list is in
// the save's slot 6, the usual one in slot 2. The pair search looks for the field that reads the one and then the
// other. The marks at which the game is in the first world, and in the second (the usual character's).
constexpr std::uint32_t kPairFirst = 6;
constexpr std::uint32_t kPairSecond = 2;
constexpr std::uint32_t kFirstWorldMark = 1;
constexpr std::uint32_t kSecondWorldMark = 3;
constexpr std::size_t kMaxRisingCandidates = std::size_t{1} << 14;
constexpr std::size_t kFlagHops = 8;            // objects one step on from the flags' store
constexpr std::size_t kFlagHopsDeeper = 4;      // objects one step more, from each of those
constexpr std::size_t kFlagHopsDeeperAll = 32;  // and of those in all
constexpr std::uint64_t kBackendEveryUs = 10'000'000;  // EldenNet's own reader on the flags: every ten seconds
constexpr std::uint64_t kWorldEveryUs = 1'000'000;     // and on the world's leads: once a second
// The survey's open candidates of the eighteenth round (a character's object; our own numbers, leads): two places
// for the type, the block a character is in now and two places for the map it belongs to, and the dead bit with its
// two rivals (the byte times 8, plus the bit).
constexpr std::uint32_t kTypeAtA = 0x68, kTypeAtB = 0x40, kDesignNpcType = 5;
constexpr std::uint32_t kBlockNowAt = 0x38, kBlockHomeAtA = 0x70, kBlockHomeAtB = 0xC;
constexpr std::uint32_t kDeadBit = 0x1C6 * 8 + 0, kDeadRivalA = 0x1C6 * 8 + 1, kDeadRivalB = 0x53B * 8 + 4;
constexpr std::uint32_t kSurveyMark = 1;               // plan 1: the mark at which the tester stands in the world
constexpr std::size_t kBackendFlags = 64;              // flags it is asked for at most
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
    auto* rising = rising_.load(std::memory_order_acquire);
    auto* watch = watch_.load(std::memory_order_acquire);
    auto* sweep = sweep_.load(std::memory_order_acquire);
    if (sweep != nullptr && !sweep->due(frame.qpc_now_us)) sweep = nullptr;  // it rests between its passes
    auto* tree = tree_.load(std::memory_order_acquire);
    if (tree != nullptr && tree->done()) tree = nullptr;  // walked once
    // The state search's pass, while one is asked for, has the searches' whole share to itself: the game must stay
    // in the state the tester put it in until the pass is through, so it is to be quick; the others wait.
    auto* state = state_.load(std::memory_order_acquire);
    if (state != nullptr && state->due()) {
      const std::uint64_t start = quiet_.now_us();
      for (;;) {
        const std::uint32_t before = reader_.copies();
        const bool ended = state->step(gt, reader_, quiet_.now_us());
        if (ended || reader_.spent() || quiet_.now_us() - start >= kSearchShareUs) break;
        if (reader_.copies() == before && !state->due()) break;
      }
      slots = nullptr;
      members = nullptr;
      rising = nullptr;
      watch = nullptr;
      sweep = nullptr;
      tree = nullptr;
    } else if (auto* pair = pair_.load(std::memory_order_acquire); pair != nullptr && pair->due()) {
      // The pair search's look, likewise: the first and the second world.
      const std::uint64_t start = quiet_.now_us();
      for (;;) {
        const bool ended = pair->step(gt, reader_, quiet_.now_us());
        if (ended || reader_.spent() || quiet_.now_us() - start >= kSearchShareUs) break;
      }
      slots = nullptr;
      members = nullptr;
      rising = nullptr;
      watch = nullptr;
      sweep = nullptr;
      tree = nullptr;
    } else if (auto* parts = parts_.load(std::memory_order_acquire);
               parts != nullptr && parts->due(quiet_.now_us())) {
      // The parts watch's pass, once a second: some forty reads, and the tester's phases are short. It goes before
      // the character search's look, which may take a minute (71 s in the sixteenth round) and left the watch five
      // standing passes then.
      const std::uint64_t start = quiet_.now_us();
      for (;;) {
        const bool ended = parts->step(gt, reader_, quiet_.now_us());
        if (ended || reader_.spent() || quiet_.now_us() - start >= kSearchShareUs) break;
      }
      slots = nullptr;
      members = nullptr;
      rising = nullptr;
      watch = nullptr;
      sweep = nullptr;
      tree = nullptr;
    } else if (auto* chars = chars_.load(std::memory_order_acquire); chars != nullptr && chars->due()) {
      // The character search's look, next: it too reads the game in the state the tester put it in.
      const std::uint64_t start = quiet_.now_us();
      for (;;) {
        const bool ended = chars->step(gt, reader_, quiet_.now_us());
        if (ended || reader_.spent() || quiet_.now_us() - start >= kSearchShareUs) break;
      }
      slots = nullptr;
      members = nullptr;
      rising = nullptr;
      watch = nullptr;
      sweep = nullptr;
      tree = nullptr;
    } else if (auto* survey = survey_.load(std::memory_order_acquire);
               survey != nullptr && survey->due(quiet_.now_us())) {
      // The character survey's pass (plan 1), every two seconds: some hundred and fifty reads.
      const std::uint64_t start = quiet_.now_us();
      for (;;) {
        const bool ended = survey->step(gt, reader_, quiet_.now_us());
        if (ended || reader_.spent() || quiet_.now_us() - start >= kSearchShareUs) break;
      }
      slots = nullptr;
      members = nullptr;
      rising = nullptr;
      watch = nullptr;
      sweep = nullptr;
      tree = nullptr;
    } else if (tree != nullptr) {
      // The tree's walk, likewise, until it is through: it is read once, and what the round compares waits for it.
      // With a share among the others it took seven minutes, and the eleventh round's session ended 47 records
      // short of it; alone it takes about one.
      const std::uint64_t start = quiet_.now_us();
      for (;;) {
        const std::uint32_t before = reader_.copies();
        const bool ended = tree->step(gt, reader_);
        if (ended || reader_.spent() || reader_.copies() == before) break;
        if (quiet_.now_us() - start >= kSearchShareUs) break;
      }
      slots = nullptr;
      members = nullptr;
      rising = nullptr;
      watch = nullptr;
      sweep = nullptr;
      tree = nullptr;
    }
    const std::uint64_t active = (slots != nullptr ? 1u : 0u) + (members != nullptr ? 1u : 0u) +
                                 (rising != nullptr ? 1u : 0u) + (watch != nullptr ? 1u : 0u) +
                                 (sweep != nullptr ? 1u : 0u) + (tree != nullptr ? 1u : 0u);
    if (active != 0) {
      const std::uint64_t part = kSearchShareUs / active;
      bool cut = false;
      // A search is stepped one read at a time until it has nothing to do just now (a call that copied nothing), or
      // its part of the step is over: a read that asks first costs about half a millisecond on the owner's PC (the
      // fifth round), so the clock is looked at after every one.
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
      // The watch first: it has the least to read, and wants its second kept.
      if (watch != nullptr) drive([&] { return watch->step(gt, reader_, 1, frame.qpc_now_us); });
      if (slots != nullptr) drive([&] { return slots->step(gt, reader_, 1); });
      if (members != nullptr) drive([&] { return members->step(gt, reader_, 1, frame.qpc_now_us); });
      if (rising != nullptr) drive([&] { return rising->step(gt, reader_, 1, frame.qpc_now_us); });
      if (sweep != nullptr) drive([&] { return sweep->step(gt, reader_, frame.qpc_now_us); });
      if (tree != nullptr) drive([&] { return tree->step(gt, reader_); });
      search_steps_.fetch_add(1, std::memory_order_relaxed);
      if (cut) search_cut_.fetch_add(1, std::memory_order_relaxed);
    }
    // EldenNet's own reader on the flags (WHY§457): the backend's flags group alone, every ten seconds, with the
    // list of flags T1 last handed over. It reads through this thread's careful reads (asked first), outside the
    // searches' share: a call takes as long as the tree is deep for each group asked.
    if (auto* backend = backend_.load(std::memory_order_acquire)) {
      if (!backend_bound_) {
        backend_bound_ = true;
        (void)backend->first_tick(gt, false);
      }
      // At once when T1 has a new list, else every ten seconds.
      const bool fresh = flag_list_.load(std::memory_order_acquire) != nullptr;
      if (fresh || frame.qpc_now_us - backend_at_us_ >= kBackendEveryUs) {
        backend_at_us_ = frame.qpc_now_us;
        if (const FlagList* list = flag_list_.exchange(nullptr, std::memory_order_acq_rel)) {
          (void)backend->apply(
              gt, en::proto::GameCommand{en::proto::GameCommand::SetWatchFlags{list->version, list->ids}});
          flag_count_ = std::min(list->ids->size(), kBackendFlags);
        }
        const en::gal::ErBackend::FlagsAlone done = backend->flags_alone(gt, snapshot_);
        backend_calls_.fetch_add(1, std::memory_order_relaxed);
        if (done.read) backend_reads_.fetch_add(1, std::memory_order_relaxed);
        // What the last call that read it said: a call after the game has let its store go reads nothing, and must
        // not take the answer back (the twelfth round's report said "not set" so).
        if (done.known) backend_known_.store(*done.known ? 1 : 0, std::memory_order_relaxed);
        if (snapshot_.flags) {
          for (std::size_t i = 0; i < flag_count_; ++i) {
            const unsigned state = (snapshot_.flags->valid[i] ? 1u : 0u) | (snapshot_.flags->bits[i] ? 2u : 0u);
            flag_state_[i].store(static_cast<std::uint8_t>(state), std::memory_order_relaxed);
          }
          flag_version_.store(snapshot_.flags->watch_version, std::memory_order_release);
        }
      } else if (frame.qpc_now_us - world_at_us_ >= kWorldEveryUs) {
        // EldenNet's own reader on the world's leads (WHY§567 to §569): the game's state's slot, the main player
        // with its health and position, and the near list, through the build's layout, once a second.
        // In the survey's plan every other frame also reads the near list's characters' other fields, through the
        // layout: one copy of each at the layout's size, every two seconds, as the survey's own passes.
        world_at_us_ = frame.qpc_now_us;
        world_fields_now_ = world_fields_.load(std::memory_order_acquire) && !world_fields_now_;
        world_frame(backend->world_alone(gt, world_fields_now_));
      }
    }
    steps_.fetch_add(1, std::memory_order_release);
  }

  // Steps T2 has finished: a search T1 took back two steps ago is no longer in T2's hands.
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t search_steps() const noexcept { return search_steps_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t search_cut() const noexcept { return search_cut_.load(std::memory_order_relaxed); }

  void set_slots(en::gal::probe::SlotSearch* search) noexcept { slots_.store(search, std::memory_order_release); }
  void set_members(lodestar::MemberSearch* search) noexcept { members_.store(search, std::memory_order_release); }
  void set_rising(en::gal::probe::RisingSearch* search) noexcept { rising_.store(search, std::memory_order_release); }
  void set_watch(lodestar::Watch* watch) noexcept { watch_.store(watch, std::memory_order_release); }
  void set_sweep(lodestar::Sweep* sweep) noexcept { sweep_.store(sweep, std::memory_order_release); }
  void set_tree(lodestar::TreeRead* tree) noexcept { tree_.store(tree, std::memory_order_release); }
  void set_state(lodestar::StateSearch* state) noexcept { state_.store(state, std::memory_order_release); }
  void set_chars(lodestar::CharSearch* chars) noexcept { chars_.store(chars, std::memory_order_release); }
  void set_pair(lodestar::PairSearch* pair) noexcept { pair_.store(pair, std::memory_order_release); }
  void set_parts(lodestar::PartsWatch* parts) noexcept { parts_.store(parts, std::memory_order_release); }
  void set_survey(lodestar::CharSurvey* survey) noexcept { survey_.store(survey, std::memory_order_release); }
  // EldenNet's own reader: built by T1 over this thread's careful reads, stepped here. A list of flags is handed
  // over by its address; T1 keeps every list it ever handed over.
  struct FlagList {
    std::uint32_t version = 0;
    std::shared_ptr<const std::vector<std::uint32_t>> ids;
  };
  struct FlagsRead {
    std::uint64_t calls = 0;
    std::uint64_t reads = 0;
    int known = -1;             // the known flag at the last call: 1 set, 0 not set, -1 not read
    std::uint32_t version = 0;  // the list the states belong to
    std::array<std::uint8_t, kBackendFlags> states{};  // bit 0: valid; bit 1: set
  };
  // The tester's next mark, from T1: the world reader's frames are counted by the marks so far.
  void world_mark() noexcept { world_marks_.fetch_add(1, std::memory_order_release); }
  // Whether the world reader's frames also read the list's characters' other fields (the survey's plan).
  void set_world_fields(bool on) noexcept { world_fields_.store(on, std::memory_order_release); }
  // What the world reader's frames gave so far, for T1 (a copy, under the lock T2 only ever tries).
  [[nodiscard]] lodestar::WorldRead world_read() const {
    const std::lock_guard<std::mutex> lock(world_mutex_);
    return world_shared_;
  }
  [[nodiscard]] en::gal::SysLayer& careful_sys() noexcept { return quiet_; }
  void set_backend(en::gal::ErBackend* backend) noexcept { backend_.store(backend, std::memory_order_release); }
  void set_flag_list(const FlagList* list) noexcept { flag_list_.store(list, std::memory_order_release); }
  [[nodiscard]] FlagsRead flags_read() const noexcept {
    FlagsRead out;
    out.calls = backend_calls_.load(std::memory_order_relaxed);
    out.reads = backend_reads_.load(std::memory_order_relaxed);
    out.known = backend_known_.load(std::memory_order_relaxed);
    out.version = flag_version_.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < kBackendFlags; ++i) out.states[i] = flag_state_[i].load(std::memory_order_relaxed);
    return out;
  }
  [[nodiscard]] const en::gal::GuardedReader& reader() const noexcept { return reader_; }
  [[nodiscard]] const AskFirstCopier& asks() const noexcept { return quiet_.asks(); }

 private:
  lodestar::Trial& trial_;
  QuietSys quiet_;
  en::gal::GuardedReader reader_;  // the searches', over QuietSys; the trial has its own, over the game's
  std::atomic<en::gal::probe::SlotSearch*> slots_{nullptr};
  std::atomic<lodestar::MemberSearch*> members_{nullptr};
  std::atomic<en::gal::probe::RisingSearch*> rising_{nullptr};
  std::atomic<lodestar::Watch*> watch_{nullptr};
  std::atomic<lodestar::Sweep*> sweep_{nullptr};
  std::atomic<lodestar::TreeRead*> tree_{nullptr};
  std::atomic<lodestar::StateSearch*> state_{nullptr};
  std::atomic<lodestar::CharSearch*> chars_{nullptr};
  std::atomic<lodestar::PairSearch*> pair_{nullptr};
  std::atomic<lodestar::PartsWatch*> parts_{nullptr};
  std::atomic<lodestar::CharSurvey*> survey_{nullptr};
  std::atomic<en::gal::ErBackend*> backend_{nullptr};
  std::atomic<const FlagList*> flag_list_{nullptr};
  en::proto::WorldSnapshot snapshot_;  // the backend's, of which only the flags are filled
  bool backend_bound_ = false;
  std::uint64_t backend_at_us_ = 0;

  // One frame of the world reader, counted by the marks so far. Nothing is allocated here: the lists were sized
  // when the body was made. The position stays on this thread: only whether it moved is kept.
  void world_frame(const en::gal::ErBackend::WorldAlone& a) {
    lodestar::WorldRead& w = world_local_;
    const std::size_t span =
        std::min<std::size_t>(world_marks_.load(std::memory_order_acquire), lodestar::WorldRead::kSpans - 1);
    w.lacks = a.lacks;
    ++w.calls;
    w.state_frames += a.state ? 1 : 0;
    w.slot_frames += a.save_slot && *a.save_slot >= 0 ? 1 : 0;
    w.player_frames += a.player ? 1 : 0;
    if (a.hp) {
      ++w.hp_frames;
      w.hp_over += a.hp->max == 0 || a.hp->cur > a.hp->max ? 1 : 0;
    }
    w.list_frames += a.list ? 1 : 0;
    w.near_most = std::max(w.near_most, a.near_all);
    w.near_read += a.near_read;
    w.near_chr += a.near_chr;
    w.near_distance += a.near_distance;
    w.near_rising += a.near_rising;
    if (a.pos) {
      ++w.pos_frames;
      if (world_prev_ok_ && world_prev_span_ == span) {
        ++w.pos_frames_by[span];
        const float dx = (*a.pos)[0] - world_prev_[0], dz = (*a.pos)[2] - world_prev_[2];
        if (std::fabs(dx) > 0.01f || std::fabs(dz) > 0.01f) ++w.pos_moved_by[span];
      }
      world_prev_ = *a.pos;
      world_prev_span_ = span;
    }
    world_prev_ok_ = a.pos.has_value();
    w.fields_bound = a.fields_bound;
    if (a.fields) {
      ++w.fields_frames;
      w.chr_self += a.near_self;
      w.chr_object += a.near_object;
      w.chr_handle += a.near_handle;
      w.chr_param += a.near_param;
      w.chr_kin += a.near_kin;
      w.chr_entity += a.near_entity;
      w.chr_dead += a.near_dead;
      w.chr_on_screen += a.near_on_screen;
      w.handles_shared += a.near_handles < a.near_handle ? 1 : 0;
      w.objects_most = std::max(w.objects_most, a.near_object);
      w.handles_most = std::max(w.handles_most, a.near_handles);
      w.kinds_most = std::max(w.kinds_most, a.near_kinds);
      w.entities_most = std::max(w.entities_most, a.near_entities);
      w.teams_most = std::max(w.teams_most, a.near_teams);
      w.team_group_most = std::max(w.team_group_most, a.near_team_most);
      if (a.self_object) {
        ++w.self_frames;
        w.self_handle_frames += a.self_handle ? 1 : 0;
        w.self_param_frames += a.self_param ? 1 : 0;
        w.self_dead_frames += a.self_dead ? 1 : 0;
        w.self_team_alone_frames += a.self_team_alone ? 1 : 0;
      }
      w.chr_object_by[span] += a.near_object;
      w.chr_dead_by[span] += a.near_dead;
      w.chr_on_screen_by[span] += a.near_on_screen;
    }
    // The span's first frame: what the reader read just after the tester's mark (at the title's mark, the title).
    if (world_at_set_[span] == 0) {
      lodestar::WorldAt& at = w.at_marks[span];
      at = lodestar::WorldAt{};
      at.mark = static_cast<std::uint32_t>(span);
      at.there = a.state;
      at.loaded = a.save_slot && *a.save_slot >= 0;
      at.slot = at.loaded ? static_cast<std::uint32_t>(*a.save_slot) : 0;
      at.player = a.player;
      at.has_hp = a.hp.has_value();
      at.hp = a.hp ? a.hp->cur : 0;
      at.max_hp = a.hp ? a.hp->max : 0;
      at.near_all = a.near_all;
      world_at_set_[span] = 1;
    }
    const std::unique_lock<std::mutex> lock(world_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return;  // T1 is copying the last one: the next frame publishes
    world_shared_.lacks = w.lacks;
    world_shared_.calls = w.calls;
    world_shared_.state_frames = w.state_frames;
    world_shared_.slot_frames = w.slot_frames;
    world_shared_.player_frames = w.player_frames;
    world_shared_.hp_frames = w.hp_frames;
    world_shared_.hp_over = w.hp_over;
    world_shared_.pos_frames = w.pos_frames;
    world_shared_.list_frames = w.list_frames;
    world_shared_.near_most = w.near_most;
    world_shared_.near_read = w.near_read;
    world_shared_.near_chr = w.near_chr;
    world_shared_.near_distance = w.near_distance;
    world_shared_.near_rising = w.near_rising;
    std::copy(w.pos_frames_by.begin(), w.pos_frames_by.end(), world_shared_.pos_frames_by.begin());
    std::copy(w.pos_moved_by.begin(), w.pos_moved_by.end(), world_shared_.pos_moved_by.begin());
    std::copy(w.at_marks.begin(), w.at_marks.end(), world_shared_.at_marks.begin());
    world_shared_.fields_bound = w.fields_bound;
    world_shared_.fields_frames = w.fields_frames;
    world_shared_.chr_self = w.chr_self;
    world_shared_.chr_object = w.chr_object;
    world_shared_.chr_handle = w.chr_handle;
    world_shared_.chr_param = w.chr_param;
    world_shared_.chr_kin = w.chr_kin;
    world_shared_.chr_entity = w.chr_entity;
    world_shared_.chr_dead = w.chr_dead;
    world_shared_.chr_on_screen = w.chr_on_screen;
    world_shared_.handles_shared = w.handles_shared;
    world_shared_.objects_most = w.objects_most;
    world_shared_.handles_most = w.handles_most;
    world_shared_.kinds_most = w.kinds_most;
    world_shared_.entities_most = w.entities_most;
    world_shared_.teams_most = w.teams_most;
    world_shared_.team_group_most = w.team_group_most;
    world_shared_.self_frames = w.self_frames;
    world_shared_.self_handle_frames = w.self_handle_frames;
    world_shared_.self_param_frames = w.self_param_frames;
    world_shared_.self_dead_frames = w.self_dead_frames;
    world_shared_.self_team_alone_frames = w.self_team_alone_frames;
    std::copy(w.chr_object_by.begin(), w.chr_object_by.end(), world_shared_.chr_object_by.begin());
    std::copy(w.chr_dead_by.begin(), w.chr_dead_by.end(), world_shared_.chr_dead_by.begin());
    std::copy(w.chr_on_screen_by.begin(), w.chr_on_screen_by.end(), world_shared_.chr_on_screen_by.begin());
  }
  static lodestar::WorldRead sized_world() {
    lodestar::WorldRead w;
    w.pos_frames_by.assign(lodestar::WorldRead::kSpans, 0);
    w.pos_moved_by.assign(lodestar::WorldRead::kSpans, 0);
    w.at_marks.assign(lodestar::WorldRead::kSpans, lodestar::WorldAt{});
    w.chr_object_by.assign(lodestar::WorldRead::kSpans, 0);
    w.chr_dead_by.assign(lodestar::WorldRead::kSpans, 0);
    w.chr_on_screen_by.assign(lodestar::WorldRead::kSpans, 0);
    return w;
  }
  std::uint64_t world_at_us_ = 0;
  std::atomic<bool> world_fields_{false};
  bool world_fields_now_ = false;                     // this thread's: the last frame read the fields
  std::atomic<std::uint32_t> world_marks_{0};
  lodestar::WorldRead world_local_ = sized_world();   // this thread's
  lodestar::WorldRead world_shared_ = sized_world();  // under world_mutex_
  mutable std::mutex world_mutex_;
  std::array<std::uint8_t, lodestar::WorldRead::kSpans> world_at_set_{};  // 1: the span's first frame is kept
  std::array<float, 3> world_prev_{};
  std::size_t world_prev_span_ = 0;
  bool world_prev_ok_ = false;
  std::size_t flag_count_ = 0;
  std::atomic<std::uint64_t> backend_calls_{0};
  std::atomic<std::uint64_t> backend_reads_{0};
  std::atomic<int> backend_known_{-1};
  std::atomic<std::uint32_t> flag_version_{0};
  std::array<std::atomic<std::uint8_t>, kBackendFlags> flag_state_{};
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
    gated_ = &gated;
    (void)make_dirs(folder_);
    // The round's plan, as the launcher wrote it with its request: which tasks its panel lists, and so what the
    // tester's marks mean. A plan this build does not know is a refusal: nothing is read.
    plan_ = block::probe_plan();
    log(Line("read").word("stage", "plan").num("plan", plan_).flag("known", plan_ < w::kPlanCount),
        plan_ < w::kPlanCount ? LogLevel::Info : LogLevel::Error);
    if (plan_ >= w::kPlanCount) return stop("plan");
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
    body_->set_world_fields(plan_ == w::kPlanSurvey);
    // The watch: what the named managers' objects hold, by how their first bytes behave. It asks before each read,
    // as the searches do, and asks Windows what stands behind a word that could be an address (nothing is read).
    {
      en::gal::NativeSys* sys = s_.sys;
      watch_ = new lodestar::Watch(  // lives for the process
          s_.sys->module_base(), image_->size_of_image, namer_->primary_tables(), [sys](std::uint64_t at) {
            return liveness(sys->region_at(at)) == en::gal::probe::Liveness::Live;
          });
      add_ways();
      body_->set_watch(watch_);
    }
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
    // The state search (WHY§460: the game state's static, by a field that reads -1 at the title and the save's slot
    // in the world): over the data's slots that point at live memory now, at the title. Its first pass is asked for
    // at once; the later ones at the tester's marks (in the world, at the title again, in the world again).
    // Both are the rounds' up to the seventeenth (plan 0); the survey's plan has neither.
    if (plan_ == w::kPlanRounds) {
      en::gal::probe::HeapSlots slots = live_slots(kMaxSlotCandidates);
      state_ = new lodestar::StateSearch(s_.sys->module_base(), std::move(slots.slots), &s_.sys->image());
      state_->ask();
      body_->set_state(state_);
      log(Line("read").word("stage", "state").num("candidates", state_->found().candidates));
      make_chars();
    }
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
    // The clock: the object behind every static, of any of the accessor's shapes, watched for minutes and hours
    // packed into bits and for a rising fraction (the watch's wide ways), every six seconds.
    std::vector<std::uint32_t> every;
    std::size_t wide = 0;
    for (const lodestar::StaticFound& st : statics) {
      every.push_back(st.rva);
      lodestar::Watch::Way way{"static", 0, st.rva, 0, false, 0, true};
      way.wide = true;
      way.every_us = 3'000'000;  // a game minute is about three seconds (the seventh round): a step a reading
      wide += watch_ != nullptr && watch_->add(way) ? 1 : 0;
    }
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
    log(Line("read").word("stage", "searches").num("wide_ways", wide)
            .num("pointer_like", search_.pointer_like).num("live", search_.live)
            .num("not_committed", search_.not_committed).num("no_access", search_.no_access)
            .num("rising_candidates", rising_->candidates()).num("member_tables", member_tables)
            .num("ms", since_ms()));
  }

  // EldenNet's own reader on the flags (WHY§457): discovery through the build's layout as it stands (the table and
  // the flags' fields; a partial pass, on a thread of our own: it calls nothing in the game), and the backend built
  // on it over T2's careful reads. Once, when the searches start.
  // The character search (WHY§460's second step): made once the characters manager's static and the two character
  // classes' tables of methods are known, which is at the title; its first look is asked for at once, the later
  // ones at the tester's marks, with the state search's.
  void make_chars() {
    if (chars_ != nullptr || body_ == nullptr) return;
    const std::uint32_t manager = en::gal::roles::info(en::gal::roles::Item::Characters).id;
    const auto held = std::find_if(held_.managers.begin(), held_.managers.end(), [&](const lodestar::ManagerFound& m) {
      return m.id == manager && m.status == en::gal::derive::Status::Derived;
    });
    if (held == held_.managers.end()) return;
    const auto tables = [&](std::string_view label) {  // the catalogue's label of the class
      for (const lodestar::ClassFound& c : found_.classes) {
        const en::gal::IdEntry* id = ids_.find(c.label, true);
        if (id != nullptr && id->label == label && c.status == en::gal::derive::Status::Derived) return c.vtables;
      }
      return std::vector<std::uint32_t>{};
    };
    lodestar::CharSearch::Options o;
    o.base = s_.sys->module_base();
    o.image_size = s_.sys->image().size();
    o.manager_slot = held->rva;
    o.character_vtables = tables("character");
    o.player_vtables = tables("player_character");
    o.main_player_word = main_player_word_;
    o.list_word = list_word_;
    o.image = &s_.sys->image();
    if (o.character_vtables.empty() || o.player_vtables.empty()) return;
    const std::size_t classes = o.character_vtables.size(), players = o.player_vtables.size();
    chars_ = new lodestar::CharSearch(std::move(o));  // lives for the process
    // No look yet: the thirteenth round's first look, asked for as the manager was first named, ran through the
    // loading and saw the manager being filled. A look is asked for when the tester says the game is in the world.
    chars_manager_ = held->rva;
    body_->set_chars(chars_);
    log(Line("read").word("stage", "chars").num("classes", classes).num("player_classes", players)
            .num("mark", marks_seen_));
  }

  // The pair search: over the data's slots in use now, in the first world.
  void make_pair() {
    if (pair_ != nullptr || body_ == nullptr) return;
    en::gal::probe::HeapSlots slots = live_slots(kMaxSlotCandidates);
    pair_ = new lodestar::PairSearch(std::move(slots.slots), kPairFirst, kPairSecond,
                                     &s_.sys->image());  // lives for the process
    pair_->ask();
    body_->set_pair(pair_);
    log(Line("read").word("stage", "pair").num("candidates", pair_->found().candidates).num("first", kPairFirst)
            .num("second", kPairSecond));
  }

  // The parts watch, once the character search's look in the second world is through: it gives where in the manager
  // the main player's address stands, and which of the player's addresses is the holder of its parts (the one whose
  // target begins with the most addresses of classed objects).
  // The character survey (plan 1), from the build's layout: the characters manager's item, the main player's word,
  // the near list and its element, and the way to a character's health. Made at the tester's first mark, or as soon
  // after it as discovery is through; whatever the layout lacks is said, and the survey is then not made.
  void make_survey() {
    if (survey_ != nullptr || survey_tried_ || body_ == nullptr || discovery_ == nullptr) return;
    survey_tried_ = true;
    namespace roles = en::gal::roles;
    const en::gal::roles::Bound& b = discovery_->bound();
    const std::uint32_t manager = roles::info(roles::Item::Characters).id;
    const auto held = std::find_if(held_.managers.begin(), held_.managers.end(), [&](const lodestar::ManagerFound& m) {
      return m.id == manager && m.status == en::gal::derive::Status::Derived;
    });
    lodestar::CharSurvey::Options o;
    for (const lodestar::ClassFound& c : found_.classes) {
      const en::gal::IdEntry* id = ids_.find(c.label, true);
      if (id != nullptr && id->label == "character" && c.status == en::gal::derive::Status::Derived) {
        o.character_vtables = c.vtables;
      }
    }
    const en::gal::FieldLayout* main = b.field(roles::Fld::MainPlayer);
    const en::gal::FieldLayout* list = b.field(roles::Fld::ByDistance);
    const en::gal::FieldLayout* chr = b.field(roles::Fld::NearChr);
    const en::gal::FieldLayout* parts = b.field(roles::Fld::ChrParts);
    const en::gal::FieldLayout* vitals = b.field(roles::Fld::PartsVitals);
    const en::gal::FieldLayout* hp = b.field(roles::Fld::Hp);
    const std::uint32_t stride = b.stride_of(roles::Cls::NearEntry);
    const bool no_list = list == nullptr || chr == nullptr || stride == 0;
    const std::uint32_t lacks = (held == held_.managers.end() ? 1u : 0u) | (o.character_vtables.empty() ? 2u : 0u) |
                                (main == nullptr ? 4u : 0u) | (no_list ? 8u : 0u);
    if (lacks != 0) {
      log(Line("read").word("stage", "survey").flag("made", false).num("lacks", lacks), LogLevel::Warn);
      return;
    }
    o.base = s_.sys->module_base();
    o.image_size = s_.sys->image().size();
    o.manager_slot = held->rva;
    o.main_player_word = main->off;
    o.list_word = list->off;
    o.stride = stride;
    o.chr_at = chr->off;
    // The health, where the layout has the way to it; without it the survey sees no death.
    const bool health = parts != nullptr && vitals != nullptr && hp != nullptr;
    o.parts_at = health ? parts->off : 0;
    o.vitals_at = health ? vitals->off : 0;
    o.hp_at = health ? hp->off : 0;
    o.image = &s_.sys->image();
    // What the eighteenth round's survey left open, as checks of its own candidates (our numbers; WHY§605). The
    // type: the main player's reads nothing by the design, and a character of the world's the design's number for
    // one (NPC§1.4). The block: where a character is now against the map it belongs to. The dead bit's two rivals.
    using Check = lodestar::SurveyCheck;
    const auto check = [&](std::uint32_t kind, std::uint32_t a, std::uint32_t b) {
      Check c;
      c.kind = kind;
      c.a = a;
      c.b = b;
      o.checks.push_back(c);
    };
    for (const std::uint32_t type_at : {kTypeAtA, kTypeAtB}) {
      check(Check::kIs, type_at, 0);
      check(Check::kIs, type_at, kDesignNpcType);
      check(Check::kChanged, type_at, 0);
    }
    check(Check::kBlock, kBlockNowAt, 0);
    check(Check::kChanged, kBlockNowAt, 0);
    check(Check::kWithPlayer, kBlockNowAt, kBlockNowAt);
    for (const std::uint32_t home_at : {kBlockHomeAtA, kBlockHomeAtB}) {
      check(Check::kBlock, home_at, 0);
      check(Check::kChanged, home_at, 0);
      check(Check::kEqual, kBlockNowAt, home_at);
      check(Check::kWithPlayer, home_at, kBlockNowAt);
    }
    check(Check::kEqual, kBlockHomeAtA, kBlockHomeAtB);
    check(Check::kBitsEqual, kDeadBit, kDeadRivalA);
    check(Check::kBitsEqual, kDeadBit, kDeadRivalB);
    const std::size_t classes = o.character_vtables.size();
    survey_ = new lodestar::CharSurvey(std::move(o));  // lives for the process
    // Marks that came before discovery was through are phases gone by.
    for (std::uint32_t m = kSurveyMark; m < marks_seen_; ++m) survey_->mark(s_.sys->now_us());
    body_->set_survey(survey_);
    log(Line("read").word("stage", "survey").flag("made", true).num("classes", classes).flag("health", health)
            .num("mark", marks_seen_));
  }

  // The parts watch from the build's layout alone (the main player's word and the holder's place), at the second
  // world's mark: the tester's standing task is then the watch's from its start.
  void make_parts_by_layout() {
    if (parts_ != nullptr || parts_tried_ || body_ == nullptr) return;
    if (main_player_word_ == 0 || parts_holder_at_ == 0 || chars_manager_ == 0) return;
    parts_tried_ = true;
    lodestar::PartsWatch::Options o;
    o.player_word = main_player_word_;
    o.holder_at = parts_holder_at_;
    o.base = s_.sys->module_base();
    o.image_size = s_.sys->image().size();
    o.manager_slot = chars_manager_;
    o.image = &s_.sys->image();
    parts_ = new lodestar::PartsWatch(o);  // lives for the process
    body_->set_parts(parts_);
    log(Line("read").word("stage", "parts").flag("made", true).num("player_word", o.player_word)
            .num("holder_at", o.holder_at).flag("by_layout", true).num("mark", marks_seen_));
  }

  void make_parts(const lodestar::CharFound& chars) {
    if (parts_ != nullptr || parts_tried_ || body_ == nullptr) return;
    const lodestar::CharLook* look = nullptr;
    for (const lodestar::CharLook& l : chars.looks) {
      if (l.mark == kSecondWorldMark && l.through) look = &l;
    }
    if (look == nullptr) return;
    parts_tried_ = true;
    lodestar::PartsWatch::Options o;
    // The main player's word: the layout's, where the look saw an object of the player's class there; else the one
    // word that held such an object, where there was one object only. (A world may hold several objects of the
    // player's class: the fourteenth round's look met eight.)
    bool player = false;
    for (const lodestar::CharPointer& c : look->pointers) {
      if (c.player && main_player_word_ != 0 && c.offset == main_player_word_) {
        o.player_word = c.offset;
        player = true;
      }
    }
    for (const lodestar::CharPointer& c : look->pointers) {
      if (c.player && !player && look->players == 1) {
        o.player_word = c.offset;
        player = true;
      }
    }
    std::uint32_t most = 0;
    for (const lodestar::CharMember& m : look->members) {
      if (m.classed > most) {
        most = m.classed;
        o.holder_at = m.offset;
      }
    }
    if (!player || most < 6) {
      log(Line("read").word("stage", "parts").flag("made", false).num("players", look->players).num("classed", most),
          LogLevel::Warn);
      return;
    }
    o.base = s_.sys->module_base();
    o.image_size = s_.sys->image().size();
    o.manager_slot = chars_manager_;
    o.image = &s_.sys->image();
    parts_ = new lodestar::PartsWatch(o);  // lives for the process
    // Marks that came while the look ran are phases gone by.
    for (std::uint32_t m = kSecondWorldMark; m < marks_seen_; ++m) parts_->mark(s_.sys->now_us());
    body_->set_parts(parts_);
    log(Line("read").word("stage", "parts").flag("made", true).num("player_word", o.player_word)
            .num("holder_at", o.holder_at).num("classed", most).num("mark", marks_seen_));
  }

  void prepare_backend() {
    backend_tried_ = true;
    lodestar::BackendFound b;
    const auto text = [&](const std::string& name) {
      std::wstring wide(name.begin(), name.end());  // gal/'s names are ASCII
      for (wchar_t& c : wide) {
        if (c == L'/') c = L'\\';
      }
      return read_text(s_.paths.gal + L"\\" + wide, 256u << 10);
    };
    const std::optional<std::string> layout = text(gated_->profile.entry().layout);
    const std::optional<std::string> recipes = text("recipes.toml");
    std::optional<std::string> patterns = text("patterns.toml");
    if (!patterns) patterns = "schema = 1\n";
    if (!layout) {
      log(Line("read").word("stage", "backend").flag("layout", false), LogLevel::Warn);
      backend_found_ = b;
      return;
    }
    en::gal::DiscoveryOptions options;
    if (recipes) options.recipes = *recipes;
    if (const std::optional<std::string> ids = text("ids.toml")) options.ids = *ids;
    options.frame = en::gal::FrameProvider::OwnThread;
    options.mode = en::gal::LayoutMode::Dev;
    options.partial = true;
    const std::uint64_t t0 = since_ms();
    auto found = en::gal::discover(*s_.sys, *gated_, *patterns, *layout, options);
    if (!found) {
      log(Line("read").word("stage", "backend").flag("discovered", false).num("problems", found.error().size()),
          LogLevel::Warn);
      backend_found_ = b;
      return;
    }
    discovery_ = new en::gal::Discovery(std::move(*found));  // lives for the process
    if (chars_ != nullptr) {
      // The save data's static, from the build's table: the character search reads its object's addresses too.
      if (const auto save = discovery_->address(en::gal::roles::Item::SaveData)) {
        chars_->set_save_slot(static_cast<std::uint32_t>(*save - s_.sys->module_base()));
      }
    }
    const en::gal::DiscoveryReport& r = discovery_->report();
    b.discovered = true;
    b.flags_group = discovery_->has(en::proto::Group::Flags);
    b.ready = r.ready;
    for (const en::gal::ItemResult& item : r.items) b.items += item.status == en::gal::ItemStatus::Resolved ? 1 : 0;
    b.table_items = r.table_items;
    b.table_refused = r.table_refused;
    // The main player's word in the characters manager, where the build's layout has it: the character search and
    // the parts watch take the main player from there.
    if (const en::gal::FieldLayout* main = discovery_->bound().field(en::gal::roles::Fld::MainPlayer)) {
      main_player_word_ = main->off;
      if (chars_ != nullptr) chars_->set_main_player_word(main->off);
    }
    // And a list's place there (`characters.by_distance`): the character search judges the run at it first.
    // And the holder of a character's parts (`character.parts`): with both, the parts watch starts at the second
    // world's mark, without waiting for the character search's look.
    if (const en::gal::FieldLayout* parts = discovery_->bound().field(en::gal::roles::Fld::ChrParts)) {
      parts_holder_at_ = parts->off;
    }
    if (const en::gal::FieldLayout* list = discovery_->bound().field(en::gal::roles::Fld::ByDistance)) {
      list_word_ = list->off;
      if (chars_ != nullptr) chars_->set_list_word(list->off);
    }
    if (const en::gal::FieldLayout* holder = discovery_->bound().field(en::gal::roles::Fld::HolderBase)) {
      holder_offset_ = holder->off;
    }
    if (const en::gal::FieldLayout* value = discovery_->bound().field(en::gal::roles::Fld::NodeValue)) {
      index_field_ = value->off;
    }
    if (b.flags_group && body_ != nullptr) {
      en::gal::BackendOptions o;
      o.budget = en::gal::ReadBudget{8192, 4u << 20};
      backend_ = new en::gal::ErBackend(body_->careful_sys(), *discovery_, o);  // lives for the process
      body_->set_backend(backend_);
    }
    backend_found_ = b;
    log(Line("read").word("stage", "backend").flag("discovered", true).flag("flags_group", b.flags_group)
            .flag("ready", b.ready).num("items", b.items).num("table", b.table_items)
            .num("table_refused", b.table_refused).num("ms", since_ms() - t0));
  }

  // The flags the backend is asked for, and how its answers compare: the flag that is set in every playthrough,
  // then each flag a marked flip of the holder's stretch was joined to (its number from the record's key and the
  // bit's place), with what the sweep saw the bit become.
  void compare_backend() {
    if (!backend_found_) return;
    lodestar::BackendFound b = *backend_found_;
    if (backend_ == nullptr || body_ == nullptr) {
      found_.backend = b;
      return;
    }
    std::vector<std::uint32_t> ids = {en::gal::kFlagKnownSet};
    std::vector<bool> expected = {true};
    if (found_.tree && found_.tree->key_field != 0 && found_.sweep && holder_offset_) {
      // What each bit of the holder's stretch became, by its last flip around a mark.
      std::map<std::pair<std::uint32_t, std::uint32_t>, bool> became;
      for (const lodestar::SweepTarget& t : found_.sweep->targets) {
        if (t.deeper || t.offset != *holder_offset_) continue;
        for (const lodestar::MarkFlip& m : t.mark_flips) became[{m.flip.offset, m.flip.bit}] = m.flip.set;
      }
      for (const lodestar::TreeJoin& j : found_.tree->joins) {
        if (j.target != *holder_offset_ || j.match != lodestar::TreeMatch::Index || !j.reached) continue;
        if (ids.size() >= kBackendFlags) break;
        const std::uint64_t group = j.values[found_.tree->key_field / 4];
        const std::uint32_t place = (j.flip % lodestar::TreeRead::kBlock) * 8 + (7 - j.bit);
        const std::uint64_t id = group * en::gal::kFlagDivisor + place;
        const auto was = became.find({j.flip, j.bit});
        if (id > 0xFFFFFFFFull || was == became.end()) continue;
        if (std::find(ids.begin(), ids.end(), static_cast<std::uint32_t>(id)) != ids.end()) continue;
        ids.push_back(static_cast<std::uint32_t>(id));
        expected.push_back(was->second);
      }
    }
    if (ids != flag_ids_) {
      // A new list: handed to T2 by its address, and kept for the process.
      flag_ids_ = ids;
      auto* list = new Body::FlagList{++flag_version_, std::make_shared<const std::vector<std::uint32_t>>(ids)};
      body_->set_flag_list(list);
      log(Line("read").word("stage", "backend_flags").num("flags", ids.size()).num("version", flag_version_));
    }
    flag_expected_ = expected;
    const Body::FlagsRead read = body_->flags_read();
    b.calls = read.calls;
    b.reads = read.reads;
    b.known_read = read.known >= 0;
    b.known_set = read.known == 1;
    b.tripped = !backend_->book().live(en::proto::Group::Flags);
    b.reader_faults = backend_->reader().faults(en::gal::FaultKind::Copy);
    if (read.version == flag_version_ && read.version != 0) {
      // The answers to the list as it stands now: kept until a later list is answered.
      backend_flags_.clear();
      for (std::size_t i = 0; i < flag_ids_.size() && i < kBackendFlags; ++i) {
        lodestar::BackendFlag f;
        f.flag = flag_ids_[i];
        f.known = i == 0;
        f.expected = flag_expected_[i];
        f.valid = (read.states[i] & 1u) != 0;
        f.set = (read.states[i] & 2u) != 0;
        backend_flags_.push_back(f);
      }
    }
    b.flags = backend_flags_;
    found_.backend = b;
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
      std::uint64_t most_refs = 0;
      if (!result.slots.empty()) {
        // The code's references to each slot found: where they hold one object, the one the code uses is the
        // manager's, and its references are its witnesses. The image's code is read once for all of them.
        std::vector<std::uint32_t> sorted = result.slots;
        std::sort(sorted.begin(), sorted.end());
        if (sorted.size() > 16) sorted.resize(16);
        const std::vector<lodestar::References> refs =
            lodestar::references(*image_, s_.sys->image(), ".text", sorted, lodestar::kMaxSiteWitnesses);
        std::vector<std::uint32_t> counts;
        std::vector<std::vector<std::uint32_t>> places;
        for (const lodestar::References& r : refs) {
          counts.push_back(static_cast<std::uint32_t>(std::min<std::uint64_t>(r.count, 0xFFFFFFFFu)));
          places.push_back(r.places);
          most_refs = std::max(most_refs, r.count);
        }
        lodestar::set_search_refs(held_, slot_id_, counts, places);
      }
      log(Line("read").word("stage", "search").num("id", slot_id_).num("candidates", result.candidates)
              .num("checked", result.checked).num("found", slot_search_->found_total()).num("objects", result.objects)
              .num("unreadable", result.unreadable).num("most_refs", most_refs).num("named", held_.named));
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
      // And no lead is searched for more than three times: what three searches did not find, a fourth will not.
      if (m.status == en::gal::derive::Status::Derived || m.search_found != 0 || m.search_runs >= 3) continue;
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

  // The watch's ways, as the round finds what to watch: each named manager's object; each place the member search
  // found an unnamed lead's object at (the world's clock among them, watched for a clock); and the object behind the
  // slot whose field rose by the clock in nearly every pass (the play time's).
  void add_ways() {
    using Way = lodestar::Watch::Way;
    const std::uint32_t clock_id = en::gal::roles::info(en::gal::roles::Item::WorldClock).id;
    const std::uint32_t flags_id = en::gal::roles::info(en::gal::roles::Item::EventFlags).id;
    for (const lodestar::ManagerFound& m : held_.managers) {
      if (m.status != en::gal::derive::Status::Derived) continue;
      Way way{m.label, m.id, m.rva, 0, false, m.vtable, true};
      way.every_us = 2'000'000;  // every two seconds: the reads are wanted elsewhere (the sixth round)
      if (watch_->add(way)) said_ways_ = false;
    }
    if (members_ != nullptr) {
      std::size_t taken = 0;
      for (const lodestar::MemberHit& h : members_->found().hits) {
        // A pointer to the lead's object is a way; an object "inside" another has proved a neighbour in memory.
        if (h.embedded || ++taken > 4) continue;
        // The clock's: 16 KiB of the object and of the manager that points at it, since the first 512 bytes of
        // neither change in play (the fifth round).
        Way to{h.label, h.id, h.rva, h.offset, true, h.vtable, true};
        Way holder{"holder", 0, h.rva, 0, false, 0, true};
        to.every_us = 2'000'000;
        holder.every_us = 2'000'000;
        if (h.id == clock_id) {
          to.span = lodestar::Watch::kMaxSpan;
          holder.span = lodestar::Watch::kMaxSpan;
        }
        if (watch_->add(to)) said_ways_ = false;
        if (watch_->add(holder)) said_ways_ = false;
      }
    }
    // One step on from the flags' store: the objects its first words point at (the watch says which words are
    // addresses of live memory), each watched as the store is, and each the start of a stretch the sweep goes
    // through for bits that flip once. Then one step more: what those objects' first words point at.
    const auto sweep_target = [&](const lodestar::Sweep::Target& target) {
      if (sweep_ == nullptr) sweep_ = new lodestar::Sweep(s_.sys->module_base());  // lives for the process
      if (sweep_->add(target)) said_ways_ = false;
      body_->set_sweep(sweep_);
    };
    if (!flag_hops_ && body_ != nullptr) {
      for (const lodestar::WatchObject& o : watch_->found().objects) {
        if (o.id != flags_id || o.offset != 0 || o.deref || o.readings < 3) continue;
        flag_hops_ = true;
        std::size_t hops = 0;
        for (const lodestar::WatchWord& word : o.words) {
          if (word.kind != lodestar::WordKind::Heap || word.offset == 0) continue;
          if (++hops > kFlagHops) break;
          Way hop{o.label, o.id, o.rva, word.offset, true, 0, false};
          hop.every_us = 3'000'000;
          if (watch_->add(hop)) said_ways_ = false;
          sweep_target(lodestar::Sweep::Target{o.label, o.id, o.rva, word.offset, false, 0,
                                               lodestar::Sweep::kMaxPieces});
        }
      }
    } else if (flag_hops_ && !flag_hops2_) {
      std::vector<lodestar::WatchObject> first;
      bool ready = true;
      for (lodestar::WatchObject& o : watch_->found().objects) {
        if (o.id != flags_id || !o.deref || o.deeper) continue;
        if (o.readings + o.empty + o.unreadable < 3) ready = false;
        first.push_back(std::move(o));
      }
      if (ready) {
        flag_hops2_ = true;
        // The tree beside the flags (our notes' lead; the seventh round saw its records): the first of those
        // objects whose first three words are all addresses of live memory is taken for its head, and walked once.
        for (const lodestar::WatchObject& o : first) {
          if (tree_ != nullptr || o.readings < 3) continue;
          std::size_t links = 0;
          for (const lodestar::WatchWord& word : o.words) {
            if (word.offset <= 16 && word.kind == lodestar::WordKind::Heap) ++links;
          }
          if (links != 3) continue;
          // Lives for the process.
          tree_ = new lodestar::TreeRead(s_.sys->module_base(), image_->size_of_image, o.rva, o.offset);
          body_->set_tree(tree_);
          log(Line("read").word("stage", "tree").num("offset", o.offset));
        }
        std::size_t deeper = 0;
        for (const lodestar::WatchObject& o : first) {
          std::size_t hops = 0;
          for (const lodestar::WatchWord& word : o.words) {
            if (word.kind != lodestar::WordKind::Heap) continue;
            if (++hops > kFlagHopsDeeper || ++deeper > kFlagHopsDeeperAll) break;
            Way hop{o.label, o.id, o.rva, o.offset, true, 0, false};
            hop.every_us = 10'000'000;
            hop.deeper = true;
            hop.via = word.offset;
            if (watch_->add(hop)) said_ways_ = false;
            sweep_target(lodestar::Sweep::Target{o.label, o.id, o.rva, o.offset, true, word.offset, 16});
          }
        }
      }
    }
    if (rising_ != nullptr && !play_time_way_) {
      const auto hits = rising_->hits();
      // The play time's field does what a play time does: it rose in ten passes at least and did anything else in
      // no more than a tenth as many; and it is the only field that did. (The rule used to ask for three times the
      // next field's rises: in the seventh session another field rose nearly as often, and the right one was
      // refused.)
      const auto steady = [](const en::gal::probe::RisingSearch::Hit& h) {
        return h.rose >= 10 && std::uint64_t{h.other} * 10 <= h.rose;
      };
      if (!hits.empty() && steady(hits[0]) && (hits.size() < 2 || !steady(hits[1]))) {
        // Where that object is a named manager's it is watched already, under the manager's label.
        play_time_way_ = true;
        said_ways_ = false;
        (void)watch_->add(Way{"play_time", 0, hits[0].slot, 0, false, 0, false});
        // The slot is the save data's static, by what its object holds (proposal 0005 6.1's invariant): unless a
        // lead's manager sits there already. The code's references to it are its sites and witnesses.
        const en::gal::roles::ItemInfo save = en::gal::roles::info(en::gal::roles::Item::SaveData);
        if (!named_at(hits[0].slot)) {
          lodestar::InvariantFound& v = invariant(save.id, save.label, hits[0].slot, hits[0].offset);
          log(Line("read").word("stage", "invariant").num("id", save.id).num("sites", v.sites));
        }
      }
    }
    // The invariant's counts follow the rising search for as long as its field stays the first.
    const std::uint32_t save_id = en::gal::roles::info(en::gal::roles::Item::SaveData).id;
    for (lodestar::InvariantFound& v : found_.invariants) {
      if (v.id != save_id || rising_ == nullptr) continue;
      const auto hits = rising_->hits();
      if (!hits.empty() && hits[0].slot == v.rva && hits[0].offset == v.offset) {
        v.rose = hits[0].rose;
        v.passes = hits[0].rose + hits[0].still + hits[0].other;  // the times it was compared
      }
    }
    // The world's clock, by what its object holds: of the statics' objects (the watch's wide ways), one in which a
    // run of six bits went round as minutes do, twice at least, with hours beside it that stepped as often (the
    // seventh round found it so, and by the hours the owner let pass). Where two statics' objects show the same
    // word, the one that shows it nearest its start is the clock's; whether they are one piece of memory is said.
    {
      const lodestar::WatchObject* best = nullptr;
      const lodestar::WatchObject* other = nullptr;
      const lodestar::WatchFound seen = watch_->found();
      for (const lodestar::WatchObject& o : seen.objects) {
        if (!o.wide || o.bits.empty() || o.bits[0].wraps < 2 || !o.bits[0].has_hour || o.bits[0].ticks < 30) continue;
        if (best == nullptr || o.bits[0].offset < best->bits[0].offset) {
          other = best;
          best = &o;
        } else if (other == nullptr) {
          other = &o;
        }
      }
      const std::uint32_t clock = en::gal::roles::info(en::gal::roles::Item::WorldClock).id;
      // Not where a lead's class has named the clock's manager already, nor at a static another manager holds.
      const bool named =
          std::any_of(held_.managers.begin(), held_.managers.end(), [&](const lodestar::ManagerFound& m) {
            return m.id == clock && m.status == en::gal::derive::Status::Derived;
          });
      if (best != nullptr && !named && !named_at(best->rva)) {
        const bool fresh = std::none_of(found_.invariants.begin(), found_.invariants.end(),
                                        [&](const lodestar::InvariantFound& v) { return v.id == clock; });
        lodestar::InvariantFound& v =
            invariant(clock, en::gal::roles::info(en::gal::roles::Item::WorldClock).label, best->rva,
                      best->bits[0].offset);
        v.rose = best->bits[0].ticks;
        v.passes = best->bits[0].ticks + best->bits[0].jumps;
        if (fresh) {
          const bool same = other != nullptr && best->address != 0 &&
                            best->address + best->bits[0].offset == other->address + other->bits[0].offset;
          log(Line("read").word("stage", "invariant").num("id", clock).num("sites", v.sites)
                  .num("offset", v.offset).num("bit", best->bits[0].bit).num("hour_bit", best->bits[0].hour_bit)
                  .flag("second_static", other != nullptr).flag("same_memory", same));
        }
      }
    }
    // The rising search has done its work once its first field has risen in a dozen passes: the watch follows that
    // field from then on, and the search's reads go to the others.
    if (rising_ != nullptr && !rising_stopped_ && play_time_way_ && body_ != nullptr) {
      const auto hits = rising_->hits();
      if (!hits.empty() && hits[0].rose >= 12) {
        rising_stopped_ = true;
        body_->set_rising(nullptr);  // the search itself stays, for the report
        log(Line("read").word("stage", "rising_done").num("rose", hits[0].rose).num("passes", rising_->passes()));
      }
    }
    if (!said_ways_) {
      said_ways_ = true;
      log(Line("read").word("stage", "watch").num("ways", watch_->ways()).flag("play_time", play_time_way_)
              .flag("flag_hops", flag_hops_).flag("flag_hops_deeper", flag_hops2_)
              .num("sweep_targets", sweep_ != nullptr ? sweep_->targets() : 0));
    }
  }

  // The static found by what its object holds, under a catalogue id: made with the code's references to it as its
  // sites and witnesses, or the one already there (its static and offset brought up to date).
  lodestar::InvariantFound& invariant(std::uint32_t id, std::string_view label, std::uint32_t rva,
                                      std::uint32_t offset) {
    for (lodestar::InvariantFound& v : found_.invariants) {
      if (v.id != id) continue;
      if (v.rva == rva) {
        v.offset = offset;
        return v;
      }
      v = lodestar::InvariantFound{};  // another static now: its references are found afresh
      v.id = id;
      return fill_invariant(v, label, rva, offset);
    }
    found_.invariants.emplace_back();
    found_.invariants.back().id = id;
    return fill_invariant(found_.invariants.back(), label, rva, offset);
  }
  lodestar::InvariantFound& fill_invariant(lodestar::InvariantFound& v, std::string_view label, std::uint32_t rva,
                                           std::uint32_t offset) {
    const std::vector<std::uint32_t> slot = {rva};
    const std::vector<lodestar::References> refs =
        lodestar::references(*image_, s_.sys->image(), ".text", slot, lodestar::kMaxSiteWitnesses);
    v.label = std::string(label);
    v.rva = rva;
    v.offset = offset;
    v.sites = static_cast<std::uint32_t>(std::min<std::uint64_t>(refs[0].count, 0xFFFFFFFFu));
    v.witnesses = refs[0].places;
    return v;
  }

  // What tells one watched object from another: its way.
  using WayKey = std::array<std::uint32_t, 5>;
  static WayKey way_key(const lodestar::WatchObject& o) {
    return WayKey{o.rva, o.offset, (o.deref ? 1u : 0u) | (o.deeper ? 2u : 0u) | (o.wide ? 4u : 0u), o.via, o.id};
  }

  // The report, and the table when the names changed (or `table` asks).
  void write(bool table) {
    found_.instances = held_;
    if (trial_ != nullptr) found_.trial = trial_->found();
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
    if (watch_ != nullptr) {
      add_ways();
      found_.watch = watch_->found();
      // An object whose words were once left out stays without them: the report only grows.
      for (lodestar::WatchObject& o : found_.watch->objects) {
        if (std::find(left_out_.begin(), left_out_.end(), way_key(o)) == left_out_.end()) continue;
        o.words.clear();
        o.words_left_out = true;
      }
    }
    if (sweep_ != nullptr) found_.sweep = sweep_->found();
    if (tree_ != nullptr) {
      if (tree_->done() && !tree_->settled()) {
        // Which of the records' words are addresses: asked of Windows, here on T1, once; nothing is read.
        en::gal::NativeSys* sys = s_.sys;
        tree_->settle([sys](std::uint64_t at) {
          return liveness(sys->region_at(at)) == en::gal::probe::Liveness::Live;
        });
        const lodestar::TreeFound walked = tree_->found({});
        log(Line("read").word("stage", "tree_done").num("records", walked.records).num("off_shape", walked.off_shape)
                .num("unreadable", walked.unreadable).flag("capped", walked.capped).num("order", walked.order));
      }
      // The flips to join with its records: those seen around the marks in the first-step stretches still read.
      std::vector<lodestar::TreeRead::Flip> flips;
      if (found_.sweep) {
        std::vector<const lodestar::SweepTarget*> stretches;
        for (const lodestar::SweepTarget& t : found_.sweep->targets) stretches.push_back(&t);
        std::stable_sort(stretches.begin(), stretches.end(),
                         [](const auto* a, const auto* b) { return a->offset < b->offset; });
        for (const lodestar::SweepTarget* stretch : stretches) {
          const lodestar::SweepTarget& t = *stretch;
          if (t.deeper || t.retired) continue;
          // Only the holder's stretch holds flags (the layout says which it is): the others' joins are chance.
          if (holder_offset_ && t.offset != *holder_offset_) continue;
          for (const lodestar::MarkFlip& m : t.mark_flips) {
            flips.push_back(lodestar::TreeRead::Flip{m.mark, t.offset, m.flip.offset, m.flip.bit, t.address});
          }
        }
      }
      found_.tree = tree_->found(flips, index_field_);
      // Where the store keeps the flags' divisor (1,000) and a block's size (125), if it keeps them: its first 256
      // bytes read once here on T1, asked first; only the offsets of those two numbers are kept. EldenNet's own
      // reader checks the divisor before it trusts the tree.
      if (!store_looked_ && tree_->done()) {
        store_looked_ = true;
        std::array<std::byte, 8> slot{};
        if (ask_first_.copy(s_.sys->module_base() + found_.tree->rva, slot).copied) {
          std::uint64_t store = 0;
          std::memcpy(&store, slot.data(), 8);
          std::array<std::byte, 256> bytes{};
          if (store != 0 && ask_first_.copy(store, bytes).copied) {
            for (std::uint32_t at = 0; at + 4 <= bytes.size(); at += 4) {
              std::uint32_t v = 0;
              std::memcpy(&v, bytes.data() + at, 4);
              if (v == 1000) divisor_at_.push_back(at);
              if (v == lodestar::TreeRead::kBlock) block_at_.push_back(at);
            }
          }
        }
        log(Line("read").word("stage", "store").num("divisor_places", divisor_at_.size())
                .num("block_places", block_at_.size()));
      }
      found_.tree->divisor_at = divisor_at_;
      found_.tree->block_at = block_at_;
    }
    compare_backend();
    if (state_ != nullptr) {
      // A first pass made while a save was loaded is no pass at the title.
      if (state_->passes() == 0 && trial_ != nullptr && trial_->found().world_readings != 0) state_dirty_ = true;
      lodestar::StateFound f = state_->found();
      f.title_clean = !state_dirty_;
      // Each look as it ends: its time, and whether the tester had moved the game on before it was through.
      for (; state_looks_said_ < f.passes && state_looks_said_ < f.looks.size(); ++state_looks_said_) {
        const lodestar::StateLook& l = f.looks[state_looks_said_];
        log(Line("read").word("stage", "state_look").num("n", state_looks_said_ + 1).num("took_ms", l.took_ms)
                .flag("overrun", l.overrun));
      }
      if (f.passes >= lodestar::StateSearch::kPasses && !state_said_) {
        // The hits' slots, with the code's references to each; one slot alone is the game state's static, by what
        // its object holds (the invariant recipe), unless a lead's class has named it or a manager sits there.
        state_said_ = true;
        std::vector<std::uint32_t> slots;
        for (const lodestar::StateHit& h : f.hits) {
          if (std::find(slots.begin(), slots.end(), h.rva) == slots.end()) slots.push_back(h.rva);
        }
        if (!slots.empty()) {
          const std::vector<lodestar::References> refs =
              lodestar::references(*image_, s_.sys->image(), ".text", slots, lodestar::kMaxSiteWitnesses);
          for (std::size_t i = 0; i < slots.size() && i < refs.size(); ++i) {
            state_sites_[slots[i]] = static_cast<std::uint32_t>(std::min<std::uint64_t>(refs[i].count, 0xFFFFFFFFu));
          }
        }
        const en::gal::roles::ItemInfo game = en::gal::roles::info(en::gal::roles::Item::GameState);
        const bool named =
            std::any_of(held_.managers.begin(), held_.managers.end(), [&](const lodestar::ManagerFound& m) {
              return m.id == game.id && m.status == en::gal::derive::Status::Derived;
            });
        // Which hit names the slot. Where two saves were loaded (the tester's first character, then the usual one),
        // the field that read two numbers is the save's slot, if one field in one slot alone did. Where the same
        // save was loaded twice, only a search that hit one slot in all can tell.
        const lodestar::StateHit* pick = nullptr;
        if (f.slots_differ == 1 && f.fields_differ == 1) {
          for (const lodestar::StateHit& h : f.hits) {
            if (h.value != h.again && pick == nullptr) pick = &h;
          }
        } else if (f.slots_differ == 0 && f.slots_hit == 1 && !f.hits.empty()) {
          pick = &f.hits[0];
        }
        // The state search names nothing any more: its one candidate, with two characters loaded, read numbers
        // that are not their slots (the owner's save file). The pair search, which knows the slots, names the slot.
        const bool names = false;
        (void)game;
        log(Line("read").word("stage", "state_done").num("slots", f.slots_hit).num("differ", f.slots_differ)
                .num("fields_differ", f.fields_differ).num("hits", f.hits.size())
                .flag("title_clean", f.title_clean).flag("overrun", f.overrun).flag("named", named)
                .flag("names", names).num("offset", pick != nullptr ? pick->offset : 0)
                .num("value", pick != nullptr ? pick->value : 0).num("again", pick != nullptr ? pick->again : 0)
                .num("sites", pick != nullptr ? state_sites_[pick->rva] : 0));
      }
      for (lodestar::StateHit& h : f.hits) {
        const auto sites = state_sites_.find(h.rva);
        if (sites != state_sites_.end()) h.sites = sites->second;
      }
      found_.state = std::move(f);
    }
    if (chars_ == nullptr && plan_ == w::kPlanRounds) make_chars();
    if (chars_ != nullptr) {
      lodestar::CharFound f = chars_->found();
      for (; chars_looks_said_ < f.looks.size(); ++chars_looks_said_) {
        const lodestar::CharLook& l = f.looks[chars_looks_said_];
        log(Line("read").word("stage", "chars_look").num("n", chars_looks_said_ + 1).num("mark", l.mark)
                .num("took_ms", l.took_ms).num("reads", l.reads).flag("overrun", l.overrun).flag("cut", l.cut)
                .num("manager_bytes", l.manager_bytes).num("characters", l.characters).num("players", l.players)
                .num("lists", l.lists.size()).num("sets", l.sets.size()).num("members", l.members_total));
      }
      make_parts(f);
      found_.chars = std::move(f);
    }
    if (pair_ != nullptr) {
      lodestar::PairFound f = pair_->found();
      if (f.passes >= lodestar::PairSearch::kPasses && !pair_said_) {
        // The hits' slots, with the code's references to each. One 32-bit field alone that read the first
        // character's slot and then the second's is the save's slot, and its slot the game's state's static, by
        // what its object holds (the invariant recipe), unless something is named there already.
        pair_said_ = true;
        std::vector<std::uint32_t> slots;
        for (const lodestar::PairHit& h : f.hits) {
          if (std::find(slots.begin(), slots.end(), h.rva) == slots.end()) slots.push_back(h.rva);
        }
        if (!slots.empty()) {
          const std::vector<lodestar::References> refs =
              lodestar::references(*image_, s_.sys->image(), ".text", slots, lodestar::kMaxSiteWitnesses);
          for (std::size_t i = 0; i < slots.size() && i < refs.size(); ++i) {
            pair_sites_[slots[i]] = static_cast<std::uint32_t>(std::min<std::uint64_t>(refs[i].count, 0xFFFFFFFFu));
          }
        }
        const en::gal::roles::ItemInfo game = en::gal::roles::info(en::gal::roles::Item::GameState);
        // Which of the hits' objects is of the class the game state's lead names (the leads' [statics]): each
        // object's first word, one guarded read, against the image's own type information. The fifteenth round
        // had 17 such fields behind 13 slots; the class tells the game's state among them.
        pair_leads_ = namer_ != nullptr ? namer_->static_lead_tables(game.label) : 0;
        std::size_t lead_hits = 0, lead_at = 0;
        for (std::size_t i = 0; i < f.hits.size() && pair_leads_ != 0; ++i) {
          const lodestar::PairHit& h = f.hits[i];
          if (h.width != 4) continue;
          std::array<std::byte, 8> slot{};
          std::uint64_t object = 0;
          if (s_.sys->image().read(h.rva, slot)) std::memcpy(&object, slot.data(), 8);
          const std::optional<std::uint64_t> first = object != 0 ? read_word(object) : std::nullopt;
          const std::uint64_t base = s_.sys->module_base();
          if (!first || *first < base || *first - base > 0xFFFFFFFFull) continue;
          if (namer_->static_lead_class(game.label, static_cast<std::uint32_t>(*first - base)) == 0) continue;
          pair_lead_.insert({h.rva, h.offset});
          if (lead_hits++ == 0) lead_at = i;
        }
        // One 32-bit field alone names the slot; so does the one field in an object of the lead's class.
        const std::size_t pick = lead_hits == 1 ? lead_at : 0;
        const bool names = (f.word_hits == 1 || lead_hits == 1) && !f.overrun && !f.hits.empty() &&
                           f.hits[pick].width == 4 && !named_at(f.hits[pick].rva);
        if (names) {
          lodestar::InvariantFound& v = invariant(game.id, game.label, f.hits[pick].rva, f.hits[pick].offset);
          v.rose = 2;  // the two worlds, each read as its character's slot
          v.passes = 2;
        }
        log(Line("read").word("stage", "pair_done").num("word_hits", f.word_hits).num("byte_hits", f.byte_hits)
                .flag("overrun", f.overrun).flag("names", names).num("leads", pair_leads_)
                .num("lead_hits", lead_hits)
                .num("offset", f.hits.empty() ? 0 : f.hits[pick].offset)
                .num("sites", f.hits.empty() ? 0 : pair_sites_[f.hits[pick].rva]));
      }
      f.leads = pair_leads_;
      for (lodestar::PairHit& h : f.hits) {
        const auto sites = pair_sites_.find(h.rva);
        if (sites != pair_sites_.end()) h.sites = sites->second;
        h.lead = pair_lead_.count({h.rva, h.offset}) != 0;
      }
      found_.pair = std::move(f);
    }
    if (parts_ != nullptr) {
      lodestar::PartsFound f = parts_->found();
      if (f.phase != parts_phase_said_) {
        parts_phase_said_ = f.phase;
        log(Line("read").word("stage", "parts_phase").num("phase", f.phase).num("passes", f.passes)
                .num("absent", f.absent).num("parts", f.parts.size()).num("moves", f.moves_total)
                .num("falls", f.falls_total));
      }
      found_.parts = std::move(f);
    }
    if (plan_ == w::kPlanSurvey && survey_ == nullptr && marks_seen_ >= kSurveyMark) make_survey();
    if (survey_ != nullptr) {
      lodestar::SurveyFound f = survey_->found();
      if (f.phase != survey_phase_said_ || (f.deaths != survey_deaths_said_)) {
        survey_phase_said_ = f.phase;
        survey_deaths_said_ = f.deaths;
        log(Line("read").word("stage", "survey_phase").num("phase", f.phase).num("passes", f.passes)
                .num("absent", f.absent).num("tracked", f.tracked).num("listed", f.most_listed)
                .num("deaths", f.deaths).num("fields", f.fields_total).num("dead_bits", f.dead_total)
                .num("screen_bits", f.screen_total));
      }
      found_.survey = std::move(f);
    }
    if (backend_ != nullptr && body_ != nullptr) {
      // EldenNet's own reader on the world's leads: the spans the tester's marks have opened so far.
      lodestar::WorldRead f = body_->world_read();
      const std::size_t spans = std::min<std::size_t>(std::size_t{marks_seen_} + 1, lodestar::WorldRead::kSpans);
      f.pos_frames_by.resize(spans);
      f.pos_moved_by.resize(spans);
      f.at_marks.resize(spans);
      f.chr_object_by.resize(spans);
      f.chr_dead_by.resize(spans);
      f.chr_on_screen_by.resize(spans);
      if (f.calls != 0 && !world_said_) {
        world_said_ = true;
        log(Line("read").word("stage", "world").num("lacks", f.lacks).num("fields_bound", f.fields_bound));
      }
      if (f.calls != 0) found_.world = std::move(f);
    }
    // A report the writer refuses (too large, most likely: a long round's watch) is written again smaller, a step
    // at a time: the watched object with the most words loses them (and says so), then the next, the flags' objects
    // last (theirs are what this round is after); with no words left, the sweep's flips go, the longest list
    // first; then the watch, then the sweep. The sixth round dropped every object's words at once.
    const std::uint32_t flags_id = en::gal::roles::info(en::gal::roles::Item::EventFlags).id;
    auto text = lodestar::write_report(found_);
    for (int smaller = 0; !text && smaller < 400; ++smaller) {
      if (refusals_++ < 4) {
        log(Line("read").word("report", "refused").num("try", static_cast<std::uint64_t>(smaller))
                .flag("too_large", text.error().problem.find("larger") != std::string::npos),
            LogLevel::Error);
      }
      if (!found_.watch && !found_.sweep && !found_.tree && !found_.backend && !found_.state) break;
      lodestar::WatchObject* most = nullptr;
      if (found_.watch) {
        const auto ahead = [&](const lodestar::WatchObject& a, const lodestar::WatchObject& b) {
          if ((a.id == flags_id) != (b.id == flags_id)) return b.id == flags_id;
          return a.words.size() > b.words.size();
        };
        for (lodestar::WatchObject& o : found_.watch->objects) {
          if (!o.words.empty() && (most == nullptr || ahead(o, *most))) most = &o;
        }
      }
      lodestar::SweepTarget* longest = nullptr;
      if (found_.sweep) {
        for (lodestar::SweepTarget& t : found_.sweep->targets) {
          if (!t.flips.empty() && (longest == nullptr || t.flips.size() > longest->flips.size())) longest = &t;
        }
      }
      // With no words left: the object with the most runs of bits listed loses them (its rises and their jumps
      // stay).
      lodestar::WatchObject* fullest = nullptr;
      if (found_.watch && most == nullptr) {
        for (lodestar::WatchObject& o : found_.watch->objects) {
          const std::size_t runs = o.bits.size() + o.hours.size();
          if (runs != 0 && (fullest == nullptr || runs > fullest->bits.size() + fullest->hours.size())) fullest = &o;
        }
      }
      if (most != nullptr) {
        log(Line("read").word("report", "smaller").num("rva", most->rva).num("offset", most->offset)
                .num("via", most->via).num("words", most->words.size()));
        left_out_.push_back(way_key(*most));
        most->words.clear();
        most->words_left_out = true;
      } else if (fullest != nullptr) {
        fullest->bits.clear();
        fullest->hours.clear();
        std::erase_if(fullest->jumps, [](const lodestar::JumpEvent& e) { return e.as == lodestar::JumpAs::Hour; });
      } else if (longest != nullptr) {
        longest->flips.clear();
      } else if (found_.watch) {
        found_.watch.reset();
      } else if (found_.sweep) {
        found_.sweep.reset();
      } else if (found_.tree) {
        found_.tree.reset();
      } else if (found_.backend) {
        found_.backend.reset();
      } else {
        found_.state.reset();
      }
      text = lodestar::write_report(found_);
    }
    if (text) (void)write_text(folder_ + L"\\lodestar-found.json", *text);
    const std::uint64_t placed = held_.named + found_.invariants.size();
    if (!table && placed == tabled_) return;
    tabled_ = placed;
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
        if (searching_ && !backend_tried_) prepare_backend();
        if (searching_) slot_searches();
      }
      // The tester's marks (the launcher's panel: a task confirmed as done): the sweep keeps what flipped around
      // each, and starts its next pass at once.
      // A mark that comes before the sweep has a target is kept and handed on when it has, so that the marks keep
      // their numbers.
      for (const std::uint32_t n = block::marks(); marks_seen_ < n;) {
        ++marks_seen_;
        marks_kept_.push_back(s_.sys->now_us());
        if (plan_ == w::kPlanRounds) {
          if (state_ != nullptr) state_->ask();  // the tester says the game is in the next state
          if (marks_seen_ == kFirstWorldMark) make_pair();
          if (marks_seen_ == kSecondWorldMark) {
            if (pair_ != nullptr) pair_->ask();
            if (chars_ == nullptr) make_chars();
            if (chars_ != nullptr) chars_->ask(marks_seen_);
          }
          if (parts_ != nullptr) parts_->mark(marks_kept_.back());  // the tester's next phase
          // The second world's mark opens the parts watch's first phase (the standing task), where the layout can
          // start it; the marks after it move it on.
          if (marks_seen_ == kSecondWorldMark) make_parts_by_layout();
        } else {
          // The survey's plan: its first mark (the tester stands in the world) starts the survey, in its standing
          // phase; each mark after it moves the survey on.
          if (survey_ != nullptr) survey_->mark(marks_kept_.back());
          if (marks_seen_ == kSurveyMark) make_survey();
        }
        if (body_ != nullptr) body_->world_mark();
        log(Line("read").word("stage", "mark").num("n", marks_seen_).flag("sweep", sweep_ != nullptr));
      }
      for (; sweep_ != nullptr && marks_given_ < marks_kept_.size(); ++marks_given_) {
        sweep_->mark(marks_kept_[marks_given_]);
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
        if (watch_ != nullptr && rising_ != nullptr) {
          const lodestar::WatchFound seen = watch_->found();
          log(Line("read").word("stage", "held").num("watch_passes", seen.passes).num("wide_ways", seen.wide_ways)
                  .num("watched", seen.objects.size())
                  .num("rising_passes", rising_->passes()).num("rising_hits", rising_->hits().size())
                  .num("risen", rising_->risen())
                  .num("member_passes", members_ != nullptr ? members_->found().passes : 0)
                  .num("member_hits", members_ != nullptr ? members_->found().hits.size() : 0)
                  .num("search_faults", body_->reader().faults(en::gal::FaultKind::Copy))
                  .num("asked", body_->asks().asked()).num("refused_gone", body_->asks().refused_gone())
                  .num("refused_access", body_->asks().refused_access())
                  .num("copy_failed", body_->asks().copy_failed()).num("search_steps", body_->search_steps())
                  .num("search_cut", body_->search_cut()));
          if (sweep_ != nullptr) {
            const lodestar::SweepFound swept = sweep_->found();
            std::uint64_t bytes = 0, flips = 0, changed = 0;
            for (const lodestar::SweepTarget& t : swept.targets) {
              bytes += t.bytes;
              flips += t.flips.size();
              changed += t.changed_words;
            }
            log(Line("read").word("stage", "sweep").num("passes", swept.passes).num("targets", swept.targets.size())
                    .num("kib", bytes >> 10).num("changed_words", changed).num("flips", flips));
          }
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
  en::gal::probe::RisingSearch* rising_ = nullptr;
  lodestar::MemberSearch* members_ = nullptr;
  lodestar::Watch* watch_ = nullptr;
  bool play_time_way_ = false;
  lodestar::Sweep* sweep_ = nullptr;
  lodestar::TreeRead* tree_ = nullptr;
  lodestar::StateSearch* state_ = nullptr;
  bool state_dirty_ = false;                        // a save was loaded before the first pass was through
  bool state_said_ = false;
  lodestar::CharSearch* chars_ = nullptr;
  std::uint32_t chars_manager_ = 0;                 // the characters manager's static
  std::uint32_t main_player_word_ = 0;              // in the manager's object, by the build's layout; 0: not there
  std::uint32_t list_word_ = 0;                     // a list's bounds there, by the build's layout; 0: not there
  std::uint32_t parts_holder_at_ = 0;               // the holder of parts in a character, by the layout; 0: not there
  lodestar::PairSearch* pair_ = nullptr;
  bool pair_said_ = false;
  std::map<std::uint32_t, std::uint32_t> pair_sites_;  // by slot: the code's references to it
  std::set<std::pair<std::uint32_t, std::uint32_t>> pair_lead_;  // slot and field: its object is of the lead's class
  std::uint32_t pair_leads_ = 0;                    // tables of methods under the game state's lead's name
  lodestar::PartsWatch* parts_ = nullptr;
  bool parts_tried_ = false;
  std::uint32_t parts_phase_said_ = 0;
  bool world_said_ = false;
  std::uint32_t plan_ = 0;                          // the round's plan (watch.hpp's kPlan*), read once at its start
  lodestar::CharSurvey* survey_ = nullptr;          // plan 1
  bool survey_tried_ = false;
  std::uint32_t survey_phase_said_ = 0;
  std::uint32_t survey_deaths_said_ = 0;
  std::size_t chars_looks_said_ = 0;                // looks whose end the log has told
  std::size_t state_looks_said_ = 0;                // looks whose end the log has told
  std::map<std::uint32_t, std::uint32_t> state_sites_;  // by slot: the code's references to it
  const en::gal::Gated* gated_ = nullptr;           // the gate's, which lives for the process
  en::gal::Discovery* discovery_ = nullptr;         // EldenNet's own, through the build's layout
  en::gal::ErBackend* backend_ = nullptr;           // built on it; stepped on T2
  bool backend_tried_ = false;
  std::optional<lodestar::BackendFound> backend_found_;
  std::optional<std::uint32_t> holder_offset_;      // where the store keeps the holder, by the layout
  std::optional<std::uint32_t> index_field_;        // where a record keeps its block's index, by the layout
  std::vector<std::uint32_t> flag_ids_;             // the flags the backend is asked for
  std::vector<bool> flag_expected_;
  std::uint32_t flag_version_ = 0;
  std::vector<lodestar::BackendFlag> backend_flags_;
  bool store_looked_ = false;
  std::vector<std::uint32_t> divisor_at_;  // offsets in the flags' store that hold 1,000
  std::vector<std::uint32_t> block_at_;    // and 125
  std::uint32_t marks_seen_ = 0;
  std::vector<std::uint64_t> marks_kept_;  // each mark's time, on this process's clock
  std::size_t marks_given_ = 0;            // of those, the ones the sweep has
  bool flag_hops_ = false;
  bool flag_hops2_ = false;
  bool rising_stopped_ = false;
  std::vector<WayKey> left_out_;  // the watched objects whose words the report leaves out
  bool said_ways_ = true;
  std::uint32_t refusals_ = 0;
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
