// The S0 spikes in the launcher (s0.hpp): eldennet.dll's report read, the S0 kind's tasks and the console's words.
#include "en/launcher/s0.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace en::launcher {

namespace w = proto::watch;

namespace {

std::string spike_name(w::Spike s) {
  switch (s) {
    case w::Spike::S0a: return "S0-A";
    case w::Spike::S0b: return "S0-B";
    case w::Spike::S0c: return "S0-C";
    case w::Spike::S0d: return "S0-D";
    case w::Spike::S0dCrash: return "S0-D's crashes";
    default: return "The spike";
  }
}

template <class T>
void take(const nlohmann::json& j, const char* key, T& out) {
  const auto it = j.find(key);
  if (it == j.end()) return;
  if constexpr (std::is_same_v<T, bool>) {
    if (it->is_boolean()) out = it->get<bool>();
  } else if constexpr (std::is_same_v<T, std::string>) {
    // A word of the report's closed lists: lower-case letters and _, short. Anything else keeps the default, so that
    // nothing but such a word reaches the launcher's log.
    if (!it->is_string()) return;
    const std::string v = it->get<std::string>();
    if (!v.empty() && v.size() <= 24 && std::all_of(v.begin(), v.end(), [](char c) {
          return (c >= 'a' && c <= 'z') || c == '_';
        })) {
      out = v;
    }
  } else {
    if (it->is_number_unsigned() && it->get<std::uint64_t>() <= std::numeric_limits<T>::max()) {
      out = static_cast<T>(it->get<std::uint64_t>());
    }
  }
}

// The object under `key`, or an empty one.
nlohmann::json part(const nlohmann::json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_object() ? *it : nlohmann::json::object();
}

// Why a start stopped short of its reading, as a sentence's start.
std::string reason_text(std::string_view waits) {
  if (waits == "layout") return "The start's layout is not complete yet";
  if (waits == "files") return "gal's files beside eldennet.dll are missing or were refused";
  if (waits == "build") return "This game's build is not one EldenNet knows";
  if (waits == "anticheat") return "Easy Anti-Cheat is running";
  if (waits == "not_started") return "The game did not finish starting";
  return "Something the start needs is missing";
}

std::string table_text(const S0Facts& f) {
  if (f.table_items == 0) return " The layout holds no table of ids.";
  const std::string n = std::to_string(f.table_items);
  if (table_passed(f)) {
    return " All " + n + " ids of the build's table passed their checks at the start, and again once it had stopped.";
  }
  return " The build's table: " + n + " ids, " + std::to_string(f.table_refused) + " refused at the start; checked " +
         (f.table_rechecked ? "again, " + std::to_string(f.table_again_refused) + " refused and " +
                                  std::to_string(f.table_changed) + " changed."
                            : "again: not done.");
}

std::string trial_text(const S0Facts& f) {
  if (f.forced == 0) return {};
  return " Forced faults in EldenNet's own reads: " + std::to_string(f.caught) + " of " + std::to_string(f.forced) +
         " caught.";
}

Task wait_title() {
  return {TaskKind::Round, "Wait at the title screen",
          "EldenNet is starting. Stay at the title screen until this row turns grey (about a minute).", 0, false, ""};
}
Task load_save() {
  return {TaskKind::Do, "Load your save", "When you stand in the world, press Pause.", 0, true, "I am in the world"};
}
Task quit() {
  return {TaskKind::End, "Quit the game", "That was the last task: quit as you usually do.", 0, false, ""};
}

}  // namespace

std::optional<S0Facts> read_s0_facts(std::string_view text) {
  if (text.size() > kMaxSpikeReportBytes) return std::nullopt;
  const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
  if (j.is_discarded() || !j.is_object() || j.value("report", std::string()) != "spike_run") return std::nullopt;
  S0Facts f;
  take(j, "state", f.state);
  take(j, "banner", f.banner);
  take(j, "waits", f.waits);
  const nlohmann::json& table = part(j, "table");
  take(table, "items", f.table_items);
  take(table, "refused", f.table_refused);
  take(table, "rechecked", f.table_rechecked);
  take(table, "again_refused", f.table_again_refused);
  take(table, "changed", f.table_changed);
  take(part(j, "discovery"), "getter_calls", f.getter_calls);
  const nlohmann::json& trial = part(j, "trial");
  std::uint32_t seh = 0, rpm = 0, seh_caught = 0, rpm_caught = 0;
  take(trial, "seh_forced", seh);
  take(trial, "rpm_forced", rpm);
  take(trial, "seh_caught", seh_caught);
  take(trial, "rpm_caught", rpm_caught);
  f.forced = seh + rpm;
  f.caught = seh_caught + rpm_caught;
  take(part(j, "inject"), "group", f.inject_group);
  const nlohmann::json& reading = part(j, "reading");
  take(reading, "started", f.reading);
  take(reading, "ms", f.reading_ms);
  return f;
}

bool table_passed(const S0Facts& f) noexcept {
  if (f.table_items == 0) return true;  // no table: nothing of it to fail
  return f.table_refused == 0 && f.table_rechecked && f.table_again_refused == 0 && f.table_changed == 0;
}

std::string s0_who(w::Spike spike) { return "EldenNet " + spike_name(spike) + ": "; }

std::vector<Task> s0_tasks(w::Spike spike, std::uint32_t run, std::uint32_t runs) {
  using K = TaskKind;
  const std::string of = "Run " + std::to_string(run) + " of " + std::to_string(runs);
  const std::string next = run < runs ? "The next run starts by itself a few seconds later." : "This is the last run.";
  switch (spike) {
    case w::Spike::S0a:
      return {{K::Round, of + ": leave the game alone",
               "The game starts, EldenNet starts in it, and the launcher closes the game again by itself. Do not "
               "press anything.",
               0, false, ""},
              {K::End, "The launcher closes the game", next, 0, false, ""}};
    case w::Spike::S0dCrash:
      return {{K::Round, of + ": leave the game alone",
               "Once EldenNet has started, the launcher asks it to crash the game on purpose, and checks that the "
               "crash is recorded. The game closing with an error is expected.",
               0, false, ""},
              {K::End, "The game closes", next, 0, false, ""}};
    case w::Spike::S0b:
      return {wait_title(), load_save(),
              {K::Wait, "Play for two hours",
               "Play as you usually do: the open world, a dungeon, fights, a death or two, fast travel and the menus. "
               "You may stop early: quit the game, and the report says how long EldenNet read.",
               7200, false, ""},
              {K::End, "Quit the game", "The two hours are up: quit as you usually do.", 0, false, ""}};
    case w::Spike::S0c:
      return {wait_title(),
              {K::Do, "Look at the title screen's menu",
               "Note whether it offers online play or shows a message about the network, then press Pause.", 0, true,
               "I have looked"},
              load_save(),
              {K::Wait, "Play for five minutes",
               "Open the game's menu once and look at its network settings without changing them: note whether online "
               "play is offered.",
               300, false, ""},
              quit()};
    case w::Spike::S0d:
      return {{K::Round, "Wait at the title screen",
               "EldenNet is starting, and makes some of its own reads fail on purpose, which it catches. Stay at the "
               "title screen until this row turns grey (about a minute).",
               0, false, ""},
              load_save(),
              {K::Do, "Open the Steam overlay once",
               "Press Shift+Tab, wait a moment, press Shift+Tab again, then press Pause. If the Steam overlay does not "
               "show in this game, just press Pause.",
               0, false, "Done, or no overlay"},
              {K::Wait, "Play for ten minutes",
               "Play as you usually do. EldenNet makes some of its own reads fail on purpose; the game should not "
               "notice.",
               600, false, ""},
              quit()};
    default:
      return {};
  }
}

std::vector<Task> s0_waiting_tasks(w::Spike spike, const S0Facts& f) {
  if (spike != w::Spike::S0b) return {};  // every other spike's steps stay as they were: see s0_waits_text
  return {{TaskKind::End, "Quit the game", s0_waits_text(spike, f), 0, false, ""}};
}

std::string s0_begin_text(w::Spike spike, std::uint32_t run, std::uint32_t runs) {
  const std::string of = "Run " + std::to_string(run) + " of " + std::to_string(runs);
  switch (spike) {
    case w::Spike::S0a:
      return of + " starts: the game opens, EldenNet starts in it, and the launcher closes the game again. Leave it "
                  "alone.";
    case w::Spike::S0dCrash:
      return of + " starts: once EldenNet has started, the launcher asks it to crash the game on purpose, and checks "
                  "that the crash is recorded.";
    case w::Spike::S0b:
      return "The game starts now. Follow the panel beside it: wait at the title, load your save, then play for two "
             "hours.";
    case w::Spike::S0c:
      return "The game starts now, offline and without Easy Anti-Cheat; the launcher counts its connections to "
             "anything but this PC. Follow the panel beside it.";
    case w::Spike::S0d:
      return "The game starts now. EldenNet makes some of its own reads fail on purpose and catches them. Follow the "
             "panel beside it.";
    default:
      return {};
  }
}

std::string s0_stopped_text(w::Spike spike, const S0Facts& f) {
  std::string out;
  if (f.state == "ready") {
    out = "The start is ready, and EldenNet reads the game on a thread of its own.";
  } else if (f.state == "inert") {
    out = "The start stopped inert: the reader's first checks failed.";
  } else if (f.state == "exit") {
    return "EldenNet is turned off in settings.toml, so it did nothing in this run.";
  } else {
    out = "The start stopped at its banner (" + f.banner + ").";
  }
  out += table_text(f);
  if (spike == w::Spike::S0d || spike == w::Spike::S0dCrash) out += trial_text(f);
  if (f.getter_calls != 0) out += " It called the game " + std::to_string(f.getter_calls) + " times.";
  return out;
}

std::string s0_waits_text(w::Spike spike, const S0Facts& f) {
  const std::string reason = reason_text(f.waits);
  switch (spike) {
    case w::Spike::S0a:
      return reason + ": this run stopped at its banner, which S0-A counts as L1, as it should.";
    case w::Spike::S0b:
      return reason + ": S0-B waits for it. Nothing is counted against it. Quit the game: there is nothing more to "
                      "do in this run.";
    case w::Spike::S0c:
      return reason + ": S0-C's check of the gate waits for it. The offline start and the game's connections are "
                      "measured all the same: follow the panel.";
    case w::Spike::S0d:
      return reason + ": S0-D's breakers wait for it. The forced faults were measured all the same (" +
             std::to_string(f.caught) + " of " + std::to_string(f.forced) + " caught): follow the panel.";
    case w::Spike::S0dCrash:
      return reason + ": the crash is asked for at the banner instead, and is recorded all the same.";
    default:
      return reason + ".";
  }
}

std::string s0_missing_text(w::Spike spike) {
  return spike_name(spike) + " has no report of EldenNet's for this run: eldennet.dll is not this package's, or it "
                             "could not write to its folder. The launcher's own count goes on.";
}

std::string s0_end_text(w::Spike spike, const Tally& t, bool passed) {
  const std::string verdict =
      passed ? " The launcher's part passes; spike-report gives the spike's verdict."
             : " The launcher's part does not pass: the lines above say which run. spike-report gives the details.";
  if (spike == w::Spike::S0a || spike == w::Spike::S0dCrash) {
    std::string out = std::to_string(t.runs) + (t.runs == 1 ? " run: " : " runs: ") + std::to_string(t.ready) +
                      " ready, " + std::to_string(t.banner) + " at a banner, " + std::to_string(t.inert) +
                      " inert, " + std::to_string(t.timeout) + " timed out, " + std::to_string(t.crashes) +
                      " crashed";
    if (spike == w::Spike::S0a) {
      out += "; the build's table failed in " + std::to_string(t.table_failed) + " of " + std::to_string(t.reports) +
             " reports";
    } else {
      out += "; " + std::to_string(t.records) + " crashes recorded";
    }
    if (t.waiting != 0) out += "; " + std::to_string(t.waiting) + " waited for the start's layout, as they should";
    return out + "." + verdict;
  }
  std::string out = t.crashes != 0 ? "The session ended with a crash" : "The session ended without a crash";
  if (t.waiting != 0) out += "; the start waited, so what needs the reading was not measured, and is not counted";
  return out + "." + verdict;
}

}  // namespace en::launcher
