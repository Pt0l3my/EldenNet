// The content pack: the persona index and the persona records, read and checked (content.hpp; NPC §2.1, §3.1, §3.2;
// DESIGN §11.4). Each file is checked as en-proto's unsafe_for_toml has it before toml++ reads it, then held to a
// schema of our own, written from NPC §3.1's table: types, the closed values, counts and caps in words and characters.
#include "en/core/content.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>

#include <toml.hpp>

#include "en/core/prompt.hpp"  // kRecordFields
#include "schema.hpp"

namespace en::core {

namespace {

using namespace schema;

constexpr std::array<std::string_view, 5> kRegisters = {"archaic-regal", "formal", "colloquial", "broken", "chant"};
constexpr std::array<std::string_view, 4> kPronouns = {"he", "she", "they", "it"};
constexpr std::array<std::string_view, 3> kHears = {"all", "sky", "none"};
constexpr std::array<std::string_view, 3> kChat = {"allowed", "refuse", "mute"};
constexpr std::array<std::string_view, 4> kState = {"awake", "asleep", "senseless", "unknown"};
constexpr std::array<std::string_view, 3> kApproach = {"none", "native", "unknown"};
constexpr std::array<std::string_view, 3> kNews = {"close", "plain", "eager"};
constexpr std::array<std::string_view, 3> kCertainty = {"fact", "rumour", "belief"};
constexpr std::array<std::string_view, 2> kDrafted = {"hand", "assisted"};

// NPC §3.1's table, field by field.
constexpr std::array<Field, 3> kPublic = {text("summary", 60), text("appearance", 40), text("role", 20)};
constexpr std::array<Field, 5> kPrivate = {
    text("personality", 80),
    text("honesty", 26),  // truthful | guarded | liar, then at most 25 words (checked below)
    text("claims", 80),
    list("seeks", 0, 3, 15),
    list("ties", 0, 4, 20),
};
constexpr std::array<Field, 2> kTic = {required(text("text", 12)), integer("max_per_conversation", 1, 2)};
constexpr Spec kTicSpec{kTic};
constexpr std::array<Field, 5> kVoice = {
    text("cadence", 60),
    text("diction", 40),
    text("deflection", 30),
    words1(list("openers", 0, 4)),
    tables("tics", 0, 3, &kTicSpec),
};
constexpr std::array<Field, 2> kKnowledge = {list("always", 0, 8, 0, 64), list("topics", 0, 16, 0, 64)};
constexpr std::array<Field, 9> kTie = {
    required(text("item", 0, 64)),
    choice("certainty", kCertainty),
    text("when", 0, 400),
    text("from", 0, 64),
    text("line", 25, 160),
    integer("spoiler", 0, 3),
    list("lexicon", 0, 0, 0, 64),
    text("basis", 0, 32),
    list("evidence", 0, 0, 0, 64),
};
constexpr Spec kTieSpec{kTie};
constexpr std::array<Field, 5> kAwareness = {
    regions("home", 3),
    tables("tie", 0, 6, &kTieSpec),
    list("skip", 0, 12, 0, 120),
    list("sleep_leads", 0, 20, 0, 8),
    choice("news", kNews),
};
constexpr std::array<Field, 2> kCompanion = {text("skills", 60), text("beside", 40)};
constexpr std::array<Field, 2> kSeed = {list("greet", 3, 3, 20), list("beckon", 3, 3, 20)};
constexpr std::array<Field, 2> kUnlock = {text("text", 80), list("kb", 0, 6, 0, 64)};
constexpr Spec kUnlockSpec{kUnlock};
constexpr std::array<Field, 23> kStage = {
    required(text("id", 0, 32)),
    required(text("when", 0, 200)),
    text("identity", 40),
    text("where", 20),
    text("mood", 8),
    text("gist", 60),
    text("road", 30),
    region("at"),
    choice("hears", kHears),
    list("knows", 0, 4, 0, 64),
    text("awaits", 0, 64),
    text("heard", 25),
    text("short", 8),
    table("unlock", &kUnlockSpec),
    list("forbid", 0, 6, 12),
    choice("chat", kChat),
    text("beckon", 20),
    text("beckon_src", 0, 64),
    text("talked", 0, 200),
    choice("state", kState),
    choice("approach", kApproach),
    number("approach_m"),
    text("approach_played", 0, 64),
};
constexpr Spec kStageSpec{kStage};
constexpr Spec kPublicSpec{kPublic}, kPrivateSpec{kPrivate}, kVoiceSpec{kVoice}, kKnowledgeSpec{kKnowledge},
    kAwarenessSpec{kAwareness}, kCompanionSpec{kCompanion}, kSeedSpec{kSeed};
constexpr std::array<Field, 21> kPersona = {
    required(text("id", 0, 32)),
    required(integer("schema", 1, 1)),
    choice("drafted", kDrafted),  // provenance, code only (PL12; NPC §4.5): how it was drafted
    text("review", 0, 64),        // and its review record: a review's id, or "pending" until the owner's review
    required(text("display_name_fallback", 0, 32)),
    required(choice("register", kRegisters)),
    integer("reply_words_max", 10, 60),
    list("address_forms", 0, 3, 3),
    required(choice("pronouns", kPronouns)),
    required(list("deflections", 3, 5, 20)),
    list("subjects", 0, 8, 10, 50),
    list("stirs", 0, 6, 12),
    text("instructions", 35, 210),
    table("public", &kPublicSpec),
    table("private", &kPrivateSpec),
    table("voice", &kVoiceSpec),
    table("knowledge", &kKnowledgeSpec),
    table("awareness", &kAwarenessSpec),
    table("companion", &kCompanionSpec),
    table("seed", &kSeedSpec),
    required(tables("stages", 1, 12, &kStageSpec)),
};
constexpr Spec kPersonaSpec{kPersona};

// ---- identity/personas.toml ----

constexpr std::array<Field, 5> kNpcParam = {
    required(integer("param", 1, 0xFFFFFFFF)),
    required(text("form", 0, 32)),
    integer("expect_name_id", 0, 0xFFFFFFFF),
    boolean("verified"),
    text("reveal_stage", 0, 32),
};
constexpr Spec kNpcParamSpec{kNpcParam};
constexpr std::array<Field, 3> kEntity = {required(integer("id", 1, 0xFFFFFFFF)), boolean("verified"), text("note", 0, 200)};
constexpr Spec kEntitySpec{kEntity};
constexpr std::array<Field, 3> kFlagBlock = {required(text("alias", 0, 32)), required(integer("base", 0, 0xFFFFFFFF)),
                                             boolean("verified")};
constexpr Spec kFlagBlockSpec{kFlagBlock};
constexpr std::array<Field, 8> kRow = {
    required(text("id", 0, 32)),
    required(text("file", 0, 96)),
    integer("talk_code", 0, 0xFFFF),
    table("flag_block", &kFlagBlockSpec),
    tables("npc_params", 0, 16, &kNpcParamSpec),
    tables("entity_ids", 0, 16, &kEntitySpec),
    boolean("enabled"),
    boolean("test_only"),
};
constexpr Spec kRowSpec{kRow};
constexpr std::array<Field, 3> kAlias = {required(text("old", 0, 32)), required(text("new", 0, 32)),
                                         required(text("since", 0, 16))};
constexpr Spec kAliasSpec{kAlias};
constexpr std::array<Field, 3> kIndex = {required(integer("schema", 1, 1)), tables("persona", 0, 0, &kRowSpec),
                                         tables("alias", 0, 0, &kAliasSpec)};
constexpr Spec kIndexSpec{kIndex};

// ---- overrides/disable.toml (ARCH §11.6; DESIGN §11.4) ----

constexpr std::array<Field, 6> kDisable = {
    list("personas", 0, 0, 0, 64), list("cards", 0, 0, 0, 64),    list("characters", 0, 0, 0, 64),
    list("kb", 0, 0, 0, 64),       list("triggers", 0, 0, 0, 64), list("rules", 0, 0, 0, 64),
};
constexpr Spec kDisableSpec{kDisable};

// Turns off the personas the file names by id (their rows read as not enabled, so no record is read for them and no
// index, directory or knowledge holds them) and lists them in `pack.disabled_personas`. An id that names nothing is a
// warning, never an error (S11.3.g): every card's, since v0.2 ships none; a list v0.2 does not act on yet (characters,
// kb, triggers, rules) draws one warning. The file's schema is checked as any content's: an unknown key or a wrong
// type is an error. A warning names the key and the item's place, never the id.
void read_disable(const ContentFile& file, ContentPack& pack, Errors& errors) {
  const std::size_t before = errors.count();
  auto table = parse(file, kContentFileMaxBytes, errors);
  if (!table) return;
  Checker(file.path, errors).check(*table, kDisableSpec, "");
  if (errors.count() != before) return;
  const auto warn = [&](std::size_t line, std::string problem) {
    if (pack.warnings.size() < 64) pack.warnings.push_back(ContentError{file.path, line, std::move(problem)});
  };
  if (const auto* list = (*table)["personas"].as_array()) {
    for (std::size_t i = 0; i < list->size(); ++i) {
      const toml::node& n = *list->get(i);
      const std::string id = n.value_or(std::string());
      const std::string at = "personas[" + std::to_string(i) + "]";
      const auto row = std::find_if(pack.rows.begin(), pack.rows.end(), [&](const PersonaRow& r) { return r.id == id; });
      if (row == pack.rows.end()) {
        warn(line_of(n), at + ": names no persona of the index, so nothing is turned off");
        continue;
      }
      row->enabled = false;
      if (std::find(pack.disabled_personas.begin(), pack.disabled_personas.end(), id) == pack.disabled_personas.end()) {
        pack.disabled_personas.push_back(id);
      }
    }
  }
  if (const auto* list = (*table)["cards"].as_array()) {
    for (std::size_t i = 0; i < list->size(); ++i) {
      warn(line_of(*list->get(i)), "cards[" + std::to_string(i) + "]: names no shipped card, so nothing is turned off");
    }
  }
  for (const char* key : {"characters", "kb", "triggers", "rules"}) {
    if (const auto* list = (*table)[key].as_array(); list != nullptr && !list->empty()) {
      warn(line_of(*list), std::string(key) + ": not read in this version, so nothing is turned off");
    }
  }
}

bool is_persona_path(std::string_view p) {
  constexpr std::string_view prefix = "personas/";
  constexpr std::string_view suffix = ".toml";
  if (p.size() <= prefix.size() + suffix.size() || p.substr(0, prefix.size()) != prefix ||
      p.substr(p.size() - suffix.size()) != suffix) {
    return false;
  }
  return is_slug(p.substr(prefix.size(), p.size() - prefix.size() - suffix.size()), 32);
}

std::vector<PersonaRow> read_index(const ContentFile& file, Errors& errors) {
  std::vector<PersonaRow> rows;
  const std::size_t before = errors.count();
  auto table = parse(file, kContentFileMaxBytes, errors);
  if (!table) return rows;
  Checker(file.path, errors).check(*table, kIndexSpec, "");
  if (errors.count() != before) return rows;
  std::set<std::string> ids;
  std::map<std::uint32_t, std::string> params;
  std::map<std::uint32_t, std::string> entities;
  if (const auto* list = (*table)["persona"].as_array()) {
    for (const toml::node& n : *list) {
      const toml::table& t = *n.as_table();
      PersonaRow r;
      r.id = t["id"].value_or(std::string());
      r.file = t["file"].value_or(std::string());
      r.line = line_of(n);
      if (const auto tc = t["talk_code"].value<std::int64_t>()) r.talk_code = static_cast<std::uint16_t>(*tc);
      if (const auto* fb = t["flag_block"].as_table()) {
        r.flag_block = FlagBlockRef{(*fb)["alias"].value_or(std::string()),
                                    static_cast<std::uint32_t>((*fb)["base"].value_or(std::int64_t{0})),
                                    (*fb)["verified"].value_or(false)};
      }
      r.enabled = t["enabled"].value_or(true);
      r.test_only = t["test_only"].value_or(false);
      if (!is_slug(r.id, 32)) errors.add(file.path, r.line, "persona.id: must be a slug of at most 32 characters");
      if (!ids.insert(r.id).second) errors.add(file.path, r.line, "persona.id: " + shown(r.id) + " is not unique");
      if (!is_persona_path(r.file)) errors.add(file.path, r.line, "persona.file: must be personas/<slug>.toml");
      if (const auto* a = t["npc_params"].as_array()) {
        for (const toml::node& p : *a) {
          const toml::table& pt = *p.as_table();
          NpcParamRow row;
          row.param = static_cast<std::uint32_t>(pt["param"].value_or(std::int64_t{0}));
          row.form = pt["form"].value_or(std::string());
          row.expect_name_id = static_cast<std::uint32_t>(pt["expect_name_id"].value_or(std::int64_t{0}));
          row.verified = pt["verified"].value_or(false);
          row.reveal_stage = pt["reveal_stage"].value_or(std::string());
          const auto [it, fresh] = params.emplace(row.param, r.id);
          if (!fresh && it->second != r.id) {
            errors.add(file.path, line_of(p), "persona.npc_params: a param row is bound to two personas");
          }
          r.npc_params.push_back(std::move(row));
        }
      }
      if (const auto* a = t["entity_ids"].as_array()) {
        for (const toml::node& e : *a) {
          const toml::table& et = *e.as_table();
          EntityRow row;
          row.id = static_cast<std::uint32_t>(et["id"].value_or(std::int64_t{0}));
          row.verified = et["verified"].value_or(false);
          const auto [it, fresh] = entities.emplace(row.id, r.id);
          if (!fresh && it->second != r.id) {
            errors.add(file.path, line_of(e), "persona.entity_ids: an entity id is bound to two personas");
          }
          r.entity_ids.push_back(row);
        }
      }
      rows.push_back(std::move(r));
    }
  }
  return rows;
}

// ---- personas/<id>.toml ----

std::optional<Persona> read_persona(const ContentFile& file, const PersonaRow& row, Errors& errors) {
  const std::size_t before = errors.count();
  auto table = parse(file, kPersonaFileMaxBytes, errors);
  if (!table) return std::nullopt;
  Checker checker(file.path, errors);
  checker.check(*table, kPersonaSpec, "");
  if (errors.count() != before) return std::nullopt;
  const toml::table& t = *table;
  Persona p;
  p.id = t["id"].value_or(std::string());
  p.file = file.path;
  p.origin = file.origin;
  if (p.id != row.id) errors.add(file.path, line_of(*t.get("id")), "id: must be the id its row in identity/personas.toml gives");
  // PL12: a shipped persona records how it was drafted and its review (a test persona and the player's own need not).
  if (file.origin == ContentOrigin::Shipped && !row.test_only && (!t.contains("drafted") || !t.contains("review"))) {
    errors.add(file.path, line_of(*t.get("id")), "drafted, review: a shipped persona must set both (PL12)");
  }
  if (const auto honesty = t["private"]["honesty"].value<std::string>()) {
    const std::string_view h = *honesty;
    const bool known = h.rfind("truthful", 0) == 0 || h.rfind("guarded", 0) == 0 || h.rfind("liar", 0) == 0;
    if (!known) errors.add(file.path, line_of(*t["private"]["honesty"].node()), "private.honesty: must start with truthful, guarded or liar");
  }
  p.display_name_fallback = t["display_name_fallback"].value_or(std::string());
  p.reg = enum_of<Register>(t["register"].value_or(std::string()), kRegisters, Register::Formal);
  p.reply_words_max = static_cast<std::uint8_t>(t["reply_words_max"].value_or(std::int64_t{40}));
  p.address_forms = strings_at(t, "address_forms");
  p.deflections = strings_at(t, "deflections");
  if (const auto* voice = t["voice"].as_table()) {
    p.openers = strings_at(*voice, "openers");
    if (const auto* tics = (*voice)["tics"].as_array()) {
      for (const toml::node& n : *tics) {
        const toml::table& tt = *n.as_table();
        p.tics.push_back(Tic{tt["text"].value_or(std::string()),
                             static_cast<std::uint8_t>(tt["max_per_conversation"].value_or(std::int64_t{1}))});
      }
    }
  }
  std::set<std::string> stage_ids;
  const toml::array& stages = *t["stages"].as_array();
  for (std::size_t i = 0; i < stages.size(); ++i) {
    const toml::table& st = *stages.get(i)->as_table();
    PersonaStage s;
    s.id = st["id"].value_or(std::string());
    s.when = st["when"].value_or(std::string());
    s.line = line_of(*stages.get(i));
    const std::string at = "stages[" + std::to_string(i) + "]";
    if (!is_slug(s.id, 32)) errors.add(file.path, s.line, at + ".id: must be a slug of at most 32 characters");
    if (!stage_ids.insert(s.id).second) errors.add(file.path, s.line, at + ".id: " + shown(s.id) + " is not unique");
    if (const auto v = st["state"].value<std::string>()) s.state = enum_of<Conscious>(*v, kState, Conscious::Unknown);
    if (const auto v = st["approach"].value<std::string>()) s.approach = enum_of<Approach>(*v, kApproach, Approach::Unknown);
    if (const auto v = st["chat"].value<std::string>()) {
      s.chat = enum_of<ChatMode>(*v, kChat, ChatMode::Mute);
      s.own_chat = true;
    }
    constexpr std::array<std::string_view, 12> kShows = {"unlock", "identity", "beckon", "gist", "where", "mood",
                                                         "road",   "at",       "hears",  "knows", "heard", "short"};
    s.shows = std::any_of(kShows.begin(), kShows.end(), [&](std::string_view k) { return st.contains(k); }) ||
              (s.own_chat && s.chat == ChatMode::Allowed);
    if (i == 0) {
      if (s.when != "true") errors.add(file.path, s.line, at + ".when: the base stage's must be \"true\"");
      for (const std::string_view k : {std::string_view("road"), std::string_view("at"), std::string_view("state")}) {
        if (!st.contains(k)) errors.add(file.path, s.line, at + "." + std::string(k) + ": the base stage must set it");
      }
    }
    if (i == 0 && st.contains("unlock")) errors.add(file.path, s.line, at + ".unlock: never on the base stage");
    if (st.contains("beckon") != st.contains("beckon_src")) {
      errors.add(file.path, s.line, at + ".beckon: needs beckon_src, and beckon_src needs beckon");
    }
    if (const auto src = st["beckon_src"].value<std::string>(); src && src->rfind("ci:", 0) == 0 && !row.test_only) {
      errors.add(file.path, s.line, at + ".beckon_src: a ci: record only on a test_only persona");
    }
    p.stages.push_back(std::move(s));
  }
  if (errors.count() != before) return std::nullopt;
  p.record = to_json_value(t);
  return p;
}

std::uint64_t fnv1a(std::string_view bytes, std::uint64_t h) {
  for (const char c : bytes) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001b3ull;
  }
  return h;
}

}  // namespace

std::string describe(const ContentError& e) {
  if (e.file.empty()) return e.problem;
  return e.file + ":" + std::to_string(e.line) + ": " + e.problem;
}

const char* to_string(Register v) noexcept {
  const auto i = static_cast<std::size_t>(v);
  return i < kRegisters.size() ? kRegisters[i].data() : "formal";
}
const char* to_string(ChatMode v) noexcept {
  const auto i = static_cast<std::size_t>(v);
  return i < kChat.size() ? kChat[i].data() : "mute";
}
const char* to_string(Conscious v) noexcept {
  const auto i = static_cast<std::size_t>(v);
  return i < kState.size() ? kState[i].data() : "unknown";
}

nlohmann::json Persona::resolved(std::size_t active) const {
  // The record with `stages` replaced by the active stage, §3.2's inheritance applied (`road`, `at`, `hears` and
  // `knows` from the last stage that set them)...
  nlohmann::json full = record;
  full.erase("stages");
  if (const auto rs = record.find("stages"); rs != record.end() && rs->is_array() && !rs->empty()) {
    const std::size_t last = std::min(active, rs->size() - 1);
    nlohmann::json stage = (*rs)[last];
    for (const char* k : {"road", "at", "hears", "knows"}) {
      if (stage.contains(k)) continue;
      for (std::size_t i = last; i-- > 0;) {
        if ((*rs)[i].contains(k)) {
          stage[k] = (*rs)[i][k];
          break;
        }
      }
    }
    full["stage"] = std::move(stage);
  }
  // ...then only the fields the dialogue prompt reads (kRecordFields), so nothing that is never rendered reaches the
  // renderer: the stages' `forbid` (eval-only, O36, O86), `when` and gate fields, the seed, the deflections, awareness.
  nlohmann::json out = nlohmann::json::object();
  for (const std::string_view path : kRecordFields) {
    const nlohmann::json* v = &full;
    std::string pointer;
    for (std::size_t start = 0; v != nullptr && start <= path.size();) {
      const std::size_t dot = std::min(path.find('.', start), path.size());
      const std::string key(path.substr(start, dot - start));
      const auto it = v->is_object() ? v->find(key) : v->end();
      v = v->is_object() && it != v->end() ? &*it : nullptr;
      pointer += "/" + key;
      start = dot + 1;
    }
    if (v != nullptr) out[nlohmann::json::json_pointer(pointer)] = *v;
  }
  return out;
}

Conscious Persona::state_at(std::size_t active) const noexcept {
  if (stages.empty()) return Conscious::Unknown;
  for (std::size_t i = std::min(active, stages.size() - 1) + 1; i-- > 0;) {
    if (stages[i].state) return *stages[i].state;
  }
  return Conscious::Unknown;
}

Approach Persona::approach_at(std::size_t active) const noexcept {
  if (stages.empty()) return Approach::Unknown;
  for (std::size_t i = std::min(active, stages.size() - 1) + 1; i-- > 0;) {
    if (stages[i].approach) return *stages[i].approach;
  }
  return Approach::Unknown;  // unset reads unknown (§3.1)
}

ChatMode Persona::chat_at(std::size_t active) const noexcept {
  if (stages.empty()) return ChatMode::Mute;
  return stages[std::min(active, stages.size() - 1)].chat;
}

std::vector<std::string> Persona::forbid_at(std::size_t active) const {
  std::vector<std::string> out;
  const auto rs = record.find("stages");
  if (rs == record.end() || !rs->is_array() || rs->empty()) return out;
  const nlohmann::json& stage = (*rs)[std::min(active, rs->size() - 1)];
  if (const auto f = stage.find("forbid"); f != stage.end() && f->is_array()) {
    for (const auto& v : *f) {
      if (v.is_string()) out.push_back(v.get<std::string>());
    }
  }
  return out;
}

const Persona* ContentPack::persona(std::string_view id) const noexcept {
  for (const Persona& p : personas) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const PersonaRow* ContentPack::row(std::string_view id) const noexcept {
  for (const PersonaRow& r : rows) {
    if (r.id == id) return &r;
  }
  return nullptr;
}

proto::Expected<ContentPack, std::vector<ContentError>> load_content(std::span<const ContentFile> files) {
  std::vector<ContentError> problems;
  Errors errors(problems);
  // The overrides replace the shipped files of the same path whole (DESIGN §11.4).
  std::map<std::string, const ContentFile*> by_path;
  for (const ContentFile& f : files) {
    const auto it = by_path.find(f.path);
    if (it == by_path.end()) {
      by_path.emplace(f.path, &f);
    } else if (f.origin == ContentOrigin::Override || it->second->origin == f.origin) {
      if (it->second->origin == f.origin) errors.add(f.path, 0, "is given twice");
      it->second = &f;
    }
  }
  ContentPack pack;
  std::uint64_t h = 0xcbf29ce484222325ull;
  const auto hash_file = [&](const ContentFile& f) {
    h = fnv1a(f.path, h);
    h = fnv1a(std::string_view("\0", 1), h);
    h = fnv1a(f.text, h);
    h = fnv1a(std::string_view("\0", 1), h);
  };
  const auto index = by_path.find(std::string(kPersonaIndexPath));
  if (index == by_path.end()) {
    errors.add(std::string(kPersonaIndexPath), 0, "is missing");
    errors.finish();
    return proto::Unexpected{std::move(problems)};
  }
  hash_file(*index->second);
  pack.rows = read_index(*index->second, errors);
  if (const auto disable = by_path.find(std::string(kDisablePath)); disable != by_path.end()) {
    hash_file(*disable->second);
    read_disable(*disable->second, pack, errors);
  }
  for (const PersonaRow& row : pack.rows) {
    if (!row.enabled) continue;
    const auto f = by_path.find(row.file);
    if (f == by_path.end()) {
      errors.add(index->second->path, row.line, "persona.file: " + shown(row.file) + " is missing");
      continue;
    }
    hash_file(*f->second);
    if (auto p = read_persona(*f->second, row, errors)) pack.personas.push_back(std::move(*p));
  }
  for (const auto& [path, f] : by_path) {
    if (path.rfind("identity/", 0) != 0 && path.rfind("personas/", 0) != 0 && path != kDisablePath) {
      pack.others.push_back(ContentFile{path, f->text, f->origin});
    }
  }
  errors.finish();
  if (!problems.empty()) return proto::Unexpected{std::move(problems)};
  pack.hash = h;
  return pack;
}

}  // namespace en::core
