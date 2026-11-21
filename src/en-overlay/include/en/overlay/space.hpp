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

#include "en/proto/llm.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/watch.hpp"

namespace en::overlay {

// The increment eldennet.dll registers, its release's (GraceUI D173): a page of a later one is absent from both views.
inline constexpr std::string_view kIncrement = "0.2";  // the AI pages' (PAGES §4.6.2)
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

// ---- AI (PAGES §4.6.2): en-llm's configuration, health and runs as the DLL knows them, in en-overlay's own types (it
// links no en-llm, ARCH §2.3; the DLL maps them). Ids are the configuration's (providers.toml's provider ids, its model
// ids); a base URL is shown on AI › Providers alone (PV2). Never a key: a key's status alone. Never an id a server
// listed, never the model's text.
enum class AiFiles : std::uint8_t { Missing, Refused, Ready };  // providers.toml: none, refused (its checks), read
enum class AiFlavor : std::uint8_t { OpenAI, Ollama, LlamaCpp, LmStudio, KoboldCpp };
enum class AiJson : std::uint8_t { None, Object, Schema };
enum class AiKey : std::uint8_t { NotNeeded, Set, Missing, Rejected };
// An outcome's class in the player's words (ARCH §11.2's classes, folded).
enum class AiFailure : std::uint8_t {
  None, Unreachable, KeyRefused, RateLimited, TimedOut, BadReply, BadRequest, Refused, GateClosed, Busy, NoProvider,
  Cancelled,
};
enum class AiStep : std::uint8_t { Waiting, Running, Passed, Failed, Skipped };
enum class AiVerdict : std::uint8_t { None, Quick, Acceptable, Slow };

struct AiProvider {
  std::string id;
  bool cloud = true;
  AiFlavor flavor = AiFlavor::OpenAI;
  std::string base_url;
  AiKey key = AiKey::NotNeeded;
  int concurrency = 1;
  AiJson json = AiJson::None;
  bool disabled = false;           // its key was rejected (health)
  std::uint32_t cooling_s = 0;     // rate-limited: the seconds left
  std::uint32_t pairs_cooling = 0;  // of its (provider, model) pairs, those cooling after three failures
  std::int64_t answered_ms = -1;   // when it last answered this session (since T1's start); -1: not this session
  std::vector<std::string_view> slots;  // the slots whose model or fallback it serves
};

struct AiSlot {
  std::string_view slot;  // en-proto's slot names
  std::string provider;
  std::string model;
  std::string fallback_provider;
  std::string fallback;
  std::string_view same_as;  // the slot it borrows its models from, or empty
  bool healthy = true;       // a model of it is usable now
};

struct AiList {
  std::string provider;
  AiStep state = AiStep::Waiting;
  AiFailure why = AiFailure::None;
  int listed = 0;
  std::vector<std::string_view> missing;  // the slots whose model it does not list
};

struct AiRun {
  std::uint64_t at_ms = 0;  // since T1's start
  std::string model;
  AiVerdict verdict = AiVerdict::None;
  bool passed = false;
};

struct AiSelfTest {
  bool ran = false;      // a run has started this session
  bool running = false;
  std::uint64_t at_ms = 0;
  std::vector<AiList> lists;
  AiStep latency = AiStep::Waiting;
  AiFailure latency_why = AiFailure::None;
  AiStep json = AiStep::Waiting;
  AiFailure json_why = AiFailure::None;
  std::string provider;
  std::string model;
  std::int64_t first_token_ms = -1;
  double tokens_per_second = 0;
  std::int64_t first_page_ms = 0;
  AiVerdict verdict = AiVerdict::None;
  std::vector<AiRun> earlier;  // this session's runs before this one, newest first
};

struct AiFacts {
  AiFiles files = AiFiles::Missing;
  std::string preset;                  // providers.toml's, as the file names it
  std::vector<std::string> presets;    // models.toml's
  std::vector<std::string> keys_set;   // the names secrets.toml holds a key under (for the key rows' status)
  std::vector<AiProvider> providers;   // providers.toml's, in its order
  std::vector<AiSlot> slots;           // the jobs this increment shows, in ARCH §11.4's order
  std::vector<std::string> preset_missing;  // the chosen preset's providers that providers.toml lacks
  bool preset_needs_probe = false;          // one of them has no address: Find local servers finds it (later)
  std::vector<std::string> unused;          // the providers no job uses
  AiSelfTest selftest;
  bool listing = false;                // the model list runs
  bool listed = false;                 // it has run this session
  std::vector<AiList> lists;           // its last result
};

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
  const AiFacts* ai = nullptr;    // the AI pages' facts; null: none yet (their topics publish nothing)
};

// A long command the space took (selftest.run, models.list) or a change of providers.toml it asked for
// (providers.add, providers.remove): answered `ok` at once, for the DLL to start; its progress reaches the views
// through the facts.
enum class AiRequest : std::uint8_t { SelfTest, ListModels, AddProviders, RemoveUnused };

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
  // A secret's is held and published as its status alone, "set" or "" (the AI pages' keys).
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
  [[nodiscard]] std::vector<AiRequest> take_requests();
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
  std::string ai_command(const nlohmann::json& id, std::string_view name, const nlohmann::json& args,
                         const SpaceFacts& facts);
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
  std::vector<AiRequest> requests_;
  std::vector<SpaceNote> notes_;
  bool valid_ = false;
};

// The time since T1's start as the log page shows it: "0:12:03".
[[nodiscard]] std::string clock_of(std::uint64_t ms);
// A log line as the logs stream holds it: any run of 17 digits starting 7656119 (a SteamID64's shape) "redacted" (PV5).
[[nodiscard]] std::string scrubbed(std::string_view line);

}  // namespace en::overlay
