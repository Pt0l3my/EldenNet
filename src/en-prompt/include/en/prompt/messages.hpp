#pragma once
// Chat messages from rendered text, by SkyrimNet's message markers (SN03 §2.2) and NPC §7.1's message assembly.
//
//   [ system ] [ user ] [ assistant ]   start a message with that role; [ end system ] and the like close one
//                                       (optional, but an end marker must match the open message).
//   [ cache ]                           a cache breakpoint where it stands: placed at the end of a message, it marks
//                                       the message it ends. SkyrimNet's region form, [ cache ] … [ end cache ],
//                                       puts the breakpoint at [ end cache ].
//   [ raw ] … [ end raw ]               keeps marker-like text inside it as plain text.
//
// After splitting, every line is trimmed and blank lines are dropped (SkyrimNet sends no indentation and no blank
// lines: the owner's syntax notes, §2), empty messages are dropped and, by default, consecutive messages that share
// a role are merged with a blank line (NPC §7.1, for strict chat templates). Breakpoints survive the merge.
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace en::prompt {

enum class Role { system, user, assistant };

std::string_view to_string(Role role);

// A stretch of a message's text. A message is split into parts only at its cache breakpoints, so every part but the
// last ends at one.
struct Part {
  std::string text;    // starts with the separator from the part before it: the parts joined are the message
  bool cache = false;  // a cache breakpoint at the end of this part
};

struct Message {
  Role role = Role::user;
  std::vector<Part> parts;

  // What the model reads: the parts joined.
  std::string text() const;
  // Whether the message ends at a cache breakpoint (the brief's "cache mark").
  bool cache() const;
  // How many breakpoints the message holds.
  std::size_t breakpoints() const;
};

struct AssembleOptions {
  // Consecutive messages with the same role become one, joined by a blank line (NPC §7.1).
  bool merge_same_role = true;
};

// Splits rendered text into messages at its markers and tidies their whitespace. Throws TemplateError, naming
// `source_name` and the output line, for text outside a message, a mismatched end marker, a breakpoint that marks
// no text, an unclosed [ raw ] and a line that looks like a marker but is not one (a typo, or a marker of another
// syntax such as "[history]").
std::vector<Message> assemble_messages(std::string_view rendered, std::string_view source_name,
                                       const AssembleOptions& options = {});

// Joins consecutive messages that share a role with a blank line, keeping every breakpoint.
std::vector<Message> merge_same_role(std::vector<Message> messages);

// Throws TemplateError when template syntax ("{{", "{%", "{#") or an unfilled placeholder ("{name}", "${") is left
// in rendered text. Text printed from data is exempt: records are quoted data, never template source (NPC §7.1).
void check_unresolved(std::string_view rendered, std::string_view source_name);

// The design's estimate, ceil(bytes / 3.6 × 1.15) (NPC §7.1, P1 §5.4).
std::size_t estimate_tokens(std::string_view text);
std::size_t estimate_tokens(const std::vector<Message>& messages);

// [{"role", "content", "cache", "parts": [{"text", "cache"}, …]}, …]: one object per message, parts always present.
nlohmann::json to_json(const std::vector<Message>& messages);

// The messages written out in marker syntax, for reading.
std::string format_plain(const std::vector<Message>& messages);

}  // namespace en::prompt
