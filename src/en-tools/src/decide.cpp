// eldennet-engine decide: asks a decision of the decisions slot and prints the pick (roadmap S8.1.a; ARCH §11.8).
//
//   eldennet-engine decide --config <dir> --request <file.json> [--models <models.toml>] [--verbose]
//
// The request file:
//   {"task": "next_speaker", "kind": "choice" | "yes_no", "subject": "<profile id>" (optional),
//    "question": "...", "state": [{"name": "...", "text": "..."}],
//    "options": [{"key": "A", "text": "..."}, {"key": "none", "text": "silence"}],
//    "rule": "<the key the task's rules pick>", "messages": [...] (optional: rendered by en-prompt)}
// The pick goes to stdout as JSON; the rules answer whenever the model cannot, so a pick always comes.
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "en/llm/client.hpp"
#include "en/llm/config.hpp"
#include "en/llm/decision.hpp"
#include "en/llm/message.hpp"

namespace {

struct DecideArgs {
  std::string config;
  std::string request;
  std::optional<std::string> models;
  bool verbose = false;
};

void decide_usage() {
  std::fputs(
      "usage: eldennet-engine decide --config <dir> --request <file.json> [--models <models.toml>] [--verbose]\n"
      "  <file.json>: {\"task\", \"kind\", \"question\", \"state\", \"options\", \"rule\"}; see decide.cpp.\n",
      stderr);
}

bool parse_decide_args(int argc, char** argv, DecideArgs& args) {
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    if (flag == "--verbose") {
      args.verbose = true;
      continue;
    }
    if (i + 1 >= argc) {
      std::fprintf(stderr, "decide: %s needs a value\n", flag.c_str());
      return false;
    }
    const char* v = argv[++i];
    if (flag == "--config") {
      args.config = v;
    } else if (flag == "--request") {
      args.request = v;
    } else if (flag == "--models") {
      args.models = v;
    } else {
      std::fprintf(stderr, "decide: unknown option %s\n", flag.c_str());
      return false;
    }
  }
  if (args.config.empty() || args.request.empty()) {
    std::fputs("decide: --config and --request are required\n", stderr);
    return false;
  }
  return true;
}

std::string string_field(const nlohmann::json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return {};
  if (!it->is_string()) throw std::invalid_argument(std::string("\"") + key + "\" must be a string");
  return it->get<std::string>();
}

// The request file into a call. The command has no code to run as the task's rules, so the file names the key
// they picked ("rule"), and the call's rule returns it.
en::llm::DecisionCall read_call(const nlohmann::json& j) {
  if (!j.is_object()) throw std::invalid_argument("the request must be a JSON object");
  en::llm::DecisionCall call;
  call.task = string_field(j, "task");
  const std::string kind = string_field(j, "kind");
  if (kind.empty() || kind == "choice") {
    call.kind = en::llm::DecisionKind::Choice;
  } else if (kind == "yes_no") {
    call.kind = en::llm::DecisionKind::YesNo;
  } else {
    throw std::invalid_argument("\"kind\" is choice or yes_no");
  }
  if (const std::string subject = string_field(j, "subject"); !subject.empty()) call.subject = subject;
  call.question = string_field(j, "question");
  if (auto state = j.find("state"); state != j.end()) {
    if (!state->is_array()) throw std::invalid_argument("\"state\" must be an array");
    for (const auto& r : *state) call.state.push_back({string_field(r, "name"), string_field(r, "text")});
  }
  if (auto options = j.find("options"); options != j.end()) {
    if (!options->is_array()) throw std::invalid_argument("\"options\" must be an array");
    for (const auto& o : *options) call.options.push_back({string_field(o, "key"), string_field(o, "text")});
  }
  if (auto messages = j.find("messages"); messages != j.end()) call.messages = en::llm::parse_messages(*messages);
  if (const std::string slot = string_field(j, "slot"); !slot.empty()) call.slot = slot;
  const std::string rule = string_field(j, "rule");
  if (rule.empty()) throw std::invalid_argument("\"rule\" (the key the task's rules pick) is required");
  call.rule = [rule] { return rule; };
  return call;
}

}  // namespace

int run_decide(int argc, char** argv) {
  DecideArgs args;
  if (!parse_decide_args(argc, argv, args)) {
    decide_usage();
    return 2;
  }
  en::llm::Config config;
  try {
    config = en::llm::load_config_dir(args.config, args.models);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "decide: %s\n", e.what());
    return 2;
  }
  for (const auto& warning : config.warnings) std::fprintf(stderr, "[decide] note: %s\n", warning.c_str());

  en::llm::DecisionCall call;
  try {
    std::ifstream in(args.request, std::ios::binary);
    if (!in) throw std::invalid_argument("cannot read the request file");
    std::ostringstream text;
    text << in.rdbuf();
    call = read_call(nlohmann::json::parse(text.str()));
    en::llm::validate(call);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "decide: %s\n", e.what());
    return 2;
  }

  auto transport = en::llm::make_default_transport_factory();
  if (!transport) std::fputs("[decide] this build has no HTTP transport: the rules decide\n", stderr);
  en::llm::ClientOptions options;
  const bool verbose = args.verbose;
  options.log = [verbose](en::llm::LogLevel level, const std::string& line) {
    if (level != en::llm::LogLevel::Debug || verbose) std::fprintf(stderr, "[en-llm] %s\n", line.c_str());
  };
  en::llm::Client client(std::move(config), std::move(transport), std::move(options));

  const en::llm::DecisionPick pick = client.decide(call);
  nlohmann::json out = pick.row();
  out["read"] = en::llm::to_string(pick.read);
  out["fallback"] = pick.fallback;
  out["miss"] = pick.miss;
  out["error"] = pick.error == en::llm::ErrorClass::None ? nlohmann::json(nullptr) : nlohmann::json(en::llm::to_string(pick.error));
  out["probabilities"] = nlohmann::json::array();
  for (const auto& p : pick.probabilities) out["probabilities"].push_back({{"key", p.key}, {"p", p.probability}});
  if (!pick.provider.empty()) {
    out["provider"] = pick.provider;
    out["model"] = pick.model;
  }
  std::puts(out.dump(2).c_str());
  return 0;
}
