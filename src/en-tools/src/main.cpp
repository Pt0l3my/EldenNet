// eldennet-engine: runs EldenNet's engine outside the game (roadmap S8.1.a, S8.1.g).
// Each subcommand lives in its own file: render_prompt.cpp, chat.cpp, decide.cpp, probe.cpp (probe and models).
#include <cstdio>
#include <cstring>
#include <string>

int run_render_prompt(int argc, char** argv);
int run_chat(int argc, char** argv);
int run_decide(int argc, char** argv);
int run_probe(int argc, char** argv);
int run_models(int argc, char** argv);

namespace {
void usage() {
  std::fputs(
      "usage: eldennet-engine <command> [options]\n"
      "  render-prompt  render a template folder with a JSON context into chat messages\n"
      "  chat           send rendered messages to a configured slot and stream the reply\n"
      "  decide         ask a decision of a configured slot\n"
      "  probe          find the local model servers on this PC's usual ports\n"
      "  models         list the models each configured provider offers\n"
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
  if (cmd == "version") {
    std::puts("eldennet-engine 0.0.1");
    return 0;
  }
  usage();
  return 2;
}
