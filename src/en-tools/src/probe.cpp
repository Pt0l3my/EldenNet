// eldennet-engine probe and models: the local probes and a provider's model list (roadmap S8.1.g; ARCH §11.4).
//
//   eldennet-engine probe [--json]
//   eldennet-engine models --config <dir> [--provider <id>] [--models <models.toml>] [--json]
//
// probe asks the usual model-server ports of this PC and needs no configuration; models asks every configured provider,
// or the one named, for its model list. Both exit 0 once they have run, whatever they found; 2 on a usage or
// configuration error; 1 when this build has no HTTP transport.
#include <cstddef>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/config.hpp"
#include "en/llm/probe.hpp"
#include "en/llm/transport.hpp"

namespace {

using nlohmann::json;

// The servers' own names, for people to read; JSON keeps the configuration's flavour words.
const char* owner_name(en::llm::Flavor flavor) {
  switch (flavor) {
    case en::llm::Flavor::LmStudio: return "LM Studio";
    case en::llm::Flavor::Ollama: return "Ollama";
    case en::llm::Flavor::LlamaCpp: return "llama.cpp";
    case en::llm::Flavor::KoboldCpp: return "KoboldCpp";
    case en::llm::Flavor::OpenAI: break;
  }
  return "an OpenAI-compatible server";
}

std::string count_text(std::size_t count, std::size_t omitted) {
  std::string text = std::to_string(count) + (count == 1 ? " model" : " models");
  if (omitted > 0) text += " (" + std::to_string(omitted) + " more left out)";
  return text;
}

void probe_usage() { std::fputs("usage: eldennet-engine probe [--json]\n", stderr); }

void models_usage() {
  std::fputs("usage: eldennet-engine models --config <dir> [--provider <id>] [--models <models.toml>] [--json]\n", stderr);
}

void print_endpoints(const std::vector<en::llm::LocalEndpoint>& endpoints) {
  if (endpoints.empty()) {
    std::string ports;
    for (const auto& p : en::llm::probe_ports()) ports += (ports.empty() ? "" : ", ") + std::to_string(p.port);
    std::printf("no local model server answered (ports %s)\n", ports.c_str());
    return;
  }
  for (const auto& e : endpoints) {
    const std::string confirmation =
        e.confirmed ? "confirmed" : std::string("not confirmed (usually ") + owner_name(e.hint) + ")";
    std::printf("port %d: %s, %s, %s\n", e.port, en::llm::to_string(e.flavor), confirmation.c_str(),
                count_text(e.models.size(), e.models_omitted).c_str());
  }
}

json endpoints_json(const std::vector<en::llm::LocalEndpoint>& endpoints) {
  json list = json::array();
  for (const auto& e : endpoints) {
    list.push_back({{"port", e.port},
                    {"base_url", e.base_url},
                    {"flavor", en::llm::to_string(e.flavor)},
                    {"confirmed", e.confirmed},
                    {"hint", en::llm::to_string(e.hint)},
                    {"models", e.models},
                    {"models_omitted", e.models_omitted}});
  }
  return json{{"endpoints", list}};
}

struct ModelsArgs {
  std::string config;
  std::optional<std::string> provider;
  std::optional<std::string> models;
  bool json = false;
};

bool parse_models_args(int argc, char** argv, ModelsArgs& args) {
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    if (flag == "--json") {
      args.json = true;
      continue;
    }
    if (flag != "--config" && flag != "--provider" && flag != "--models") {
      std::fprintf(stderr, "models: unknown option %s\n", flag.c_str());
      return false;
    }
    if (i + 1 >= argc) {
      std::fprintf(stderr, "models: %s needs a value\n", flag.c_str());
      return false;
    }
    const char* v = argv[++i];
    if (flag == "--config") {
      args.config = v;
    } else if (flag == "--provider") {
      args.provider = v;
    } else {
      args.models = v;
    }
  }
  if (args.config.empty()) {
    std::fputs("models: --config is required\n", stderr);
    return false;
  }
  return true;
}

void print_listing(const en::llm::ProviderConfig& provider, const en::llm::ModelsResult& r) {
  if (!r.ok()) {
    std::printf("%s: %s, %s\n", provider.id.c_str(), en::llm::to_string(r.error), r.message.c_str());
    return;
  }
  std::printf("%s: %s\n", provider.id.c_str(), count_text(r.models.size(), r.omitted).c_str());
  for (const std::string& id : r.models) std::printf("  %s\n", id.c_str());
}

json listing_json(const en::llm::ProviderConfig& provider, const en::llm::ModelsResult& r) {
  return {{"provider", provider.id},
          {"error", r.ok() ? json(nullptr) : json(en::llm::to_string(r.error))},
          {"http_status", r.http_status},
          {"message", r.message},
          {"models", r.models},
          {"models_omitted", r.omitted}};
}

}  // namespace

int run_probe(int argc, char** argv) {
  bool as_json = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) != "--json") {
      std::fprintf(stderr, "probe: unknown option %s\n", argv[i]);
      probe_usage();
      return 2;
    }
    as_json = true;
  }
  const auto transport = en::llm::make_default_transport_factory();
  if (!transport) {
    std::fputs("probe: this build has no HTTP transport\n", stderr);
    return 1;
  }
  const std::vector<en::llm::LocalEndpoint> endpoints = en::llm::probe_local(*transport);
  if (as_json) {
    std::puts(endpoints_json(endpoints).dump(2).c_str());
  } else {
    print_endpoints(endpoints);
  }
  return 0;
}

int run_models(int argc, char** argv) {
  ModelsArgs args;
  if (!parse_models_args(argc, argv, args)) {
    models_usage();
    return 2;
  }
  en::llm::Config config;
  try {
    config = en::llm::load_config_dir(args.config, args.models);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "models: %s\n", e.what());
    return 2;
  }
  for (const auto& warning : config.warnings) std::fprintf(stderr, "[models] note: %s\n", warning.c_str());

  std::vector<const en::llm::ProviderConfig*> asked;
  if (args.provider) {
    const en::llm::ProviderConfig* provider = config.provider(*args.provider);
    if (provider == nullptr) {
      std::fprintf(stderr, "models: the configuration has no provider \"%s\"\n", args.provider->c_str());
      return 2;
    }
    asked.push_back(provider);
  } else {
    for (const auto& provider : config.providers) asked.push_back(&provider);
  }
  if (asked.empty()) {
    std::fputs("models: the configuration names no provider\n", stderr);
    return 2;
  }
  const auto transport = en::llm::make_default_transport_factory();
  if (!transport) {
    std::fputs("models: this build has no HTTP transport\n", stderr);
    return 1;
  }

  json out = {{"providers", json::array()}};
  for (const en::llm::ProviderConfig* provider : asked) {
    const en::llm::ModelsResult r = en::llm::list_models(*transport, *provider);
    if (args.json) {
      out["providers"].push_back(listing_json(*provider, r));
    } else {
      print_listing(*provider, r);
    }
  }
  if (args.json) std::puts(out.dump(2).c_str());
  return 0;
}
