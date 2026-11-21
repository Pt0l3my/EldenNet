#pragma once
// ErBackend (ARCH §2.2, §6.6 to §6.9, §7.1): the GameBackend that reads the game through GuardedReader, by the layout
// and the items discovery found, on the game thread only. Each snapshot re-reads every static and compares each
// instance's vtable (R5), walks the characters near the player, and reads each live group, with its per-tick
// plausibility checks: a hard fault counts toward the group's breaker (three in 60 s trip it, R8), and a structural
// group that trips makes the backend inert (L1). Phase A's self-tests run on the first ticks after registration, phase
// B's at the start of each playthrough (ARCH §6.7). While loading only the lifecycle subset is read (R11); at the title,
// or with the session not solo, only the session and the lifecycle subset (Dormant). Everything it needs is allocated
// when it is made: nothing on the game thread allocates or frees (R6), and the watch lists the core sends are handed
// back, never freed here (C5).
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "en/gal/discovery.hpp"
#include "en/gal/guarded.hpp"
#include "en/gal/health.hpp"
#include "en/gal/roles.hpp"
#include "en/gal/sys.hpp"
#include "en/proto/backend.hpp"
#include "en/proto/lifecycle.hpp"

namespace en::gal {

// ARCH §6.9's flag formats and ARCH §6.7's thresholds: the design's own numbers.
inline constexpr std::uint32_t kFlagDivisor = 1000;
inline constexpr std::uint32_t kFlagBlockBytes = 125;
inline constexpr std::uint32_t kFlagKnownSet = 100;  // phase B: this flag is set in every playthrough
inline constexpr std::uint32_t kFlagTreeDepth = 64;
inline constexpr std::uint32_t kMinTextTables = 20;
inline constexpr std::uint32_t kWalkEntries = 48;
inline constexpr std::uint32_t kPhaseATicks = 8;

struct BackendOptions {
  ReadBudget budget;
  std::uint32_t keep = 32;            // characters kept, nearest first (DESIGN §5.1)
  float keep_within_m = 40.0f;
  std::uint32_t throttled_keep = 16;  // while throttled (R10)
  float throttled_within_m = 25.0f;
  std::uint32_t text_per_tick = 16;   // new look-ups per snapshot (ARCH §7.2)
  std::uint32_t params_per_tick = 32;
};

// How much a snapshot reads (ARCH §6.8, §7.2).
enum class ReadMode : std::uint8_t {
  Full,      // in the world, solo: every live group
  Dormant,   // the title, or a session that is not solo: the session and the lifecycle subset
  Loading,   // between worlds: the lifecycle subset only (R11)
};
[[nodiscard]] const char* to_string(ReadMode m) noexcept;

// What the first tick found at the layout's group index (ARCH §3.2's FirstTick).
struct GroupCheck {
  bool ok = false;
  bool readable = false;                 // the hub's table could be read
  std::optional<std::uint32_t> found_at; // where the layout's name is, if elsewhere (the probe records it)
};

// The outcome of a tick of phase A.
enum class PhaseA : std::uint8_t { Running, Passed, Failed };

class ErBackend final : public proto::GameBackend {
 public:
  ErBackend(SysLayer& sys, const Discovery& discovery, const BackendOptions& options = {});
  ErBackend(const ErBackend&) = delete;
  ErBackend& operator=(const ErBackend&) = delete;
  ~ErBackend() override;

  // ---- GameBackend ----
  proto::Capabilities probe(const proto::GameThread& gt) override;
  proto::Expected<void, proto::ReadError> snapshot(const proto::GameThread& gt, proto::WorldSnapshot& out) override;
  proto::Expected<void, proto::ReadError> render_feed(const proto::GameThread& gt, std::span<const proto::ChrHandle> want,
                                                      proto::RenderFeed& out) override;
  proto::Expected<void, proto::ApplyError> apply(const proto::GameThread& gt, const proto::GameCommand& cmd) override;

  // ---- The task's controls ----
  // The first tick: the game thread's stack joins the stack table, reads are bound to this thread, and the group at the
  // layout's index must carry the layout's name.
  GroupCheck first_tick(const proto::GameThread& gt);
  // One tick of phase A (at most kPhaseATicks): each untested group's test. Failed means a structural group failed.
  PhaseA phase_a(const proto::GameThread& gt);
  // Whether a structural group tripped: the task goes inert and reads nothing more (L1).
  [[nodiscard]] bool inert() const noexcept { return inert_; }
  void make_inert() noexcept { inert_ = true; }
  void set_throttled(bool throttled) noexcept { throttled_ = throttled; }
  void set_flag_frame_due(bool due) noexcept { flag_frame_due_ = due; }
  [[nodiscard]] ReadMode last_mode() const noexcept { return mode_; }
  [[nodiscard]] const GroupBook& book() const noexcept { return book_; }
  [[nodiscard]] GuardedReader& reader() noexcept { return reader_; }
  [[nodiscard]] const GuardedReader& reader() const noexcept { return reader_; }
  [[nodiscard]] const Discovery& discovery() const noexcept { return d_; }
  // Lifecycle messages the reads gave rise to (a group tripped, a playthrough started), in order; at most 16 wait.
  bool pop_lifecycle(proto::Lifecycle& out) noexcept;
  // A watch list replaced by a newer one, for the core to take back (C5); null when none waits.
  std::shared_ptr<const std::vector<std::uint32_t>> take_retired() noexcept;
  // Soft skips (list entries dropped) since the start, by group.
  [[nodiscard]] std::uint64_t soft_skips(proto::Group g) const noexcept { return soft_[static_cast<std::size_t>(g)]; }
  [[nodiscard]] std::uint64_t vtable_skips() const noexcept { return vtable_skips_; }
  // The characters the core tracks (SetTracked), for the render feed.
  [[nodiscard]] std::span<const proto::ChrHandle> tracked() const noexcept { return {tracked_.data(), tracked_count_}; }

 private:
  class Scratch;

  [[nodiscard]] std::uint64_t now_ms() const noexcept;
  void fault(proto::Group g) noexcept;
  void soft(proto::Group g) noexcept;
  // R5: an item's instance, re-read from its static this tick; 0 when null or unresolved.
  [[nodiscard]] std::uint64_t instance(roles::Item item) const noexcept { return inst_[roles::index(item)]; }
  void read_statics(const proto::GameThread& gt);
  // Copies an object of class `c` at `address` into the scratch slot `slot` and checks its vtable against the class's
  // set: nothing on a fault (counted against `g`) or a mismatch (`mismatch` set).
  // `hard` false: a fault is not the group's (the render feed's and the first tick's reads).
  std::optional<ObjView> object(const proto::GameThread& gt, proto::Group g, roles::Cls c, std::uint64_t address,
                                std::size_t slot, bool* mismatch = nullptr, bool hard = true);
  // A singleton's or static's instance, re-read from its static and copied once a tick, and checked (R5's vtable
  // compare, with its three-tick rule).
  std::optional<ObjView> item_object(const proto::GameThread& gt, proto::Group g, roles::Item item, bool hard = true);

  void read_lifecycle(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_session(const proto::GameThread& gt, proto::WorldSnapshot& out);
  bool read_player(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_world(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_ui(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_place(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_camera(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_flags(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void read_requests(const proto::GameThread& gt, proto::WorldSnapshot& out);
  void phase_b(const proto::GameThread& gt, const proto::WorldSnapshot& out);
  void playthrough_ready(const proto::GameThread& gt, const proto::WorldSnapshot& out);

  // A character's parts: vitals and motion (world), reaction and grip (combat). Fills `chr`; false on a world fault.
  bool read_parts(const proto::GameThread& gt, const ObjView& chr_obj, proto::Meter& hp, std::array<float, 3>& pos,
                  std::int32_t* recv_dmg, std::uint8_t* throw_state, proto::Meter* fp, proto::Meter* stamina);
  // Text and params (readers.cpp).
  bool text_lookup(const proto::GameThread& gt, std::uint32_t table, std::uint32_t id, proto::TextEntry& out);
  bool param_lookup(const proto::GameThread& gt, proto::ParamKind kind, std::uint32_t id, proto::ParamRowInfo& out,
                    bool* found);
  std::optional<std::uint64_t> flag_block(const proto::GameThread& gt, const ObjView& flags, std::uint32_t group);
  PhaseA text_phase_a(const proto::GameThread& gt);
  PhaseA params_phase_a(const proto::GameThread& gt);
  bool text_phase_b(const proto::GameThread& gt);
  void push_lifecycle(const proto::Lifecycle& l) noexcept;
  void trip(proto::Group g, GroupWhy why) noexcept;

  SysLayer& sys_;
  const Discovery& d_;
  const roles::Bound& b_;
  BackendOptions opts_;
  GuardedReader reader_;
  GroupBook book_;
  std::unique_ptr<Scratch> scratch_;
  std::array<std::uint64_t, roles::kItemCount> inst_{};
  std::array<std::uint8_t, roles::kItemCount> tick_{};     // this tick: unread, static read, object read, failed, null
  std::array<std::uint8_t, roles::kItemCount> vt_miss_{};  // consecutive vtable mismatches
  std::array<std::uint64_t, proto::kGroupCount> soft_{};
  std::uint64_t vtable_skips_ = 0;
  bool inert_ = false;
  std::array<float, 16> cam_{};  // the last snapshot's view, for the bearings and the render feed
  float cam_fov_ = 0.0f;
  float cam_aspect_ = 0.0f;
  bool cam_ok_ = false;
  bool throttled_ = false;
  bool flag_frame_due_ = false;
  ReadMode mode_ = ReadMode::Dormant;
  std::uint32_t phase_a_ticks_ = 0;
  bool phase_a_done_ = false;
  bool in_world_ = false;        // the last snapshot was in the world
  bool phase_b_pending_ = false; // the next full snapshot starts a playthrough
  std::uint64_t main_player_ = 0;
  std::optional<std::uint64_t> last_play_time_;
  std::uint64_t pending_gap_ = 0;
  // The characters of the last snapshot's walk, by handle, for the render feed (validated every frame).
  struct Seen {
    std::uint64_t handle = 0;
    std::uint64_t address = 0;
  };
  std::array<Seen, proto::kMaxSnapshotChars + 1> seen_{};
  std::size_t seen_count_ = 0;
  // The watch list and its order by group (preallocated), the retired lists, the pending look-ups.
  std::shared_ptr<const std::vector<std::uint32_t>> watch_;
  std::uint32_t watch_version_ = 0;
  std::vector<std::uint16_t> watch_order_;  // kMaxWatchFlags entries, sorted by group
  std::size_t watch_count_ = 0;
  std::array<std::shared_ptr<const std::vector<std::uint32_t>>, 4> retired_{};
  struct TextAsk {
    std::uint16_t fmg = 0;
    std::uint32_t id = 0;
  };
  struct ParamAsk {
    proto::ParamKind kind = proto::ParamKind::NpcParam;
    std::uint32_t id = 0;
  };
  std::array<TextAsk, 64> text_asks_{};
  std::size_t text_count_ = 0;
  std::array<ParamAsk, 128> param_asks_{};
  std::size_t param_count_ = 0;
  std::array<proto::ChrHandle, 8> tracked_{};
  std::size_t tracked_count_ = 0;
  std::array<proto::Lifecycle, 16> life_{};
  std::size_t life_head_ = 0;
  std::size_t life_count_ = 0;
};

}  // namespace en::gal
