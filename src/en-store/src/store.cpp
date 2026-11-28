// The store's synchronous core (store.hpp): one request, one transaction, one result.
#include "en/store/store.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

#include "en/store/migrate.hpp"

namespace en::store {

namespace {

namespace db = proto::db;
namespace fs = std::filesystem;

template <class... F>
struct Overloaded : F... {
  using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

proto::DbResult failed(std::uint64_t seq, Fault f) { return proto::DbResult{seq, db::Failed{f}}; }

// A transaction that fails leaves the file as it was, so the open file's counters in RAM go back with it: else a run
// of appends retried one by one would find the counter ahead and never write it (MEM §8.6: never decreases, always
// persisted).
class Rewind {
 public:
  explicit Rewind(Playthrough& pt)
      : pt_(pt), journey_(pt.journey), epoch_(pt.epoch), event_(pt.next_event_id), conv_(pt.next_conv_id) {}
  ~Rewind() {
    if (kept_) return;
    pt_.journey = journey_;
    pt_.epoch = epoch_;
    pt_.next_event_id = event_;
    pt_.next_conv_id = conv_;
  }
  Rewind(const Rewind&) = delete;
  Rewind& operator=(const Rewind&) = delete;
  void keep() noexcept { kept_ = true; }

 private:
  Playthrough& pt_;
  std::uint16_t journey_;
  std::int64_t epoch_;
  proto::EventId event_;
  proto::RowId conv_;
  bool kept_ = false;
};

// The first epoch of a journey later than `journey`, if the file has one: a save from an earlier journey is cut at its
// start at the latest, so that the trim always takes the file back into the save's journey.
Result<std::optional<proto::Pt>> later_journey_start(const Db& d, std::uint16_t journey) {
  auto s = Stmt::prepare(d, "SELECT min(start_pt) FROM epochs WHERE journey > ?1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey});
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  if (!*r || s->null(0)) return std::optional<proto::Pt>{};
  return std::optional<proto::Pt>{s->i64(0)};
}

}  // namespace

StoreCore::StoreCore(StoreCfg cfg) : cfg_(std::move(cfg)) {}

StoreCore::~StoreCore() { close(); }

std::int64_t StoreCore::wall() const {
  if (cfg_.wall) return cfg_.wall();
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void StoreCore::close() noexcept {
  choice_.reset();
  pt_.reset();  // the file closes with its Db
}

Result<void> StoreCore::init_file(const proto::PlaythroughKey& key, proto::Pt pt_now, std::uint16_t journey,
                                  const std::string& account_pick) {
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  // No SteamID: it names the player's account, is never recorded (steamid.hpp), and the folder's name holds it.
  const std::pair<std::string_view, std::string> rows[] = {
      {"slot", number_text(key.slot)},
      {"fp_version", "1"},
      {"fp8", key.fp8},
      {"name_hash", key.name_hash},
      {"archetype", number_text(key.archetype)},
      {"gift", number_text(key.gift)},
      {"first_seen_pt", number_text(pt_now)},
      {"next_event_id", "1"},
      {"next_conv_id", "1"},
      {"facts_stale", "0"},
      {"created_by", cfg_.build},
      {"last_opened_by", cfg_.build},
  };
  for (const auto& [k, v] : rows) {
    if (auto r = meta_set(pt_->db, k, v); !r) return r;
  }
  if (!account_pick.empty()) {  // MEM §8.2: the reason words alone
    if (auto r = meta_set(pt_->db, "account_pick", account_pick); !r) return r;
  }
  pt_->next_event_id = 1;
  pt_->next_conv_id = 1;
  if (auto r = new_epoch(*pt_, journey, pt_now, "open"); !r) return Unexpected{r.error()};
  return tx->commit();
}

db::Opened StoreCore::do_open(const db::Open& o, std::optional<Fault>& fault) {
  db::Opened opened;
  const SaveFolder folder = save_folder(cfg_.saves, o.key.steamid64);
  auto loc = locate(folder, o.key, wall());
  if (!loc) {
    fault = loc.error();
    return opened;
  }
  opened.restored_backup = loc->restored_backup;
  opened.restored_archive = loc->restored_archive;
  opened.archived_other = loc->archived_other;
  std::optional<std::uint64_t> free;
  if (cfg_.free_bytes) {
    free = cfg_.free_bytes(folder.dir);
  } else {
    std::error_code ec;
    const auto sp = fs::space(folder.dir, ec);
    if (!ec) free = sp.available;
  }
  opened.low_disk = free && *free < kLowDiskBytes;

  bool create = loc->create;
  auto file = Db::open(loc->path, FileKind::Playthrough, create, cfg_.busy_timeout_ms);
  if (!file && !create && (file.error() == Fault::NotADb || file.error() == Fault::Corrupt)) {
    // MEM §8.6: a corrupt file is set aside, and the newest backup takes its place, or a new file does.
    (void)set_aside(loc->path, wall());
    opened.restored_backup = restore_newest(folder, o.key.slot, o.key.fp8);
    create = !opened.restored_backup;
    file = Db::open(loc->path, FileKind::Playthrough, create, cfg_.busy_timeout_ms);
  }
  if (!file) {
    fault = file.error();
    return opened;
  }
  Playthrough pt;
  pt.db = std::move(*file);
  pt.folder = folder;
  pt.key = o.key;
  pt.wall = wall();
  const auto set = playthrough_migrations();
  auto cur = user_version(pt.db);
  if (!cur) {
    fault = cur.error();
    return opened;
  }
  // A file newer than this build may read is refused before anything touches it, its backups included (MEM §10).
  if (*cur > top_of(set)) {
    auto readable = migrate(pt.db, set, {});
    if (!readable) {
      fault = readable.error();
      return opened;
    }
  }
  // The start backup (MEM §8.1), before anything changes the file.
  if (!create) (void)rotate_backup(pt.db, folder, o.key.slot, o.key.fp8);
  const fs::path pre = !create && *cur > 0 && *cur < top_of(set)
                           ? pre_migration_backup(folder, o.key.slot, o.key.fp8, *cur, top_of(set))
                           : fs::path();
  auto m = migrate(pt.db, set, pre);
  if (!m) {
    fault = m.error();  // a newer file this build may not read (TooNew), or a failed migration: memory off
    return opened;
  }
  opened.migrated = m->from != 0 && m->to != m->from;
  pt_ = std::move(pt);
  if (create || m->from == 0) {
    if (auto r = init_file(o.key, o.pt_now, o.journey, o.account_pick); !r) {
      fault = r.error();
      close();
      return opened;
    }
    opened.created = true;
  } else {
    auto tx = Transaction::immediate(pt_->db);
    Result<void> r = tx ? meta_set(pt_->db, "last_opened_by", cfg_.build) : Result<void>(Unexpected{tx.error()});
    if (r) r = exec(pt_->db, "DELETE FROM meta WHERE k = 'steamid64'");  // written by slices 1 to 4's builds
    if (r && !o.account_pick.empty()) r = meta_set(pt_->db, "account_pick", o.account_pick);  // MEM §8.2
    if (r && loc->renamed) r = meta_set(pt_->db, "fp8", o.key.fp8);
    if (r && loc->renamed) r = meta_set(pt_->db, "gift", number_text(o.key.gift));
    if (r) r = tx->commit();
    if (r) r = load_meta(*pt_);
    if (!r) {
      fault = r.error();
      close();
      return opened;
    }
  }
  // A journey the file has not seen yet: its epoch (MEM §9.4; a JourneyChanged while EldenNet was not running).
  if (o.journey > pt_->journey) {
    auto tx = Transaction::immediate(pt_->db);
    Result<void> r = tx ? Result<void>{} : Result<void>(Unexpected{tx.error()});
    if (r) {
      auto e = new_epoch(*pt_, o.journey, o.pt_now, "journey");
      if (!e) r = Unexpected{e.error()};
    }
    if (r) r = tx->commit();
    if (!r) {
      fault = r.error();
      close();
      return opened;
    }
  }
  // MEM §9.1, as O144 narrowed it.
  auto h = newest_pt(*pt_);
  if (!h) {
    fault = h.error();
    close();
    return opened;
  }
  const proto::Pt delta = *h - o.pt_now;
  const bool lower = o.journey < pt_->journey;
  if (!lower && delta <= 0) {
    opened.rollback = db::Rollback{db::Rollback::Kind::Clean, delta, 0, false, 0, {}};
  } else {
    proto::Pt cut = o.pt_now;
    if (lower) {
      auto start = later_journey_start(pt_->db, o.journey);
      if (!start) {
        fault = start.error();
        close();
        return opened;
      }
      if (*start) cut = std::min(cut, **start - 1);
    }
    auto plan = plan_cut(pt_->db, cut);
    if (!plan) {
      fault = plan.error();
      close();
      return opened;
    }
    auto summary = summarise(*pt_, *plan);
    if (!summary) {
      fault = summary.error();
      close();
      return opened;
    }
    summary->save_pt = o.pt_now;
    summary->file_pt = *h;
    summary->save_journey = o.journey;
    db::Rollback rb{db::Rollback::Kind::Choice, delta, cut, lower, 0, std::move(*summary)};
    if ((lower || delta > kSilentTrimMs) && rb.summary.matters()) {
      // The choice: nothing is trimmed, and nothing recovered, until the player answers.
      choice_ = Choice{cut, o.pt_now, o.journey, rb, o.account_pick};
      opened.rollback = rb;
      opened.next_event_id = pt_->next_event_id;
      opened.next_conv_id = pt_->next_conv_id;
      opened.journey = pt_->journey;
      opened.epoch = pt_->epoch;
      return opened;
    }
    // A silent trim: a crash after the last autosave, or a gap that holds nothing that matters.
    auto tx = Transaction::immediate(pt_->db);
    Result<std::uint64_t> rows = tx ? trim(*pt_, *plan) : Result<std::uint64_t>(Unexpected{tx.error()});
    Result<void> done = rows ? tx->commit() : Result<void>(Unexpected{rows.error()});
    if (!done) {
      fault = done.error();
      close();
      return opened;
    }
    rb.kind = db::Rollback::Kind::Trimmed;
    rb.rows = *rows;
    opened.rollback = std::move(rb);
  }
  auto finished = finish_open(std::move(opened));
  if (!finished) {
    fault = finished.error();
    close();
    return db::Opened{};
  }
  return std::move(*finished);
}

Result<db::Opened> StoreCore::finish_open(db::Opened opened) {
  // MEM §3.8, once the timeline is resolved.
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  auto closed = recover(*pt_);
  if (!closed) return Unexpected{closed.error()};
  if (auto r = tx->commit(); !r) return Unexpected{r.error()};
  opened.closed_conversations = std::move(*closed);
  auto jobs = open_jobs(pt_->db);
  if (!jobs) return Unexpected{jobs.error()};
  opened.open_jobs = std::move(*jobs);
  auto stale = meta_get(pt_->db, "facts_stale");
  if (!stale) return Unexpected{stale.error()};
  opened.facts_stale = stale->value_or("0") == "1";
  opened.next_event_id = pt_->next_event_id;
  opened.next_conv_id = pt_->next_conv_id;
  opened.journey = pt_->journey;
  opened.epoch = pt_->epoch;
  return opened;
}

db::Opened StoreCore::do_resolve(const db::Resolve& r, std::optional<Fault>& fault) {
  const Choice choice = *choice_;
  choice_.reset();
  db::Opened opened;
  switch (r.choice) {
    case db::Choice::Forget: {
      auto plan = plan_cut(pt_->db, choice.cut);
      auto tx = Transaction::immediate(pt_->db);
      Result<std::uint64_t> rows = !plan ? Result<std::uint64_t>(Unexpected{plan.error()})
                                   : !tx ? Result<std::uint64_t>(Unexpected{tx.error()})
                                         : trim(*pt_, *plan);
      Result<void> done = rows ? tx->commit() : Result<void>(Unexpected{rows.error()});
      if (!done) {
        fault = done.error();
        close();
        return opened;
      }
      opened.rollback = choice.rollback;
      opened.rollback.kind = db::Rollback::Kind::Trimmed;
      opened.rollback.rows = *rows;
      break;
    }
    case db::Choice::Keep: {
      // MEM §9.3: every row kept, and a new epoch from the save's play time.
      auto tx = Transaction::immediate(pt_->db);
      Result<void> done = tx ? Result<void>{} : Result<void>(Unexpected{tx.error()});
      if (done) {
        auto e = new_epoch(*pt_, choice.journey, choice.pt_now, "keep");
        if (!e) done = Unexpected{e.error()};
      }
      if (done) done = tx->commit();
      if (!done) {
        fault = done.error();
        close();
        return opened;
      }
      opened.rollback = db::Rollback{db::Rollback::Kind::Clean, choice.rollback.delta_ms, 0, false, 0, {}};
      break;
    }
    case db::Choice::NewCharacter: {
      // The old file goes to the archive, never deleted, and a new one takes its place (MEM §8.3).
      const SaveFolder folder = pt_->folder;
      const proto::PlaythroughKey key = pt_->key;
      const fs::path live = folder.live(key.slot, key.fp8);
      pt_.reset();
      if (!archive_file(folder, live, wall())) {
        fault = Fault::IoErr;
        return opened;
      }
      auto file = Db::open(live, FileKind::Playthrough, true, cfg_.busy_timeout_ms);
      if (!file) {
        fault = file.error();
        return opened;
      }
      Playthrough pt;
      pt.db = std::move(*file);
      pt.folder = folder;
      pt.key = key;
      pt.wall = wall();
      if (auto m = migrate(pt.db, playthrough_migrations(), {}); !m) {
        fault = m.error();
        return opened;
      }
      pt_ = std::move(pt);
      if (auto i = init_file(key, choice.pt_now, choice.journey, choice.account_pick); !i) {
        fault = i.error();
        close();
        return opened;
      }
      opened.created = true;
      opened.archived_other = true;
      opened.rollback = db::Rollback{db::Rollback::Kind::Clean, 0, 0, false, 0, {}};
      break;
    }
  }
  auto finished = finish_open(std::move(opened));
  if (!finished) {
    fault = finished.error();
    close();
    return db::Opened{};
  }
  return std::move(*finished);
}

Result<std::uint64_t> StoreCore::forget(const db::Forget& f) {
  // MEM §9.5: a backup first (the undo), then one transaction, then the WAL emptied so the rows do not linger in it.
  if (auto b = rotate_backup(pt_->db, pt_->folder, pt_->key.slot, pt_->key.fp8); !b) return Unexpected{b.error()};
  Rewind rewind(*pt_);
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  auto rows = store::forget(*pt_, f.journey, f.persona, f.at_pt);
  if (!rows) return Unexpected{rows.error()};
  if (auto r = tx->commit(); !r) return Unexpected{r.error()};
  rewind.keep();
  (void)checkpoint(pt_->db, true);
  return *rows;
}

Result<void> StoreCore::append_all(const std::vector<db::Append>& appends) {
  if (!pt_ || choice_) return Unexpected{Fault::NotOpen};
  pt_->wall = wall();
  Rewind rewind(*pt_);
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  for (const db::Append& a : appends) {
    if (auto r = append(*pt_, a); !r) return r;
  }
  if (auto c = tx->commit(); !c) return c;
  rewind.keep();
  return {};
}

bool Breaker::counts(Fault f) noexcept {
  return f != Fault::Busy && f != Fault::Invalid && f != Fault::NotOpen && f != Fault::TooNew;
}

bool Breaker::note(Fault f, std::uint64_t mono_ms) {
  if (cause_) return true;
  if (!counts(f)) return false;
  if (f == Fault::Corrupt || f == Fault::NotADb || f == Fault::IoErr) {
    cause_ = f;
    return true;
  }
  std::erase_if(recent_, [&](std::uint64_t t) { return mono_ms >= t + kWindowMs || t > mono_ms; });
  recent_.push_back(mono_ms);
  if (recent_.size() >= kFailures) cause_ = f;
  return cause_.has_value();
}

namespace {

std::string key_id(const proto::PlaythroughKey& k) {
  return std::to_string(k.steamid64) + "/" + number_text(k.slot) + "/" + k.fp8;
}

}  // namespace

std::uint64_t StoreCore::mono() const {
  if (cfg_.mono_ms) return cfg_.mono_ms();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

std::optional<Fault> StoreCore::tripped_for(const proto::PlaythroughKey& key) const {
  const std::string id = key_id(key);
  for (const auto& [k, f] : tripped_) {
    if (k == id) return f;
  }
  return std::nullopt;
}

void StoreCore::trip(Fault cause) {
  if (!pt_) return;
  tripped_.emplace_back(key_id(pt_->key), cause);
  const fs::path live = pt_->folder.live(pt_->key.slot, pt_->key.fp8);
  close();
  // A corrupt file is set aside, so the next session restores the newest backup (MEM §8.6).
  if (cause == Fault::Corrupt || cause == Fault::NotADb) (void)set_aside(live, wall());
  breaker_ = Breaker{};
}

proto::DbResult StoreCore::handle(const proto::DbRequest& req) {
  // An Open of a file tripped this session answers its trip; everything else runs, and a failure is counted.
  if (const auto* o = std::get_if<db::Open>(&req.op)) {
    if (const auto f = tripped_for(o->key)) {
      close();
      return proto::DbResult{req.seq, db::Tripped{*f}};
    }
  }
  proto::DbResult out = run(req);
  if (const auto* f = std::get_if<db::Failed>(&out.result); f != nullptr && pt_ && breaker_.note(f->fault, mono())) {
    const Fault cause = breaker_.cause().value_or(f->fault);
    trip(cause);
    out.result = db::Tripped{cause};
  }
  return out;
}

proto::DbResult StoreCore::run(const proto::DbRequest& req) {
  const std::uint64_t seq = req.seq;
  // A write: one transaction, Done or its fault.
  const auto write = [&](auto&& op) -> proto::DbResult {
    if (!pt_ || choice_) return failed(seq, Fault::NotOpen);
    pt_->wall = wall();
    Rewind rewind(*pt_);
    auto tx = Transaction::immediate(pt_->db);
    if (!tx) return failed(seq, tx.error());
    auto r = op();
    if (!r) return failed(seq, r.error());
    if (auto c = tx->commit(); !c) return failed(seq, c.error());
    rewind.keep();
    return proto::DbResult{seq, db::Done{}};
  };
  const auto query = [&](auto&& op) -> proto::DbResult {
    if (!pt_ || choice_) return failed(seq, Fault::NotOpen);
    auto r = op();
    if (!r) return failed(seq, r.error());
    return proto::DbResult{seq, std::move(*r)};
  };
  return std::visit(Overloaded{
                        [&](const db::Open& o) -> proto::DbResult {
                          close();
                          std::optional<Fault> fault;
                          db::Opened opened = do_open(o, fault);
                          if (fault) return failed(seq, *fault);
                          return proto::DbResult{seq, std::move(opened)};
                        },
                        [&](const db::Resolve& r) -> proto::DbResult {
                          if (!pt_ || !choice_) return failed(seq, Fault::Invalid);
                          std::optional<Fault> fault;
                          db::Opened opened = do_resolve(r, fault);
                          if (fault) return failed(seq, *fault);
                          return proto::DbResult{seq, db::Resolved{std::move(opened)}};
                        },
                        [&](const db::Close&) -> proto::DbResult {
                          close();
                          return proto::DbResult{seq, db::Done{}};
                        },
                        [&](const db::Append& a) { return write([&] { return append(*pt_, a); }); },
                        [&](const db::AppendPage& p) { return write([&] { return append_page(*pt_, p); }); },
                        [&](const db::ConvOpen& c) { return write([&] { return conv_open(*pt_, c.row); }); },
                        [&](const db::ConvClose& c) { return write([&] { return conv_close(*pt_, c.row); }); },
                        [&](const db::UpsertJob& j) -> proto::DbResult {
                          if (!pt_ || choice_) return failed(seq, Fault::NotOpen);
                          pt_->wall = wall();
                          auto tx = Transaction::immediate(pt_->db);
                          if (!tx) return failed(seq, tx.error());
                          auto id = upsert_job(*pt_, j.job);
                          if (!id) return failed(seq, id.error());
                          if (auto c = tx->commit(); !c) return failed(seq, c.error());
                          return proto::DbResult{seq, db::JobSaved{*id}};
                        },
                        [&](const db::CommitJournal& c) { return write([&] { return commit_journal(*pt_, c); }); },
                        [&](const db::CommitLived& c) { return write([&] { return commit_lived(*pt_, c); }); },
                        [&](const db::PutFacts& f) { return write([&] { return put_facts(*pt_, f); }); },
                        [&](const db::Forget& f) -> proto::DbResult {
                          if (!pt_ || choice_) return failed(seq, Fault::NotOpen);
                          pt_->wall = wall();
                          auto rows = forget(f);
                          if (!rows) return failed(seq, rows.error());
                          return proto::DbResult{seq, db::Done{}};
                        },
                        [&](const db::Checkpoint&) -> proto::DbResult {
                          // Passive: it never waits for a reader. A failed sync or write is a fault like any other.
                          if (pt_ && !choice_) {
                            if (auto r = checkpoint(pt_->db, false); !r) return failed(seq, r.error());
                          }
                          return proto::DbResult{seq, db::Done{}};
                        },
                        [&](const db::Bundle& q) { return query([&] { return bundle(*pt_, q); }); },
                        [&](const db::JobInput& q) { return query([&] { return job_input(*pt_, q); }); },
                        [&](const db::LivedInput& q) { return query([&] { return lived_input(*pt_, q); }); },
                        [&](const db::EventsForFold& q) { return query([&] { return fold_events(*pt_, q); }); },
                        [&](const db::PageQuery& q) -> proto::DbResult {
                          if (q.what == db::PageQuery::What::Backups) {
                            if (!pt_) return failed(seq, Fault::NotOpen);
                            const BackupInfo b = backups_of(pt_->folder, pt_->key.slot, pt_->key.fp8);
                            db::PageRows rows;
                            rows.what = q.what;
                            rows.backups = static_cast<std::uint32_t>(b.count);
                            rows.backup_walls = b.walls;
                            return proto::DbResult{seq, std::move(rows)};
                          }
                          return query([&] { return page(*pt_, q); });
                        },
                    },
                    req.op);
}

}  // namespace en::store
