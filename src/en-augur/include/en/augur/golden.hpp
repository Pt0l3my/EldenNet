#pragma once
// The golden files' form of an event (ARCH §8.12): one JSON object a line, keys in a fixed order, numbers rounded to
// three decimals, and every character handle replaced by a name the caller chooses: in a golden file h1, h2, ... in
// order of first appearance (GoldenNames), so that no handle of a session, no name and no SteamID is ever present.
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>

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

// A kind's payload as the golden form writes it (Tools › Workshop's schemas, PAGES §4.6.8): each field's name, in the
// form's order, and its type in words: "u32", "u32 or null", "int", "number", "bool", "word", "actor", "actor or
// null", "persona or null" (a profile's index), "bars" (each {boss: actor, name_id}) or "handles" (their names). A
// kind whose payload the form leaves empty (those with no source in this version) has none. The tests hold it to
// event_json, kind by kind.
struct FieldInfo {
  std::string_view name;
  std::string_view type;
};
[[nodiscard]] std::span<const FieldInfo> kind_fields(std::size_t kind) noexcept;
// The fields joined for a reader: "place_id (u32), prev (u32 or null)"; empty for none.
[[nodiscard]] std::string fields_text(std::size_t kind);

}  // namespace en::augur
