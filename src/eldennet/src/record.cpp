// The recorder's lines (record.hpp).
#include "en/shell/record.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <variant>

#include <nlohmann/json.hpp>

namespace en::shell {

namespace report = gal::report;
using report::Key;
using report::Type;

namespace {

constexpr std::array<std::string_view, 7> kKinds = {"header", "snap", "life", "banner", "change", "cmd", "mark"};
constexpr std::array<std::string_view, 4> kRegions = {"ww", "jp", "other", "none"};
constexpr std::array<std::string_view, 6> kSpikes = {"none", "s0a", "s0b", "s0c", "s0d", "s0d-crash"};
constexpr std::array<std::string_view, 11> kGroups = {"session", "world", "combat", "player", "flags", "text",
                                                      "ui",      "place", "camera", "params", "status"};
constexpr std::array<std::string_view, 3> kGroupStates = {"Untested", "Ok", "Tripped"};
constexpr std::array<std::string_view, 5> kRoles = {"Unknown", "Host", "Cooperator", "Invader", "Other"};
constexpr std::array<std::string_view, 5> kLobbies = {"Unknown", "None", "Hosting", "Joined", "Other"};
constexpr std::array<std::string_view, 4> kScreens = {"Unknown", "InGame", "Loading", "Title"};
constexpr std::array<std::string_view, 5> kHuds = {"Unknown", "HideAll", "ShowAll", "PopupMenu", "Default"};
constexpr std::array<std::string_view, 3> kBodies = {"Unknown", "A", "B"};
constexpr std::array<std::string_view, 7> kStatuses = {"Frostbite", "Poison",  "ScarletRot", "Sleep",
                                                       "Hemorrhage", "Madness", "DeathBlight"};
constexpr std::array<std::string_view, 4> kParams = {"NpcParam", "BonfireWarpParam", "WorldMapPlaceNameParam",
                                                     "SpEffectParam"};
constexpr std::array<std::string_view, 3> kLife = {"data_ready", "playthrough_ready", "group_tripped"};
constexpr std::array<std::string_view, 8> kBanners = {"unknown_build",     "anticheat",        "not_started",
                                                      "discovery_failed",  "register_faulted", "inert",
                                                      "session_failed",    "task_tripped"};
constexpr std::array<std::string_view, 6> kChanges = {"phase", "gate_opened", "gate_closed", "level", "banner", "badge"};
constexpr std::array<std::string_view, 5> kCommands = {"noop", "set_watch_flags", "set_tracked", "resolve_text",
                                                       "resolve_params"};

constexpr std::array<Key, 87> kKeys = {{
    {"t", Type::Unsigned},
    {"k", Type::Word, Type::Object, kKinds},
    // header
    {"ensnap", Type::Unsigned},
    {"game_version", Type::Array, Type::Unsigned},
    {"en_version", Type::Array, Type::Unsigned},
    {"region", Type::Word, Type::Object, kRegions},
    {"spike", Type::Word, Type::Object, kSpikes},
    {"probe_build", Type::Boolean},
    {"file", Type::Unsigned},
    // snap
    {"seq", Type::Unsigned},
    {"mono_ms", Type::Unsigned},
    {"gap", Type::Boolean},
    {"build", Type::Unsigned},
    {"health", Type::Object},
    {"groups", Type::Array, Type::Object},
    {"group", Type::Word, Type::Object, kGroups},
    {"state", Type::Word, Type::Object, kGroupStates},
    {"phase_b", Type::Boolean},
    {"faults", Type::Unsigned},
    {"task_us_last", Type::Unsigned},
    {"task_us_p99", Type::Unsigned},
    {"session", Type::Object},
    {"role", Type::Word, Type::Object, kRoles},
    {"is_my_world", Type::Boolean},
    {"lobby", Type::Word, Type::Object, kLobbies},
    {"player_set_len", Type::Unsigned},
    {"anticheat", Type::Boolean},
    {"ersc_loaded", Type::Boolean},
    {"life", Type::Object},
    {"main_player", Type::Boolean},
    {"screen", Type::Word, Type::Object, kScreens},
    {"load_counter", Type::Unsigned},
    {"ui", Type::Object},
    {"hud", Type::Word, Type::Object, kHuds},
    {"banner", Type::Unsigned},
    {"popup_talk_id", Type::Unsigned},
    {"talk_cam", Type::Boolean},
    {"subtitles_on", Type::Boolean},
    {"player", Type::Object},
    {"handle", Type::Unsigned},
    {"level", Type::Unsigned},
    {"archetype", Type::Unsigned},
    {"body", Type::Word, Type::Object, kBodies},
    {"hp", Type::Object},
    {"fp", Type::Object},
    {"stamina", Type::Object},
    {"cur", Type::Unsigned},
    {"max", Type::Unsigned},
    {"runes_held", Type::Unsigned},
    {"deaths", Type::Unsigned},
    {"play_time_ms", Type::Unsigned},
    {"ng_lvl", Type::Unsigned},
    {"save_slot", Type::Unsigned},
    {"dead", Type::Boolean},
    {"riding", Type::Boolean},
    {"locked_on", Type::Unsigned},
    {"last_hit_by", Type::Unsigned},
    {"throw_state", Type::Unsigned},
    {"roundtable", Type::Boolean},
    {"statuses", Type::Array, Type::Word, kStatuses},
    {"block", Type::Unsigned},
    {"pos", Type::Array, Type::Float},
    {"gear", Type::Array, Type::Unsigned},
    {"boss_flag", Type::Boolean},
    {"place", Type::Object},
    {"place_id", Type::Unsigned},
    {"clock", Type::Array, Type::Unsigned},
    {"boss", Type::Array, Type::Object},
    {"name_id", Type::Signed},
    {"damage_taken", Type::Signed},
    {"chars", Type::Array, Type::Object},
    {"param", Type::Unsigned},
    {"model", Type::Unsigned},
    {"entity", Type::Unsigned},
    {"kind", Type::Unsigned},
    {"team", Type::Unsigned},
    {"recv_dmg_type", Type::Signed},
    {"dist_m", Type::Float},
    {"bearing_deg", Type::Float},
    {"on_screen", Type::Boolean},
    {"flags", Type::Object},
    {"watch_version", Type::Unsigned},
    {"set", Type::Array, Type::Unsigned},
    {"valid", Type::Array, Type::Unsigned},
    {"texts", Type::Array, Type::Object},
    {"params", Type::Array, Type::Object},
    // life, banner, change, cmd, mark: below
    {"n", Type::Unsigned},
}};

constexpr std::array<Key, 16> kMoreKeys = {{
    {"fmg", Type::Unsigned},
    {"id", Type::Unsigned},
    {"found", Type::Boolean},
    {"param_kind", Type::Word, Type::Object, kParams},
    {"fields", Type::Unsigned},
    {"life_kind", Type::Word, Type::Object, kLife},
    {"caps", Type::Array, Type::Word, kGroups},
    {"slot", Type::Unsigned},
    {"keepsake", Type::Unsigned},
    {"name_chars", Type::Unsigned},
    {"reason", Type::Word, Type::Object, kBanners},
    {"what", Type::Word, Type::Object, kChanges},
    {"from", Type::Unsigned},
    {"to", Type::Unsigned},
    {"cmd", Type::Word, Type::Object, kCommands},
    {"version", Type::Unsigned},
}};

// One vocabulary of both lists, built once at compile time.
constexpr std::array<Key, kKeys.size() + kMoreKeys.size()> joined() {
  std::array<Key, kKeys.size() + kMoreKeys.size()> out{};
  for (std::size_t i = 0; i < kKeys.size(); ++i) out[i] = kKeys[i];
  for (std::size_t i = 0; i < kMoreKeys.size(); ++i) out[kKeys.size() + i] = kMoreKeys[i];
  return out;
}
constexpr std::array<Key, kKeys.size() + kMoreKeys.size()> kAll = joined();
constexpr report::Vocabulary kVocabulary{"recording", 1, kAll};

template <class E, std::size_t N>
std::string_view name_of(E e, const std::array<std::string_view, N>& words) noexcept {
  const auto i = static_cast<std::size_t>(e);
  return i < N ? words[i] : words[0];
}

report::Writer start(std::uint64_t t, std::string_view kind) {
  report::Writer w(kVocabulary, report::Style::Line);
  w.number("t", t);
  w.word("k", kind);
  return w;
}

std::string finish(report::Writer& w) {
  auto out = w.finish();
  return out ? std::move(*out) : std::string();
}

void meter(report::Writer& w, std::string_view key, const proto::Meter& m) {
  w.begin_object(key);
  w.number("cur", m.cur);
  w.number("max", m.max);
  w.end_object();
}

void pos(report::Writer& w, const std::array<float, 3>& p) {
  w.begin_array("pos");
  for (const float v : p) {
    if (std::isfinite(v)) w.real(v);
  }
  w.end_array();
}

void statuses(report::Writer& w, proto::StatusSet set) {
  w.begin_array("statuses");
  for (std::size_t i = 0; i < proto::kStatusCount; ++i) {
    if ((static_cast<unsigned>(set) & (1u << i)) != 0) w.word(kStatuses[i]);
  }
  w.end_array();
}

}  // namespace

const report::Vocabulary& recording_vocabulary() noexcept { return kVocabulary; }

std::string header_line(const RecordingHeader& h, std::uint64_t t) {
  report::Writer w = start(t, "header");
  w.number("ensnap", 1);
  w.begin_array("game_version");
  for (const auto v : h.game_version) w.number(v);
  w.end_array();
  w.begin_array("en_version");
  for (const auto v : h.en_version) w.number(v);
  w.end_array();
  w.word("region", h.region);
  w.word("spike", h.spike);
  w.boolean("probe_build", h.probe_build);
  w.number("file", h.file);
  return finish(w);
}

std::string snapshot_line(const proto::WorldSnapshot& s, std::uint64_t t) {
  report::Writer w = start(t, "snap");
  w.number("seq", s.seq);
  w.number("mono_ms", s.mono_ms);
  w.boolean("gap", s.gap);
  w.number("build", s.build.index);
  w.begin_object("health");
  w.begin_array("groups");
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    const proto::GroupHealth& gh = s.health.groups[g];
    w.begin_object();
    w.word("group", kGroups[g]);
    w.word("state", name_of(gh.state, kGroupStates));
    w.boolean("phase_b", gh.phase_b);
    w.number("faults", gh.faults);
    w.end_object();
  }
  w.end_array();
  w.number("task_us_last", s.health.task_us_last);
  w.number("task_us_p99", s.health.task_us_p99);
  w.end_object();
  w.begin_object("session");
  w.word("role", name_of(s.session.role, kRoles));
  w.boolean("is_my_world", s.session.is_my_world);
  w.word("lobby", name_of(s.session.lobby, kLobbies));
  w.number("player_set_len", s.session.player_set_len);
  w.boolean("anticheat", s.session.anticheat);
  w.boolean("ersc_loaded", s.session.ersc_loaded);
  w.end_object();
  w.begin_object("life");
  w.boolean("main_player", s.life.main_player);
  w.word("screen", name_of(s.life.screen, kScreens));
  if (s.life.load_counter) w.number("load_counter", *s.life.load_counter);
  w.end_object();
  w.begin_object("ui");
  w.word("hud", name_of(s.ui.hud, kHuds));
  if (s.ui.banner) w.number("banner", *s.ui.banner);
  if (s.ui.popup_talk_id) w.number("popup_talk_id", *s.ui.popup_talk_id);
  w.boolean("talk_cam", s.ui.talk_cam);
  w.boolean("subtitles_on", s.ui.subtitles_on);
  w.end_object();
  if (s.player) {
    const proto::Player& p = *s.player;
    w.begin_object("player");
    w.number("handle", p.handle.raw);
    w.number("level", p.level);
    w.number("archetype", p.archetype);
    w.word("body", name_of(p.body, kBodies));
    meter(w, "hp", p.hp);
    meter(w, "fp", p.fp);
    meter(w, "stamina", p.stamina);
    w.number("runes_held", p.runes_held);
    w.number("deaths", p.deaths);
    w.number("play_time_ms", p.play_time_ms);
    w.number("ng_lvl", p.ng_lvl);
    w.number("save_slot", p.save_slot);
    w.boolean("dead", p.dead);
    w.boolean("riding", p.riding);
    if (p.locked_on) w.number("locked_on", p.locked_on->raw);
    if (p.last_hit_by) w.number("last_hit_by", p.last_hit_by->raw);
    w.number("throw_state", p.throw_state);
    w.boolean("roundtable", p.roundtable);
    statuses(w, p.statuses);
    w.number("block", p.block.packed);
    pos(w, p.pos);
    w.begin_array("gear");
    for (const std::uint32_t g : {p.gear.right_weapon, p.gear.left_weapon, p.gear.head, p.gear.chest, p.gear.hands,
                                  p.gear.legs}) {
      w.number(g);
    }
    w.end_array();
    w.boolean("boss_flag", p.boss_flag);
    w.end_object();
  }
  if (s.place) {
    w.begin_object("place");
    if (s.place->place_id) w.number("place_id", *s.place->place_id);
    w.end_object();
  }
  if (s.clock) {
    w.begin_array("clock");
    w.number(s.clock->h);
    w.number(s.clock->m);
    w.end_array();
  }
  w.begin_array("boss");
  for (const proto::BossBar& b : s.boss.bars) {
    w.begin_object();
    w.number("handle", b.handle.raw);
    w.signed_number("name_id", b.name_id);
    w.signed_number("damage_taken", b.damage_taken);
    w.end_object();
  }
  w.end_array();
  w.begin_array("chars");
  for (const proto::Chr& c : s.chars) {
    w.begin_object();
    w.number("handle", c.handle.raw);
    w.number("param", c.param);
    w.number("model", c.model);
    w.number("entity", c.entity);
    w.number("kind", c.kind);
    w.number("team", c.team);
    meter(w, "hp", c.hp);
    w.boolean("dead", c.dead);
    if (c.last_hit_by) w.number("last_hit_by", c.last_hit_by->raw);
    w.signed_number("recv_dmg_type", c.recv_dmg_type);
    w.number("throw_state", c.throw_state);
    if (std::isfinite(c.dist_m)) w.real("dist_m", c.dist_m);
    if (std::isfinite(c.bearing_deg)) w.real("bearing_deg", c.bearing_deg);
    w.boolean("on_screen", c.on_screen);
    statuses(w, c.statuses);
    w.number("block", c.block.packed);
    pos(w, c.pos);
    w.end_object();
  }
  w.end_array();
  if (s.flags) {
    w.begin_object("flags");
    w.number("watch_version", s.flags->watch_version);
    w.begin_array("set");
    for (std::size_t i = 0; i < proto::kMaxWatchFlags; ++i) {
      if (s.flags->bits[i]) w.number(i);
    }
    w.end_array();
    w.begin_array("valid");
    for (std::size_t i = 0; i < proto::kMaxWatchFlags; ++i) {
      if (s.flags->valid[i]) w.number(i);
    }
    w.end_array();
    w.end_object();
  }
  w.begin_array("texts");
  for (const proto::TextEntry& e : s.new_text) {
    w.begin_object();
    w.number("fmg", e.fmg);
    w.number("id", e.id);
    w.boolean("found", e.text.has_value());
    w.end_object();
  }
  w.end_array();
  w.begin_array("params");
  for (const proto::ParamRowInfo& r : s.new_params) {
    w.begin_object();
    w.word("param_kind", name_of(r.param, kParams));
    w.number("id", r.id);
    w.number("fields", r.fields.size());
    w.end_object();
  }
  w.end_array();
  return finish(w);
}

std::string lifecycle_line(const proto::Lifecycle& l, std::uint64_t t) {
  report::Writer w = start(t, "life");
  if (const auto* d = std::get_if<proto::Lifecycle::DataReady>(&l.payload)) {
    w.word("life_kind", "data_ready");
    w.number("build", d->build.index);
    w.begin_array("caps");
    for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
      if ((static_cast<unsigned>(d->caps.groups) & (1u << g)) != 0) w.word(kGroups[g]);
    }
    w.end_array();
  } else if (const auto* p = std::get_if<proto::Lifecycle::PlaythroughReady>(&l.payload)) {
    w.word("life_kind", "playthrough_ready");
    w.number("slot", p->slot);
    w.number("archetype", p->archetype);
    w.number("keepsake", p->keepsake);
    w.number("play_time_ms", p->play_time_ms);
    w.number("name_chars", p->name.size());  // its length only: the name is never recorded here
  } else if (const auto* g = std::get_if<proto::Lifecycle::GroupTripped>(&l.payload)) {
    w.word("life_kind", "group_tripped");
    w.word("group", name_of(g->group, kGroups));
  }
  return finish(w);
}

std::string banner_line(core::BannerReason reason, std::uint64_t t) {
  report::Writer w = start(t, "banner");
  w.word("reason", name_of(reason, kBanners));
  return finish(w);
}

std::string change_line(const core::Change& c, std::uint64_t t) {
  report::Writer w = start(t, "change");
  w.word("what", name_of(c.what, kChanges));
  w.number("from", c.from);
  w.number("to", c.to);
  w.number("mono_ms", c.at);
  return finish(w);
}

std::string command_line(const proto::GameCommand& c, std::uint64_t t) {
  report::Writer w = start(t, "cmd");
  w.word("cmd", kCommands[c.payload.index()]);
  if (const auto* f = std::get_if<proto::GameCommand::SetWatchFlags>(&c.payload)) {
    w.number("version", f->version);
    w.number("n", f->ids ? f->ids->size() : 0);
  } else if (const auto* s = std::get_if<proto::GameCommand::SetTracked>(&c.payload)) {
    w.number("n", s->handles.size());
  } else if (const auto* r = std::get_if<proto::GameCommand::ResolveText>(&c.payload)) {
    w.number("fmg", r->fmg);
    w.number("n", r->ids.size());
  } else if (const auto* p = std::get_if<proto::GameCommand::ResolveParams>(&c.payload)) {
    w.word("param_kind", name_of(p->param, kParams));
    w.number("n", p->ids.size());
  }
  return finish(w);
}

std::string mark_line(std::uint32_t n, std::uint64_t t) {
  report::Writer w = start(t, "mark");
  w.number("n", n);
  return finish(w);
}

// ---- Reading back ----

namespace {

using Json = nlohmann::json;

template <std::size_t N>
std::optional<std::size_t> index_of(const Json& j, std::string_view key, const std::array<std::string_view, N>& words) {
  const auto it = j.find(key);
  if (it == j.end() || !it->is_string()) return std::nullopt;
  const std::string& v = it->get_ref<const std::string&>();
  for (std::size_t i = 0; i < N; ++i) {
    if (words[i] == v) return i;
  }
  return std::nullopt;
}

std::uint64_t unsigned_of(const Json& j, std::string_view key, std::uint64_t otherwise = 0) {
  const auto it = j.find(key);
  return it != j.end() && it->is_number_unsigned() ? it->get<std::uint64_t>() : otherwise;
}

bool bool_of(const Json& j, std::string_view key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_boolean() && it->get<bool>();
}

template <class T>
T narrow(std::uint64_t v) {
  return static_cast<T>(std::min<std::uint64_t>(v, std::numeric_limits<T>::max()));
}

bool read_snap(const Json& j, RecordedLine& out) {
  auto s = std::make_unique<proto::WorldSnapshot>();
  s->seq = unsigned_of(j, "seq");
  s->mono_ms = unsigned_of(j, "mono_ms");
  s->gap = bool_of(j, "gap");
  s->build.index = narrow<std::uint16_t>(unsigned_of(j, "build"));
  const auto health = j.find("health");
  if (health == j.end() || !health->is_object()) return false;
  if (const auto groups = health->find("groups"); groups != health->end() && groups->is_array()) {
    for (const Json& g : *groups) {
      const auto at = index_of(g, "group", kGroups);
      const auto state = index_of(g, "state", kGroupStates);
      if (!at || !state) return false;
      proto::GroupHealth& gh = s->health.groups[*at];
      gh.state = static_cast<proto::GroupState>(*state);
      gh.phase_b = bool_of(g, "phase_b");
      gh.faults = narrow<std::uint8_t>(unsigned_of(g, "faults"));
    }
  }
  s->health.task_us_last = narrow<std::uint32_t>(unsigned_of(*health, "task_us_last"));
  s->health.task_us_p99 = narrow<std::uint32_t>(unsigned_of(*health, "task_us_p99"));
  if (const auto session = j.find("session"); session != j.end() && session->is_object()) {
    s->session.role = static_cast<proto::MultiplayRole>(index_of(*session, "role", kRoles).value_or(0));
    s->session.is_my_world = bool_of(*session, "is_my_world");
    s->session.lobby = static_cast<proto::LobbyState>(index_of(*session, "lobby", kLobbies).value_or(0));
    s->session.player_set_len = narrow<std::uint8_t>(unsigned_of(*session, "player_set_len"));
    s->session.anticheat = bool_of(*session, "anticheat");
    s->session.ersc_loaded = bool_of(*session, "ersc_loaded");
  }
  if (const auto life = j.find("life"); life != j.end() && life->is_object()) {
    s->life.main_player = bool_of(*life, "main_player");
    s->life.screen = static_cast<proto::Screen>(index_of(*life, "screen", kScreens).value_or(0));
    if (life->contains("load_counter")) s->life.load_counter = narrow<std::uint32_t>(unsigned_of(*life, "load_counter"));
  }
  if (const auto ui = j.find("ui"); ui != j.end() && ui->is_object()) {
    s->ui.hud = static_cast<proto::HudState>(index_of(*ui, "hud", kHuds).value_or(0));
    s->ui.talk_cam = bool_of(*ui, "talk_cam");
  }
  if (const auto p = j.find("player"); p != j.end() && p->is_object()) {
    proto::Player pl;
    pl.handle.raw = unsigned_of(*p, "handle");
    pl.level = narrow<std::uint16_t>(unsigned_of(*p, "level"));
    pl.archetype = narrow<std::uint8_t>(unsigned_of(*p, "archetype"));
    pl.play_time_ms = unsigned_of(*p, "play_time_ms");
    pl.deaths = narrow<std::uint32_t>(unsigned_of(*p, "deaths"));
    pl.ng_lvl = narrow<std::uint8_t>(unsigned_of(*p, "ng_lvl"));
    pl.save_slot = narrow<std::uint8_t>(unsigned_of(*p, "save_slot"));
    pl.dead = bool_of(*p, "dead");
    pl.riding = bool_of(*p, "riding");
    pl.boss_flag = bool_of(*p, "boss_flag");
    s->player = pl;
  }
  if (const auto chars = j.find("chars"); chars != j.end() && chars->is_array()) out.chars = chars->size();
  if (const auto flags = j.find("flags"); flags != j.end() && flags->is_object()) {
    out.flag_frame = true;
    if (const auto set = flags->find("set"); set != flags->end() && set->is_array()) out.flags = set->size();
  }
  out.snap = std::move(s);
  return true;
}

bool read_life(const Json& j, RecordedLine& out) {
  const auto kind = index_of(j, "life_kind", kLife);
  if (!kind) return false;
  if (*kind == 0) {
    proto::Lifecycle::DataReady d;
    d.build.index = narrow<std::uint16_t>(unsigned_of(j, "build"));
    unsigned groups = 0;
    if (const auto caps = j.find("caps"); caps != j.end() && caps->is_array()) {
      for (const Json& c : *caps) {
        for (std::size_t g = 0; c.is_string() && g < kGroups.size(); ++g) {
          if (kGroups[g] == c.get_ref<const std::string&>()) groups |= 1u << g;
        }
      }
    }
    d.caps.groups = static_cast<proto::GroupSet>(groups);
    out.life.payload = d;
  } else if (*kind == 1) {
    proto::Lifecycle::PlaythroughReady p;
    p.slot = narrow<std::uint8_t>(unsigned_of(j, "slot"));
    p.archetype = narrow<std::uint8_t>(unsigned_of(j, "archetype"));
    p.keepsake = narrow<std::uint8_t>(unsigned_of(j, "keepsake"));
    p.play_time_ms = unsigned_of(j, "play_time_ms");
    const std::size_t chars = std::min<std::size_t>(unsigned_of(j, "name_chars"), p.name.capacity());
    (void)p.name.try_assign(std::string(chars, 'x'));  // the stand-in: the name was never recorded
    out.life.payload = p;
  } else {
    const auto group = index_of(j, "group", kGroups);
    if (!group) return false;
    out.life.payload = proto::Lifecycle::GroupTripped{static_cast<proto::Group>(*group)};
  }
  return true;
}

}  // namespace

std::string_view kind_name(RecordedLine::Kind k) noexcept { return kKinds[static_cast<std::size_t>(k)]; }
std::string_view group_name(proto::Group g) noexcept { return kGroups[static_cast<std::size_t>(g)]; }

std::optional<RecordedLine> read_recorded_line(std::string_view line) {
  const Json j = Json::parse(line, nullptr, false);
  if (j.is_discarded() || !j.is_object()) return std::nullopt;
  const auto report_kind = j.find("report");
  if (report_kind == j.end() || *report_kind != "recording" || unsigned_of(j, "schema") != 1) return std::nullopt;
  const auto kind = index_of(j, "k", kKinds);
  if (!kind || !j.contains("t")) return std::nullopt;
  RecordedLine out;
  out.kind = static_cast<RecordedLine::Kind>(*kind);
  out.t = unsigned_of(j, "t");
  switch (out.kind) {
    case RecordedLine::Kind::Header: {
      const auto gv = j.find("game_version");
      const auto ev = j.find("en_version");
      for (std::size_t i = 0; gv != j.end() && gv->is_array() && i < gv->size() && i < 4; ++i) {
        out.header.game_version[i] = narrow<std::uint16_t>((*gv)[i].is_number_unsigned() ? (*gv)[i].get<std::uint64_t>() : 0);
      }
      for (std::size_t i = 0; ev != j.end() && ev->is_array() && i < ev->size() && i < 3; ++i) {
        out.header.en_version[i] = narrow<std::uint16_t>((*ev)[i].is_number_unsigned() ? (*ev)[i].get<std::uint64_t>() : 0);
      }
      out.header.region = kRegions[index_of(j, "region", kRegions).value_or(3)];
      out.header.spike = kSpikes[index_of(j, "spike", kSpikes).value_or(0)];
      out.header.probe_build = bool_of(j, "probe_build");
      out.header.file = unsigned_of(j, "file");
      return out;
    }
    case RecordedLine::Kind::Snap:
      if (!read_snap(j, out)) return std::nullopt;
      return out;
    case RecordedLine::Kind::Life:
      if (!read_life(j, out)) return std::nullopt;
      return out;
    case RecordedLine::Kind::Banner: {
      const auto reason = index_of(j, "reason", kBanners);
      if (!reason) return std::nullopt;
      out.banner = static_cast<core::BannerReason>(*reason);
      return out;
    }
    case RecordedLine::Kind::Change: {
      const auto what = index_of(j, "what", kChanges);
      if (!what) return std::nullopt;
      out.change.what = static_cast<core::Change::What>(*what);
      out.change.from = narrow<std::uint8_t>(unsigned_of(j, "from"));
      out.change.to = narrow<std::uint8_t>(unsigned_of(j, "to"));
      out.change.at = unsigned_of(j, "mono_ms");
      return out;
    }
    case RecordedLine::Kind::Cmd: {
      const auto cmd = index_of(j, "cmd", kCommands);
      if (!cmd) return std::nullopt;
      out.cmd = kCommands[*cmd];
      return out;
    }
    case RecordedLine::Kind::Mark:
      out.mark = narrow<std::uint32_t>(unsigned_of(j, "n"));
      return out;
  }
  return std::nullopt;
}

}  // namespace en::shell
