// Compiling a template. inja parses it; this file then rewrites the parsed tree in place, once, so that every
// operator, function, condition, loop and printed value runs through checked code that knows its place in the
// template, and lints what can be known before rendering (NPC §7.1, WHY§99). inja's renderer then evaluates the
// rewritten tree as it is: its private state is never touched.
//
// Why a rewrite: inja 3.4's renderer keeps its state private, so it cannot be subclassed, but its parsed nodes are
// public. A FunctionNode whose operation is Callback runs the callback stored in the node itself, so each node can
// carry its own check, with its own line and column.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>

#include "detail.hpp"

namespace en::prompt::detail {
namespace {

using nlohmann::json;
using Op = inja::FunctionStorage::Operation;
using ExprPtr = std::shared_ptr<inja::ExpressionNode>;

[[noreturn]] void fail_at(const Site& site, const std::string& message) { throw TemplateError(message, site); }

// What to do instead, for a value that is not a boolean where one is needed.
std::string condition_advice(const json& value) {
  switch (kind_of(value)) {
    case kind::string:
      return ": a string never decides a condition (\"false\" would read as true); compare it, or test it with "
             "empty()";
    case kind::null:
      return ": null is a field left unset; test it with empty()";
    case kind::number:
      return ": compare it, as in n > 0";
    case kind::list:
    case kind::object:
      return ": test it with empty()";
    default:
      return "";
  }
}

// ----- checked replacements for inja's own operations -----

inja::CallbackFunction checked_condition(Site site, std::string role) {
  return [site = std::move(site), role = std::move(role)](inja::Arguments& args) -> json {
    const json& value = *args.at(0);
    if (!value.is_boolean()) {
      fail_at(site, role + " must be a boolean, got " + describe(kind_of(value)) + condition_advice(value));
    }
    return value;
  };
}

inja::CallbackFunction checked_not(Site site) {
  return [site = std::move(site)](inja::Arguments& args) -> json {
    const json& value = *args.at(0);
    if (!value.is_boolean()) {
      fail_at(site, "'not' takes a boolean, got " + describe(kind_of(value)) + condition_advice(value));
    }
    return !value.get<bool>();
  };
}

inja::CallbackFunction checked_in(Site site) {
  return [site = std::move(site)](inja::Arguments& args) -> json {
    const json& value = *args.at(0);
    const json& container = *args.at(1);
    const std::optional<bool> found = holds(container, value);
    if (!found) {
      fail_at(site, "'in' can't look for " + describe(kind_of(value)) + " in " + describe(kind_of(container)));
    }
    return *found;
  };
}

enum class Comparison { equal, not_equal, less, less_equal, greater, greater_equal };

const char* symbol(Comparison op) {
  switch (op) {
    case Comparison::equal:
      return "==";
    case Comparison::not_equal:
      return "!=";
    case Comparison::less:
      return "<";
    case Comparison::less_equal:
      return "<=";
    case Comparison::greater:
      return ">";
    case Comparison::greater_equal:
      return ">=";
  }
  return "?";
}

inja::CallbackFunction checked_comparison(Site site, Comparison op) {
  return [site = std::move(site), op](inja::Arguments& args) -> json {
    const json& a = *args.at(0);
    const json& b = *args.at(1);
    const std::string operation = std::string("'") + symbol(op) + "'";
    if (op == Comparison::equal || op == Comparison::not_equal) {
      // Any value may be compared with null; otherwise both sides must be of one kind, so that 3 == "3" or
      // true == "true" is a mistake caught rather than a quiet false.
      bool same = false;
      if (a.is_null() || b.is_null()) {
        same = a.is_null() && b.is_null();
      } else if (kind_of(a) != kind_of(b)) {
        fail_at(site, operation + " compares " + describe(kind_of(a)) + " with " + describe(kind_of(b)));
      } else {
        same = (a == b);
      }
      return op == Comparison::equal ? same : !same;
    }
    if (!(a.is_number() && b.is_number()) && !(a.is_string() && b.is_string())) {
      fail_at(site, operation + " takes two numbers or two strings, got " + describe(kind_of(a)) + " and " +
                        describe(kind_of(b)));
    }
    switch (op) {
      case Comparison::less:
        return a < b;
      case Comparison::less_equal:
        return a <= b;
      case Comparison::greater:
        return a > b;
      default:
        return a >= b;
    }
  };
}

enum class Arithmetic { add, subtract, multiply, divide, modulo, power };

const char* symbol(Arithmetic op) {
  switch (op) {
    case Arithmetic::add:
      return "+";
    case Arithmetic::subtract:
      return "-";
    case Arithmetic::multiply:
      return "*";
    case Arithmetic::divide:
      return "/";
    case Arithmetic::modulo:
      return "%";
    case Arithmetic::power:
      return "^";
  }
  return "?";
}

constexpr double whole_number_limit = 9.0e18;  // inside int64, with room for rounding

inja::CallbackFunction checked_arithmetic(Site site, Arithmetic op) {
  return [site = std::move(site), op](inja::Arguments& args) -> json {
    const json& a = *args.at(0);
    const json& b = *args.at(1);
    const std::string operation = std::string("'") + symbol(op) + "'";
    if (op == Arithmetic::add && a.is_string() && b.is_string()) {
      return a.get_ref<const std::string&>() + b.get_ref<const std::string&>();
    }
    if (!a.is_number() || !b.is_number()) {
      fail_at(site, operation + (op == Arithmetic::add ? " adds two numbers or joins two strings" : " takes two numbers") +
                        ", got " + describe(kind_of(a)) + " and " + describe(kind_of(b)));
    }
    const bool whole = a.is_number_integer() && b.is_number_integer();
    const double x = a.get<double>();
    const double y = b.get<double>();
    switch (op) {
      case Arithmetic::add:
      case Arithmetic::subtract:
      case Arithmetic::multiply: {
        const double approximate = op == Arithmetic::add ? x + y : (op == Arithmetic::subtract ? x - y : x * y);
        if (!whole) return approximate;
        if (!(std::fabs(approximate) < whole_number_limit)) fail_at(site, operation + " gives a number too large");
        const auto i = a.get<std::int64_t>();
        const auto j = b.get<std::int64_t>();
        return op == Arithmetic::add ? i + j : (op == Arithmetic::subtract ? i - j : i * j);
      }
      case Arithmetic::divide:
        if (y == 0) fail_at(site, "division by zero");
        return x / y;
      case Arithmetic::modulo: {
        if (!whole) fail_at(site, "'%' takes whole numbers");
        const auto j = b.get<std::int64_t>();
        if (j == 0) fail_at(site, "'%' by zero");
        if (j == -1) return 0;  // INT64_MIN % -1 would overflow
        return a.get<std::int64_t>() % j;
      }
      case Arithmetic::power: {
        const double result = std::pow(x, y);
        if (whole && b.get<std::int64_t>() >= 0 && std::fabs(result) < 9.0e15) {
          return static_cast<std::int64_t>(std::llround(result));
        }
        return result;
      }
    }
    return nullptr;
  };
}

bool is_index(std::string_view text) {
  return !text.empty() && text.size() < 10 && std::all_of(text.begin(), text.end(), [](char c) {
    return c >= '0' && c <= '9';
  });
}

// "(expression).field": inja's AtId, which would throw an uncaught exception for a missing field.
inja::CallbackFunction checked_field(Site site, std::string path) {
  return [site = std::move(site), path = std::move(path)](inja::Arguments& args) -> json {
    const json* current = args.at(0);
    for (const std::string& segment : split_dots(path)) {
      if (current->is_object()) {
        const auto it = current->find(segment);
        if (it == current->end()) fail_at(site, "this object has no field '" + segment + "'");
        current = &*it;
      } else if (current->is_array() && is_index(segment)) {
        const std::size_t index = std::stoul(segment);
        if (index >= current->size()) {
          fail_at(site, "index " + segment + " is outside a list of " + std::to_string(current->size()));
        }
        current = &(*current)[index];
      } else {
        fail_at(site, "can't read the field '" + segment + "' of " + describe(kind_of(*current)));
      }
    }
    return *current;
  };
}

inja::CallbackFunction checked_iterable(Site site, bool object) {
  return [site = std::move(site), object](inja::Arguments& args) -> json {
    const json& value = *args.at(0);
    if (object ? !value.is_object() : !value.is_array()) {
      fail_at(site, std::string(object ? "a for loop over keys and values needs an object"
                                       : "a for loop needs a list") +
                        ", got " + describe(kind_of(value)));
    }
    return value;
  };
}

// {{ value }}: only text, numbers and booleans print. Data is escaped so that its markers and braces stay text;
// the direct output of a render function is template text and keeps its markers.
inja::CallbackFunction checked_print(Site site, std::string name, bool markup) {
  return [site = std::move(site), name = std::move(name), markup](inja::Arguments& args) -> json {
    const json& value = *args.at(0);
    switch (value.type()) {
      case json::value_t::string:
        return markup ? value : json(escape_data(value.get_ref<const std::string&>()));
      case json::value_t::boolean:
        return value.get<bool>() ? "true" : "false";
      case json::value_t::number_integer:
      case json::value_t::number_unsigned:
      case json::value_t::number_float:
        return value.dump();
      case json::value_t::null: {
        const std::string what = name.empty() ? std::string("the value") : "'" + name + "'";
        const std::string guard = "{% if not empty(" + (name.empty() ? std::string("...") : name) + ") %}";
        fail_at(site, what + " is null, a field left unset: print it inside " + guard);
      }
      default:
        fail_at(site, "can't print " + describe(kind_of(value)) + " as text: use join() or to_json()");
    }
  };
}

// A registry function, with its arguments' kinds checked against its parameters before it runs.
inja::CallbackFunction checked_function(std::shared_ptr<const FunctionSpec> spec, Site site) {
  return [spec = std::move(spec), site = std::move(site)](inja::Arguments& args) -> json {
    for (std::size_t i = 0; i < args.size(); ++i) {
      const Param& param = i < spec->params.size() ? spec->params[i] : spec->params.back();
      const Kinds actual = kind_of(*args[i]);
      if ((actual & param.kinds) == 0) {
        fail_at(site, spec->name + "(): " + param.name + " must be " + describe(param.kinds) + ", got " +
                          describe(actual) + (actual == kind::null ? " (a field left unset)" : ""));
      }
    }
    const Call call(*spec, args, site, current_host());
    try {
      return spec->implementation(call);
    } catch (const TemplateError&) {
      throw;
    } catch (const std::exception& e) {
      fail_at(site, spec->name + "(): " + e.what());
    }
  };
}

bool is_comparison(Op op) {
  return op == Op::In || op == Op::Equal || op == Op::NotEqual || op == Op::Greater || op == Op::GreaterEqual ||
         op == Op::Less || op == Op::LessEqual;
}

bool is_arithmetic(Op op) {
  return op == Op::Add || op == Op::Subtract || op == Op::Multiplication || op == Op::Division || op == Op::Power ||
         op == Op::Modulo;
}

std::string arity_words(const FunctionSpec& spec) {
  const std::size_t low = spec.min_args();
  const auto count = [](std::size_t n) { return std::to_string(n) + (n == 1 ? " argument" : " arguments"); };
  if (spec.variadic) return "at least " + count(low);
  if (low == spec.max_args()) return count(low);
  return std::to_string(low) + " to " + count(spec.max_args());
}

// Rewrites one template's tree and gathers what the lint and the environment need from it.
class Rewriter {
 public:
  Rewriter(Compiled& out, const FunctionRegistry& registry, std::vector<Finding>& findings)
      : out_(out), content_(out.tmpl.content), registry_(registry), findings_(findings) {
    line_starts_.push_back(0);
    for (std::size_t i = 0; i < content_.size(); ++i) {
      if (content_[i] == '\n') line_starts_.push_back(i + 1);
    }
  }

  void run() {
    collect_set_names(out_.tmpl.root);
    block(out_.tmpl.root);
  }

 private:
  Site site(std::size_t pos) const {
    const auto after = std::upper_bound(line_starts_.begin(), line_starts_.end(), pos);
    const auto line = static_cast<std::size_t>(after - line_starts_.begin());
    return Site{out_.name, line, pos - line_starts_[line - 1] + 1};
  }

  void find(std::size_t pos, const char* rule, std::string message) {
    findings_.push_back(Finding{site(pos), rule, std::move(message)});
  }

  static void bind(inja::FunctionNode& node, inja::CallbackFunction check) {
    node.operation = Op::Callback;
    node.callback = std::move(check);
  }

  // A callback node around `inner`, for a check that inja's own node lacks.
  static ExprPtr wrap(ExprPtr inner, inja::CallbackFunction check) {
    auto node = std::make_shared<inja::FunctionNode>(std::string_view("__check"), inner->pos);
    node->number_args = 1;
    node->arguments.push_back(std::move(inner));
    node->callback = std::move(check);
    return node;
  }

  void collect_set_names(const inja::BlockNode& body) {
    for (const auto& node : body.nodes) {
      if (const auto set = std::dynamic_pointer_cast<inja::SetStatementNode>(node)) {
        set_names_.insert(split_dots(set->key).front());
      } else if (const auto test = std::dynamic_pointer_cast<inja::IfStatementNode>(node)) {
        collect_set_names(test->true_statement);
        collect_set_names(test->false_statement);
      } else if (const auto loop = std::dynamic_pointer_cast<inja::ForStatementNode>(node)) {
        collect_set_names(loop->body);
      } else if (const auto named = std::dynamic_pointer_cast<inja::BlockStatementNode>(node)) {
        collect_set_names(named->block);
      }
    }
  }

  void block(inja::BlockNode& body) {
    for (const auto& node : body.nodes) {
      if (const auto print_node = std::dynamic_pointer_cast<inja::ExpressionListNode>(node)) {
        print(*print_node);
      } else if (const auto test = std::dynamic_pointer_cast<inja::IfStatementNode>(node)) {
        if (test->condition.root) {
          lint_condition_value(test->condition.root);
          const Site at = site(test->condition.root->pos);
          test->condition.root = wrap(expression(test->condition.root), checked_condition(at, "a condition"));
        }
        block(test->true_statement);
        block(test->false_statement);
      } else if (const auto loop = std::dynamic_pointer_cast<inja::ForStatementNode>(node)) {
        for_loop(*loop, node);
      } else if (const auto set = std::dynamic_pointer_cast<inja::SetStatementNode>(node)) {
        if (set->expression.root) set->expression.root = expression(set->expression.root);
        out_.locals.emplace_back(split_dots(set->key).front(), site(set->pos));
      } else if (const auto include = std::dynamic_pointer_cast<inja::IncludeStatementNode>(node)) {
        find(include->pos, "include",
             "{% include %} is not used here: write {{ render_template(\"" + include->file + "\") }}");
      } else if (const auto extends = std::dynamic_pointer_cast<inja::ExtendsStatementNode>(node)) {
        find(extends->pos, "extends",
             "{% extends %} is not used here: compose with render_template() or a folder of numbered parts");
      } else if (const auto named = std::dynamic_pointer_cast<inja::BlockStatementNode>(node)) {
        block(named->block);
      }
    }
  }

  void for_loop(inja::ForStatementNode& loop, const std::shared_ptr<inja::AstNode>& node) {
    std::vector<std::string> names;
    bool object = false;
    if (const auto items = std::dynamic_pointer_cast<inja::ForArrayStatementNode>(node)) {
      names.push_back(items->value);
    } else if (const auto pairs = std::dynamic_pointer_cast<inja::ForObjectStatementNode>(node)) {
      names = {pairs->key, pairs->value};
      object = true;
    }
    for (const std::string& name : names) out_.locals.emplace_back(name, site(loop.pos));
    if (loop.condition.root) {
      const Site at = site(loop.condition.root->pos);
      loop.condition.root = wrap(expression(loop.condition.root), checked_iterable(at, object));
    }
    scopes_.push_back(std::move(names));
    ++loop_depth_;
    block(loop.body);
    --loop_depth_;
    scopes_.pop_back();
  }

  void print(inja::ExpressionListNode& node) {
    if (!node.root) return;  // inja reports an empty expression when it renders
    const ExprPtr original = node.root;
    bool markup = false;
    std::string name;
    if (const auto call = std::dynamic_pointer_cast<inja::FunctionNode>(original); call && !call->name.empty()) {
      if (const auto spec = registry_.find(call->name)) {
        markup = spec->markup;
        if ((spec->returns & (kind::string | kind::number | kind::boolean)) == 0) {
          find(call->pos, "print-structure",
               call->name + "() returns " + describe(spec->returns) +
                   ", which can't be printed as text: use join() or to_json()");
        }
      }
    } else if (const auto data = std::dynamic_pointer_cast<inja::DataNode>(original)) {
      name = data->name;
    }
    const Site at = site(original->pos);
    node.root = wrap(expression(original), checked_print(at, name, markup));
  }

  ExprPtr expression(ExprPtr node) {
    if (!node || std::dynamic_pointer_cast<inja::LiteralNode>(node)) return node;
    if (const auto data = std::dynamic_pointer_cast<inja::DataNode>(node)) return variable(data);
    if (const auto call = std::dynamic_pointer_cast<inja::FunctionNode>(node)) function(*call);
    return node;
  }

  ExprPtr variable(const std::shared_ptr<inja::DataNode>& node) {
    const std::string& name = node->name;
    if (name.find('-') != std::string::npos || name.find('/') != std::string::npos) {
      find(node->pos, "operator-in-name", "inja reads '" + name + "' as one name: put spaces around '-' and '/'");
    }
    std::vector<std::string> segments = split_dots(name);
    if (segments.front() == "loop") {
      if (loop_depth_ == 0) find(node->pos, "loop-outside-for", "'" + name + "' is used outside a for loop");
      record(name, node->pos, true);
      const std::string mapped = loop_alias(std::move(segments));
      if (mapped != name) return std::make_shared<inja::DataNode>(mapped, node->pos);
      return node;
    }
    if (segments.size() == 1) {
      if (const auto spec = registry_.find(name); spec && spec->min_args() == 0) {
        find(node->pos, "function-without-call", "'" + name + "' is a function: call it as " + name + "()");
      }
    }
    record(name, node->pos, is_bound(segments.front()));
    return node;
  }

  // Jinja's loop.first, loop.last and loop.index0 are inja's loop.is_first, loop.is_last and loop.index; stock
  // inja fails on the Jinja names and SkyrimNet's fork left them unresolved, so `not loop.last` was always true.
  static std::string loop_alias(std::vector<std::string> segments) {
    std::size_t i = 1;
    while (i < segments.size() && segments[i] == "parent") ++i;
    if (i < segments.size()) {
      if (segments[i] == "first") segments[i] = "is_first";
      else if (segments[i] == "last") segments[i] = "is_last";
      else if (segments[i] == "index0") segments[i] = "index";
    }
    std::string out;
    for (std::size_t k = 0; k < segments.size(); ++k) out += (k ? "." : "") + segments[k];
    return out;
  }

  void function(inja::FunctionNode& node) {
    const bool named = !node.name.empty();
    // `not` binds tighter than a comparison, so `not a == b` is `(not a) == b`. Look before the rewrite, which
    // changes the operations.
    if (!named && (is_comparison(node.operation) || is_arithmetic(node.operation)) && !node.arguments.empty()) {
      check_not_operand(node.arguments.front());
    }
    if (!named && node.operation == Op::AtId) {
      field_access(node);
      return;
    }
    if (!named && (node.operation == Op::And || node.operation == Op::Or || node.operation == Op::Not)) {
      for (const ExprPtr& argument : node.arguments) lint_condition_value(argument);
    }
    for (ExprPtr& argument : node.arguments) argument = expression(argument);
    if (named) {
      named_call(node);
      return;
    }
    const Site at = site(node.pos);
    switch (node.operation) {
      case Op::Not:
        bind(node, checked_not(at));
        break;
      case Op::And:
      case Op::Or: {
        // inja's own and/or stay, so they still stop at the first side that decides; each side is checked.
        const std::string role = std::string("each side of '") + (node.operation == Op::And ? "and" : "or") + "'";
        for (ExprPtr& argument : node.arguments) {
          argument = wrap(argument, checked_condition(site(argument->pos), role));
        }
        break;
      }
      case Op::In:
        bind(node, checked_in(at));
        break;
      case Op::Equal:
        bind(node, checked_comparison(at, Comparison::equal));
        break;
      case Op::NotEqual:
        bind(node, checked_comparison(at, Comparison::not_equal));
        break;
      case Op::Less:
        bind(node, checked_comparison(at, Comparison::less));
        break;
      case Op::LessEqual:
        bind(node, checked_comparison(at, Comparison::less_equal));
        break;
      case Op::Greater:
        bind(node, checked_comparison(at, Comparison::greater));
        break;
      case Op::GreaterEqual:
        bind(node, checked_comparison(at, Comparison::greater_equal));
        break;
      case Op::Add:
        bind(node, checked_arithmetic(at, Arithmetic::add));
        break;
      case Op::Subtract:
        bind(node, checked_arithmetic(at, Arithmetic::subtract));
        break;
      case Op::Multiplication:
        bind(node, checked_arithmetic(at, Arithmetic::multiply));
        break;
      case Op::Division:
        bind(node, checked_arithmetic(at, Arithmetic::divide));
        break;
      case Op::Modulo:
        bind(node, checked_arithmetic(at, Arithmetic::modulo));
        break;
      case Op::Power:
        bind(node, checked_arithmetic(at, Arithmetic::power));
        break;
      default:
        find(node.pos, "unsupported", "this operator is not supported");
        break;
    }
  }

  void named_call(inja::FunctionNode& node) {
    const auto spec = registry_.find(node.name);
    if (!spec) {
      std::string message = "unknown function '" + node.name + "'";
      const std::string close = closest_name(node.name, registry_);
      message += close.empty() ? " (render-prompt --list-functions lists them)" : ": did you mean " + close + "()?";
      find(node.pos, "unknown-function", message);
      return;
    }
    const std::size_t count = node.arguments.size();
    if (count < spec->min_args() || count > spec->max_args()) {
      find(node.pos, "arity", signature(*spec) + " takes " + arity_words(*spec) + ", not " + std::to_string(count));
      return;
    }
    if (spec->markup && count >= 1) {
      if (const auto literal = std::dynamic_pointer_cast<inja::LiteralNode>(node.arguments.front());
          literal && literal->value.is_string()) {
        out_.references.emplace_back(literal->value.get<std::string>(), site(node.pos));
      }
    }
    bind(node, checked_function(spec, site(node.pos)));
  }

  void field_access(inja::FunctionNode& node) {
    const auto key = node.arguments.size() == 2 ? std::dynamic_pointer_cast<inja::DataNode>(node.arguments[1]) : nullptr;
    if (!key) {
      find(node.pos, "syntax", "a '.' must be followed by a field name");
      return;
    }
    const std::string path = key->name;
    node.arguments[0] = expression(node.arguments[0]);
    node.arguments.resize(1);  // the field name is not a variable to look up
    node.number_args = 1;
    bind(node, checked_field(site(node.pos), path));
  }

  void check_not_operand(const ExprPtr& operand) {
    const auto negation = std::dynamic_pointer_cast<inja::FunctionNode>(operand);
    if (!negation || !negation->name.empty() || negation->operation != Op::Not) return;
    std::size_t i = negation->pos;
    while (i > 0 && is_space(content_[i - 1])) --i;
    if (i > 0 && content_[i - 1] == '(') return;  // "(not a) == b" says what it means
    find(negation->pos, "not-precedence",
         "'not' binds tighter than the operator after it: write not (a == b), or (not a) == b if that is meant");
  }

  // Lints what a condition, or a side of and/or/not, can be known to be before rendering.
  void lint_condition_value(const ExprPtr& node) {
    if (const auto literal = std::dynamic_pointer_cast<inja::LiteralNode>(node)) {
      if (!literal->value.is_boolean()) {
        find(literal->pos, "non-boolean-condition",
             "a condition must be a boolean, and this one is " + describe(kind_of(literal->value)));
      }
      return;
    }
    const auto call = std::dynamic_pointer_cast<inja::FunctionNode>(node);
    if (!call) return;
    if (!call->name.empty()) {
      const auto spec = registry_.find(call->name);
      if (spec && (spec->returns & kind::boolean) == 0) {
        find(call->pos, "non-boolean-condition",
             call->name + "() returns " + describe(spec->returns) + ", never a boolean, so it can't be a condition");
      }
    } else if (is_arithmetic(call->operation)) {
      find(call->pos, "non-boolean-condition", "arithmetic gives a number, never a boolean, so it can't be a condition");
    }
  }

  bool is_bound(const std::string& root) const {
    if (set_names_.count(root) != 0) return true;
    return std::any_of(scopes_.begin(), scopes_.end(), [&](const std::vector<std::string>& scope) {
      return std::find(scope.begin(), scope.end(), root) != scope.end();
    });
  }

  void record(const std::string& name, std::size_t pos, bool bound) {
    out_.variables.push_back(VariableUse{name, site(pos), bound});
  }

  Compiled& out_;
  const std::string& content_;
  const FunctionRegistry& registry_;
  std::vector<Finding>& findings_;
  std::vector<std::size_t> line_starts_;
  std::vector<std::vector<std::string>> scopes_;  // the loop variables in scope, innermost last
  std::set<std::string, std::less<>> set_names_;
  std::size_t loop_depth_ = 0;
};

Finding parse_finding(const std::string& name, const inja::InjaError& error, const FunctionRegistry& registry) {
  const Site site{name, error.location.line, error.location.column};
  const std::string unknown = "unknown function ";
  if (error.message.compare(0, unknown.size(), unknown) == 0) {
    const std::string function = error.message.substr(unknown.size());
    std::string message = "unknown function '" + function + "'";
    const std::string close = closest_name(function, registry);
    message += close.empty() ? " (render-prompt --list-functions lists them)" : ": did you mean " + close + "()?";
    return {site, "unknown-function", message};
  }
  if (error.message.find("got 'raw'") != std::string::npos) {
    return {site, "syntax",
            "this inja has no {% raw %}: print braces as data, as in {{ \"{{\" }}, and keep marker-like text "
            "between [ raw ] and [ end raw ]"};
  }
  if (error.message.find("got '|'") != std::string::npos) {
    return {site, "syntax", "this inja (3.4) has no filters: call the function instead, as in upper(x) for x | upper"};
  }
  return {site, "syntax", error.message};
}

}  // namespace

std::shared_ptr<Compiled> compile(inja::Environment& parser, const std::string& name, std::string text,
                                  const FunctionRegistry& registry, std::vector<Finding>& findings) {
  auto compiled = std::make_shared<Compiled>();
  compiled->name = name;
  try {
    compiled->tmpl = parser.parse(text);
  } catch (const inja::InjaError& error) {
    findings.push_back(parse_finding(name, error, registry));
    return nullptr;
  } catch (const nlohmann::json::exception&) {
    findings.push_back({Site{name}, "invalid-literal",
                        "a list or an object written in the template is not valid JSON: variables can't go inside "
                        "one (build it with object())"});
    return nullptr;
  }
  Rewriter(*compiled, registry, findings).run();
  return compiled;
}

}  // namespace en::prompt::detail
