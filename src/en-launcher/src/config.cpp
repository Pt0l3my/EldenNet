// launcher.toml (config.hpp).
#include "en/launcher/config.hpp"

#include <toml.hpp>

#include "en/launcher/steam.hpp"
#include "en/proto/toml_check.hpp"

namespace en::launcher {

namespace {

std::string toml_quoted(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') out.push_back('\\');
    if (static_cast<unsigned char>(c) < 0x20) continue;  // a path holds no control character
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

}  // namespace

ConfigRead read_config(std::string_view text) {
  ConfigRead r;
  if (proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    r.problems.emplace_back("launcher.toml cannot be read safely: the defaults are used");
    return r;
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    r.problems.emplace_back("launcher.toml is not valid TOML: the defaults are used");
    return r;
  }
  const toml::table& t = parsed.table();
  for (const auto& [key, value] : t) {
    const std::string_view k = key.str();
    if (k == "game") {
      if (const auto s = value.value<std::string>(); s && !s->empty()) {
        r.config.game = path_from_utf8(*s);
      } else {
        r.problems.emplace_back("launcher.toml: game is not a path");
      }
    } else if (k == "appid_method") {
      const auto s = value.value<std::string>();
      if (const auto m = s ? appid_method_named(*s) : std::nullopt) {
        r.config.appid_method = *m;
      } else {
        r.problems.emplace_back("launcher.toml: appid_method is \"file\" or \"env\"");
      }
    } else if (k == "remove_after") {
      const auto s = value.value<std::string>();
      if (const auto m = s ? remove_after_named(*s) : std::nullopt) {
        r.config.remove_after = *m;
      } else {
        r.problems.emplace_back("launcher.toml: remove_after is \"window\", \"exit\" or \"never\"");
      }
    } else if (k == "notices_seen") {
      const auto n = value.value<std::int64_t>();
      if (n && *n >= 0 && *n <= 0xFFFF) {
        r.config.notices_seen = static_cast<std::uint32_t>(*n);
      } else {
        r.problems.emplace_back("launcher.toml: notices_seen is a small whole number");
      }
    } else if (k != "schema") {
      r.problems.emplace_back("launcher.toml: an unknown setting is left alone");
    }
  }
  return r;
}

std::string config_text(const LauncherConfig& c) {
  std::string out =
      "# EldenNet's launcher settings (ARCH 4.1, 4.5, 4.8). The launcher writes this file; you may edit it.\n"
      "schema = 1\n";
  if (c.game) {
    out += "# The game's executable or folder, remembered from --game.\ngame = " + toml_quoted(path_to_utf8(*c.game)) +
           "\n";
  }
  out += "# How the game starts without Easy Anti-Cheat: \"file\" (steam_appid.txt beside the game) or \"env\".\n";
  out += std::string("appid_method = \"") + to_string(c.appid_method) + "\"\n";
  out += "# When the launcher removes a steam_appid.txt it created: \"window\", \"exit\" or \"never\".\n";
  out += std::string("remove_after = \"") + to_string(c.remove_after) + "\"\n";
  out += "# The first-screen notices' version you chose not to see again.\nnotices_seen = " +
         std::to_string(c.notices_seen) + "\n";
  return out;
}

}  // namespace en::launcher
