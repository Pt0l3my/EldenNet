// eldennet-engine chat: sends rendered messages to a configured slot and streams the reply (roadmap S8.1.a).
//
//   eldennet-engine chat --config <dir> --slot <name> --messages <file.json>
//                        [--models <models.toml>] [--purpose reply|own|decision|background] [--max-tokens N] [--verbose]
//
// The reply goes to stdout as it streams; a summary and any error go to stderr. Ctrl-C cancels the request.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "en/llm/client.hpp"
#include "en/llm/config.hpp"
#include "en/llm/message.hpp"

namespace {

std::atomic<bool> g_interrupted{false};

extern "C" void on_interrupt(int) { g_interrupted.store(true); }

struct ChatArgs {
  std::string config;
  std::string slot = "dialogue";
  std::string messages;
  std::optional<std::string> models;
  std::string purpose = "reply";
  std::optional<int> max_tokens;
  bool verbose = false;
};

void chat_usage() {
  std::fputs(
      "usage: eldennet-engine chat --config <dir> --slot <name> --messages <file.json>\n"
      "                            [--models <models.toml>] [--purpose reply|own|decision|background]\n"
      "                            [--max-tokens N] [--verbose]\n"
      "  <dir> holds providers.toml, and settings.toml, secrets.toml and models.toml where used.\n"
      "  <file.json> is an array of {\"role\", \"content\"} messages, or {\"messages\": [...]}, as render-prompt writes.\n",
      stderr);
}

bool parse_chat_args(int argc, char** argv, ChatArgs& args) {
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    auto value = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    if (flag == "--verbose") {
      args.verbose = true;
      continue;
    }
    const char* v = value();
    if (v == nullptr) {
      std::fprintf(stderr, "chat: %s needs a value\n", flag.c_str());
      return false;
    }
    if (flag == "--config") {
      args.config = v;
    } else if (flag == "--slot") {
      args.slot = v;
    } else if (flag == "--messages") {
      args.messages = v;
    } else if (flag == "--models") {
      args.models = v;
    } else if (flag == "--purpose") {
      args.purpose = v;
    } else if (flag == "--max-tokens") {
      try {
        args.max_tokens = std::stoi(v);
      } catch (const std::exception&) {
        std::fputs("chat: --max-tokens needs a number\n", stderr);
        return false;
      }
    } else {
      std::fprintf(stderr, "chat: unknown option %s\n", flag.c_str());
      return false;
    }
  }
  if (args.config.empty() || args.messages.empty()) {
    std::fputs("chat: --config and --messages are required\n", stderr);
    return false;
  }
  return true;
}

std::optional<en::llm::Purpose> parse_purpose(const std::string& text) {
  if (text == "reply") return en::llm::Purpose::Reply;
  if (text == "own") return en::llm::Purpose::OwnInitiative;
  if (text == "decision") return en::llm::Purpose::Decision;
  if (text == "background") return en::llm::Purpose::Background;
  return std::nullopt;
}

std::optional<std::string> read_text(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

class PrintSink final : public en::llm::StreamSink {
 public:
  void on_delta(std::string_view text) override {
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
    printed_ = true;
  }
  void on_reset() override { std::fputs("\n[the text above was the model's reasoning, not its reply]\n", stderr); }
  bool printed() const { return printed_; }

 private:
  bool printed_ = false;
};

std::string tokens_text(const en::llm::Usage& usage) {
  if (usage.prompt_tokens < 0 && usage.completion_tokens < 0) return "tokens not reported";
  std::string text = "tokens " + std::to_string(usage.prompt_tokens) + " in";
  if (usage.cached_tokens >= 0) text += " (" + std::to_string(usage.cached_tokens) + " cached)";
  return text + ", " + std::to_string(usage.completion_tokens) + " out";
}

void print_summary(const en::llm::ChatResult& r) {
  std::string attempts;
  for (const auto& a : r.attempts) {
    if (!attempts.empty()) attempts += "; ";
    attempts += a.provider + " / " + a.model + ": " + (a.error == en::llm::ErrorClass::None ? "ok" : en::llm::to_string(a.error));
    if (a.backup) attempts += " (backup)";
    if (a.lost_race) attempts += " (lost the race)";
    if (a.stripped) attempts += " (stripped)";
    if (a.minimal_effort) attempts += " (minimal effort)";
  }
  std::fprintf(stderr, "[chat] %s / %s: first text %s, total %lld ms, %s, finish %s\n", r.provider.c_str(),
               r.model.c_str(),
               r.first_token.count() >= 0 ? (std::to_string(r.first_token.count()) + " ms").c_str() : "none",
               static_cast<long long>(r.total.count()), tokens_text(r.usage).c_str(),
               r.finish_reason.empty() ? "-" : r.finish_reason.c_str());
  if (r.attempts.size() > 1) std::fprintf(stderr, "[chat] attempts: %s\n", attempts.c_str());
}

}  // namespace

int run_chat(int argc, char** argv) {
  ChatArgs args;
  if (!parse_chat_args(argc, argv, args)) {
    chat_usage();
    return 2;
  }
  const auto purpose = parse_purpose(args.purpose);
  if (!purpose) {
    std::fputs("chat: --purpose is reply, own, decision or background\n", stderr);
    return 2;
  }

  en::llm::Config config;
  try {
    config = en::llm::load_config_dir(args.config, args.models);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "chat: %s\n", e.what());
    return 2;
  }
  for (const auto& warning : config.warnings) std::fprintf(stderr, "[chat] note: %s\n", warning.c_str());

  en::llm::ChatRequest request;
  request.slot = args.slot;
  request.purpose = *purpose;
  request.max_tokens = args.max_tokens;
  const auto text = read_text(args.messages);
  if (!text) {
    std::fputs("chat: cannot read the messages file\n", stderr);
    return 2;
  }
  try {
    request.messages = en::llm::parse_messages(nlohmann::json::parse(*text));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "chat: the messages file: %s\n", e.what());
    return 2;
  }

  auto transport = en::llm::make_default_transport_factory();
  if (!transport) {
    std::fputs("chat: this build has no HTTP transport (no libcurl, or the system's WinHTTP could not be loaded)\n", stderr);
    return 1;
  }
  en::llm::ClientOptions options;
  const bool verbose = args.verbose;
  options.log = [verbose](en::llm::LogLevel level, const std::string& line) {
    if (level != en::llm::LogLevel::Debug || verbose) std::fprintf(stderr, "[en-llm] %s\n", line.c_str());
  };
  en::llm::Client client(std::move(config), std::move(transport), std::move(options));

  // A signal handler may only set a flag; a watcher turns it into the request's cancellation.
  std::stop_source stop;
  std::signal(SIGINT, on_interrupt);
  std::atomic<bool> finished{false};
  std::thread watcher([&] {
    while (!finished.load()) {
      if (g_interrupted.load()) {
        stop.request_stop();
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });

  PrintSink sink;
  const en::llm::ChatResult result = client.chat(request, &sink, stop.get_token());
  finished.store(true);
  watcher.join();
  std::signal(SIGINT, SIG_DFL);

  if (sink.printed()) std::fputs("\n", stdout);
  std::fflush(stdout);  // the reply's last line before the summary on stderr
  print_summary(result);
  if (!result.ok()) {
    std::fprintf(stderr, "chat: %s: %s\n", en::llm::to_string(result.error), result.message.c_str());
    return 1;
  }
  return 0;
}
