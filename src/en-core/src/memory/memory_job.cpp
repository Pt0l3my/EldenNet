// The memory job's input and prompt (memory_job.hpp; MEM §2.6, §3.3 to §3.5).
#include "en/core/memory_job.hpp"

#include <algorithm>
#include <cctype>
#include <map>

#include "en/augur/audience.hpp"
#include "en/core/facts.hpp"
#include "en/prompt/environment.hpp"
#include "en/prompt/messages.hpp"

namespace en::core {

namespace {

using json = nlohmann::json;

constexpr proto::Pt kMin = 60000;
constexpr std::size_t kTalkMax = 60, kNotesMax = 10, kContextMax = 12, kOverheardMax = 6;
constexpr std::string_view kMemoryFolder = "prompts/memory";
constexpr std::array<std::string_view, 6> kParts = {"01_frame.j2", "02_keeper.j2", "03_journal.j2",
                                                    "04_since.j2", "05_checks.j2", "06_task.j2"};

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// The word as the Tarnished typed it, when `word` occurs in `line` as a whole word (case-insensitive).
std::optional<std::string> whole_word(std::string_view line, std::string_view word) {
  const std::string l = lower(line);
  const std::string w = lower(word);
  for (std::size_t at = l.find(w); at != std::string::npos; at = l.find(w, at + 1)) {
    const bool left = at == 0 || std::isalpha(static_cast<unsigned char>(l[at - 1])) == 0;
    const std::size_t end = at + w.size();
    const bool right = end >= l.size() || std::isalpha(static_cast<unsigned char>(l[end])) == 0;
    if (left && right) return std::string(line.substr(at, w.size()));
  }
  return std::nullopt;
}

std::string payload_text(const proto::StoredEvent& e, const char* key) {
  const json p = json::parse(e.payload, nullptr, false);
  if (!p.is_object() || !p.contains(key) || !p[key].is_string()) return {};
  return p[key].get<std::string>();
}

std::string name_of_actor(const std::string& key, const InputCtx& ctx) {
  (void)ctx;
  if (key.rfind("persona:", 0) == 0) return "someone you know";  // another persona's runtime name: not in the store
  return "someone";
}

const char* certainty_word(std::string_view c) {
  if (c == "fact") return "knew";
  if (c == "belief") return "believed";
  return "heard";
}

std::string disposition_words(std::int8_t d) {
  switch (d) {
    case -2:
      return "distrust";
    case -1:
      return "wary";
    case 1:
      return "fairly well disposed";
    case 2:
      return "trusting";
    default:
      return "undecided";
  }
}

}  // namespace

std::string when_words(proto::Pt pt, proto::Pt now) {
  const proto::Pt gap = now > pt ? now - pt : 0;
  if (gap <= 10 * kMin) return "a moment ago";
  if (gap <= 60 * kMin) return "a little while ago";
  if (gap <= 6 * 60 * kMin) return "earlier today";
  return "some time ago";
}

JobInput assemble_input(const proto::db::JobInputResult& r, const InputCtx& ctx) {
  JobInput in;
  in.persona = r.job.persona;
  in.upto = r.job.upto_event_id.value_or(0);
  in.conv = r.job.conv_id;
  const Persona* p = ctx.persona;
  if (p != nullptr) {
    in.name = p->display_name_fallback;
    const json rec = p->resolved(ctx.stage);
    if (const auto pub = rec.find("public"); pub != rec.end() && pub->is_object()) {
      in.brief = pub->value("summary", std::string());
    }
    if (const auto priv = rec.find("private"); priv != rec.end() && priv->is_object()) {
      in.honesty = priv->value("honesty", std::string());
    }
    if (const auto st = rec.find("stage"); st != rec.end() && st->is_object()) {
      in.gist = st->value("gist", std::string());
      in.road = st->value("road", std::string());
    }
  }
  if (ctx.facts != nullptr) {
    if (const proto::FactRow* s = find_fact(*ctx.facts, fact::kStageSeen)) in.stage_seen = s->value;
  }
  in.new_world = ctx.new_world;
  if (r.journal) in.journal = journal_of(*r.journal);
  const std::string self = "persona:" + r.job.persona;
  std::vector<proto::RowId> own_convs;
  for (const proto::ConversationRow& c : r.conversations) {
    if (c.persona == r.job.persona) own_convs.push_back(c.id);
  }
  // Talk: this persona's conversations' committed lines, in order. Notes: the rest, as MEM §2.6 renders them.
  std::vector<JobLine> talk;
  struct Note {
    JobLine line;
    proto::Pt pt = 0;
  };
  std::vector<Note> notes;
  std::map<proto::RowId, Note> overheard;  // grouped per conversation
  proto::Pt last_fall = -10 * kMin - 1;
  for (const proto::StoredEvent& e : r.evidence) {
    in.as_of_pt = std::max(in.as_of_pt, e.t_play_end_ms.value_or(e.t_play_ms));
    const auto v = augur::visible_to(e, r.job.persona, augur::VisCtx{e.journey});
    if (!v) continue;
    const proto::How how = v->how;
    const bool speech = e.kind == "PlayerSaid" || e.kind == "NpcSaid";
    const bool mine = e.conv_id && std::find(own_convs.begin(), own_convs.end(), *e.conv_id) != own_convs.end();
    if (speech && mine && how == proto::How::Direct && e.text) {
      JobLine l;
      l.kind = JobLine::Kind::Talk;
      l.player = e.kind == "PlayerSaid";
      l.own = e.kind == "NpcSaid" && e.subject == self;
      l.who = l.player ? "The Tarnished" : in.name;
      l.text = *e.text;
      l.event = e.id;
      talk.push_back(std::move(l));
      continue;
    }
    if (speech && how == proto::How::Overheard && e.text && e.conv_id) {
      Note& n = overheard[*e.conv_id];
      if (n.line.text.empty()) {
        n.line.kind = JobLine::Kind::Note;
        n.line.overheard = true;
        n.line.text = "You overheard the Tarnished speaking with someone:";
        n.line.event = e.id;
      }
      n.line.player = n.line.player || e.kind == "PlayerSaid";
      const auto count = static_cast<std::size_t>(std::count(n.line.text.begin(), n.line.text.end(), '"')) / 2;
      if (count < kOverheardMax) {
        n.line.text += std::string(" ") + (e.kind == "PlayerSaid" ? "The Tarnished: \"" : "Them: \"") + *e.text + "\"";
      }
      n.pt = e.t_play_ms;
      continue;
    }
    Note n;
    n.pt = e.t_play_ms;
    n.line.kind = JobLine::Kind::Note;
    n.line.event = e.id;
    n.line.seen = how == proto::How::Direct || how == proto::How::Witnessed;
    if (e.kind == "PersonaAttacked") {
      n.line.text = e.subject == self ? "The Tarnished struck you."
                                      : "You saw the Tarnished strike " + name_of_actor(e.subject, ctx) + ".";
    } else if (e.kind == "FoughtBeside" && e.subject == self) {
      n.line.text = "You fought beside the Tarnished against a hard foe.";
    } else if (e.kind == "BossFightEnded" && payload_text(e, "outcome") == "Victory" && how == proto::How::Witnessed) {
      n.line.text = "You saw the Tarnished bring down a mighty foe.";
    } else if (e.kind == "PersonaDied" && how == proto::How::Witnessed) {
      const json pl = json::parse(e.payload, nullptr, false);
      const bool by = pl.is_object() && pl.value("by_player", false);
      n.line.text = "You saw " + name_of_actor(e.subject, ctx) + " die" + (by ? ", at the Tarnished's hand." : ".");
    } else if (e.kind == "PlayerDied" && how == proto::How::Witnessed) {
      if (e.t_play_ms - last_fall < 10 * kMin) continue;  // at most one fall per 10 min
      last_fall = e.t_play_ms;
      n.line.text = "You saw the Tarnished fall.";
    } else if (e.kind == "NativeTalkEnded" && e.subject == self) {
      n.line.text = "You and the Tarnished spoke in your usual way.";
      n.line.seen = false;
    } else {
      continue;
    }
    notes.push_back(std::move(n));
  }
  for (auto& [conv, n] : overheard) notes.push_back(std::move(n));
  // Notes: at most 10, newest first.
  std::stable_sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) { return a.pt > b.pt; });
  if (notes.size() > kNotesMax) notes.resize(kNotesMax);
  if (talk.size() > kTalkMax) {
    talk.erase(talk.begin(), talk.end() - static_cast<std::ptrdiff_t>(kTalkMax));
    in.talk_cut = true;
  }
  for (std::size_t i = 0; i < talk.size(); ++i) talk[i].id = "L" + std::to_string(i + 1);
  in.lines = std::move(talk);
  for (std::size_t i = 0; i < notes.size(); ++i) {
    notes[i].line.id = "N" + std::to_string(i + 1);
    notes[i].line.when = when_words(notes[i].pt, ctx.now);
    in.notes.push_back(std::move(notes[i].line));
  }
  // Context: the conversations' shown_context (MEM §12.7): world items by short with their certainty word, then the
  // "just now" lines.
  for (const proto::ConversationRow& c : r.conversations) {
    if (c.persona != r.job.persona) continue;
    const json a = json::parse(c.shown_context, nullptr, false);
    if (!a.is_array()) continue;
    for (const json& e : a) {
      if (in.context.size() >= kContextMax) break;
      JobLine l;
      l.kind = JobLine::Kind::Context;
      if (e.contains("item") && e["item"].is_string()) {
        const std::string sh = ctx.knowledge != nullptr ? ctx.knowledge->short_of(e["item"].get<std::string>()) : "";
        if (sh.empty()) continue;
        l.world = true;
        l.text = std::string("(") + certainty_word(e.value("certainty", std::string("rumour"))) + ") " + sh;
      } else if (e.contains("line") && e["line"].is_string()) {
        l.just_now = true;
        l.text = e["line"].get<std::string>();
      } else {
        continue;
      }
      const bool dup =
          std::any_of(in.context.begin(), in.context.end(), [&](const JobLine& x) { return x.text == l.text; });
      if (!dup) in.context.push_back(std::move(l));
    }
  }
  for (std::size_t i = 0; i < in.context.size(); ++i) in.context[i].id = "C" + std::to_string(i + 1);
  // The checks offered (MEM §2.5): a boss the Tarnished named in a committed line of theirs, the Great Runes, the name.
  if (ctx.knowledge != nullptr) {
    for (const CatalogueFlag& f : ctx.knowledge->catalogue().flags) {
      if (f.alias.rfind("boss.", 0) != 0 || f.match.empty()) continue;
      const std::string boss = f.alias.substr(5);
      std::optional<std::string> said;
      for (const JobLine& l : in.lines) {
        if (!l.player) continue;
        for (const std::string& m : f.match) {
          if (!said) said = whole_word(l.text, m);
        }
      }
      if (said) {
        in.checks.push_back(CheckOffer{"slew:" + boss, "the Tarnished says they killed the one they called " + *said});
      }
    }
  }
  for (int n = 1; n <= 7; ++n) {
    in.checks.push_back(CheckOffer{"great_runes>=" + std::to_string(n), "the Tarnished says they hold at least " +
                                                                            std::to_string(n) + " of the Great Runes"});
  }
  in.checks.push_back(CheckOffer{"name", "the name the Tarnished gave as their own"});
  return in;
}

proto::Expected<std::vector<proto::Message>, std::string> memory_messages(std::span<const ContentFile> files,
                                                                          const JobInput& in,
                                                                          std::string_view reminder) {
  std::map<std::string, std::string> mine;
  for (const ContentFile& f : files) {
    if (f.path.rfind(std::string(kMemoryFolder) + "/", 0) == 0) mine[f.path] = f.text;
  }
  for (const std::string_view part : kParts) {
    if (!mine.contains(std::string(kMemoryFolder) + "/" + std::string(part))) {
      return proto::Unexpected{"the pack has no " + std::string(kMemoryFolder) + "/" + std::string(part)};
    }
  }
  json keepsakes = json::array();
  std::string summary, told;
  int disposition = 0;
  if (in.journal) {
    summary = in.journal->summary;
    told = in.journal->told_name.value_or(std::string());
    disposition = in.journal->disposition;
    const auto prot = protected_keepsake(*in.journal);
    for (std::size_t i = 0; i < in.journal->keepsakes.size(); ++i) {
      const Keepsake& k = in.journal->keepsakes[i];
      char w[8];
      std::snprintf(w, sizeof w, "%.2f", k.weight);
      keepsakes.push_back({{"id", k.id},
                           {"basis", k.basis == Keepsake::Basis::Seen ? "seen" : "said"},
                           {"weight", w},
                           {"about", k.about},
                           {"protected", prot && *prot == i},
                           {"text", k.text}});
    }
  }
  const auto lines = [](const std::vector<JobLine>& v) {
    json a = json::array();
    for (const JobLine& l : v) a.push_back({{"id", l.id}, {"who", l.who}, {"text", l.text}, {"when", l.when}});
    return a;
  };
  json checks = json::array();
  for (const CheckOffer& c : in.checks) checks.push_back({{"key", c.key}, {"meaning", c.meaning}});
  const json context = {{"name", in.name},
                        {"brief", in.brief},
                        {"honesty", in.honesty},
                        {"gist", in.gist},
                        {"road", in.road},
                        {"new_world", in.new_world},
                        {"summary", summary},
                        {"keepsakes", keepsakes},
                        {"disposition", disposition},
                        {"disposition_words", disposition_words(static_cast<std::int8_t>(disposition))},
                        {"told_name", told},
                        {"talk_cut", in.talk_cut},
                        {"lines", lines(in.lines)},
                        {"notes", lines(in.notes)},
                        {"context", lines(in.context)},
                        {"checks", checks},
                        {"reminder", std::string(reminder)}};
  try {
    const prompt::Environment env(std::make_shared<const prompt::MemorySource>(std::move(mine)));
    std::vector<proto::Message> out;
    for (const std::string_view part : kParts) {
      for (const prompt::Message& m : env.render(std::string(kMemoryFolder) + "/" + std::string(part), context)) {
        proto::Message msg;
        msg.role = m.role == prompt::Role::system ? proto::Role::System
                   : m.role == prompt::Role::user ? proto::Role::User
                                                  : proto::Role::Assistant;
        for (const prompt::Part& pp : m.parts) msg.parts.push_back(proto::Part{pp.text, pp.cache});
        proto::normalise(msg);
        out.push_back(std::move(msg));
      }
    }
    return out;
  } catch (const std::exception& e) {
    return proto::Unexpected{std::string("the memory prompt could not be made: ") + e.what()};
  }
}

json memory_schema() {
  const json id = {{"type", json::array({"string", "null"})}, {"pattern", "^k[0-9]{1,4}$"}};
  const json keepsake = {
      {"type", "object"},
      {"additionalProperties", false},
      {"required", json::array({"id", "text", "basis", "weight", "about", "refs", "check_key"})},
      {"properties",
       {{"id", id},
        {"text", {{"type", "string"}, {"maxLength", 200}}},
        {"basis", {{"enum", json::array({"said", "seen"})}}},
        {"weight", {{"type", "number"}, {"minimum", 0}, {"maximum", 1}}},
        {"about", {{"enum", json::array({"tarnished", "self", "world"})}}},
        {"refs",
         {{"type", "array"}, {"maxItems", 4}, {"items", {{"type", "string"}, {"pattern", "^[LNC][0-9]{1,3}$"}}}}},
        {"check_key", {{"type", json::array({"string", "null"})}, {"maxLength", 32}}}}}};
  return {
      {"type", "object"},
      {"additionalProperties", false},
      {"required", json::array({"summary", "keep", "keepsakes", "told_name", "disposition_delta"})},
      {"properties",
       {{"summary", {{"type", "string"}, {"maxLength", 900}}},
        {"keep", {{"type", "array"}, {"maxItems", 8}, {"items", {{"type", "string"}, {"pattern", "^k[0-9]{1,4}$"}}}}},
        {"keepsakes", {{"type", "array"}, {"maxItems", 6}, {"items", keepsake}}},
        {"told_name", {{"type", json::array({"string", "null"})}, {"maxLength", 32}}},
        {"disposition_delta", {{"enum", json::array({-1, 0, 1})}}}}}};
}

}  // namespace en::core
