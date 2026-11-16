#pragma once
// Shared test setup: invented providers (".invalid" hosts, which never resolve), invented keys and models, a client
// on the fake transport and a manual clock.
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "en/llm/client.hpp"
#include "en/llm/config.hpp"
#include "fake_transport.hpp"

namespace en::llm::test {

inline constexpr const char* kSecrets = "[keys]\ncloud = \"invented-test-key\"\nother = \"invented-other-key\"\n";

// A cloud provider with the reasoning switch, a routing preference, a JSON schema and a quantisation floor; dialogue
// has a model and a fallback.
inline constexpr const char* kCloud = R"(
schema = 1
preset = "custom"

[[provider]]
id = "cloud"
flavor = "openai"
kind = "cloud"
base_url = "https://llm.example.invalid/api/v1/"
key_ref = "secrets:cloud"
extra_body = { reasoning = { enabled = false }, provider = { sort = "latency" } }
json_mode = "json_schema"
quant_floor = "fp8"

[slots.dialogue]
model = "model-a"
fallback = "model-b"
)";

// The same, with dialogue's fallback on a second cloud provider.
inline constexpr const char* kTwoClouds = R"(
schema = 1

[[provider]]
id = "cloud"
base_url = "https://llm.example.invalid/api/v1"
key_ref = "secrets:cloud"
extra_body = { reasoning = { enabled = false } }

[[provider]]
id = "other"
base_url = "https://other.example.invalid/v1"
key_ref = "secrets:other"
extra_body = { reasoning_effort = "none" }

[slots.dialogue]
provider = "cloud"
model = "model-a"
fallback = "model-b"
fallback_provider = "other"
)";

// A local llama.cpp-style server, one request at a time.
inline constexpr const char* kLocal = R"(
schema = 1

[[provider]]
id = "local"
flavor = "llamacpp"
kind = "local"
base_url = "http://127.0.0.1:9931/v1"
extra_body = { chat_template_kwargs = { enable_thinking = false } }

[slots.dialogue]
model = "local-model"
fallback = "local-fallback"
)";

inline std::optional<std::string> no_env(const std::string&) { return std::nullopt; }

inline Config make_config(const std::string& providers, const std::string& secrets = kSecrets,
                          std::optional<std::string> settings = std::nullopt) {
  ConfigTexts texts;
  texts.providers = providers;
  texts.secrets = secrets;
  texts.settings = std::move(settings);
  return parse_config(texts, no_env);
}

class CollectSink final : public StreamSink {
 public:
  void on_delta(std::string_view text) override { text_ += text; }
  void on_reset() override {
    ++resets_;
    text_.clear();
  }
  const std::string& text() const { return text_; }
  int resets() const { return resets_; }

 private:
  std::string text_;
  int resets_ = 0;
};

struct Rig {
  std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
  std::shared_ptr<FakeServer> server = std::make_shared<FakeServer>(clock);
  std::unique_ptr<Client> client;

  explicit Rig(const std::string& providers = kCloud, const std::string& secrets = kSecrets,
               std::optional<std::string> settings = std::nullopt) {
    ClientOptions options;
    options.clock = clock;
    options.log = [this](LogLevel, const std::string& line) {
      std::lock_guard lock(log_mutex_);
      log_.push_back(line);
    };
    client = std::make_unique<Client>(make_config(providers, secrets, std::move(settings)), server, options);
  }

  ChatResult chat(const std::string& slot = "dialogue", Purpose purpose = Purpose::Reply, StreamSink* sink = nullptr,
                  std::stop_token stop = {}) {
    ChatRequest request;
    request.slot = slot;
    request.purpose = purpose;
    request.messages = {Message::system("You are an invented test speaker.", true), Message::user("Hello there.")};
    return client->chat(request, sink, std::move(stop));
  }

  std::vector<std::string> log() const {
    std::lock_guard lock(log_mutex_);
    return log_;
  }

  int log_count(const std::string& needle) const {
    int n = 0;
    for (const auto& line : log()) n += line.find(needle) != std::string::npos ? 1 : 0;
    return n;
  }

 private:
  mutable std::mutex log_mutex_;
  std::vector<std::string> log_;
};

}  // namespace en::llm::test
