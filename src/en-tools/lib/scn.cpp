// The scenario DSL: parser, player and expectations (scn.hpp; ARCH §8.12).
#include "scn.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

#include "en/augur/golden.hpp"

namespace en::tools {

namespace {

constexpr std::uint64_t kPlayerHandle = 0x10;
constexpr std::uint64_t kFirstHandle = 0x1000;
constexpr std::size_t kMaxActors = 32;
constexpr std::uint64_t kMaxEnd = 4ull * 3600 * 1000;

bool is_name(std::string_view s) {
  if (s.empty() || s.size() > 32 || !(s[0] >= 'a' && s[0] <= 'z')) return false;
  return std::all_of(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

std::optional<std::uint64_t> to_u64(std::string_view s) {
  std::uint64_t v = 0;
  if (s.empty()) return std::nullopt;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (r.ec != std::errc() || r.ptr != s.data() + s.size()) return std::nullopt;
  return v;
}

std::optional<double> to_f(std::string_view s) {
  double v = 0;
  if (s.empty()) return std::nullopt;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (r.ec != std::errc() || r.ptr != s.data() + s.size() || !std::isfinite(v)) return std::nullopt;
  return v;
}

std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> out;
  std::size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && (line[at] == ' ' || line[at] == '\t')) ++at;
    const std::size_t from = at;
    while (at < line.size() && line[at] != ' ' && line[at] != '\t') ++at;
    if (at > from) out.push_back(line.substr(from, at - from));
  }
  return out;
}

// key=value among the words
std::optional<std::string_view> opt(const std::vector<std::string>& args, std::string_view key) {
  for (const std::string& a : args) {
    if (a.size() > key.size() && a.compare(0, key.size(), key) == 0 && a[key.size()] == '=') {
      return std::string_view(a).substr(key.size() + 1);
    }
  }
  return std::nullopt;
}

std::optional<std::array<float, 3>> vec3(std::string_view s) {
  std::array<float, 3> v{};
  for (std::size_t i = 0; i < 3; ++i) {
    const std::size_t comma = s.find(',');
    const std::string_view part = i < 2 ? s.substr(0, comma) : s;
    if ((i < 2 && comma == std::string_view::npos) || (i == 2 && s.find(',') != std::string_view::npos)) return std::nullopt;
    const auto f = to_f(part);
    if (!f) return std::nullopt;
    v[i] = static_cast<float>(*f);
    if (i < 2) s.remove_prefix(comma + 1);
  }
  return v;
}

constexpr std::array<std::string_view, 24> kVerbs = {
    "spawn", "despawn", "move",  "hit", "heal",  "kill",  "lock",    "unlock", "throw", "ride",  "place", "banner",
    "boss",  "talk",    "ng",    "session", "title", "load", "wait", "drop_snapshots", "set_flag", "say", "", ""};

}  // namespace

proto::Expected<Scenario, ScnError> parse_scn(std::string_view text) {
  Scenario sc;
  std::size_t n = 0;
  std::size_t at = 0;
  std::uint64_t last_at = 0;
  bool explicit_end = false;
  if (text.size() > 256 * 1024) return proto::Unexpected{ScnError{0, "is larger than 256 KiB"}};
  while (at <= text.size()) {
    std::size_t eol = text.find('\n', at);
    if (eol == std::string_view::npos) eol = text.size();
    std::string_view line = text.substr(at, eol - at);
    at = eol + 1;
    ++n;
    if (const auto hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    for (const char c : line) {
      if (static_cast<unsigned char>(c) >= 0x7F || (static_cast<unsigned char>(c) < 0x20 && c != '\t')) {
        return proto::Unexpected{ScnError{n, "holds a character outside printable ASCII"}};
      }
    }
    const auto w = words(line);
    if (w.empty()) {
      if (eol == text.size()) break;
      continue;
    }
    const auto fail = [&](const std::string& why) { return proto::Unexpected{ScnError{n, why}}; };
    if (w[0] == "route") {
      if (w.size() != 2 || w[1].size() > 48 || w[1].empty() ||
          !std::all_of(w[1].begin(), w[1].end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; })) {
        return fail("route takes one name of lower-case letters, digits and '-'");
      }
      sc.route = std::string(w[1]);
    } else if (w[0] == "seed" || w[0] == "step" || w[0] == "end") {
      const auto v = w.size() == 2 ? to_u64(w[1]) : std::nullopt;
      if (!v) return fail(std::string(w[0]) + " takes one whole number");
      if (w[0] == "seed") sc.seed = *v;
      if (w[0] == "step") {
        if (*v < 50 || *v > 5000) return fail("step is 50 to 5000 ms");
        sc.step = *v;
      }
      if (w[0] == "end") {
        if (*v > kMaxEnd) return fail("end is at most 4 hours");
        sc.end = *v;
        explicit_end = true;
      }
    } else if (w[0] == "at") {
      const auto t = w.size() >= 3 ? to_u64(w[1]) : std::nullopt;
      if (!t || *t > kMaxEnd) return fail("at takes a time in ms and a verb");
      if (std::find(kVerbs.begin(), kVerbs.end(), w[2]) == kVerbs.end() || w[2].empty()) return fail("an unknown verb");
      if (w[2] == "set_flag") return fail("set_flag waits for the flag catalogue and FlagWatch (v0.2)");
      if (w[2] == "say") return fail("say waits for the Pipeline source and the chat (v0.2 on)");
      Action a;
      a.at = *t;
      a.line = n;
      a.verb = std::string(w[2]);
      for (std::size_t i = 3; i < w.size(); ++i) a.args.emplace_back(w[i]);
      last_at = std::max(last_at, *t);
      sc.actions.push_back(std::move(a));
    } else if (w[0] == "expect") {
      Expect e;
      e.line = n;
      std::size_t i = 1;
      if (i >= w.size()) return fail("expect takes event, no, count or audience");
      if (w[i] == "event") {
        e.what = Expect::What::Event;
        ++i;
      } else if (w[i] == "no") {
        e.what = Expect::What::No;
        ++i;
        if (i < w.size() && w[i] == "event") ++i;
      } else if (w[i] == "count") {
        e.what = Expect::What::Count;
        const auto c = i + 1 < w.size() ? to_u64(w[i + 1]) : std::nullopt;
        if (!c) return fail("expect count takes a number");
        e.count = static_cast<std::size_t>(*c);
        i += 2;
      } else if (w[i] == "audience") {
        e.what = Expect::What::Audience;
        ++i;
      } else if (w[i] == "bark") {
        return fail("bark rules are v0.4's: there is no bark to expect yet");
      } else {
        return fail("expect takes event, no, count or audience");
      }
      if (i >= w.size() || !augur::kind_index(w[i])) return fail("an event kind of the catalogue is missing");
      e.kind = std::string(w[i++]);
      for (; i < w.size(); ++i) {
        if (w[i] == "within") {
          const std::string_view r = i + 1 < w.size() ? w[i + 1] : std::string_view{};
          const std::size_t dots = r.find("..");
          const auto a = dots != std::string_view::npos ? to_u64(r.substr(0, dots)) : std::nullopt;
          const auto b = dots != std::string_view::npos ? to_u64(r.substr(dots + 2)) : std::nullopt;
          if (!a || !b || *a > *b) return fail("within takes <from>..<to> in ms");
          e.within = std::pair{*a, *b};
          ++i;
        } else if (w[i] == "=" && e.what == Expect::What::Audience) {
          std::string list;
          for (std::size_t k = i + 1; k < w.size(); ++k) list += std::string(w[k]);
          if (list.size() < 2 || list.front() != '[' || list.back() != ']') return fail("an audience is a list: [] or [1, 2]");
          std::string_view inner = std::string_view(list).substr(1, list.size() - 2);
          while (!inner.empty()) {
            const std::size_t comma = inner.find(',');
            const auto id = to_u64(inner.substr(0, comma));
            if (!id) return fail("an audience lists persona ids");
            e.audience.push_back(static_cast<std::uint32_t>(*id));
            inner = comma == std::string_view::npos ? std::string_view{} : inner.substr(comma + 1);
          }
          break;
        } else {
          const std::size_t eq = w[i].find('=');
          if (eq == std::string_view::npos || eq == 0) return fail("a field is written field=value");
          e.fields.emplace_back(std::string(w[i].substr(0, eq)), std::string(w[i].substr(eq + 1)));
        }
      }
      sc.expects.push_back(std::move(e));
    } else {
      return fail("a line starts with route, seed, step, end, at or expect");
    }
    if (eol == text.size()) break;
  }
  if (sc.route.empty()) return proto::Unexpected{ScnError{0, "has no route line"}};
  if (!explicit_end) sc.end = last_at + 3000;
  std::stable_sort(sc.actions.begin(), sc.actions.end(), [](const Action& a, const Action& b) { return a.at < b.at; });
  return sc;
}

namespace {

struct Motion {
  bool on = false;
  std::uint64_t t0 = 0, t1 = 0;
  std::array<float, 3> from{}, to{};
};

struct Actor {
  std::string name;
  std::uint64_t handle = 0;
  bool present = false;
  std::uint8_t team = 0;
  std::uint32_t param = 0;
  proto::Meter hp{1000, 1000};
  bool dead = false;
  std::optional<proto::ChrHandle> last_hit_by;
  std::uint8_t throw_state = 0;
  std::array<float, 3> pos{};
  bool on_screen = true;
  bool boss = false;
  Motion motion;
};

struct World {
  Actor player;
  std::vector<Actor> actors;
  std::uint32_t deaths = 0;
  bool riding = false;
  std::optional<proto::ChrHandle> locked_on;
  std::uint8_t ng = 0;
  std::optional<std::uint32_t> place;
  std::optional<std::uint16_t> banner;
  bool talk = false;
  bool coop = false;
  bool title = false;
  std::uint64_t loading_until = 0;
  bool revive_after_load = false;
  std::uint32_t counter = 7;
  std::uint64_t drop = 0;
};

// splitmix64, for the random drops.
struct Rng {
  std::uint64_t s;
  std::uint64_t next() {
    std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
};

float dist(const std::array<float, 3>& a, const std::array<float, 3>& b) {
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::optional<std::string> apply(World& w, const Action& a, std::uint64_t t) {
  const auto who = [&](std::string_view name) -> Actor* {
    if (name == "player") return &w.player;
    for (Actor& x : w.actors) {
      if (x.name == name) return &x;
    }
    return nullptr;
  };
  const auto arg = [&](std::size_t i) -> std::string_view { return i < a.args.size() ? std::string_view(a.args[i]) : std::string_view{}; };
  const auto need = [&](std::string_view name) -> std::optional<std::string> {
    if (who(name) == nullptr) return "names no one spawned";
    return std::nullopt;
  };
  if (a.verb == "spawn") {
    const std::string_view name = arg(0);
    if (!is_name(name)) return "spawn takes a name of lower-case letters, digits and '_'";
    Actor* x = who(name);
    if (x == nullptr) {
      if (w.actors.size() >= kMaxActors) return "more than 32 characters";
      Actor fresh;
      fresh.name = std::string(name);
      fresh.handle = kFirstHandle + 0x10 * w.actors.size();
      w.actors.push_back(fresh);
      x = &w.actors.back();
    }
    x->present = true;
    if (const auto v = opt(a.args, "team")) {
      const auto n = to_u64(*v);
      if (!n || *n > 255) return "team is 0 to 255";
      x->team = static_cast<std::uint8_t>(*n);
    }
    if (const auto v = opt(a.args, "pos")) {
      const auto p = vec3(*v);
      if (!p) return "pos is x,y,z in metres";
      x->pos = *p;
    }
    if (const auto v = opt(a.args, "hp")) {
      const std::size_t slash = v->find('/');
      const auto cur = to_u64(v->substr(0, slash));
      const auto max = slash != std::string_view::npos ? to_u64(v->substr(slash + 1)) : std::nullopt;
      if (!cur || !max || *cur > *max || *max > 1000000) return "hp is cur/max";
      x->hp = {static_cast<std::uint32_t>(*cur), static_cast<std::uint32_t>(*max)};
    }
    if (const auto v = opt(a.args, "param")) {
      const auto n = to_u64(*v);
      if (!n || *n > 0xFFFFFFFFull) return "param is a whole number";
      x->param = static_cast<std::uint32_t>(*n);
    }
    x->on_screen = std::find(a.args.begin(), a.args.end(), "offscreen") == a.args.end();
    return std::nullopt;
  }
  if (a.verb == "despawn") {
    if (auto e = need(arg(0))) return e;
    if (arg(0) == "player") return "the Tarnished cannot despawn";
    who(arg(0))->present = false;
    return std::nullopt;
  }
  if (a.verb == "move") {
    if (auto e = need(arg(0))) return e;
    Actor* x = who(arg(0));
    const auto to = opt(a.args, "to");
    const auto p = to ? vec3(*to) : std::nullopt;
    if (!p) return "move takes to=x,y,z";
    const auto over = opt(a.args, "over");
    const auto ms = over ? to_u64(*over) : std::optional<std::uint64_t>{0};
    if (!ms) return "over is a time in ms";
    if (*ms == 0) {
      x->pos = *p;
      x->motion.on = false;
    } else {
      x->motion = Motion{true, t, t + *ms, x->pos, *p};
    }
    return std::nullopt;
  }
  if (a.verb == "hit") {
    if (a.args.size() < 3 || a.args[1] != "->") return "hit takes <a> -> <b> dmg=<n>";
    if (auto e = need(arg(0))) return e;
    if (auto e = need(arg(2))) return e;
    const auto dmg = opt(a.args, "dmg");
    const auto n = dmg ? to_u64(*dmg) : std::nullopt;
    if (!n) return "hit takes dmg=<n>";
    Actor* b = who(arg(2));
    b->hp.cur = b->hp.cur > *n ? b->hp.cur - static_cast<std::uint32_t>(*n) : 0;
    b->last_hit_by = proto::ChrHandle{who(arg(0))->handle};
    return std::nullopt;
  }
  if (a.verb == "heal") {
    if (auto e = need(arg(0))) return e;
    const auto v = opt(a.args, "amount");
    const auto n = v ? to_u64(*v) : std::nullopt;
    if (!n) return "heal takes amount=<n>";
    Actor* x = who(arg(0));
    x->hp.cur = static_cast<std::uint32_t>(std::min<std::uint64_t>(x->hp.max, x->hp.cur + *n));
    return std::nullopt;
  }
  if (a.verb == "kill") {
    if (auto e = need(arg(0))) return e;
    Actor* x = who(arg(0));
    if (const auto by = opt(a.args, "by")) {
      if (who(*by) == nullptr) return "by names no one spawned";
      x->last_hit_by = proto::ChrHandle{who(*by)->handle};
    }
    x->hp.cur = 0;
    x->dead = true;
    if (x == &w.player) ++w.deaths;
    return std::nullopt;
  }
  if (a.verb == "lock") {
    if (auto e = need(arg(0))) return e;
    w.locked_on = proto::ChrHandle{who(arg(0))->handle};
    return std::nullopt;
  }
  if (a.verb == "unlock") {
    w.locked_on.reset();
    return std::nullopt;
  }
  if (a.verb == "throw") {
    if (auto e = need(arg(0))) return e;
    const auto n = to_u64(arg(1));
    if (!n || *n > 255) return "throw takes a state, 0 to 255";
    who(arg(0))->throw_state = static_cast<std::uint8_t>(*n);
    return std::nullopt;
  }
  if (a.verb == "ride" || a.verb == "talk") {
    if (arg(0) != "on" && arg(0) != "off") return a.verb + " takes on or off";
    (a.verb == "ride" ? w.riding : w.talk) = arg(0) == "on";
    return std::nullopt;
  }
  if (a.verb == "place" || a.verb == "banner") {
    if (arg(0) == "none") {
      if (a.verb == "place") w.place.reset();
      else w.banner.reset();
      return std::nullopt;
    }
    const auto n = to_u64(arg(0));
    if (!n || *n == 0 || *n > (a.verb == "banner" ? 0xFFFFull : 0xFFFFFFFFull)) return a.verb + " takes an id or none";
    if (a.verb == "place") w.place = static_cast<std::uint32_t>(*n);
    else w.banner = static_cast<std::uint16_t>(*n);
    return std::nullopt;
  }
  if (a.verb == "boss") {
    if (auto e = need(arg(0))) return e;
    if (arg(1) != "on" && arg(1) != "off") return "boss takes <name> on|off";
    who(arg(0))->boss = arg(1) == "on";
    return std::nullopt;
  }
  if (a.verb == "ng") {
    const auto n = to_u64(arg(0));
    if (!n || *n > 7) return "ng is 0 to 7";
    w.ng = static_cast<std::uint8_t>(*n);
    return std::nullopt;
  }
  if (a.verb == "session") {
    if (arg(0) != "solo" && arg(0) != "coop") return "session takes solo or coop";
    w.coop = arg(0) == "coop";
    return std::nullopt;
  }
  if (a.verb == "title") {
    w.title = true;
    return std::nullopt;
  }
  if (a.verb == "load") {
    const auto over = opt(a.args, "over");
    const auto ms = over ? to_u64(*over) : std::optional<std::uint64_t>{2000};
    if (!ms || *ms < 1) return "load takes over=<ms>";
    w.loading_until = t + *ms;
    w.revive_after_load = std::find(a.args.begin(), a.args.end(), "revive") != a.args.end();
    ++w.counter;
    return std::nullopt;
  }
  if (a.verb == "wait") return std::nullopt;
  if (a.verb == "drop_snapshots") {
    const auto n = to_u64(arg(0));
    if (!n || *n > 10000) return "drop_snapshots takes a count";
    w.drop += *n;
    return std::nullopt;
  }
  return "an unknown verb";
}

proto::ReadHealth healthy() {
  proto::ReadHealth h;
  for (auto& g : h.groups) {
    g.state = proto::GroupState::Ok;
    g.phase_b = true;
  }
  return h;
}

}  // namespace

proto::Expected<Simulated, ScnError> simulate(const Scenario& sc, unsigned drop_percent, std::uint64_t drop_seed) {
  Simulated out;
  World w;
  w.player.name = "player";
  w.player.handle = kPlayerHandle;
  w.player.present = true;
  out.names[kPlayerHandle] = "player";
  Rng rng{drop_seed ^ (sc.seed * 0x2545F4914F6CDD1Dull)};
  // The lifecycle messages a session starts with: DataReady, then the playthrough (an invented stand-in for a name).
  proto::Capabilities caps;
  caps.groups = proto::FlagSetTraits<proto::GroupSet>::all;
  out.steps.push_back(Step{0, core::CoreInput{proto::Lifecycle{proto::Lifecycle::DataReady{proto::BuildKey{0}, caps}}}});
  proto::Lifecycle::PlaythroughReady pr;
  pr.slot = 0;
  (void)pr.name.try_assign("x");
  pr.archetype = 1;
  pr.play_time_ms = 600000;
  out.steps.push_back(Step{0, core::CoreInput{proto::Lifecycle{pr}}});
  std::size_t next = 0;
  std::uint64_t seq = 0;
  for (std::uint64_t t = 0; t <= sc.end; t += sc.step) {
    for (; next < sc.actions.size() && sc.actions[next].at <= t; ++next) {
      if (const auto e = apply(w, sc.actions[next], t)) return proto::Unexpected{ScnError{sc.actions[next].line, *e}};
    }
    for (const Actor& a : w.actors) out.names[a.handle] = a.name;
    for (Actor* a : [&] {
           std::vector<Actor*> all{&w.player};
           for (Actor& x : w.actors) all.push_back(&x);
           return all;
         }()) {
      if (!a->motion.on) continue;
      const float f = t >= a->motion.t1 ? 1.0f
                                        : static_cast<float>(t - a->motion.t0) / static_cast<float>(a->motion.t1 - a->motion.t0);
      for (std::size_t k = 0; k < 3; ++k) a->pos[k] = a->motion.from[k] + (a->motion.to[k] - a->motion.from[k]) * f;
      if (t >= a->motion.t1) a->motion.on = false;
    }
    const bool loading = t < w.loading_until;
    if (!loading && w.revive_after_load && w.loading_until != 0) {
      w.player.dead = false;
      w.player.hp.cur = w.player.hp.max;
      w.player.last_hit_by.reset();
      w.revive_after_load = false;
    }
    ++seq;
    if (w.drop > 0) {
      --w.drop;
      continue;
    }
    if (drop_percent > 0 && t > 0 && rng.next() % 100 < drop_percent) continue;
    auto s = std::make_unique<proto::WorldSnapshot>();
    s->seq = seq;
    s->mono_ms = t;
    s->health = healthy();
    s->session.role = proto::MultiplayRole::Host;
    s->session.is_my_world = true;
    s->session.lobby = proto::LobbyState::None;
    s->session.player_set_len = w.coop ? 2 : 1;
    s->ui.hud = proto::HudState::Default;
    if (w.title) {
      s->life.screen = proto::Screen::Title;
      s->health.groups[static_cast<std::size_t>(proto::Group::Player)].phase_b = false;
      out.steps.push_back(Step{t, core::CoreInput{std::move(s)}});
      continue;
    }
    s->life.load_counter = w.counter;
    if (loading) {
      s->life.screen = proto::Screen::Loading;
      out.steps.push_back(Step{t, core::CoreInput{std::move(s)}});
      continue;
    }
    s->life.main_player = true;
    s->life.screen = proto::Screen::InGame;
    s->ui.banner = w.banner;
    if (w.talk) s->ui.popup_talk_id = 1;
    proto::Player p;
    p.handle.raw = kPlayerHandle;
    p.level = 40;
    p.archetype = 1;
    p.save_slot = 0;
    p.hp = w.player.hp;
    p.fp = {100, 100};
    p.stamina = {100, 100};
    p.deaths = w.deaths;
    p.play_time_ms = 600000 + t;
    p.ng_lvl = w.ng;
    p.dead = w.player.dead;
    p.riding = w.riding;
    p.locked_on = w.locked_on;
    p.last_hit_by = w.player.last_hit_by;
    p.throw_state = w.player.throw_state;
    p.block = proto::BlockId::of(60, 40, 36, 0);
    p.pos = w.player.pos;
    s->player = p;
    proto::Place place;
    place.place_id = w.place;
    place.overworld = true;
    s->place = place;
    std::vector<const Actor*> present;
    for (const Actor& a : w.actors) {
      if (a.present) present.push_back(&a);
    }
    std::stable_sort(present.begin(), present.end(),
                     [&](const Actor* a, const Actor* b) { return dist(a->pos, w.player.pos) < dist(b->pos, w.player.pos); });
    for (const Actor* a : present) {
      proto::Chr c;
      c.handle.raw = a->handle;
      c.param = a->param;
      c.team = a->team;
      c.hp = a->hp;
      c.dead = a->dead;
      c.last_hit_by = a->last_hit_by;
      c.throw_state = a->throw_state;
      c.dist_m = dist(a->pos, w.player.pos);
      c.bearing_deg = 0;
      c.on_screen = a->on_screen;
      c.block = proto::BlockId::of(60, 40, 36, 0);
      c.pos = a->pos;
      (void)s->chars.try_push_back(c);
      if (a->boss) {
        (void)s->boss.bars.try_push_back(proto::BossBar{c.handle, static_cast<std::int32_t>(a->param), 0});
      }
    }
    s->boss.active = !s->boss.bars.empty();
    out.steps.push_back(Step{t, core::CoreInput{std::move(s)}});
  }
  return out;
}

std::vector<std::string> check_expects(const Scenario& sc, const std::vector<augur::Event>& events,
                                       const std::map<std::uint64_t, std::string>& names) {
  const augur::HandleNamer namer = [&](augur::ChrHandle h) {
    const auto it = names.find(h.raw);
    return it != names.end() ? it->second : std::string("unnamed");
  };
  std::vector<nlohmann::ordered_json> js;
  js.reserve(events.size());
  for (const augur::Event& e : events) js.push_back(augur::event_json(e, namer));
  const auto value_matches = [](const nlohmann::ordered_json& v, const std::string& want) {
    if (v.is_null()) return want == "null" || want == "none";
    if (v.is_boolean()) return want == (v.get<bool>() ? "true" : "false");
    if (v.is_number()) {
      const auto f = to_f(want);
      return f && std::fabs(*f - v.get<double>()) < 1e-6;
    }
    if (v.is_string()) {
      const std::string& s = v.get_ref<const std::string&>();
      const std::size_t hash = s.find('#');
      return s == want || (hash != std::string::npos && s.substr(hash + 1) == want);
    }
    if (v.is_array()) {
      std::string list = "[";
      for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) list += ",";
        list += v[i].is_string() ? v[i].get<std::string>() : v[i].dump();
      }
      return list + "]" == want;
    }
    return false;
  };
  std::vector<std::string> failures;
  for (const Expect& x : sc.expects) {
    std::vector<std::size_t> hits;
    for (std::size_t i = 0; i < js.size(); ++i) {
      const auto& j = js[i];
      if (j["kind"] != x.kind) continue;
      if (x.within && (events[i].t_mono_ms < x.within->first || events[i].t_mono_ms > x.within->second)) continue;
      bool ok = true;
      for (const auto& [k, v] : x.fields) {
        const nlohmann::ordered_json* field = nullptr;
        if (k != "payload" && j.contains(k)) field = &j[k];
        else if (j["payload"].contains(k)) field = &j["payload"][k];
        if (field == nullptr || !value_matches(*field, v)) ok = false;
      }
      if (ok) hits.push_back(i);
    }
    const std::string where = "scn: line " + std::to_string(x.line) + ": ";
    switch (x.what) {
      case Expect::What::Event:
        if (hits.empty()) failures.push_back(where + "no " + x.kind + " matched");
        break;
      case Expect::What::No:
        if (!hits.empty()) {
          failures.push_back(where + std::to_string(hits.size()) + " " + x.kind + " matched, none expected (the first at " +
                             std::to_string(events[hits[0]].t_mono_ms) + " ms)");
        }
        break;
      case Expect::What::Count:
        if (hits.size() != x.count) {
          failures.push_back(where + std::to_string(hits.size()) + " " + x.kind + " matched, " + std::to_string(x.count) +
                             " expected");
        }
        break;
      case Expect::What::Audience: {
        if (hits.empty()) {
          failures.push_back(where + "no " + x.kind + " matched");
          break;
        }
        std::vector<std::uint32_t> got;
        for (const auto& a : events[hits[0]].audience) got.push_back(a.persona.index);
        if (got != x.audience) failures.push_back(where + "the audience differs");
        break;
      }
    }
  }
  return failures;
}

}  // namespace en::tools
