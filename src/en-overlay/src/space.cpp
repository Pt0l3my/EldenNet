// EldenNet's space (space.hpp): its topics and its one command path, portable.
#include "en/overlay/space.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

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
  return json{{"checks", std::move(checks)}};
}

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
  return json{{"items", std::move(items)}, {"checks", std::move(checks)}, {"columns", std::move(columns)},
              {"rows", std::move(rows)}};
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

const char* key_words(AiKey k) noexcept {
  switch (k) {
    case AiKey::NotNeeded: return "None needed";
    case AiKey::Set: return "Set";
    case AiKey::Missing: return "Missing";
    case AiKey::Rejected: return "Rejected";
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

// "Conversation's", "Conversation's or Memory's".
std::string slots_owned(const std::vector<std::string_view>& slots) {
  std::string out;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    if (i != 0) out += i + 1 == slots.size() ? " or " : ", ";
    out += std::string(slot_words(slots[i])) + "'s";
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

// What the AI can do now and why, which every AI page leads with (PAGES §4.4: L2, no healthy model, the gate).
std::string ai_cause(const SpaceFacts& f) {
  const AiFacts& ai = *f.ai;
  if (f.level == Level::Inactive) return "EldenNet is inactive, so it calls no model: Home's Problems say why.";
  if (ai.files == AiFiles::Refused) {
    return "Your providers file has a problem, so EldenNet calls no model. System › Logs names the line.";
  }
  if (ai.providers.empty()) {
    return "No provider yet: choose a preset on Models, then press Add provider on Providers.";
  }
  const AiSlot* talk = nullptr;
  for (const AiSlot& s : ai.slots) {
    if (s.slot == proto::kDialogue) talk = &s;
  }
  if (talk == nullptr || talk->model.empty()) {
    return "Conversation has no model yet: choose a preset on Models, or name one in your providers file.";
  }
  if (const AiProvider* p = provider_of(ai, talk->provider)) {
    if (p->key == AiKey::Missing) return p->id + " has no key yet: paste it under Keys on Providers.";
    if (p->key == AiKey::Rejected) return p->id + " refused its key: paste it again under Keys on Providers.";
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
  for (const AiProvider& p : ai.providers) {
    rows.push_back(json{{"id", p.id}, {"kind", p.cloud ? "Cloud" : "Local"}, {"type", flavor_words(p.flavor)},
                        {"address", p.base_url}, {"key", key_words(p.key)}, {"requests", std::to_string(p.concurrency)},
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
    if (p.disabled) {
      state = "Failing: it refused its key";
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

json slots_body(const SpaceFacts& f) {
  if (f.ai == nullptr) return json::object();
  const AiFacts& ai = *f.ai;
  json columns = json::array({json{{"id", "job"}, {"label", "Job"}}, json{{"id", "provider"}, {"label", "Provider"}},
                              json{{"id", "model"}, {"label", "Model"}},
                              json{{"id", "fallback"}, {"label", "Fallback"}},
                              json{{"id", "state"}, {"label", "State"}}});
  json rows = json::array();
  for (const AiSlot& s : ai.slots) {
    json row{{"job", slot_words(s.slot)}};
    if (!s.same_as.empty()) {
      const std::string same = std::string("Same as ") + slot_words(s.same_as);
      row["provider"] = same;
      row["model"] = same;
      row["fallback"] = same;
    } else {
      row["provider"] = s.model.empty() ? std::string("None") : s.provider;
      row["model"] = s.model.empty() ? std::string("None") : s.model;
      row["fallback"] = s.fallback.empty() ? std::string("None")
                                           : s.fallback_provider == s.provider ? s.fallback
                                                                               : s.fallback + " (" + s.fallback_provider + ")";
    }
    row["state"] = s.model.empty() && s.same_as.empty() ? "No model" : s.healthy ? "Ready" : "Resting after failures";
    rows.push_back(std::move(row));
  }
  return json{{"columns", std::move(columns)}, {"rows", std::move(rows)}};
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

// The rate of each state topic (GraceUI DESIGN §6.5): the least time between two publishes.
std::uint64_t interval_of(std::string_view topic) noexcept {
  if (topic == "status") return 250;
  if (topic == "system.status") return 1000;
  if (topic == "logs") return 1000;
  if (topic == "health") return 1000;  // a cooldown counts down each second
  if (topic == "selftest" || topic == "providers") return 250;
  return 0;  // on change
}

constexpr std::array<std::string_view, 10> kStates = {"settings", "status",    "errors",   "system.status",
                                                      "system.compatibility", "ai.status", "providers", "health",
                                                      "ai.slots",             "selftest"};

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
  changes_.push_back(Change{s.key, value});
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
  // The AI pages' settings live in providers.toml and secrets.toml, not settings.toml.
  const bool ai = key == "ai.preset" || key.rfind("keys.", 0) == 0;
  if (ai) {
    if (facts.ai == nullptr) return refuse("not_now", "EldenNet's model settings are not ready yet.");
    if (facts.ai->files == AiFiles::Refused) {
      return refuse("not_now", "Your providers file has a problem: correct it first. System › Logs names the line.");
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
  change(*s, value);
  note.result = "ok";
  return answer(id, "", "");
}

std::string Space::ai_command(const json& id, std::string_view name, const json& args, const SpaceFacts& facts) {
  SpaceNote& note = notes_.back();
  const auto refuse = [&](std::string_view code, std::string_view detail) {
    note.result = code;
    return answer(id, code, detail);
  };
  const AiFacts* ai = facts.ai;
  if (ai == nullptr || facts.level == Level::Starting) return refuse("not_now", "EldenNet is still starting.");
  if (facts.level == Level::Inactive) {
    return refuse("not_now", "EldenNet is inactive, so it calls no model: Home's Problems say why.");
  }
  if (ai->files == AiFiles::Refused) {
    return refuse("not_now", "Your providers file has a problem: correct it first. System › Logs names the line.");
  }
  const bool calls = name == "selftest.run" || name == "models.list";
  if (calls) {
    if (ai->providers.empty()) {
      return refuse("not_now", "Add a provider first: choose a preset on Models, then press Add provider on Providers.");
    }
    if ((name == "selftest.run" && ai->selftest.running) || (name == "models.list" && ai->listing)) {
      return refuse("busy", "");
    }
    if (ai->selftest.running || ai->listing) {
      return refuse("not_now", name == "selftest.run" ? "The model list is running: try again when it ends."
                                                      : "The self-test is running: try again when it ends.");
    }
    // Only while EldenNet's gate allows network (GraceUI DESIGN §6.6; ARCH §5.1's title clause lets it at the title).
    if (!facts.gate_open) {
      return refuse("not_now", "Model calls wait until EldenNet has confirmed that this session is offline and solo.");
    }
    requests_.push_back(name == "selftest.run" ? AiRequest::SelfTest : AiRequest::ListModels);
    note.result = "ok";
    return answer(id, "", "");
  }
  if (name == "providers.add") {
    const std::string preset = values_.value("ai.preset", std::string("custom"));
    if (preset == "custom") {
      return refuse("not_now", "Custom keeps your providers file as it is: choose a preset on Models to add its "
                               "provider.");
    }
    if (ai->preset_needs_probe) {
      return refuse("not_now", "This preset finds its server on this PC with Find local servers, which comes in a "
                               "later version.");
    }
    if (ai->preset_missing.empty()) {
      return refuse("not_now", "Your providers already include every one this preset uses.");
    }
    requests_.push_back(AiRequest::AddProviders);
    note.result = "ok";
    return answer(id, "", "");
  }
  // providers.remove: destructive, so only once confirmed (GraceUI D173, D183); a view sends the confirmation after
  // the action's second press.
  const bool confirmed = args.is_object() && args.value("confirm", false);
  if (!confirmed) return refuse("confirm_required", "");
  if (ai->selftest.running || ai->listing) {
    return refuse("not_now", "A self-test or a model list is running: try again when it ends.");
  }
  if (ai->unused.empty()) return refuse("not_now", "Every provider is used by a job: there is none to remove.");
  requests_.push_back(AiRequest::RemoveUnused);
  note.result = "ok";
  return answer(id, "", "");
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
  if (facts.settings_refused) {
    return refuse("not_now", "Your settings file has a problem: correct it first. System › Logs names the setting.");
  }
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
  for (const Setting* s : chosen) change(*s, s->fallback);
  note.result = "ok";
  return answer(id, "", "");
}

std::string Space::command(std::string_view cmd_frame, const SpaceFacts& facts) {
  const json f = json::parse(cmd_frame, nullptr, false);
  const json id = !f.is_discarded() && f.is_object() && f.contains("id") ? f["id"] : json(0);
  const std::string name = !f.is_discarded() && f.is_object() ? f.value("name", "") : "";
  notes_.push_back(SpaceNote{SpaceNote::What::Command, name.empty() ? "none" : name, {}, {}});
  if (f.is_discarded() || !f.is_object() || name.empty()) {
    notes_.back().result = "bad_message";
    return answer(id, "bad_message", "");
  }
  const json args = f.contains("args") ? f["args"] : json::object();
  if (name == "settings.set") return set(id, args, facts);
  if (name == "settings.reset") return reset(id, args, facts);
  if (name == "selftest.run" || name == "models.list" || name == "providers.add" || name == "providers.remove") {
    return ai_command(id, name, args, facts);
  }
  notes_.back().result = "unknown_command";
  return answer(id, "unknown_command", "");
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
  json changed = json::array();
  for (const Setting& s : settings_) {
    if (values_.contains(s.key) && values_.at(s.key) != s.fallback) changed.push_back(s.key);
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
  if (topic == "system.compatibility") return compatibility_body(facts);
  if (topic == "ai.status") return ai_status_body(facts);
  if (topic == "providers") return providers_body(facts);
  if (topic == "health") return health_body(facts);
  if (topic == "ai.slots") return slots_body(facts);
  if (topic == "selftest") return selftest_body(facts);
  return json::object();
}

void Space::collect(const SpaceFacts& facts, std::uint64_t now_ms, std::vector<std::string>& frames) {
  for (const std::string_view topic : kStates) {
    if (!watching(topic)) continue;
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
