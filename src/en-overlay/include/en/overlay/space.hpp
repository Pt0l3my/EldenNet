#pragma once
// EldenNet's space (PAGES §2 and §3; GraceUI's G20, D160, D169 and eldennet-changes §19, §23, §24, §27 to §29): its page
// data and settings description (generated from pages/space.toml into space_data.cpp), the topics its pages read, built
// from what EldenNet knows, and its one validating command path (GraceUI's invariant 3). Portable, on en-proto and
// nlohmann/json: no Win32 and no call to GraceUI. The Overlay (overlay.hpp) hands it the frames poll gave and publishes
// and answers what it returns; the DLL fills its facts on T6 and applies what a command changed. T6 alone uses it.
//
// Projections only (PAGES §3 item 1, SP1): every body holds EldenNet's own words and numbers, from the facts below,
// which carry no game text, flag, stage, prompt or model text, no path and no name. Absence is silent (item 2): a fact
// EldenNet does not have is left out, never shown as a placeholder. The logs stream holds the log's own lines, which
// are EldenNet's words and numbers only (en_shell's Line), scrubbed again here of anything shaped like a SteamID64
// (PV5). Every word a player reads is a draft for the owner's review (the slice's notes list them).
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/snapshot.hpp"
#include "en/proto/watch.hpp"

namespace en::overlay {

// The increment eldennet.dll registers, its release's (GraceUI D173): a page of a later one is absent from both views.
inline constexpr std::string_view kIncrement = "0.1";
inline constexpr std::string_view kSpaceId = "eldennet";

// The generated page data and settings description (space_data.cpp, tools/pages/build.py), as the protocol's JSON.
[[nodiscard]] std::string_view space_pages() noexcept;
[[nodiscard]] std::string_view settings_description() noexcept;

// What EldenNet knows, mirrored in en-overlay's own enums (it links neither en-core nor en-gal, ARCH §2.3): the DLL maps
// en-core's and en-gal's into these, a switch per enum, on T6.
enum class Level : std::uint8_t { Starting, Inactive, Observing };  // en-core's Starting, L1 and L2 (ARCH §6.8)
enum class Stop : std::uint8_t {  // en-core's BannerReason, or none
  None,
  UnknownBuild,
  AntiCheat,
  NotStarted,
  DiscoveryFailed,
  RegisterFaulted,
  Inert,
  SessionFailed,
  TaskTripped,
  SpikeRun,
};
enum class Where : std::uint8_t { Starting, Title, Loading, World, Stopped };  // en-core's phase, as the player meets it
enum class Unclean : std::uint8_t { None, Gap, SessionFailed, AntiCheat, NotSolo, Lobby, Loading };  // the gate's why
enum class Reading : std::uint8_t { None, Registered, PhaseA, Running, Inert };  // en-gal's task, or none yet

struct SpaceFacts {
  std::array<std::uint16_t, 3> version{};  // EldenNet's
  std::uint32_t graceui = 0;               // the GraceUI connected (GRACEUI_VERSION_PACK)
  std::array<std::uint16_t, 3> floor{};    // the oldest GraceUI EldenNet works with
  proto::watch::InitState init = proto::watch::InitState::None;  // T1's start
  Level level = Level::Starting;
  Stop stop = Stop::None;
  std::string_view stop_text;  // en-core's banner text for `stop`: EldenNet's own words for the player
  Where where = Where::Starting;
  bool gate_open = false;
  Unclean gate_why = Unclean::Loading;
  bool dormant = true;
  bool badge = false;  // "paused (multiplayer)"
  bool build_known = false;
  std::array<std::uint16_t, 4> build{};  // the game's file version
  bool build_supported = false;          // gal/builds.toml knows it
  Reading reading = Reading::None;
  std::uint64_t snapshots = 0;
  std::uint32_t task_p99_us = 0;
  bool have_health = false;  // a snapshot's read health below
  proto::ReadHealth health{};
  std::uint64_t tripped = 0;  // the groups whose breaker tripped this session, a bit each (proto::Group's value)
  bool watcher = false;       // T10 runs
  bool logger = false;        // T11 writes
  std::uint64_t log_dropped = 0;
  bool recording_asked = false;  // debug.record_sessions was on at the start
  bool recording = false;        // and the recorder is writing
  std::uint64_t recorded = 0;    // its lines
  bool launcher = false;          // started by EldenNet's launcher (its watch block is attached)
  bool settings_refused = false;  // settings.toml was refused at the start: the defaults are in use
  std::uint64_t session_ms = 0;   // since T1's start
};

// What happened at the space on a step, for the DLL's log: our own words only (topic and command names are the space's
// declared ids; a key is a setting's key; never a value).
struct SpaceNote {
  enum class What : std::uint8_t { Watched, Unwatched, Command };
  What what = What::Watched;
  std::string topic;   // Watched, Unwatched: the topic; Command: the command's name
  std::string key;     // Command: the setting's key or the reset's target, if any
  std::string result;  // Command: "ok" or the error code
};

class Space {
 public:
  // Reads the settings description (settings_description()); valid() is false if it fails, a defect en_overlay_tests
  // would have caught. Every value starts at its default.
  Space() : Space(settings_description()) {}
  // The same over another settings description (the tests', for setting types this slice does not have).
  explicit Space(std::string_view description);

  [[nodiscard]] bool valid() const noexcept { return valid_; }

  // ---- The settings it describes, by key, as JSON values. The DLL sets them from settings.toml as read at the start.
  // A secret's is held and published as its status alone, "set" or "" (none in this slice).
  void set_value(std::string_view key, const nlohmann::json& value);
  [[nodiscard]] const nlohmann::json& values() const noexcept { return values_; }
  [[nodiscard]] bool has_setting(std::string_view key) const;

  // ---- GraceUI's events (D169): each frame as poll gave it.
  void watched(std::string_view sub_frame);
  void unwatched(std::string_view unsub_frame);
  [[nodiscard]] bool watching(std::string_view topic) const { return watched_.count(std::string(topic)) != 0; }
  // A view's command, validated here and answered at once, exactly once: the result frame to hand to `answer`. Nothing
  // it does takes long in this slice; a later long command answers at once and publishes its progress (§23 item 4).
  // `facts` says whether settings.toml may be written (not while the file the player wrote was refused).
  [[nodiscard]] std::string command(std::string_view cmd_frame, const SpaceFacts& facts);

  // A setting a command changed, for the DLL to apply (its effect) and to write to settings.toml.
  struct Change {
    std::string key;
    nlohmann::json value;
  };
  [[nodiscard]] std::vector<Change> take_changes();
  [[nodiscard]] std::vector<SpaceNote> take_notes();

  // ---- The logs stream: one of the log's lines (its text, `<event> key=value ...`, EldenNet's own), `t_ms` since T1's
  // start. The ring holds the newest 500, whether or not anyone watches.
  void log_line(std::string_view text, std::uint64_t t_ms);

  // ---- Each wake: the frames to publish now, in order. A topic nobody watches gets none (D169); a watched state
  // topic gets its body when it first is watched and whenever it changes, no oftener than its rate (GraceUI DESIGN
  // §6.5); the watched logs stream gets its rows from the oldest held, then each new row, in batches of at most 100.
  void collect(const SpaceFacts& facts, std::uint64_t now_ms, std::vector<std::string>& frames);

  // ---- The bodies, as collect publishes them (the tests and the fixtures read them).
  [[nodiscard]] nlohmann::json body(std::string_view topic, const SpaceFacts& facts) const;
  [[nodiscard]] nlohmann::json settings_body() const;
  [[nodiscard]] nlohmann::json log_rows() const;  // every row held, oldest first

 private:
  struct Setting {
    std::string key;
    std::string type;
    nlohmann::json fallback;  // its default
    std::vector<std::string> choices;
    double min = 0;
    double max = 0;
    bool bounded = false;
    std::string third;  // a tri-state's third value
    std::string home;
    bool locked = false;
  };
  struct Topic {
    std::uint64_t rev = 0;
    std::string last;           // the body last published, as text
    std::uint64_t last_ms = 0;  // when
    bool fresh = false;         // watched since its last publish: publish now, changed or not
  };
  struct Row {
    std::uint64_t seq = 0;
    std::string time;
    std::string line;
  };

  const Setting* find(std::string_view key) const;
  std::string answer(const nlohmann::json& id, std::string_view error, std::string_view detail) const;
  std::string set(const nlohmann::json& id, const nlohmann::json& args, const SpaceFacts& facts);
  std::string reset(const nlohmann::json& id, const nlohmann::json& args, const SpaceFacts& facts);
  void change(const Setting& s, const nlohmann::json& value);
  nlohmann::json settings_description_json_;
  std::vector<Setting> settings_;
  nlohmann::json values_ = nlohmann::json::object();
  std::uint64_t settings_rev_ = 0;  // each change
  std::set<std::string> watched_;
  std::map<std::string, Topic> topics_;
  std::deque<Row> rows_;
  std::uint64_t next_seq_ = 1;
  std::uint64_t logs_sent_ = 0;    // the last row's seq the watched logs stream has had, 0 for none
  std::vector<Change> changes_;
  std::vector<SpaceNote> notes_;
  bool valid_ = false;
};

// The time since T1's start as the log page shows it: "0:12:03".
[[nodiscard]] std::string clock_of(std::uint64_t ms);
// A log line as the logs stream holds it: any run of 17 digits starting 7656119 (a SteamID64's shape) "redacted" (PV5).
[[nodiscard]] std::string scrubbed(std::string_view line);

}  // namespace en::overlay
