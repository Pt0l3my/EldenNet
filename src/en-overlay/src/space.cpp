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

// The rate of each state topic (GraceUI DESIGN §6.5): the least time between two publishes.
std::uint64_t interval_of(std::string_view topic) noexcept {
  if (topic == "status") return 250;
  if (topic == "system.status") return 1000;
  if (topic == "logs") return 1000;
  return 0;  // on change
}

constexpr std::array<std::string_view, 5> kStates = {"settings", "status", "errors", "system.status",
                                                     "system.compatibility"};

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
  if (facts.settings_refused) {
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
  change(*s, value);
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
  notes_.back().result = "unknown_command";
  return answer(id, "unknown_command", "");
}

std::vector<Space::Change> Space::take_changes() {
  std::vector<Change> out;
  out.swap(changes_);
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
