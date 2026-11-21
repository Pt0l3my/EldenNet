// R1, the in-game round of the derivation (O89; plan §2; the owner's offline round R0c), in a probe build of
// eldennet.dll with `probe = { round = "r1" }`. On T1, after the window and `probe.settle_s` seconds:
// 1. The derivation over the live image (en-gal's derive.hpp, ImageLayout::Memory, with gal/leads.toml): the FD4
//    singletons named through their class objects as the game filled them (dlrf_name_layout), register_task and its
//    ABI from the hub's static and the game's own call sites, the group table as loaded, the RTTI sets. Passes at once,
//    then at +15, +30 and +60 s while the registration's instance is not named. Written as probe\r1-derive.json
//    (derive.json's schema 1). global_hinstance's slot: the data slots that hold the module's base.
// 2. The checks before the one call: the gate passed; the FD4 idiom the live image gives the one R0 cut from the file
//    (gal/patterns.toml's fd4_idiom, reviewed); the name layout read by at least half the class objects; the
//    registration's callee a function with three agreeing argument registers; the group's index found in the loaded
//    table and equal to the one the gated build's layout gives (gal/layouts/, a lead); the hub's instance non-null.
//    Any check that fails: no call, and the report says which. Nothing is guessed, and nothing rests on a derivation
//    that was not reviewed (O89 (a)). The callee's summary, from the live image (WHY§227): its calls, its branches
//    before the first call, and what it does with each argument register before it (probe.hpp's summarise_callee);
//    since spike.7 (WHY§235) the arguments in each call, each branch's flag test, and each callee it calls (or
//    tail-jumps to) summarised in turn with the arguments followed in; since spike.8 (WHY§240) the instructions as
//    forms, the values loaded from memory followed, and the tail function's table matched against run 7's reading;
//    since spike.9 (WHY§248) the group's own function (the tail function's direct jump, or the entry's vtable slot) and
//    its callees, with the group's entry and our task followed in: every load from either, its value's uses, and the
//    task's stores; since spike.10 (WHY§289) G's method (the second singleton's, reached through a stub) and its
//    callees, with G's loads and the entry's word's uses, what the game's own call sites pass as the task, and the
//    registration's sibling. At the Mark the hub's reachable memory (spike.5's walk) is walked, again 2 s later and
//    kept on our heap, and the two compared (the control diff), since spike.9 from the table's singleton and the
//    group's entry too, and since spike.10 from G; then, at once, the table is read (with the singleton's and the
//    entry's RTTI classes against the leads), G and the entries' words, and the call made. Since spike.11 (WHY§293)
//    the request G's method builds (its vtable, fields, class and slots), its slots' functions and the function G's
//    method calls with it summarised with the request in rcx, one level further each, every summary in parts where its
//    code goes past its extent; the fields of G the method reads, and G's queue (a fifth root), are read just before
//    the call and 50 and 250 ms after it.
// 3. The call, at the first Mark (the launcher's Pause, which the owner presses at the title with no save loaded;
//    `probe.register = "auto"` calls at once, "never" not at all): our task object of `probe.object_size` zeroed bytes
//    with a fake vtable of guarded slots, each counting its calls and running the probe's body (the run slot is what
//    R1 finds), in the derived registers and group.
// 4. On the game thread, within the reader's budget: the slot counts; how much of our object the hub wrote; the hub
//    census (the vtables of the objects the hub points at); the rising-counter search over the heap statics (the play
//    time's static has no RTTI); the RTTI-classed statics' slot searches.
// 5. On T1 every 2 s, probe\r1-probe.json (probe_report.hpp): the checks, the call, the counts, and, once the census is
//    done, the base classes the hub's objects share with each one's derived classes' slots and run slot. Once statics'
//    slots are found, the derivation runs again with them, so that r1-derive.json carries a pattern at each.
// 6. On T1, 2 and 20 s after the call (WHY§209, WHY§227, WHY§235): the hub's reachable memory walked again, searched
//    for our task object's address and compared with the walk kept from before the call (each changed word marked when
//    the control diff changed it too), from each of its roots (WHY§248); then every readable committed region of the
//    process searched for our task object's address, our fake vtable's and its slots' (ours counted apart), and for our
//    object's and vtable's under a high-bit tag; and, while the task has not run, how much of our object the hub wrote.
//    At 2 s the table is read again (WHY§240), and G, the entries' words and the array's views (WHY§289); since
//    spike.11 G's fields and its queue again, the searches count the request's vtable (each hit read as a request, its
//    task's class), and the classes met are given with their bases.
// Every file is numbers and the leads' labels only (O75); the reviewing agent turns them into gal/ data.
#include "dll.hpp"

#if EN_PROBE

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "en/gal/derive.hpp"
#include "en/gal/fd4.hpp"
#include "en/gal/guarded.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/probe.hpp"
#include "en/gal/probe_report.hpp"
#include "en/gal/rtti.hpp"
#include "en/gal/sys_boot.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

void arm_allocation_watch(bool armed) noexcept;
std::uint64_t allocations_watched() noexcept;
std::optional<std::size_t> own_heap_bases(std::span<std::uint64_t> out) noexcept;  // alloc_count.cpp

namespace {

namespace w = en::proto::watch;
namespace derive = en::gal::derive;
namespace probe = en::gal::probe;
using en::gal::probe::CheckStatus;
using en::shell::Line;
using en::shell::LogLevel;

constexpr std::uint32_t kSpanObject = 0x400;  // how much of our object the probe copies back
constexpr std::size_t kMaxInner = 8;          // the callee's callees summarised (WHY§235)
// The group's function, its callees, G's method and its callees (WHY§248); since spike.11 the request's slots and one
// level further (WHY§293).
constexpr std::size_t kMaxGroup = 24;
constexpr std::size_t kMaxRequestSummaries = 6;  // the request's slots summarised (WHY§293)
constexpr std::size_t kMaxGFields = 16;          // G's fields read across the call (WHY§293)
constexpr std::size_t kMaxSibling = 12;       // the sibling's chain summarised (WHY§289)
constexpr std::uint32_t kMaxSiteSpan = 64u << 10;  // a call site at most this far into its function is read (WHY§289)
// Run 9's walks (WHY§289): one object every root reached, at +8 of CSTask's instance and +0x58 of the group's entry; and
// the array behind CSTask's instance at +0x3B0, then +0x3D0, seen as four views by sibling pointers 32 bytes apart.
constexpr std::uint32_t kGAtHub = 0x08, kGAtEntry = 0x58;
constexpr std::array<std::array<std::uint32_t, 3>, 4> kViewPaths = {
    {{0x3B0, 0x3D0, 200}, {0x3B0, 0x3D0, 232}, {0x3B0, 0x3D0, 264}, {0x3B0, 0x3D0, 296}}};
// After the call, before the 2 s reads: since spike.11 G's fields and its queue (the array's views at 2 s only).
constexpr std::array<std::uint64_t, 2> kEarlyReadsMs = {50, 250};

// The probe's body on the game thread (every slot of our fake vtable runs it for our own object).
class R1Body final : public en::gal::TaskBody {
 public:
  R1Body(Shell& s, std::unique_ptr<probe::Census> census, std::unique_ptr<probe::RisingSearch> rising,
         std::vector<std::unique_ptr<probe::SlotSearch>> statics, std::uint32_t object_size)
      : s_(s),
        reader_(*s.sys),
        census_(std::move(census)),
        rising_(std::move(rising)),
        statics_(std::move(statics)),
        object_(std::min(object_size, kSpanObject), std::byte{0}),
        t1_(GetCurrentThreadId()) {}

  void run(const en::proto::GameThread& gt, const en::gal::FrameCtx& frame) override {
    if (GetCurrentThreadId() == t1_) return;  // a slot the hub calls inside register_task, on T1: counted only
    if (!started_) {
      started_ = true;
      block::game_thread(GetCurrentThreadId());
      s_.sys->note_own_stack();
      reader_.bind_thread();
      first_us_.store(frame.qpc_now_us, std::memory_order_release);
    }
    const std::uint64_t n = invocations_.fetch_add(1, std::memory_order_relaxed) + 1;
    arm_allocation_watch(n > 3);
    reader_.begin_tick();
    if (census_ && !census_->done()) census_->step(gt, reader_, 48);
    for (auto& st : statics_) {
      if (st->checked() < st->candidates()) {
        (void)st->step(gt, reader_, 32);
        break;
      }
    }
    if (rising_) (void)rising_->step(gt, reader_, 48, frame.qpc_now_us);
    if (n % 64 == 1 && !object_.empty()) {
      const std::uint64_t at = s_.sys->task_object();
      if (at != 0 && reader_.copy_into(gt, en::proto::Group::Status, at + 8, object_, 8)) {
        std::uint32_t last = 0, nonzero = 0;
        for (std::size_t i = 0; i < object_.size(); ++i) {
          if (object_[i] != std::byte{0}) {
            last = static_cast<std::uint32_t>(i + 1);
            ++nonzero;
          }
        }
        written_.store(last, std::memory_order_relaxed);
        nonzero_.store(nonzero, std::memory_order_relaxed);
      }
    }
    arm_allocation_watch(false);
  }

  [[nodiscard]] std::uint64_t invocations() const noexcept { return invocations_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t first_us() const noexcept { return first_us_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint32_t written() const noexcept { return written_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint32_t nonzero() const noexcept { return nonzero_.load(std::memory_order_relaxed); }
  [[nodiscard]] const en::gal::GuardedReader& reader() const noexcept { return reader_; }
  [[nodiscard]] const probe::Census* census() const noexcept { return census_.get(); }
  [[nodiscard]] const probe::RisingSearch* rising() const noexcept { return rising_.get(); }
  [[nodiscard]] const std::vector<std::unique_ptr<probe::SlotSearch>>& statics() const noexcept { return statics_; }

 private:
  Shell& s_;
  en::gal::GuardedReader reader_;
  std::unique_ptr<probe::Census> census_;
  std::unique_ptr<probe::RisingSearch> rising_;
  std::vector<std::unique_ptr<probe::SlotSearch>> statics_;
  std::vector<std::byte> object_;
  DWORD t1_;
  bool started_ = false;
  std::atomic<std::uint64_t> invocations_{0};
  std::atomic<std::uint64_t> first_us_{0};
  std::atomic<std::uint32_t> written_{0};
  std::atomic<std::uint32_t> nonzero_{0};
};

class Round {
 public:
  explicit Round(Shell& s) : s_(s), folder_(s.paths.data + L"\\probe") {}

  void run(const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>& gated) {
    f_.gate_passed = gated.has_value();
    (void)make_dirs(folder_);
    check("gate", "", f_.gate_passed ? CheckStatus::Confirmed : CheckStatus::Missing);
    log(Line("r1").word("stage", "settle").num("s", s_.settings.probe.settle_s));
    Sleep(s_.settings.probe.settle_s * 1000);

    // ---- The leads and the live image ----
    const std::optional<std::string> leads_text = read_text(s_.paths.gal + L"\\leads.toml", 64u << 10);
    auto leads = leads_text ? derive::parse_leads(*leads_text, "leads.toml")
                            : en::proto::Expected<derive::Leads, en::gal::SchemaErrors>(
                                  en::proto::Unexpected{en::gal::SchemaErrors{}});
    auto image = en::gal::read_image(s_.sys->image(), en::gal::ImageLayout::Memory);
    if (!leads || !image) return fail(!leads ? "leads" : "image");
    leads_ = std::move(*leads);
    image_ = std::make_unique<en::gal::ImageInfo>(std::move(*image));
    base_ = s_.sys->module_base();

    // ---- global_hinstance's slot: the data slots holding the module's base ----
    const probe::Holding h = probe::slots_holding(*s_.sys, *image_, base_);
    f_.hinstance = h.slots;
    f_.hinstance_count = h.count;
    for (const derive::StaticLead& st : leads_.statics) {
      if (st.module && h.count == 1) slots_[st.label] = h.slots[0];
    }

    // ---- 1. The derivation over the live image, with its retries ----
    enter_state(w::InitState::Discover);
    const std::uint64_t t0 = since_ms();
    for (const std::uint64_t at : {0ull, 15'000ull, 30'000ull, 60'000ull}) {
      while (since_ms() - t0 < at) Sleep(200);
      derive_now();
      if (instance_named()) break;
    }
    f_.stage = "derived";

    // ---- 2. The checks, and the callee's summary from the live image (WHY§227) ----
    const bool ready = run_checks();
    summarise_callee();
    write_report();
    if (!ready) {
      f_.stage = "refused";
      f_.register_status = "refused";
      write_report();
      log(Line("r1").word("stage", "refused"), LogLevel::Error);
      push_banner(s_, en::core::BannerReason::DiscoveryFailed, "r1_checks");
      block::ready_ms(static_cast<std::uint32_t>(since_ms()));
      block::state(w::InitState::Banner);
      return report_loop(nullptr);
    }

    // ---- 3. The call, at the first Mark ----
    if (s_.settings.probe.register_when == en::shell::ProbeRegister::Never) {
      f_.register_status = "not_called";
      write_report();
      return report_loop(nullptr);
    }
    if (s_.settings.probe.register_when == en::shell::ProbeRegister::Mark) {
      f_.stage = "waiting";
      f_.register_status = "waiting_for_mark";
      write_report();
      enter_state(w::InitState::Register);
      log(Line("r1").word("stage", "waiting_for_mark").num("marks", block::marks()));
      const std::uint32_t before = block::marks();
      while (block::marks() <= before) Sleep(100);
    }
    R1Body* body = prepare();
    block::install_filter();
    (void)s_.sys->rescan_stacks();  // as before the normal start's call (WHY§190)
    table_roots();                  // WHY§248: the singleton and the group's entry, two more roots; WHY§289: and G
    control_walk();                 // WHY§240: the hub at the Mark, and 2 s later
    (void)s_.sys->rescan_stacks();
    keep_hub();
    find_views();                   // WHY§289: the array's views in the walk just kept
    read_table(true);
    if (f_.method) {
      f_.method->before = read_method(0);
      read_g_fields(0);  // WHY§293: and the path the method will take
      read_queue(0, roots_[4].address != 0 ? &roots_[4].before : nullptr);
    }
    main_thread_ = main_window_thread();
    enter_state(w::InitState::Register);
    en::gal::TaskShape shape;
    shape.vtable_slots = f_.vtable_slots;
    shape.run_slot = 0;
    shape.every_slot = true;
    shape.object_size = f_.object_size;
    shape.group_index = f_.group_index;
    shape.abi = abi_;
    const auto called = s_.sys->register_task(base_ + f_.callee, instance_, shape, *body);
    called_ms_ = since_ms();
    f_.register_status = called ? "ok" : "faulted";
    f_.stage = called ? "registered" : "failed";
    for (const std::uint64_t at : kEarlyReadsMs) {  // WHY§293: G's fields and its queue, a frame or two after the call
      while (since_ms() - called_ms_ < at) Sleep(1);
      read_g_fields(since_ms() - called_ms_);
      read_queue(since_ms() - called_ms_);
    }
    control_diff();
    if (!called) {
      log(Line("r1").word("register", f_.register_status).hex("callee", f_.callee).num("group", f_.group_index)
              .num("slots", f_.vtable_slots).num("object_size", f_.object_size));
      write_report();
      push_banner(s_, en::core::BannerReason::RegisterFaulted, en::gal::to_string(called.error()));
      block::state(w::InitState::Banner);
      return report_loop(nullptr);
    }
    read_vtable();
    // WHY§240: rax as the call left it, as a kind (a registration's bool answer sets al alone).
    probe::DiffSpec kinds = kinds_spec();
    kinds.group_index = 0;
    f_.returned = probe::classify_value(*called, kinds);
    f_.returned_low = static_cast<std::uint8_t>(*called & 0xFF);
    log(Line("r1").word("register", f_.register_status).hex("callee", f_.callee).num("group", f_.group_index)
            .num("slots", f_.vtable_slots).num("object_size", f_.object_size)
            .word("returned", returned_word(f_.returned->kind)).num("returned_low", *f_.returned_low));
    write_report();
    enter_state(w::InitState::FirstTick);
    report_loop(body);
  }

 private:
  void enter_state(w::InitState st) {
    block::state(st);
    log(Line("init").word("state", w::to_string(st)));
  }

  void fail(const char* what) {
    f_.stage = "failed";
    write_report();
    log(Line("r1").word("stage", "failed").word("what", what), LogLevel::Error);
    push_banner(s_, en::core::BannerReason::DiscoveryFailed, what);
    block::ready_ms(static_cast<std::uint32_t>(since_ms()));
    block::state(w::InitState::Banner);
  }

  void check(std::string_view item, std::string label, CheckStatus st, std::optional<std::uint64_t> value = {},
             std::optional<std::uint64_t> expected = {}) {
    for (probe::Check& c : f_.checks) {
      if (c.item == item && c.label == label) {
        c = probe::Check{item, std::move(label), st, value, expected};
        return;
      }
    }
    f_.checks.push_back(probe::Check{item, std::move(label), st, value, expected});
  }

  bool instance_named() const {
    for (const derive::SingletonResult& sr : result_.fd4.singletons) {
      if (sr.label == leads_.register_instance) return sr.status == derive::Status::Derived;
    }
    return false;
  }

  void derive_now() {
    const std::uint64_t t = since_ms();
    derive::Options options;
    for (const auto& [label, slot] : slots_) options.static_slots.emplace_back(label, slot);
    // O92: one 8-byte read at an instance outside the image (its vtable), on T1 before any call, through the same
    // crash-guarded copy and plausibility rules as the reader's (ARCH §6.5); a vtable pointer does not change while
    // its object lives. Refused or faulted: nothing.
    options.read_instance_word = [this](std::uint64_t address) -> std::optional<std::uint64_t> {
      if (en::gal::implausible(address, 8, 8, s_.sys->stacks())) return std::nullopt;
      std::array<std::byte, 8> v{};
      if (!s_.sys->copier().copy(address, v).copied) return std::nullopt;
      std::uint64_t out = 0;
      std::memcpy(&out, v.data(), 8);
      return out;
    };
    result_ = derive::derive(*image_, s_.sys->image(), leads_, options);
    ++f_.derive_passes;
    f_.derive_ms = since_ms() - t;
    {  // the name search's numbers, for the reviewer (WHY§206)
      const auto& g = result_.fd4.diagnostics;
      f_.names.of = result_.fd4.names_of;
      f_.names.agree = g.names_best;
      f_.names.qualified = g.names_qualified;
      f_.names.offset = g.best_names && g.names_best != 0 ? std::optional<std::uint32_t>(g.best_names->offset)
                                                          : std::nullopt;
      f_.names.inst_statics = g.inst_statics;
      f_.names.inst_null = g.inst_null;
      f_.names.inst_unread = g.inst_unread;
      f_.names.inst_unclassed = g.inst_unclassed;
      f_.names.inst_classed = g.inst_classed;
      f_.names.inst_matched = g.inst_matched;
      f_.names.slots.clear();
      for (const derive::NameSlot& n : g.name_slots) {
        f_.names.slots.push_back(probe::NameSlot{n.offset, n.null_ptr, n.to_rdata, n.to_data, n.to_code, n.to_other,
                                               n.outside, n.plain, n.qualified, n.other_text, n.inline_text});
      }
    }
    const auto json = derive::write_report(result_);
    f_.derive_written = json.has_value() && write_text(folder_ + L"\\r1-derive.json", *json);
    log(Line("r1").word("stage", "derive").num("pass", f_.derive_passes).num("ms", f_.derive_ms)
            .word("fd4", derive::to_string(result_.fd4.status)).word("register", derive::to_string(result_.registration.status))
            .word("group", derive::to_string(result_.group.status)).flag("written", f_.derive_written));
  }

  // The layout gal/builds.toml names for the gated build, read as a dev build reads it (leads allowed; only its [task]
  // table matters here): its group's index, the reviewed lead R1 must find again in the loaded table. Nothing when the
  // file cannot be read or is refused, which fails the group check: R1 calls only on reviewed leads.
  std::optional<std::uint64_t> draft_group(const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>* gated) {
    if (gated == nullptr || !*gated) return std::nullopt;
    const std::string& name = (*gated)->profile.entry().layout;
    std::wstring rel(name.begin(), name.end());  // builds.toml's layout path is ASCII, checked
    std::replace(rel.begin(), rel.end(), L'/', L'\\');
    const std::optional<std::string> text = read_text(s_.paths.gal + L"\\" + rel, 256u << 10);
    if (!text) return std::nullopt;
    en::gal::LayoutNeeds needs;
    needs.task = true;
    const auto layout = en::gal::load_layout((*gated)->profile, *text, needs, en::gal::LayoutMode::Dev, "layout");
    if (!layout || !layout->task()) {
      log(Line("r1").word("draft_layout", "refused").num("problems", layout ? 0 : layout.error().size()),
          LogLevel::Warn);
      return std::nullopt;
    }
    return layout->task()->group_index;
  }

  // gal/patterns.toml's FD4 idiom, the lead R0 derived from the executable file (its id fd4_idiom).
  std::optional<en::gal::PatternSpec> lead_idiom() {
    const std::optional<std::string> text = read_text(s_.paths.gal + L"\\patterns.toml", 256u << 10);
    if (!text) return std::nullopt;
    const auto set = en::gal::parse_patterns(*text, {}, "patterns.toml");
    if (!set) {
      log(Line("r1").word("patterns", "refused").num("problems", set.error().size()), LogLevel::Warn);
      return std::nullopt;
    }
    const en::gal::PatternSpec* idiom = set->find("fd4_idiom");
    if (idiom == nullptr || idiom->kind != en::gal::PatternKind::Idiom) return std::nullopt;
    return *idiom;
  }

 public:
  const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>* gated_ = nullptr;

 private:
  bool run_checks() {
    bool ok = f_.gate_passed;
    const derive::Fd4Result& fd = result_.fd4;
    // The FD4 idiom: the one R0 cut from the file (gal/patterns.toml's, reviewed), byte for byte and capture for
    // capture, or the derivation over the live image is not the one that was reviewed (O89 (a)), and nothing is called.
    // The live hits are reported beside the lead's count (where it gives one) for the reviewer; they do not decide.
    {
      const std::optional<en::gal::PatternSpec> lead = lead_idiom();
      const std::optional<en::gal::fd4::Idiom> want = lead ? en::gal::fd4::idiom_from(*lead) : std::nullopt;
      const bool same = want && fd.idiom && want->bytes == fd.idiom->bytes && want->static_slot == fd.idiom->static_slot &&
                        want->class_object == fd.idiom->class_object && want->getter == fd.idiom->getter;
      const std::optional<std::uint64_t> count =
          lead && lead->expect_min == lead->expect_max ? std::optional<std::uint64_t>(lead->expect_min) : std::nullopt;
      check("fd4_idiom", "", same ? CheckStatus::Confirmed : want ? CheckStatus::Differs : CheckStatus::Missing,
            fd.idiom ? std::optional<std::uint64_t>(fd.idiom_hits) : std::nullopt, count);
      ok = ok && same;
    }
    // The name layout: read by at least half the class objects; or, without it, the singletons named by their instances'
    // RTTI (O92), the registration's instance among them. Either way each singleton has its own check below.
    const bool names = fd.names.has_value() && fd.names_of > 0 && fd.names_agree * 2 >= fd.names_of;
    check("name_layout", "", names ? CheckStatus::Confirmed : CheckStatus::Missing,
          fd.names ? std::optional<std::uint64_t>(fd.names->offset) : std::nullopt);
    ok = ok && (names || (fd.diagnostics.inst_matched > 0 && instance_named()));
    for (const derive::SingletonResult& sr : fd.singletons) {
      check("singleton", sr.label, sr.status == derive::Status::Derived ? CheckStatus::Confirmed : CheckStatus::Missing,
            sr.status == derive::Status::Derived ? std::optional<std::uint64_t>(sr.static_slot) : std::nullopt);
    }
    // The registration: a function, its three registers agreeing at every call site.
    const derive::RegisterResult& reg = result_.registration;
    const derive::RegisterCandidate* best = reg.candidates.empty() ? nullptr : &reg.candidates.front();
    const bool reg_ok = reg.status == derive::Status::Derived && best != nullptr && best->function;
    check("register", "", reg_ok ? CheckStatus::Confirmed : CheckStatus::Missing,
          best != nullptr ? std::optional<std::uint64_t>(best->callee) : std::nullopt);
    const auto reg_of = [](std::optional<std::uint8_t> r) -> std::optional<en::gal::ArgRegister> {
      if (!r) return std::nullopt;
      switch (*r) {
        case 1: return en::gal::ArgRegister::Rcx;
        case 2: return en::gal::ArgRegister::Rdx;
        case 8: return en::gal::ArgRegister::R8;
        case 9: return en::gal::ArgRegister::R9;
        default: return std::nullopt;
      }
    };
    bool abi_ok = false;
    if (best != nullptr) {
      const auto t = reg_of(best->abi.task), i = reg_of(best->abi.instance), g = reg_of(best->abi.group);
      abi_ok = best->abi.shaped && best->abi_agrees && t && i && g && *t != *i && *t != *g && *i != *g;
      if (abi_ok) {
        abi_ = en::gal::RegisterAbi{*t, *i, *g};
        f_.registers = {*best->abi.task, *best->abi.instance, *best->abi.group};
      }
      check("register_abi", "", abi_ok ? CheckStatus::Confirmed : CheckStatus::Differs,
            std::uint64_t{best->abi.task.value_or(0)} * 65536 + std::uint64_t{best->abi.instance.value_or(0)} * 256 +
                best->abi.group.value_or(0));
    } else {
      check("register_abi", "", CheckStatus::Missing);
    }
    ok = ok && reg_ok && abi_ok;
    if (reg_ok) f_.callee = best->callee;
    // The group's index, in the loaded table (and as the draft layout has it, where there is one).
    const derive::GroupResult& gr = result_.group;
    const derive::GroupCandidate* table = nullptr;
    for (const derive::GroupCandidate& c : gr.candidates) {
      if (c.entries >= 2) {
        table = &c;
        break;
      }
    }
    const std::optional<std::uint64_t> expected = draft_group(gated_);
    CheckStatus gst = table == nullptr || !expected ? CheckStatus::Missing : CheckStatus::Confirmed;
    if (table != nullptr && expected && *expected != table->index) gst = CheckStatus::Differs;
    check("group", "", gst, table != nullptr ? std::optional<std::uint64_t>(table->index) : std::nullopt, expected);
    ok = ok && gst == CheckStatus::Confirmed;
    if (table != nullptr) f_.group_index = table->index;
    // The task's shape: from the task classes where the leads name a base; else the census finds it after the call.
    const derive::TaskResult& task = result_.task;
    check("task_shape", "", task.run_slot ? CheckStatus::Confirmed : CheckStatus::Missing,
          task.run_slot ? std::optional<std::uint64_t>(*task.run_slot) : std::nullopt);
    f_.vtable_slots = task.status == derive::Status::Derived && task.slots_max > 0
                          ? std::min<std::uint32_t>(task.slots_max, en::gal::kMaxTaskSlots)
                          : en::gal::kMaxTaskSlots;
    f_.object_size = s_.settings.probe.object_size;
    // The hub's instance, from its static through the image (T1 reads the image only).
    for (const derive::SingletonResult& sr : fd.singletons) {
      if (sr.label == leads_.register_instance && sr.status == derive::Status::Derived) {
        std::array<std::byte, 8> v{};
        if (s_.sys->image().read(sr.static_slot, v)) std::memcpy(&instance_, v.data(), 8);
        hub_static_ = base_ + sr.static_slot;
      }
    }
    ok = ok && instance_ != 0;
    log(Line("r1").word("stage", "checks").flag("ready", ok).flag("names", names).flag("register", reg_ok)
            .flag("abi", abi_ok).word("group", gst == CheckStatus::Confirmed ? "confirmed" : "not_confirmed")
            .flag("instance", instance_ != 0));
    return ok;
  }

  // The image read once and its RTTI walk, on T1 (WHY§293: before the callee's summary, which reads its parts and the
  // request's class from them).
  void image_bytes() {
    if (!img_) img_ = std::make_unique<en::gal::ImageBytes>(*image_, s_.sys->image());
    if (!walk_) walk_ = std::make_unique<en::gal::rtti::Walk>(en::gal::rtti::walk(*img_));
  }

  R1Body* prepare() {
    image_bytes();
    std::vector<std::uint64_t> vts;
    vts.reserve(walk_->vtables.size());
    for (const en::gal::rtti::Vtable& v : walk_->vtables) vts.push_back(base_ + v.rva);
    std::vector<en::gal::AddressRange> data;
    for (const en::gal::Section& sec : image_->sections) {
      if ((sec.characteristics & en::gal::kSectionWrite) != 0 && (sec.characteristics & en::gal::kSectionExecute) == 0) {
        data.push_back(en::gal::AddressRange{base_ + sec.rva, base_ + sec.rva + sec.mapped_size()});
      }
    }
    auto census = std::make_unique<probe::Census>(std::move(vts), hub_static_, base_, image_->size_of_image,
                                                  std::move(data));
    // Every heap static of the image (up to 65,536: 16 MB of copies kept on T1's heap), so that the play time's static
    // is not cut off by the cap.
    auto rising = std::make_unique<probe::RisingSearch>(probe::heap_slots(*s_.sys, *image_), 0x100, 1u << 16);
    std::vector<std::unique_ptr<probe::SlotSearch>> statics;
    for (std::size_t i = 0; i < leads_.statics.size() && i < result_.statics.size(); ++i) {
      const derive::StaticResult& sr = result_.statics[i];
      if (leads_.statics[i].module || sr.rtti.type_descriptor == 0) continue;
      const en::gal::rtti::Set set = en::gal::rtti::derived_from(*walk_, sr.rtti.type_descriptor, false);
      std::vector<std::uint64_t> abs;
      for (const std::uint32_t v : set.primary) abs.push_back(base_ + v);
      for (const std::uint32_t v : set.subobject) abs.push_back(base_ + v);
      sets_.push_back(std::make_unique<en::gal::VtableSet>());
      sets_.back()->assign(std::move(abs));
      statics.push_back(std::make_unique<probe::SlotSearch>(*s_.sys, *image_, *sets_.back()));
      static_labels_.push_back(leads_.statics[i].label);
    }
    f_.rising_candidates = rising->candidates();
    return new R1Body(s_, std::move(census), std::move(rising), std::move(statics), f_.object_size);  // never freed
  }

  void bases(const probe::Census& census) {
    // The type descriptors the found classes list among their bases (the class itself first, and left out here),
    // counted; the most-listed are the candidates for the task's base class.
    std::map<std::uint32_t, std::uint32_t> listed;
    for (const std::uint64_t vt : census.vtables()) {
      if (vt < base_) continue;
      const auto rva = static_cast<std::uint32_t>(vt - base_);
      const auto it = std::lower_bound(walk_->vtables.begin(), walk_->vtables.end(), rva,
                                       [](const en::gal::rtti::Vtable& v, std::uint32_t r) { return v.rva < r; });
      if (it == walk_->vtables.end() || it->rva != rva || it->locator >= walk_->locators.size()) continue;
      const en::gal::rtti::Locator& loc = walk_->locators[it->locator];
      std::set<std::uint32_t> seen;
      for (std::size_t b = 1; b < loc.bases.size(); ++b) {
        if (seen.insert(loc.bases[b]).second) ++listed[loc.bases[b]];
      }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> order(listed.begin(), listed.end());
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
      return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    f_.bases.clear();
    for (std::size_t i = 0; i < order.size() && i < 4; ++i) {
      const derive::TaskResult t = derive::task_from_base(*walk_, *img_, order[i].first);
      f_.bases.push_back(probe::Base{order[i].first, order[i].second, static_cast<std::uint32_t>(t.vtables.size()),
                                     t.slots_min, t.slots_max, t.run_slot});
    }
    if (!f_.bases.empty() && f_.bases.front().run_slot) {
      check("task_shape", "", CheckStatus::Confirmed, *f_.bases.front().run_slot);
    }
  }

  void update(const R1Body& body) {
    f_.invocations = body.invocations();
    const std::uint64_t first = body.first_us();
    f_.first_tick_ms = first == 0 ? 0 : (first / 1000 > called_mono_ms() ? first / 1000 - called_mono_ms() : 0);
    f_.slot_calls.clear();
    for (std::uint32_t i = 0; i < f_.vtable_slots; ++i) {
      if (const std::uint64_t n = s_.sys->slot_calls(i); n != 0) f_.slot_calls.emplace_back(i, n);
    }
    if (body.invocations() > 0) {  // else object_from_t1's, read after the call (WHY§227)
      f_.object_written = body.written();
      f_.object_nonzero = body.nonzero();
    }
    const en::gal::GuardedReader& r = body.reader();
    f_.copy_faults = r.faults(en::gal::FaultKind::Copy);
    f_.stack_refused = r.faults(en::gal::FaultKind::Stack);
    f_.allocations = allocations_watched();
    if (const probe::Census* c = body.census()) {
      f_.census_copies = c->copies();
      f_.census_faults = c->faults();
      f_.census_objects = c->objects();
      if (c->done() && !f_.census_done) {
        f_.census_done = true;
        f_.census_vtables.clear();
        const auto vts = c->vtables();
        const auto counts = c->counts();
        for (std::size_t i = 0; i < vts.size(); ++i) {
          if (vts[i] >= base_) f_.census_vtables.emplace_back(static_cast<std::uint32_t>(vts[i] - base_), counts[i]);
        }
        bases(*c);
      }
    }
    bool new_slot = false;
    if (const probe::RisingSearch* rs = body.rising()) {
      f_.rising_passes = rs->passes();
      f_.rising.clear();
      for (const probe::RisingSearch::Hit& h : rs->hits()) {
        f_.rising.push_back(probe::Rising{h.slot, h.offset, h.rose, h.still, h.other});
      }
      // A static whose class has no RTTI: the one slot that rose and also stood still (a play time) is its candidate.
      std::set<std::uint32_t> play;
      for (const auto& h : f_.rising) {
        if (h.rose >= 2 && h.still >= 1) play.insert(h.slot);
      }
      for (std::size_t i = 0; i < leads_.statics.size() && i < result_.statics.size(); ++i) {
        const derive::StaticLead& lead = leads_.statics[i];
        if (!lead.module && result_.statics[i].rtti.type_descriptor == 0 && play.size() == 1 &&
            slots_.emplace(lead.label, *play.begin()).second) {
          new_slot = true;
        }
      }
    }
    f_.statics.clear();
    for (std::size_t i = 0; i < body.statics().size(); ++i) {
      const probe::SlotSearch& ss = *body.statics()[i];
      probe::Static out{static_labels_[i], ss.candidates(), ss.checked(), {}};
      for (const std::uint32_t at : ss.found()) out.found.push_back(at);
      if (ss.checked() >= ss.candidates() && out.found.size() == 1 &&
          slots_.emplace(static_labels_[i], out.found[0]).second) {
        new_slot = true;
      }
      f_.statics.push_back(std::move(out));
    }
    if (f_.invocations > 0) f_.stage = "running";
    if (new_slot) derive_now();  // patterns cut at the statics' references, now that their slots are known
  }

  std::uint64_t called_mono_ms() const noexcept { return called_mono_; }

  // Spike.5's walk of the hub's reachable memory (WHY§209): breadth first from the hub's instance, up to three pointers
  // deep, read-only through the crash-guarded copy under the reader's plausibility rules, within fixed caps; pointers
  // into the image are not followed, nor, after the call, those into our task's block.
  // Since spike.9 (WHY§248) from any of three roots: the hub's instance, the tail function's singleton, the group's entry.
  probe::HubWalkSpec hub_walk_spec(std::uint64_t root) const {
    probe::HubWalkSpec spec;
    spec.instance = root;
    spec.image_base = base_;
    spec.image_end = base_ + image_->size_of_image;
    spec.own_image = own_image();
    if (const std::uint64_t task = s_.sys->task_object(); task != 0) {
      spec.keep = en::gal::AddressRange{task, task + 16 + f_.object_size};
    }
    return spec;
  }

  // Our module's mapped range: the regions from its base that VirtualQuery gives the same allocation base.
  en::gal::AddressRange own_image() const {
    const std::uint64_t base = en::gal::sys::boot::address_of_module(s_.module);
    std::uint64_t end = base;
    for (std::size_t n = 0; base != 0 && n < 256; ++n) {
      const std::optional<probe::MemoryRegion> r = s_.sys->region_at(end);
      if (!r || r->allocation_base != base || r->base != end || r->size == 0) break;
      end = r->base + r->size;
    }
    return en::gal::AddressRange{base, end};
  }

  // WHY§235: just before the call, the hub's reachable memory, kept on our heap (within the walk's 6 MiB) for the diffs;
  // since spike.9 the same from the singleton and the group's entry, each within the same bound.
  void keep_hub() {
    for (Root& r : roots_) {
      if (r.address == 0) continue;
      r.before = probe::walk_hub(*s_.sys, hub_walk_spec(r.address));
      r.kept_ms = since_ms();
      log(Line("r1").word("stage", "hub_kept").word("root", r.word).num("blocks", r.before.blocks.size())
              .num("bytes", r.before.bytes.size()).flag("capped", r.before.capped));
    }
  }

  // WHY§240: at the Mark, the same walk 2 s before the one kept for the call (A, then B): what the hub changes on its
  // own over the interval. Three walks' blocks at most are ever kept per root: A is let go once A and B are compared.
  void control_walk() {
    for (Root& r : roots_) {
      if (r.address == 0) continue;
      r.control = probe::walk_hub(*s_.sys, hub_walk_spec(r.address));
      r.control_ms = since_ms();
      log(Line("r1").word("stage", "hub_control").word("root", r.word).num("blocks", r.control.blocks.size())
              .num("bytes", r.control.bytes.size()).flag("capped", r.control.capped));
    }
    Sleep(2000);
  }
  void control_diff() {
    for (std::size_t i = 0; i < roots_.size(); ++i) {
      Root& r = roots_[i];
      if (r.control.blocks.empty() || r.before.blocks.empty()) continue;
      probe::DiffResult d = probe::diff_hub(r.control, r.before, kinds_spec());
      d.diff.at_ms = r.kept_ms > r.control_ms ? r.kept_ms - r.control_ms : 0;  // the interval A to B
      d.diff.root = r.word;
      log(Line("r1").word("stage", "hub_diff_control").word("root", r.word).num("at_ms", d.diff.at_ms)
              .num("compared", d.diff.compared).num("changed", d.diff.changed).num("new_blocks", d.diff.new_blocks)
              .flag("capped", d.diff.capped));
      r.control_changes = std::move(d.changed);
      r.controlled = true;
      if (i == 0) {
        f_.hub_diff_control = std::move(d.diff);
      } else {
        f_.hub_diff_controls.push_back(std::move(d.diff));
      }
      r.control = probe::HubWalk{};
    }
  }

  // WHY§248: the singleton and the group's entry as the table holds them at the Mark, read through the crash-guarded
  // copy when the table's shapes matched: the walk's two more roots. The hub's instance is the first.
  // WHY§289: and G, the second singleton, as the fourth, unless it is one of the others or the object CSTask's instance
  // or the entry holds where run 9's walks met one object (its memory is in their walks already).
  void table_roots() {
    roots_[0].address = instance_;
    if (!f_.table || !f_.table->matched) return;
    probe::TableAddresses at;
    (void)probe::read_table(*f_.table, table_spec(), block_reader(), &at);
    roots_[1].address = at.singleton;
    roots_[2].address = at.entry;
    if (f_.method) {
      std::uint64_t g = 0;
      const probe::MethodRead m = read_method(0, &g);
      if (g == 0) {
        f_.method->g_walk = "none";
      } else if (g == instance_ || g == at.singleton || g == at.entry) {
        f_.method->g_walk = "is_root";
      } else if (m.at_hub.value_or(false) || m.at_entry.value_or(false)) {
        f_.method->g_walk = "held_by_root";
      } else {
        f_.method->g_walk = "walked";
        roots_[3].address = g;
      }
      g_addr_ = g;
      // WHY§293: G's queue, the object G holds at the field its method passes to the queued path's call; a fifth
      // root unless it is another root's object.
      if (f_.queue && g != 0) {
        const probe::BlockReader read = block_reader();
        std::array<std::byte, 8> b{};
        if (read(g + static_cast<std::uint64_t>(f_.queue->g_disp), b)) std::memcpy(&queue_addr_, b.data(), 8);
        const bool other = std::any_of(roots_.begin(), roots_.begin() + 4,
                                       [&](const Root& r) { return r.address != 0 && r.address == queue_addr_; });
        f_.queue->walk = queue_addr_ == 0 ? "none" : other || queue_addr_ == g ? "is_root" : "walked";
        if (f_.queue->walk == std::string_view("walked")) roots_[4].address = queue_addr_;
      }
    }
    log(Line("r1").word("stage", "roots").flag("singleton", at.singleton != 0).flag("group_entry", at.entry != 0)
            .word("g", f_.method ? f_.method->g_walk : "none").word("queue", f_.queue ? f_.queue->walk : "none"));
  }

  // WHY§289: G read through its slot, its class, whether CSTask's instance and the group's entry hold it where run 9's
  // walks met one object, and the entries' words: group 110's, how many hold their own index, how many distinct values.
  probe::MethodRead read_method(std::uint64_t at_ms, std::uint64_t* g_out = nullptr) const {
    probe::MethodRead r;
    r.at_ms = at_ms;
    if (!f_.method || !f_.table || !f_.table->matched) return r;
    const probe::BlockReader read = block_reader();
    const auto word = [&](std::uint64_t at, std::size_t width) -> std::optional<std::uint64_t> {
      std::array<std::byte, 8> b{};
      if (width == 0 || width > 8 || !read(at, std::span<std::byte>(b.data(), width))) return std::nullopt;
      std::uint64_t v = 0;
      std::memcpy(&v, b.data(), width);
      return v;
    };
    const std::optional<std::uint64_t> g = word(base_ + f_.method->slot, 8);
    if (!g) return r;
    r.read = true;
    if (g_out != nullptr) *g_out = *g;
    probe::DiffSpec kinds = kinds_spec();
    kinds.group_index = 0;
    r.g = probe::classify_value(*g, kinds);
    if (*g != 0) {
      if (const auto first = word(*g, 8)) r.g_class = class_match(*first);
      if (instance_ != 0) {
        if (const auto h = word(instance_ + f_.method->hub_off, 8)) r.at_hub = *h == *g;
      }
    }
    probe::TableAddresses at;
    (void)probe::read_table(*f_.table, table_spec(), read, &at);
    if (at.entry != 0 && *g != 0) {
      if (const auto e = word(at.entry + f_.method->entry_off, 8)) r.at_entry = *e == *g;
    }
    if (f_.method->word_disp) {
      std::set<std::uint64_t> values;
      std::map<std::uint32_t, std::uint32_t> highs;  // WHY§293: the words' high 16 bits
      for (std::size_t i = 0; i < at.entries.size(); ++i) {
        if (at.entries[i] == 0) continue;
        const auto v = word(at.entries[i] + static_cast<std::uint64_t>(*f_.method->word_disp), f_.method->word_width);
        if (!v) continue;
        ++r.words_read;
        if (*v == i) ++r.own_index;
        if ((*v & 0xFFFF) == i) ++r.own_low;
        ++highs[static_cast<std::uint32_t>((*v >> 16) & 0xFFFF)];
        values.insert(*v);
        if (i == f_.group_index) r.word = static_cast<std::uint32_t>(*v);
      }
      r.distinct = static_cast<std::uint32_t>(values.size());
      for (const auto& [high, n] : highs) {
        if (r.highs.size() >= probe::kMaxHighs) break;
        r.highs.emplace_back(high, n);
      }
    }
    return r;
  }

  // WHY§293: the request's class by its vtable, and its slots: the vtable's words that point into the game's code.
  void request_class() {
    probe::RequestShape& r = *f_.request;
    r.cls = class_match(base_ + r.vtable);
    r.slots.clear();
    for (std::uint32_t i = 0; i < probe::kMaxRequestSlots; ++i) {
      const std::optional<std::uint64_t> v = img_ ? img_->u64(r.vtable + 8 * i) : std::nullopt;
      if (!v || *v < base_ || *v - base_ > 0xFFFF'FFFFull) break;
      const auto rva = static_cast<std::uint32_t>(*v - base_);
      const en::gal::Section* sec = image_->section_at(rva);
      if (sec == nullptr || (sec->characteristics & en::gal::kSectionExecute) == 0) break;
      r.slots.push_back(rva);
    }
    r.slot_count = static_cast<std::uint32_t>(r.slots.size());
    log(Line("r1").word("stage", "request").hex("vtable", r.vtable).num("disp", static_cast<std::uint64_t>(r.disp))
            .num("fields", r.fields.size()).num("slots", r.slot_count).flag("now", r.now.has_value())
            .flag("classed", r.cls && r.cls->classed));
  }

  // WHY§293: the fields of G its method reads (each load or in-place use based on G itself, by offset and width, the
  // first kMaxGFields), the byte it compares in place first (which decides the path), and G's queue: the 8-byte field
  // passed last in rcx to a call.
  void g_fields_of(const probe::CalleeCode& m) {
    g_fields_.clear();
    std::optional<std::int64_t> queue;
    for (const probe::FieldRead& r : m.g_reads) {
      if (r.base_src.kind != probe::Source::Kind::Entry || r.index) continue;
      const std::pair<std::int64_t, std::uint8_t> f{r.disp, r.width};
      if (g_fields_.size() < kMaxGFields && std::find(g_fields_.begin(), g_fields_.end(), f) == g_fields_.end()) {
        g_fields_.push_back(f);
      }
      const auto compared = [](const probe::FieldUse& u) { return u.kind == probe::UseKind::Compared; };
      if (r.width == 1 && r.direct && !f_.method->defer_disp && std::any_of(r.uses.begin(), r.uses.end(), compared)) {
        f_.method->defer_disp = r.disp;
      }
      for (const probe::FieldUse& u : r.uses) {
        if (r.width == 8 && u.kind == probe::UseKind::Passed && u.reg == std::optional<std::uint8_t>(1) && u.target &&
            (!f_.request || f_.request->now != u.target)) {
          queue = r.disp;
        }
      }
    }
    if (queue) {
      f_.queue = probe::Queue{};
      f_.queue->g_disp = *queue;
    }
    log(Line("r1").word("stage", "g_fields").num("fields", g_fields_.size())
            .flag("defer", f_.method->defer_disp.has_value()).flag("queue", queue.has_value()));
  }

  // WHY§293: a word of `width` bytes at an address, through the crash-guarded copy.
  std::optional<std::uint64_t> word_at(std::uint64_t at, std::size_t width) const {
    std::array<std::byte, 8> b{};
    if (width == 0 || width > 8 || at == 0 || !block_reader()(at, std::span<std::byte>(b.data(), width))) {
      return std::nullopt;
    }
    std::uint64_t v = 0;
    std::memcpy(&v, b.data(), width);
    return v;
  }
  // A pointer's class: the value itself where it is a vtable, else its object's first word's.
  std::optional<probe::ClassMatch> class_of(std::uint64_t v) const {
    const probe::ClassMatch direct = class_match(v);
    if (direct.vtable) return direct;
    const std::optional<std::uint64_t> first = word_at(v, 8);
    if (!first) return std::nullopt;
    return class_match(*first);
  }
  probe::DiffSpec value_kinds() const {
    probe::DiffSpec kinds = kinds_spec();
    kinds.group_index = 0;
    return kinds;
  }
  // G's manager as G holds it now (the G field the request's manager field was stored from), or nothing.
  std::optional<std::uint64_t> manager_now() const {
    if (!f_.request || g_addr_ == 0) return std::nullopt;
    for (const probe::RequestField& f : f_.request->fields) {
      if (f.role == probe::FieldRole::Manager && f.source.kind == probe::Source::Kind::Memory) {
        return word_at(g_addr_ + static_cast<std::uint64_t>(f.source.disp), 8);
      }
    }
    return std::nullopt;
  }

  // WHY§293: G's fields as read now, each by its width; the first read (just before the call) gives the path.
  void read_g_fields(std::uint64_t at_ms) {
    if (!f_.method || g_fields_.empty()) return;
    probe::GFieldsRead r;
    r.at_ms = at_ms;
    const std::optional<std::uint64_t> g = word_at(base_ + f_.method->slot, 8);
    r.read = g.has_value();
    const probe::DiffSpec kinds = value_kinds();
    for (const auto& [disp, width] : g_fields_) {
      probe::GField x;
      x.disp = disp;
      x.width = width;
      const std::optional<std::uint64_t> v =
          g && *g != 0 ? word_at(*g + static_cast<std::uint64_t>(disp), width) : std::nullopt;
      x.read = v.has_value();
      if (v && width < 8) {
        x.value = *v;
      } else if (v) {
        x.kind = probe::classify_value(*v, kinds);
        if (x.kind->kind == probe::ValueKind::Small && x.kind->value && *x.kind->value >= 4096) x.kind->value.reset();
        if (x.kind->kind == probe::ValueKind::Heap || x.kind->kind == probe::ValueKind::Game) x.cls = class_of(*v);
      }
      r.fields.push_back(std::move(x));
    }
    if (f_.method->g_fields.empty() && f_.method->defer_disp) {
      for (const probe::GField& x : r.fields) {
        if (x.disp == *f_.method->defer_disp && x.width == 1 && x.value) {
          f_.method->path_taken = *x.value == 0 ? "now" : "queued";
        }
      }
    }
    log(Line("r1").word("stage", "g_fields_read").num("at_ms", at_ms).flag("read", r.read)
            .word("path", f_.method->path_taken.empty() ? "none" : f_.method->path_taken));
    f_.method->g_fields.push_back(std::move(r));
  }

  // WHY§293: a request object's fields as `b` (its first bytes) holds them: its task (ours, or its kind and class), its
  // manager field against G's, its word and its zeroed byte.
  probe::RequestRead request_read(std::span<const std::byte> b, std::optional<std::uint64_t> manager,
                                  std::uint64_t* task_out = nullptr) const {
    probe::RequestRead r;
    if (!f_.request) return r;
    const auto field = [&](std::optional<std::uint32_t> off, std::size_t width) -> std::optional<std::uint64_t> {
      if (!off || *off + width > b.size()) return std::nullopt;
      std::uint64_t v = 0;
      std::memcpy(&v, b.data() + *off, width);
      return v;
    };
    if (const auto t = field(f_.request->task_field, 8)) {
      const std::uint64_t ours = s_.sys->task_object();
      r.ours = ours != 0 && *t == ours;
      if (!*r.ours) {
        r.task = probe::classify_value(*t, value_kinds());
        if (r.task->kind == probe::ValueKind::Heap || r.task->kind == probe::ValueKind::Game) {
          r.task_class = class_of(*t);
          if (task_out != nullptr) *task_out = *t;
        }
      }
    }
    if (const auto m = field(f_.request->field(probe::FieldRole::Manager), 8); m && manager) r.manager = *m == *manager;
    if (const auto w = field(f_.request->field(probe::FieldRole::Word), 4)) r.word = static_cast<std::uint32_t>(*w);
    if (const auto z = field(f_.request->field(probe::FieldRole::Zero), 1)) r.flag = static_cast<std::uint32_t>(*z);
    return r;
  }

  // WHY§293: G's queue walked (or the walk kept just before the call), its blocks whose first word is the request's
  // vtable, each read as a request.
  void read_queue(std::uint64_t at_ms, const probe::HubWalk* kept = nullptr) {
    if (!f_.queue) return;
    probe::QueueRead q;
    q.at_ms = at_ms;
    if (queue_addr_ != 0 && f_.request) {
      probe::HubWalk fresh;
      if (kept == nullptr || kept->blocks.empty()) {
        fresh = probe::walk_hub(*s_.sys, hub_walk_spec(queue_addr_));
        kept = &fresh;
      }
      q.read = !kept->blocks.empty();
      q.blocks = kept->blocks.size();
      const std::uint64_t vt = base_ + f_.request->vtable;
      const std::optional<std::uint64_t> manager = manager_now();
      for (const probe::HubBlock& blk : kept->blocks) {
        if (!blk.read || blk.len < 8 || blk.at + blk.len > kept->bytes.size()) continue;
        const std::span<const std::byte> b(kept->bytes.data() + blk.at, blk.len);
        std::uint64_t first = 0;
        std::memcpy(&first, b.data(), 8);
        if (first != vt) continue;
        ++q.found;
        if (q.requests.size() >= probe::kMaxQueued) continue;
        probe::QueuedRequest x;
        x.depth = blk.depth;
        x.path.assign(blk.path.begin(), blk.path.begin() + std::min<std::size_t>(blk.depth, blk.path.size()));
        x.fields = request_read(b.first(std::min<std::size_t>(b.size(), 64)), manager);
        q.requests.push_back(std::move(x));
      }
    }
    log(Line("r1").word("stage", "queue").num("at_ms", at_ms).flag("read", q.read).num("blocks", q.blocks)
            .num("found", q.found));
    f_.queue->reads.push_back(std::move(q));
  }

  // WHY§293: the game's requests a memory search found, each read as a request, and for each distinct class of their
  // tasks (the first kMaxTaskClasses) its vtable's slots and the first 16 words of one such task.
  probe::RequestSearch request_search_of(const probe::MemorySearch& m) const {
    probe::RequestSearch rs;
    const std::optional<std::uint64_t> manager = manager_now();
    const probe::DiffSpec kinds = value_kinds();
    const probe::BlockReader read = block_reader();
    for (const std::uint64_t at : m.request_at) {
      std::array<std::byte, 32> b{};
      std::uint64_t task = 0;
      rs.hits.push_back(read(at, b) ? request_read(b, manager, &task) : probe::RequestRead{});
      const std::optional<std::uint64_t> first = task != 0 ? word_at(task, 8) : std::nullopt;
      if (!first || *first < base_ || *first - base_ > 0xFFFF'FFFFull) continue;
      const auto vt = static_cast<std::uint32_t>(*first - base_);
      const probe::ClassMatch cls = class_match(*first);
      const auto same = std::find_if(rs.tasks.begin(), rs.tasks.end(), [&](const probe::GameTask& t) {
        return cls.classed ? t.cls.type_descriptor == cls.type_descriptor : t.vtable == vt;
      });
      if (same != rs.tasks.end()) {
        ++same->count;
        continue;
      }
      if (rs.tasks.size() >= probe::kMaxTaskClasses) continue;
      probe::GameTask t;
      t.cls = cls;
      t.vtable = vt;
      t.count = 1;
      const auto it = std::lower_bound(walk_->vtables.begin(), walk_->vtables.end(), vt,
                                       [](const en::gal::rtti::Vtable& v, std::uint32_t r) { return v.rva < r; });
      if (it != walk_->vtables.end() && it->rva == vt) {
        t.slot_count = std::min<std::uint32_t>(it->slots, static_cast<std::uint32_t>(probe::kMaxVtableSlots));
      }
      std::array<std::byte, 8 * probe::kTaskWords> words{};
      if (read(task, words)) {
        for (std::size_t k = 0; k < probe::kTaskWords; ++k) {
          std::uint64_t v = 0;
          std::memcpy(&v, words.data() + 8 * k, 8);
          probe::TaskWord w;
          w.value = probe::classify_value(v, kinds);
          if (w.value.kind == probe::ValueKind::Heap || w.value.kind == probe::ValueKind::Game) w.cls = class_of(v);
          t.words.push_back(std::move(w));
        }
      }
      rs.tasks.push_back(std::move(t));
    }
    return rs;
  }

  // WHY§293: the RTTI classes of the objects met (G's manager, the request, CSTask's instance, the group's entry, G
  // where it classes, the game's tasks the searches found), each with its bases, and the bases two or more share.
  void classes() {
    if (!walk_) return;
    f_.classes.clear();
    f_.shared_bases.clear();
    std::vector<std::pair<std::string_view, std::uint64_t>> objects;  // the object, its first word
    if (const auto m = manager_now(); m && *m != 0) {
      if (const auto first = word_at(*m, 8)) objects.emplace_back("manager", *first);
    }
    if (f_.request) objects.emplace_back("request", base_ + f_.request->vtable);
    if (instance_ != 0) {
      if (const auto first = word_at(instance_, 8)) objects.emplace_back("task_hub", *first);
    }
    if (f_.table && f_.table->matched) {
      probe::TableAddresses at;
      (void)probe::read_table(*f_.table, table_spec(), block_reader(), &at);
      if (at.entry != 0) objects.emplace_back("group_entry", at.entry_first);
    }
    if (g_addr_ != 0) {
      const auto first = word_at(g_addr_, 8);
      if (first && class_match(*first).classed) objects.emplace_back("g", *first);
    }
    std::set<std::uint32_t> tasks;
    for (const probe::RequestSearch& rs : f_.request_search) {
      for (const probe::GameTask& t : rs.tasks) {
        if (tasks.insert(t.vtable).second) objects.emplace_back("game_task", base_ + t.vtable);
      }
    }
    std::map<std::uint32_t, std::uint32_t> listed;
    for (const auto& [object, first] : objects) {
      probe::ClassBases c;
      c.object = object;
      c.cls = class_match(first);
      if (!c.cls.vtable) continue;
      const std::uint32_t rva = static_cast<std::uint32_t>(first - base_);
      const auto it = std::lower_bound(walk_->vtables.begin(), walk_->vtables.end(), rva,
                                       [](const en::gal::rtti::Vtable& v, std::uint32_t r) { return v.rva < r; });
      if (it != walk_->vtables.end() && it->rva == rva && it->locator < walk_->locators.size()) {
        const std::vector<std::uint32_t>& bases = walk_->locators[it->locator].bases;
        std::set<std::uint32_t> seen;
        for (std::size_t b = 1; b < bases.size(); ++b) {
          if (!seen.insert(bases[b]).second) continue;
          ++c.base_count;
          if (c.bases.size() < probe::kMaxBases) c.bases.push_back(bases[b]);
          ++listed[bases[b]];
        }
      }
      f_.classes.push_back(std::move(c));
    }
    for (const auto& [td, n] : listed) {
      if (n >= 2) f_.shared_bases.emplace_back(td, n);
    }
    log(Line("r1").word("stage", "classes_bases").num("classes", f_.classes.size())
            .num("shared", f_.shared_bases.size()));
  }

  // WHY§289: the array's views, as the hub's walk just before the call holds them (by run 9's paths).
  void find_views() {
    probe::ArrayViews v;
    const probe::HubWalk& walk = roots_[0].before;
    views_.assign(kViewPaths.size(), View{});
    for (std::size_t i = 0; i < kViewPaths.size(); ++i) {
      probe::ArrayView a;
      a.path.assign(kViewPaths[i].begin(), kViewPaths[i].end());
      for (const probe::HubBlock& b : walk.blocks) {
        if (b.depth != 3 || !b.read || b.path != kViewPaths[i] || b.at + b.len > walk.bytes.size()) continue;
        a.found = true;
        a.bytes = b.len;
        views_[i].address = b.address;
        views_[i].bytes.assign(walk.bytes.begin() + static_cast<std::ptrdiff_t>(b.at),
                               walk.bytes.begin() + static_cast<std::ptrdiff_t>(b.at + b.len));
        break;
      }
      v.views.push_back(std::move(a));
    }
    log(Line("r1").word("stage", "views").num("found", static_cast<std::uint64_t>(std::count_if(
        v.views.begin(), v.views.end(), [](const probe::ArrayView& a) { return a.found; }))));
    f_.views = std::move(v);
  }
  // Each view read again and compared with the walk's bytes: the changed words, a new rdata value's class, and whether
  // a changed word holds the game's main thread's id or ours.
  void read_views() {
    if (!f_.views) return;
    const std::uint64_t at_ms = since_ms() - called_ms_;
    probe::DiffSpec kinds = kinds_spec();
    kinds.group_index = 0;
    const probe::BlockReader read = block_reader();
    for (std::size_t i = 0; i < views_.size(); ++i) {
      if (views_[i].address == 0) continue;
      std::vector<std::byte> now(views_[i].bytes.size());
      probe::ViewRead r;
      if (read(views_[i].address, now)) {
        r = probe::diff_view(views_[i].bytes, now, kinds, main_thread_, GetCurrentThreadId());
        for (probe::ViewWord& x : r.words) {
          if (x.after.kind == probe::ValueKind::Game && x.after.section == probe::SectionKind::Rdata && x.after.rva) {
            x.after_class = class_match(base_ + *x.after.rva);
          }
        }
      }
      r.at_ms = at_ms;
      r.view = static_cast<std::uint32_t>(i);
      log(Line("r1").word("stage", "view").num("view", i).num("at_ms", at_ms).flag("read", r.read)
              .num("changed", r.changed).flag("main_thread", r.main_thread));
      f_.views->reads.push_back(std::move(r));
    }
  }

  // The diff's classification of values: our task's block, our vtable, our module, the game's image by its sections,
  // what any other pointer points into.
  probe::DiffSpec kinds_spec() const {
    probe::DiffSpec spec;
    spec.group_index = f_.group_index;
    if (const std::uint64_t target = s_.sys->task_object(); target != 0) {
      spec.task = en::gal::AddressRange{target, target + 16 + f_.object_size};
    }
    if (vtable_ != 0) spec.vtable = en::gal::AddressRange{vtable_, vtable_ + 8 * thunks_.size()};
    spec.own_base = en::gal::sys::boot::address_of_module(s_.module);
    spec.game_base = base_;
    spec.game_sections = sections();
    spec.query = [this](std::uint64_t a) { return s_.sys->region_at(a); };
    return spec;
  }
  std::vector<probe::SectionSpan> sections() const {
    std::vector<probe::SectionSpan> out;
    for (const en::gal::Section& sec : image_->sections) {
      out.push_back(probe::SectionSpan{sec.rva, sec.mapped_size(), probe::section_kind_of(sec.characteristics)});
    }
    return out;
  }
  static const char* returned_word(probe::ValueKind k) noexcept {
    switch (k) {
      case probe::ValueKind::Zero: return "zero";
      case probe::ValueKind::Small: return "small";
      case probe::ValueKind::Task: return "task";
      case probe::ValueKind::Ours: return "ours";
      case probe::ValueKind::Game: return "game";
      case probe::ValueKind::Heap: return "heap";
      default: return "other";
    }
  }

  // WHY§240: the tail function's table, when its shapes matched, read through the crash-guarded copy under the reader's
  // plausibility rules on T1: just before the call, and 2 s after it. Where the group's entry leads through its vtable
  // to a function of the game's code, that function is summarised one level deeper (once).
  void read_table(bool before) {
    if (!f_.table || !f_.table->matched) return;
    probe::TableAddresses at;
    probe::TableRead r = probe::read_table(*f_.table, table_spec(), block_reader(), &at);
    r.at_ms = before ? 0 : since_ms() - called_ms_;
    log(Line("r1").word("stage", before ? "table_before" : "table_after").flag("read", r.read)
            .num("entries", r.entries).num("nonzero", r.nonzero)
            .word("entry", r.entry ? returned_word(r.entry->kind) : "none"));
    if (r.slot_value && r.slot_value->kind == probe::ValueKind::Game &&
        r.slot_value->section == probe::SectionKind::Code && r.slot_value->rva && f_.table->site) {
      summarise_group(*r.slot_value->rva);
    }
    if (before) {  // WHY§248: the classes of the singleton and of the group's entry, by their first words
      if (at.singleton != 0) f_.table->singleton_class = class_match(at.singleton_first);
      if (at.entry != 0) f_.table->entry_class = class_match(at.entry_first);
      const auto word = [](const std::optional<probe::ClassMatch>& m) {
        return !m ? "none" : m->match == probe::LeadMatch::Imp ? "imp" : m->match == probe::LeadMatch::Name ? "name"
                                                                                                             : "none";
      };
      log(Line("r1").word("stage", "classes").word("singleton", word(f_.table->singleton_class))
              .word("entry", word(f_.table->entry_class)));
    }
    (before ? f_.table->before : f_.table->after) = std::move(r);
  }

  probe::TableReadSpec table_spec() const {
    probe::TableReadSpec spec;
    spec.module_base = base_;
    spec.group_index = f_.group_index;
    spec.instance = instance_;
    for (const derive::SingletonResult& sr : result_.fd4.singletons) {
      if (sr.status != derive::Status::Derived) continue;
      std::array<std::byte, 8> v{};
      std::uint64_t inst = 0;
      if (!s_.sys->image().read(sr.static_slot, v)) continue;
      std::memcpy(&inst, v.data(), 8);
      for (std::size_t i = 0; i < leads_.singletons.size(); ++i) {
        if (leads_.singletons[i].label == sr.label && inst != 0) {
          spec.leads.emplace_back(static_cast<std::uint32_t>(i), inst);
        }
      }
    }
    spec.kinds = kinds_spec();
    spec.kinds.group_index = 0;
    return spec;
  }
  probe::BlockReader block_reader() const {
    return [this](std::uint64_t at, std::span<std::byte> out) {
      if (en::gal::implausible(at, out.size(), 1, s_.sys->stacks())) return false;
      return s_.sys->copier().copy(at, out).copied;
    };
  }

  // WHY§248: an object's class through its first word, by the RTTI walk of the live image, against the leads' names (O92's
  // rule: the innermost name, as the lead's or with Imp). Numbers only: the lead's index, never a name.
  probe::ClassMatch class_match(std::uint64_t first) const {
    probe::ClassMatch m;
    if (!walk_ || !img_ || first < base_ || first - base_ > 0xFFFF'FFFFull) return m;
    const auto rva = static_cast<std::uint32_t>(first - base_);
    const auto it = std::lower_bound(walk_->vtables.begin(), walk_->vtables.end(), rva,
                                     [](const en::gal::rtti::Vtable& v, std::uint32_t r) { return v.rva < r; });
    if (it == walk_->vtables.end() || it->rva != rva || it->locator >= walk_->locators.size()) return m;
    m.vtable = true;
    const en::gal::rtti::Locator& loc = walk_->locators[it->locator];
    const std::optional<std::string> cls =
        loc.offset == 0 ? derive::innermost_class(*img_, loc.type_descriptor) : std::nullopt;
    if (!cls) return m;
    m.classed = true;
    m.type_descriptor = loc.type_descriptor;
    std::vector<std::string> names;
    names.reserve(leads_.singletons.size());
    for (const derive::NamedLead& lead : leads_.singletons) names.push_back(lead.name);
    probe::match_lead(m, *cls, names);
    return m;
  }

  // WHY§248: the group's own function (the target of the tail function's jump at the table's site: direct, or through
  // the entry's vtable), summarised with the group's entry in rcx and our task followed in, and its own direct callees
  // and tail jumps one level further, each with the arguments its site passes; kMaxGroup summaries at most, once.
  // WHY§289: the site that reaches G's method (rcx loaded from a slot, our task passed) gives its entry state (G in rcx,
  // the entry's word); a stub there is followed to G's method at the same depth, and G's method's own callees and tail
  // jumps are summarised one level further (4), stubs again followed at their depth.
  void summarise_group(std::uint32_t rva) {
    if (group_done_ || !f_.table || !f_.table->site) return;
    const probe::InnerCallee* tail = nullptr;
    for (const probe::InnerCallee& t : f_.callee_inner) {
      if (t.tail && t.depth == 1 && t.rva == f_.table->rva) tail = &t;
    }
    if (tail == nullptr) return;
    const auto jump = std::find_if(tail->code.calls.begin(), tail->code.calls.end(),
                                   [&](const probe::CalleeCall& k) { return k.offset == *f_.table->site; });
    if (jump == tail->code.calls.end()) return;
    group_done_ = true;
    const probe::EntryState entry = probe::group_entry_at(*jump);
    std::vector<probe::InnerCallee> group;
    group.push_back(inner_at(rva, *jump, entry, 2, tail->rva, true));
    std::set<std::uint32_t> seen{rva};
    const probe::CalleeCode gcode = group.front().code;
    const probe::CalleeCall* msite = probe::method_site(gcode, f_.registers[0]);
    std::optional<std::size_t> method;  // G's method's summary, in `group`
    probe::EntryState method_entry{};
    for (const probe::CalleeCall& k : gcode.calls) {
      if (group.size() >= kMaxGroup) break;
      if (!k.target || (k.form != probe::CallForm::Direct && k.form != probe::CallForm::Tail) ||
          !seen.insert(*k.target).second) {
        continue;
      }
      const bool at_method = msite != nullptr && k.offset == msite->offset;
      const probe::EntryState e = at_method ? probe::method_entry_at(k, entry) : probe::entry_at(k, entry);
      group.push_back(inner_at(*k.target, k, e, 3, rva, k.form == probe::CallForm::Tail));
      if (!at_method) continue;
      probe::Method m;
      m.site = k.offset;
      for (const probe::PassedSource& p : k.pass_src) {
        if (p.reg == 1 && p.source.kind == probe::Source::Kind::Rip) m.slot = p.source.target;
        const std::size_t i = p.reg == 1 ? 0 : p.reg == 2 ? 1 : p.reg == 8 ? 2 : 3;
        if (e[i].word && p.source.kind == probe::Source::Kind::Memory && !m.word_disp) {
          m.word_disp = p.source.disp;
          m.word_width = p.source.width;
        }
      }
      m.hub_off = kGAtHub;
      m.entry_off = kGAtEntry;
      m.rva = *k.target;
      method = group.size() - 1;
      method_entry = e;
      const probe::CalleeCall* sj = probe::stub_jump(group.back().code);
      if (sj != nullptr && sj->target && group.size() < kMaxGroup && seen.insert(*sj->target).second) {
        m.stub = *k.target;
        m.rva = *sj->target;
        method_entry = probe::through_stub(group.back().code, e);
        group.push_back(inner_at(*sj->target, k, method_entry, 3, rva, true));
        group.back().stub = m.stub;
        group.back().zeroed = zeroed(method_entry);
        method = group.size() - 1;
      }
      f_.method = m;
    }
    if (method) {
      const std::uint32_t mrva = group[*method].rva;
      const probe::CalleeCode mcode = group[*method].code;
      const std::uint8_t task = f_.registers[0];
      // WHY§293: the request G's method builds, its class and slots; G's fields it reads; G's queue.
      f_.request = probe::request_shape(mcode, method_entry, task, sections());
      if (f_.request) request_class();
      g_fields_of(mcode);
      const auto entry_of = [&](const probe::CalleeCall& k, const probe::EntryState& from) {
        return f_.request ? probe::request_entry_at(k, from, *f_.request, task) : probe::entry_at(k, from);
      };
      // A summary (and the entry it was made with) whose callees are summarised one level further (WHY§293): the
      // function G's method calls with the request, and each of the request's slots.
      std::vector<std::pair<std::size_t, probe::EntryState>> further;
      // A summary's stub followed to its target at the same depth: the summary its callees come from.
      const auto follow = [&](const probe::CalleeCall& k, const probe::EntryState& e, std::uint32_t depth,
                              std::uint32_t parent) -> std::size_t {
        const probe::CalleeCall* sj = probe::stub_jump(group.back().code);
        if (sj != nullptr && sj->target && group.size() < kMaxGroup && seen.insert(*sj->target).second) {
          const std::uint32_t stub = *k.target;
          const probe::EntryState te = probe::through_stub(group.back().code, e);
          group.push_back(inner_at(*sj->target, k, te, depth, parent, true));
          group.back().stub = stub;
          group.back().zeroed = zeroed(te);
          further_entry_ = te;
        } else {
          further_entry_ = e;
        }
        return group.size() - 1;
      };
      for (const probe::CalleeCall& k : mcode.calls) {
        if (group.size() >= kMaxGroup) break;
        if (!k.target || (k.form != probe::CallForm::Direct && k.form != probe::CallForm::Tail) ||
            !seen.insert(*k.target).second) {
          continue;
        }
        const probe::EntryState e = entry_of(k, method_entry);
        group.push_back(inner_at(*k.target, k, e, 4, mrva, k.form == probe::CallForm::Tail));
        const std::size_t at = follow(k, e, 4, mrva);
        if (f_.request && f_.request->now == k.target) further.emplace_back(at, further_entry_);
      }
      // The request's slots, each entered with the request in rcx (its site the store of the request's vtable).
      if (f_.request) {
        probe::CalleeCall site;
        site.offset = f_.request->site;
        site.target = f_.request->vtable;
        probe::Source place;
        place.kind = probe::Source::Kind::Stack;
        place.disp = f_.request->disp;
        site.pass_src.push_back(probe::PassedSource{1, place});
        probe::EntryState e{};
        e[0].request = true;
        for (std::size_t i = 0; i < f_.request->slots.size() && i < kMaxRequestSummaries; ++i) {
          const std::uint32_t slot = f_.request->slots[i];
          if (!seen.insert(slot).second) {  // already summarised (the function G's method calls, say): its slot
            for (probe::InnerCallee& in : group) {
              if (in.rva == slot && in.depth == 4 && !in.request_slot) in.request_slot = static_cast<std::uint32_t>(i);
            }
            continue;
          }
          if (group.size() >= kMaxGroup) break;
          probe::CalleeCall k = site;
          k.target = slot;
          group.push_back(inner_at(slot, k, e, 4, mrva, false));
          group.back().request_slot = static_cast<std::uint32_t>(i);
          const std::size_t at = follow(k, e, 4, mrva);
          group[at].request_slot = static_cast<std::uint32_t>(i);
          if (const probe::CalleeCall* sj = probe::stub_jump(group[at].code); sj != nullptr && sj->target) {
            for (probe::InnerCallee& in : group) {  // a stub to a function summarised already: that one's slot too
              if (in.rva == *sj->target && !in.request_slot) in.request_slot = static_cast<std::uint32_t>(i);
            }
          }
          further.emplace_back(at, further_entry_);
        }
      }
      // One level further (5), within the cap.
      for (const auto& [at, from] : further) {
        const std::vector<probe::CalleeCall> calls = group[at].code.calls;
        const std::uint32_t parent = group[at].rva;
        for (const probe::CalleeCall& k : calls) {
          if (group.size() >= kMaxGroup) break;
          if (!k.target || (k.form != probe::CallForm::Direct && k.form != probe::CallForm::Tail) ||
              !seen.insert(*k.target).second) {
            continue;
          }
          const probe::EntryState e = entry_of(k, from);
          group.push_back(inner_at(*k.target, k, e, 5, parent, k.form == probe::CallForm::Tail));
          (void)follow(k, e, 5, parent);
        }
      }
    }
    for (probe::InnerCallee& in : group) f_.callee_inner.push_back(std::move(in));
    if (f_.method) {
      log(Line("r1").word("stage", "method").hex("rva", f_.method->rva).hex("slot", f_.method->slot)
              .flag("stub", f_.method->stub.has_value()).flag("word", f_.method->word_disp.has_value()));
    }
  }
  static std::vector<std::uint8_t> zeroed(const probe::EntryState& e) {
    static constexpr std::array<std::uint8_t, 4> kRegs = {1, 2, 8, 9};
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < e.size(); ++i) {
      if (e[i].zero) out.push_back(kRegs[i]);
    }
    return out;
  }
  probe::InnerCallee inner_at(std::uint32_t rva, const probe::CalleeCall& site, const probe::EntryState& entry,
                              std::uint32_t depth, std::uint32_t parent, bool tail) const {
    const auto [code, extent] = code_at(rva);
    probe::InnerCallee in;
    in.rva = rva;
    in.site = site.offset;
    in.tail = tail;
    in.code = probe::summarise_callee(code, rva, extent, entry, f_.registers[0], options());
    in.depth = depth;
    in.parent = parent;
    for (const probe::PassedSource& p : site.pass_src) {
      probe::PassedSource e = p;
      const std::size_t i = p.reg == 1 ? 0 : p.reg == 2 ? 1 : p.reg == 8 ? 2 : 3;
      e.holds_entry = entry[i].entry;
      e.holds_g = entry[i].g;
      e.holds_word = entry[i].word;
      e.holds_request = entry[i].request;
      e.holds_manager = entry[i].manager;
      in.entry_src.push_back(std::move(e));
    }
    probe::mark_sections(in.code, sections());
    log(Line("r1").word("stage", "inner").hex("rva", in.rva).num("site", in.site).num("depth", depth)
            .num("bytes", in.code.bytes).num("calls", in.code.call_count).num("task_reads", in.code.task_read_count)
            .num("entry_reads", in.code.entry_read_count).num("task_stores", in.code.task_store_count)
            .num("g_reads", in.code.g_read_count).num("word_uses", in.code.word_use_count)
            .num("parts", in.code.parts.size()).num("request_reads", in.code.request_read_count)
            .num("manager_reads", in.code.manager_read_count));
    return in;
  }

  // WHY§289: what the game's own call sites of the registration (derive's, the first 16) pass as the task, read from
  // the code of the function holding each call (the exception directory's), up to kMaxSiteSpan into it.
  void game_sites() {
    const derive::RegisterResult& reg = result_.registration;
    if (reg.candidates.empty() || f_.registers[0] == 0) return;
    for (const std::uint32_t site : reg.candidates.front().callers) {
      probe::SiteArg a;
      a.site = site;
      const auto fn = std::upper_bound(image_->functions.begin(), image_->functions.end(), site,
                                       [](std::uint32_t at, const en::gal::RuntimeFunction& r) { return at < r.begin; });
      if (fn != image_->functions.begin()) {
        const en::gal::RuntimeFunction& r = *std::prev(fn);
        const std::uint64_t into = std::uint64_t{site} - r.begin;
        if (into < r.length() && into + 16 <= kMaxSiteSpan) {
          std::vector<std::byte> code(static_cast<std::size_t>(into) + 16);
          if (s_.sys->image().read(r.begin, code)) a = probe::task_argument(code, r.begin, site, f_.registers[0]);
        }
      }
      f_.game_sites.push_back(a);
    }
    log(Line("r1").word("stage", "game_sites").num("sites", f_.game_sites.size())
            .num("decoded", static_cast<std::uint64_t>(std::count_if(f_.game_sites.begin(), f_.game_sites.end(),
                                                                     [](const probe::SiteArg& a) { return a.decoded; }))));
  }

  // WHY§289: the registration's sibling of the same argument shape (derive's next candidate with the same three
  // registers), summarised with its direct callees and tail jumps breadth first down to the registration chain's depth
  // (4), tail jumps first, stubs followed at their depth, kMaxSibling summaries at most, each once.
  void summarise_sibling() {
    const auto& cands = result_.registration.candidates;
    if (cands.size() < 2 || f_.registers[0] == 0) return;
    const derive::RegisterCandidate& best = cands.front();
    const derive::RegisterCandidate* sib = nullptr;
    for (std::size_t i = 1; i < cands.size() && sib == nullptr; ++i) {
      const derive::RegisterCandidate& c = cands[i];
      if (c.function && c.abi.shaped && c.abi_agrees && c.abi.task == best.abi.task &&
          c.abi.instance == best.abi.instance && c.abi.group == best.abi.group && c.callee != best.callee) {
        sib = &c;
      }
    }
    if (sib == nullptr) return;
    probe::Sibling s;
    s.rva = sib->callee;
    s.sites = sib->sites;
    {
      const auto [code, extent] = code_at(sib->callee);
      s.code = probe::summarise_callee(code, sib->callee, extent, probe::kOwnArguments, f_.registers[0], options());
      probe::mark_sections(s.code, sections());
    }
    std::set<std::uint32_t> seen{sib->callee};
    std::vector<probe::EntryState> entries;
    struct Item {
      std::optional<std::size_t> at;  // the summary in s.inner (none: the sibling itself)
      std::uint32_t depth = 0;
    };
    std::vector<Item> queue{Item{std::nullopt, 0}};
    for (std::size_t q = 0; q < queue.size() && s.inner.size() < kMaxSibling; ++q) {
      const Item item = queue[q];
      if (item.depth >= 4) continue;
      const std::vector<probe::CalleeCall> calls = item.at ? s.inner[*item.at].code.calls : s.code.calls;
      const probe::EntryState from = item.at ? entries[*item.at] : probe::kOwnArguments;
      const std::uint32_t parent = item.at ? s.inner[*item.at].rva : s.rva;
      for (const bool tails : {true, false}) {
        for (const probe::CalleeCall& k : calls) {
          if (s.inner.size() >= kMaxSibling) break;
          if (!k.target || k.form != (tails ? probe::CallForm::Tail : probe::CallForm::Direct) ||
              !seen.insert(*k.target).second) {
            continue;
          }
          const probe::EntryState e = item.at ? probe::entry_at(k, from) : probe::entry_at(k);
          s.inner.push_back(inner_at(*k.target, k, e, item.depth + 1, parent, tails));
          entries.push_back(e);
          const probe::CalleeCall* sj = probe::stub_jump(s.inner.back().code);
          if (sj != nullptr && sj->target && s.inner.size() < kMaxSibling && seen.insert(*sj->target).second) {
            const std::uint32_t stub = *k.target;
            const probe::EntryState te = probe::through_stub(s.inner.back().code, e);
            s.inner.push_back(inner_at(*sj->target, k, te, item.depth + 1, parent, true));
            s.inner.back().stub = stub;
            s.inner.back().zeroed = zeroed(te);
            entries.push_back(te);
          }
          queue.push_back(Item{s.inner.size() - 1, item.depth + 1});
        }
      }
    }
    log(Line("r1").word("stage", "sibling").hex("rva", s.rva).num("sites", s.sites).num("inner", s.inner.size()));
    f_.sibling = std::move(s);
  }

  // WHY§235: our fake vtable's address (our object's first word) and its slots' (our module's thunks), read back from
  // our own memory after the call, for the whole-memory search and the diff's kinds.
  void read_vtable() {
    const std::uint64_t at = s_.sys->task_object();
    std::array<std::byte, 8> v{};
    if (at == 0 || !s_.sys->copier().copy(at, v).copied) return;
    std::memcpy(&vtable_, v.data(), 8);
    std::vector<std::byte> t(std::size_t{8} * f_.vtable_slots);
    if (vtable_ == 0 || t.empty() || !s_.sys->copier().copy(vtable_, t).copied) return;
    thunks_.assign(f_.vtable_slots, 0);
    for (std::size_t i = 0; i < thunks_.size(); ++i) std::memcpy(&thunks_[i], t.data() + 8 * i, 8);
  }

  // WHY§209: the hub's reachable memory searched for our task object; WHY§235: and compared with the walk kept from
  // before the call, block by block by path (the second diff against the first's changes, for their steadiness).
  // WHY§248: the same diffs from the singleton and the group's entry, each `root` named.
  void search_hub() {
    const std::uint64_t target = s_.sys->task_object();
    for (std::size_t i = 0; i < roots_.size(); ++i) {
      Root& r = roots_[i];
      if (r.address == 0) continue;
      const std::uint64_t at_ms = since_ms() - called_ms_;
      const probe::HubWalk walk = probe::walk_hub(*s_.sys, hub_walk_spec(r.address));
      if (i == 0) {
        probe::HubSearch h = probe::search_walk(walk, target, target + f_.object_size, f_.group_index);
        h.at_ms = at_ms;
        log(Line("r1").word("stage", "hub_search").num("at_ms", h.at_ms).num("nodes", h.nodes).num("found", h.found)
                .num("inside", h.inside).num("faults", h.faults));
        f_.hub_search.push_back(std::move(h));
      }
      if (r.before.blocks.empty()) continue;
      probe::DiffSpec spec = kinds_spec();
      spec.earlier = r.diffs == 0 ? nullptr : &r.first_changes;
      spec.control = r.controlled ? &r.control_changes : nullptr;
      probe::DiffResult d = probe::diff_hub(r.before, walk, spec);
      d.diff.at_ms = at_ms;
      d.diff.root = r.word;
      log(Line("r1").word("stage", "hub_diff").word("root", r.word).num("at_ms", d.diff.at_ms)
              .num("compared", d.diff.compared).num("changed", d.diff.changed).num("new_blocks", d.diff.new_blocks)
              .flag("capped", d.diff.capped));
      if (r.diffs == 0) {
        r.first_changes = std::move(d.changed);
      } else {
        r.before = probe::HubWalk{};  // the last diff: what was kept is let go
        r.first_changes.clear();
        r.control_changes.clear();
      }
      ++r.diffs;
      f_.hub_diff.push_back(std::move(d.diff));
    }
  }

  // The code at `rva` as the live image holds it: within its function of the exception directory (its length the
  // extent), or up to the 4 KiB bound, never past its section. WHY§293: for a function with an extent, the 64 KiB from
  // its start (its parts past the extent among them), from the image read once where it has been.
  std::pair<std::vector<std::byte>, std::uint32_t> code_at(std::uint32_t rva) const {
    std::uint32_t extent = 0;
    const auto fn = std::lower_bound(image_->functions.begin(), image_->functions.end(), rva,
                                     [](const en::gal::RuntimeFunction& r, std::uint32_t at) { return r.begin < at; });
    if (fn != image_->functions.end() && fn->begin == rva) extent = fn->length();
    std::uint32_t len = extent != 0 ? std::max(std::min(extent, probe::kCalleeBound), probe::kPartSpan)
                                    : probe::kCalleeBound;
    for (const en::gal::Section& sec : image_->sections) {
      if (sec.contains(rva)) len = std::min(len, sec.rva + sec.mapped_size() - rva);
    }
    std::vector<std::byte> code(len);
    if (img_) {
      const std::span<const std::byte> v = img_->view(rva, len);
      if (v.size() == len) {
        std::copy(v.begin(), v.end(), code.begin());
      } else {
        code.clear();
      }
    } else if (!s_.sys->image().read(rva, code)) {
      code.clear();
    }
    return {std::move(code), extent};
  }
  // What every summary knows of the image (WHY§293): the starts, the directory's entries and chained ones, the request.
  probe::SummaryOptions options() const {
    probe::SummaryOptions o;
    o.starts = starts_;
    o.functions = image_->functions;
    o.chained = chained_;
    o.request = f_.request ? &*f_.request : nullptr;
    return o;
  }

  // WHY§227: what the callee does with its arguments, from the live image (the code the call runs), decoded within its
  // function of the exception directory, or up to its first return, and at most 4 KiB; no call is followed. WHY§235:
  // then each callee it calls directly or tail-jumps to, one level deeper, with the arguments its call site passes.
  // WHY§248: our task's argument (the registers' first) is followed in every summary: its loads and their uses.
  void summarise_callee() {
    if (f_.callee == 0) return;
    const std::uint8_t task = f_.registers[0];
    image_bytes();  // WHY§293: the image read once, its RTTI walk, and the directory's chained entries
    chained_ = probe::chained_entries(*img_, image_->functions);
    const auto [code, extent] = code_at(f_.callee);
    starts_.reserve(image_->functions.size());
    for (const en::gal::RuntimeFunction& r : image_->functions) starts_.push_back(r.begin);
    std::sort(starts_.begin(), starts_.end());
    log(Line("r1").word("stage", "directory").num("functions", image_->functions.size())
            .num("chained", chained_.size()));
    f_.callee_code = probe::summarise_callee(code, f_.callee, extent, probe::kOwnArguments, task, options());
    probe::mark_sections(*f_.callee_code, sections());
    const probe::CalleeCode& c = *f_.callee_code;
    log(Line("r1").word("stage", "callee").num("bytes", c.bytes).num("instructions", c.instructions)
            .num("calls", c.call_count).num("branches", c.branch_count).flag("returns_early", c.returns_early));
    for (const probe::CalleeCall& k : c.calls) {
      if (f_.callee_inner.size() >= kMaxInner) break;
      if (!k.target || (k.form != probe::CallForm::Direct && k.form != probe::CallForm::Tail)) continue;
      const auto [inner, inner_extent] = code_at(*k.target);
      probe::InnerCallee in;
      in.rva = *k.target;
      in.site = k.offset;
      in.tail = k.form == probe::CallForm::Tail;
      in.code = probe::summarise_callee(inner, *k.target, inner_extent, probe::entry_at(k), task, options());
      probe::mark_sections(in.code, sections());
      log(Line("r1").word("stage", "inner").hex("rva", in.rva).num("site", in.site).num("bytes", in.code.bytes)
              .num("calls", in.code.call_count).num("branches", in.code.branch_count));
      f_.callee_inner.push_back(std::move(in));
    }
    // WHY§240: the table the tail function reads, when the shapes are run 7's; read just before the call.
    f_.table = probe::match_table(c, f_.callee_inner, f_.registers[2]);
    log(Line("r1").word("stage", "table").flag("matched", f_.table->matched)
            .word("failed", f_.table->matched ? "none" : f_.table->failed).hex("slot", f_.table->singleton_slot)
            .num("bound", f_.table->bound).flag("direct", f_.table->target.has_value()));
    // WHY§248: where the tail function's jump is direct (run 8's), the group's function is in the image: summarised now.
    if (f_.table->matched && f_.table->target) summarise_group(*f_.table->target);
    game_sites();         // WHY§289: what the game's own sites pass as the task
    summarise_sibling();  // and the registration's sibling, for comparison
  }

  // WHY§227: every committed, readable region of the process searched for our task object's address, on T1, through
  // the crash-guarded copy, within 64 GiB (WHY§235: run 6's 8 GiB cap left 2 GiB unread) and 30 s; never a known
  // stack, nor T1's own. Since spike.7 our fake vtable's and its slots' addresses too. Ours counted apart: our module,
  // and the regions of the DLL's own heap (alloc_count.cpp), of our task's block and of our vtable's.
  void search_memory() {
    probe::MemorySearchSpec spec;
    spec.target = s_.sys->task_object();
    spec.object_bytes = std::uint64_t{8} + f_.object_size;
    spec.vtable = vtable_;
    spec.slots = thunks_;
    spec.group_index = f_.group_index;
    spec.game_base = base_;
    spec.own_base = en::gal::sys::boot::address_of_module(s_.module);
    std::array<std::uint64_t, 1024> bases{};
    const std::optional<std::size_t> n = own_heap_bases(bases);
    spec.own_known = n.has_value();
    if (n) spec.own_bases.assign(bases.begin(), bases.begin() + static_cast<std::ptrdiff_t>(*n));
    spec.own_bases.push_back(spec.target);  // our task's block is an allocation of its own (VirtualAlloc's)
    if (vtable_ != 0) spec.own_bases.push_back(vtable_);  // and so is our vtable
    if (f_.request) spec.request_vtable = base_ + f_.request->vtable;  // WHY§293: the game's requests
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    spec.skip.push_back(en::gal::AddressRange{low, high});
    const std::uint64_t at = since_ms() - called_ms_;
    probe::MemorySearch m =
        probe::search_memory(*s_.sys, [this](std::uint64_t a) { return s_.sys->region_at(a); }, spec);
    m.at_ms = at;
    log(Line("r1").word("stage", "memory_search").num("at_ms", m.at_ms).num("ms", m.ms).num("read_mb", m.read_bytes >> 20)
            .num("committed_mb", m.committed_bytes >> 20).num("found", m.found).num("inside", m.inside)
            .num("own_found", m.own_found).num("vtable_found", m.vtable_found).num("slot_found", m.slot_found)
            .flag("own_known", m.own_known).num("request_found", m.request_found)
            .word("stopped", m.stopped == probe::SearchStop::Done    ? "done"
                             : m.stopped == probe::SearchStop::Bytes ? "bytes_cap"
                                                                     : "time_cap"));
    f_.request_search.push_back(m.request_searched ? request_search_of(m) : probe::RequestSearch{});
    f_.memory_search.push_back(std::move(m));
    classes();
  }

  // How much of our object the hub wrote, read on T1 from our own block when the task has not run (its body reads it
  // otherwise).
  void object_from_t1() {
    const std::uint64_t at = s_.sys->task_object();
    if (at == 0 || f_.object_size == 0) return;
    std::vector<std::byte> obj(std::min<std::uint32_t>(f_.object_size, en::gal::kMaxCopyBytes));
    if (!s_.sys->copier().copy(at + 8, obj).copied) return;
    std::uint32_t last = 0, nonzero = 0;
    for (std::size_t i = 0; i < obj.size(); ++i) {
      if (obj[i] != std::byte{0}) {
        last = static_cast<std::uint32_t>(i + 1);
        ++nonzero;
      }
    }
    f_.object_written = last;
    f_.object_nonzero = nonzero;
  }

  void report_loop(R1Body* body) {
    bool confirmed = false;
    called_mono_ = mono_ms() - (since_ms() - called_ms_);
    std::size_t searches = 0;
    for (;;) {
      if (body != nullptr && searches < 2 && since_ms() - called_ms_ >= (searches == 0 ? 2000u : 20000u)) {
        if (searches == 0) {
          read_table(false);
          read_views();
          if (f_.method) f_.method->after = read_method(since_ms() - called_ms_);
          read_g_fields(since_ms() - called_ms_);
          read_queue(since_ms() - called_ms_);
        }
        search_hub();
        search_memory();
        if (body->invocations() == 0) object_from_t1();
        ++searches;
      }
      if (body != nullptr) {
        update(*body);
        if (!confirmed && body->invocations() > 0) {
          confirmed = true;
          block::ready_ms(static_cast<std::uint32_t>(since_ms()));
          enter_state(w::InitState::Ready);  // "task confirmed": the launcher's console shows it
          log(Line("r1").word("stage", "task_confirmed").num("ms", since_ms() - called_ms_));
        }
      }
      f_.elapsed_ms = since_ms();
      write_report();
      Sleep(2000);
    }
  }

  // Within the report's 256 KiB: past it, the summaries' forms are left out (each marked forms_dropped), pass by pass:
  // the sibling's first; then the minor summaries' (the wrapper's callees that are not its tail jump, the group
  // function's other callees, G's method's callees that are not the function it calls with the request, and the level
  // past the request's methods); then the tail function's, the wrapper's and the group's own function's. Past that the
  // summaries are trimmed (`trimmed`: their call sites, branches and guards left out, their reads and stores kept): the
  // sibling's and the minor ones; then the sibling's callees are left out, then the minor ones past the first four
  // (`inner_dropped`), then all of them and the sibling. WHY§293: G's method's, the request's slots' and the function
  // G's method calls with the request keep their forms longest (pass 9), and last the group's own function, G's method
  // and those are trimmed. The game's requests' tasks' words go at pass 4. Each pass keeps the earlier ones' cuts.
  void write_report() {
    auto text = probe::write_report(f_);
    constexpr int kPasses = 11;
    for (int pass = 0; !text && pass < kPasses && f_.callee_code; ++pass) {
      probe::Findings g = f_;
      cut(g, pass);
      text = probe::write_report(g);
      if (text) log(Line("r1").word("report", "forms_dropped").num("pass", pass), LogLevel::Warn);
    }
    if (!text) {
      log(Line("r1").word("report", "refused").word("where", "probe_report"), LogLevel::Error);
      return;
    }
    (void)write_text(folder_ + L"\\r1-probe.json", *text);
  }
  // The cuts of every pass up to `pass` (write_report's order).
  void cut(probe::Findings& g, int pass) const {
    const auto drop_forms = [](probe::CalleeCode& c) {
      if (c.forms.empty()) return;
      c.forms.clear();
      c.forms_dropped = true;
    };
    const auto is_method = [&](const probe::InnerCallee& in) {
      return in.depth == 3 && f_.method && (in.rva == f_.method->rva || (f_.method->stub && in.rva == *f_.method->stub));
    };
    // WHY§293: G's method, the request's slots and the function G's method calls with the request.
    const auto precious = [&](const probe::InnerCallee& in) {
      return is_method(in) || in.request_slot.has_value() ||
             (in.depth == 4 && f_.request && f_.request->now == std::optional<std::uint32_t>(in.rva));
    };
    // Each summary's rank: when its forms go, and whether it is among those trimmed and left out first.
    const auto rank = [&](const probe::InnerCallee& in) {
      if (precious(in)) return 9;
      if (in.depth == 1) return in.tail ? 2 : 1;
      if (in.depth == 2) return 4;
      return 1;  // the group function's other callees, G's method's other callees, the level past the request's
    };
    if (g.sibling) {
      drop_forms(g.sibling->code);
      for (probe::InnerCallee& in : g.sibling->inner) {
        drop_forms(in.code);
        if (pass >= 5) trim(in.code);
      }
      if (pass >= 5) trim(g.sibling->code);
      if (pass >= 6 && !g.sibling->inner.empty()) {
        g.sibling->inner_dropped = static_cast<std::uint32_t>(g.sibling->inner.size());
        g.sibling->inner.clear();
      }
      if (pass >= 8) g.sibling.reset();
    }
    if (pass >= 4) {
      for (probe::RequestSearch& rs : g.request_search) {
        for (probe::GameTask& t : rs.tasks) t.words.clear();
      }
    }
    for (probe::InnerCallee& in : g.callee_inner) {
      const int r = rank(in);
      if (pass >= r) drop_forms(in.code);
      const bool minor = in.depth >= 3 && !precious(in);
      if ((minor && pass >= 5) || ((in.depth == 2 || precious(in)) && pass >= 10)) trim(in.code);
    }
    if (pass >= 3) drop_forms(*g.callee_code);
    if (pass >= 7) {
      std::size_t kept = 0;
      const std::size_t keep = pass == 7 ? 4 : 0;
      const auto before = g.callee_inner.size();
      std::erase_if(g.callee_inner, [&](const probe::InnerCallee& in) {
        const bool minor = in.depth >= 3 && !precious(in);
        return minor && kept++ >= keep;
      });
      g.inner_dropped = static_cast<std::uint32_t>(before - g.callee_inner.size());
    }
  }
  // A summary's call sites, branches and guards left out (their counts kept), its reads and stores kept.
  static void trim(probe::CalleeCode& c) {
    if (c.calls.empty() && c.branches.empty() && c.guards.empty()) return;
    c.calls.clear();
    c.branches.clear();
    c.guards.clear();
    c.trimmed = true;
  }

  Shell& s_;
  std::wstring folder_;
  probe::Findings f_;
  derive::Leads leads_;
  std::unique_ptr<en::gal::ImageInfo> image_;
  std::unique_ptr<en::gal::ImageBytes> img_;
  std::unique_ptr<en::gal::rtti::Walk> walk_;
  std::vector<std::unique_ptr<en::gal::VtableSet>> sets_;
  std::vector<std::string> static_labels_;
  derive::Result result_;
  std::map<std::string, std::uint32_t> slots_;  // the statics' slots R1 has found, by label
  std::uint64_t base_ = 0;
  std::uint64_t instance_ = 0;
  std::uint64_t hub_static_ = 0;
  en::gal::RegisterAbi abi_;
  std::uint64_t called_ms_ = 0;
  std::uint64_t called_mono_ = 0;
  // A walk's root (WHY§248: the hub's instance, the tail function's singleton, the group's entry) and what is kept of
  // it: the walk just before the call (WHY§235), the same walk at the Mark 2 s before it (WHY§240), the control diff's
  // changes for each later diff's in_control, and the first diff's for the second's steadiness.
  struct Root {
    std::string_view word;
    std::uint64_t address = 0;
    probe::HubWalk before;
    probe::HubWalk control;
    std::vector<probe::ChangedWord> control_changes;
    std::vector<probe::ChangedWord> first_changes;
    std::uint64_t control_ms = 0;
    std::uint64_t kept_ms = 0;
    bool controlled = false;
    std::size_t diffs = 0;
  };
  std::array<Root, 5> roots_ = make_roots();
  static std::array<Root, 5> make_roots() {
    std::array<Root, 5> r;
    r[0].word = "task_hub";
    r[1].word = "singleton";
    r[2].word = "group_entry";
    r[3].word = "g_singleton";  // WHY§289
    r[4].word = "g_queue";      // WHY§293
    return r;
  }
  // WHY§289: one of the array's views, as the walk before the call kept it.
  struct View {
    std::uint64_t address = 0;
    std::vector<std::byte> bytes;
  };
  std::vector<View> views_;
  std::vector<std::uint32_t> starts_;             // the exception directory's functions' starts, ascending (WHY§289)
  std::vector<probe::ChainedEntry> chained_;      // its chained entries, by primary (WHY§293)
  std::vector<std::pair<std::int64_t, std::uint8_t>> g_fields_;  // G's fields its method reads (WHY§293)
  std::uint64_t g_addr_ = 0;                      // G, as its slot held it at the Mark
  std::uint64_t queue_addr_ = 0;                  // G's queue, as G held it at the Mark
  DWORD main_thread_ = 0;                         // the game's main window's thread, read just before the call
  bool group_done_ = false;                       // WHY§248: the group's function and its callees summarised
  probe::EntryState further_entry_{};             // WHY§293: the entry of the summary a stub led to
  std::uint64_t vtable_ = 0;                      // our fake vtable's address, read back after the call
  std::vector<std::uint64_t> thunks_;             // its slots'
};

}  // namespace

void run_r1(Shell& s, const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>* gated) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto* round = new Round(s);  // lives for the process: T1 stays its reporter
    round->gated_ = gated;
    round->run(*gated);
  });
}

}  // namespace eldennet

#else

namespace eldennet {
void run_r1(Shell&, const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>*) noexcept {}
}  // namespace eldennet

#endif
