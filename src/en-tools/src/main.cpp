// eldennet-engine: runs EldenNet's engine outside the game (roadmap S8.1.a, S8.1.g).
// Each subcommand lives in its own file: render_prompt.cpp, chat.cpp, decide.cpp, probe.cpp (probe and models),
// probe_exe.cpp (the derivation's offline round), lint.cpp (lint), spike.cpp (rec-stats, replay, spike-report) and
// golden.cpp (golden, over lib/: the scenario DSL and the golden runner, and the prompt goldens), lexicon.cpp
// (lexicon: the lint's lexicon) and eval.cpp (eval and mockllm, over lib/'s eval runner and scripted model).
#include <cstdio>
#include <cstring>
#include <string>

#ifndef EN_ENGINE_VERSION
#define EN_ENGINE_VERSION "0.0.1"
#endif

int run_render_prompt(int argc, char** argv);
int run_chat(int argc, char** argv);
int run_decide(int argc, char** argv);
int run_probe(int argc, char** argv);
int run_models(int argc, char** argv);
int run_probe_exe(int argc, char** argv);
int run_lodestar(int argc, char** argv);
int run_lint(int argc, char** argv);
int run_rec_stats(int argc, char** argv);
int run_replay(int argc, char** argv);
int run_spike_report(int argc, char** argv);
int run_golden(int argc, char** argv);
int run_lexicon(int argc, char** argv);
int run_eval(int argc, char** argv);
int run_mockllm(int argc, char** argv);

namespace {
void usage() {
  std::fputs(
      "usage: eldennet-engine <command> [options]\n"
      "  render-prompt  render a template folder with a JSON context into chat messages\n"
      "  chat           send rendered messages to a configured slot and stream the reply\n"
      "  decide         ask a decision of a configured slot\n"
      "  probe          find the local model servers on this PC's usual ports\n"
      "  models         list the models each configured provider offers\n"
      "  probe-exe      read a game's executable file and derive what the game access layer needs (devkit)\n"
      "  lodestar       find where EldenNet's ids lie in a game's executable file, as a report of numbers (devkit)\n"
      "  lint           check a numbers-only report (--report), a recording (--recording) or gal/ (--gal)\n"
      "  rec-stats      what a recording holds, as a numbers-only report\n"
      "  replay         step a recording's inputs through the core (and Augur) again, and compare its changes\n"
      "  golden         run a scenario or a recording through Augur, or render a prompt fixture, and compare with\n"
      "                 its golden file\n"
      "  lexicon        the lint's lexicon: fingerprints for the safety list, and a check of the pack's lists\n"
      "  spike-report   DESIGN 16.1's pass rules for S0-A to S0-D over spike logs and recordings\n"
      "  eval           run a scripted conversation against a scripted or recorded model, and report\n"
      "  mockllm        check a scripted model's script and print the events it gives\n"
      "  version        print the version\n",
      stderr);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return 2;
  }
  const std::string cmd = argv[1];
  if (cmd == "render-prompt") return run_render_prompt(argc - 1, argv + 1);
  if (cmd == "chat") return run_chat(argc - 1, argv + 1);
  if (cmd == "decide") return run_decide(argc - 1, argv + 1);
  if (cmd == "probe") return run_probe(argc - 1, argv + 1);
  if (cmd == "models") return run_models(argc - 1, argv + 1);
  if (cmd == "probe-exe") return run_probe_exe(argc - 1, argv + 1);
  if (cmd == "lodestar") return run_lodestar(argc - 1, argv + 1);
  if (cmd == "lint") return run_lint(argc - 1, argv + 1);
  if (cmd == "rec-stats") return run_rec_stats(argc - 1, argv + 1);
  if (cmd == "replay") return run_replay(argc - 1, argv + 1);
  if (cmd == "spike-report") return run_spike_report(argc - 1, argv + 1);
  if (cmd == "golden") return run_golden(argc - 1, argv + 1);
  if (cmd == "lexicon") return run_lexicon(argc - 1, argv + 1);
  if (cmd == "eval") return run_eval(argc - 1, argv + 1);
  if (cmd == "mockllm") return run_mockllm(argc - 1, argv + 1);
  if (cmd == "version") {
    std::puts("eldennet-engine " EN_ENGINE_VERSION);
    return 0;
  }
  usage();
  return 2;
}
