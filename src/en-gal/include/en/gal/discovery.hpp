#pragma once
// Discovery in game (ARCH §3.2's Gate and Discover, §6.2): over the main module's mapped image, as the SysLayer shows it
// (guarded copies in game, a byte buffer in the tests), driven by gal/'s files and never by names in code:
// - the gate: the image's fingerprint against builds.toml (R7: a Layout only for the build it matched);
// - the AOB items of patterns.toml (register_task and the statics, by their items' labels) in one multi-pattern pass;
//   a static the probe round gives by its slot (the derivation's `--static label=rva`, R1) needs no pattern;
// - the FD4 singletons: patterns.toml's `fd4_idiom` and its path suffix, each site named through the runtime class
//   object where builds.toml says the name layout is confirmed (`dlrf_name_layout`), else by calling the getter once
//   per candidate under SEH (ARCH §6.2 step 3, the init-only fallback), and matched to the layout's classes by their
//   `singleton` names; a name with two statics is ambiguous and its item missing;
// - the RTTI walk: for each layout class with `rtti`, the vtables of the class and of every class deriving from it.
// A pass resolves what it can; required items still missing are retried at +5, +15 and +30 s (ARCH §3.2; DESIGN §3.5).
// An optional item missing turns its group off. Everything here runs on T1, before registration, over the image only.
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/health.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/roles.hpp"
#include "en/gal/schema.hpp"
#include "en/gal/sys.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/snapshot.hpp"

namespace en::gal {

// A class's vtables, and its subclasses' (absolute addresses), sorted for a binary search on the game thread.
class VtableSet {
 public:
  void assign(std::vector<std::uint64_t> vtables);
  [[nodiscard]] bool contains(std::uint64_t vtable) const noexcept;
  [[nodiscard]] bool empty() const noexcept { return v_.empty(); }
  [[nodiscard]] std::span<const std::uint64_t> all() const noexcept { return v_; }

 private:
  std::vector<std::uint64_t> v_;
};

// The gate's outcome (ARCH §3.2's Gate): the mapped image read, and the build it matched.
struct Gated {
  ImageInfo image;
  BuildProfile profile;
};

struct GateFailure {
  enum class What : std::uint8_t { Image, BuildsFile, Gate } what = What::Image;
  std::string text;  // what was wrong, worded for the log: never a value of the files
};

// Reads the mapped image's headers through the SysLayer and gates them on builds.toml's text.
[[nodiscard]] proto::Expected<Gated, GateFailure> gate_image(const SysLayer& sys, std::string_view builds_text);

struct DiscoveryOptions {
  LayoutMode mode = LayoutMode::Release;  // a dev build accepts a layout's leads, with a banner
  // The probe round's slots of statics (R1; the derivation's --static label=rva): such a static needs no pattern.
  std::vector<std::pair<std::string, std::uint32_t>> static_slots;
};

enum class ItemStatus : std::uint8_t {
  Resolved,
  Missing,    // no pattern match, no FD4 site of its name, or no pattern at all
  Ambiguous,  // two statics behind one name, or hits that disagree
  Refused,    // found, but a rule refused it (placement, function bounds, a slot outside the data section)
};
[[nodiscard]] const char* to_string(ItemStatus s) noexcept;

struct ItemResult {
  ItemStatus status = ItemStatus::Missing;
  std::uint64_t address = 0;  // the static's slot, or the function's entry: absolute
  std::uint32_t rva = 0;
  std::uint32_t sites = 0;  // an FD4 singleton's sites
  bool from_slot = false;   // given by the probe round's slot, not found by a pattern
  const char* why = "";     // a fixed text
};

struct DiscoveryReport {
  std::array<ItemResult, roles::kItemCount> items{};
  std::array<GroupWhy, proto::kGroupCount> groups{};  // None where the group is available
  std::array<const char*, proto::kGroupCount> group_detail{};  // which class, field or item it lacks
  std::uint64_t pattern_hits = 0;
  std::uint64_t unscanned = 0;
  std::uint64_t fd4_hits = 0;
  std::uint64_t fd4_accepted = 0;
  std::uint64_t fd4_names = 0;     // distinct names read
  std::uint64_t getter_calls = 0;  // the fallback's calls (ARCH §6.2 step 3)
  std::uint64_t rtti_locators = 0;
  std::uint64_t rtti_vtables = 0;
  std::uint32_t rtti_classes = 0;   // layout classes whose RTTI resolved
  std::uint32_t rtti_missing = 0;   // layout classes whose RTTI did not
  std::uint64_t image_unreadable = 0;
  std::uint64_t pass_us = 0;
  bool layout_has_leads = false;
  bool ready = false;          // every required item, the boot fields and the structural groups are there
  const char* failure = "";    // why not ready, a fixed text
};

class Discovery {
 public:
  Discovery(Discovery&&) noexcept = default;
  Discovery& operator=(Discovery&&) noexcept = default;
  Discovery(const Discovery&) = delete;
  Discovery& operator=(const Discovery&) = delete;
  ~Discovery() = default;

  [[nodiscard]] const ImageInfo& image() const noexcept { return image_; }
  [[nodiscard]] const BuildProfile& profile() const noexcept { return profile_; }
  [[nodiscard]] const Layout& layout() const noexcept { return *layout_; }
  [[nodiscard]] const roles::Bound& bound() const noexcept { return *bound_; }
  [[nodiscard]] std::uint64_t module_base() const noexcept { return base_; }
  [[nodiscard]] const DiscoveryReport& report() const noexcept { return report_; }
  [[nodiscard]] bool ready() const noexcept { return report_.ready; }
  // An item's static slot or entry (absolute), when resolved.
  [[nodiscard]] std::optional<std::uint64_t> address(roles::Item item) const noexcept;
  // A class's vtable set; empty where the class declares no RTTI (no check).
  [[nodiscard]] const VtableSet& vtables(roles::Cls c) const noexcept { return sets_[static_cast<std::size_t>(c)]; }
  // The groups discovery found everything for.
  [[nodiscard]] proto::GroupSet groups() const noexcept;
  [[nodiscard]] bool has(proto::Group g) const noexcept {
    return report_.groups[static_cast<std::size_t>(g)] == GroupWhy::None;
  }

 private:
  Discovery(ImageInfo image, BuildProfile profile, std::unique_ptr<Layout> layout);
  friend proto::Expected<Discovery, SchemaErrors> discover(SysLayer& sys, const Gated& gated,
                                                           std::string_view patterns_text,
                                                           std::string_view layout_text,
                                                           const DiscoveryOptions& options);

  ImageInfo image_;
  BuildProfile profile_;
  std::unique_ptr<Layout> layout_;
  std::unique_ptr<roles::Bound> bound_;
  std::uint64_t base_ = 0;
  std::array<VtableSet, roles::kClsCount> sets_;
  DiscoveryReport report_;
};

// One pass of discovery. A refused patterns.toml or layout is an error (permanent: nothing is retried); otherwise the
// Discovery says what it found, and whether that is enough to register (ready()).
[[nodiscard]] proto::Expected<Discovery, SchemaErrors> discover(SysLayer& sys, const Gated& gated,
                                                                std::string_view patterns_text,
                                                                std::string_view layout_text,
                                                                const DiscoveryOptions& options = {});

// ARCH §3.2's retries: a pass at once, then at +5, +15 and +30 s while a required item is missing.
inline constexpr std::array<std::uint64_t, 4> kRetryAtMs = {0, 5'000, 15'000, 30'000};

struct Retried {
  proto::Expected<Discovery, SchemaErrors> last;
  std::uint32_t passes = 0;
};
// Runs the passes, calling `wait_until_ms(t)` (relative to the first pass's start) before each later one.
[[nodiscard]] Retried discover_with_retries(SysLayer& sys, const Gated& gated, std::string_view patterns_text,
                                            std::string_view layout_text, const DiscoveryOptions& options,
                                            const std::function<void(std::uint64_t)>& wait_until_ms);

}  // namespace en::gal
