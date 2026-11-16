// Steam's KeyValues text (vdf.hpp).
#include "en/launcher/vdf.hpp"

#include <optional>

namespace en::launcher::vdf {

namespace {

bool same_key(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

enum class Tok { End, Open, Close, Text, Condition };

class Lexer {
 public:
  explicit Lexer(std::string_view t) : t_(t) {}

  // The next token; false with `error` set when the text is malformed.
  bool next(Tok& kind, std::string& text) {
    skip();
    text.clear();
    if (at_ >= t_.size()) {
      kind = Tok::End;
      return true;
    }
    const char c = t_[at_];
    if (c == '{') {
      ++at_;
      kind = Tok::Open;
      return true;
    }
    if (c == '}') {
      ++at_;
      kind = Tok::Close;
      return true;
    }
    if (c == '[') {
      while (at_ < t_.size() && t_[at_] != ']' && t_[at_] != '\n') ++at_;
      if (at_ >= t_.size() || t_[at_] != ']') return fail("a condition is not closed");
      ++at_;
      kind = Tok::Condition;
      return true;
    }
    kind = Tok::Text;
    if (c == '"') {
      ++at_;
      while (at_ < t_.size() && t_[at_] != '"') {
        char ch = t_[at_++];
        if (ch == '\n') ++line_;
        if (ch == '\\' && at_ < t_.size()) {
          const char e = t_[at_++];
          ch = e == 'n' ? '\n' : e == 't' ? '\t' : e;  // \\ and \" (and any other) stand for themselves
        }
        text.push_back(ch);
      }
      if (at_ >= t_.size()) return fail("a quoted string is not closed");
      ++at_;
      return true;
    }
    while (at_ < t_.size()) {
      const char ch = t_[at_];
      if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '{' || ch == '}' || ch == '"') break;
      text.push_back(ch);
      ++at_;
    }
    return true;
  }

  [[nodiscard]] std::size_t line() const noexcept { return line_; }
  [[nodiscard]] const char* problem() const noexcept { return problem_; }

 private:
  void skip() {
    while (at_ < t_.size()) {
      const char c = t_[at_];
      if (c == '\n') {
        ++line_;
        ++at_;
      } else if (c == ' ' || c == '\t' || c == '\r') {
        ++at_;
      } else if (c == '/' && at_ + 1 < t_.size() && t_[at_ + 1] == '/') {
        while (at_ < t_.size() && t_[at_] != '\n') ++at_;
      } else {
        break;
      }
    }
  }
  bool fail(const char* p) {
    problem_ = p;
    return false;
  }

  std::string_view t_;
  std::size_t at_ = 0;
  std::size_t line_ = 1;
  const char* problem_ = "";
};

class Parser {
 public:
  explicit Parser(std::string_view t) : lex_(t) {}

  bool object(Node& into, std::size_t depth, bool top) {
    Tok kind = Tok::End;
    std::string text;
    for (;;) {
      if (!lex_.next(kind, text)) return fail(lex_.problem());
      if (kind == Tok::End) return top ? true : fail("an object is not closed");
      if (kind == Tok::Close) return top ? fail("a closing brace with no object open") : true;
      if (kind == Tok::Condition) continue;  // a `[$…]` after a value or an object: set aside
      if (kind != Tok::Text) return fail("a key was expected");
      if (++nodes_ > kMaxNodes) return fail("too many keys");
      Node child;
      child.key = std::move(text);
      if (!lex_.next(kind, text)) return fail(lex_.problem());
      if (kind == Tok::Open) {
        if (depth + 1 > kMaxDepth) return fail("objects nested too deep");
        child.object = true;
        if (!object(child, depth + 1, false)) return false;
      } else if (kind == Tok::Text) {
        child.value = std::move(text);
      } else {
        return fail("a value or an object was expected after a key");
      }
      into.children.push_back(std::move(child));
    }
  }

  [[nodiscard]] Error error() const noexcept { return Error{lex_.line(), problem_}; }

 private:
  bool fail(const char* p) {
    problem_ = p;
    return false;
  }

  Lexer lex_;
  std::size_t nodes_ = 0;
  const char* problem_ = "";
};

}  // namespace

proto::Expected<Node, Error> parse(std::string_view text) {
  if (text.size() > kMaxText) return proto::Unexpected{Error{0, "the text is larger than 1 MiB"}};
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);  // a UTF-8 byte-order mark
  }
  Parser p(text);
  Node root;
  root.object = true;
  if (!p.object(root, 0, true)) return proto::Unexpected{p.error()};
  return root;
}

const Node* find(const Node& obj, std::string_view key) noexcept {
  for (const Node& c : obj.children) {
    if (same_key(c.key, key)) return &c;
  }
  return nullptr;
}

}  // namespace en::launcher::vdf
