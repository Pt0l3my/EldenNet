// The runs with a folder of their own (GraceUI's observer's, S0-E and S4, and the S0 runs): the spike folder's name and
// the game's environment.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/launcher/spikes.hpp"

using namespace en::launcher;
namespace w = en::proto::watch;

TEST_CASE("the spike folder: the word and the run's number in the spikes folder, from 1") {
  const std::vector<std::string> none;
  CHECK(next_spike_folder(w::Spike::S0e, none) == "s0e-1");
  CHECK(next_spike_folder(w::Spike::S0eAlone, none) == "s0e_alone-1");
  CHECK(next_spike_folder(w::Spike::S4, none) == "s4-1");
  // One more than the highest of its own word's, whatever the order and the gaps.
  const std::vector<std::string> some{"s0e-2", "s0e-10", "s0e-7", "s4-3"};
  CHECK(next_spike_folder(w::Spike::S0e, some) == "s0e-11");
  CHECK(next_spike_folder(w::Spike::S4, some) == "s4-4");
  CHECK(next_spike_folder(w::Spike::S0eAlone, some) == "s0e_alone-1");
  // What counts for nothing: another word's folder that starts alike, a spike log's name, a sign, a space, no digits,
  // more than nine digits, a bare word.
  const std::vector<std::string> others{"s0e_alone-40", "s0e-20261120-101500.jsonl", "s0e-+5", "s0e- 6", "s0e-",
                                        "s0e-1234567890", "s0e", "s0e-3x"};
  CHECK(next_spike_folder(w::Spike::S0e, others) == "s0e-1");
  // Windows' names ignore case: a folder "S0E-9" holds s0e-9's place.
  CHECK(next_spike_folder(w::Spike::S0e, std::vector<std::string>{"S0E-9"}) == "s0e-10");
  CHECK(next_spike_folder(w::Spike::S0eAlone, others) == "s0e_alone-41");
  CHECK(next_spike_folder(w::Spike::S0e, std::vector<std::string>{"s0e-000000009"}) == "s0e-10");
  // Nothing personal: the word, a dash and digits.
  for (const std::string& name : {next_spike_folder(w::Spike::S0e, some), next_spike_folder(w::Spike::S4, none)}) {
    for (const char c : name) CHECK(((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'));
  }
  // An observer's run and, since 2026-11-23, an S0 run have one (s0.hpp); a start that is no spike's, none.
  CHECK(next_spike_folder(w::Spike::None, some).empty());
  CHECK(next_spike_folder(w::Spike::S0a, some) == "s0a-1");
  // s0d and s0d-crash are words of their own: neither's folders count for the other.
  const std::vector<std::string> s0d{"s0d-2", "s0d-crash-5", "s0d-crash-6"};
  CHECK(next_spike_folder(w::Spike::S0d, s0d) == "s0d-3");
  CHECK(next_spike_folder(w::Spike::S0dCrash, s0d) == "s0d-crash-7");
  for (const w::Spike s : {w::Spike::S0b, w::Spike::S0c}) {
    CHECK(next_spike_folder(s, s0d) == std::string(w::to_string(s)) + "-1");
  }
}

TEST_CASE("the game's environment: the spike folder set in an observer's run, never left over otherwise") {
  const std::vector<std::wstring> launcher{L"=C:=C:\\work", L"Path=C:\\Windows", L"ELDENNET_SPIKE_DIR=C:\\old",
                                           L"SteamAppId=1", L"eldennet_spike_dir=C:\\older", L"TEMP=C:\\Temp"};
  // No spike folder: the launcher's own variable (in any case) is dropped, everything else kept in its order.
  CHECK(game_environment(launcher, {}) ==
        std::vector<std::wstring>{L"=C:=C:\\work", L"Path=C:\\Windows", L"SteamAppId=1", L"TEMP=C:\\Temp"});
  // A spike folder: the run's own, once, last.
  GameEnvironment spike;
  spike.spike_dir = L"C:\\appdata\\EldenNet\\spikes\\s0e-3";
  CHECK(game_environment(launcher, spike) ==
        std::vector<std::wstring>{L"=C:=C:\\work", L"Path=C:\\Windows", L"SteamAppId=1", L"TEMP=C:\\Temp",
                                  L"ELDENNET_SPIKE_DIR=C:\\appdata\\EldenNet\\spikes\\s0e-3"});
  // The env method (ARCH §4.5): SteamAppId and SteamGameId replaced, beside the spike folder.
  GameEnvironment both = spike;
  both.appid = 1245620;
  CHECK(game_environment(launcher, both) ==
        std::vector<std::wstring>{L"=C:=C:\\work", L"Path=C:\\Windows", L"TEMP=C:\\Temp", L"SteamAppId=1245620",
                                  L"SteamGameId=1245620", L"ELDENNET_SPIKE_DIR=C:\\appdata\\EldenNet\\spikes\\s0e-3"});
  // A name that only starts alike, or a value that holds the name, stays.
  const std::vector<std::wstring> alike{L"ELDENNET_SPIKE_DIRS=C:\\x", L"OTHER=ELDENNET_SPIKE_DIR=C:\\y"};
  CHECK(game_environment(alike, {}) == alike);
  CHECK(game_environment(std::vector<std::wstring>{}, {}).empty());
}
