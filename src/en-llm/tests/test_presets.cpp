// Presets with several providers, and the reserved slots (O88; ARCH §11.1, §11.4): both forms of a preset's
// providers, every refusal with its file and line, the provider providers.toml's [slots] take from a preset, the
// reserved slots read and checked but used by no request, and each slot's requests reaching its own provider, with its
// own key, through the fake transport. Endpoints, keys and models are invented.
#include <doctest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/llm/config.hpp"
#include "en/llm/decision.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;

namespace {

constexpr const char* kPresetKeys = "[keys]\nrouter = \"invented-router-key\"\nnano = \"invented-nano-key\"\n";

// Two cloud providers, each with its own key and its own reasoning switch. `more` starts on line 15.
std::string two_providers(const std::string& preset, const std::string& more) {
  return "schema = 1\npreset = \"" + preset + "\"\n" + R"(
[[provider]]
id = "router"
base_url = "https://router.example.invalid/api/v1"
key_ref = "secrets:router"
extra_body = { reasoning = { enabled = false } }

[[provider]]
id = "nano"
base_url = "https://nano.example.invalid/v1"
key_ref = "secrets:nano"
extra_body = { reasoning_effort = "none" }
)" + more;
}

// Both forms in one file, as models.toml ships them. A preset's names are its own: "main" and "fast" match the
// providers above by base_url.
constexpr const char* kPresetModels = R"(
schema = 1

[preset.one-provider]
provider = { flavor = "openai", kind = "cloud", base_url = "https://router.example.invalid/api/v1" }
dialogue  = { model = "talker", fallback = "talker-big" }
bark_live = { model = "barker" }

[preset.two-clouds.providers.main]
flavor = "openai"
kind = "cloud"
base_url = "https://router.example.invalid/api/v1/"

[preset.two-clouds.providers.fast]
base_url = "https://nano.example.invalid/v1"

[preset.two-clouds]
dialogue  = { provider = "main", model = "talker", fallback = "talker-elsewhere", fallback_provider = "fast" }
bark_live = { provider = "fast", model = "barker" }
bark_pool = { provider = "fast", model = "pooler" }
memory    = { provider = "main", model = "rememberer" }
decisions = { provider = "fast", model = "decider" }
actions   = { provider = "fast", model = "actor" }

[preset.split.providers.main]
base_url = "https://router.example.invalid/api/v1"

[preset.split.providers.fast]
base_url = "https://nano.example.invalid/v1"

[preset.split]
dialogue  = { provider = "main", model = "talker" }
bark_live = { provider = "fast", model = "barker" }

[preset.named-one.providers.only]
base_url = "https://nano.example.invalid/v1"

[preset.named-one]
dialogue = { model = "talker" }
memory   = { provider = "only", model = "rememberer" }

[preset.same-name.providers.nano]
base_url = "https://nano.example.invalid/v1"

[preset.same-name]
dialogue = { provider = "nano", model = "talker" }
)";

Config preset_config(const std::string& preset, const std::string& more = "",
                     const std::string& models = kPresetModels) {
  ConfigTexts t;
  t.providers = two_providers(preset, more);
  t.models = models;
  t.secrets = kPresetKeys;
  return parse_config(t, no_env);
}

std::string refusal_of(const std::string& models, const std::string& more = "", const std::string& preset = "p") {
  try {
    preset_config(preset, more, models);
  } catch (const ConfigError& e) {
    return e.what();
  }
  return "";
}

using Refs = std::vector<std::pair<std::string, std::string>>;  // provider, model

Refs refs_of(const std::vector<ModelRef>& models) {
  Refs out;
  for (const ModelRef& m : models) out.emplace_back(m.provider, m.model);
  return out;
}

constexpr auto npos = std::string::npos;

// models.toml's first lines for a preset `p` that names one provider, `main`; the slots start on line 5.
const std::string kNamedHead =
    "schema = 1\n[preset.p.providers.main]\nbase_url = \"https://router.example.invalid/api/v1\"\n[preset.p]\n";
// The same with two, `main` and `fast`; the slots start on line 7.
const std::string kTwoHead =
    "schema = 1\n[preset.p.providers.main]\nbase_url = \"https://router.example.invalid/api/v1\"\n"
    "[preset.p.providers.fast]\nbase_url = \"https://nano.example.invalid/v1\"\n[preset.p]\n";

}  // namespace

TEST_CASE("a preset may name several providers: each slot names its own, and its fallback's") {
  const Config cfg = preset_config("two-clouds");
  CHECK(refs_of(cfg.slot("dialogue").models) == Refs{{"router", "talker"}, {"nano", "talker-elsewhere"}});
  CHECK(refs_of(cfg.slot("bark_live").models) == Refs{{"nano", "barker"}});
  CHECK(refs_of(cfg.slot("bark_pool").models) == Refs{{"nano", "pooler"}});
  CHECK(refs_of(cfg.slot("memory").models) == Refs{{"router", "rememberer"}});
  CHECK(refs_of(cfg.slot("decisions").models) == Refs{{"nano", "decider"}});
  // A slot the preset leaves out borrows as before, providers and all: draft and the self-test take dialogue's.
  CHECK(refs_of(cfg.slot("draft").models) == refs_of(cfg.slot("dialogue").models));
  CHECK(refs_of(cfg.slot("selftest").models) == refs_of(cfg.slot("dialogue").models));
  // A preset adds no provider of its own, and the keys stay with providers.toml's, by reference into secrets.toml.
  REQUIRE(cfg.providers.size() == 2);
  REQUIRE(cfg.provider("router")->key.has_value());
  CHECK(cfg.provider("router")->key->reveal() == "invented-router-key");
  REQUIRE(cfg.provider("nano")->key.has_value());
  CHECK(cfg.provider("nano")->key->reveal() == "invented-nano-key");
  CHECK(cfg.warnings.empty());
}

TEST_CASE("today's form, one provider = { ... }, still works, beside the named form in the same file") {
  const Config cfg = preset_config("one-provider");
  CHECK(refs_of(cfg.slot("dialogue").models) == Refs{{"router", "talker"}, {"router", "talker-big"}});
  CHECK(refs_of(cfg.slot("bark_live").models) == Refs{{"router", "barker"}});
  CHECK(refs_of(cfg.slot("decisions").models) == Refs{{"router", "barker"}});  // bark_live's, as before
  CHECK(refs_of(cfg.slot("memory").models) == Refs{{"router", "talker"}, {"router", "talker-big"}});
  CHECK(cfg.reserved.at("actions").empty());
  CHECK(cfg.reserved.at("vision").empty());
}

TEST_CASE("a preset that names one provider lets its slots leave it out; a name may be the provider's own id") {
  const Config one = preset_config("named-one");
  CHECK(refs_of(one.slot("dialogue").models) == Refs{{"nano", "talker"}});
  CHECK(refs_of(one.slot("memory").models) == Refs{{"nano", "rememberer"}});
  // The same name for the same provider is no clash: the wizard writes providers.toml so (ARCH §11.7).
  CHECK(refs_of(preset_config("same-name").slot("dialogue").models) == Refs{{"nano", "talker"}});
}

TEST_CASE("providers.toml's [slots] default to the provider the preset gives the slot") {
  const Config cfg = preset_config("split",
                                   "[slots.bark_live]\nmodel = \"barker-2\"\n"
                                   "[slots.decisions]\nmodel = \"decider-2\"\n"
                                   "[slots.draft]\nmodel = \"drafter\"\nfallback = \"drafter-big\"\n"
                                   "[slots.vision]\nmodel = \"seer\"\n"
                                   "[slots.memory]\nmodel = \"rememberer\"\nprovider = \"nano\"\n");
  CHECK(refs_of(cfg.slot("bark_live").models) == Refs{{"nano", "barker-2"}});    // the one the preset gives it
  CHECK(refs_of(cfg.slot("decisions").models) == Refs{{"nano", "decider-2"}});   // bark_live's, which it borrows
  CHECK(refs_of(cfg.slot("draft").models) == Refs{{"router", "drafter"}, {"router", "drafter-big"}});  // dialogue's
  CHECK(refs_of(cfg.reserved.at("vision")) == Refs{{"router", "seer"}});  // a reserved slot: dialogue's too
  CHECK(refs_of(cfg.slot("memory").models) == Refs{{"nano", "rememberer"}});  // named, it wins
  // Without the override, decisions borrows bark_live's model, on bark_live's provider.
  CHECK(refs_of(preset_config("split").slot("decisions").models) == Refs{{"nano", "barker"}});
}

TEST_CASE("refused with the file and line: a slot naming a provider its preset does not define") {
  std::string e = refusal_of(kNamedHead +
                             "dialogue = { provider = \"main\", model = \"m\" }\n"
                             "bark_live = { provider = \"ghost\", model = \"b\" }\n");
  CHECK(e.find("models.toml: preset.p.bark_live: provider \"ghost\" is not a provider the preset defines (line 6)") !=
        npos);
  e = refusal_of(kNamedHead + "dialogue = { model = \"m\", fallback = \"f\", fallback_provider = \"ghost\" }\n");
  CHECK(e.find("preset.p.dialogue: fallback_provider \"ghost\" is not a provider the preset defines (line 5)") != npos);
  // Checked even where the entry is never used: here every slot in use has its own, so `all` is not.
  e = refusal_of(kNamedHead + "all = { provider = \"ghost\", model = \"m\" }\n" +
                 "dialogue = { model = \"m\" }\nbark_live = { model = \"m\" }\nbark_pool = { model = \"m\" }\n"
                 "memory = { model = \"m\" }\nselftest = { model = \"m\" }\ndraft = { model = \"m\" }\n"
                 "decisions = { model = \"m\" }\n");
  CHECK(e.find("preset.p.all: provider \"ghost\" is not a provider the preset defines (line 5)") != npos);
  // In the one-provider form, a slot has no name to give.
  e = refusal_of("schema = 1\n[preset.p]\nprovider = { base_url = \"https://router.example.invalid/api/v1\" }\n"
                 "dialogue = { provider = \"router\", model = \"m\" }\n");
  CHECK(e.find("preset.p.dialogue: provider \"router\" is not a provider the preset defines (its one provider = { ... "
               "} has no name) (line 4)") != npos);
}

TEST_CASE("refused with the file and line: one name for two providers") {
  // Given twice in a preset: TOML itself refuses the second table, at its line.
  std::string e = refusal_of(kNamedHead + "dialogue = { model = \"m\" }\n" +
                             "[preset.p.providers.main]\nbase_url = \"https://nano.example.invalid/v1\"\n");
  CHECK(e.rfind("models.toml: ", 0) == 0);
  CHECK(e.find("cannot redefine existing table 'preset.p.providers.main' (line 6)") != npos);
  // A preset's name that is another provider's id in providers.toml would stand for two.
  e = refusal_of("schema = 1\n[preset.p.providers.nano]\nbase_url = \"https://router.example.invalid/api/v1\"\n"
                 "[preset.p]\ndialogue = { model = \"m\" }\n");
  CHECK(e.find("models.toml: preset.p.providers.nano: matches providers.toml's \"router\", but providers.toml has a "
               "provider \"nano\" too: one name for two providers (line 2)") != npos);
  // And providers.toml's own two providers with one id, as before, at the second one's line.
  e = refusal_of(kNamedHead + "dialogue = { model = \"m\" }\n",
                 "[[provider]]\nid = \"nano\"\nbase_url = \"https://elsewhere.example.invalid/v1\"\n");
  CHECK(e.find("providers.toml: two providers have the id \"nano\" (line 15)") != npos);
}

TEST_CASE("refused with the file and line: a preset with neither form, or with both") {
  std::string e = refusal_of("schema = 1\n[preset.p]\ndialogue = { model = \"m\" }\n");
  CHECK(e.find("models.toml: preset.p: names no provider") != npos);
  CHECK(e.find("(line 2)") != npos);
  e = refusal_of("schema = 1\n[preset.p]\nprovider = { base_url = \"https://router.example.invalid/api/v1\" }\n"
                 "dialogue = { model = \"m\" }\n[preset.p.providers.main]\n"
                 "base_url = \"https://nano.example.invalid/v1\"\n");
  CHECK(e.find("models.toml: preset.p: has both provider = { ... } and providers: use one form (line 3)") != npos);
  e = refusal_of("schema = 1\n[preset.p]\nproviders = {}\ndialogue = { model = \"m\" }\n");
  CHECK(e.find("preset.p: names no provider in its providers table (line 3)") != npos);
}

TEST_CASE("refused too: no provider named among several, a key in a preset, a fallback_provider alone") {
  std::string e = refusal_of(kTwoHead + "dialogue = { model = \"m\" }\n");
  CHECK(e.find("models.toml: preset.p.dialogue: needs provider = \"<name>\": the preset names several providers (line "
               "7)") != npos);
  e = refusal_of(kTwoHead + "all = { model_hint = [\"small-model\"] }\n");
  CHECK(e.find("preset.p.all: needs provider = \"<name>\"") != npos);

  // A preset never holds a key, and the refusal never repeats one.
  e = refusal_of("schema = 1\n[preset.p.providers.main]\nbase_url = \"https://router.example.invalid/api/v1\"\n"
                 "key = \"invented-inline-key\"\n[preset.p]\ndialogue = { model = \"m\" }\n");
  CHECK(e.find("models.toml: preset.p.providers.main: a preset never holds a key") != npos);
  CHECK(e.find("(line 4)") != npos);
  CHECK(e.find("invented-inline-key") == npos);
  e = refusal_of("schema = 1\n[preset.p]\n"
                 "provider = { base_url = \"https://router.example.invalid/api/v1\", key_ref = \"secrets:router\" }\n"
                 "dialogue = { model = \"m\" }\n");
  CHECK(e.find("preset.p.provider: a preset never holds a key") != npos);

  e = refusal_of(kTwoHead + "dialogue = { provider = \"main\", model = \"m\", fallback_provider = \"fast\" }\n");
  CHECK(e.find("preset.p.dialogue: names a fallback_provider but no fallback (line 7)") != npos);
  e = refusal_of(kNamedHead + "dialogue = { model = \"m\" }\n", "", "p");
  CHECK(e.empty());  // the heads themselves are good
}

TEST_CASE("the reserved slots are read and checked in [slots.*] and in presets, and have no budget yet") {
  CHECK(known_slots().size() == 7);
  REQUIRE(reserved_slots().size() == 2);
  CHECK(reserved_slots()[0] == "actions");
  CHECK(reserved_slots()[1] == "vision");
  for (const std::string_view slot : known_slots()) CHECK_FALSE(is_reserved_slot(slot));
  CHECK_FALSE(is_reserved_slot("banter"));

  const Config cfg = preset_config("custom",
                                   "[slots.dialogue]\nprovider = \"router\"\nmodel = \"talker\"\n"
                                   "[slots.actions]\nprovider = \"nano\"\nmodel = \"actor\"\nfallback = \"actor-big\"\n"
                                   "[slots.vision]\nprovider = \"router\"\nmodel = \"seer\"\n");
  CHECK(refs_of(cfg.reserved.at("actions")) == Refs{{"nano", "actor"}, {"nano", "actor-big"}});
  CHECK(refs_of(cfg.reserved.at("vision")) == Refs{{"router", "seer"}});
  CHECK(cfg.slots.count("actions") == 0);
  CHECK(cfg.slots.count("vision") == 0);
  // Until their features exist, a request naming one gets dialogue's slot: its models and its budget.
  CHECK(cfg.slot("actions").name == "dialogue");
  CHECK(cfg.slot("vision").budget.max_tokens == 140);
  // In a preset too, which names actions on its fast provider.
  const Config preset = preset_config("two-clouds");
  CHECK(refs_of(preset.reserved.at("actions")) == Refs{{"nano", "actor"}});
  CHECK(preset.reserved.at("vision").empty());
  CHECK(preset.slot("actions").name == "dialogue");

  // Checked as every slot is.
  std::string e = refusal_of("", "[slots.vision]\nprovider = \"ghost\"\nmodel = \"seer\"\n", "custom");
  CHECK(e.find("providers.toml: slots.vision: names no provider \"ghost\" (line 16)") != npos);
  e = refusal_of("", "[slots.actions]\nmodel = \"actor\"\n", "custom");
  CHECK(e.find("slots.actions: needs provider = \"<id>\": providers.toml has several providers") != npos);
  e = refusal_of(kNamedHead + "dialogue = { model = \"m\" }\nvision = { provider = \"ghost\", model = \"seer\" }\n");
  CHECK(e.find("preset.p.vision: provider \"ghost\" is not a provider the preset defines (line 6)") != npos);
  e = refusal_of(kNamedHead + "dialogue = { model = \"m\" }\nactions = { model = \"actor\", budget = 3 }\n");
  CHECK(e.find("preset.p.actions: unknown key \"budget\" (line 6)") != npos);
}

TEST_CASE("each slot's requests reach its own provider, with that provider's key, through the fake transport") {
  Rig rig(preset_config("two-clouds"));
  rig.server->script("talker", sse_reply({"Spoken through the router."}));
  rig.server->script("pooler", sse_reply({"Pooled through the other."}));
  rig.server->script("rememberer", sse_reply({"Remembered through the router."}));
  rig.server->script("decider", sse_reply({"A"}));

  CHECK(rig.chat("dialogue").provider == "router");
  CHECK(rig.chat("bark_pool", Purpose::Background).provider == "nano");
  CHECK(rig.chat("memory", Purpose::Background).provider == "router");
  DecisionCall call;
  call.task = "next_speaker";
  call.question = "Who answers the line just heard?";
  call.state = {{"place", "an invented hall"}};
  call.options = {{"A", "an invented smith"}, {"none", "silence"}};
  call.rule = "none";
  const DecisionPick pick = rig.client->decide(call);
  CHECK(pick.key == "A");
  CHECK(pick.provider == "nano");
  CHECK(pick.model == "decider");

  const auto requests = rig.server->requests();
  REQUIRE(requests.size() == 4);
  const std::string router = "https://router.example.invalid/api/v1/chat/completions";
  const std::string nano = "https://nano.example.invalid/v1/chat/completions";
  CHECK(requests[0].url == router);
  CHECK(requests[0].authorization == "Bearer invented-router-key");
  CHECK(requests[0].body["reasoning"]["enabled"] == false);  // each provider's own switch
  CHECK(requests[1].url == nano);
  CHECK(requests[1].authorization == "Bearer invented-nano-key");
  CHECK(requests[1].body["reasoning_effort"] == "none");
  CHECK(requests[2].url == router);
  CHECK(requests[2].authorization == "Bearer invented-router-key");
  CHECK(requests[3].url == nano);
  CHECK(requests[3].authorization == "Bearer invented-nano-key");
  CHECK(requests[3].model == "decider");
}

TEST_CASE("a fallback on the preset's other provider reaches that provider") {
  Rig rig(preset_config("two-clouds"));
  rig.server->script("talker", http_error(429, "{}"));
  rig.server->script("talker-elsewhere", sse_reply({"From the other provider."}));
  const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
  CHECK(r.ok());
  CHECK(r.provider == "nano");
  CHECK(r.model == "talker-elsewhere");
  const auto requests = rig.server->requests();
  REQUIRE(requests.size() == 2);
  CHECK(requests[0].url == "https://router.example.invalid/api/v1/chat/completions");
  CHECK(requests[1].url == "https://nano.example.invalid/v1/chat/completions");
  CHECK(requests[1].authorization == "Bearer invented-nano-key");
}

TEST_CASE("a request on a reserved slot falls through to dialogue: its model, its provider and its budget") {
  Rig rig(preset_config("two-clouds"));  // which names actions on its fast provider, with a model of its own
  rig.server->script("talker", sse_reply({"Taken as dialogue."}));
  rig.server->script("talker", sse_reply({"Seen as dialogue."}));
  CHECK(rig.chat("actions").ok());
  CHECK(rig.chat("vision").ok());
  const auto requests = rig.server->requests();
  REQUIRE(requests.size() == 2);
  for (const auto& r : requests) {
    CHECK(r.model == "talker");
    CHECK(r.url == "https://router.example.invalid/api/v1/chat/completions");
    CHECK(r.body["max_tokens"] == 140);
  }
}
