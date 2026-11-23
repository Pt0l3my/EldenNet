// The runs with a folder of their own (GraceUI's observer's and the S0 runs): the folder's name and the game's
// environment (spikes.hpp).
#include "en/launcher/spikes.hpp"

#include "en/launcher/s0.hpp"

namespace en::launcher {

namespace {

constexpr wchar_t lower(wchar_t c) noexcept { return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c; }

// The entry's name is `name` (Windows' rule: names are compared ignoring case; ASCII is all ours need).
bool named(std::wstring_view entry, std::wstring_view name) noexcept {
  if (entry.size() <= name.size() || entry[name.size()] != L'=') return false;
  for (std::size_t i = 0; i < name.size(); ++i) {
    if (lower(entry[i]) != lower(name[i])) return false;
  }
  return true;
}

}  // namespace

std::string next_spike_folder(proto::watch::Spike spike, std::span<const std::string> existing) {
  if (!run_folder(spike)) return {};
  const std::string prefix = std::string(proto::watch::to_string(spike)) + "-";
  std::uint32_t highest = 0;
  for (const std::string& name : existing) {
    if (name.size() <= prefix.size() || name.size() > prefix.size() + 9) continue;
    bool same = true;  // Windows' folder names ignore case: "S0E-9" is s0e-9's place
    for (std::size_t i = 0; i < prefix.size() && same; ++i) {
      same = lower(static_cast<wchar_t>(static_cast<unsigned char>(name[i]))) == static_cast<wchar_t>(prefix[i]);
    }
    if (!same) continue;
    std::uint32_t n = 0;
    bool digits = true;
    for (std::size_t i = prefix.size(); i < name.size() && digits; ++i) {
      digits = name[i] >= '0' && name[i] <= '9';
      n = n * 10 + static_cast<std::uint32_t>(name[i] - '0');
    }
    if (digits && n > highest) highest = n;
  }
  // At most 999,999,999 runs of one word: past that the folder is reused (no PC gets there).
  return prefix + std::to_string(highest < 999'999'999u ? highest + 1 : highest);
}

std::vector<std::wstring> game_environment(std::span<const std::wstring> entries, const GameEnvironment& edit) {
  std::vector<std::wstring> out;
  out.reserve(entries.size() + 3);
  for (const std::wstring& entry : entries) {
    if (named(entry, kSpikeDirVariable)) continue;
    if (edit.appid && (named(entry, L"SteamAppId") || named(entry, L"SteamGameId"))) continue;
    out.push_back(entry);
  }
  if (edit.appid) {
    const std::wstring id = std::to_wstring(*edit.appid);
    out.push_back(L"SteamAppId=" + id);
    out.push_back(L"SteamGameId=" + id);
  }
  if (edit.spike_dir) out.push_back(std::wstring(kSpikeDirVariable) + L"=" + *edit.spike_dir);
  return out;
}

}  // namespace en::launcher
