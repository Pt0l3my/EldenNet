// Quest and world awareness: the world files, the predicates in three values, what holds and who knows it
// (knowledge.hpp; DESIGN §10.6; MEM §6, §7, §12).
#include "en/core/knowledge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <functional>
#include <set>
#include <tuple>

#include <toml.hpp>

#include "en/prompt/messages.hpp"  // estimate_tokens
#include "../content/schema.hpp"

namespace en::core {

namespace {

using namespace schema;
using Op = KnowledgeOp;

constexpr std::array<std::string_view, 3> kNg = {"reset", "keep", "unknown"};
constexpr std::array<std::string_view, 2> kKind = {"outcome", "state"};
constexpr std::array<std::string_view, 3> kReach = {"everywhere", "region", "linked"};
constexpr std::array<std::string_view, 2> kNear = {"fact", "rumour"};
constexpr std::array<std::string_view, 3> kCertaintyNames = {"fact", "rumour", "belief"};
constexpr std::array<std::string_view, 6> kPathNames = {"own", "kept", "tie", "sky", "near", "word"};

// ---- The files' schemas (MEM §6.1, §12.2; NPC §9.5) ----

constexpr std::array<Field, 5> kBlock = {required(text("alias", 0, 32)), required(integer("base", 0, 0xFFFFFFFF)),
                                         list("src", 0, 8, 0, 64), required(boolean("verified")),
                                         list("evidence", 0, 16, 0, 64)};
constexpr Spec kBlockSpec{kBlock};
constexpr std::array<Field, 14> kFlag = {
    required(text("alias", 0, 48)),
    integer("id", 0, 0xFFFFFFFF),
    text("block", 0, 32),
    integer("offset", 0, 19),
    required(choice("kind", kKind)),
    required(choice("ng", kNg)),
    list("src", 0, 8, 0, 64),
    required(boolean("verified")),
    list("evidence", 0, 16, 0, 64),
    required(integer("spoiler", 0, 3)),
    boolean("dlc"),
    boolean("core"),
    list("match", 0, 8, 1, 32),
    text("note", 0, 200),
};
constexpr Spec kFlagSpec{kFlag};
constexpr std::array<Field, 3> kCatalogue = {required(integer("schema", 1, 1)), tables("block", 0, 0, &kBlockSpec),
                                             tables("flag", 0, 0, &kFlagSpec)};
constexpr Spec kCatalogueSpec{kCatalogue};

constexpr std::array<Field, 2> kRegion = {required(text("key", 0, 32)), required(boolean("sky"))};
constexpr Spec kRegionSpec{kRegion};
constexpr std::array<Field, 2> kRegions = {required(integer("schema", 1, 1)), tables("region", 0, 64, &kRegionSpec)};
constexpr Spec kRegionsSpec{kRegions};

constexpr std::array<Field, 15> kStage = {
    required(text("id", 0, 32)),
    required(text("when", 0, 400)),
    text("boss", 0, 32),
    required(choice("reach", kReach)),
    list("regions", 0, 8, 0, 32),
    choice("near", kNear),
    required(integer("rank", 0, 100)),
    required(integer("spoiler", 0, 3)),
    list("supersedes", 0, 8, 0, 64),
    list("clears", 0, 16, 0, 64),
    required(text("short", 8)),
    required(text("line", 25, 160)),
    list("topics", 0, 12, 1, 32),
    required(list("lexicon", 0, 8, 0, 32)),
    text("note", 0, 200),
};
constexpr Spec kStageSpec{kStage};
constexpr std::array<Field, 2> kStages = {required(integer("schema", 1, 1)), tables("world_stage", 0, 32, &kStageSpec)};
constexpr Spec kStagesSpec{kStages};

constexpr std::array<Field, 6> kStep = {
    required(text("id", 0, 32)),        required(text("when", 0, 400)), required(integer("spoiler", 0, 3)),
    required(text("short", 8)),         required(text("heard", 25, 160)), required(list("lexicon", 0, 8, 0, 32)),
};
constexpr Spec kStepSpec{kStep};
constexpr std::array<Field, 4> kTrack = {required(text("id", 0, 32)), text("block", 0, 32), text("note", 0, 200),
                                         required(tables("step", 1, 16, &kStepSpec))};
constexpr Spec kTrackSpec{kTrack};
constexpr std::array<Field, 2> kTracks = {required(integer("schema", 1, 1)), tables("track", 0, 3, &kTrackSpec)};
constexpr Spec kTracksSpec{kTracks};

// ---- The predicate grammar (MEM §6.5) ----

struct Token {
  enum class Kind : std::uint8_t { Name, Str, Int, LParen, RParen, Comma, Not, And, Or, Ge, End, Bad } kind = Kind::End;
  std::string text;
  std::size_t col = 0;
};

std::vector<Token> lex(std::string_view s) {
  std::vector<Token> out;
  std::size_t i = 0;
  while (i < s.size()) {
    const char c = s[i];
    if (c == ' ' || c == '\t') {
      ++i;
      continue;
    }
    Token t;
    t.col = i + 1;
    if (c >= 'a' && c <= 'z') {
      std::size_t j = i;
      while (j < s.size() && ((s[j] >= 'a' && s[j] <= 'z') || s[j] == '_')) ++j;
      t.kind = Token::Kind::Name;
      t.text = std::string(s.substr(i, j - i));
      i = j;
    } else if (c >= '0' && c <= '9') {
      std::size_t j = i;
      while (j < s.size() && s[j] >= '0' && s[j] <= '9' && j - i < 9) ++j;
      t.kind = Token::Kind::Int;
      t.text = std::string(s.substr(i, j - i));
      i = j;
    } else if (c == '\'') {
      std::size_t j = i + 1;
      while (j < s.size() && s[j] != '\'') ++j;
      if (j >= s.size()) {
        t.kind = Token::Kind::Bad;
        i = s.size();
      } else {
        t.kind = Token::Kind::Str;
        t.text = std::string(s.substr(i + 1, j - i - 1));
        i = j + 1;
      }
    } else if (c == '(' || c == ')' || c == ',') {
      t.kind = c == '(' ? Token::Kind::LParen : c == ')' ? Token::Kind::RParen : Token::Kind::Comma;
      ++i;
    } else if (c == '!') {
      t.kind = Token::Kind::Not;
      ++i;
    } else if (c == '&' && i + 1 < s.size() && s[i + 1] == '&') {
      t.kind = Token::Kind::And;
      i += 2;
    } else if (c == '|' && i + 1 < s.size() && s[i + 1] == '|') {
      t.kind = Token::Kind::Or;
      i += 2;
    } else if (c == '>' && i + 1 < s.size() && s[i + 1] == '=') {
      t.kind = Token::Kind::Ge;
      i += 2;
    } else {
      t.kind = Token::Kind::Bad;
      i = s.size();
    }
    out.push_back(std::move(t));
    if (out.back().kind == Token::Kind::Bad) break;
  }
  Token end;
  end.col = s.size() + 1;
  out.push_back(end);
  return out;
}

// Where a predicate is used: what it may name (W1: a world stage's `when` uses outcome flags and world() alone).
enum class Use : std::uint8_t { WorldStage, Step, PersonaStage, Tie };

// An alias (MEM §6.1): ^[a-z][a-z0-9_]*(\.[a-z0-9_]+){1,3}$, checked by hand (no static object: ARCH §3.1).
bool is_alias(std::string_view s) {
  if (s.empty() || s.size() > 48 || !(s[0] >= 'a' && s[0] <= 'z')) return false;
  std::size_t dots = 0;
  bool after_dot = false;
  for (const char c : s) {
    if (c == '.') {
      if (after_dot) return false;
      ++dots;
      after_dot = true;
      continue;
    }
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    after_dot = false;
  }
  return dots >= 1 && dots <= 3 && !after_dot;
}

}  // namespace

// The compiler and the evaluator need the content's names: a friend-like helper with access through Knowledge's
// public parts and the program table it builds.
struct KnowledgeBuilder {
  Knowledge& k;
  const ContentPack& pack;
  Errors& errors;
  // Program slots: each predicate is compiled into programs_[slot]; `stage()` names a slot.
  std::map<std::pair<std::string, std::string>, std::size_t> stage_slot;  // (track or persona, step or stage) -> slot
  std::vector<std::vector<std::size_t>> deps;                             // slot -> the slots it reads

  std::size_t new_slot() {
    k.programs_.emplace_back();
    deps.emplace_back();
    return k.programs_.size() - 1;
  }

  std::optional<std::size_t> world_index(std::string_view id) const {
    for (std::size_t i = 0; i < k.stages_.size(); ++i) {
      if (k.stages_[i].id == id) return i;
    }
    return std::nullopt;
  }
  std::optional<std::size_t> region_index(std::string_view key) const {
    for (std::size_t i = 0; i < k.regions_.size(); ++i) {
      if (k.regions_[i].key == key) return i;
    }
    return std::nullopt;
  }

  // Compiles `expr` into `slot`; a problem is reported against (file, line, key) in fixed words and the column.
  void compile(std::size_t slot, std::string_view expr, Use use, const std::string& file, std::size_t line,
               const std::string& key) {
    std::vector<Token> toks = lex(expr);
    std::size_t pos = 0;
    std::size_t atoms = 0;
    std::vector<Op> prog;
    std::string problem;
    std::size_t at = 0;
    const auto fail = [&](std::size_t col, const char* what) {
      if (problem.empty()) {
        problem = what;
        at = col;
      }
    };
    const auto peek = [&]() -> const Token& { return toks[std::min(pos, toks.size() - 1)]; };
    const auto take = [&]() -> const Token& { return toks[std::min(pos++, toks.size() - 1)]; };
    const auto expect = [&](Token::Kind kind, const char* what) {
      if (peek().kind != kind) {
        fail(peek().col, what);
        return false;
      }
      ++pos;
      return true;
    };
    std::function<void(int)> or_expr;
    std::function<void(int)> unary;
    const auto string_arg = [&]() -> std::optional<std::string> {
      if (!expect(Token::Kind::LParen, "a call needs (")) return std::nullopt;
      if (peek().kind != Token::Kind::Str) {
        fail(peek().col, peek().kind == Token::Kind::Int ? "a call takes a quoted name, never a number"
                                                         : "a call needs a quoted name");
        return std::nullopt;
      }
      return take().text;
    };
    const auto atom = [&](int depth) {
      const Token& t = take();
      if (t.kind == Token::Kind::LParen) {
        or_expr(depth + 1);
        expect(Token::Kind::RParen, "a ( is not closed");
        return;
      }
      if (t.kind != Token::Kind::Name) {
        fail(t.col, "a term is expected");
        return;
      }
      if (++atoms > 32) fail(t.col, "more than 32 atoms");
      if (t.text == "true" || t.text == "false") {
        prog.push_back(Op{Op::Code::Const, t.text == "true" ? 1u : 0u});
        return;
      }
      if (t.text == "runes" || t.text == "journey") {
        const bool runes = t.text == "runes";
        if (use == Use::WorldStage && !runes) fail(t.col, "a world stage's when uses outcome flags and world() only (W1)");
        if (!expect(Token::Kind::Ge, "runes and journey take >= and a number")) return;
        if (peek().kind != Token::Kind::Int) {
          fail(peek().col, "runes and journey take >= and a number");
          return;
        }
        const long n = std::stol(take().text);
        if (runes && (n < 0 || n > 7)) fail(t.col, "runes takes 0 to 7");
        if (!runes && (n < 1 || n > 8)) fail(t.col, "journey takes 1 to 8");
        if (runes && n > 0 && !k.catalogue_.find("runes.ge" + std::to_string(n))) {
          fail(t.col, "runes needs its runes.ge alias in the catalogue");
        }
        prog.push_back(Op{runes ? Op::Code::Runes : Op::Code::Journey, static_cast<std::uint32_t>(n)});
        return;
      }
      const std::string name = t.text;
      if (name == "flag" || name == "boss") {
        const auto a = string_arg();
        if (!a) return;
        expect(Token::Kind::RParen, "a call is not closed");
        const std::string alias = name == "boss" ? "boss." + *a : *a;
        if (!is_alias(alias)) {
          fail(t.col, "not an alias");
          return;
        }
        if (alias.rfind("grace.", 0) == 0) fail(t.col, "grace flags never gate content");
        const auto i = k.catalogue_.find(alias);
        if (!i) {
          fail(t.col, name == "boss" ? "boss() names no boss alias of the catalogue" : "flag() names no alias of the catalogue");
          return;
        }
        prog.push_back(Op{Op::Code::Flag, static_cast<std::uint32_t>(*i)});
        return;
      }
      if (name == "world") {
        const auto a = string_arg();
        if (!a) return;
        expect(Token::Kind::RParen, "a call is not closed");
        const auto i = world_index(*a);
        if (!i) {
          fail(t.col, "world() names no world stage");
          return;
        }
        deps[slot].push_back(k.stage_when_[*i]);
        prog.push_back(Op{Op::Code::World, static_cast<std::uint32_t>(*i)});
        return;
      }
      if (name == "region") {
        const auto a = string_arg();
        if (!a) return;
        expect(Token::Kind::RParen, "a call is not closed");
        if (use == Use::WorldStage) fail(t.col, "a world stage's when uses outcome flags and world() only (W1)");
        const auto i = region_index(*a);
        if (!i) {
          fail(t.col, "region() names no region of regions.toml");
          return;
        }
        prog.push_back(Op{Op::Code::Region, static_cast<std::uint32_t>(*i)});
        return;
      }
      if (name == "stage") {
        const auto a = string_arg();
        if (!a) return;
        if (!expect(Token::Kind::Comma, "stage() takes two quoted names")) return;
        if (peek().kind != Token::Kind::Str) {
          fail(peek().col, "stage() takes two quoted names");
          return;
        }
        const std::string b = take().text;
        expect(Token::Kind::RParen, "a call is not closed");
        if (use == Use::WorldStage) fail(t.col, "a world stage's when uses outcome flags and world() only (W1)");
        const auto it = stage_slot.find({*a, b});
        if (it == stage_slot.end()) {
          fail(t.col, "stage() names no persona's or track's stage");
          return;
        }
        deps[slot].push_back(it->second);
        prog.push_back(Op{Op::Code::Stage, static_cast<std::uint32_t>(it->second)});
        return;
      }
      if (name == "knows") {
        fail(t.col, "knows() belongs to the KB's requires, which v0.2 does not read");
        return;
      }
      fail(t.col, "an unknown call");
    };
    unary = [&](int depth) {
      if (depth > 8) {
        fail(peek().col, "nests deeper than 8");
        return;
      }
      if (peek().kind == Token::Kind::Not) {
        const Token& t = take();
        if (use == Use::WorldStage) fail(t.col, "a world stage's when uses outcome flags and world() only (W1)");
        unary(depth + 1);
        prog.push_back(Op{Op::Code::Not, 0});
        return;
      }
      atom(depth);
    };
    const auto and_expr = [&](int depth) {
      unary(depth);
      while (problem.empty() && peek().kind == Token::Kind::And) {
        ++pos;
        unary(depth);
        prog.push_back(Op{Op::Code::And, 0});
      }
    };
    or_expr = [&](int depth) {
      and_expr(depth);
      while (problem.empty() && peek().kind == Token::Kind::Or) {
        ++pos;
        and_expr(depth);
        prog.push_back(Op{Op::Code::Or, 0});
      }
    };
    if (toks.size() == 1) fail(1, "is empty");
    or_expr(0);
    if (problem.empty() && peek().kind != Token::Kind::End) fail(peek().col, "a term is left over");
    if (!problem.empty()) {
      errors.add(file, line, key + ": " + problem + " (column " + std::to_string(at) + ")");
      k.programs_[slot] = {Op{Op::Code::Const, 0}};
      deps[slot].clear();
      return;
    }
    k.programs_[slot] = std::move(prog);
  }

  // A cycle among stage() and world() is an error, and a chain of stage() deeper than 4 too (MEM §6.5).
  void check_cycles(const std::vector<std::tuple<std::size_t, std::string, std::size_t, std::string>>& where) {
    std::vector<std::uint8_t> mark(k.programs_.size(), 0);  // 0 new, 1 on the path, 2 done
    std::vector<std::size_t> depth(k.programs_.size(), 0);
    std::function<bool(std::size_t)> visit = [&](std::size_t s) -> bool {
      if (mark[s] == 1) return false;
      if (mark[s] == 2) return true;
      mark[s] = 1;
      std::size_t d = 0;
      for (const std::size_t n : deps[s]) {
        if (!visit(n)) return false;
        d = std::max(d, depth[n] + 1);
      }
      depth[s] = d;
      mark[s] = 2;
      return true;
    };
    for (const auto& [slot, file, line, key] : where) {
      std::fill(mark.begin(), mark.end(), std::uint8_t{0});
      if (!visit(slot)) {
        errors.add(file, line, key + ": stage() or world() reads itself in a cycle");
        for (std::size_t s = 0; s < k.programs_.size(); ++s) deps[s].clear();  // evaluation must end
        for (auto& p : k.programs_) p = {Op{Op::Code::Const, 0}};
        return;
      }
      if (depth[slot] > 4) errors.add(file, line, key + ": stage() and world() chain deeper than 4");
    }
  }
};

namespace {

// ---- Evaluation (MEM §6.6) ----

Tri kleene_not(Tri v) { return v == Tri::T ? Tri::F : v == Tri::F ? Tri::T : Tri::U; }
Tri kleene_and(Tri a, Tri b) {
  if (a == Tri::F || b == Tri::F) return Tri::F;
  return a == Tri::T && b == Tri::T ? Tri::T : Tri::U;
}
Tri kleene_or(Tri a, Tri b) {
  if (a == Tri::T || b == Tri::T) return Tri::T;
  return a == Tri::F && b == Tri::F ? Tri::F : Tri::U;
}

}  // namespace

struct KnowledgeEval {
  const std::vector<std::vector<KnowledgeOp>>& programs;
  const std::vector<std::size_t>& stage_when;
  const std::vector<WorldStage>& stages;
  const std::vector<Region>& regions;
  const Catalogue& catalogue;
  const FlagState& fs;
  std::vector<std::optional<Tri>> world;  // memo per world stage

  Tri flag(std::size_t i) const { return fs.flag(i); }

  Tri runes(std::uint32_t n) const {
    if (n == 0) return Tri::T;
    const auto i = catalogue.find("runes.ge" + std::to_string(n));
    return i ? flag(*i) : Tri::U;
  }

  Tri world_stage(std::size_t x) {
    if (world[x]) return *world[x];
    world[x] = Tri::U;  // a cycle is refused at load; this only bounds a defect
    Tri v = run(stage_when[x]);
    if (v == Tri::T && stages[x].spoiler >= 2 && !atoms_ok(stage_when[x])) v = Tri::U;  // MEM §7.4
    world[x] = v;
    return v;
  }

  Tri run(std::size_t slot) {
    std::vector<Tri> st;
    for (const KnowledgeOp& op : programs[slot]) {
      switch (op.code) {
        case KnowledgeOp::Code::Const:
          st.push_back(op.a != 0 ? Tri::T : Tri::F);
          break;
        case KnowledgeOp::Code::Flag:
          st.push_back(flag(op.a));
          break;
        case KnowledgeOp::Code::Runes:
          st.push_back(runes(op.a));
          break;
        case KnowledgeOp::Code::Journey:
          st.push_back(fs.journey >= op.a ? Tri::T : Tri::F);
          break;
        case KnowledgeOp::Code::Region:
          st.push_back(fs.region ? (*fs.region == regions[op.a].key ? Tri::T : Tri::F) : Tri::U);
          break;
        case KnowledgeOp::Code::Stage:
          st.push_back(run(op.a));
          break;
        case KnowledgeOp::Code::World:
          st.push_back(world_stage(op.a));
          break;
        case KnowledgeOp::Code::Not:
          if (!st.empty()) st.back() = kleene_not(st.back());
          break;
        case KnowledgeOp::Code::And:
        case KnowledgeOp::Code::Or: {
          if (st.size() < 2) return Tri::U;
          const Tri b = st.back();
          st.pop_back();
          st.back() = op.code == KnowledgeOp::Code::And ? kleene_and(st.back(), b) : kleene_or(st.back(), b);
          break;
        }
      }
    }
    return st.size() == 1 ? st.back() : Tri::U;
  }

  // Every flag atom of the program, through stage() and world(), verified and valid (MEM §7.4).
  bool atoms_ok(std::size_t slot) const {
    for (const KnowledgeOp& op : programs[slot]) {
      switch (op.code) {
        case KnowledgeOp::Code::Flag:
          if (!fs.have_frame || op.a >= fs.verified.size() || fs.verified[op.a] == 0 || fs.valid[op.a] == 0) return false;
          break;
        case KnowledgeOp::Code::Runes:
          if (op.a > 0) {
            const auto i = catalogue.find("runes.ge" + std::to_string(op.a));
            if (!i || !fs.have_frame || fs.verified[*i] == 0 || fs.valid[*i] == 0) return false;
          }
          break;
        case KnowledgeOp::Code::Stage:
          if (!atoms_ok(op.a)) return false;
          break;
        case KnowledgeOp::Code::World:
          if (!atoms_ok(stage_when[op.a])) return false;
          break;
        default:
          break;
      }
    }
    return true;
  }
};

const char* to_string(Tri v) noexcept { return v == Tri::T ? "T" : v == Tri::F ? "F" : "U"; }
const char* to_string(Certainty v) noexcept {
  const auto i = static_cast<std::size_t>(v);
  return i < kCertaintyNames.size() ? kCertaintyNames[i].data() : "rumour";
}
const char* to_string(Path v) noexcept {
  const auto i = static_cast<std::size_t>(v);
  return i < kPathNames.size() ? kPathNames[i].data() : "word";
}

std::optional<std::size_t> Catalogue::find(std::string_view alias) const noexcept {
  for (std::size_t i = 0; i < flags.size(); ++i) {
    if (flags[i].alias == alias) return i;
  }
  return std::nullopt;
}

const CatalogueBlock* Catalogue::block(std::string_view alias) const noexcept {
  for (const CatalogueBlock& b : blocks) {
    if (b.alias == alias) return &b;
  }
  return nullptr;
}

bool Catalogue::verified(std::size_t flag, Realm realm) const noexcept {
  if (flag >= flags.size()) return false;
  return flags[flag].verified && (!flags[flag].ci || realm == Realm::Eval);
}

Tri FlagState::flag(std::size_t i) const noexcept {
  if (!have_frame || i >= bits.size() || verified[i] == 0 || valid[i] == 0) return Tri::U;
  return bits[i] != 0 ? Tri::T : Tri::F;
}

namespace {

std::string at_index(const char* table, std::size_t i) { return std::string(table) + "[" + std::to_string(i) + "]"; }

const ContentFile* other(const ContentPack& pack, std::string_view path) {
  for (const ContentFile& f : pack.others) {
    if (f.path == path) return &f;
  }
  return nullptr;
}

std::optional<Certainty> certainty_of(std::string_view s) {
  for (std::size_t i = 0; i < kCertaintyNames.size(); ++i) {
    if (kCertaintyNames[i] == s) return static_cast<Certainty>(i);
  }
  return std::nullopt;
}

std::vector<std::string> json_strings(const nlohmann::json& j, const char* key) {
  std::vector<std::string> out;
  if (const auto it = j.find(key); it != j.end() && it->is_array()) {
    for (const auto& v : *it) {
      if (v.is_string()) out.push_back(v.get<std::string>());
    }
  }
  return out;
}

std::string json_string(const nlohmann::json& j, const char* key) {
  if (const auto it = j.find(key); it != j.end() && it->is_string()) return it->get<std::string>();
  return {};
}

}  // namespace

proto::Expected<Knowledge, std::vector<ContentError>> Knowledge::build(const ContentPack& pack) {
  std::vector<ContentError> problems;
  Errors errors(problems);
  Knowledge k;
  KnowledgeBuilder b{k, pack, errors, {}, {}};

  // flags/catalog.toml
  if (const ContentFile* f = other(pack, kCataloguePath)) {
    const std::size_t before = errors.count();
    if (auto t = parse(*f, kContentFileMaxBytes, errors)) {
      Checker(f->path, errors).check(*t, kCatalogueSpec, "");
      if (errors.count() == before) {
        if (const auto* blocks = (*t)["block"].as_array()) {
          for (std::size_t i = 0; i < blocks->size(); ++i) {
            const toml::table& bt = *blocks->get(i)->as_table();
            CatalogueBlock cb;
            cb.alias = bt["alias"].value_or(std::string());
            cb.base = static_cast<std::uint32_t>(bt["base"].value_or(std::int64_t{0}));
            cb.verified = bt["verified"].value_or(false);
            if (!is_slug(cb.alias, 32)) errors.add(f->path, line_of(*blocks->get(i)), at_index("block", i) + ".alias: must be a slug");
            if (k.catalogue_.block(cb.alias) != nullptr) {
              errors.add(f->path, line_of(*blocks->get(i)), at_index("block", i) + ".alias: is not unique");
            }
            k.catalogue_.blocks.push_back(std::move(cb));
          }
        }
        if (const auto* flags = (*t)["flag"].as_array()) {
          for (std::size_t i = 0; i < flags->size(); ++i) {
            const toml::table& ft = *flags->get(i)->as_table();
            const std::size_t ln = line_of(*flags->get(i));
            const std::string at = at_index("flag", i);
            CatalogueFlag cf;
            cf.alias = ft["alias"].value_or(std::string());
            cf.block = ft["block"].value_or(std::string());
            cf.offset = static_cast<std::uint8_t>(ft["offset"].value_or(std::int64_t{0}));
            cf.outcome = ft["kind"].value_or(std::string()) == "outcome";
            cf.ng = enum_of<Ng>(ft["ng"].value_or(std::string()), kNg, Ng::Unknown);
            cf.verified = ft["verified"].value_or(false);
            cf.spoiler = static_cast<std::uint8_t>(ft["spoiler"].value_or(std::int64_t{0}));
            cf.dlc = ft["dlc"].value_or(false);
            cf.core = ft["core"].value_or(false);
            cf.match = strings_at(ft, "match");
            const std::vector<std::string> evidence = strings_at(ft, "evidence");
            cf.ci = std::any_of(evidence.begin(), evidence.end(), [](const std::string& e) { return e.rfind("ci:", 0) == 0; });
            if (!is_alias(cf.alias)) errors.add(f->path, ln, at + ".alias: must be an alias, as boss.godrick");
            if (k.catalogue_.find(cf.alias)) errors.add(f->path, ln, at + ".alias: is not unique");
            const bool has_id = ft.contains("id");
            const bool has_block = ft.contains("block") || ft.contains("offset");
            if (has_id == has_block) errors.add(f->path, ln, at + ": needs an id, or a block and its offset");
            if (has_block && !(ft.contains("block") && ft.contains("offset"))) {
              errors.add(f->path, ln, at + ": a block flag needs both its block and its offset");
            }
            if (has_id) {
              cf.id = static_cast<std::uint32_t>(ft["id"].value_or(std::int64_t{0}));
            } else if (const CatalogueBlock* blk = k.catalogue_.block(cf.block)) {
              cf.id = blk->base == 0 ? 0 : blk->base + cf.offset;
            } else if (!cf.block.empty()) {
              errors.add(f->path, ln, at + ".block: names no block of the catalogue");
            }
            if (cf.verified && evidence.empty()) errors.add(f->path, ln, at + ".verified: needs its evidence (MEM §6.3)");
            if (cf.verified && cf.id == 0) errors.add(f->path, ln, at + ".verified: an unresolved alias is never verified");
            k.catalogue_.flags.push_back(std::move(cf));
          }
        }
      }
    }
  }

  // regions.toml
  if (const ContentFile* f = other(pack, kRegionsPath)) {
    const std::size_t before = errors.count();
    if (auto t = parse(*f, kContentFileMaxBytes, errors)) {
      Checker(f->path, errors).check(*t, kRegionsSpec, "");
      if (errors.count() == before) {
        if (const auto* rs = (*t)["region"].as_array()) {
          for (std::size_t i = 0; i < rs->size(); ++i) {
            const toml::table& rt = *rs->get(i)->as_table();
            Region r{rt["key"].value_or(std::string()), rt["sky"].value_or(false)};
            if (!is_region(r.key)) errors.add(f->path, line_of(*rs->get(i)), at_index("region", i) + ".key: must be a region key");
            if (b.region_index(r.key)) errors.add(f->path, line_of(*rs->get(i)), at_index("region", i) + ".key: is not unique");
            k.regions_.push_back(std::move(r));
          }
        }
      }
    }
  }

  // Ids are unique across world stages, tracks and personas (MEM §12.2), so world() and stage() never collide.
  std::set<std::string> ids;
  for (const Persona& p : pack.personas) ids.insert(p.id);

  // world/stages.toml: read first, compiled once every slot exists.
  struct Pending {
    std::size_t slot;
    std::string when;
    Use use;
    std::string file;
    std::size_t line;
    std::string key;
  };
  std::vector<Pending> pending;
  if (const ContentFile* f = other(pack, kWorldStagesPath)) {
    const std::size_t before = errors.count();
    if (auto t = parse(*f, kContentFileMaxBytes, errors)) {
      Checker(f->path, errors).check(*t, kStagesSpec, "");
      if (errors.count() == before) {
        if (const auto* ss = (*t)["world_stage"].as_array()) {
          for (std::size_t i = 0; i < ss->size(); ++i) {
            const toml::table& st = *ss->get(i)->as_table();
            const std::size_t ln = line_of(*ss->get(i));
            const std::string at = at_index("world_stage", i);
            WorldStage w;
            w.id = st["id"].value_or(std::string());
            w.when = st["when"].value_or(std::string());
            w.boss = st["boss"].value_or(std::string());
            w.reach = enum_of<Reach>(st["reach"].value_or(std::string()), kReach, Reach::Linked);
            w.regions = strings_at(st, "regions");
            w.near = st["near"].value_or(std::string("rumour")) == "fact" ? Certainty::Fact : Certainty::Rumour;
            w.rank = static_cast<std::uint8_t>(st["rank"].value_or(std::int64_t{0}));
            w.spoiler = static_cast<std::uint8_t>(st["spoiler"].value_or(std::int64_t{0}));
            w.supersedes = strings_at(st, "supersedes");
            w.short_text = st["short"].value_or(std::string());
            w.line = st["line"].value_or(std::string());
            w.topics = strings_at(st, "topics");
            w.source_line = ln;
            if (!is_slug(w.id, 32)) errors.add(f->path, ln, at + ".id: must be a slug of at most 32 characters");
            if (!ids.insert(w.id).second) errors.add(f->path, ln, at + ".id: is not unique among world stages, tracks and personas");
            if (w.reach == Reach::Region && w.regions.empty()) errors.add(f->path, ln, at + ".regions: reach = region needs them");
            for (const std::string& r : w.regions) {
              if (!b.region_index(r)) errors.add(f->path, ln, at + ".regions: names no region of regions.toml");
            }
            if (!w.boss.empty() && !k.catalogue_.find("boss." + w.boss)) {
              errors.add(f->path, ln, at + ".boss: names no boss alias of the catalogue");
            }
            k.stage_when_.push_back(b.new_slot());
            pending.push_back(Pending{k.stage_when_.back(), w.when, Use::WorldStage, f->path, ln, at + ".when"});
            k.stages_.push_back(std::move(w));
          }
          for (std::size_t i = 0; i < k.stages_.size(); ++i) {
            for (const std::string& sup : k.stages_[i].supersedes) {
              if (sup.rfind("w:", 0) != 0 || !b.world_index(std::string_view(sup).substr(2))) {
                errors.add(f->path, k.stages_[i].source_line, at_index("world_stage", i) + ".supersedes: names no world stage");
              }
            }
          }
        }
      }
    }
  }

  // world/tracks.toml
  if (const ContentFile* f = other(pack, kTracksPath)) {
    const std::size_t before = errors.count();
    if (auto t = parse(*f, kContentFileMaxBytes, errors)) {
      Checker(f->path, errors).check(*t, kTracksSpec, "");
      if (errors.count() == before) {
        if (const auto* ts = (*t)["track"].as_array()) {
          for (std::size_t i = 0; i < ts->size(); ++i) {
            const toml::table& tt = *ts->get(i)->as_table();
            const std::size_t ln = line_of(*ts->get(i));
            const std::string at = at_index("track", i);
            Track tr;
            tr.id = tt["id"].value_or(std::string());
            tr.block = tt["block"].value_or(std::string());
            if (!is_slug(tr.id, 32)) errors.add(f->path, ln, at + ".id: must be a slug of at most 32 characters");
            if (!ids.insert(tr.id).second) errors.add(f->path, ln, at + ".id: is not unique among world stages, tracks and personas");
            if (!tr.block.empty() && k.catalogue_.block(tr.block) == nullptr) {
              errors.add(f->path, ln, at + ".block: names no block of the catalogue");
            }
            std::vector<std::size_t> slots;
            std::set<std::string> step_ids;
            const toml::array& steps = *tt["step"].as_array();
            for (std::size_t j = 0; j < steps.size(); ++j) {
              const toml::table& sp = *steps.get(j)->as_table();
              const std::size_t sl = line_of(*steps.get(j));
              TrackStep step;
              step.id = sp["id"].value_or(std::string());
              step.when = sp["when"].value_or(std::string());
              step.spoiler = static_cast<std::uint8_t>(sp["spoiler"].value_or(std::int64_t{0}));
              step.short_text = sp["short"].value_or(std::string());
              step.heard = sp["heard"].value_or(std::string());
              const std::string sat = at + "." + at_index("step", j);
              if (!is_slug(step.id, 32) || !step_ids.insert(step.id).second) {
                errors.add(f->path, sl, sat + ".id: must be a slug, unique in its track");
              }
              slots.push_back(b.new_slot());
              b.stage_slot[{tr.id, step.id}] = slots.back();
              pending.push_back(Pending{slots.back(), step.when, Use::Step, f->path, sl, sat + ".when"});
              tr.steps.push_back(std::move(step));
            }
            k.step_when_.push_back(std::move(slots));
            k.tracks_.push_back(std::move(tr));
          }
        }
      }
    }
  }

  // Each row's flag block agrees with the catalogue's block of that alias, the source of truth (MEM §6.1).
  for (const PersonaRow& row : pack.rows) {
    if (!row.enabled || !row.flag_block) continue;
    const CatalogueBlock* blk = k.catalogue_.block(row.flag_block->alias);
    if (blk == nullptr || blk->base != row.flag_block->base) {
      errors.add(std::string(kPersonaIndexPath), row.line, "persona.flag_block: must agree with the catalogue's block");
    }
  }
  // The personas' stages and awareness, from their records (content.cpp checked their schema).
  for (const Persona& p : pack.personas) {
    KnowledgePersona info;
    info.id = p.id;
    const nlohmann::json& rec = p.record;
    const auto rs = rec.find("stages");
    std::string at, hears = "all", road;
    std::vector<std::string> knows;
    if (rs != rec.end() && rs->is_array()) {
      for (std::size_t i = 0; i < rs->size(); ++i) {
        const nlohmann::json& st = (*rs)[i];
        KnowledgePersona::StageInfo si;
        si.id = json_string(st, "id");
        if (st.contains("at")) at = json_string(st, "at");
        if (st.contains("hears")) hears = json_string(st, "hears");
        if (st.contains("road")) road = json_string(st, "road");
        if (st.contains("knows")) knows = json_strings(st, "knows");
        si.at = at;
        si.hears = hears;
        si.road = road;
        si.heard = json_string(st, "heard");
        si.knows = knows;
        si.shows = i < p.stages.size() && p.stages[i].shows;
        const std::size_t ln = i < p.stages.size() ? p.stages[i].line : 0;
        const std::string sat = "stages[" + std::to_string(i) + "]";
        if (!k.regions_.empty() && st.contains("at") && !b.region_index(at)) {
          errors.add(p.file, ln, sat + ".at: names no region of regions.toml");
        }
        for (const std::string& x : json_strings(st, "knows")) {
          if (x.rfind("w:", 0) != 0 || !b.world_index(std::string_view(x).substr(2))) {
            errors.add(p.file, ln, sat + ".knows: names no world stage");
          }
        }
        if (st.contains("awaits")) {
          const std::string x = json_string(st, "awaits");
          if (x.rfind("w:", 0) != 0 || !b.world_index(std::string_view(x).substr(2))) {
            errors.add(p.file, ln, sat + ".awaits: names no world stage");
          }
        }
        si.when = b.new_slot();
        b.stage_slot[{p.id, si.id}] = si.when;
        pending.push_back(Pending{si.when, json_string(st, "when"), Use::PersonaStage, p.file, ln, sat + ".when"});
        info.stages.push_back(std::move(si));
      }
    }
    if (const auto aw = rec.find("awareness"); aw != rec.end() && aw->is_object()) {
      info.home = json_strings(*aw, "home");
      for (const std::string& h : info.home) {
        if (!k.regions_.empty() && !b.region_index(h)) errors.add(p.file, 0, "awareness.home: names no region of regions.toml");
      }
      if (const auto ties = aw->find("tie"); ties != aw->end() && ties->is_array()) {
        for (std::size_t i = 0; i < ties->size(); ++i) {
          const nlohmann::json& tj = (*ties)[i];
          const std::string tat = "awareness.tie[" + std::to_string(i) + "]";
          KnowledgePersona::TieInfo ti;
          ti.tie.item = json_string(tj, "item");
          if (const std::string c = json_string(tj, "certainty"); !c.empty()) ti.tie.certainty = certainty_of(c);
          ti.tie.when = json_string(tj, "when");
          ti.tie.from = json_string(tj, "from");
          ti.tie.line = json_string(tj, "line");
          if (const auto sp = tj.find("spoiler"); sp != tj.end() && sp->is_number_integer()) {
            ti.tie.spoiler = static_cast<std::uint8_t>(sp->get<std::int64_t>());
          }
          const std::string basis = json_string(tj, "basis");
          ti.tie.played = basis == "played";
          ti.tie.evidence = !json_strings(tj, "evidence").empty();
          if (!basis.empty() && basis != "flag" && basis != "played") errors.add(p.file, 0, tat + ".basis: must be flag or played");
          const std::string& item = ti.tie.item;
          const bool world_item = item.rfind("w:", 0) == 0;
          const bool track_item = item.rfind("q:", 0) == 0;
          if (world_item) {
            if (!b.world_index(std::string_view(item).substr(2))) errors.add(p.file, 0, tat + ".item: names no world stage");
          } else if (track_item) {
            const std::string t = item.substr(2);
            const bool track = std::any_of(k.tracks_.begin(), k.tracks_.end(), [&](const Track& x) { return x.id == t; });
            const bool persona = pack.persona(t) != nullptr;
            if (t == p.id) errors.add(p.file, 0, tat + ".item: never the persona's own track (W8)");
            if (!track && !persona) errors.add(p.file, 0, tat + ".item: names no track");
            if (!ti.tie.certainty) errors.add(p.file, 0, tat + ".certainty: a track tie needs one");
            if (!ti.tie.line.empty()) errors.add(p.file, 0, tat + ".line: only a world-stage tie has its own line");
          } else {
            errors.add(p.file, 0, tat + ".item: must be w:<world stage> or q:<track>");
          }
          if (!ti.tie.line.empty() && (!tj.contains("spoiler") || !tj.contains("lexicon"))) {
            errors.add(p.file, 0, tat + ".line: needs its spoiler and its lexicon");
          }
          if (world_item && !ti.tie.from.empty()) errors.add(p.file, 0, tat + ".from: a track tie's only");
          if (world_item && tj.contains("spoiler")) {
            if (const auto x = b.world_index(std::string_view(item).substr(2)); x && ti.tie.spoiler < k.stages_[*x].spoiler) {
              errors.add(p.file, 0, tat + ".spoiler: at least its item's");
            }
          }
          if (!ti.tie.when.empty()) {
            ti.when = b.new_slot();
            pending.push_back(Pending{*ti.when, ti.tie.when, Use::Tie, p.file, 0, tat + ".when"});
          }
          info.ties.push_back(std::move(ti));
        }
      }
    }
    k.personas_.push_back(std::move(info));
  }
  // A track tie's `from` names a step of its track; a persona's track is its stages.
  for (std::size_t pi = 0; pi < k.personas_.size(); ++pi) {
    for (std::size_t ti = 0; ti < k.personas_[pi].ties.size(); ++ti) {
      const Tie& tie = k.personas_[pi].ties[ti].tie;
      if (tie.from.empty() || tie.item.rfind("q:", 0) != 0) continue;
      if (!b.stage_slot.contains({tie.item.substr(2), tie.from})) {
        errors.add(pack.personas[pi].file, 0, "awareness.tie[" + std::to_string(ti) + "].from: names no step of its track");
      }
    }
  }
  for (const Pending& pd : pending) b.compile(pd.slot, pd.when, pd.use, pd.file, pd.line, pd.key);
  std::vector<std::tuple<std::size_t, std::string, std::size_t, std::string>> where;
  for (const Pending& pd : pending) where.emplace_back(pd.slot, pd.file, pd.line, pd.key);
  if (errors.count() == 0) b.check_cycles(where);
  errors.finish();
  if (!problems.empty()) return proto::Unexpected{std::move(problems)};
  return k;
}

FlagState Knowledge::unread() const {
  FlagState fs;
  const std::size_t n = catalogue_.flags.size();
  fs.bits.assign(n, 0);
  fs.valid.assign(n, 0);
  fs.verified.assign(n, 0);
  return fs;
}

proto::Expected<FlagState, std::string> Knowledge::fixture(std::string_view text) const {
  if (const auto unsafe = proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    return proto::Unexpected{"line " + std::to_string(unsafe->line) + ": refused before it is parsed"};
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) return proto::Unexpected{std::string("the fixture state is not valid TOML")};
  const toml::table& t = parsed.table();
  for (auto&& [key, v] : t) {
    const std::string_view kk = key.str();
    if (kk != "schema" && kk != "state" && kk != "journey" && kk != "flags" && kk != "blocks" && kk != "tracks") {
      return proto::Unexpected{"an unknown key " + std::string(kk)};
    }
  }
  if (t["schema"].value_or(std::int64_t{0}) != 1) return proto::Unexpected{std::string("schema must be 1")};
  FlagState fs = unread();
  fs.have_frame = true;
  std::fill(fs.valid.begin(), fs.valid.end(), std::uint8_t{1});
  std::fill(fs.verified.begin(), fs.verified.end(), std::uint8_t{1});
  fs.journey = static_cast<std::uint8_t>(std::clamp<std::int64_t>(t["journey"].value_or(std::int64_t{1}), 1, 8));
  if (const toml::table* flags = t["flags"].as_table()) {
    for (auto&& [alias, v] : *flags) {
      const auto i = catalogue_.find(alias.str());
      if (!i) return proto::Unexpected{"flags: an alias the catalogue lacks: " + std::string(alias.str())};
      if (!v.is_boolean()) return proto::Unexpected{"flags: a value that is not true or false"};
      fs.bits[*i] = v.value_or(false) ? 1 : 0;
    }
  }
  if (const toml::table* blocks = t["blocks"].as_table()) {
    for (auto&& [name, v] : *blocks) {
      const toml::table* bt = v.as_table();
      if (bt == nullptr) return proto::Unexpected{std::string("blocks: an entry that is not a table")};
      if (catalogue_.block(name.str()) == nullptr) continue;  // a persona of a later version
      for (const char* which : {"set", "clear"}) {
        if (const toml::array* offs = (*bt)[which].as_array()) {
          for (const toml::node& o : *offs) {
            const std::int64_t off = o.value_or(std::int64_t{-1});
            for (std::size_t i = 0; i < catalogue_.flags.size(); ++i) {
              if (catalogue_.flags[i].block == name.str() && catalogue_.flags[i].offset == off) {
                fs.bits[i] = std::string_view(which) == "set" ? 1 : 0;
              }
            }
          }
        }
      }
    }
  }
  if (const toml::table* tracks = t["tracks"].as_table()) {
    for (auto&& [name, v] : *tracks) {
      const auto tr = std::find_if(tracks_.begin(), tracks_.end(), [&](const Track& x) { return x.id == name.str(); });
      if (tr == tracks_.end()) return proto::Unexpected{"tracks: a track world/tracks.toml lacks: " + std::string(name.str())};
      const std::size_t ti = static_cast<std::size_t>(tr - tracks_.begin());
      if (const toml::array* steps = v.as_array()) {
        for (const toml::node& s : *steps) {
          const std::string sid = s.value_or(std::string());
          const auto st = std::find_if(tr->steps.begin(), tr->steps.end(), [&](const TrackStep& x) { return x.id == sid; });
          if (st == tr->steps.end()) return proto::Unexpected{"tracks: a step its track lacks: " + sid};
          // The step's flags are set: every alias its `when` names.
          for (const KnowledgeOp& op : programs_[step_when_[ti][static_cast<std::size_t>(st - tr->steps.begin())]]) {
            if (op.code == KnowledgeOp::Code::Flag) fs.bits[op.a] = 1;
          }
        }
      }
    }
  }
  return fs;
}

WatchList Knowledge::watch(Realm realm, std::uint32_t version) const {
  WatchList w;
  w.version = version;
  std::vector<std::pair<std::uint32_t, std::size_t>> by_id;
  for (std::size_t i = 0; i < catalogue_.flags.size(); ++i) {
    if (catalogue_.flags[i].id != 0 && catalogue_.verified(i, realm)) by_id.emplace_back(catalogue_.flags[i].id, i);
  }
  std::sort(by_id.begin(), by_id.end());
  for (const auto& [id, i] : by_id) {
    if (w.ids.size() >= proto::kMaxWatchFlags) break;
    if (!w.ids.empty() && w.ids.back() == id) continue;  // two aliases of one id: the first is read
    w.ids.push_back(id);
    w.flags.push_back(i);
  }
  return w;
}

FlagState Knowledge::from_frame(const WatchList& watch, const proto::FlagFrame& frame, Realm realm,
                                std::uint8_t journey) const {
  FlagState fs = unread();
  fs.journey = std::clamp<std::uint8_t>(journey, 1, 8);
  if (frame.watch_version != watch.version || watch.ids.empty()) return fs;
  fs.have_frame = true;
  for (std::size_t i = 0; i < catalogue_.flags.size(); ++i) fs.verified[i] = catalogue_.verified(i, realm) ? 1 : 0;
  for (std::size_t b = 0; b < watch.ids.size() && b < proto::kMaxWatchFlags; ++b) {
    const std::size_t f = watch.flags[b];
    fs.bits[f] = frame.bits[b] ? 1 : 0;
    fs.valid[f] = frame.valid[b] ? 1 : 0;
  }
  return fs;
}

WorldView Knowledge::derive(const FlagState& fs) const {
  WorldView v;
  v.have_frame = fs.have_frame;
  KnowledgeEval e{programs_, stage_when_, stages_, regions_, catalogue_, fs, std::vector<std::optional<Tri>>(stages_.size())};
  for (std::size_t x = 0; x < stages_.size(); ++x) v.stages.push_back(e.world_stage(x));
  for (std::size_t t = 0; t < tracks_.size(); ++t) {
    std::optional<std::size_t> active;
    for (std::size_t s = 0; s < tracks_[t].steps.size(); ++s) {
      if (e.run(step_when_[t][s]) == Tri::T) active = s;  // a step whose when is Unknown is inactive (MEM §12.3)
    }
    v.tracks.push_back(active);
  }
  for (std::size_t p = 0; p < personas_.size(); ++p) v.personas.push_back(active_stage(p, fs));
  return v;
}

std::size_t Knowledge::active_stage(std::size_t persona, const FlagState& fs) const {
  if (persona >= personas_.size()) return 0;
  const KnowledgePersona& p = personas_[persona];
  KnowledgeEval e{programs_, stage_when_, stages_, regions_, catalogue_, fs, std::vector<std::optional<Tri>>(stages_.size())};
  std::size_t active = 0;
  for (std::size_t i = 1; i < p.stages.size(); ++i) {
    const Tri v = e.run(p.stages[i].when);
    if (v == Tri::T || (v == Tri::U && !p.stages[i].shows)) active = i;  // Unknown hides, never reveals (NPC §3.2)
  }
  return active;
}

Tri Knowledge::eval(std::string_view expr, const FlagState& fs, const WorldView& view) const {
  (void)view;
  std::vector<ContentError> problems;
  Errors errors(problems);
  Knowledge copy = *this;
  ContentPack none;
  KnowledgeBuilder b{copy, none, errors, {}, {}};
  b.deps.resize(copy.programs_.size());
  for (std::size_t p = 0; p < copy.personas_.size(); ++p) {
    for (const auto& s : copy.personas_[p].stages) b.stage_slot[{copy.personas_[p].id, s.id}] = s.when;
  }
  for (std::size_t t = 0; t < copy.tracks_.size(); ++t) {
    for (std::size_t s = 0; s < copy.tracks_[t].steps.size(); ++s) b.stage_slot[{copy.tracks_[t].id, copy.tracks_[t].steps[s].id}] = copy.step_when_[t][s];
  }
  const std::size_t slot = b.new_slot();
  b.compile(slot, expr, Use::Tie, "", 0, "expr");
  if (!problems.empty()) return Tri::U;
  KnowledgeEval e{copy.programs_, copy.stage_when_, copy.stages_, copy.regions_, copy.catalogue_, fs,
                  std::vector<std::optional<Tri>>(copy.stages_.size())};
  return e.run(slot);
}

std::optional<std::size_t> Knowledge::step_of(std::string_view track, std::string_view step) const {
  for (const Track& t : tracks_) {
    if (t.id != track) continue;
    for (std::size_t i = 0; i < t.steps.size(); ++i) {
      if (t.steps[i].id == step) return i;
    }
  }
  for (const KnowledgePersona& p : personas_) {
    if (p.id != track) continue;
    for (std::size_t i = 0; i < p.stages.size(); ++i) {
      if (p.stages[i].id == step) return i;
    }
  }
  return std::nullopt;
}

std::vector<Known> Knowledge::known(std::size_t persona, const FlagState& fs, const WorldView& view,
                                    std::span<const Seen> seen) const {
  std::vector<Known> out;
  for (KnownLine& l : known_lines(persona, fs, view, seen)) out.push_back(std::move(l.known));
  return out;
}

std::vector<KnownLine> Knowledge::known_lines(std::size_t persona, const FlagState& fs, const WorldView& view,
                                              std::span<const Seen> seen) const {
  std::vector<KnownLine> out;
  if (persona >= personas_.size() || persona >= view.personas.size()) return out;
  const KnowledgePersona& p = personas_[persona];
  if (p.stages.empty()) return out;
  const KnowledgePersona::StageInfo& s = p.stages[std::min(view.personas[persona], p.stages.size() - 1)];
  KnowledgeEval e{programs_, stage_when_, stages_, regions_, catalogue_, fs, std::vector<std::optional<Tri>>(stages_.size())};
  for (std::size_t x = 0; x < stages_.size() && x < view.stages.size(); ++x) e.world[x] = view.stages[x];

  const auto region_sky = [&](const std::string& key) {
    for (const Region& r : regions_) {
      if (r.key == key) return r.sky;
    }
    return false;
  };
  // An active tie: its when holds, it passes MEM §7.4 at spoiler ≥ 2, and a played tie cites its evidence (W8).
  const auto active = [&](const KnowledgePersona::TieInfo& t, std::uint8_t item_spoiler) {
    if (t.when) {
      if (e.run(*t.when) != Tri::T) return false;
      if (std::max(t.tie.spoiler, item_spoiler) >= 2 && !e.atoms_ok(*t.when)) return false;
    }
    return !t.tie.played || t.tie.evidence;
  };

  struct Item {
    std::string id;
    std::uint8_t spoiler;
    const WorldStage* world;  // null for a track step
    std::string track;
    std::size_t step;
    std::string heard;  // a track step's line for others
  };
  std::vector<Item> items;
  for (std::size_t x = 0; x < stages_.size(); ++x) {
    if (x < view.stages.size() && view.stages[x] == Tri::T) items.push_back(Item{"w:" + stages_[x].id, stages_[x].spoiler, &stages_[x], "", 0, ""});
  }
  for (std::size_t t = 0; t < tracks_.size() && t < view.tracks.size(); ++t) {
    if (const auto a = view.tracks[t]) {
      items.push_back(Item{"q:" + tracks_[t].id + "." + tracks_[t].steps[*a].id, tracks_[t].steps[*a].spoiler, nullptr,
                           tracks_[t].id, *a, tracks_[t].steps[*a].heard});
    }
  }
  // Persona tracks that another persona ties (q:<persona>.<stage>): the active stage holds.
  for (std::size_t q = 0; q < personas_.size() && q < view.personas.size(); ++q) {
    if (q == persona) continue;
    const bool tied = std::any_of(p.ties.begin(), p.ties.end(), [&](const auto& t) { return t.tie.item == "q:" + personas_[q].id; });
    if (!tied || personas_[q].stages.empty()) continue;
    const std::size_t a = std::min(view.personas[q], personas_[q].stages.size() - 1);
    items.push_back(Item{"q:" + personas_[q].id + "." + personas_[q].stages[a].id, 0, nullptr, personas_[q].id, a,
                         personas_[q].stages[a].heard});
  }

  const std::string& H = s.hears;
  std::set<std::string> hearing(p.home.begin(), p.home.end());
  if (!s.at.empty()) hearing.insert(s.at);
  for (const Item& it : items) {
    std::vector<std::pair<Certainty, Path>> paths;
    bool own = false;
    if (std::find(s.knows.begin(), s.knows.end(), it.id) != s.knows.end()) {
      paths.emplace_back(Certainty::Fact, Path::Own);
      own = true;
    }
    for (const Seen& w : seen) {
      if (w.item == it.id) paths.emplace_back(w.certainty, Path::Kept);
    }
    bool tied = false;
    std::string tie_line;
    for (const auto& t : p.ties) {
      bool cover = t.tie.item == it.id;
      if (!cover && !it.track.empty() && t.tie.item == "q:" + it.track) {
        cover = true;
        if (!t.tie.from.empty()) {
          const auto from = step_of(it.track, t.tie.from);
          cover = from && it.step >= *from;
        }
      }
      if (!cover || !active(t, it.spoiler)) continue;
      tied = true;  // an active tie, with or without a certainty: prominence, and its own line (MEM §12.6)
      if (tie_line.empty()) tie_line = t.tie.line;
      if (!t.tie.certainty) continue;
      own = true;  // own affairs: a tie with a certainty covers it (MEM §12.5)
      if (H == "all") paths.emplace_back(*t.tie.certainty, Path::Tie);
    }
    if (it.world != nullptr) {
      const WorldStage& w = *it.world;
      if (w.reach == Reach::Everywhere) {
        if ((H == "all" || H == "sky") && region_sky(s.at)) {
          paths.emplace_back(Certainty::Fact, Path::Sky);
        } else if (H == "all" && w.spoiler <= 1) {
          paths.emplace_back(Certainty::Rumour, Path::Word);  // no sky over it: word of the road
        }
      } else if (w.reach == Reach::Region && H == "all") {
        const bool near = std::any_of(w.regions.begin(), w.regions.end(), [&](const std::string& r) { return hearing.contains(r); });
        if (near) {
          paths.emplace_back(w.near, Path::Near);
        } else if (w.spoiler <= 1) {
          paths.emplace_back(Certainty::Rumour, Path::Word);  // spoiler ≥ 2 never spreads loose
        }
      }
    }
    if (paths.empty()) continue;  // absence is silence
    const auto best = *std::min_element(paths.begin(), paths.end());
    KnownLine l;
    l.known = Known{it.id, best.first, best.second, own};
    l.tied = tied;
    l.rank = it.world != nullptr ? it.world->rank : 0;  // a track step has no rank (MEM §12.6)
    l.line = !tie_line.empty() ? tie_line : it.world != nullptr ? it.world->line : it.heard;
    l.supersedes = it.world != nullptr ? it.world->supersedes : std::vector<std::string>{};
    out.push_back(std::move(l));
    if (out.size() >= kKnownMax) break;
  }
  return out;
}

namespace {

// Block 5b as the template sets it out (prompts/dialogue/05b_standing.j2), for its estimate while lines are chosen.
std::string standing_text(const std::string& road, const Standing& tied, const Standing& land) {
  std::string t = "As things stand now:\n";
  if (!road.empty()) t += "Your own road: " + road + "\n";
  const auto part = [&](const char* label, const std::vector<std::string>& lines) {
    if (lines.empty()) return;
    t += label;
    for (std::size_t i = 0; i < lines.size(); ++i) t += (i == 0 ? "" : " ") + lines[i];
    t += "\n";
  };
  part("Tied to your road, you know: ", tied.known);
  part("Tied to your road, you have heard: ", tied.heard);
  part("Tied to your road, you believe: ", tied.believed);
  part("In the land, you know: ", land.known);
  part("In the land, you have heard: ", land.heard);
  part("In the land, you believe: ", land.believed);
  return t;
}

void put(Standing& s, Certainty c, const std::string& line) {
  (c == Certainty::Fact ? s.known : c == Certainty::Rumour ? s.heard : s.believed).push_back(line);
}

}  // namespace

Grounding Knowledge::ground(std::size_t persona, const FlagState& fs, const WorldView& view,
                            std::span<const Seen> seen) const {
  Grounding g;
  if (persona >= personas_.size() || persona >= view.personas.size() || personas_[persona].stages.empty()) return g;
  const KnowledgePersona& p = personas_[persona];
  g.stage = std::min(view.personas[persona], p.stages.size() - 1);
  std::vector<KnownLine> lines = known_lines(persona, fs, view, seen);
  for (const KnownLine& l : lines) g.known.push_back(l.known);
  // The known set, less what a known item supersedes; then own affairs, tied, rank (higher first), id (MEM §12.6).
  std::set<std::string> superseded;
  for (const KnownLine& l : lines) superseded.insert(l.supersedes.begin(), l.supersedes.end());
  std::erase_if(lines, [&](const KnownLine& l) { return superseded.contains(l.known.item); });
  std::stable_sort(lines.begin(), lines.end(), [](const KnownLine& a, const KnownLine& b) {
    if (a.known.own_affairs != b.known.own_affairs) return a.known.own_affairs;
    if (a.tied != b.tied) return a.tied;
    if (a.rank != b.rank) return a.rank > b.rank;
    return a.known.item < b.known.item;
  });
  // At most four lines while the block stays within its 200 tokens; the rest are offered to block 7 (v0.3).
  const std::string& road = p.stages[g.stage].road;
  std::size_t taken = 0;
  for (; taken < lines.size() && taken < kStandingLinesMax; ++taken) {
    Standing tied = g.tied, land = g.land;
    put(lines[taken].known.own_affairs ? tied : land, lines[taken].known.certainty, lines[taken].line);
    if (prompt::estimate_tokens(standing_text(road, tied, land)) > kStandingTokensMax) break;
    g.tied = std::move(tied);
    g.land = std::move(land);
    g.shown.push_back(lines[taken].known.item);
  }
  for (std::size_t i = taken; i < lines.size(); ++i) g.offered.push_back(lines[i].known.item);
  return g;
}

KnowledgeView Knowledge::view(const FlagState& fs) const {
  KnowledgeView v;
  v.world = derive(fs);
  for (std::size_t p = 0; p < personas_.size(); ++p) v.personas.push_back(ground(p, fs, v.world));
  return v;
}

std::string you_now(const proto::Meter& hp, bool fighting, bool self_awareness) {
  if (!self_awareness) return {};
  const char* wounds = "unhurt";
  if (hp.max > 0) {
    const double share = static_cast<double>(std::min(hp.cur, hp.max)) / static_cast<double>(hp.max);
    wounds = share >= 0.75 ? "unhurt" : share >= 0.5 ? "bloodied" : share >= 0.25 ? "badly hurt" : "barely standing";
  }
  const bool hurt = std::string_view(wounds) != "unhurt";
  if (!hurt && !fighting) return {};  // a calm prompt pays nothing (NPC §7.3)
  std::string out = wounds;
  if (fighting) out += ", in the thick of a fight";
  return out;  // the statuses' words join after S8 (DESIGN §9.9)
}

}  // namespace en::core
