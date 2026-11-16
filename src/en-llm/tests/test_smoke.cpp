#include <doctest.h>
#include <nlohmann/json.hpp>
#include <toml.hpp>
#include "en/llm/version.hpp"

TEST_CASE("toml++ and nlohmann/json are available") {
  auto tbl = toml::parse("slot = \"dialogue\"\nmax_tokens = 140\n");
  CHECK(tbl["slot"].value_or(std::string{}) == "dialogue");
  CHECK(tbl["max_tokens"].value_or(0) == 140);
  nlohmann::json j = {{"stream", true}};
  CHECK(j["stream"].get<bool>());
  CHECK(std::string(en::llm::version()) == "0.0.1");
}
