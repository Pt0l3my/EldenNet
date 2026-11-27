// Tools › Workshop's keeping (workshop.hpp).
#include "en/shell/workshop.hpp"

#include <algorithm>
#include <cctype>

#include "en/augur/golden.hpp"
#include "en/augur/rules.hpp"
#include "en/augur/signals.hpp"
#include "en/augur/words.hpp"
#include "en/core/knowledge.hpp"
#include "en/proto/fixed.hpp"

namespace en::shell {

namespace {

// A byte that belongs to a word: an ASCII letter or digit, or any byte of a multi-byte character.
bool word_byte(char c) noexcept {
  const auto u = static_cast<unsigned char>(c);
  return u >= 0x80 || std::isalnum(u) != 0;
}

bool same_ascii(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto x = static_cast<unsigned char>(a[i]);
    const auto y = static_cast<unsigned char>(b[i]);
    if (x == y) continue;
    if (x >= 0x80 || y >= 0x80 || std::tolower(x) != std::tolower(y)) return false;
  }
  return true;
}

// The text cut to `max` bytes at a character's start; the bytes left out added to `cut`.
std::string bounded(std::string text, std::size_t max, std::size_t& cut) {
  if (text.size() <= max) return text;
  const std::size_t keep = proto::utf8_prefix(text, max);
  cut += text.size() - keep;
  text.resize(keep);
  return text;
}

const char* ng_word(core::Ng ng) noexcept {
  switch (ng) {
    case core::Ng::Reset: return "reset";
    case core::Ng::Keep: return "keep";
    case core::Ng::Unknown: break;
  }
  return "unknown";
}

}  // namespace

std::string mask_name(std::string_view text, std::string_view name) {
  if (name.empty() || text.size() < name.size()) return std::string(text);
  std::string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size()) {
    if (i + name.size() <= text.size() && same_ascii(text.substr(i, name.size()), name) &&
        (i == 0 || !word_byte(text[i - 1])) &&
        (i + name.size() == text.size() || !word_byte(text[i + name.size()]))) {
      out += kNameMask;
      i += name.size();
      continue;
    }
    out.push_back(text[i++]);
  }
  return out;
}

std::string unknown_toml(std::uint32_t param, std::uint32_t entity) {
  const std::string id = "new_" + std::to_string(param);
  std::string out = "[[persona]]\n";
  out += "id = \"" + id + "\"  # yours to name: a-z, 0-9 and _, at most 32\n";
  out += "file = \"personas/" + id + ".toml\"  # its record, yours to write (NPC §3.1)\n";
  out += "npc_params = [{ param = " + std::to_string(param) + ", form = \"default\", verified = false }]\n";
  if (entity != 0) out += "entity_ids = [{ id = " + std::to_string(entity) + ", verified = false }]\n";
  out += "enabled = true\n";
  return out;
}

void Workshop::clear() {
  view_ = proto::WorkshopView{};
  handles_.clear();
  stream_.clear();
}

std::string Workshop::scrubbed(std::string_view text) const { return scrub_ ? scrub_(text) : std::string(text); }

std::string Workshop::name_of(proto::ChrHandle h) {
  const auto it = handles_.find(h.raw);
  if (it != handles_.end()) return "h" + std::to_string(it->second);
  if (handles_.size() >= kWorkshopHandles) return "h?";
  const auto n = static_cast<std::uint32_t>(handles_.size() + 1);
  handles_.emplace(h.raw, n);
  return "h" + std::to_string(n);
}

void Workshop::sent(const proto::LlmRequest& request, proto::CallPurpose purpose, std::uint64_t at_ms) {
  const auto* chat = std::get_if<proto::ChatRequest>(&request.body);
  if (chat == nullptr) return;
  proto::WorkshopExchange x;
  x.any = true;
  x.id = request.id;
  x.at_ms = at_ms;
  x.slot = chat->slot;
  x.purpose = proto::words(purpose);
  std::size_t room = proto::kWorkshopMessagesMax;
  for (const proto::Message& m : chat->messages) {
    proto::WorkshopMessage w;
    w.role = proto::to_string(m.role);
    w.cache_break = m.has_cache_break();
    std::string text = scrubbed(m.joined());
    if (text.size() > room) {
      // Cut at a character's start; the messages after it are left out whole.
      const std::size_t keep = proto::utf8_prefix(text, room);
      x.messages_cut += text.size() - keep;
      text.resize(keep);
      room = 0;
    } else {
      room -= text.size();
    }
    w.text = std::move(text);
    x.messages.push_back(std::move(w));
  }
  x.max_tokens = chat->max_tokens.value_or(-1);
  x.temperature = chat->temperature.value_or(-1.0);
  x.schema = chat->json_schema.has_value();
  x.stops = chat->stop.size();
  view_.exchange = std::move(x);
  stream_.clear();
}

void Workshop::event(const proto::LlmEvent& e, std::uint64_t at_ms) {
  proto::WorkshopExchange& x = view_.exchange;
  if (!x.any || e.id != x.id || x.ended) return;
  const auto shown = [](const std::string& id) { return proto::shown_model(id) ? id : std::string(); };
  switch (e.kind) {
    case proto::EventKind::Started:
      x.started = true;
      x.provider = e.provider;
      x.model = shown(e.model);
      break;
    case proto::EventKind::Delta:
      // The stream's pieces may split a key or the name: the whole of what came is scrubbed again each time.
      if (stream_.size() < proto::kWorkshopReplyMax) stream_ += e.text;
      x.text_cut = 0;
      x.text = bounded(scrubbed(stream_), proto::kWorkshopReplyMax, x.text_cut);
      break;
    case proto::EventKind::Reset:
      stream_.clear();
      x.text.clear();
      x.text_cut = 0;
      break;
    case proto::EventKind::Ended: {
      x.ended = true;
      x.total_ms = static_cast<std::int64_t>(at_ms >= x.at_ms ? at_ms - x.at_ms : 0);
      if (!e.chat) break;
      const proto::ChatResult& r = *e.chat;
      if (!r.provider.empty()) x.provider = r.provider;
      if (!r.model.empty()) x.model = shown(r.model);
      x.served_model = shown(r.served_model);
      x.text_cut = 0;
      x.text = bounded(scrubbed(r.text), proto::kWorkshopReplyMax, x.text_cut);
      x.raw.clear();
      x.raw_cut = 0;
      if (r.raw_text != r.text) x.raw = bounded(scrubbed(r.raw_text), proto::kWorkshopReplyMax, x.raw_cut);
      x.finish = r.finish_reason;
      x.error = r.error;
      for (auto it = r.attempts.rbegin(); it != r.attempts.rend(); ++it) {
        if (it->http_status != 0) {
          x.http_status = it->http_status;
          break;
        }
      }
      x.tokens_in = r.usage.prompt_tokens;
      x.tokens_out = r.usage.completion_tokens;
      x.tokens_cached = r.usage.cached_tokens;
      x.first_token_ms = r.first_token.count();
      x.total_ms = r.total.count();
      x.attempts = static_cast<std::uint32_t>(r.attempts.size());
      break;
    }
    case proto::EventKind::WarmUpEnded:
    case proto::EventKind::ModelsEnded: break;
  }
  if (x.ended) stream_.clear();
}

proto::WorkshopEvent Workshop::event_row(const augur::Event& ev, std::uint64_t t_ms) {
  ++view_.events.seen;
  proto::WorkshopEvent row;
  row.t_ms = t_ms;
  row.kind = augur::kind_word(ev.kind);
  row.cls = augur::to_string(ev.event_class);
  row.source = augur::to_string(ev.source);
  std::string line = augur::golden_line(ev, [this](proto::ChrHandle h) { return name_of(h); });
  std::size_t cut = 0;
  row.line = bounded(scrubbed(line), proto::kWorkshopEventLineMax, cut);
  return row;
}

void Workshop::drops(std::uint64_t t, std::uint64_t k) noexcept {
  view_.events.dropped_t = t;
  view_.events.dropped_k = k;
}

void Workshop::see(const proto::WorldSnapshot& snapshot, const core::IdentityIndex& index, std::uint64_t t_ms) {
  std::vector<proto::WorkshopUnknown>& rows = view_.unknown;
  for (const proto::Chr& c : snapshot.chars) {
    if (index.resolve(c)) continue;
    auto it = std::find_if(rows.begin(), rows.end(), [&](const proto::WorkshopUnknown& u) {
      return u.param == c.param && u.entity == c.entity;
    });
    if (it == rows.end()) {
      if (rows.size() >= proto::kWorkshopUnknownMax) {
        // The least recently seen gives way.
        auto oldest = std::min_element(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
          return a.last_ms < b.last_ms;
        });
        rows.erase(oldest);
      }
      proto::WorkshopUnknown u;
      u.param = c.param;
      u.entity = c.entity;
      u.first_ms = t_ms;
      u.nearest_m = c.dist_m;
      u.toml = unknown_toml(c.param, c.entity);
      rows.push_back(std::move(u));
      it = rows.end() - 1;
    }
    it->model = c.model;
    it->kind = c.kind;
    it->team = c.team;
    it->last_ms = t_ms;
    ++it->seen;
    it->nearest_m = std::min(it->nearest_m, c.dist_m);
  }
  // The most recently seen first, then by row.
  std::stable_sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
    return a.last_ms != b.last_ms ? a.last_ms > b.last_ms : a.param < b.param;
  });
}

proto::WorkshopWorld Workshop::world_of(const core::Core& core) {
  proto::WorkshopWorld w;
  const core::Knowledge& k = core.knowledge();
  const core::FlagState& fs = core.flags();
  const core::WatchList& watch = core.watch_list();
  const core::KnowledgeView& kv = core.known_view();
  w.have_frame = fs.have_frame;
  w.journey = fs.journey;
  w.watch_version = watch.version;
  w.watched = watch.ids.size();
  w.region = fs.region.value_or(std::string());
  const auto& flags = k.catalogue().flags;
  const core::Realm realm = core.directory().index().realm();
  for (std::size_t i = 0; i < flags.size(); ++i) {
    if (w.flags.size() >= proto::kWorkshopRowsMax) {
      ++w.flags_cut;
      continue;
    }
    const core::CatalogueFlag& f = flags[i];
    proto::WorkshopFlag row;
    row.alias = f.alias;
    row.id = f.id;
    row.block = f.block;
    row.offset = f.offset;
    row.watched = std::find(watch.flags.begin(), watch.flags.end(), i) != watch.flags.end();
    row.bit = i < fs.bits.size() && fs.bits[i] != 0;
    row.valid = i < fs.valid.size() && fs.valid[i] != 0;
    row.verified_file = f.verified;
    row.verified_here = k.catalogue().verified(i, realm);
    row.reads = core::to_string(fs.flag(i));
    row.kind = f.outcome ? "outcome" : "state";
    row.ng = ng_word(f.ng);
    row.spoiler = f.spoiler;
    w.flags.push_back(std::move(row));
  }
  const auto stages = k.stages();
  for (std::size_t i = 0; i < stages.size(); ++i) {
    proto::WorkshopStage row;
    row.id = stages[i].id;
    row.value = i < kv.world.stages.size() ? core::to_string(kv.world.stages[i]) : "U";
    row.when = stages[i].when;
    row.boss = stages[i].boss;
    row.spoiler = stages[i].spoiler;
    row.rank = stages[i].rank;
    w.stages.push_back(std::move(row));
  }
  const auto tracks = k.tracks();
  for (std::size_t i = 0; i < tracks.size(); ++i) {
    proto::WorkshopTrack row;
    row.id = tracks[i].id;
    if (i < kv.world.tracks.size() && kv.world.tracks[i] && *kv.world.tracks[i] < tracks[i].steps.size()) {
      row.step = tracks[i].steps[*kv.world.tracks[i]].id;
    }
    w.tracks.push_back(std::move(row));
  }
  return w;
}

proto::WorkshopKnownSets Workshop::known_of(const core::Core& core) {
  proto::WorkshopKnownSets out;
  const core::KnowledgeView& kv = core.known_view();
  const core::Directory& d = core.directory();
  out.personas = kv.personas.size();
  for (std::size_t p = 0; p < kv.personas.size(); ++p) {
    const core::Grounding& g = kv.personas[p];
    const std::string persona = p < d.size() ? d.persona(p).id : std::to_string(p);
    const std::string stage = p < d.size() && g.stage < d.persona(p).stages.size()
                                  ? d.persona(p).stages[g.stage].id
                                  : std::to_string(g.stage);
    for (const core::Known& k : g.known) {
      if (out.rows.size() >= proto::kWorkshopRowsMax) {
        ++out.cut;
        continue;
      }
      proto::WorkshopKnown row;
      row.persona = persona;
      row.stage = stage;
      row.item = k.item;
      row.certainty = core::to_string(k.certainty);
      row.path = core::to_string(k.path);
      row.own_affairs = k.own_affairs;
      if (std::find(g.shown.begin(), g.shown.end(), k.item) != g.shown.end()) {
        row.block5b = "shown";
      } else if (std::find(g.offered.begin(), g.offered.end(), k.item) != g.offered.end()) {
        row.block5b = "offered";
      }
      out.rows.push_back(std::move(row));
    }
  }
  return out;
}

proto::WorkshopKinds Workshop::kinds_of(const core::Core& core) {
  proto::WorkshopKinds out;
  const std::shared_ptr<const augur::RuleSet>& set = core.augur().rules();
  for (std::size_t i = 0; i < augur::kKindCount; ++i) {
    proto::WorkshopKind k;
    k.name = std::string(augur::kind_name(i));
    std::string placeholders;
    for (const augur::PayloadInfo& p : augur::payloads()) {
      if (p.kind != k.name) continue;
      if (!k.built_by.empty()) k.built_by += ", ";
      k.built_by += p.name;
      for (const std::string_view ph : p.placeholders) {
        if (ph.empty() || placeholders.find(ph) != std::string::npos) continue;
        placeholders += (placeholders.empty() ? "" : ", ") + std::string(ph);
      }
    }
    if (!k.built_by.empty()) k.placeholders = placeholders.empty() ? "subject" : "subject, " + placeholders;
    k.waits_for = std::string(augur::waits_for(i));
    k.fields = augur::fields_text(i);
    if (set) {
      k.rules = static_cast<std::size_t>(std::count_if(set->rules().begin(), set->rules().end(),
                                                       [&](const augur::Rule& r) { return r.kind == i; }));
    }
    out.kinds.push_back(std::move(k));
  }
  if (set) {
    for (const augur::Rule& r : set->rules()) {
      proto::WorkshopRule row;
      row.id = r.id ? *r.id : std::string();
      row.kind = std::string(augur::kind_name(r.kind));
      row.cls = augur::to_string(r.cls);
      row.signal = std::string(augur::signal_info(r.signal).name);
      row.detector = augur::to_string(r.detect.kind);
      row.radius = augur::to_string(r.radius);
      row.key = r.key;
      row.ttl_s = r.ttl_s;
      row.cooldown_s = r.cooldown_s;
      row.where = r.file + ":" + std::to_string(r.line);
      out.rules.push_back(std::move(row));
    }
  }
  return out;
}

}  // namespace en::shell
