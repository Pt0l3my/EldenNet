// The AI pages' parts of en-llm: the handle's model lists, the self-test's run and its report, and the edits of
// providers.toml and secrets.toml. Invented providers (".invalid" hosts), models and keys only.
#include <doctest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "en/llm/edit.hpp"
#include "en/llm/selftest.hpp"
#include "en/llm/service.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using std::chrono::milliseconds;

namespace {

constexpr Millis kPatience{60000};

bool eventually(const std::function<bool()>& condition) {
  const auto give_up = std::chrono::steady_clock::now() + kPatience;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > give_up) return false;
    std::this_thread::sleep_for(milliseconds(1));
  }
  return true;
}

FakeScript models_of(const std::vector<std::string>& ids, int status = 200) {
  json data = json::array();
  for (const std::string& id : ids) data.push_back({{"id", id}, {"object", "model"}});
  return http_error(status, json{{"object", "list"}, {"data", data}}.dump(), {}, Millis(40));
}

struct Run {
  std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
  std::shared_ptr<FakeServer> server = std::make_shared<FakeServer>(clock);
  std::vector<std::string> lines;
  std::mutex lines_mutex;
  std::unique_ptr<Service> service;

  explicit Run(const std::string& providers = kCloud, const std::string& secrets = kSecrets) {
    ServiceOptions options;
    options.client.clock = clock;
    options.client.log = [this](LogLevel, const std::string& line) {
      std::lock_guard lock(lines_mutex);
      lines.push_back(line);
    };
    service = std::make_unique<Service>(make_config(providers, secrets), server, std::move(options));
  }

  // Drives a run as the DLL's core thread does: a step, then each event handed over, until the run is done.
  void drive(SelfTest& run) {
    run.step(*service);
    while (!run.done()) {
      std::optional<LlmEvent> e = service->wait(kPatience);
      REQUIRE(e.has_value());
      CHECK(run.take(*e));
      run.step(*service);
    }
  }
};

}  // namespace

TEST_CASE("service: a provider's model list runs on a worker and ends with one ModelsEnded event") {
  Run rig;
  rig.server->script("", models_of({"model-a", "model-b", "model-c"}));
  const std::optional<RequestId> id = rig.service->list_models("cloud");
  REQUIRE(id.has_value());
  const std::optional<LlmEvent> e = rig.service->wait(kPatience);
  REQUIRE(e.has_value());
  CHECK(e->kind == EventKind::ModelsEnded);
  CHECK(e->terminal());
  CHECK(e->id == *id);
  CHECK(e->provider == "cloud");
  CHECK(e->models.error == ErrorClass::None);
  CHECK(e->models.status == 200);
  CHECK(e->models.models == std::vector<std::string>{"model-a", "model-b", "model-c"});
  const std::vector<Recorded> sent = rig.server->requests();
  REQUIRE(sent.size() == 1);
  CHECK(sent[0].method == "GET");
  CHECK(sent[0].url == "https://llm.example.invalid/api/v1/models");
  CHECK(sent[0].authorization == "Bearer invented-test-key");
  CHECK(rig.service->client().requests_last_hour() == 0);  // a list is no request of the hour's budget
  for (const std::string& line : rig.lines) {
    CHECK(line.find("invented-test-key") == std::string::npos);
    CHECK(line.find("example.invalid") == std::string::npos);
    CHECK(line.find("model-a") == std::string::npos);  // what a server lists is never logged
  }
}

TEST_CASE("service: a model list for no such provider is not taken; a closed gate ends one at once") {
  Run rig;
  CHECK_FALSE(rig.service->list_models("nobody").has_value());
  rig.service->set_gate_open(false);
  const std::optional<RequestId> id = rig.service->list_models("cloud");
  REQUIRE(id.has_value());
  const std::optional<LlmEvent> e = rig.service->wait(kPatience);
  REQUIRE(e.has_value());
  CHECK(e->kind == EventKind::ModelsEnded);
  CHECK(e->models.error == ErrorClass::GateClosed);
  CHECK(rig.server->requests().empty());

  // One that runs stops when the gate closes.
  rig.service->set_gate_open(true);
  rig.server->set_auto_advance(false);
  rig.server->script("", hang());
  const std::optional<RequestId> running = rig.service->list_models("cloud");
  REQUIRE(running.has_value());
  REQUIRE(eventually([&] { return rig.server->active() == 1; }));
  rig.service->set_gate_open(false);
  const std::optional<LlmEvent> stopped = rig.service->wait(kPatience);
  REQUIRE(stopped.has_value());
  CHECK(stopped->id == *running);
  CHECK(stopped->models.error == ErrorClass::GateClosed);
}

TEST_CASE("service: a rejected key ends the list as Auth") {
  Run rig;
  rig.server->script("", http_error(401, R"({"error": {"message": "an invented refusal"}})"));
  REQUIRE(rig.service->list_models("cloud").has_value());
  const std::optional<LlmEvent> e = rig.service->wait(kPatience);
  REQUIRE(e.has_value());
  CHECK(e->models.error == ErrorClass::Auth);
  CHECK(e->models.status == 401);
  CHECK(e->models.models.empty());
}

TEST_CASE("self-test: lists, the latency check and the JSON check, with the verdict from the measured figures") {
  Run rig;
  rig.server->script("", models_of({"model-a", "model-b"}));
  // selftest and memory borrow dialogue's model-a: the latency reply first, then the JSON answer.
  rig.server->script("model-a", sse_reply({"A lantern ", "glows by ", "the road."}, "stop", Millis(300), Millis(100),
                                          json{{"prompt_tokens", 30}, {"completion_tokens", 9}}));
  rig.server->script("model-a", sse_reply({R"({"place": "harbour", )", R"("count": 4})"}));
  SelfTest run(*rig.service->client().config(), false, 1000);
  rig.drive(run);
  const SelfTestReport& r = run.report();
  REQUIRE(r.lists.size() == 1);
  CHECK(r.lists[0].provider == "cloud");
  CHECK(r.lists[0].state == CheckState::Passed);
  CHECK(r.lists[0].listed == 2);
  CHECK(r.lists[0].missing.empty());
  CHECK(r.latency == CheckState::Passed);
  CHECK(r.provider == "cloud");
  CHECK(r.model == "model-a");
  CHECK(r.first_token == Millis(300));
  CHECK(r.tokens == 9);
  // Nine tokens over the 300 ms from the first delta to the end: 30 a second; the first page is 300 ms + 15 / 30 s.
  CHECK(r.tokens_per_second == doctest::Approx(30.0));
  CHECK(r.first_page == Millis(800));
  CHECK(r.verdict == Verdict::Quick);
  CHECK(r.json == CheckState::Passed);
  CHECK(r.done());
  CHECK(r.passed());

  const std::vector<Recorded> sent = rig.server->requests();
  REQUIRE(sent.size() == 3);
  CHECK(sent[1].body["max_tokens"] == 32);  // the selftest slot's budget
  CHECK(sent[2].body.contains("response_format"));  // the schema, on a provider that takes one
}

TEST_CASE("self-test: a provider that does not answer its list skips the requests on it; a missing model fails it") {
  SUBCASE("the key refused") {
    Run rig;
    rig.server->script("", http_error(403, "{}"));
    SelfTest run(*rig.service->client().config(), false, 10);
    rig.drive(run);
    const SelfTestReport& r = run.report();
    CHECK(r.lists[0].state == CheckState::Failed);
    CHECK(r.lists[0].error == ErrorClass::Auth);
    CHECK(r.latency == CheckState::Skipped);
    CHECK(r.json == CheckState::Skipped);
    CHECK(r.verdict == Verdict::None);
    CHECK_FALSE(r.passed());
    CHECK(rig.server->requests().size() == 1);
  }
  SUBCASE("the slot's model not listed") {
    Run rig;
    rig.server->script("", models_of({"model-b"}));
    rig.server->script("model-a", sse_reply({"Fine."}));
    rig.server->script("model-a", sse_reply({R"({"place": "mill", "count": 2})"}));
    SelfTest run(*rig.service->client().config(), false, 10);
    rig.drive(run);
    const SelfTestReport& r = run.report();
    CHECK(r.lists[0].state == CheckState::Failed);
    CHECK(r.lists[0].error == ErrorClass::None);
    CHECK(r.lists[0].missing == std::vector<std::string>{"dialogue"});  // the slots that borrow it add nothing
    CHECK(r.latency == CheckState::Passed);  // it answered: the latency check still runs
    CHECK(r.json == CheckState::Passed);
  }
  SUBCASE("the lists alone") {
    Run rig(kTwoClouds);
    rig.server->script("", models_of({"model-a"}));
    rig.server->script("", models_of({"model-b"}));
    SelfTest run(*rig.service->client().config(), true, 10);
    rig.drive(run);
    const SelfTestReport& r = run.report();
    REQUIRE(r.lists.size() == 2);
    CHECK(r.lists[0].provider == "cloud");
    CHECK(r.lists[1].provider == "other");
    CHECK(r.done());
    CHECK(r.latency == CheckState::Waiting);  // not part of a model list
    CHECK(rig.server->requests().size() == 2);
  }
}

TEST_CASE("self-test: the JSON check takes the object asked for, and nothing else") {
  CHECK(SelfTest::json_answer_ok(R"({"place": "harbour", "count": 4})"));
  CHECK(SelfTest::json_answer_ok("```json\n{\"place\": \"harbour\", \"count\": 4}\n```"));
  CHECK_FALSE(SelfTest::json_answer_ok(R"({"place": "harbour"})"));
  CHECK_FALSE(SelfTest::json_answer_ok(R"({"place": "harbour", "count": "4"})"));
  CHECK_FALSE(SelfTest::json_answer_ok(R"({"place": "", "count": 4})"));
  CHECK_FALSE(SelfTest::json_answer_ok(R"({"place": "harbour", "count": 4, "more": 1})"));
  CHECK_FALSE(SelfTest::json_answer_ok("Here it is: {\"place\": \"harbour\", \"count\": 4}"));
  CHECK_FALSE(SelfTest::json_answer_ok(""));
}

TEST_CASE("self-test: the verdict's bounds are C5's") {
  CHECK(verdict_of(Millis(1600)) == Verdict::Quick);
  CHECK(verdict_of(Millis(1601)) == Verdict::Acceptable);
  CHECK(verdict_of(Millis(3000)) == Verdict::Acceptable);
  CHECK(verdict_of(Millis(3001)) == Verdict::Slow);
}

TEST_CASE("self-test: a run cancelled midway reads Skipped where it did not finish") {
  Run rig;
  rig.server->set_auto_advance(false);
  rig.server->script("", hang());
  SelfTest run(*rig.service->client().config(), false, 10);
  run.step(*rig.service);
  REQUIRE(eventually([&] { return rig.server->active() == 1; }));
  run.cancel(*rig.service);
  CHECK(run.done());
  CHECK(run.report().lists[0].state == CheckState::Skipped);
  CHECK(run.report().latency == CheckState::Skipped);
}

// ---- The edits.

TEST_CASE("edit_providers: a new file, a preset's provider added, the preset set; checked by parse_config") {
  ProvidersEdit edit;
  edit.preset = "custom";
  edit.add.push_back(ProviderEntry{"router", Flavor::OpenAI, ProviderKind::Cloud,
                                   "https://router.example.invalid/api/v1/", "secrets:router", JsonMode::JsonSchema});
  const EditedText made = edit_providers("", edit);
  REQUIRE(made.ok);
  CHECK(made.text.find("schema = 1") != std::string::npos);
  CHECK(made.text.find("[[provider]]") != std::string::npos);
  ConfigTexts texts;
  texts.providers = made.text;
  texts.secrets = "[keys]\nrouter = \"invented-router-key\"\n";
  const Config cfg = parse_config(texts, no_env);
  REQUIRE(cfg.providers.size() == 1);
  CHECK(cfg.providers[0].id == "router");
  CHECK(cfg.providers[0].base_url == "https://router.example.invalid/api/v1");
  CHECK(cfg.providers[0].json_mode == JsonMode::JsonSchema);
  CHECK(cfg.providers[0].key.has_value());
  CHECK(cfg.preset == "custom");

  // Adding the same id again is refused; another is appended, every other key kept.
  CHECK(edit_providers(made.text, edit).why == "duplicate_id");
  ProvidersEdit more;
  more.add.push_back(ProviderEntry{"second", Flavor::OpenAI, ProviderKind::Cloud, "https://second.example.invalid/v1",
                                   "secrets:second", JsonMode::None});
  const EditedText both = edit_providers(made.text, more);
  REQUIRE(both.ok);
  texts.providers = both.text;
  CHECK(parse_config(texts, no_env).providers.size() == 2);
}

TEST_CASE("edit_providers: a removed provider takes the [slots.*] that name it; a refused file is never written") {
  const EditedText removed = edit_providers(kTwoClouds, ProvidersEdit{std::nullopt, {}, {"other"}});
  REQUIRE(removed.ok);
  CHECK(removed.text.find("other.example.invalid") == std::string::npos);
  CHECK(removed.text.find("[slots") == std::string::npos);  // dialogue's slot named "other" for its fallback
  ConfigTexts texts;
  texts.providers = removed.text;
  texts.secrets = kSecrets;
  CHECK(parse_config(texts, no_env).providers.size() == 1);
  CHECK(edit_providers(kTwoClouds, ProvidersEdit{std::nullopt, {}, {"nobody"}}).why == "unknown_id");
  CHECK(edit_providers("schema = 1\n[provider\n", ProvidersEdit{}).why == "refused");
  CHECK(edit_providers("schema = 1\nprovider = 3\n", ProvidersEdit{std::nullopt, {}, {"x"}}).why == "not_a_table");
}

TEST_CASE("preset_providers: the providers a preset names that providers.toml lacks, each with its own key") {
  const std::string models = R"(
schema = 1

[preset.two.providers.main]
flavor = "openai"
kind = "cloud"
base_url = "https://main.example.invalid/api/v1"

[preset.two.providers.fast]
base_url = "https://fast.example.invalid/v1"

[preset.two]
dialogue = { provider = "main", model = "talker" }
bark_live = { provider = "fast", model = "quick" }

[preset.one]
provider = { flavor = "openai", kind = "cloud", base_url = "https://one.example.invalid/v1/" }
dialogue = { model = "talker" }

[preset.near]
provider = { kind = "local" }
all = { model_hint = ["small"] }
)";
  CHECK(preset_ids(models) == std::vector<std::string>{"near", "one", "two"});  // toml++ keeps them by name
  const std::vector<ProviderEntry> two = preset_providers(models, "two", {});
  REQUIRE(two.size() == 2);
  // toml++ keeps a table's keys in order: fast before main.
  const auto main = std::find_if(two.begin(), two.end(), [](const ProviderEntry& e) { return e.id == "main"; });
  REQUIRE(main != two.end());
  CHECK(main->key_ref == "secrets:main");
  CHECK(main->kind == ProviderKind::Cloud);
  const std::vector<ProviderEntry> one = preset_providers(models, "one", {});
  REQUIRE(one.size() == 1);
  CHECK(one[0].id == "one");
  CHECK(one[0].base_url == "https://one.example.invalid/v1");

  // Matched by base_url: nothing to add once providers.toml has it, and the preset's configuration then loads.
  ProvidersEdit add{std::string("two"), two, {}};
  const EditedText written = edit_providers("", add);
  REQUIRE(written.ok);
  ConfigTexts texts;
  texts.providers = written.text;
  texts.models = models;
  const Config cfg = parse_config(texts, no_env);
  CHECK(preset_providers(models, "two", cfg.providers).empty());
  CHECK(cfg.slot(kDialogue).models.front().provider == "main");
  CHECK(cfg.slot(kBarkLive).models.front().provider == "fast");
  CHECK(unused_providers(cfg).empty());

  CHECK_THROWS_AS((void)preset_providers(models, "missing", {}), ConfigError);
  CHECK_THROWS_AS((void)preset_providers(models, "near", {}), ConfigError);  // the probe finds a local server
}

TEST_CASE("unused_providers and secret_names") {
  const std::string three = std::string(kTwoClouds) + R"(
[[provider]]
id = "idle"
base_url = "https://idle.example.invalid/v1"
key_ref = "secrets:idle"
)";
  ConfigTexts texts;
  texts.providers = three;
  texts.secrets = kSecrets;
  const Config cfg = parse_config(texts, no_env);
  CHECK(unused_providers(cfg) == std::vector<std::string>{"idle"});
  CHECK(secret_names(cfg.providers) == std::vector<std::string>{"cloud", "idle", "other"});
}

TEST_CASE("edit_secrets: a key set and cleared, every other kept, never in a reason") {
  const Secret key("invented-new-key");
  const EditedSecrets set = edit_secrets(kSecrets, "router", &key);
  REQUIRE(set.ok);
  CHECK(set.text.reveal().find("router = 'invented-new-key'") != std::string::npos);
  CHECK(set.text.reveal().find("invented-test-key") != std::string::npos);
  const EditedSecrets cleared = edit_secrets(set.text.reveal(), "cloud", nullptr);
  REQUIRE(cleared.ok);
  CHECK(cleared.text.reveal().find("invented-test-key") == std::string::npos);
  CHECK(cleared.text.reveal().find("invented-new-key") != std::string::npos);
  const EditedSecrets fresh = edit_secrets("", "router", &key);
  REQUIRE(fresh.ok);
  ConfigTexts texts;
  texts.providers = R"(
schema = 1
[[provider]]
id = "router"
base_url = "https://router.example.invalid/v1"
key_ref = "secrets:router"
)";
  texts.secrets = fresh.text.reveal();
  CHECK(parse_config(texts, no_env).providers[0].key->reveal() == "invented-new-key");
  const EditedSecrets bad = edit_secrets("[keys\nx = \"invented-test-key\"\n", "router", &key);
  CHECK_FALSE(bad.ok);
  CHECK(bad.why == "refused");
  CHECK(edit_secrets(kSecrets, "a name", &key).why == "bad_name");
  CHECK(edit_secrets("keys = 1\n", "router", &key).why == "not_a_table");
  CHECK(keys_set(set.text.reveal()) == std::vector<std::string>{"cloud", "other", "router"});
  CHECK(keys_set("[keys]\nempty = \"\"\n").empty());
  CHECK(keys_set("[keys\n").empty());
}

TEST_CASE("clean_key: a pasted key's spaces and line end go; a key with spaces or controls inside is none") {
  CHECK(clean_key("  invented-key\r\n")->reveal() == "invented-key");
  CHECK_FALSE(clean_key("").has_value());
  CHECK_FALSE(clean_key(" \n").has_value());
  CHECK_FALSE(clean_key("two words").has_value());
  CHECK_FALSE(clean_key(std::string("with\x01" "control")).has_value());
  CHECK_FALSE(clean_key(std::string(1025, 'k')).has_value());
  CHECK(clean_key(std::string(1024, 'k')).has_value());
}
