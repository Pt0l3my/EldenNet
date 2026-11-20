// settings.toml for the first in-game build (settings.hpp; ARCH §11.5).
#include "en/shell/settings.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <toml.hpp>

#include "en/proto/toml_check.hpp"

namespace en::shell {

const char* to_string(LogLevel l) noexcept {
  switch (l) {
    case LogLevel::Error: return "error";
    case LogLevel::Warn: return "warn";
    case LogLevel::Info: return "info";
    case LogLevel::Debug: return "debug";
  }
  return "info";
}

const char* to_string(CopyMode c) noexcept { return c == CopyMode::Rpm ? "rpm" : "seh"; }

const char* to_string(ProbeRound r) noexcept { return r == ProbeRound::R1 ? "r1" : "none"; }

const char* to_string(ProbeRegister r) noexcept {
  switch (r) {
    case ProbeRegister::Mark: return "mark";
    case ProbeRegister::Auto: return "auto";
    case ProbeRegister::Never: return "never";
  }
  return "mark";
}

namespace {

// ARCH §11.5's top-level tables and keys for later increments: accepted, not read in this build.
constexpr std::array<std::string_view, 12> kLater = {"features", "autonomy", "range",   "pacing",    "ui",    "pools",
                                                      "budget",   "safety",   "hotkeys", "augur", "reactions", "allow"};

class Reader {
 public:
  explicit Reader(LoadedSettings& out) : out_(out) {}

  void problem(std::string where, std::string what) {
    if (out_.problems.size() < 32) out_.problems.push_back(SettingsProblem{std::move(where), std::move(what)});
  }

  std::optional<bool> boolean(const toml::node& n, const std::string& where) {
    if (const auto v = n.value<bool>(); n.is_boolean() && v) return *v;
    problem(where, "must be true or false");
    return std::nullopt;
  }

  std::optional<std::int64_t> integer(const toml::node& n, const std::string& where, std::int64_t lo, std::int64_t hi) {
    if (n.is_integer()) {
      const std::int64_t v = *n.value<std::int64_t>();
      if (v >= lo && v <= hi) return v;
    }
    problem(where, "must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
    return std::nullopt;
  }

  template <std::size_t N>
  std::optional<std::size_t> word(const toml::node& n, const std::string& where,
                                  const std::array<std::string_view, N>& words) {
    if (n.is_string()) {
      const std::string_view s = *n.value<std::string_view>();
      for (std::size_t i = 0; i < N; ++i) {
        if (s == words[i]) return i;
      }
    }
    std::string list;
    for (const std::string_view w : words) list += (list.empty() ? "" : ", ") + std::string(w);
    problem(where, "must be one of " + list);
    return std::nullopt;
  }

  const toml::table* table(const toml::node& n, const std::string& where) {
    if (const toml::table* t = n.as_table()) return t;
    problem(where, "must be a table");
    return nullptr;
  }

 private:
  LoadedSettings& out_;
};

void read_debug(Reader& r, const toml::table& t, Settings& s) {
  for (const auto& [k, v] : t) {
    const std::string key(k.str());
    const std::string where = "debug." + key;
    if (key == "record_sessions") {
      if (const auto b = r.boolean(v, where)) s.record_sessions = *b;
    } else if (key == "log_level") {
      if (const auto w = r.word(v, where, std::array<std::string_view, 4>{"error", "warn", "info", "debug"})) {
        s.log_level = static_cast<LogLevel>(*w);
      }
    } else if (key == "panel") {
      if (const auto b = r.boolean(v, where)) s.debug_panel = *b;  // the F11 view (part 12)
    } else if (key == "record_redact_text") {
      (void)r.boolean(v, where);  // the text redaction comes with S10.2.d's keyed hashes
    } else {
      r.problem(where, "is not a key of settings.toml");
    }
  }
}

void read_probe(Reader& r, const toml::table& t, Settings& s) {
  ProbeSettings& p = s.probe;
  for (const auto& [k, v] : t) {
    const std::string key(k.str());
    const std::string where = "probe." + key;
    if (key == "round") {
      if (const auto w = r.word(v, where, std::array<std::string_view, 2>{"none", "r1"})) {
        p.round = static_cast<ProbeRound>(*w);
      }
    } else if (key == "register") {
      if (const auto w = r.word(v, where, std::array<std::string_view, 3>{"mark", "auto", "never"})) {
        p.register_when = static_cast<ProbeRegister>(*w);
      }
    } else if (key == "object_size") {
      if (const auto n = r.integer(v, where, 0x10, 0x4000)) p.object_size = static_cast<std::uint32_t>(*n);
    } else if (key == "settle_s") {
      if (const auto n = r.integer(v, where, 0, 120)) p.settle_s = static_cast<std::uint32_t>(*n);
    } else if (key == "fault_group") {
      if (const auto w = r.word(v, where, std::array<std::string_view, proto::kGroupCount>{
                                              "session", "world", "combat", "player", "flags", "text", "ui", "place",
                                              "camera", "params", "status"})) {
        p.fault_group = static_cast<proto::Group>(*w);
      }
    } else if (key == "fault_every") {
      if (const auto n = r.integer(v, where, 0, 1'000'000)) p.fault_every = static_cast<std::uint32_t>(*n);
    } else if (key == "fault_after_s") {
      if (const auto n = r.integer(v, where, 0, 3600)) p.fault_after_s = static_cast<std::uint32_t>(*n);
    } else {
      r.problem(where, "is not a key of settings.toml");
    }
  }
}

}  // namespace

LoadedSettings parse_settings(std::string_view text, bool probe_build) {
  LoadedSettings out;
  Reader r(out);
  if (text.size() > kMaxSettingsBytes) {
    r.problem("", "the file is larger than 64 KiB");
  } else if (const std::optional<proto::TomlRefusal> unsafe = proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    r.problem("line " + std::to_string(unsafe->line), unsafe->problem);
  } else {
    toml::parse_result parsed = toml::parse(text, std::string_view{"settings.toml"});
    if (!parsed) {
      const toml::source_position at = parsed.error().source().begin;
      r.problem("line " + std::to_string(at.line), "is not valid TOML");
    } else {
      Settings& s = out.settings;
      for (const auto& [k, v] : parsed.table()) {
        const std::string key(k.str());
        if (key == "schema") {
          (void)r.integer(v, key, 1, 2);  // 1 is migrated in memory, 2 is this schema; a newer one is refused
        } else if (key == "enabled") {
          if (const auto b = r.boolean(v, key)) s.enabled = *b;
        } else if (key == "debug") {
          if (const toml::table* t = r.table(v, key)) read_debug(r, *t, s);
        } else if (key == "overlay") {
          if (const toml::table* t = r.table(v, key)) {
            for (const auto& [ok, ov] : *t) {
              if (ok.str() != "connect") {
                r.problem("overlay." + std::string(ok.str()), "is not a key of settings.toml");
              } else if (const auto b = r.boolean(ov, "overlay.connect")) {
                s.overlay_connect = *b;
              }
            }
          }
        } else if (key == "gal") {
          if (const toml::table* t = r.table(v, key)) {
            for (const auto& [gk, gv] : *t) {
              if (gk.str() != "copy") {
                r.problem("gal." + std::string(gk.str()), "is not a key of settings.toml");
              } else if (const auto w = r.word(gv, "gal.copy", std::array<std::string_view, 2>{"seh", "rpm"})) {
                s.copy = static_cast<CopyMode>(*w);
              }
            }
          }
        } else if (key == "probe" && probe_build) {
          if (const toml::table* t = r.table(v, key)) read_probe(r, *t, s);
        } else if (std::find(kLater.begin(), kLater.end(), key) != kLater.end()) {
          (void)r.table(v, key);
        } else {
          r.problem(key, key == "probe" ? "is a probe build's (EN_PROBE) only" : "is not a key of settings.toml");
        }
      }
    }
  }
  if (!out.problems.empty()) {
    out.refused = true;
    out.settings = Settings{};
    out.notes.clear();
  }
  return out;
}

}  // namespace en::shell
