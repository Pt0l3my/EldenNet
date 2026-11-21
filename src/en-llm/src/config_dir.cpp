// Reading the configuration's files from a folder (config.hpp's load_config_dir), for the tools and the tests. In a
// file of its own, so that a module that never reads a folder, eldennet.dll, links no std::filesystem: the DLL reads
// its files through its own platform layer and hands their texts to parse_config.
#include <fstream>
#include <sstream>

#include "en/llm/config.hpp"

namespace en::llm {

namespace {

constexpr std::uintmax_t kMaxFileBytes = 64 * 1024;  // DESIGN §11.4

std::optional<std::string> read_file(const std::filesystem::path& path, bool required) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    if (required) throw ConfigError(path.filename().string(), "not found in the configuration folder");
    return std::nullopt;
  }
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) throw ConfigError(path.filename().string(), "cannot be read");
  if (size > kMaxFileBytes) throw ConfigError(path.filename().string(), "is larger than 64 KB");
  std::ifstream in(path, std::ios::binary);
  if (!in) throw ConfigError(path.filename().string(), "cannot be opened");
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

Config load_config_dir(const std::filesystem::path& dir, const std::optional<std::filesystem::path>& models_file,
                       const EnvLookup& env) {
  ConfigTexts texts;
  texts.providers = *read_file(dir / "providers.toml", /*required=*/true);
  texts.models = read_file(models_file ? *models_file : dir / "models.toml", /*required=*/models_file.has_value());
  texts.settings = read_file(dir / "settings.toml", false);
  texts.secrets = read_file(dir / "secrets.toml", false);
  return parse_config(texts, env);
}

}  // namespace en::llm
