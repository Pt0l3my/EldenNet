#pragma once
// The runs with a folder of their own: GraceUI's observer's (S0-E's present-hook half and S4, input; roadmap S10.1.g,
// S11.1.a) and, since 2026-11-23, the S0 runs (S0-A to S0-D, s0.hpp; roadmap S10.1.b). Each run gets a folder of its
// own, %APPDATA%\EldenNet\spikes\<word>-<n> (the launcher's --appdata moves it as it moves everything), made before
// the game starts and named to the game in ELDENNET_SPIKE_DIR, where the observer (GraceUI's grace-spike.dll) or
// eldennet.dll (its report of an S0 run) writes. The folder's name is the spike's word and the run's number in that
// folder: nothing personal. In every other start the variable is not set, and one the launcher's own environment had
// is not passed on.
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/watch.hpp"

namespace en::launcher {

inline constexpr std::wstring_view kSpikeDirVariable = L"ELDENNET_SPIKE_DIR";

// The folder's name for the next run of `spike` (one with a folder: s0.hpp's run_folder), given the names of the
// spikes folder's subfolders: `<word>-<n>`, n one more than the highest such n already there, from 1. A name that is
// not the word, a dash and 1 to 9 digits (a spike log, another word's folder, a sign, a space) counts for nothing.
// Empty for any other spike.
[[nodiscard]] std::string next_spike_folder(proto::watch::Spike spike, std::span<const std::string> existing);

// The game's environment, made from the launcher's own entries ("NAME=value", in their order; Windows' "=C:=..."
// entries kept as they are). Names compare as Windows compares them, ignoring case.
struct GameEnvironment {
  std::optional<std::uint32_t> appid;     // the env method (ARCH §4.5): SteamAppId and SteamGameId, set to it
  std::optional<std::wstring> spike_dir;  // ELDENNET_SPIKE_DIR, set to it; with none, the launcher's own is dropped
};
[[nodiscard]] std::vector<std::wstring> game_environment(std::span<const std::wstring> entries,
                                                         const GameEnvironment& edit);

}  // namespace en::launcher
