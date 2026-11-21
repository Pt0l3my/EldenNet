#pragma once
// The golden files' form of an event (ARCH §8.12): one JSON object a line, keys in a fixed order, numbers rounded to
// three decimals, and every character handle replaced by a name the caller chooses: in a golden file h1, h2, ... in
// order of first appearance (GoldenNames), so that no handle of a session, no name and no SteamID is ever present.
#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "en/augur/events.hpp"

namespace en::augur {

using HandleNamer = std::function<std::string(ChrHandle)>;

// h1, h2, ... in order of first appearance.
class GoldenNames {
 public:
  std::string operator()(ChrHandle h);

 private:
  std::map<std::uint64_t, std::uint32_t> seen_;
};

[[nodiscard]] nlohmann::ordered_json event_json(const Event& ev, const HandleNamer& name);
// The line, without its newline.
[[nodiscard]] std::string golden_line(const Event& ev, const HandleNamer& name);
// An actor as the golden form writes it: "player", "mute#h3", "persona:2#h4", "card:1.0#h5".
[[nodiscard]] std::string actor_text(const ActorRef& a, const HandleNamer& name);
// A key with each "#<hex handle>" replaced by its name.
[[nodiscard]] std::string key_names(const std::string& key, const HandleNamer& name);

}  // namespace en::augur
