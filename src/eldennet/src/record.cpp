// The recorder's lines (record.hpp).
#include "en/shell/record.hpp"

#include <cmath>
#include <variant>

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

}  // namespace en::shell
