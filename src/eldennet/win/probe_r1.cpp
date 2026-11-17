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
//    that was not reviewed (O89 (a)).
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
// Every file is numbers and the leads' labels only (O75); the reviewing agent turns them into gal/ data.
#include "dll.hpp"

#if EN_PROBE

#include <algorithm>
#include <map>
#include <set>

#include "en/gal/derive.hpp"
#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/probe.hpp"
#include "en/gal/rtti.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/probe_report.hpp"

namespace eldennet {

void arm_allocation_watch(bool armed) noexcept;
std::uint64_t allocations_watched() noexcept;

namespace {

namespace w = en::proto::watch;
namespace derive = en::gal::derive;
namespace probe = en::gal::probe;
using en::shell::CheckStatus;
using en::shell::Line;
using en::shell::LogLevel;
using en::shell::ProbeCheck;

constexpr std::uint32_t kSpanObject = 0x400;  // how much of our object the probe copies back

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

    // ---- 2. The checks ----
    const bool ready = run_checks();
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
    log(Line("r1").word("register", f_.register_status).hex("callee", f_.callee).num("group", f_.group_index)
            .num("slots", f_.vtable_slots).num("object_size", f_.object_size));
    write_report();
    if (!called) {
      push_banner(s_, en::core::BannerReason::RegisterFaulted, en::gal::to_string(called.error()));
      block::state(w::InitState::Banner);
      return report_loop(nullptr);
    }
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
    for (ProbeCheck& c : f_.checks) {
      if (c.item == item && c.label == label) {
        c = ProbeCheck{item, std::move(label), st, value, expected};
        return;
      }
    }
    f_.checks.push_back(ProbeCheck{item, std::move(label), st, value, expected});
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
    result_ = derive::derive(*image_, s_.sys->image(), leads_, options);
    ++f_.derive_passes;
    f_.derive_ms = since_ms() - t;
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
    // The name layout: read by at least half the class objects.
    const bool names = fd.names.has_value() && fd.names_of > 0 && fd.names_agree * 2 >= fd.names_of;
    check("name_layout", "", names ? CheckStatus::Confirmed : CheckStatus::Missing,
          fd.names ? std::optional<std::uint64_t>(fd.names->offset) : std::nullopt);
    ok = ok && names;
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
      f_.bases.push_back(en::shell::ProbeBase{order[i].first, order[i].second, static_cast<std::uint32_t>(t.vtables.size()),
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
    f_.object_written = body.written();
    f_.object_nonzero = body.nonzero();
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
        f_.rising.push_back(en::shell::ProbeRising{h.slot, h.offset, h.rose, h.still, h.other});
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
      en::shell::ProbeStatic out{static_labels_[i], ss.candidates(), ss.checked(), {}};
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

  void report_loop(R1Body* body) {
    bool confirmed = false;
    called_mono_ = mono_ms() - (since_ms() - called_ms_);
    for (;;) {
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

  void write_report() {
    const auto text = en::shell::write_probe_report(f_);
    if (!text) {
      log(Line("r1").word("report", "refused").word("where", "probe_report"), LogLevel::Error);
      return;
    }
    (void)write_text(folder_ + L"\\r1-probe.json", *text);
  }

  Shell& s_;
  std::wstring folder_;
  en::shell::ProbeFindings f_;
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
