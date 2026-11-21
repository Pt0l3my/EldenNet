#include <doctest.h>
#include <inja.hpp>
#include "en/prompt/version.hpp"

TEST_CASE("inja renders a variable") {
  inja::Environment env;
  nlohmann::json data;
  data["name"] = "the hooded smith";
  CHECK(env.render("{{ name }}", data) == "the hooded smith");
  CHECK(std::string(en::prompt::version()) == "0.0.1");
}
