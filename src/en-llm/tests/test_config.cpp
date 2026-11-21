// Configuration and secrets (ARCH §11.1, §11.4, §11.5).
#include <doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "en/llm/config.hpp"
#include "en/llm/secret.hpp"
#include "rig.hpp"

using namespace en::llm;
using en::llm::test::kSecrets;
using en::llm::test::make_config;
using en::llm::test::no_env;

namespace {

constexpr const char* kPresetProviders = R"(
schema = 1
preset = "cloud-test"

[[provider]]
id = "router"
flavor = "openai"
kind = "cloud"
base_url = "https://router.example.invalid/api/v1"
key_ref = "secrets:cloud"
headers = { "X-Title" = "invented-title", "X-Extra-Key" = "secrets:other" }
extra_body = { reasoning = { enabled = false }, provider = { sort = "latency" } }
max_concurrency = 3
json_mode = "json_schema"
quant_floor = "fp8"

[slots.memory]
model = "memory-override"
)";

constexpr const char* kModels = R"(
schema = 1

[preset.cloud-test]
provider = { flavor = "openai", kind = "cloud", base_url = "https://router.example.invalid/api/v1/" }
dialogue  = { model = "talker", fallback = "talker-big" }
bark_live = { model = "barker" }
bark_pool = { model = "pooler" }
memory    = { model = "rememberer" }
warn_free_tier_for = ["bark_pool"]

[preset.local-test]
provider = { kind = "local" }
all = { model_hint = ["small-model"], quant = "Q4", background_at_rest_only = true }
)";

std::string error_of(const ConfigTexts& texts) {
  try {
    parse_config(texts, no_env);
  } catch (const ConfigError& e) {
    return e.what();
  }
  return "";
}

ConfigTexts providers_only(const std::string& providers) {
  ConfigTexts t;
  t.providers = providers;
  t.secrets = kSecrets;
  return t;
}

}  // namespace

TEST_CASE("a preset fills the slots, and providers.toml's [slots] override it") {
  ConfigTexts t;
  t.providers = kPresetProviders;
  t.models = kModels;
  t.secrets = kSecrets;
  t.settings = "schema = 2\nenabled = true\nbudget = { requests_per_hour = 900 }\n";
  const Config cfg = parse_config(t, no_env);

  REQUIRE(cfg.providers.size() == 1);
  const ProviderConfig& p = cfg.providers[0];
  CHECK(p.id == "router");
  CHECK(p.flavor == Flavor::OpenAI);
  CHECK(p.kind == ProviderKind::Cloud);
  CHECK(p.base_url == "https://router.example.invalid/api/v1");
  REQUIRE(p.key.has_value());
  CHECK(p.key->reveal() == "invented-test-key");
  CHECK(p.keys_resolved());
  REQUIRE(p.headers.size() == 1);
  CHECK(p.headers[0].name == "X-Title");
  REQUIRE(p.secret_headers.size() == 1);
  CHECK(p.secret_headers[0].first == "X-Extra-Key");
  CHECK(p.extra_body["reasoning"]["enabled"] == false);
  CHECK(p.extra_body["provider"]["sort"] == "latency");
  CHECK(p.max_concurrency == 3);
  CHECK(p.json_mode == JsonMode::JsonSchema);
  CHECK(p.quant_floor == "fp8");
  CHECK(p.cache_marks == CacheMarks::None);
  CHECK(p.logprobs);

  CHECK(cfg.preset == "cloud-test");
  CHECK(cfg.requests_per_hour == 900);
  const auto& dialogue = cfg.slot("dialogue").models;
  REQUIRE(dialogue.size() == 2);
  CHECK(dialogue[0].provider == "router");
  CHECK(dialogue[0].model == "talker");
  CHECK(dialogue[1].model == "talker-big");
  CHECK(cfg.slot("bark_live").models.at(0).model == "barker");
  CHECK(cfg.slot("bark_pool").models.at(0).model == "pooler");
  CHECK(cfg.slot("memory").models.at(0).model == "memory-override");  // providers.toml wins
  // A preset that names no draft model uses its dialogue model, and no decisions model its bark_live model.
  CHECK(cfg.slot("draft").models.at(0).model == "talker");
  CHECK(cfg.slot("decisions").models.at(0).model == "barker");
  CHECK(cfg.slot("selftest").models.at(0).model == "talker");
}

TEST_CASE("every slot has the design's budgets, and unknown slot names fall through to dialogue") {
  const Config cfg = make_config(en::llm::test::kCloud);
  const SlotBudget& d = cfg.slot("dialogue").budget;
  CHECK(d.max_tokens == 140);
  CHECK(d.temperature == doctest::Approx(0.8));
  CHECK(d.first_token == Millis(6000));
  CHECK(d.first_token_local == Millis(10000));
  CHECK(d.total == Millis(20000));
  CHECK(cfg.slot("bark_live").budget.max_tokens == 32);
  CHECK(cfg.slot("bark_live").budget.total == Millis(3000));
  CHECK(cfg.slot("bark_pool").budget.retries == 1);
  CHECK(cfg.slot("memory").budget.retries == 2);
  CHECK(cfg.slot("memory").budget.temperature == doctest::Approx(0.3));
  CHECK(cfg.slot("decisions").budget.max_tokens == 100);
  CHECK(cfg.slot("decisions").budget.temperature == doctest::Approx(0.0));
  CHECK(cfg.slot("decisions").budget.first_token == Millis(1500));
  CHECK(cfg.slot("decisions").budget.first_token_local == Millis(3000));
  CHECK(cfg.slot("decisions").budget.total == Millis(5000));
  CHECK(cfg.slot("some_new_slot").name == "dialogue");
  CHECK(cfg.requests_per_hour == 600);
}

TEST_CASE("a local preset with model hints only leaves a warning, not an error") {
  ConfigTexts t;
  t.providers = "schema = 1\npreset = \"local-test\"\n[[provider]]\nid = \"lm\"\nbase_url = \"http://127.0.0.1:1234/v1\"\n";
  t.models = kModels;
  const Config cfg = parse_config(t, no_env);
  CHECK(cfg.providers[0].kind == ProviderKind::Local);  // a loopback address is local unless told otherwise
  CHECK(cfg.providers[0].max_concurrency == 1);
  CHECK(cfg.slot("dialogue").models.empty());
  CHECK_FALSE(cfg.warnings.empty());
}

TEST_CASE("strict parsing: unknown keys and newer schemas are refused, with the line") {
  CHECK(error_of(providers_only("schema = 1\ncolour = \"red\"\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"))
            .find("unknown key \"colour\" (line 2)") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\nmax_tokens = 4\n"))
            .find("unknown key \"max_tokens\"") != std::string::npos);
  CHECK(error_of(providers_only("schema = 2\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"))
            .find("newer") != std::string::npos);
  CHECK(error_of(providers_only("[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"))
            .find("needs schema") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"
                                "[slots.banter]\nmodel = \"m\"\n"))
            .find("slots.banter") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"
                                "[slots.dialogue]\nmodle = \"m\"\n"))
            .find("unknown key \"modle\"") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"
                                "flavor = \"gpt\"\n"))
            .find("flavor must be") != std::string::npos);

  ConfigTexts settings = providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n");
  settings.settings = "budget = { requests_per_hour = 10 }\n";
  CHECK(error_of(settings).find("60 to 3600") != std::string::npos);
  settings.settings = "budget = { requests_per_hour = 600, burst = 3 }\n";
  CHECK(error_of(settings).find("unknown key \"burst\"") != std::string::npos);
  settings.settings = "schema = 3\n";
  CHECK(error_of(settings).find("newer") != std::string::npos);
  // Keys other modules own are theirs to check.
  settings.settings = "schema = 2\npacing = { gap_s = 2.5 }\nbudget = { requests_per_hour = 120 }\n";
  CHECK(parse_config(settings, no_env).requests_per_hour == 120);

  ConfigTexts models = providers_only("schema = 1\npreset = \"p\"\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n");
  models.models = "schema = 1\n[preset.p]\ndialogue = { model = \"m\", temperature = 2 }\n";
  CHECK(error_of(models).find("unknown key \"temperature\"") != std::string::npos);
  models.models = "schema = 1\n[preset.q]\ndialogue = { model = \"m\" }\n";
  CHECK(error_of(models).find("no preset \"p\"") != std::string::npos);
  models.models = "schema = 1\n[preset.p]\nprovider = { base_url = \"https://elsewhere.example.invalid\" }\ndialogue = { model = \"m\" }\n";
  CHECK(error_of(models).find("needs a provider") != std::string::npos);
}

TEST_CASE("keys come from secrets.toml or the environment, and are never written inline or echoed") {
  const std::string base = "schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n";

  ConfigTexts env_ref = providers_only(base + "key_ref = \"env:EN_TEST_KEY\"\n");
  const EnvLookup env = [](const std::string& name) -> std::optional<std::string> {
    if (name == "EN_TEST_KEY") return std::string("invented-env-key");
    return std::nullopt;
  };
  const Config from_env = parse_config(env_ref, env);
  REQUIRE(from_env.providers[0].key.has_value());
  CHECK(from_env.providers[0].key->reveal() == "invented-env-key");

  // A ref that names no key leaves the provider unused, and says so.
  const Config missing = parse_config(providers_only(base + "key_ref = \"secrets:absent\"\n"), no_env);
  CHECK_FALSE(missing.providers[0].keys_resolved());
  CHECK_FALSE(missing.warnings.empty());

  // A key written inline is refused without being repeated.
  const std::string inline_error = error_of(providers_only(base + "key_ref = \"invented-inline-value\"\n"));
  CHECK(inline_error.find("never written inline") != std::string::npos);
  CHECK(inline_error.find("invented-inline-value") == std::string::npos);

  // secrets.toml's errors name lines and key names, never values.
  ConfigTexts bad = providers_only(base);
  bad.secrets = "[keys]\nx = \"invented-secret-value\nbroken";
  std::string e = error_of(bad);
  CHECK(e.find("not valid TOML") != std::string::npos);
  CHECK(e.find("invented-secret-value") == std::string::npos);
  bad.secrets = "[keys]\nx = 12345678\n";
  e = error_of(bad);
  CHECK(e.find("keys.x must be a string") != std::string::npos);
  CHECK(e.find("12345678") == std::string::npos);
  bad.secrets = "x = \"invented-secret-value\"\n";
  e = error_of(bad);
  CHECK(e.find("unknown key \"x\"") != std::string::npos);
  CHECK(e.find("invented-secret-value") == std::string::npos);
}

TEST_CASE("a Secret prints redacted") {
  const Secret secret("invented-secret-value");
  std::ostringstream out;
  out << secret;
  CHECK(out.str() == "<redacted>");
  CHECK(secret.reveal() == "invented-secret-value");
  Secret copy = secret;
  copy = Secret("other");
  CHECK(copy.reveal() == "other");
}

namespace {

// The refusal a providers.toml of one provider meets, by its kind and line (en-llm's ConfigRefusal).
struct Refused {
  ConfigRefusal refusal = ConfigRefusal::Other;
  std::uint32_t line = 0;
  std::string what;
};

Refused refusal_of(const std::string& provider) {
  try {
    parse_config(providers_only("schema = 1\n[[provider]]\nid = \"x\"\n" + provider), no_env);
  } catch (const ConfigError& e) {
    return Refused{e.refusal(), e.line(), e.what()};
  }
  return Refused{ConfigRefusal::Other, 0, "accepted"};
}

}  // namespace

TEST_CASE("provider rules: a key never in clear text to another computer, http:// to a public host never, a base URL "
          "with no name, password, query or fragment, each refusal named with its line") {
  // http:// to a public host, whatever the kind.
  Refused r = refusal_of("kind = \"local\"\nbase_url = \"http://models.example.invalid/v1\"\n");
  CHECK(r.refusal == ConfigRefusal::PlainPublic);
  CHECK(r.line == 5);
  CHECK(r.what.find("models.example.invalid") == std::string::npos);  // the message never quotes the address
  CHECK(refusal_of("kind = \"local\"\nbase_url = \"http://192.0.2.7:1234/v1\"\n").refusal ==
        ConfigRefusal::PlainPublic);
  // A private address over http:// stays as it was, for a local server with no key; a key there is refused, by
  // key_ref or a header's secret, and a cloud provider there needs https:// as before.
  CHECK(refusal_of("kind = \"local\"\nbase_url = \"http://[fd00::20]:1234/v1\"\n").what == "accepted");
  const std::string private_v4 = std::to_string(10) + ".1.2.3";  // invented, by number (the sweep's ip rule)
  CHECK(refusal_of("kind = \"local\"\nbase_url = \"http://" + private_v4 + ":1234/v1\"\n").what == "accepted");
  r = refusal_of("kind = \"local\"\nbase_url = \"http://[fd00::20]:1234/v1\"\nkey_ref = \"secrets:cloud\"\n");
  CHECK(r.refusal == ConfigRefusal::PlainKey);
  CHECK(r.line == 6);
  CHECK(refusal_of("kind = \"local\"\nbase_url = \"http://[fd00::20]:1234/v1\"\nkey_ref = \"env:SOME_KEY\"\n")
            .refusal == ConfigRefusal::PlainKey);
  CHECK(refusal_of("kind = \"local\"\nbase_url = \"http://[fd00::20]:1234/v1\"\n"
                   "headers = { X-Token = \"secrets:cloud\" }\n")
            .refusal == ConfigRefusal::PlainKey);
  CHECK(refusal_of("kind = \"local\"\nbase_url = \"http://[fd00::20]:1234/v1\"\nheaders = { X-Client = \"eldennet\" }\n")
            .what == "accepted");  // a plain header is no key
  // A home server is local by default (O98 (a)); one marked cloud there needs https://, as before.
  CHECK(refusal_of("base_url = \"http://[fd00::20]:1234/v1\"\n").what == "accepted");
  CHECK(refusal_of("kind = \"cloud\"\nbase_url = \"http://[fd00::20]:1234/v1\"\n").refusal ==
        ConfigRefusal::PlainCloud);
  CHECK(refusal_of("base_url = \"http://storeroom.invalid:1234/v1\"\n").refusal == ConfigRefusal::PlainPublic);  // a name
  ConfigTexts home = providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"http://[fd00::20]:1234/v1\"\n");
  CHECK(parse_config(home, no_env).providers[0].kind == ProviderKind::Local);
  // This PC: http:// with a key, as before.
  CHECK(refusal_of("base_url = \"http://127.0.0.1:1234/v1\"\nkey_ref = \"secrets:cloud\"\n").what == "accepted");
  CHECK(refusal_of("base_url = \"http://localhost:11434/v1\"\nkey_ref = \"secrets:cloud\"\n").what == "accepted");
  // A name or password, a query or a fragment, a scheme: refused, the message quoting none of them.
  r = refusal_of("base_url = \"https://someone:hunter@models.example.invalid/v1\"\n");
  CHECK(r.refusal == ConfigRefusal::Credentials);
  CHECK(r.what.find("hunter") == std::string::npos);
  CHECK(r.line == 4);
  CHECK(refusal_of("base_url = \"https://models.example.invalid/v1?key=abc\"\n").refusal ==
        ConfigRefusal::QueryOrFragment);
  CHECK(refusal_of("base_url = \"https://models.example.invalid/v1#top\"\n").refusal ==
        ConfigRefusal::QueryOrFragment);
  CHECK(refusal_of("base_url = \"ftp://models.example.invalid/v1\"\n").refusal == ConfigRefusal::Scheme);
  // Every other refusal is Other, with its line where it names one.
  r = refusal_of("base_url = \"https://models.example.invalid/v1\"\nmax_concurrency = \"two\"\n");
  CHECK(r.refusal == ConfigRefusal::Other);
  CHECK(refusal_of("base_url = \"https://models.example.invalid/v1\"\n").what == "accepted");
}

TEST_CASE("provider rules: https for cloud keys, client fields stay the client's, flavour defaults") {
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nkind = \"cloud\"\nbase_url = \"http://x.example.invalid\"\n"))
            .find("https") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"
                                "extra_body = { model = \"other\" }\n"))
            .find("extra_body may not set \"model\"") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"
                                "quant_floor = \"fp7\"\n"))
            .find("quant_floor") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n"
                                "[[provider]]\nid = \"x\"\nbase_url = \"https://y.example.invalid\"\n"))
            .find("two providers") != std::string::npos);
  CHECK(error_of(providers_only("schema = 1\n")).find("names no [[provider]]") != std::string::npos);

  const Config local = make_config(en::llm::test::kLocal);
  CHECK(local.providers[0].cache_marks == CacheMarks::CachePrompt);
  CHECK(local.providers[0].logprobs);
  CHECK(local.providers[0].max_concurrency == 1);
  const Config ollama = make_config("schema = 1\n[[provider]]\nid = \"o\"\nflavor = \"ollama\"\nbase_url = \"http://localhost:11434/v1\"\n");
  CHECK(ollama.providers[0].kind == ProviderKind::Local);
  CHECK(ollama.providers[0].cache_marks == CacheMarks::None);
  CHECK_FALSE(ollama.providers[0].logprobs);
}

TEST_CASE("the quantisation floor admits its level and everything above") {
  CHECK(quantizations_from("fp8") == std::vector<std::string>{"fp8", "fp16", "bf16", "fp32"});
  CHECK(quantizations_from("int8") == std::vector<std::string>{"int8", "fp8", "fp16", "bf16", "fp32"});
  CHECK(quantizations_from("fp32") == std::vector<std::string>{"fp32"});
}

TEST_CASE("the configuration folder is read with its size cap") {
  namespace fs = std::filesystem;
  // A name of its own in every process: two test runs at once never share a folder (std::rand() is unseeded).
  const fs::path dir = fs::temp_directory_path() /
                      ("en-llm-config-test-" + std::to_string(std::random_device{}()) + "-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(dir);
  auto write = [&](const char* name, const std::string& text) { std::ofstream(dir / name, std::ios::binary) << text; };
  CHECK_THROWS_AS(load_config_dir(dir), ConfigError);  // no providers.toml
  write("providers.toml", kPresetProviders);
  write("models.toml", kModels);
  write("secrets.toml", kSecrets);
  write("settings.toml", "budget = { requests_per_hour = 300 }\n");
  const Config cfg = load_config_dir(dir, std::nullopt, no_env);
  CHECK(cfg.requests_per_hour == 300);
  CHECK(cfg.slot("dialogue").models.at(0).model == "talker");
  write("settings.toml", std::string(70 * 1024, '#'));
  try {
    load_config_dir(dir, std::nullopt, no_env);
    CHECK(false);
  } catch (const ConfigError& e) {
    CHECK(std::string(e.what()).find("64 KB") != std::string::npos);
  }
  fs::remove_all(dir);
}

TEST_CASE("the player's files are checked before toml++ reads them, and a refusal quotes nothing (WHY\u00A7122)") {
  ConfigTexts t = providers_only("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n");
  // toml++ 3.4.0's traps, each undefined behaviour in a release build: refused with the file and the line.
  t.settings = "budget = { requests_per_hour = 600 }\n[!]\n";
  CHECK(error_of(t).find("settings.toml: line 2: ") != std::string::npos);
  t.settings = "x = [1, }]\n";
  CHECK(error_of(t).find("settings.toml: line 1: ") != std::string::npos);
  t.settings = "budget = { requests_per_hour = 600 } \xC3\xA9\n";
  CHECK(error_of(t).find("outside plain ASCII") != std::string::npos);
  t.settings = std::nullopt;
  t.secrets = "[keys]\ncloud = \"sk-invented-\xC3\xA9\" \xC3\xA9\n";
  const std::string secret_error = error_of(t);
  CHECK(secret_error.find("secrets.toml: line 2: ") != std::string::npos);
  CHECK(secret_error.find("sk-invented") == std::string::npos);
  // A letter outside ASCII inside a quoted string, and in a comment, is the player's to write.
  t.secrets = kSecrets;
  t.settings = "# r\xC3\xA9glages\nbudget = { requests_per_hour = 700 }\n";
  CHECK(parse_config(t, no_env).requests_per_hour == 700);
}
