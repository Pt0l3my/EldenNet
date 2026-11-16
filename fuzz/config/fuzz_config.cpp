// en_fuzz_config: en-llm's readers of the player's files (providers.toml, models.toml, settings.toml, secrets.toml), as
// parse_config runs them, each text checked by en-proto's unsafe_for_toml before toml++ reads it (WHY§122). The input is
// up to four texts split at NUL bytes, in that order. A finding is anything besides a ConfigError leaving parse_config,
// and any sanitizer report or abort: toml++ 3.4.0's traps are assertions in this Debug build.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "en/llm/config.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  std::string parts[4];
  std::size_t at = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    const std::size_t cut = i < 3 ? input.find('\0', at) : std::string_view::npos;
    parts[i] = std::string(input.substr(at, cut == std::string_view::npos ? std::string_view::npos : cut - at));
    if (cut == std::string_view::npos) {
      at = input.size();
    } else {
      at = cut + 1;
    }
  }
  en::llm::ConfigTexts texts;
  texts.providers = parts[0];
  if (!parts[1].empty()) texts.models = parts[1];
  if (!parts[2].empty()) texts.settings = parts[2];
  if (!parts[3].empty()) texts.secrets = parts[3];
  try {
    (void)en::llm::parse_config(texts, [](const std::string&) { return std::optional<std::string>{}; });
  } catch (const en::llm::ConfigError&) {
  }
  return 0;
}
