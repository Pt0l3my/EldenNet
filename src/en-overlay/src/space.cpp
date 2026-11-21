// EldenNet's space (space.hpp): its topics and its one command path, portable.
#include "en/overlay/space.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <optional>
#include <set>
#include <utility>

#include "en/proto/base_url.hpp"
#include "graceui.h"

namespace en::overlay {

namespace {

using nlohmann::json;
namespace pw = proto::watch;

// ---- The words players read (drafts for the owner's review; PAGES §4.1 item 6: plain words, British spelling, "you").

struct Lamp {
  const char* word;  // GraceUI DESIGN §4.2.3's lamp words; null while EldenNet is starting (no lamp yet)
  const char* tone;  // GraceUI's web shell's tones (moss, ember, glint, rot)
};

const char* start_words(pw::InitState s) noexcept {
  switch (s) {
    case pw::InitState::None:
    case pw::InitState::Pin: return "Starting";
    case pw::InitState::Gate: return "Checking the game build";
    case pw::InitState::Safety: return "Checking for anti-cheat";
    case pw::InitState::Settings: return "Reading its settings";
    case pw::InitState::WaitWindow: return "Waiting for the game's window";
    case pw::InitState::Connect: return "Connecting to GraceUI";
    case pw::InitState::ReadyScan:
    case pw::InitState::WaitEngine: return "Waiting for the game to finish starting";
    case pw::InitState::Discover: return "Finding what it reads in the game";
    case pw::InitState::Register: return "Joining the game's frame";
    case pw::InitState::FirstTick: return "Checking its first reading";
    case pw::InitState::Ready: return "Started";
    case pw::InitState::Inert: return "Stopped: its first reading failed its checks";
    case pw::InitState::Banner: return "Stopped (Home's Problems say why)";
    case pw::InitState::Exit: return "Off";
  }
  return "Starting";
}

const char* fix_words(Stop s) noexcept {
  switch (s) {
    case Stop::None: return "";
    case Stop::UnknownBuild:
      return "An update of EldenNet adds this build once it has been checked. The game itself runs as usual.";
    case Stop::AntiCheat: return "Start the game with the EldenNet launcher. The game itself runs as usual.";
    case Stop::NotStarted: return "Start the game again. If it happens every time, keep the log for a bug report.";
    case Stop::DiscoveryFailed:
    case Stop::RegisterFaulted:
    case Stop::Inert: return "Look for an update of EldenNet for this build. The game itself runs as usual.";
    case Stop::SessionFailed:
      return "Start the game offline with the EldenNet launcher. The game itself runs as usual.";
    case Stop::TaskTripped: return "Restart the game to try again. If it happens again, keep the log for a bug report.";
    case Stop::SpikeRun: return "A test run: there is nothing to do.";
  }
  return "";
}

// Why EldenNet is dormant, in the player's words.
const char* dormant_words(const SpaceFacts& f) noexcept {
  if (f.badge || f.gate_why == Unclean::NotSolo || f.gate_why == Unclean::Lobby) {
    return "EldenNet pauses while you play with others online.";
  }
  if (f.gate_why == Unclean::AntiCheat) return "EldenNet stays off while Easy Anti-Cheat runs.";
  if (f.gate_why == Unclean::SessionFailed) return "EldenNet could not confirm that this session is offline and solo.";
  if (f.where == Where::Title) return "At the title screen: EldenNet waits until a character is loaded.";
  if (f.where == Where::Loading || f.gate_why == Unclean::Loading) return "Loading: EldenNet waits for the world.";
  if (f.gate_why == Unclean::Gap) return "EldenNet missed a reading of the game and waits for the next.";
  return "EldenNet checks that this session is offline and solo.";
}

Lamp lamp_of(const SpaceFacts& f) noexcept {
  switch (f.level) {
    case Level::Starting: return {nullptr, "glint"};
    case Level::Inactive: return {"Inactive", "rot"};
    case Level::Observing: return f.dormant ? Lamp{"Dormant", "glint"} : Lamp{"Observing", "ember"};
  }
  return {nullptr, "glint"};
}

std::string reason_of(const SpaceFacts& f) {
  switch (f.level) {
    case Level::Starting: return start_words(f.init);
    case Level::Inactive: return std::string(f.stop_text);
    case Level::Observing:
      return f.dormant ? dormant_words(f) : "EldenNet reads the game. NPCs speak from a later version.";
  }
  return {};
}

const char* group_words(proto::Group g) noexcept {
  switch (g) {
    case proto::Group::Session: return "The session";
    case proto::Group::World: return "The world";
    case proto::Group::Combat: return "Combat";
    case proto::Group::Player: return "Your character";
    case proto::Group::Flags: return "Progress";
    case proto::Group::Text: return "The game's text";
    case proto::Group::Ui: return "The game's screens";
    case proto::Group::Place: return "Place";
    case proto::Group::Camera: return "The camera";
    case proto::Group::Params: return "The game's tables";
    case proto::Group::Status: return "Statuses";
  }
  return "";
}

const char* group_state_words(proto::GroupState s) noexcept {
  switch (s) {
    case proto::GroupState::Untested: return "Not tested yet";
    case proto::GroupState::Ok: return "Passed";
    case proto::GroupState::Tripped: return "Stopped after faults";
  }
  return "";
}

std::string version_of(const std::array<std::uint16_t, 3>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]);
}

std::string packed_version(std::uint32_t v) {
  return std::to_string(GRACEUI_VERSION_MAJOR(v)) + "." + std::to_string(GRACEUI_VERSION_MINOR(v)) + "." +
         std::to_string(GRACEUI_VERSION_PATCH(v));
}

std::string build_of(const std::array<std::uint16_t, 4>& b) {
  return std::to_string(b[0]) + "." + std::to_string(b[1]) + "." + std::to_string(b[2]) + "." + std::to_string(b[3]);
}

std::string duration_of(std::uint64_t ms) {
  const std::uint64_t minutes = ms / 60000;
  if (minutes == 0) return "Under a minute";
  if (minutes < 60) return std::to_string(minutes) + " min";
  return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

// Why providers.toml was refused, in the player's words, with where (the file's line, never its text).
std::string refusal_words(AiRefusal r, std::uint32_t line) {
  std::string what;
  switch (r) {
    case AiRefusal::Scheme: what = "A provider's base_url starts with neither https:// nor http://."; break;
    case AiRefusal::Credentials:
      what = "A provider's base_url holds a name or password: its key goes in secrets.toml, named by key_ref.";
      break;
    case AiRefusal::QueryOrFragment: what = "A provider's base_url has a ? or # in it: it ends at its path."; break;
    case AiRefusal::PlainPublic:
      what = "A provider's base_url uses http:// to a host that is neither this PC nor a private address: use https://.";
      break;
    case AiRefusal::PlainCloud: what = "A cloud provider's base_url uses http://: use https://."; break;
    case AiRefusal::PlainKey:
      what = "A provider with a key uses http:// to another computer, where its key would cross the network "
             "unencrypted: use https://, or remove its key.";
      break;
    case AiRefusal::Other: what = "It holds something EldenNet cannot use."; break;
  }
  what += line != 0 ? " Correct providers.toml at line " + std::to_string(line) + ", then start the game again."
                    : " Correct providers.toml, then start the game again.";
  return what;
}

json item(std::string_view label, std::string value) { return json{{"label", label}, {"value", std::move(value)}}; }

json check(std::string_view label, std::string_view state, std::string detail) {
  json c{{"label", label}, {"state", state}};
  if (!detail.empty()) c["detail"] = std::move(detail);
  return c;
}

std::string text_of(const json& j) { return j.dump(-1, ' ', false, json::error_handler_t::replace); }

// ---- Bodies.

json status_body(const SpaceFacts& f) {
  const Lamp lamp = lamp_of(f);
  const std::string reason = reason_of(f);
  json b = json::object();
  if (lamp.word != nullptr) {
    b["word"] = lamp.word;
    b["tone"] = lamp.tone;
    if (!reason.empty()) b["reason"] = reason;
    b["page"] = "system.status";
  }
  json items = json::array();
  items.push_back(item("State", lamp.word != nullptr ? lamp.word : "Starting"));
  if (!reason.empty()) items.push_back(item("Why", reason));
  items.push_back(item("EldenNet", version_of(f.version)));
  if (f.graceui != 0) items.push_back(item("GraceUI", packed_version(f.graceui)));
  if (f.build_known) {
    items.push_back(item("Game build", build_of(f.build) + (f.build_supported ? ", supported" : ", not supported yet")));
  }
  items.push_back(item("This session", duration_of(f.session_ms)));
  b["items"] = std::move(items);
  return b;
}

json errors_body(const SpaceFacts& f) {
  json checks = json::array();
  if (f.stop != Stop::None && !f.stop_text.empty()) {
    checks.push_back(check(f.stop_text, f.stop == Stop::SpikeRun ? "warn" : "fail", fix_words(f.stop)));
  }
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    if ((f.tripped & (1ull << g)) == 0 || f.stop == Stop::TaskTripped) continue;
    checks.push_back(check(std::string(group_words(static_cast<proto::Group>(g))) +
                               ": EldenNet stopped reading it after faults.",
                           "warn", "Everything else goes on. Restarting the game tries it again."));
  }
  if (f.settings_refused) {
    checks.push_back(check("Your settings file has a problem, so EldenNet uses its defaults.", "warn",
                           "System › Logs names the setting. Correct it in settings.toml, then start the game again."));
  }
  if (f.ai != nullptr && f.ai->files == AiFiles::Refused) {
    checks.push_back(check("Your providers file has a problem, so EldenNet calls no model.", "warn",
                           refusal_words(f.ai->refusal, f.ai->refusal_line)));
  }
  return json{{"checks", std::move(checks)}};
}

json compatibility_body(const SpaceFacts& f);

json system_status_body(const SpaceFacts& f) {
  const Lamp lamp = lamp_of(f);
  const std::string reason = reason_of(f);
  json items = json::array();
  items.push_back(item("State", lamp.word != nullptr ? lamp.word : "Starting"));
  if (!reason.empty()) items.push_back(item("Why", reason));
  items.push_back(item("Start", start_words(f.init)));
  if (f.build_known) {
    items.push_back(item("Game build", build_of(f.build) + (f.build_supported ? ", supported" : ", not supported yet")));
  }
  if (f.level == Level::Observing) {
    items.push_back(item("Offline and solo", f.gate_open ? "Confirmed" : dormant_words(f)));
  }
  if (f.have_health) {
    std::size_t passed = 0;
    for (const proto::GroupHealth& g : f.health.groups) passed += g.state == proto::GroupState::Ok ? 1 : 0;
    items.push_back(item("What it can read", std::to_string(passed) + " of " + std::to_string(proto::kGroupCount) +
                                                 " groups have passed their self-tests"));
  }

  // EldenNet's parts, a lamp each.
  json checks = json::array();
  const bool started = f.init == pw::InitState::Ready;
  const bool stopped = f.init == pw::InitState::Banner || f.init == pw::InitState::Inert;
  checks.push_back(check("Start", started ? "pass" : stopped ? "fail" : "wait", start_words(f.init)));
  switch (f.reading) {
    case Reading::None:
      checks.push_back(check("Reading the game", stopped ? "fail" : "wait", stopped ? "Not started" : "Not yet"));
      break;
    case Reading::Registered:
    case Reading::PhaseA: checks.push_back(check("Reading the game", "wait", "Its first readings")); break;
    case Reading::Running:
      checks.push_back(check("Reading the game", "pass", std::to_string(f.snapshots) + " readings so far"));
      break;
    case Reading::Inert: checks.push_back(check("Reading the game", "fail", "Stopped")); break;
  }
  checks.push_back(check("The core", "pass", "Running"));
  // The providers file, once read: refused, in words (its line, never its text); else nothing to say here.
  if (f.ai != nullptr && f.ai->files == AiFiles::Refused) {
    checks.push_back(check("The providers file", "fail", refusal_words(f.ai->refusal, f.ai->refusal_line)));
  }
  checks.push_back(check("Anti-cheat watch", f.watcher ? "pass" : "fail",
                         f.watcher ? "Checks the game's modules every minute" : "Not running"));
  if (!f.logger) {
    checks.push_back(check("The log", "fail", "Not writing"));
  } else if (f.log_dropped != 0) {
    checks.push_back(check("The log", "warn", std::to_string(f.log_dropped) + " lines dropped"));
  } else {
    checks.push_back(check("The log", "pass", "Writing"));
  }
  if (f.recording_asked) {
    checks.push_back(f.recording ? check("Recording", "pass", std::to_string(f.recorded) + " lines recorded")
                                 : check("Recording", "fail", "It could not start"));
  }
  if (f.graceui != 0) checks.push_back(check("GraceUI", "pass", "Connected, version " + packed_version(f.graceui)));

  // What it reads: a row a group, once a snapshot has said.
  json rows = json::array();
  if (f.have_health) {
    for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
      const proto::GroupHealth& h = f.health.groups[g];
      const bool tripped = (f.tripped & (1ull << g)) != 0 || h.state == proto::GroupState::Tripped;
      rows.push_back(json{{"group", group_words(static_cast<proto::Group>(g))},
                          {"state", tripped ? group_state_words(proto::GroupState::Tripped) : group_state_words(h.state)}});
    }
  }
  json columns = json::array({json{{"id", "group"}, {"label", "Group"}}, json{{"id", "state"}, {"label", "Self-test"}}});
  // The Compatibility tab's checks, a member of the same body (GraceUI D185's `props.field`): one topic for the page.
  return json{{"items", std::move(items)},     {"checks", std::move(checks)},
              {"columns", std::move(columns)}, {"rows", std::move(rows)},
              {"compatibility", compatibility_body(f)}};
}

json compatibility_body(const SpaceFacts& f) {
  json checks = json::array();
  if (f.build_known) {
    checks.push_back(check("Game build " + build_of(f.build), f.build_supported ? "pass" : "fail",
                           f.build_supported ? "Supported" : "Not supported yet"));
  }
  checks.push_back(check("EldenNet " + version_of(f.version), "pass",
                         f.launcher ? "Started by the EldenNet launcher"
                                    : "Started by another loader; EldenNet started its crash watch itself"));
  if (f.graceui != 0) {
    checks.push_back(check("GraceUI " + packed_version(f.graceui), "pass",
                           "EldenNet needs GraceUI " + version_of(f.floor) + " or later"));
  }
  return json{{"checks", std::move(checks)}};
}


// ---- AI (PAGES §4.6.2). Projections only: the configuration's ids, our words and numbers; a base URL on Providers
// alone (PV2); a key's status, never a key; never an id a server listed, never the model's text (PV4).

const char* slot_words(std::string_view slot) noexcept {
  if (slot == proto::kDialogue) return "Conversation";
  if (slot == proto::kBarkLive) return "Quick lines";
  if (slot == proto::kBarkPool) return "Prepared lines";
  if (slot == proto::kMemory) return "Memory";
  if (slot == proto::kSelftest) return "Self-test";
  if (slot == proto::kDecisions) return "Decisions";
  if (slot == proto::kDraft) return "Profile drafts";
  return "";
}

const char* flavor_words(AiFlavor f) noexcept {
  switch (f) {
    case AiFlavor::OpenAI: return "OpenAI-compatible";
    case AiFlavor::Ollama: return "Ollama";
    case AiFlavor::LlamaCpp: return "llama.cpp";
    case AiFlavor::LmStudio: return "LM Studio";
    case AiFlavor::KoboldCpp: return "KoboldCpp";
  }
  return "";
}

const char* json_words(AiJson j) noexcept {
  switch (j) {
    case AiJson::None: return "Off";
    case AiJson::Object: return "JSON object";
    case AiJson::Schema: return "JSON schema";
  }
  return "";
}

// What went wrong, as the end of a sentence about a provider ("… could not be reached").
const char* failure_words(AiFailure f) noexcept {
  switch (f) {
    case AiFailure::None: return "";
    case AiFailure::Unreachable: return "Could not be reached.";
    case AiFailure::KeyRefused: return "Refused the key.";
    case AiFailure::RateLimited: return "Asked EldenNet to wait: too many requests.";
    case AiFailure::TimedOut: return "Took too long to answer.";
    case AiFailure::BadReply: return "Answered with something EldenNet could not read.";
    case AiFailure::BadRequest: return "Refused the request: the model may not exist there.";
    case AiFailure::Refused: return "Declined to answer.";
    case AiFailure::GateClosed: return "Not sent: model calls wait until this session is confirmed offline and solo.";
    case AiFailure::Busy: return "Not sent: the provider was busy with other calls.";
    case AiFailure::NoProvider: return "Not sent: no provider answers for it.";
    case AiFailure::Cancelled: return "Stopped before it finished.";
  }
  return "";
}

const char* verdict_words(AiVerdict v) noexcept {
  switch (v) {
    case AiVerdict::None: return "";
    case AiVerdict::Quick: return "Quick";
    case AiVerdict::Acceptable: return "Acceptable";
    case AiVerdict::Slow: return "Slow";
  }
  return "";
}

std::string seconds_of(std::int64_t ms) {
  char text[32];
  std::snprintf(text, sizeof text, "%.2f s", static_cast<double>(ms) / 1000.0);
  return text;
}

// A job's name as its owner: "Conversation's", "Quick lines'".
std::string owned(std::string_view slot) {
  const std::string name = slot_words(slot);
  return !name.empty() && name.back() == 's' ? name + "'" : name + "'s";
}

// "Conversation's", "Conversation's or Memory's".
std::string slots_owned(const std::vector<std::string_view>& slots) {
  std::string out;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    if (i != 0) out += i + 1 == slots.size() ? " or " : ", ";
    out += owned(slots[i]);
  }
  return out;
}

const char* step_state(AiStep s) noexcept {
  switch (s) {
    case AiStep::Passed: return "pass";
    case AiStep::Failed: return "fail";
    case AiStep::Skipped: return "warn";
    case AiStep::Waiting:
    case AiStep::Running: break;
  }
  return "wait";
}

std::string list_words(const AiList& l) {
  switch (l.state) {
    case AiStep::Waiting: return "Waiting for its turn.";
    case AiStep::Running: return "Asking for its models.";
    case AiStep::Skipped: return "Not asked.";
    case AiStep::Passed:
    case AiStep::Failed: break;
  }
  if (l.why != AiFailure::None) return failure_words(l.why);
  const std::string count = l.listed == 1 ? "1 model" : std::to_string(l.listed) + " models";
  if (l.missing.empty()) return "Lists " + count + ", every job's model among them.";
  return "Lists " + count + ", but not " + slots_owned(l.missing) + " model.";
}

const AiProvider* provider_of(const AiFacts& ai, std::string_view id) {
  for (const AiProvider& p : ai.providers) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const AiSlot* slot_of(const AiFacts& ai, std::string_view slot) {
  for (const AiSlot& s : ai.slots) {
    if (s.slot == slot) return &s;
  }
  return nullptr;
}

// What a provider listed last, or null when it has not answered a list this session.
const std::vector<std::string>* listed_of(const AiFacts& ai, std::string_view provider) {
  for (const AiModels& m : ai.models) {
    if (m.provider == provider) return &m.ids;
  }
  return nullptr;
}

// The banner's own words at L1 (en-core's, "EldenNet inactive: …", O95's one form), at most 200 bytes, cut at a
// character: the greyed pages' words, the AI pages' lead and their refusals alike.
std::string banner_words(const SpaceFacts& f) {
  std::string words = f.stop_text.empty() ? std::string("EldenNet inactive: Home's Problems say why.")
                                          : std::string(f.stop_text);
  if (words.size() > 200) {
    std::size_t cut = 200;
    while (cut > 0 && (static_cast<unsigned char>(words[cut]) & 0xC0) == 0x80) --cut;  // at a character
    words.resize(cut);
  }
  return words;
}

// The jobs AI › Models offers a card for, in ARCH §11.4's order, each with four rows: its provider and its model, and its
// fallback's provider and its fallback, each model row with its choices' topic; every provider row's choices are every
// job's (`ai.choices.providers`). The reserved slots (actions, vision) have no card until their features exist (PAGES
// §4.5).
struct Job {
  std::string_view slot;
  std::string_view key;
  std::string_view topic;
  std::string_view provider_key;
  std::string_view fallback_key;
  std::string_view fallback_topic;
  std::string_view fallback_provider_key;
};
constexpr std::array<Job, 5> kJobs = {{
    {proto::kDialogue, "slots.dialogue.model", "ai.choices.dialogue", "slots.dialogue.provider",
     "slots.dialogue.fallback", "ai.choices.dialogue.fallback", "slots.dialogue.fallback_provider"},
    {proto::kBarkLive, "slots.bark_live.model", "ai.choices.bark_live", "slots.bark_live.provider",
     "slots.bark_live.fallback", "ai.choices.bark_live.fallback", "slots.bark_live.fallback_provider"},
    {proto::kBarkPool, "slots.bark_pool.model", "ai.choices.bark_pool", "slots.bark_pool.provider",
     "slots.bark_pool.fallback", "ai.choices.bark_pool.fallback", "slots.bark_pool.fallback_provider"},
    {proto::kMemory, "slots.memory.model", "ai.choices.memory", "slots.memory.provider", "slots.memory.fallback",
     "ai.choices.memory.fallback", "slots.memory.fallback_provider"},
    {proto::kSelftest, "slots.selftest.model", "ai.choices.selftest", "slots.selftest.provider",
     "slots.selftest.fallback", "ai.choices.selftest.fallback", "slots.selftest.fallback_provider"},
}};
constexpr std::string_view kProviderChoices = "ai.choices.providers";
constexpr std::string_view kJobStates = "ai.jobs";  // each card's live status (GraceUI D202), a member per slot
constexpr std::string_view kBudgets = "ai.budgets";  // each card's fixed budgets, a member per slot

// Which of a job's four rows a setting's key is.
enum class Part : std::uint8_t { Model, Provider, Fallback, FallbackProvider };
struct JobRow {
  const Job* job = nullptr;
  Part part = Part::Model;
};

JobRow row_of_key(std::string_view key) {
  for (const Job& j : kJobs) {
    if (j.key == key) return {&j, Part::Model};
    if (j.provider_key == key) return {&j, Part::Provider};
    if (j.fallback_key == key) return {&j, Part::Fallback};
    if (j.fallback_provider_key == key) return {&j, Part::FallbackProvider};
  }
  return {};
}

bool of_fallback(Part p) noexcept { return p == Part::Fallback || p == Part::FallbackProvider; }

const Job* job_of_slot(std::string_view slot) {
  for (const Job& j : kJobs) {
    if (j.slot == slot) return &j;
  }
  return nullptr;
}

// A provider whose last call was refused for its key, or for want of one: no job's choice until it has a key again.
bool key_refused(const AiProvider& p) noexcept { return p.disabled || p.key == AiKey::Rejected; }

// Why a refused provider is not a job's choice, in the player's words.
std::string refused_words(const AiProvider& p) {
  return p.key_place == AiKeyPlace::None ? p.id + " asks for a key: paste it on its row on Providers first."
                                         : p.id + " refused its key: paste it again on its row on Providers first.";
}

// Why a job's fallback cannot be chosen now, in the player's words, or nothing. It needs a model of the job's own: a
// table that gives a fallback alone keeps the preset's model, and a job that borrows another's models has none to keep.
std::string fallback_refusal(const Job& job, const AiSlot* s) {
  if (s == nullptr || s->model.empty()) return "Choose " + owned(job.slot) + " model first.";
  if (!s->same_as.empty()) {
    return std::string(slot_words(job.slot)) + " uses " + owned(s->same_as) + " models: choose a model of its own first.";
  }
  return {};
}

// "Conversation", "Conversation and Memory", "Conversation, Memory and Self-test".
std::string jobs_named(const std::vector<std::string_view>& slots) {
  std::string out;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    if (i != 0) out += i + 1 == slots.size() ? " and " : ", ";
    out += slot_words(slots[i]);
  }
  return out;
}

// What the AI can do now and why, which every AI page leads with (PAGES §4.4: L2, no healthy model, the gate).
std::string ai_cause(const SpaceFacts& f) {
  const AiFacts& ai = *f.ai;
  if (f.level == Level::Inactive) return banner_words(f);  // the greyed page's one form (O95)
  if (ai.files == AiFiles::Refused) {
    return "Your providers file has a problem, so EldenNet calls no model: System › Status says what.";
  }
  if (ai.providers.empty()) {
    return "No provider yet: on Providers, add the preset's provider or one of your own.";
  }
  const AiSlot* talk = nullptr;
  for (const AiSlot& s : ai.slots) {
    if (s.slot == proto::kDialogue) talk = &s;
  }
  if (talk == nullptr || talk->model.empty()) {
    return "Conversation has no model yet: choose a preset on Models, or name one in your providers file.";
  }
  if (const AiProvider* p = provider_of(ai, talk->provider)) {
    if (p->key == AiKey::Missing) return p->id + " has no key yet: paste it on its row on Providers.";
    if (p->key == AiKey::Rejected && p->key_place == AiKeyPlace::None) {
      return p->id + " asks for a key: paste it on its row on Providers.";
    }
    if (p->key == AiKey::Rejected) return p->id + " refused its key: paste it again on its row on Providers.";
  }
  if (!talk->healthy) {
    return "No model answers Conversation now: its models are resting after failures, and try again within a minute.";
  }
  if (!f.gate_open) {
    if (f.badge || f.gate_why == Unclean::NotSolo || f.gate_why == Unclean::Lobby) {
      return "Model calls wait while you play with others online.";
    }
    return "Model calls wait until EldenNet has confirmed that this session is offline and solo.";
  }
  if (f.level == Level::Starting) return "EldenNet is still starting.";
  return "EldenNet only observes in this version: NPCs speak from a later one. The self-test checks your provider "
         "meanwhile.";
}

json ai_status_body(const SpaceFacts& f) {
  if (f.ai == nullptr) return json::object();
  return json{{"sentence", ai_cause(f)}};
}

json providers_body(const SpaceFacts& f) {
  if (f.ai == nullptr) return json::object();
  const AiFacts& ai = *f.ai;
  json columns = json::array({json{{"id", "id"}, {"label", "Provider"}}, json{{"id", "kind"}, {"label", "Kind"}},
                              json{{"id", "type"}, {"label", "Server type"}},
                              json{{"id", "address"}, {"label", "Address"}}, json{{"id", "key"}, {"label", "Key"}},
                              json{{"id", "requests"}, {"label", "Requests at once"}, {"numeric", true}},
                              json{{"id", "json"}, {"label", "JSON answers"}}});
  json rows = json::array();
  // The key column is the row's secret (GraceUI D194, D197): its status alone, never the key: "set" (one kept), "refused"
  // (its last call was refused for its key, or for want of one: the field stays for a new one), "none" (no key's place
  // and none asked for, or http:// on another computer: no field) or "" (none kept yet).
  for (const AiProvider& p : ai.providers) {
    const char* key = p.key == AiKey::Set        ? "set"
                      : p.key == AiKey::Rejected ? "refused"
                      : p.key == AiKey::NotNeeded ? "none"
                                                  : "";
    rows.push_back(json{{"id", p.id}, {"kind", p.cloud ? "Cloud" : "Local"}, {"type", flavor_words(p.flavor)},
                        {"address", p.base_url}, {"key", key}, {"requests", std::to_string(p.concurrency)},
                        {"json", json_words(p.json)}});
  }
  // The model list's last result, a check per provider.
  json checks = json::array();
  for (const AiList& l : ai.lists) {
    checks.push_back(check(l.provider, step_state(l.state), list_words(l)));
  }
  return json{{"columns", std::move(columns)}, {"rows", std::move(rows)}, {"checks", std::move(checks)}};
}

json health_body(const SpaceFacts& f) {
  if (f.ai == nullptr) return json::object();
  const AiFacts& ai = *f.ai;
  json columns = json::array({json{{"id", "id"}, {"label", "Provider"}}, json{{"id", "state"}, {"label", "State"}},
                              json{{"id", "answered"}, {"label", "Last answered"}},
                              json{{"id", "jobs"}, {"label", "Jobs"}}});
  json rows = json::array();
  for (const AiProvider& p : ai.providers) {
    std::string state = "Healthy";
    if (key_refused(p)) {
      state = p.key_place == AiKeyPlace::None ? "Failing: it asks for a key" : "Failing: it refused its key";
    } else if (p.cooling_s != 0) {
      state = "Cooling down, " + std::to_string(p.cooling_s) + " s left";
    } else if (p.pairs_cooling != 0) {
      state = p.pairs_cooling == 1 ? "A model of it is resting after failures"
                                   : std::to_string(p.pairs_cooling) + " of its models are resting after failures";
    }
    std::string jobs;
    for (const std::string_view s : p.slots) jobs += std::string(jobs.empty() ? "" : ", ") + slot_words(s);
    rows.push_back(json{{"id", p.id}, {"state", state},
                        {"answered", p.answered_ms < 0 ? std::string("Not this session")
                                                       : clock_of(static_cast<std::uint64_t>(p.answered_ms))},
                        {"jobs", jobs.empty() ? std::string("None") : jobs}});
  }
  return json{{"columns", std::move(columns)}, {"rows", std::move(rows)}};
}

// A job's state, its card's live status (GraceUI D202) in the lamp's tones (WHY§279): from the health EldenNet keeps of
// its provider (Health's words, shortened to the head's), and whether the job has a model.
struct JobState {
  const char* word;
  const char* tone;
};

JobState job_state(const AiFacts& ai, const AiSlot& s) noexcept {
  if (s.model.empty()) return {"No model", "glint"};
  if (const AiProvider* p = provider_of(ai, s.provider)) {
    if (key_refused(*p)) return {"Failing", "rot"};
    if (p->key == AiKey::Missing) return {"No key yet", "glint"};
    if (p->cooling_s != 0) return {"Cooling down", "ember"};
  }
  if (!s.healthy) return {"Resting after failures", "ember"};
  return {"Ready", "moss"};
}

// Each card's status: a member per job the facts hold, {word, tone}. Nothing before EldenNet has its facts (collect
// publishes none), and no member for a job they do not hold (invariant 2).
json jobs_body(const SpaceFacts& f) {
  json b = json::object();
  if (f.ai == nullptr) return b;
  for (const AiSlot& s : f.ai->slots) {
    if (job_of_slot(s.slot) == nullptr) continue;
    const JobState st = job_state(*f.ai, s);
    b[std::string(s.slot)] = json{{"word", st.word}, {"tone", st.tone}};
  }
  return b;
}

// A time budget in words: "6 s", "1.5 s".
std::string budget_seconds(std::uint32_t ms) {
  char text[32];
  if (ms % 1000 == 0) {
    std::snprintf(text, sizeof text, "%u s", static_cast<unsigned>(ms / 1000));
  } else {
    std::snprintf(text, sizeof text, "%.1f s", static_cast<double>(ms) / 1000.0);
  }
  return text;
}

// Each card's key-value list: "Same as Conversation" for a job that borrows another's models (PAGES §4.5), then its
// fixed budgets, read-only (ARCH §11.4: the same in every preset). A member per job.
json budgets_body(const SpaceFacts& f) {
  json b = json::object();
  if (f.ai == nullptr) return b;
  for (const AiBudget& budget : f.ai->budgets) {
    if (job_of_slot(budget.slot) == nullptr) continue;
    json items = json::array();
    if (const AiSlot* s = slot_of(*f.ai, budget.slot); s != nullptr && !s->same_as.empty()) {
      items.push_back(item("Models", std::string("Same as ") + slot_words(s->same_as)));
    }
    items.push_back(item("Longest answer", std::to_string(budget.max_tokens) + " tokens"));
    char temperature[16];
    std::snprintf(temperature, sizeof temperature, "%g", budget.temperature);
    items.push_back(item("Temperature", temperature));
    std::string first = budget_seconds(budget.first_token_ms);
    if (budget.first_token_local_ms != budget.first_token_ms) {
      first += ", " + budget_seconds(budget.first_token_local_ms) + " on a local server";
    }
    items.push_back(item("First token within", std::move(first)));
    items.push_back(item("Whole answer within", budget_seconds(budget.total_ms)));
    items.push_back(item("Retries", budget.retries == 0 ? std::string("None") : std::to_string(budget.retries)));
    b[std::string(budget.slot)] = json{{"items", std::move(items)}};
  }
  return b;
}

json selftest_body(const SpaceFacts& f) {
  if (f.ai == nullptr || !f.ai->selftest.ran) return json::object();  // absence is silent: nothing before a run
  const AiSelfTest& st = f.ai->selftest;
  json b = json::object();
  // The verdict, or where the run is.
  if (st.running) {
    b["sentence"] = "The self-test is running: it takes a few seconds.";
  } else if (st.verdict != AiVerdict::None) {
    std::string s = std::string(verdict_words(st.verdict)) + ": the first words of a reply come in about " +
                    seconds_of(st.first_page_ms) + ".";
    if (st.verdict == AiVerdict::Slow) s += " Replies will feel late; a faster model or provider helps.";
    if (st.json == AiStep::Failed) s += " But its answers in JSON failed their check: see Steps.";
    b["sentence"] = std::move(s);
  } else {
    b["sentence"] = "The self-test could not measure a reply: see Steps for what went wrong.";
  }
  // The steps.
  json checks = json::array();
  for (const AiList& l : st.lists) {
    checks.push_back(check("Reaches " + l.provider, step_state(l.state), list_words(l)));
  }
  auto step_detail = [](AiStep s, AiFailure why, const char* passed) -> std::string {
    switch (s) {
      case AiStep::Waiting: return "Waiting for its turn.";
      case AiStep::Running: return "Running.";
      case AiStep::Skipped: return "Not run: its provider did not answer.";
      case AiStep::Passed: return passed;
      case AiStep::Failed: break;
    }
    return why != AiFailure::None ? failure_words(why) : "Answered, but not in the form asked for.";
  };
  checks.push_back(check("A short reply", step_state(st.latency),
                         step_detail(st.latency, st.latency_why, "Answered and streamed its reply.")));
  checks.push_back(check("An answer in JSON", step_state(st.json),
                         step_detail(st.json, st.json_why, "Answered with the object asked for.")));
  b["checks"] = std::move(checks);
  // The figures.
  json items = json::array();
  if (!st.model.empty()) items.push_back(item("Model", st.model));
  if (!st.provider.empty()) items.push_back(item("Provider", st.provider));
  if (st.first_token_ms >= 0) {
    items.push_back(item("First token", seconds_of(st.first_token_ms)));
    char rate[32];
    std::snprintf(rate, sizeof rate, "%.0f", st.tokens_per_second);
    items.push_back(item("Tokens per second", rate));
    items.push_back(item("First page, estimated", seconds_of(st.first_page_ms)));
  }
  if (st.json == AiStep::Passed || st.json == AiStep::Failed) {
    items.push_back(item("JSON check", st.json == AiStep::Passed ? "Passed" : "Failed"));
  }
  items.push_back(item("Run at", clock_of(st.at_ms)));
  b["items"] = std::move(items);
  // Earlier runs this session.
  json rows = json::array();
  for (const AiRun& r : st.earlier) {
    rows.push_back(json{{"time", clock_of(r.at_ms)}, {"model", r.model.empty() ? std::string("None") : r.model},
                        {"verdict", r.verdict != AiVerdict::None ? std::string(verdict_words(r.verdict))
                                                                 : std::string("Not measured")}});
  }
  b["columns"] = json::array({json{{"id", "time"}, {"label", "Time"}}, json{{"id", "model"}, {"label", "Model"}},
                              json{{"id", "verdict"}, {"label", "Verdict"}}});
  b["rows"] = std::move(rows);
  return b;
}

// A job's model picker's choices (GraceUI D193): what the job's provider (the one chosen and waiting for its model, else
// the job's now) listed last, in its order, each id pickable, at most 256 (what GraceUI reads). None before a list has
// answered: the picker says so, and keeps the job's model shown.
json choices_body(const SpaceFacts& f, std::string_view provider) {
  json choices = json::array();
  if (f.ai != nullptr) {
    const std::vector<std::string>* ids = provider.empty() ? nullptr : listed_of(*f.ai, provider);
    if (ids != nullptr) {
      for (const std::string& id : *ids) {
        if (!pickable(id)) continue;
        choices.push_back(json{{"value", id}, {"label", id}});
        if (choices.size() == 256) break;
      }
    }
  }
  return json{{"choices", std::move(choices)}};
}

// Every job's provider picker's choices: providers.toml's providers, in its order, but one whose last call was refused
// for its key (a job's pick of it stays shown, unlisted, and Health says why).
json provider_choices_body(const SpaceFacts& f) {
  json choices = json::array();
  if (f.ai != nullptr) {
    for (const AiProvider& p : f.ai->providers) {
      if (!key_refused(p)) choices.push_back(json{{"value", p.id}, {"label", p.id}});
    }
  }
  return json{{"choices", std::move(choices)}};
}

// GraceUI's framework topic (D188): the pages greyed, each with its words. At L1 Home and the AI pages (PAGES §4.4; System
// and Settings stay usable), with the banner's own words (O95: one form, en-core's "EldenNet inactive: …"); at L2, or
// with no healthy model, every page stays usable and the AI pages lead with the cause (ai.status).
json space_state_body(const SpaceFacts& f) {
  json greyed = json::object();
  if (f.level == Level::Inactive) {
    const std::string words = banner_words(f);
    for (const char* page : {"home", "ai.providers", "ai.models", "ai.selftest"}) greyed[page] = words;
  }
  return json{{"greyed", std::move(greyed)}};
}

// The rate of each state topic (GraceUI DESIGN §6.5): the least time between two publishes.
std::uint64_t interval_of(std::string_view topic) noexcept {
  if (topic == "status") return 250;
  if (topic == "system.status") return 1000;
  if (topic == "logs") return 1000;
  if (topic == "health") return 1000;  // a cooldown counts down each second
  if (topic == "selftest" || topic == "providers") return 250;
  return 0;  // on change
}

constexpr std::array<std::string_view, 22> kStates = {
    "settings",
    "status",
    "errors",
    "system.status",
    "ai.status",
    "providers",
    "health",
    kJobStates,
    kBudgets,
    "selftest",
    "space.state",
    "ai.choices.dialogue",
    "ai.choices.bark_live",
    "ai.choices.bark_pool",
    "ai.choices.memory",
    "ai.choices.selftest",
    kProviderChoices,
    "ai.choices.dialogue.fallback",
    "ai.choices.bark_live.fallback",
    "ai.choices.bark_pool.fallback",
    "ai.choices.memory.fallback",
    "ai.choices.selftest.fallback"};

std::string frame_topic(std::string_view frame) {
  const json f = json::parse(frame, nullptr, false);
  if (f.is_discarded() || !f.is_object() || !f.contains("topic") || !f["topic"].is_string()) return {};
  return f["topic"].get<std::string>();
}

}  // namespace

std::string clock_of(std::uint64_t ms) {
  const std::uint64_t s = ms / 1000;
  char text[32];
  std::snprintf(text, sizeof text, "%llu:%02llu:%02llu", static_cast<unsigned long long>(s / 3600),
                static_cast<unsigned long long>((s / 60) % 60), static_cast<unsigned long long>(s % 60));
  return text;
}

std::string scrubbed(std::string_view line) {
  std::string out;
  out.reserve(line.size());
  std::size_t i = 0;
  while (i < line.size()) {
    if (std::isdigit(static_cast<unsigned char>(line[i])) == 0) {
      out.push_back(line[i++]);
      continue;
    }
    std::size_t j = i;
    while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j])) != 0) ++j;
    const std::string_view run = line.substr(i, j - i);
    out += run.size() == 17 && run.substr(0, 7) == "7656119" ? std::string_view("redacted") : run;
    i = j;
  }
  return out;
}

bool pickable(std::string_view id) noexcept {
  if (id.empty() || id.size() > 200) return false;
  if (std::any_of(id.begin(), id.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u < 0x20 || u == 0x7F;
      })) {
    return false;
  }
  // A path's marks: a backslash, a leading '/' or '~', a drive's letter, a home folder.
  if (id.find('\\') != std::string_view::npos || id.front() == '/' || id.front() == '~') return false;
  if (id.size() >= 2 && std::isalpha(static_cast<unsigned char>(id[0])) != 0 && id[1] == ':') return false;
  return id.find("/Users/") == std::string_view::npos && id.find("/home/") == std::string_view::npos;
}

Space::Space(std::string_view description) {
  settings_description_json_ = json::parse(description, nullptr, false);
  if (settings_description_json_.is_discarded() || !settings_description_json_.is_object() ||
      !settings_description_json_.contains("settings") || !settings_description_json_["settings"].is_array()) {
    return;
  }
  for (const json& e : settings_description_json_["settings"]) {
    Setting s;
    s.key = e.value("key", "");
    s.type = e.value("type", "");
    s.fallback = e.contains("default") ? e["default"] : json();
    s.home = e.value("home", "");
    s.locked = e.value("locked", false);
    s.third = e.value("third", "auto");
    if (e.contains("choices") && e["choices"].is_array()) {
      for (const json& c : e["choices"]) s.choices.push_back(c.value("value", ""));
    }
    if (e.contains("min") && e.contains("max")) {
      s.min = e["min"].get<double>();
      s.max = e["max"].get<double>();
      s.bounded = true;
    }
    if (s.key.empty() || s.type.empty() || s.home.empty() || s.fallback.is_null()) return;
    values_[s.key] = s.fallback;
    settings_.push_back(std::move(s));
  }
  valid_ = true;
}

const Space::Setting* Space::find(std::string_view key) const {
  for (const Setting& s : settings_) {
    if (s.key == key) return &s;
  }
  return nullptr;
}

bool Space::has_setting(std::string_view key) const { return find(key) != nullptr; }

void Space::set_value(std::string_view key, const json& value) {
  const Setting* s = find(key);
  if (s == nullptr) return;
  values_[std::string(key)] =
      s->type == "secret" ? json(value.is_string() && !value.get<std::string>().empty() ? "set" : "") : value;
}

void Space::watched(std::string_view sub_frame) {
  const std::string topic = frame_topic(sub_frame);
  if (topic.empty()) return;
  watched_.insert(topic);
  topics_[topic].fresh = true;
  if (topic == "logs") logs_sent_ = 0;  // a new watcher: the stream from its oldest row held (§22 item 3)
  notes_.push_back(SpaceNote{SpaceNote::What::Watched, topic, {}, {}});
}

void Space::unwatched(std::string_view unsub_frame) {
  const std::string topic = frame_topic(unsub_frame);
  if (topic.empty()) return;
  watched_.erase(topic);
  notes_.push_back(SpaceNote{SpaceNote::What::Unwatched, topic, {}, {}});
}

std::string Space::answer(const json& id, std::string_view error, std::string_view detail) const {
  json r{{"v", 1}, {"t", "result"}, {"space", kSpaceId}, {"id", id.is_number_unsigned() ? id : json(0)}};
  if (error.empty()) {
    r["ok"] = true;
  } else {
    r["error"] = error;
    if (!detail.empty()) r["detail"] = detail;  // protocol 1.1's words under the row (§28 item 1)
  }
  return text_of(r);
}

void Space::change(const Setting& s, const json& value) {
  // A secret is write-only (GraceUI's invariant 5, PV2): the space holds and publishes its status alone, "set" or "",
  // and only the change the DLL stores carries the value.
  const json shown = s.type == "secret" ? json(value.is_string() && !value.get<std::string>().empty() ? "set" : "")
                                        : value;
  if (values_[s.key] == shown && s.type != "secret") return;
  values_[s.key] = shown;
  ++settings_rev_;
  changes_.push_back(Change{s.key, value, {}});
}

std::string Space::set(const json& id, const json& args, const SpaceFacts& facts) {
  const std::string key = args.is_object() ? args.value("key", "") : "";
  SpaceNote& note = notes_.back();
  note.key = key;
  const Setting* s = find(key);
  const json value = args.is_object() && args.contains("value") ? args["value"] : json();
  const auto refuse = [&](std::string_view code, std::string_view detail) {
    note.result = code;
    return answer(id, code, detail);
  };
  if (s == nullptr) return refuse("unknown_key", "");
  if (s->locked) return refuse("locked", "");
  // The AI pages' settings live in providers.toml, not settings.toml: the preset and each job's model (`slots.*`).
  const bool ai = key == "ai.preset" || key.rfind("slots.", 0) == 0;
  if (ai) {
    if (facts.ai == nullptr) return refuse("not_now", "EldenNet's model settings are not ready yet.");
    if (facts.ai->files == AiFiles::Refused) {
      return refuse("not_now", "Your providers file has a problem: correct it first. System › Status says what.");
    }
  } else if (facts.settings_refused) {
    return refuse("not_now", "Your settings file has a problem: correct it first. System › Logs names the setting.");
  }
  bool fits = false;
  if (s->type == "bool") {
    fits = value.is_boolean();
  } else if (s->type == "choice") {
    fits = value.is_string() &&
           std::find(s->choices.begin(), s->choices.end(), value.get<std::string>()) != s->choices.end();
  } else if (s->type == "tri_state") {
    fits = value.is_string() && (value == "on" || value == "off" || value == s->third);
  } else if (s->type == "integer" || s->type == "number" || s->type == "duration_s") {
    fits = value.is_number() && (s->type != "integer" || value.is_number_integer()) &&
           (!s->bounded || (value.get<double>() >= s->min && value.get<double>() <= s->max));
  } else if (s->type == "text" || s->type == "key_name" || s->type == "secret") {
    fits = value.is_string() && value.get<std::string>().size() <= 256;
  }
  if (!fits) return refuse("invalid_value", "");
  if (s->type == "secret") {
    // A key as pasted: spaces and line ends round it are the DLL's to take off; one inside is no key.
    std::string_view v = value.get_ref<const std::string&>();
    while (!v.empty() && (v.front() == ' ' || v.front() == '\t' || v.front() == '\r' || v.front() == '\n')) {
      v.remove_prefix(1);
    }
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r' || v.back() == '\n')) {
      v.remove_suffix(1);
    }
    if (std::any_of(v.begin(), v.end(), [](char c) { return static_cast<unsigned char>(c) <= 0x20 || c == 0x7F; })) {
      return refuse("invalid_value", "A key has no spaces or line breaks in it: paste it again.");
    }
  }
  if (key == "ai.preset" && value != "custom" &&
      std::find(facts.ai->presets.begin(), facts.ai->presets.end(), value.get<std::string>()) ==
          facts.ai->presets.end()) {
    return refuse("not_now", "This version of EldenNet has no models file for that preset yet.");
  }
  const JobRow row = row_of_key(key);
  // A picker's Default (GraceUI D199: an empty default, offered first, sent back as ""): the row's reset, the preset's
  // again.
  if (row.job != nullptr && value == "") {
    return reset(id, json{{"scope", "key"}, {"target", key}}, facts);
  }
  const auto hold = [&](const json& v) {
    json& held = values_[s->key];
    if (held != v) {
      held = v;
      ++settings_rev_;
    }
  };
  // A job's provider, or its fallback's: one of providers.toml's, its key not refused (invariant 3).
  if (row.job != nullptr && (row.part == Part::Provider || row.part == Part::FallbackProvider)) {
    const Job* job = row.job;
    const std::string& v = value.get_ref<const std::string&>();
    const AiProvider* p = provider_of(*facts.ai, v);
    if (p == nullptr) {
      return refuse("invalid_value", "That provider is not in your providers file: add it on Providers first.");
    }
    if (key_refused(*p)) return refuse("invalid_value", refused_words(*p));
    const AiSlot* slot = slot_of(*facts.ai, job->slot);
    if (row.part == Part::Provider) {
      // Chosen while the job's model is one it listed, it is written at once with that model; otherwise it waits,
      // shown on its row, for a model of its list.
      const std::string now = slot != nullptr ? slot->provider : std::string();
      const std::string model = slot != nullptr ? slot->model : std::string();
      pending_.erase(std::string(job->slot));
      hold(value);
      if (v != now) {
        const std::vector<std::string>* ids = listed_of(*facts.ai, v);
        if (!model.empty() && ids != nullptr && std::find(ids->begin(), ids->end(), model) != ids->end()) {
          changes_.push_back(Change{std::string(job->key), model, v});
        } else {
          pending_[std::string(job->slot)] = v;
        }
      }
    } else {
      // The fallback's provider, for a job with a model of its own: chosen while the fallback now is one it listed (and
      // not the job's own model there), it is written at once with it; otherwise it waits for a model of its list.
      if (const std::string why = fallback_refusal(*job, slot); !why.empty()) return refuse("not_now", why);
      const std::string now = slot->fallback.empty() ? std::string() : slot->fallback_provider;
      pending_fallback_.erase(std::string(job->slot));
      hold(value);
      if (v != now) {
        const std::vector<std::string>* ids = listed_of(*facts.ai, v);
        if (!slot->fallback.empty() && ids != nullptr &&
            std::find(ids->begin(), ids->end(), slot->fallback) != ids->end() &&
            !(v == slot->provider && slot->fallback == slot->model)) {
          changes_.push_back(Change{std::string(job->fallback_key), slot->fallback, v});
        } else {
          pending_fallback_[std::string(job->slot)] = v;
        }
      }
    }
    note.result = "ok";
    return answer(id, "", "");
  }
  // A job's model: one of the choices its picker shows, what the job's provider listed (GraceUI D193; invariant 3), on
  // the provider chosen for it and waiting, else the job's own.
  if (row.job != nullptr && row.part == Part::Model) {
    const Job* job = row.job;
    const AiSlot* slot = slot_of(*facts.ai, job->slot);
    const auto waiting = pending_.find(job->slot);
    const std::string provider = waiting != pending_.end() ? waiting->second
                                 : slot != nullptr         ? slot->provider
                                                           : std::string();
    if (provider.empty()) {
      return refuse("not_now", facts.ai->providers.empty()
                                   ? std::string("Add a provider first: on Providers, add the preset's provider or one "
                                                 "of your own.")
                                   : "Choose " + owned(job->slot) + " provider first.");
    }
    const std::vector<std::string>* ids = listed_of(*facts.ai, provider);
    if (ids == nullptr) {
      return refuse("not_now", "Press List models on Providers first: the choices are the models " + provider +
                                   " lists.");
    }
    const std::string& v = value.get_ref<const std::string&>();
    if (!pickable(v) || std::find(ids->begin(), ids->end(), v) == ids->end()) {
      return refuse("invalid_value", provider + " does not list that model: press List models on Providers again.");
    }
    const bool same = slot != nullptr && waiting == pending_.end() && v == slot->model;
    if (waiting != pending_.end()) pending_.erase(waiting);
    hold(value);
    if (!same) changes_.push_back(Change{s->key, value, provider});  // the job's model now again: nothing to write
    note.result = "ok";
    return answer(id, "", "");
  }
  // A job's fallback: one of what its provider listed (the provider chosen for it and waiting, else the fallback's now,
  // else the job's), for a job with a model of its own, and not the job's own model there.
  if (row.job != nullptr && row.part == Part::Fallback) {
    const Job* job = row.job;
    const AiSlot* slot = slot_of(*facts.ai, job->slot);
    if (const std::string why = fallback_refusal(*job, slot); !why.empty()) return refuse("not_now", why);
    const std::string provider = fallback_source(job->slot, *facts.ai);
    const std::vector<std::string>* ids = listed_of(*facts.ai, provider);
    if (ids == nullptr) {
      return refuse("not_now", "Press List models on Providers first: the choices are the models " + provider +
                                   " lists.");
    }
    const std::string& v = value.get_ref<const std::string&>();
    if (!pickable(v) || std::find(ids->begin(), ids->end(), v) == ids->end()) {
      return refuse("invalid_value", provider + " does not list that model: press List models on Providers again.");
    }
    if (provider == slot->provider && v == slot->model) {
      return refuse("invalid_value", "That is " + owned(job->slot) + " own model: choose another for its fallback.");
    }
    const auto waiting = pending_fallback_.find(job->slot);
    const bool same = waiting == pending_fallback_.end() && v == slot->fallback && provider == slot->fallback_provider;
    if (waiting != pending_fallback_.end()) pending_fallback_.erase(waiting);
    hold(value);
    if (!same) changes_.push_back(Change{s->key, value, provider});
    note.result = "ok";
    return answer(id, "", "");
  }
  change(*s, value);
  note.result = "ok";
  return answer(id, "", "");
}

namespace {

// A command's text argument: its string, or nothing when it is absent or not a string.
std::optional<std::string_view> text_arg(const json& args, const char* name) {
  if (!args.is_object()) return std::nullopt;
  const auto it = args.find(name);
  if (it == args.end() || !it->is_string()) return std::nullopt;
  return std::string_view(it->get_ref<const std::string&>());
}

bool name_char(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

bool same_name(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

// Why a base URL is refused, in the player's words (en-proto's check_base_url).
const char* url_words(proto::UrlProblem p) noexcept {
  switch (p) {
    case proto::UrlProblem::None: return "";
    case proto::UrlProblem::Empty: return "Give the provider's base URL, such as https://api.example.com/v1.";
    case proto::UrlProblem::TooLong: return "A base URL is at most 200 characters long.";
    case proto::UrlProblem::Scheme:
      return "A base URL starts with https://, or http:// for a server on this PC or your home network.";
    case proto::UrlProblem::Credentials:
      return "A base URL holds no name or password: once the provider is added, put its key on its row.";
    case proto::UrlProblem::Host: return "That base URL has no host EldenNet can use: check the address.";
    case proto::UrlProblem::Port: return "That base URL's port is not a number from 1 to 65535.";
    case proto::UrlProblem::Characters:
      return "That base URL holds a character no base URL has: check for spaces, and letters outside A to Z.";
    case proto::UrlProblem::QueryOrFragment: return "A base URL ends at its path, with no ? or # after it.";
    case proto::UrlProblem::PlainRemote:
      return "http:// goes only to this PC, or to another PC of your home network by its private address, not its "
             "name: give this server's https:// address.";
  }
  return "";
}

}  // namespace

std::string Space::ai_command(const json& id, std::string_view name, json& args, const SpaceFacts& facts) {
  SpaceNote& note = notes_.back();
  const auto refuse = [&](std::string_view code, std::string_view detail) {
    note.result = code;
    return answer(id, code, detail);
  };
  const auto take = [&](AiRequest r) {
    requests_.push_back(std::move(r));
    note.result = "ok";
    return answer(id, "", "");
  };
  const AiFacts* ai = facts.ai;
  if (ai == nullptr || facts.level == Level::Starting) return refuse("not_now", "EldenNet is still starting.");
  if (facts.level == Level::Inactive) return refuse("not_now", banner_words(facts));
  if (ai->files == AiFiles::Refused) {
    return refuse("not_now", "Your providers file has a problem: correct it first. System › Status says what.");
  }
  const bool running = ai->selftest.running || ai->listing;
  const bool calls = name == "selftest.run" || name == "models.list";
  if (calls) {
    if (ai->providers.empty()) {
      return refuse("not_now", "Add a provider first: on Providers, add the preset's provider or one of your own.");
    }
    if ((name == "selftest.run" && ai->selftest.running) || (name == "models.list" && ai->listing)) {
      return refuse("busy", "");
    }
    if (running) {
      return refuse("not_now", name == "selftest.run" ? "The model list is running: try again when it ends."
                                                      : "The self-test is running: try again when it ends.");
    }
    // Only while EldenNet's gate allows network (GraceUI DESIGN §6.6; ARCH §5.1's title clause lets it at the title).
    if (!facts.gate_open) {
      return refuse("not_now", "Model calls wait until EldenNet has confirmed that this session is offline and solo.");
    }
    AiRequest r;
    r.kind = name == "selftest.run" ? AiRequest::Kind::SelfTest : AiRequest::Kind::ListModels;
    return take(std::move(r));
  }

  if (name == "providers.add") {
    // The header action sends nothing: the chosen preset's providers. The form sends its two fields: one's own.
    const bool own = args.is_object() && (args.contains("name") || args.contains("base_url"));
    if (!own) {
      const std::string preset = values_.value("ai.preset", std::string("custom"));
      if (preset == "custom") {
        return refuse("not_now", "Custom keeps your providers file as it is: choose a preset on Models to add its "
                                 "provider, or add one of your own below.");
      }
      if (ai->preset_needs_probe) {
        return refuse("not_now", "This preset finds its server on this PC with Find local servers, which comes in a "
                                 "later version.");
      }
      if (ai->preset_missing.empty()) {
        return refuse("not_now", "Your providers already include every one this preset uses.");
      }
      AiRequest r;
      r.kind = AiRequest::Kind::AddPreset;
      return take(std::move(r));
    }
    // A provider of one's own: its name and its base URL, both checked here (invariant 3), never logged.
    const std::optional<std::string_view> typed = text_arg(args, "name");
    const std::optional<std::string_view> url_typed = text_arg(args, "base_url");
    if (!typed || typed->empty()) return refuse("invalid_value", "Give the provider a name.");
    if (typed->size() > 32 || !std::all_of(typed->begin(), typed->end(), name_char)) {
      return refuse("invalid_value", "A provider's name is up to 32 letters, digits, '-', '_' or '.', with no spaces.");
    }
    for (const AiProvider& p : ai->providers) {
      if (same_name(p.id, *typed)) return refuse("invalid_value", "A provider is already called that: choose another name.");
    }
    // Its key would be kept under its name: never one already kept for another (that key would go to this address).
    const bool kept = std::any_of(ai->keys_set.begin(), ai->keys_set.end(),
                                  [&](const std::string& k) { return same_name(k, *typed); }) ||
                      std::any_of(ai->providers.begin(), ai->providers.end(), [&](const AiProvider& p) {
                        return p.key_place == AiKeyPlace::Secrets && same_name(p.key_name, *typed);
                      });
    if (kept) return refuse("invalid_value", "A key is already kept under that name: choose another name.");
    const proto::BaseUrl url = proto::check_base_url(url_typed.value_or(std::string_view()));
    if (url.problem != proto::UrlProblem::None) return refuse("invalid_value", url_words(url.problem));
    if (running) return refuse("not_now", "A self-test or a model list is running: try again when it ends.");
    AiRequest r;
    r.kind = AiRequest::Kind::AddOwn;
    r.id = std::string(*typed);
    r.base_url = url.url;
    r.local = !url.https;  // this PC's, or the home network's by a private address (O98 (a))
    return take(std::move(r));
  }

  // The row's provider, for Remove and its key: one providers.toml has.
  const std::optional<std::string_view> row = text_arg(args, "id");
  const AiProvider* p = row ? provider_of(*ai, *row) : nullptr;

  if (name == "providers.remove") {
    // Destructive, so only once confirmed (GraceUI D173, D192); a view sends the confirmation after the second press.
    const auto confirm = args.is_object() ? args.find("confirm") : args.end();
    if (!args.is_object() || confirm == args.end() || !confirm->is_boolean() || !confirm->get<bool>()) {
      return refuse("confirm_required", "");
    }
    if (p == nullptr) return refuse("invalid_value", "That provider is not in your providers file any more.");
    note.key = p->id;
    if (running) return refuse("not_now", "A self-test or a model list is running: try again when it ends.");
    if (std::find(ai->unused.begin(), ai->unused.end(), p->id) == ai->unused.end()) {
      const std::string jobs = jobs_named(p->slots);
      const std::string who = jobs.empty() ? std::string("A job uses it")
                              : p->slots.size() == 1 ? jobs + " uses it"
                                                     : jobs + " use it";
      return refuse("not_now", who + ": on Models, choose another preset, or reset a job's model to the preset's.");
    }
    AiRequest r;
    r.kind = AiRequest::Kind::Remove;
    r.id = p->id;
    return take(std::move(r));
  }

  // providers.key: the row's key (GraceUI D194), the value written to secrets.toml and never sent back: the answer is
  // ok or words that never echo it. The frame's copy of the value is overwritten here.
  const auto wipe = [&] {
    if (args.is_object()) {
      const auto v = args.find("value");
      if (v != args.end() && v->is_string()) {
        std::string& s = v->get_ref<std::string&>();
        std::fill(s.begin(), s.end(), '\0');
      }
    }
  };
  if (p == nullptr) {
    wipe();
    return refuse("invalid_value", "That provider is not in your providers file any more.");
  }
  note.key = p->id;
  std::optional<std::string_view> typed = text_arg(args, "value");
  if (!typed) return refuse("invalid_value", "");
  std::string_view v = *typed;
  while (!v.empty() && (v.front() == ' ' || v.front() == '\t' || v.front() == '\r' || v.front() == '\n')) {
    v.remove_prefix(1);
  }
  while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r' || v.back() == '\n')) v.remove_suffix(1);
  AiRequest r;
  r.id = p->id;
  if (p->key_place == AiKeyPlace::Env) {
    wipe();
    return refuse("not_now", "This provider takes its key from an environment variable, as your providers file says.");
  }
  if (v.empty()) {
    wipe();
    if (p->key_place != AiKeyPlace::Secrets) {
      note.result = "ok";
      return answer(id, "", "");  // no key to clear
    }
    r.kind = AiRequest::Kind::ClearKey;
    return take(std::move(r));
  }
  if (p->plain) {
    wipe();
    return refuse("not_now", "A key goes only to an https:// address or to this PC: this provider is reached by "
                             "http:// on your home network, so it takes no key.");
  }
  // A row that reads Not needed (GraceUI D197) takes none: no key's place, and no call refused for want of one.
  if (p->key == AiKey::NotNeeded) {
    wipe();
    return refuse("not_now", "This provider takes no key. If it asks for one, its row says so after List models.");
  }
  if (std::any_of(v.begin(), v.end(), [](char c) { return static_cast<unsigned char>(c) <= 0x20 || c == 0x7F; })) {
    wipe();
    return refuse("invalid_value", "A key has no spaces or line breaks in it: paste it again.");
  }
  if (v.size() > 1024) {
    wipe();
    return refuse("invalid_value", "That is longer than any key: paste it again.");
  }
  r.kind = AiRequest::Kind::SetKey;
  r.key = std::string(v);
  wipe();
  return take(std::move(r));
}

std::string Space::reset(const json& id, const json& args, const SpaceFacts& facts) {
  const std::string scope = args.is_object() ? args.value("scope", "") : "";
  const std::string target = args.is_object() ? args.value("target", "") : "";
  const bool confirmed = args.is_object() && args.value("confirm", false);
  SpaceNote& note = notes_.back();
  note.key = target;
  const auto refuse = [&](std::string_view code, std::string_view detail) {
    note.result = code;
    return answer(id, code, detail);
  };
  std::vector<const Setting*> chosen;
  if (scope == "key") {
    const Setting* s = find(target);
    if (s == nullptr) return refuse("unknown_key", "");
    if (s->locked) return refuse("locked", "");
    chosen.push_back(s);
  } else if (scope == "page" || scope == "all") {
    // A page's or every setting goes back to its default only once confirmed (GraceUI DESIGN §6.6; D173).
    if (!confirmed) return refuse("confirm_required", "");
    for (const Setting& s : settings_) {
      if (!s.locked && (scope == "all" || s.home == target)) chosen.push_back(&s);
    }
    if (scope == "page" && chosen.empty()) return refuse("invalid_value", "");
  } else {
    return refuse("invalid_value", "");
  }
  // The AI pages' rows are providers.toml's (a job's model back to its preset's, the preset to the default), the rest
  // settings.toml's: neither is written while its file is refused. A page's or every setting's reset leaves the AI's
  // rows as they are while their file cannot be written; a row's own reset says why.
  const auto is_ai = [](const Setting* s) { return s->key == "ai.preset" || s->key.rfind("slots.", 0) == 0; };
  const bool ai_writable = facts.ai != nullptr && facts.ai->files != AiFiles::Refused;
  if (scope != "key" && !ai_writable) {
    chosen.erase(std::remove_if(chosen.begin(), chosen.end(), is_ai), chosen.end());
    if (chosen.empty()) {
      return refuse("not_now", facts.ai == nullptr ? "EldenNet's model settings are not ready yet."
                                                   : "Your providers file has a problem: correct it first. System › "
                                                     "Status says what.");
    }
  }
  if (facts.settings_refused && std::any_of(chosen.begin(), chosen.end(), [&](const Setting* s) { return !is_ai(s); })) {
    return refuse("not_now", "Your settings file has a problem: correct it first. System › Logs names the setting.");
  }
  if (std::any_of(chosen.begin(), chosen.end(), is_ai)) {
    if (facts.ai == nullptr) return refuse("not_now", "EldenNet's model settings are not ready yet.");
    if (facts.ai->files == AiFiles::Refused) {
      return refuse("not_now", "Your providers file has a problem: correct it first. System › Status says what.");
    }
  }
  // A job's provider and model rows reset as one: its pick removed from providers.toml, its whole [slots.<slot>] (the
  // preset's provider, model and fallback again), or, with no pick there, its provider waiting for a model dropped and
  // its rows as the facts have them. Its two fallback rows likewise, but for the fallback alone, after the jobs' own
  // rows, so that a page's reset removes each table once.
  std::set<std::string_view> jobs_reset;
  std::set<std::string_view> fallbacks_reset;
  const auto hold = [&](std::string_view key, const json& value) {
    json& held = values_[std::string(key)];
    if (held != value) {
      held = value;
      ++settings_rev_;
    }
  };
  for (const bool fallbacks : {false, true}) {
    for (const Setting* s : chosen) {
      const JobRow row = row_of_key(s->key);
      if (row.job == nullptr) {
        if (!fallbacks) change(*s, s->fallback);
        continue;
      }
      if (of_fallback(row.part) != fallbacks) continue;
      const Job* job = row.job;
      const AiSlot* slot = slot_of(*facts.ai, job->slot);
      const bool picked = picked_.count(job->slot) != 0;
      if (!fallbacks) {
        if (!jobs_reset.insert(job->slot).second) continue;
        pending_.erase(std::string(job->slot));
        pending_fallback_.erase(std::string(job->slot));
        hold(job->key, picked || slot == nullptr ? json("") : json(slot->model));
        hold(job->provider_key, picked || slot == nullptr ? json("") : json(slot->provider));
        if (picked) {
          hold(job->fallback_key, "");
          hold(job->fallback_provider_key, "");
          changes_.push_back(Change{std::string(job->key), "", ""});
        }
        continue;
      }
      if (!fallbacks_reset.insert(job->slot).second) continue;
      pending_fallback_.erase(std::string(job->slot));
      const bool fallback_picked = fallback_picked_.count(job->slot) != 0;
      if (picked && jobs_reset.count(job->slot) != 0) continue;  // the job's table goes already, its fallback with it
      const bool none = fallback_picked || slot == nullptr || slot->fallback.empty();
      hold(job->fallback_key, none ? json("") : json(slot->fallback));
      hold(job->fallback_provider_key, none ? json("") : json(slot->fallback_provider));
      if (fallback_picked) changes_.push_back(Change{std::string(job->fallback_key), "", ""});
    }
  }
  note.result = "ok";
  return answer(id, "", "");
}

std::string Space::command(std::string_view cmd_frame, const SpaceFacts& facts) {
  json f = json::parse(cmd_frame, nullptr, false);
  const json id = !f.is_discarded() && f.is_object() && f.contains("id") ? f["id"] : json(0);
  const auto named = !f.is_discarded() && f.is_object() ? f.find("name") : f.end();
  const std::string name = !f.is_discarded() && f.is_object() && named != f.end() && named->is_string()
                               ? named->get<std::string>()
                               : std::string();
  notes_.push_back(SpaceNote{SpaceNote::What::Command, name.empty() ? "none" : name, {}, {}});
  if (f.is_discarded() || !f.is_object() || name.empty()) {
    notes_.back().result = "bad_message";
    return answer(id, "bad_message", "");
  }
  json none = json::object();
  json& args = f.contains("args") ? f["args"] : none;
  try {
    if (name == "settings.set") return set(id, args, facts);
    if (name == "settings.reset") return reset(id, args, facts);
    if (name == "selftest.run" || name == "models.list" || name == "providers.add" || name == "providers.remove" ||
        name == "providers.key") {
      return ai_command(id, name, args, facts);
    }
  } catch (const json::exception&) {
    // An argument of the wrong type, which `value` throws for: no change was made.
    notes_.back().result = "invalid_value";
    return answer(id, "invalid_value", "");
  }
  notes_.back().result = "unknown_command";
  return answer(id, "unknown_command", "");
}

void Space::sync(const SpaceFacts& facts) {
  if (facts.ai == nullptr) return;
  std::set<std::string, std::less<>> picked;
  std::set<std::string, std::less<>> fallback_picked;
  for (const AiSlot& s : facts.ai->slots) {
    if (job_of_slot(s.slot) == nullptr) continue;
    if (s.picked) picked.insert(std::string(s.slot));
    if (s.fallback_picked) fallback_picked.insert(std::string(s.slot));
  }
  if (picked != picked_ || fallback_picked != fallback_picked_) {
    picked_ = std::move(picked);
    fallback_picked_ = std::move(fallback_picked);
    ++settings_rev_;
  }
  for (const Job& j : kJobs) {
    const AiSlot* s = slot_of(*facts.ai, j.slot);
    // A provider waiting for a model goes once it is the job's own (or its fallback's), or no longer in providers.toml.
    if (const auto waiting = pending_.find(j.slot); waiting != pending_.end()) {
      if (provider_of(*facts.ai, waiting->second) == nullptr || (s != nullptr && s->provider == waiting->second)) {
        pending_.erase(waiting);
      }
    }
    if (const auto waiting = pending_fallback_.find(j.slot); waiting != pending_fallback_.end()) {
      if (provider_of(*facts.ai, waiting->second) == nullptr ||
          (s != nullptr && !s->fallback.empty() && s->fallback_provider == waiting->second)) {
        pending_fallback_.erase(waiting);
      }
    }
    const auto waiting = pending_.find(j.slot);
    const auto waiting_fallback = pending_fallback_.find(j.slot);
    // GraceUI holds a picked value to 200 bytes (D193): a longer id from the player's file shows as none chosen.
    const json model = s != nullptr && s->model.size() <= 200 ? s->model : std::string();
    const json provider = waiting != pending_.end() ? waiting->second
                          : s != nullptr             ? s->provider
                                                     : std::string();
    const bool fallback = s != nullptr && !s->fallback.empty() && s->fallback.size() <= 200;
    const json fallback_model = fallback ? s->fallback : std::string();
    const json fallback_provider = waiting_fallback != pending_fallback_.end() ? waiting_fallback->second
                                   : fallback                                   ? s->fallback_provider
                                                                                : std::string();
    for (const auto& [key, value] : {std::pair{j.key, model}, std::pair{j.provider_key, provider},
                                     std::pair{j.fallback_key, fallback_model},
                                     std::pair{j.fallback_provider_key, fallback_provider}}) {
      if (find(key) == nullptr) continue;
      json& held = values_[std::string(key)];
      if (held != value) {
        held = value;
        ++settings_rev_;
      }
    }
  }
}

std::string Space::fallback_source(std::string_view slot, const AiFacts& ai) const {
  if (const auto waiting = pending_fallback_.find(slot); waiting != pending_fallback_.end()) return waiting->second;
  const AiSlot* s = slot_of(ai, slot);
  if (s == nullptr) return {};
  return s->fallback.empty() ? s->provider : s->fallback_provider;
}

std::vector<Space::Change> Space::take_changes() {
  std::vector<Change> out;
  out.swap(changes_);
  return out;
}

std::vector<AiRequest> Space::take_requests() {
  std::vector<AiRequest> out;
  out.swap(requests_);
  return out;
}

std::vector<SpaceNote> Space::take_notes() {
  std::vector<SpaceNote> out;
  out.swap(notes_);
  return out;
}

void Space::log_line(std::string_view text, std::uint64_t t_ms) {
  rows_.push_back(Row{next_seq_++, clock_of(t_ms), scrubbed(text)});
  while (rows_.size() > 500) rows_.pop_front();  // the topic's ring (pages/space.toml)
}

json Space::settings_body() const {
  // The rows changed from their defaults (GraceUI D198): a job's provider and model rows when providers.toml holds a
  // model of its own for it, its fallback rows when it holds a fallback (their values are the job's now, whose default
  // stands for the preset's), any other row by its value.
  json changed = json::array();
  for (const Setting& s : settings_) {
    const JobRow row = row_of_key(s.key);
    const bool is_changed =
        row.job == nullptr    ? values_.contains(s.key) && values_.at(s.key) != s.fallback
        : of_fallback(row.part) ? fallback_picked_.count(row.job->slot) != 0
                                : picked_.count(row.job->slot) != 0;
    if (is_changed) changed.push_back(s.key);
  }
  return json{{"schema", settings_description_json_.value("schema", json::object())},
              {"description", settings_description_json_.value("settings", json::array())},
              {"values", values_},
              {"changed", std::move(changed)}};
}

json Space::log_rows() const {
  json rows = json::array();
  for (const Row& r : rows_) rows.push_back(json{{"seq", r.seq}, {"time", r.time}, {"line", r.line}});
  return rows;
}

json Space::body(std::string_view topic, const SpaceFacts& facts) const {
  if (topic == "settings") return settings_body();
  if (topic == "status") return status_body(facts);
  if (topic == "errors") return errors_body(facts);
  if (topic == "system.status") return system_status_body(facts);
  if (topic == "ai.status") return ai_status_body(facts);
  if (topic == "providers") return providers_body(facts);
  if (topic == "health") return health_body(facts);
  if (topic == kJobStates) return jobs_body(facts);
  if (topic == kBudgets) return budgets_body(facts);
  if (topic == "selftest") return selftest_body(facts);
  if (topic == "space.state") return space_state_body(facts);
  if (topic == kProviderChoices) return provider_choices_body(facts);
  for (const Job& j : kJobs) {
    if (topic == j.fallback_topic) {
      return choices_body(facts, facts.ai != nullptr ? fallback_source(j.slot, *facts.ai) : std::string());
    }
    if (topic != j.topic) continue;
    const auto waiting = pending_.find(j.slot);
    const AiSlot* s = facts.ai != nullptr ? slot_of(*facts.ai, j.slot) : nullptr;
    return choices_body(facts, waiting != pending_.end() ? std::string_view(waiting->second)
                               : s != nullptr          ? std::string_view(s->provider)
                                                       : std::string_view());
  }
  return json::object();
}

void Space::collect(const SpaceFacts& facts, std::uint64_t now_ms, std::vector<std::string>& frames) {
  if (facts.ai != nullptr && (!synced_once_ || facts.ai->revision != synced_)) {
    sync(facts);
    synced_once_ = true;
    synced_ = facts.ai->revision;
  }
  for (const std::string_view topic : kStates) {
    if (!watching(topic)) continue;
    if (topic == kJobStates && facts.ai == nullptr) continue;  // a card's status: nothing before EldenNet knows it
    Topic& t = topics_[std::string(topic)];
    if (!t.fresh && t.rev != 0 && now_ms - t.last_ms < interval_of(topic)) continue;
    std::string text = text_of(body(topic, facts));
    if (!t.fresh && text == t.last) continue;
    t.fresh = false;
    t.last = std::move(text);
    t.last_ms = now_ms;
    ++t.rev;
    frames.push_back(R"({"v":1,"t":"state","space":"eldennet","topic":")" + std::string(topic) + R"(","rev":)" +
                     std::to_string(t.rev) + R"(,"body":)" + t.last + "}");
  }
  if (watching("logs") && !rows_.empty()) {
    Topic& t = topics_["logs"];
    const bool due = t.fresh || now_ms - t.last_ms >= interval_of("logs");
    if (due && rows_.back().seq > logs_sent_) {
      // From the oldest held for a new watcher, else from the first row not sent; batches of at most 100 rows.
      std::size_t at = 0;
      while (at < rows_.size() && rows_[at].seq <= logs_sent_) ++at;
      const std::uint64_t expected = logs_sent_ + 1;
      bool first = true;
      while (at < rows_.size()) {
        json rows = json::array();
        const std::uint64_t from = rows_[at].seq;
        for (std::size_t n = 0; n < 100 && at < rows_.size(); ++n, ++at) {
          rows.push_back(json{{"seq", rows_[at].seq}, {"time", rows_[at].time}, {"line", rows_[at].line}});
        }
        const std::uint64_t dropped = first && from > expected ? from - expected : 0;
        first = false;
        frames.push_back(text_of(json{{"v", 1}, {"t", "rows"}, {"space", kSpaceId}, {"topic", "logs"},
                                      {"from_seq", from}, {"rows", std::move(rows)}, {"dropped", dropped}}));
        logs_sent_ = rows_[at - 1].seq;
      }
      t.fresh = false;
      t.last_ms = now_ms;
    }
  }
}

}  // namespace en::overlay
