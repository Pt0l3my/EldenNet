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

Result<void> StoreCore::init_file(const proto::PlaythroughKey& key, proto::Pt pt_now, std::uint16_t journey) {
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  const std::pair<std::string_view, std::string> rows[] = {
      {"steamid64", std::to_string(key.steamid64)},
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
  auto file = Db::open(loc->path, FileKind::Playthrough, create);
  if (!file && !create && (file.error() == Fault::NotADb || file.error() == Fault::Corrupt)) {
    // MEM §8.6: a corrupt file is set aside, and the newest backup takes its place, or a new file does.
    (void)set_aside(loc->path, wall());
    opened.restored_backup = restore_newest(folder, o.key.slot, o.key.fp8);
    create = !opened.restored_backup;
    file = Db::open(loc->path, FileKind::Playthrough, create);
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
    if (auto r = init_file(o.key, o.pt_now, o.journey); !r) {
      fault = r.error();
      close();
      return opened;
    }
    opened.created = true;
  } else {
    auto tx = Transaction::immediate(pt_->db);
    Result<void> r = tx ? meta_set(pt_->db, "last_opened_by", cfg_.build) : Result<void>(Unexpected{tx.error()});
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
      choice_ = Choice{cut, o.pt_now, o.journey, rb};
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
      auto file = Db::open(live, FileKind::Playthrough, true);
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
      if (auto i = init_file(key, choice.pt_now, choice.journey); !i) {
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
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  auto rows = store::forget(*pt_, f.journey, f.persona, f.at_pt);
  if (!rows) return Unexpected{rows.error()};
  if (auto r = tx->commit(); !r) return Unexpected{r.error()};
  (void)checkpoint(pt_->db, true);
  return *rows;
}

Result<void> StoreCore::append_all(const std::vector<db::Append>& appends) {
  if (!pt_ || choice_) return Unexpected{Fault::NotOpen};
  pt_->wall = wall();
  auto tx = Transaction::immediate(pt_->db);
  if (!tx) return Unexpected{tx.error()};
  for (const db::Append& a : appends) {
    if (auto r = append(*pt_, a); !r) return r;
  }
  return tx->commit();
}

proto::DbResult StoreCore::handle(const proto::DbRequest& req) {
  const std::uint64_t seq = req.seq;
  // A write: one transaction, Done or its fault.
  const auto write = [&](auto&& op) -> proto::DbResult {
    if (!pt_ || choice_) return failed(seq, Fault::NotOpen);
    pt_->wall = wall();
    auto tx = Transaction::immediate(pt_->db);
    if (!tx) return failed(seq, tx.error());
    auto r = op();
    if (!r) return failed(seq, r.error());
    if (auto c = tx->commit(); !c) return failed(seq, c.error());
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
                          if (pt_) (void)checkpoint(pt_->db, false);
                          return proto::DbResult{seq, db::Done{}};
                        },
                        [&](const db::Bundle& q) { return query([&] { return bundle(*pt_, q); }); },
                        [&](const db::JobInput& q) { return query([&] { return job_input(*pt_, q); }); },
                        [&](const db::LivedInput& q) { return query([&] { return lived_input(*pt_, q); }); },
                        [&](const db::EventsForFold& q) { return query([&] { return fold_events(*pt_, q); }); },
                    },
                    req.op);
}

}  // namespace en::store
