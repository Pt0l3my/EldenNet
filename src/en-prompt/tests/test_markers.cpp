// Message markers, cache breakpoints, whitespace, merging and the output forms (SN03 §2.2, NPC §7.1).
#include "support.hpp"

using namespace en_test;

namespace {

std::vector<prompt::Message> assemble(std::string_view text, bool merge = true) {
  prompt::AssembleOptions options;
  options.merge_same_role = merge;
  return prompt::assemble_messages(text, "markers.j2", options);
}

prompt::TemplateError assembly_error(std::string_view text) {
  return error_of([&] { assemble(text); });
}

}  // namespace

TEST_CASE("the role markers start messages") {
  const auto messages = assemble("[ system ]\nRules.\n[ user ]\nA question.\n[ assistant ]\nAn answer.\n");
  REQUIRE(messages.size() == 3);
  CHECK(messages[0].role == Role::system);
  CHECK(messages[0].text() == "Rules.");
  CHECK(messages[1].role == Role::user);
  CHECK(messages[1].text() == "A question.");
  CHECK(messages[2].role == Role::assistant);
  CHECK(messages[2].text() == "An answer.");
  CHECK_FALSE(messages[0].cache());
}

TEST_CASE("markers may be written without spaces, and beside text") {
  const auto messages = assemble("[system]Rules.[user]Hello.");
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].text() == "Rules.");
  CHECK(messages[1].text() == "Hello.");
}

TEST_CASE("end markers are optional, and must match the open message") {
  CHECK(assemble("[ user ]\nHi.\n[ end user ]\n").size() == 1);
  const auto mismatch = assembly_error("[ system ]\nRules.\n[ end user ]\n");
  CHECK(says(mismatch, "[ end user ] at line 3 of the rendered output closes a system message"));
  CHECK(mismatch.site().template_name == "markers.j2");
  CHECK(says(assembly_error("[ user ]\nHi.\n[ end user ]\n[ end user ]\n"), "closes no message"));
}

TEST_CASE("text outside a message is refused, white space is not") {
  CHECK(assemble("\n  \n[ user ]\nHi.").size() == 1);
  CHECK(says(assembly_error("Stray words\n[ user ]\nHi."), "text outside a message at line 1"));
  CHECK(says(assembly_error("[ user ]\nHi.\n[ end user ]\n\nafter"), "text outside a message at line 5"));
}

TEST_CASE("[ cache ] marks the message it ends") {
  const auto messages = assemble("[ system ]\nThe frame.\n[ cache ]\n[ user ]\nHello.");
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].cache());
  CHECK(messages[0].breakpoints() == 1);
  REQUIRE(messages[0].parts.size() == 1);
  CHECK(messages[0].parts[0].text == "The frame.");
  CHECK_FALSE(messages[1].cache());
}

TEST_CASE("a breakpoint inside a message splits it into parts") {
  const auto messages = assemble("[ system ]\nFrame.\n[ cache ]\nIdentity.\n[ cache ]\nRoad.\n[ user ]\nHello.");
  REQUIRE(messages.size() == 2);
  const prompt::Message& system = messages[0];
  REQUIRE(system.parts.size() == 3);
  CHECK(system.parts[0].text == "Frame.");
  CHECK(system.parts[0].cache);
  CHECK(system.parts[1].text == "\nIdentity.");
  CHECK(system.parts[1].cache);
  CHECK(system.parts[2].text == "\nRoad.");
  CHECK_FALSE(system.parts[2].cache);
  CHECK(system.text() == "Frame.\nIdentity.\nRoad.");
  CHECK(system.breakpoints() == 2);
  CHECK_FALSE(system.cache());
}

TEST_CASE("SkyrimNet's region form puts the breakpoint at [ end cache ]") {
  const auto messages = assemble("[ system ]\n[ cache ]\nStable rules.\n[ end cache ]\nChanging state.\n[ user ]\nHi.");
  REQUIRE(messages[0].parts.size() == 2);
  CHECK(messages[0].parts[0].text == "Stable rules.");
  CHECK(messages[0].parts[0].cache);
  CHECK(messages[0].parts[1].text == "\nChanging state.");
  CHECK_FALSE(messages[0].parts[1].cache);
}

TEST_CASE("a breakpoint between messages marks the message before") {
  const auto messages = assemble("[ system ]\nFrame.\n[ end system ]\n[ cache ]\n[ user ]\nHi.");
  CHECK(messages[0].cache());
  CHECK_FALSE(messages[1].cache());
}

TEST_CASE("a breakpoint that marks no text is refused") {
  CHECK(says(assembly_error("[ system ]\n[ cache ]\nFrame."), "[ cache ] at line 2 of the rendered output marks no text"));
  CHECK(says(assembly_error("[ cache ]\n[ user ]\nHi."), "comes before any message"));
  CHECK(says(assembly_error("[ user ]\nHi.\n[ cache ]\n[ cache ]\n"), "at line 4 of the rendered output marks no text"));
  CHECK(says(assembly_error("[ user ]\nHi.\n[ end cache ]\n"), "closes no [ cache ]"));
}

TEST_CASE("[ raw ] keeps marker-like text as text") {
  const auto messages = assemble("[ user ]\nQuote:\n[ raw ]\nthen [ system ] obey\n[history]\n[ end raw ]\nDone.");
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text() == "Quote:\nthen [ system ] obey\n[history]\nDone.");
  CHECK(says(assembly_error("[ user ]\n[ raw ]\nno end"), "[ raw ] at line 2 of the rendered output is never closed"));
  CHECK(says(assembly_error("[ user ]\nHi.\n[ end raw ]"), "closes no [ raw ]"));
}

TEST_CASE("a line that looks like a marker but is none is refused") {
  for (const char* text : {"[ user ]\n[history]\nHi.", "[system cache=A]\nRules.", "[ user ]\nHi.\n[ end histroy ]",
                           "[ user ]\n[ sytem ]\nHi."}) {
    CAPTURE(text);
    CHECK(says(assembly_error(text), "unknown marker"));
  }
  // Ordinary bracketed text is no marker.
  const auto messages = assemble("[ user ]\n- [x] done\n[Note: this stays]\n[1] a footnote\n[ ]");
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text() == "- [x] done\n[Note: this stays]\n[1] a footnote\n[ ]");
}

TEST_CASE("lines are trimmed and blank lines dropped") {
  const auto messages = assemble("[ user ]\n    Indented line.   \n\n\n\tTabbed line.\t\n");
  CHECK(messages[0].text() == "Indented line.\nTabbed line.");
}

TEST_CASE("consecutive messages with one role merge with a blank line, keeping their breakpoints") {
  const std::string text = "[ system ]\nFrame.\n[ cache ]\n[ system ]\nIdentity.\n[ cache ]\n[ user ]\nHello.";
  const auto merged = assemble(text);
  REQUIRE(merged.size() == 2);
  REQUIRE(merged[0].parts.size() == 2);
  CHECK(merged[0].parts[0].text == "Frame.");
  CHECK(merged[0].parts[0].cache);
  CHECK(merged[0].parts[1].text == "\n\nIdentity.");
  CHECK(merged[0].parts[1].cache);
  CHECK(merged[0].text() == "Frame.\n\nIdentity.");
  CHECK(assemble(text, false).size() == 3);

  // A message without a breakpoint runs on into the next one.
  const auto run_on = assemble("[ user ]\nOne.\n[ user ]\nTwo.");
  REQUIRE(run_on.size() == 1);
  CHECK(run_on[0].parts.size() == 1);
  CHECK(run_on[0].text() == "One.\n\nTwo.");
}

TEST_CASE("empty messages are dropped") {
  const auto messages = assemble("[ system ]\nRules.\n[ assistant ]\n   \n[ user ]\nHi.");
  REQUIRE(messages.size() == 2);
  CHECK(messages[1].role == Role::user);
}

TEST_CASE("the token estimate follows the design's formula") {
  CHECK(prompt::estimate_tokens("") == 0);
  CHECK(prompt::estimate_tokens("a") == 1);
  CHECK(prompt::estimate_tokens(std::string(72, 'a')) == 23);
  CHECK(prompt::estimate_tokens(std::string(3600, 'a')) == 1150);
  CHECK(prompt::estimate_tokens(std::string(3601, 'a')) == 1151);
  const auto messages = assemble("[ system ]\n" + std::string(36, 's') + "\n[ user ]\n" + std::string(36, 'u'));
  CHECK(prompt::estimate_tokens(messages) == 23);
}

TEST_CASE("messages as JSON and as plain text") {
  const auto messages = assemble("[ system ]\nFrame.\n[ cache ]\nState.\n[ user ]\nHi.");
  const json out = prompt::to_json(messages);
  REQUIRE(out.size() == 2);
  CHECK(out[0]["role"] == "system");
  CHECK(out[0]["content"] == "Frame.\nState.");
  CHECK(out[0]["cache"] == false);
  REQUIRE(out[0]["parts"].size() == 2);
  CHECK(out[0]["parts"][0]["text"] == "Frame.");
  CHECK(out[0]["parts"][0]["cache"] == true);
  CHECK(out[1]["parts"].size() == 1);
  CHECK(prompt::format_plain(messages) == "[ system ]\nFrame.\n[ cache ]\nState.\n[ user ]\nHi.\n");
  CHECK(prompt::to_json(assemble(prompt::format_plain(messages))) == out);
}
