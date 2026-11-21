#pragma once
// The local probes and the model list (ARCH §11.4, ER06 §5.2): which model servers answer on this PC, and which
// models a configured provider lists. Both are free functions that block until they end, on the caller's thread.
//
// The caller (the wizard and the F10 "Find local servers" button) runs them, and never while the offline gate is
// closed (ARCH §11.4, §5.1). They do not know the gate: the caller checks it before it calls, and stops them with the
// stop token when the gate closes.
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "en/llm/clock.hpp"
#include "en/llm/config.hpp"
#include "en/llm/errors.hpp"
#include "en/llm/transport.hpp"

namespace en::llm {

// What is kept of a model list, so that no server can flood memory or the screens that show it.
struct ModelListLimits {
  std::size_t max_models = 1000;          // entries past this are left out
  std::size_t max_id_bytes = 256;         // a longer id is left out, never cut: a cut id names no model
  std::size_t max_body_bytes = 8u << 20;  // a longer answer is not read at all
};

struct ModelList {
  std::vector<std::string> ids;  // in the server's order
  // Entries left out: past the count, with an id too long or holding control characters, or with no id at all.
  std::size_t omitted = 0;
};

// Reads an OpenAI-style model list, {"data": [{"id": "..."}, ...]}. An empty list is a list, and so is Ollama's
// "data": null, which is how it writes an empty one. Anything else is not, nor is a list none of whose entries has
// an id.
std::optional<ModelList> parse_model_list(std::string_view body, const ModelListLimits& limits = {});

// A port the probe asks, and the server that usually listens there.
struct ProbePort {
  int port = 0;
  Flavor usual_owner = Flavor::OpenAI;
};

// ARCH §11.4's order: LM Studio 1234, Ollama 11434, llama.cpp 9931 then the legacy 8080, KoboldCpp 5001. A constant,
// so no static object is made or destroyed for it (ARCH §3.1, §1.7).
inline constexpr std::array<ProbePort, 5> kProbePorts = {{{1234, Flavor::LmStudio},
                                                         {11434, Flavor::Ollama},
                                                         {9931, Flavor::LlamaCpp},
                                                         {8080, Flavor::LlamaCpp},  // llama.cpp's port before 9931
                                                         {5001, Flavor::KoboldCpp}}};
constexpr std::span<const ProbePort> probe_ports() { return kProbePorts; }

struct LocalEndpoint {
  int port = 0;
  std::string base_url;             // http://127.0.0.1:<port>/v1, as providers.toml takes it
  Flavor flavor = Flavor::OpenAI;   // Ollama or LlamaCpp only when confirmed; otherwise OpenAI
  bool confirmed = false;           // the server answered as its flavour on that flavour's own endpoint
  Flavor hint = Flavor::OpenAI;     // the port's usual owner, for the wizard to show: never assumed, never acted on
  std::vector<std::string> models;  // the ids it lists, within the limits
  std::size_t models_omitted = 0;   // as ModelList::omitted
};

struct ProbeOptions {
  Millis connect_timeout{500};  // ARCH §11.4 (impl)
  // One deadline for the whole probe, from its start: whatever has not answered by then is cancelled, the flavour
  // checks included (ARCH §11.4, impl). A server whose check comes too late stays unconfirmed.
  Millis total{1500};
  ModelListLimits limits{1000, 256, 1u << 20};
  std::shared_ptr<Clock> clock;  // the system's steady clock when null
  Millis poll_slice{50};         // the longest the probe goes without looking at the stop token
};

// Asks every port of probe_ports() at once, on one transport, for GET http://127.0.0.1:<port>/v1/models; nothing but
// 127.0.0.1 and the three paths named here is ever contacted, and no key or header of any provider is sent. A port
// counts only if it answers 200 with a model list (parse_model_list); anything else is no endpoint. A port that
// counts is then asked GET /api/version and GET /props: the flavour is Ollama only if the first answers as Ollama
// does (a JSON object with a "version" string), LlamaCpp only if the second answers as llama.cpp's server does (a JSON
// object with its "default_generation_settings" object, "total_slots" number and "build_info" string), and neither if
// both or none do. LM Studio and KoboldCpp have no such check, so they are never confirmed: their ports give the hint.
//
// Returns one endpoint per port that counts, in probe_ports()'s order whatever order the answers came in. At the
// deadline, or when `stop` is requested, it cancels what is still running and returns the ports that had answered
// by then. Returns nothing when the factory gives no transport.
std::vector<LocalEndpoint> probe_local(TransportFactory& transports, const ProbeOptions& options = {},
                                       std::stop_token stop = {});

struct ListModelsOptions {
  Millis connect_timeout{3000};  // as a chat request's (ARCH §11.1, impl)
  Millis total{10000};           // (impl)
  ModelListLimits limits;
  std::shared_ptr<Clock> clock;  // the system's steady clock when null
  Millis poll_slice{50};         // the longest it goes without looking at the stop token
};

struct ModelsResult {
  ErrorClass error = ErrorClass::None;
  int http_status = 0;
  // What went wrong, in our own words: never a key, a URL, a host, a header's value or the server's text.
  std::string message;
  std::vector<std::string> models;  // the ids, within the limits
  std::size_t omitted = 0;          // as ModelList::omitted

  bool ok() const { return error == ErrorClass::None; }
};

// GET <base_url>/models with the provider's headers and its key in its own header (build_models_request), within
// the options' time. An HTTP error gives classify_http_status's class (401 and 403 are Auth); a failed connection or
// an answer that is not a model list is Server; no answer in time is TotalTimeout; a stop is Cancelled. A provider
// whose key did not resolve is Auth, and nothing is sent.
ModelsResult list_models(TransportFactory& transports, const ProviderConfig& provider,
                         const ListModelsOptions& options = {}, std::stop_token stop = {});

}  // namespace en::llm
