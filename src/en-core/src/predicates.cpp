// The predicate DSL for Augur's `requires` at v0.1 (predicates.hpp; DESIGN §10.4).
#include "en/core/predicates.hpp"

#include <cctype>

namespace en::core {

namespace {

constexpr std::size_t kMaxExprBytes = 1024;
constexpr int kMaxDepth = 32;

class Parser {
 public:
  explicit Parser(std::string_view s) : s_(s) {}

  bool parse(std::vector<Predicates::Op>& out, std::string& error) {
    out_ = &out;
    if (!expr(0)) {
      error = error_;
      return false;
    }
    skip();
    if (at_ != s_.size()) {
      error = fail("unexpected text");
      return false;
    }
    return true;
  }

 private:
  std::string fail(const char* what) {
    if (error_.empty()) error_ = std::string(what) + " at column " + std::to_string(at_ + 1);
    return error_;
  }
  void skip() {
    while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\t')) ++at_;
  }
  bool eat(std::string_view tok) {
    skip();
    if (s_.substr(at_, tok.size()) == tok) {
      at_ += tok.size();
      return true;
    }
    return false;
  }
  std::string_view word() {
    skip();
    const std::size_t from = at_;
    while (at_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[at_])) != 0 || s_[at_] == '_')) ++at_;
    return s_.substr(from, at_ - from);
  }
  void emit(Predicates::Op op) { out_->push_back(op); }

  bool expr(int depth) {
    if (depth > kMaxDepth) {
      fail("nested deeper than 32");
      return false;
    }
    if (!conj(depth)) return false;
    while (eat("||")) {
      if (!conj(depth)) return false;
      emit({Predicates::Op::Code::Or});
    }
    return true;
  }
  bool conj(int depth) {
    if (!unary(depth)) return false;
    while (eat("&&")) {
      if (!unary(depth)) return false;
      emit({Predicates::Op::Code::And});
    }
    return true;
  }
  bool unary(int depth) {
    if (depth > kMaxDepth) {
      fail("nested deeper than 32");
      return false;
    }
    skip();
    if (at_ < s_.size() && s_[at_] == '!' && (at_ + 1 >= s_.size() || s_[at_ + 1] != '=')) {
      ++at_;
      if (!unary(depth + 1)) return false;
      emit({Predicates::Op::Code::Not});
      return true;
    }
    return primary(depth);
  }
  bool primary(int depth) {
    if (eat("(")) {
      if (!expr(depth + 1)) return false;
      if (!eat(")")) {
        fail("a '(' without its ')'");
        return false;
      }
      return true;
    }
    const std::size_t start = at_;
    const std::string_view w = word();
    if (w.empty()) {
      fail("a term is missing");
      return false;
    }
    skip();
    if (at_ < s_.size() && s_[at_] == '(') {
      at_ = start;
      skip();
      if (w == "world" || w == "knows") {
        fail("world() and knows() are refused in Augur's requires (MEM 12.10)");
      } else if (w == "flag" || w == "boss" || w == "stage") {
        fail("names an alias, and the flag catalogue holds none until v0.2");
      } else if (w == "region") {
        fail("names a region, and no v0.1 build loads regions.toml");
      } else {
        fail("calls an unknown predicate");
      }
      return false;
    }
    if (w == "true") {
      emit({Predicates::Op::Code::True});
      return true;
    }
    if (w == "false") {
      emit({Predicates::Op::Code::False});
      return true;
    }
    if (w == "runes") {  // MEM §6.6's great runes, through the catalogue's aliases (WHY§474)
      at_ = start;
      skip();
      fail("counts the great runes through catalogue aliases, which v0.1's predicates do not read (runes_held is the "
           "runes held)");
      return false;
    }
    if (w == "runes_held" || w == "journey") {
      Predicates::Op op{Predicates::Op::Code::Cmp};
      op.var = w == "runes_held" ? 0 : 1;
      if (eat(">=")) op.cmp = 0;
      else if (eat(">")) op.cmp = 1;
      else if (eat("<=")) op.cmp = 2;
      else if (eat("<")) op.cmp = 3;
      else if (eat("==")) op.cmp = 4;
      else if (eat("!=")) op.cmp = 5;
      else {
        fail("a comparison is missing");
        return false;
      }
      skip();
      const std::size_t from = at_;
      std::uint64_t n = 0;
      while (at_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[at_])) != 0 && at_ - from < 12) {
        n = n * 10 + static_cast<std::uint64_t>(s_[at_] - '0');
        ++at_;
      }
      if (at_ == from || (at_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[at_])) != 0)) {
        fail("a whole number of at most 12 digits is missing");
        return false;
      }
      op.n = n;
      emit(op);
      return true;
    }
    at_ = start;
    skip();
    fail("an unknown word");
    return false;
  }

  std::string_view s_;
  std::size_t at_ = 0;
  std::vector<Predicates::Op>* out_ = nullptr;
  std::string error_;
};

}  // namespace

proto::Expected<augur::PredicateId, std::string> Predicates::compile(std::string_view expr) const {
  if (expr.size() > kMaxExprBytes) return proto::Unexpected{std::string("is longer than 1 KB")};
  for (const char c : expr) {
    if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) >= 0x7F) {
      return proto::Unexpected{std::string("holds a character outside printable ASCII")};
    }
  }
  std::vector<Op> program;
  std::string error;
  Parser p(expr);
  if (!p.parse(program, error)) return proto::Unexpected{std::move(error)};
  programs_.push_back(std::move(program));
  return static_cast<augur::PredicateId>(programs_.size());  // ids from 1: 0 is "true"
}

bool Predicates::eval(augur::PredicateId id, const PredicateFacts& f) const noexcept {
  if (id == augur::kAlwaysTrue) return true;
  if (id > programs_.size()) return false;
  bool stack[64];
  std::size_t top = 0;
  for (const Op& op : programs_[id - 1]) {
    switch (op.code) {
      case Op::Code::True:
      case Op::Code::False:
      case Op::Code::Cmp: {
        if (top >= 64) return false;
        bool v = op.code == Op::Code::True;
        if (op.code == Op::Code::Cmp) {
          const std::uint64_t x = op.var == 0 ? f.runes_held : f.journey;
          switch (op.cmp) {
            case 0: v = x >= op.n; break;
            case 1: v = x > op.n; break;
            case 2: v = x <= op.n; break;
            case 3: v = x < op.n; break;
            case 4: v = x == op.n; break;
            default: v = x != op.n; break;
          }
        }
        stack[top++] = v;
        break;
      }
      case Op::Code::Not:
        if (top < 1) return false;
        stack[top - 1] = !stack[top - 1];
        break;
      case Op::Code::And:
      case Op::Code::Or: {
        if (top < 2) return false;
        const bool b = stack[--top];
        const bool a = stack[top - 1];
        stack[top - 1] = op.code == Op::Code::And ? (a && b) : (a || b);
        break;
      }
    }
  }
  return top == 1 && stack[0];
}

}  // namespace en::core
