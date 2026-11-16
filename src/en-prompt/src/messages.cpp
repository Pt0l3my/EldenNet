// From rendered text to chat messages: markers, cache breakpoints, whitespace and merging (SN03 §2.2, NPC §7.1).
#include "en/prompt/messages.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

std::string_view to_string(Role role) {
  switch (role) {
    case Role::system:
      return "system";
    case Role::user:
      return "user";
    case Role::assistant:
      return "assistant";
  }
  return "user";
}

std::string Message::text() const {
  std::string out;
  for (const Part& part : parts) out += part.text;
  return out;
}

bool Message::cache() const { return !parts.empty() && parts.back().cache; }

std::size_t Message::breakpoints() const {
  return static_cast<std::size_t>(std::count_if(parts.begin(), parts.end(), [](const Part& p) { return p.cache; }));
}

namespace {

using detail::is_space;
using detail::trim;

enum class MarkerKind { start, end, cache, end_cache, raw, end_raw };

struct Marker {
  MarkerKind kind = MarkerKind::start;
  Role role = Role::user;
};

bool is_blank(char c) { return c == ' ' || c == '\t'; }

// Reads a marker at text[i], which is '[': "[", blanks, an optional "end" and blanks, a marker word, blanks, "]".
// Returns its length, or 0 when the bracket starts no marker.
std::size_t read_marker(std::string_view text, std::size_t i, Marker& marker) {
  std::size_t j = i + 1;
  const auto skip_blanks = [&] {
    while (j < text.size() && is_blank(text[j])) ++j;
  };
  skip_blanks();
  bool closing = false;
  if (text.compare(j, 3, "end") == 0 && j + 3 < text.size() && is_blank(text[j + 3])) {
    closing = true;
    j += 3;
    skip_blanks();
  }
  const std::size_t word_start = j;
  while (j < text.size() && text[j] >= 'a' && text[j] <= 'z') ++j;
  const std::string_view word = text.substr(word_start, j - word_start);
  skip_blanks();
  if (j >= text.size() || text[j] != ']') return 0;

  if (word == "system" || word == "user" || word == "assistant") {
    marker.kind = closing ? MarkerKind::end : MarkerKind::start;
    marker.role = word == "system" ? Role::system : (word == "user" ? Role::user : Role::assistant);
  } else if (word == "cache") {
    marker.kind = closing ? MarkerKind::end_cache : MarkerKind::cache;
  } else if (word == "raw") {
    marker.kind = closing ? MarkerKind::end_raw : MarkerKind::raw;
  } else {
    return 0;
  }
  return j + 1 - i;
}

// Whether a whole line reads as a marker of some syntax: "[history]", "[ end histroy ]", "[system cache=A]". Such a
// line that read_marker did not take is a typo or another syntax, and would reach the model as text.
bool looks_like_marker(std::string_view line) {
  line = trim(line);
  if (line.size() < 3 || line.front() != '[' || line.back() != ']') return false;
  std::string_view inside = trim(line.substr(1, line.size() - 2));
  if (inside.compare(0, 3, "end") == 0 && inside.size() > 3 && is_blank(inside[3])) inside = trim(inside.substr(3));
  std::size_t j = 0;
  while (j < inside.size() && ((inside[j] >= 'a' && inside[j] <= 'z') || inside[j] == '_')) ++j;
  if (j == 0) return false;
  // Then only attributes such as "cache=A".
  while (j < inside.size()) {
    if (!is_blank(inside[j])) return false;
    while (j < inside.size() && is_blank(inside[j])) ++j;
    const std::size_t key = j;
    while (j < inside.size() && (std::isalnum(static_cast<unsigned char>(inside[j])) || inside[j] == '_')) ++j;
    if (j == key || j >= inside.size() || inside[j] != '=') return false;
    ++j;
    while (j < inside.size() && !is_blank(inside[j])) ++j;
  }
  return true;
}

[[noreturn]] void fail(std::string_view source, const std::string& message) {
  throw TemplateError(message, Site{std::string(source)});
}

std::string at_line(std::size_t line) { return " at line " + std::to_string(line) + " of the rendered output"; }

struct Token {
  enum class Type { text, raw, marker };
  Type type = Type::text;
  std::string_view text;
  Marker marker;
  std::size_t line = 1;  // the output line the token starts on
};

std::size_t count_lines(std::string_view text) {
  return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}

std::vector<Token> tokenise(std::string_view text, std::string_view source) {
  std::vector<Token> tokens;
  std::size_t line = 1;
  std::size_t start = 0;
  std::size_t start_line = 1;
  const auto flush = [&](std::size_t end) {
    if (end > start) tokens.push_back({Token::Type::text, text.substr(start, end - start), {}, start_line});
  };

  std::size_t i = 0;
  while (i < text.size()) {
    Marker marker;
    const std::size_t length = text[i] == '[' ? read_marker(text, i, marker) : 0;
    if (length == 0) {
      if (text[i] == '\n') ++line;
      ++i;
      continue;
    }
    flush(i);
    if (marker.kind == MarkerKind::end_raw) fail(source, "[ end raw ]" + at_line(line) + " closes no [ raw ]");
    if (marker.kind == MarkerKind::raw) {
      // Everything up to [ end raw ] is plain text, markers included.
      const std::size_t body = i + length;
      std::size_t k = body;
      std::size_t close_length = 0;
      for (; k < text.size(); ++k) {
        Marker inner;
        if (text[k] == '[' && (close_length = read_marker(text, k, inner)) > 0 && inner.kind == MarkerKind::end_raw) {
          break;
        }
        close_length = 0;
      }
      if (close_length == 0) fail(source, "[ raw ]" + at_line(line) + " is never closed with [ end raw ]");
      tokens.push_back({Token::Type::raw, text.substr(body, k - body), {}, line});
      line += count_lines(text.substr(i, k + close_length - i));
      i = k + close_length;
    } else {
      tokens.push_back({Token::Type::marker, text.substr(i, length), marker, line});
      i += length;
    }
    start = i;
    start_line = line;
  }
  flush(text.size());
  return tokens;
}

void refuse_marker_like_lines(const Token& token, std::string_view source) {
  std::size_t line = token.line;
  std::size_t start = 0;
  for (;;) {
    const std::size_t end = token.text.find('\n', start);
    const std::string_view text = token.text.substr(start, end == std::string_view::npos ? end : end - start);
    if (looks_like_marker(text)) {
      fail(source, "unknown marker '" + std::string(trim(text)) + "'" + at_line(line) +
                       ": the markers are [ system ], [ user ], [ assistant ], [ cache ], [ raw ] and their [ end … ]");
    }
    if (end == std::string_view::npos) break;
    start = end + 1;
    ++line;
  }
}

// Every line trimmed and blank lines dropped: indentation carries no meaning in a prompt.
std::string tidy_lines(std::string_view text) {
  std::string out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t end = text.find('\n', start);
    const std::string_view line = trim(text.substr(start, end == std::string_view::npos ? end : end - start));
    if (!line.empty()) {
      if (!out.empty()) out += '\n';
      out += line;
    }
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return out;
}

// Builds messages from the token stream.
class Assembler {
 public:
  explicit Assembler(std::string_view source) : source_(source) {}

  void add_text(const Token& token) {
    if (open_) {
      drafts_.back().parts.back().text += token.text;
      return;
    }
    const std::string_view text = token.text;
    const auto visible = std::find_if(text.begin(), text.end(), [](char c) { return !is_space(c); });
    const auto first = static_cast<std::size_t>(visible - text.begin());
    if (first < text.size()) {
      fail(source_, "text outside a message" + at_line(token.line + count_lines(text.substr(0, first))) +
                        ": start it with [ system ], [ user ] or [ assistant ]");
    }
  }

  void start(Role role) {
    drafts_.push_back(Draft{role, {DraftPart{}}});
    open_ = true;
  }

  void end(Role role, std::size_t line) {
    const std::string marker = "[ end " + std::string(to_string(role)) + " ]";
    if (!open_) fail(source_, marker + at_line(line) + " closes no message");
    if (drafts_.back().role != role) {
      fail(source_, marker + at_line(line) + " closes a " + std::string(to_string(drafts_.back().role)) + " message");
    }
    open_ = false;
  }

  // A breakpoint here: the current message's text so far ends a cached stretch. Between messages, it marks the end
  // of the message before.
  void breakpoint(std::size_t line) {
    if (drafts_.empty()) fail(source_, "[ cache ]" + at_line(line) + " comes before any message");
    DraftPart& part = drafts_.back().parts.back();
    part.cache = true;
    part.cache_line = line;
    if (open_) drafts_.back().parts.push_back(DraftPart{});
  }

  std::vector<Message> finish(const AssembleOptions& options) {
    std::vector<Message> messages;
    for (Draft& draft : drafts_) {
      Message message{draft.role, {}};
      for (DraftPart& draft_part : draft.parts) {
        std::string text = tidy_lines(draft_part.text);
        if (text.empty()) {
          if (draft_part.cache) {
            fail(source_, "[ cache ]" + at_line(draft_part.cache_line) +
                              " marks no text: put it after the text whose end it marks");
          }
          continue;
        }
        if (!message.parts.empty()) text.insert(0, "\n");
        message.parts.push_back(Part{std::move(text), draft_part.cache});
      }
      if (!message.parts.empty()) messages.push_back(std::move(message));
    }
    if (options.merge_same_role) messages = merge_same_role(std::move(messages));
    for (Message& message : messages) {
      for (Part& part : message.parts) detail::restore_data(part.text);
    }
    return messages;
  }

 private:
  struct DraftPart {
    std::string text;
    bool cache = false;
    std::size_t cache_line = 0;
  };
  struct Draft {
    Role role = Role::user;
    std::vector<DraftPart> parts;
  };

  std::string_view source_;
  std::vector<Draft> drafts_;
  bool open_ = false;
};

// Whether the [ cache ] at tokens[k] opens SkyrimNet's region form: its next cache marker is [ end cache ], with no
// message boundary before it.
bool opens_region(const std::vector<Token>& tokens, std::size_t k) {
  for (std::size_t j = k + 1; j < tokens.size(); ++j) {
    if (tokens[j].type != Token::Type::marker) continue;
    if (tokens[j].marker.kind == MarkerKind::end_cache) return true;
    return false;  // a start, an end or another [ cache ] comes first
  }
  return false;
}

}  // namespace

std::vector<Message> assemble_messages(std::string_view rendered, std::string_view source_name,
                                       const AssembleOptions& options) {
  const std::vector<Token> tokens = tokenise(rendered, source_name);
  Assembler assembler(source_name);
  bool region_open = false;
  for (std::size_t k = 0; k < tokens.size(); ++k) {
    const Token& token = tokens[k];
    if (token.type == Token::Type::text) refuse_marker_like_lines(token, source_name);
    if (token.type != Token::Type::marker) {
      assembler.add_text(token);
      continue;
    }
    switch (token.marker.kind) {
      case MarkerKind::start:
        assembler.start(token.marker.role);
        break;
      case MarkerKind::end:
        assembler.end(token.marker.role, token.line);
        break;
      case MarkerKind::cache:
        if (opens_region(tokens, k)) {
          region_open = true;  // the breakpoint goes where the region ends
        } else {
          assembler.breakpoint(token.line);
        }
        break;
      case MarkerKind::end_cache:
        if (!region_open) fail(source_name, "[ end cache ]" + at_line(token.line) + " closes no [ cache ]");
        region_open = false;
        assembler.breakpoint(token.line);
        break;
      case MarkerKind::raw:
      case MarkerKind::end_raw:
        break;  // tokenise() has taken them
    }
  }
  return assembler.finish(options);
}

std::vector<Message> merge_same_role(std::vector<Message> messages) {
  std::vector<Message> out;
  for (Message& message : messages) {
    if (message.parts.empty()) continue;
    if (out.empty() || out.back().role != message.role) {
      out.push_back(std::move(message));
      continue;
    }
    std::vector<Part>& parts = out.back().parts;
    Part& first = message.parts.front();
    first.text.insert(0, "\n\n");
    if (!parts.back().cache) {
      // The message before ends without a breakpoint, so its last part simply runs on.
      parts.back().text += first.text;
      parts.back().cache = first.cache;
    } else {
      parts.push_back(std::move(first));
    }
    for (std::size_t i = 1; i < message.parts.size(); ++i) parts.push_back(std::move(message.parts[i]));
  }
  return out;
}

void check_unresolved(std::string_view rendered, std::string_view source_name) {
  const auto where = [&](std::size_t i) { return at_line(1 + count_lines(rendered.substr(0, i))); };
  for (std::size_t i = 0; i + 1 < rendered.size(); ++i) {
    const char c = rendered[i];
    const char next = rendered[i + 1];
    if (c == '{' && (next == '{' || next == '%' || next == '#')) {
      fail(source_name, std::string("unresolved template syntax '") + c + next + "' reached the output" + where(i));
    }
    if (c == '$' && next == '{') fail(source_name, "an unfilled placeholder '${' reached the output" + where(i));
    if (c == '{' && (std::isalpha(static_cast<unsigned char>(next)) || next == '_')) {
      std::size_t j = i + 1;
      while (j < rendered.size() &&
             (std::isalnum(static_cast<unsigned char>(rendered[j])) || rendered[j] == '_' || rendered[j] == '.')) {
        ++j;
      }
      if (j < rendered.size() && rendered[j] == '}') {
        fail(source_name, "the placeholder '" + std::string(rendered.substr(i, j - i + 1)) + "' was never filled" +
                              where(i));
      }
    }
  }
}

std::size_t estimate_tokens(std::string_view text) {
  // ceil(bytes / 3.6 × 1.15) = ceil(bytes × 23 / 72), in whole numbers so it is the same on every system.
  return (text.size() * 23 + 71) / 72;
}

std::size_t estimate_tokens(const std::vector<Message>& messages) {
  std::size_t bytes = 0;
  for (const Message& message : messages) {
    for (const Part& part : message.parts) bytes += part.text.size();
  }
  return (bytes * 23 + 71) / 72;
}

nlohmann::json to_json(const std::vector<Message>& messages) {
  nlohmann::json out = nlohmann::json::array();
  for (const Message& message : messages) {
    nlohmann::json parts = nlohmann::json::array();
    for (const Part& part : message.parts) parts.push_back({{"text", part.text}, {"cache", part.cache}});
    out.push_back({{"role", to_string(message.role)},
                   {"content", message.text()},
                   {"cache", message.cache()},
                   {"parts", std::move(parts)}});
  }
  return out;
}

std::string format_plain(const std::vector<Message>& messages) {
  std::string out;
  for (const Message& message : messages) {
    out += "[ " + std::string(to_string(message.role)) + " ]\n";
    for (std::size_t i = 0; i < message.parts.size(); ++i) {
      std::string_view text = message.parts[i].text;
      if (i > 0 && !text.empty() && text.front() == '\n') text.remove_prefix(1);  // the marker line ends it
      out += text;
      if (message.parts[i].cache) out += "\n[ cache ]";
      out += '\n';
    }
  }
  return out;
}

}  // namespace en::prompt
