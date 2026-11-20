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
//    task's stores. At the Mark the hub's reachable memory (spike.5's walk) is walked, again 2 s later and kept on our
//    heap, and the two compared (the control diff), since spike.9 from the table's singleton and the group's entry too;
//    then, at once, the table is read (with the singleton's and the entry's RTTI classes against the leads) and the call
//    made.
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
//    At 2 s the table is read again (WHY§240).
// Every file is numbers and the leads' labels only (O75); the reviewing agent turns them into gal/ data.
#include "dll.hpp"

#if EN_PROBE

#include <algorithm>
#include <array>
#include <cstring>
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
constexpr std::size_t kMaxGroup = 16;         // the group's function and its callees summarised (WHY§248)

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
    table_roots();                  // WHY§248: the singleton and the group's entry, two more roots for the walk
    control_walk();                 // WHY§240: the hub at the Mark, and 2 s later
    (void)s_.sys->rescan_stacks();
    keep_hub();
    read_table(true);
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

  R1Body* prepare() {
    img_ = std::make_unique<en::gal::ImageBytes>(*image_, s_.sys->image());
    walk_ = std::make_unique<en::gal::rtti::Walk>(en::gal::rtti::walk(*img_));
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
  void table_roots() {
    roots_[0].address = instance_;
    if (!f_.table || !f_.table->matched) return;
    probe::TableAddresses at;
    (void)probe::read_table(*f_.table, table_spec(), block_reader(), &at);
    roots_[1].address = at.singleton;
    roots_[2].address = at.entry;
    log(Line("r1").word("stage", "roots").flag("singleton", at.singleton != 0).flag("group_entry", at.entry != 0));
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
    const std::vector<probe::CalleeCall> calls = group.front().code.calls;
    for (const probe::CalleeCall& k : calls) {
      if (group.size() >= kMaxGroup) break;
      if (!k.target || (k.form != probe::CallForm::Direct && k.form != probe::CallForm::Tail) ||
          !seen.insert(*k.target).second) {
        continue;
      }
      group.push_back(inner_at(*k.target, k, probe::entry_at(k, entry), 3, rva, k.form == probe::CallForm::Tail));
    }
    for (probe::InnerCallee& in : group) f_.callee_inner.push_back(std::move(in));
  }
  probe::InnerCallee inner_at(std::uint32_t rva, const probe::CalleeCall& site, const probe::EntryState& entry,
                              std::uint32_t depth, std::uint32_t parent, bool tail) const {
    const auto [code, extent] = code_at(rva);
    probe::InnerCallee in;
    in.rva = rva;
    in.site = site.offset;
    in.tail = tail;
    in.code = probe::summarise_callee(code, rva, extent, entry, f_.registers[0]);
    in.depth = depth;
    in.parent = parent;
    for (const probe::PassedSource& p : site.pass_src) {
      probe::PassedSource e = p;
      const std::size_t i = p.reg == 1 ? 0 : p.reg == 2 ? 1 : p.reg == 8 ? 2 : 3;
      e.holds_entry = entry[i].entry;
      in.entry_src.push_back(std::move(e));
    }
    probe::mark_sections(in.code, sections());
    log(Line("r1").word("stage", "inner").hex("rva", in.rva).num("site", in.site).num("depth", depth)
            .num("bytes", in.code.bytes).num("calls", in.code.call_count).num("task_reads", in.code.task_read_count)
            .num("entry_reads", in.code.entry_read_count).num("task_stores", in.code.task_store_count));
    return in;
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
  // extent), or up to the 4 KiB bound, never past its section.
  std::pair<std::vector<std::byte>, std::uint32_t> code_at(std::uint32_t rva) const {
    std::uint32_t extent = 0;
    const auto fn = std::lower_bound(image_->functions.begin(), image_->functions.end(), rva,
                                     [](const en::gal::RuntimeFunction& r, std::uint32_t at) { return r.begin < at; });
    if (fn != image_->functions.end() && fn->begin == rva) extent = fn->length();
    std::uint32_t len = extent != 0 ? std::min(extent, probe::kCalleeBound) : probe::kCalleeBound;
    for (const en::gal::Section& sec : image_->sections) {
      if (sec.contains(rva)) len = std::min(len, sec.rva + sec.mapped_size() - rva);
    }
    std::vector<std::byte> code(len);
    if (!s_.sys->image().read(rva, code)) code.clear();
    return {std::move(code), extent};
  }

  // WHY§227: what the callee does with its arguments, from the live image (the code the call runs), decoded within its
  // function of the exception directory, or up to its first return, and at most 4 KiB; no call is followed. WHY§235:
  // then each callee it calls directly or tail-jumps to, one level deeper, with the arguments its call site passes.
  // WHY§248: our task's argument (the registers' first) is followed in every summary: its loads and their uses.
  void summarise_callee() {
    if (f_.callee == 0) return;
    const std::uint8_t task = f_.registers[0];
    const auto [code, extent] = code_at(f_.callee);
    f_.callee_code = probe::summarise_callee(code, f_.callee, extent, probe::kOwnArguments, task);
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
      in.code = probe::summarise_callee(inner, *k.target, inner_extent, probe::entry_at(k), task);
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
            .flag("own_known", m.own_known)
            .word("stopped", m.stopped == probe::SearchStop::Done    ? "done"
                             : m.stopped == probe::SearchStop::Bytes ? "bytes_cap"
                                                                     : "time_cap"));
    f_.memory_search.push_back(std::move(m));
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
        if (searches == 0) read_table(false);
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
  // first the wrapper's callees that are not its tail jump and (spike.9) the group function's callees, then the tail
  // function's, then the wrapper's, and last the group's own function's.
  void write_report() {
    auto text = probe::write_report(f_);
    for (int pass = 0; !text && pass < 4 && f_.callee_code; ++pass) {
      probe::Findings g = f_;
      for (probe::InnerCallee& in : g.callee_inner) {
        const int rank = in.depth == 2 ? 3 : in.depth == 1 && in.tail ? 1 : 0;
        if (rank <= pass && !in.code.forms.empty()) {
          in.code.forms.clear();
          in.code.forms_dropped = true;
        }
      }
      if (pass >= 2 && !g.callee_code->forms.empty()) {
        g.callee_code->forms.clear();
        g.callee_code->forms_dropped = true;
      }
      text = probe::write_report(g);
      if (text) log(Line("r1").word("report", "forms_dropped").num("pass", pass), LogLevel::Warn);
    }
    if (!text) {
      log(Line("r1").word("report", "refused").word("where", "probe_report"), LogLevel::Error);
      return;
    }
    (void)write_text(folder_ + L"\\r1-probe.json", *text);
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
  std::array<Root, 3> roots_ = make_roots();
  static std::array<Root, 3> make_roots() {
    std::array<Root, 3> r;
    r[0].word = "task_hub";
    r[1].word = "singleton";
    r[2].word = "group_entry";
    return r;
  }
  bool group_done_ = false;                       // WHY§248: the group's function and its callees summarised
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
