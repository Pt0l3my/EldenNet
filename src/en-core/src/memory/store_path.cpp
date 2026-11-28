// The store in the core (store_path.hpp; ARCH §9; MEM §1.5, §8.3, §8.6, §9.1).
#include "en/core/store_path.hpp"

#include <algorithm>
#include <charconv>
#include <nlohmann/json.hpp>

#include "en/augur/audience.hpp"
#include "en/augur/words.hpp"
#include "en/core/facts.hpp"
#include "en/proto/blake3.hpp"

namespace en::core {

namespace {

using json = nlohmann::json;
namespace db = proto::db;

template <class... F>
struct Overloaded : F... {
  using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

// A text's length as SQLite's length() counts it, in code points: a page's `end` in an NpcSaid's `pages` is where the
// trim's substr() cuts the text, so it is counted the same way (MEM §9.6's T1: a cut between two pages keeps the
// first).
std::size_t sql_length(std::string_view text) noexcept {
  std::size_t n = 0;
  for (const char c : text) n += (static_cast<unsigned char>(c) & 0xC0u) != 0x80u ? 1u : 0u;
  return n;
}

std::string hex(std::span<const std::uint8_t> bytes) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const std::uint8_t b : bytes) {
    out.push_back(kDigits[b >> 4]);
    out.push_back(kDigits[b & 0xF]);
  }
  return out;
}

// UTF-8 to UTF-16LE code units (MEM §8.3: the name as the save holds it). A malformed byte is taken as itself.
std::vector<std::uint8_t> utf16le(std::string_view s) {
  std::vector<std::uint8_t> out;
  const auto put = [&](std::uint32_t u) {
    out.push_back(static_cast<std::uint8_t>(u & 0xFF));
    out.push_back(static_cast<std::uint8_t>(u >> 8));
  };
  for (std::size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    std::uint32_t cp = c;
    std::size_t n = 1;
    if (c >= 0xF0 && i + 3 < s.size()) {
      cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
           ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
      n = 4;
    } else if (c >= 0xE0 && i + 2 < s.size()) {
      cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
      n = 3;
    } else if (c >= 0xC0 && i + 1 < s.size()) {
      cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
      n = 2;
    }
    if (cp >= 0x10000) {
      cp -= 0x10000;
      put(0xD800 + (cp >> 10));
      put(0xDC00 + (cp & 0x3FF));
    } else {
      put(cp);
    }
    i += n;
  }
  return out;
}

std::string actor_key(const augur::ActorRef& a, const Directory& cast) {
  switch (a.kind) {
    case augur::ActorRef::Kind::Player:
      return "player";
    case augur::ActorRef::Kind::Persona:
      return a.id < cast.size() ? "persona:" + cast.persona(a.id).id : "mute";
    case augur::ActorRef::Kind::Card:
      return "card:" + std::to_string(a.id) + "/" + std::to_string(a.variant);
    case augur::ActorRef::Kind::Mute:
      return "mute";
  }
  return "mute";
}

std::string stage_id(const Directory& cast, std::uint32_t persona, std::size_t stage) {
  if (persona >= cast.size()) return {};
  const Persona& p = cast.persona(persona);
  return stage < p.stages.size() ? p.stages[stage].id : std::string();
}

std::string stage_now(const Directory& cast, const KnowledgeView* known, std::uint32_t persona) {
  const std::size_t s = known != nullptr && persona < known->personas.size() ? known->personas[persona].stage : 0;
  return stage_id(cast, persona, s);
}

void bars_json(const proto::FixedVector<augur::BossRef, 3>& bars, json& j) {
  json a = json::array();
  for (const augur::BossRef& b : bars) a.push_back(b.name_id);
  j["bars"] = a;
}

// The stored payload of each P kind (MEM §1.1): ids and numbers, never a handle of the session.
json payload_of(const augur::Event& ev, const Directory& cast, const KnowledgeView* known) {
  json j = json::object();
  std::visit(
      Overloaded{
          [&](const augur::PlaceEntered& e) {
            j["place_id"] = e.place_id;
            j["prev"] = e.prev ? json(*e.prev) : json(nullptr);
          },
          [&](const augur::GraceRested& e) { j["grace"] = e.grace ? json(*e.grace) : json(nullptr); },
          [&](const augur::GraceLit& e) {
            j["flag"] = e.flag;
            j["grace"] = e.grace;
          },
          [&](const augur::FlagChanged& e) {
            j["alias"] = e.alias ? *e.alias : std::string();
            j["value"] = e.value;
            j["verified"] = e.verified;
          },
          [&](const augur::BossFightStarted& e) { bars_json(e.bars, j); },
          [&](const augur::BossFightEnded& e) {
            bars_json(e.bars, j);
            j["outcome"] = augur::to_string(e.outcome);
          },
          [&](const augur::PersonaAttacked& e) { j["via"] = augur::to_string(e.via); },
          [&](const augur::PersonaDied& e) { j["by_player"] = e.by_player; },
          [&](const augur::FoughtBeside& e) { j["boss"] = e.boss; },
          [&](const augur::NativeTalkEnded& e) {
            j["held_ms"] = e.held_ms;
            if (e.persona) j["stage"] = stage_now(cast, known, e.persona->index);
          },
          [&](const augur::StageChanged& e) {
            j["from"] = stage_id(cast, e.persona.index, e.from);
            j["to"] = stage_id(cast, e.persona.index, e.to);
          },
          [&](const augur::PlayerDied& e) {
            j["killer"] = e.killer ? json(actor_key(*e.killer, cast)) : json(nullptr);
            j["in_boss"] = e.in_boss;
          },
          [&](const augur::PlayerSaid& e) { j["source"] = e.input == augur::InputSource::Voice ? "voice" : "typed"; },
          [&](const augur::NpcSaid& e) {
            j["to"] = e.to ? json(actor_key(*e.to, cast)) : json(nullptr);
            j["motive"] = "reply";
            j["cue"] = nullptr;
            j["pages"] = json::array({json{{"end", 0}, {"pt", ev.t_play_ms}}});  // its end set at the commit, below
          },
          [&](const augur::JourneyChanged& e) { j["ng_lvl"] = e.ng_lvl; },
          [&](const auto&) {},
      },
      ev.kind);
  return j;
}

}  // namespace

proto::PlaythroughKey playthrough_key(std::uint64_t steamid64, std::uint8_t slot, std::string_view name,
                                      std::uint8_t archetype, std::uint8_t gift) {
  const proto::Blake3::Digest key = proto::Blake3::derive_key("eldennet playthrough fp v1").finish();
  proto::Blake3 h = proto::Blake3::keyed(key);
  std::vector<std::uint8_t> body;
  for (int i = 0; i < 8; ++i) body.push_back(static_cast<std::uint8_t>(steamid64 >> (8 * i)));
  body.push_back(slot);
  std::string_view trimmed = name;
  while (!trimmed.empty() && trimmed.back() == '\0') trimmed.remove_suffix(1);
  const std::vector<std::uint8_t> units = utf16le(trimmed);
  body.push_back(static_cast<std::uint8_t>(std::min<std::size_t>(units.size() / 2, 0xFF)));
  body.insert(body.end(), units.begin(), units.end());
  body.push_back(archetype);
  body.push_back(gift);
  h.update(std::as_bytes(std::span<const std::uint8_t>(body)));
  proto::PlaythroughKey k;
  k.steamid64 = steamid64;
  k.slot = slot;
  k.fp8 = hex(h.finish()).substr(0, 8);
  proto::Blake3 n;
  n.update(std::as_bytes(std::span<const std::uint8_t>(units)));
  k.name_hash = hex(n.finish());
  k.archetype = archetype;
  k.gift = gift;
  return k;
}

std::string memory_off_text(db::Fault cause) {
  std::string out = "Memory is off for this session: ";
  switch (cause) {
    case db::Fault::Busy:
      out += "its file is in use by another program.";
      break;
    case db::Fault::Full:
      out += "the disk is full.";
      break;
    case db::Fault::Corrupt:
    case db::Fault::NotADb:
      out += "its file is damaged. The newest backup will be used next time.";
      break;
    case db::Fault::TooNew:
      out += "its file was made by a newer EldenNet.";
      break;
    default:
      out += "its file could not be read or written.";
      break;
  }
  return out;
}

void StorePath::set_enabled(bool on) {
  if (enabled_ == on) return;
  enabled_ = on;
  if (!on) close();
}

void StorePath::open(const proto::PlaythroughKey& key, proto::Pt pt_now, std::uint16_t journey, Mono now) {
  close();
  now_ = now;
  if (!available()) return;
  key_ = key;
  open_pt_ = pt_now;
  open_journey_ = journey;
  journey_ = journey;
  busy_tries_ = 0;
  retry_at_.reset();
  mode_ = Mode::Opening;
  waiting_open_ = true;
  open_seq_ = ++seq_;
  queued_.push_back(proto::DbRequest{open_seq_, db::Open{key, pt_now, journey}});
  urgent_ = true;
}

void StorePath::close() {
  if (mode_ == Mode::Open || mode_ == Mode::Opening || mode_ == Mode::Choice) {
    queued_.push_back(proto::DbRequest{++seq_, db::Close{}});
    urgent_ = true;
  }
  mode_ = Mode::Off;
  waiting_open_ = false;
  key_.reset();
  choice_.reset();
  facts_.clear();
  convs_.clear();
  replies_.clear();
  bundles_.clear();
  bundle_seqs_.clear();
  jobs_.clear();
  job_seqs_.clear();
  input_seq_ = 0;
  lived_seq_ = 0;
  news_.clear();
  merge_after_.clear();
  dropped_ += held_.size();
  held_.clear();
  open_seq_ = fold_seq_ = 0;
  retry_at_.reset();
}

void StorePath::resolve(db::Choice choice) {
  if (mode_ != Mode::Choice) return;
  mode_ = Mode::Opening;
  open_seq_ = ++seq_;
  queued_.push_back(proto::DbRequest{open_seq_, db::Resolve{choice}});
  urgent_ = true;
}

bool StorePath::forget(std::uint16_t journey, const std::string& persona, proto::Pt at_pt) {
  if (mode_ != Mode::Open || journey != journey_) return false;
  send(db::Forget{journey, persona, at_pt}, true);  // at once: a forget's backup and its rows go before later writes
  const auto gone = [&](const std::string& p) { return persona.empty() || p == persona; };
  std::erase_if(facts_, [&](const auto& kv) { return gone(kv.first); });
  std::erase_if(bundles_, [&](const auto& kv) { return gone(kv.first); });
  std::erase_if(jobs_, [&](const auto& kv) { return gone(kv.first); });
  std::erase_if(news_, [&](const auto& kv) { return gone(kv.first); });
  std::erase_if(merge_after_, [&](const auto& kv) { return gone(kv.first); });
  return true;
}

void StorePath::fail(db::Fault cause, Effect& e) {
  mode_ = Mode::Down;
  dropped_ += held_.size();
  held_.clear();
  choice_.reset();
  e.ready = waiting_open_;
  waiting_open_ = false;
  e.notices.push_back(StoreNotice{ToastClass::Degraded, proto::ToastLevel::Warning, memory_off_text(cause)});
}

StorePath::Effect StorePath::on_result(const proto::DbResult& r) {
  Effect e;
  if (mode_ == Mode::Off || mode_ == Mode::Down) return e;
  if (const auto* t = std::get_if<db::Tripped>(&r.result)) {
    fail(t->cause, e);
    return e;
  }
  if (r.seq == open_seq_) {
    open_seq_ = 0;
    const db::Opened* o = std::get_if<db::Opened>(&r.result);
    if (const auto* res = std::get_if<db::Resolved>(&r.result)) o = &res->opened;
    if (const auto* f = std::get_if<db::Failed>(&r.result)) {
      if (f->fault == db::Fault::Busy && busy_tries_ < kBusyRetries && key_) {
        ++busy_tries_;
        retry_at_ = now_ + kBusyRetryMs;  // the lifecycle waits a little longer
        return e;
      }
      fail(f->fault, e);
      return e;
    }
    if (o == nullptr) return e;
    next_event_ = o->next_event_id;
    next_conv_ = o->next_conv_id;
    journey_ = o->journey;
    jobs_.clear();
    for (const proto::MemoryJob& j : o->open_jobs) {
      if (j.journey == journey_ && j.kind == proto::JobKind::Journal && j.state != proto::JobState::Done) {
        jobs_[j.persona] = j;
      }
    }
    facts_stale_ = o->facts_stale;
    if (o->restored_backup) {
      e.notices.push_back(StoreNotice{ToastClass::Degraded, proto::ToastLevel::Warning, std::string(kBackupRestored)});
    }
    if (o->low_disk) {
      e.notices.push_back(StoreNotice{ToastClass::Degraded, proto::ToastLevel::Warning, std::string(kLowDisk)});
    }
    e.ready = waiting_open_;
    waiting_open_ = false;
    if (o->rollback.kind == db::Rollback::Kind::Choice) {
      mode_ = Mode::Choice;
      choice_ = o->rollback.summary;
      return e;
    }
    choice_.reset();
    // The journey's facts, refolded before any write goes out (WHY§689).
    fold_seq_ = ++seq_;
    queued_.push_back(proto::DbRequest{fold_seq_, db::EventsForFold{journey_}});
    urgent_ = true;
    return e;
  }
  if (r.seq == fold_seq_) {
    fold_seq_ = 0;
    facts_.clear();
    if (const auto* f = std::get_if<db::FoldEvents>(&r.result)) {
      for (proto::PersonaFacts& p : fold_all(f->events)) facts_[p.persona] = std::move(p);
    }
    if (facts_stale_) {
      db::PutFacts put;
      put.journey = journey_;
      for (const auto& [id, f] : facts_) put.personas.push_back(f);
      send(std::move(put), false);
      facts_stale_ = false;
    }
    mode_ = Mode::Open;
    lift();
    return e;
  }
  if (const auto j = job_seqs_.find(r.seq); j != job_seqs_.end()) {
    if (const auto* saved = std::get_if<db::JobSaved>(&r.result)) {
      if (const auto it = jobs_.find(j->second); it != jobs_.end()) it->second.id = saved->id;
    }
    job_seqs_.erase(j);
    return e;
  }
  if (r.seq == lived_seq_ && lived_seq_ != 0) {
    lived_seq_ = 0;
    if (const auto* in = std::get_if<db::LivedInputResult>(&r.result)) {
      e.lived_input = *in;
      e.lived_persona = lived_persona_;
    }
    return e;
  }
  if (r.seq == input_seq_) {
    input_seq_ = 0;
    if (const auto* in = std::get_if<db::JobInputResult>(&r.result)) e.job_input = *in;
    return e;
  }
  if (const auto b = bundle_seqs_.find(r.seq); b != bundle_seqs_.end()) {
    const std::string persona = b->second;
    bundle_seqs_.erase(b);
    if (const auto* res = std::get_if<db::BundleResult>(&r.result)) {
      MemoryBundle m;
      if (res->journal) m.journal = journal_of(*res->journal);
      m.prev_tail = res->prev_tail;
      m.pending = res->pending_count;
      m.lived = res->lived;
      bundles_[persona] = std::move(m);
    }
    return e;
  }
  return e;  // a write's Done, or its fault: the store counts it (MEM §8.6)
}

void StorePath::want_bundle(const std::string& persona) {
  if (persona.empty() || mode_ != Mode::Open) return;  // before the file is open, the conversation's start asks
  for (const auto& [seq, p] : bundle_seqs_) {
    if (p == persona) return;  // one in flight already
  }
  const std::uint64_t seq = ++seq_;
  bundle_seqs_[seq] = persona;
  queued_.push_back(proto::DbRequest{seq, db::Bundle{journey_, persona}});
}

std::vector<proto::MemoryJob> StorePath::runnable() const {
  std::vector<proto::MemoryJob> out;
  for (const auto& [p, j] : jobs_) {
    if (j.state == proto::JobState::Queued && j.id != 0) out.push_back(j);
  }
  std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.created_pt < b.created_pt; });
  return out;
}

void StorePath::want_job_input(proto::RowId job) {
  if (mode_ != Mode::Open || input_seq_ != 0) return;
  input_seq_ = ++seq_;
  queued_.push_back(proto::DbRequest{input_seq_, db::JobInput{job}});
  urgent_ = true;
}

void StorePath::save_job(const proto::MemoryJob& job) {
  if (mode_ != Mode::Open) return;
  jobs_[job.persona] = job;
  const std::uint64_t seq = ++seq_;
  job_seqs_[seq] = job.persona;
  queued_.push_back(proto::DbRequest{seq, db::UpsertJob{job}});
  urgent_ = true;
  // A run that ended with a newer conversation waiting: that conversation's job now (MEM §3.1).
  if (job.state != proto::JobState::Running) {
    if (const auto m = merge_after_.find(job.persona); m != merge_after_.end()) {
      proto::MemoryJob next = job;
      if (job.state == proto::JobState::Done) {
        next.id = 0;
        next.from_event_id = job.upto_event_id;
      }
      next.upto_event_id = m->second;
      next.state = proto::JobState::Queued;
      next.attempts = 0;
      merge_after_.erase(m);
      save_job(next);
    }
  }
}

void StorePath::commit_journal(db::CommitJournal c) {
  if (mode_ != Mode::Open) return;
  const std::string persona = c.version.persona;
  for (const proto::FactRow& f : c.facts) {  // the verified facts, in RAM too
    proto::PersonaFacts& pf = facts_[persona];
    pf.persona = persona;
    std::erase_if(pf.facts, [&](const proto::FactRow& r) { return r.key == f.key; });
    pf.facts.push_back(f);
    std::sort(pf.facts.begin(), pf.facts.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
  }
  send(std::move(c), false);
  news_.erase(persona);
  if (auto it = jobs_.find(persona); it != jobs_.end()) {
    it->second.state = proto::JobState::Done;
    proto::MemoryJob done = it->second;
    jobs_.erase(it);
    if (merge_after_.contains(persona)) save_job(done);  // the waiting conversation's job, from this one's upto
  }
  bundles_.erase(persona);
  want_bundle(persona);
}

void StorePath::want_lived_input(const std::string& persona) {
  if (mode_ != Mode::Open || lived_seq_ != 0) return;
  lived_seq_ = ++seq_;
  lived_persona_ = persona;
  queued_.push_back(proto::DbRequest{lived_seq_, db::LivedInput{journey_, persona}});
  urgent_ = true;
}

void StorePath::commit_lived(db::CommitLived c) {
  if (mode_ != Mode::Open) return;
  const std::string persona = c.version.persona;
  send(std::move(c), false);
  bundles_.erase(persona);
  want_bundle(persona);
}

std::vector<std::pair<std::string, std::string>> StorePath::job_news(std::string_view persona) const {
  const auto it = news_.find(persona);
  return it != news_.end() ? it->second : std::vector<std::pair<std::string, std::string>>{};
}

const MemoryBundle* StorePath::bundle(std::string_view persona) const {
  const auto it = bundles_.find(persona);
  return it != bundles_.end() ? &it->second : nullptr;
}

void StorePath::tick(Mono now) {
  now_ = now;
  if (!retry_at_ || now < *retry_at_ || !key_ || mode_ != Mode::Opening) return;
  retry_at_.reset();
  open_seq_ = ++seq_;
  queued_.push_back(proto::DbRequest{open_seq_, db::Open{*key_, open_pt_, open_journey_}});
  urgent_ = true;
}

std::optional<proto::RowId> StorePath::conversation_row(proto::ConvId conv) const {
  const auto it = convs_.find(conv);
  if (it == convs_.end()) return std::nullopt;
  return it->second;
}

void StorePath::send(proto::DbRequest::Op op, bool speech) {
  queued_.push_back(proto::DbRequest{++seq_, std::move(op)});
  urgent_ = urgent_ || speech;
}

void StorePath::add(Op op) {
  if (mode_ == Mode::Open) {
    emit(op);
    return;
  }
  if (mode_ != Mode::Opening && mode_ != Mode::Choice) return;  // memory off: the RAM log alone
  // Held until the file is ready (MEM §9.1): a later page joins its held reply, and the ring drops the oldest
  // non-speech write when full.
  if (op.kind == Op::Kind::Page) {
    for (auto it = held_.rbegin(); it != held_.rend(); ++it) {
      if (it->kind == Op::Kind::Event && it->conv == op.conv && it->reply == op.reply && it->event.kind == "NpcSaid") {
        it->event.text = it->event.text.value_or("") + " " + op.text;
        json p = json::parse(it->event.payload, nullptr, false);
        if (p.is_object() && p["pages"].is_array()) {
          p["pages"].push_back(json{{"end", sql_length(*it->event.text)}, {"pt", op.pt}});
          it->event.payload = p.dump();
        }
        it->event.t_play_end_ms = op.pt;
        return;
      }
    }
  }
  if (held_.size() >= kHeldCap) {
    auto victim = std::find_if(held_.begin(), held_.end(), [](const Op& o) { return !o.speech; });
    if (victim == held_.end()) victim = held_.begin();
    held_.erase(victim);
    ++dropped_;
  }
  held_.push_back(std::move(op));
}

void StorePath::lift() {
  std::vector<Op> held = std::move(held_);
  held_.clear();
  for (Op& op : held) emit(op);
}

// A write goes out: its ids from the file's counters, an event's fold into the facts it moves.
void StorePath::emit(Op& op) {
  switch (op.kind) {
    case Op::Kind::ConvOpen: {
      op.row.id = next_conv_++;
      convs_[op.conv] = op.row.id;
      send(db::ConvOpen{op.row}, true);
      return;
    }
    case Op::Kind::ConvClose: {
      const auto row = conversation_row(op.conv);
      if (!row) return;
      op.row.id = *row;
      send(db::ConvClose{op.row}, true);
      return;
    }
    case Op::Kind::Page: {
      const auto it = replies_.find({op.conv, op.reply});
      if (it == replies_.end()) return;
      send(db::AppendPage{it->second, op.text, op.pt}, true);
      return;
    }
    case Op::Kind::Job: {
      // MEM §3.1: up to this conversation's end, the last event this file has been given.
      const proto::EventId upto = next_event_ - 1;
      auto it = jobs_.find(op.persona);
      if (it != jobs_.end() && it->second.state == proto::JobState::Running) {
        merge_after_[op.persona] = upto;  // the run in flight takes it once it ends
        return;
      }
      proto::MemoryJob job;
      if (it != jobs_.end()) {
        job = it->second;  // queued, deferred or parked: extended, its attempts reset
      } else {
        job.journey = journey_;
        job.persona = op.persona;
        job.kind = proto::JobKind::Journal;
        job.cause = proto::JobCause::Conversation;
        job.created_pt = op.pt;
      }
      job.upto_event_id = upto;
      if (const auto row = conversation_row(op.conv)) job.conv_id = *row;
      job.state = proto::JobState::Queued;
      job.attempts = 0;
      jobs_[op.persona] = job;
      save_job(job);
      return;
    }
    case Op::Kind::Event:
      break;
  }
  proto::StoredEvent& ev = op.event;
  ev.id = next_event_++;
  if (op.conv != 0) {
    if (const auto row = conversation_row(op.conv)) ev.conv_id = *row;
  }
  if (ev.kind == "NpcSaid" && op.conv != 0) replies_[{op.conv, op.reply}] = ev.id;
  // A later journey (NG+) starts a new partition of facts (MEM §9.4); the store starts its epoch.
  if (ev.kind == "JourneyChanged" && ev.journey > journey_) {
    journey_ = ev.journey;
    facts_.clear();
    bundles_.clear();
  }
  db::Append a;
  for (const auto& [persona, how] : augur::perceivers(ev, augur::VisCtx{ev.journey})) {
    if (ev.journey != journey_) continue;
    proto::PersonaFacts& f = facts_[persona];
    f.persona = persona;
    const std::vector<std::string> keys = fold_fact(f, ev, how);
    if (keys.empty()) continue;
    proto::PersonaFacts delta;
    delta.persona = persona;
    for (const std::string& k : keys) {
      if (const proto::FactRow* row = find_fact(f, k)) delta.facts.push_back(*row);
    }
    a.facts.push_back(std::move(delta));
  }
  a.event = std::move(ev);
  send(std::move(a), op.speech);
}

// The stored form of one P event: actors by profile id, its payload, its frozen audience.
proto::StoredEvent StorePath::stored(const augur::Event& ev, const Directory& cast, const KnowledgeView* known) const {
  proto::StoredEvent s;
  s.journey = static_cast<std::uint16_t>(ev.journey + 1);
  s.t_play_ms = static_cast<proto::Pt>(ev.t_play_ms);
  s.kind = std::string(augur::kind_name(ev.kind));
  s.rule = ev.rule ? *ev.rule : std::string();
  s.source = augur::to_string(ev.source);
  s.conf = ev.confidence;
  if (ev.subject) s.subject = actor_key(*ev.subject, cast);
  if (ev.object) s.object = actor_key(*ev.object, cast);
  if (ev.place.place_id) s.place_id = *ev.place.place_id;
  s.region = std::string(ev.place.region.view());
  if (ev.place.block.packed != 0) s.block = ev.place.block.packed;
  s.payload = payload_of(ev, cast, known).dump();
  augur::freeze_into(s, ev, [&](augur::PersonaId id) -> std::optional<std::string> {
    if (id.index >= cast.size()) return std::nullopt;
    return cast.persona(id.index).id;
  });
  return s;
}

void StorePath::write(const std::vector<augur::Event>& events, const Directory& cast, const KnowledgeView* known,
                      Mono now) {
  now_ = now;
  if (mode_ == Mode::Off || mode_ == Mode::Down) return;
  for (const augur::Event& ev : events) {
    if (ev.event_class != augur::Class::P) continue;
    // Speech is the conversation's, written with its commit (commit()).
    if (std::holds_alternative<augur::PlayerSaid>(ev.kind) || std::holds_alternative<augur::NpcSaid>(ev.kind) ||
        std::holds_alternative<augur::ConversationEnded>(ev.kind)) {
      continue;
    }
    Op op;
    op.event = stored(ev, cast, known);
    add(std::move(op));
  }
  // MEM §4.1's prefetch: a persona that came near has its bundle asked for before the talk key.
  for (const augur::Event& ev : events) {
    if (const auto* a = std::get_if<augur::PersonaApproached>(&ev.kind);
        a != nullptr && a->persona.index < cast.size()) {
      want_bundle(cast.persona(a->persona.index).id);
    }
  }
}

void StorePath::commit(const Commit& c, const augur::Event* ev, const Directory& cast, const KnowledgeView* known,
                       proto::Pt pt) {
  if (mode_ == Mode::Off || mode_ == Mode::Down) return;
  Op op;
  op.conv = c.conv;
  op.reply = c.reply;
  op.speech = true;
  switch (c.kind) {
    case Commit::Kind::Started: {
      const auto idx = cast.find(c.persona);
      if (!idx) return;
      const Persona& p = cast.persona(*idx);
      op.kind = Op::Kind::ConvOpen;
      op.row.journey = journey_;
      op.row.persona = c.persona;
      op.row.stage = stage_now(cast, known, static_cast<std::uint32_t>(*idx));
      op.row.started_pt = pt;
      op.row.persona_hash = p.hash.empty() ? std::string("-") : p.hash;
      op.row.kb_hash = std::to_string(cast.pack().hash);
      add(std::move(op));
      if (bundle(c.persona) == nullptr) want_bundle(c.persona);
      return;
    }
    case Commit::Kind::NpcPage:
      op.kind = Op::Kind::Page;
      op.text = c.text;
      op.pt = pt;
      add(std::move(op));
      return;
    case Commit::Kind::CutOff:
      return;
    case Commit::Kind::PlayerSaid:
    case Commit::Kind::NpcSaid:
    case Commit::Kind::Ended:
      break;
  }
  if (ev == nullptr) return;
  op.kind = Op::Kind::Event;
  op.event = stored(*ev, cast, known);
  op.event.text = c.kind == Commit::Kind::Ended ? std::nullopt : std::optional<std::string>(c.text);
  if (c.kind == Commit::Kind::NpcSaid) {
    op.event.t_play_end_ms = op.event.t_play_ms;
    json p = json::parse(op.event.payload, nullptr, false);
    if (p.is_object()) {
      // The first page ends where its text does, as each later page's end is its text's (a trim between the two keeps
      // it whole: MEM §9.6's T1, which the core-level oracle found at 0).
      if (p["pages"].is_array() && !p["pages"].empty()) p["pages"][0]["end"] = sql_length(c.text);
      if (!c.cue.empty()) p["cue"] = c.cue;
      op.event.payload = p.dump();
    }
  }
  if (c.kind == Commit::Kind::Ended) {
    const auto idx = cast.find(c.persona);
    json p = json::object();
    p["reason"] = to_string(c.reason);  // the core's own word; the augur event carries Augur's five
    p["player_lines"] = c.player_lines;
    p["committed"] = c.committed;
    p["known"] = c.known ? json::parse(*c.known, nullptr, false) : json(nullptr);
    if (p["known"].is_discarded()) p["known"] = nullptr;
    p["stage"] = idx ? stage_now(cast, known, static_cast<std::uint32_t>(*idx)) : std::string();
    op.event.payload = p.dump();
  }
  const proto::Pt at = op.event.t_play_ms;
  const bool queues = c.kind == Commit::Kind::Ended && c.player_lines >= 2 && c.committed >= 4;
  if (queues && c.known) {  // the news it brought, against world_seen before this end's fold
    const json k = json::parse(*c.known, nullptr, false);
    const auto f = facts_.find(c.persona);
    auto& news = news_[c.persona];
    for (const json& pair : k.is_array() ? k : json::array()) {
      if (!pair.is_array() || pair.size() != 2 || !pair[0].is_string() || !pair[1].is_string()) continue;
      const std::string item = pair[0].get<std::string>();
      bool seen = false;
      if (f != facts_.end()) {
        for (const auto& [i, w] : fact_pairs(f->second, fact::kWorldSeen)) seen = seen || i == item;
      }
      const bool listed = std::any_of(news.begin(), news.end(), [&](const auto& n) { return n.first == item; });
      if (!seen && !listed && news.size() < 3) news.emplace_back(item, pair[1].get<std::string>());
    }
  }
  add(std::move(op));
  if (c.kind == Commit::Kind::Ended) {
    Op close;
    close.kind = Op::Kind::ConvClose;
    close.conv = c.conv;
    close.row.ended_pt = at;
    close.row.end_reason = to_string(c.reason);
    close.row.player_lines = c.player_lines;
    close.row.committed_lines = c.committed;
    close.row.shown_context = c.shown_context.empty() ? std::string("[]") : c.shown_context;
    close.speech = true;
    add(std::move(close));
    // The next conversation's tail is this one's: the bundle again, after the close (T9 answers in order).
    bundles_.erase(c.persona);
    want_bundle(c.persona);
    // MEM §3.1: a conversation of at least 2 lines of the Tarnished's and 4 committed queues the persona's job.
    if (queues) {
      Op job;
      job.kind = Op::Kind::Job;
      job.conv = c.conv;
      job.persona = c.persona;
      job.pt = at;
      add(std::move(job));
    }
  }
}

void StorePath::pump(Mono now, std::vector<proto::DbRequest>& out) {
  now_ = now;
  if (queued_.empty()) return;
  if (!urgent_ && now < last_flush_ + kCoalesceMs) return;
  for (proto::DbRequest& r : queued_) out.push_back(std::move(r));
  queued_.clear();
  urgent_ = false;
  last_flush_ = now;
}

}  // namespace en::core
