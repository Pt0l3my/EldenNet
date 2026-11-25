// The model calls' record (calls.hpp): one row per call, its outcome and the session's usage, from what EldenNet sent,
// what the model client answered and what the core showed; never a prompt or an unshown answer (PV4).
#include "en/core/calls.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "en/core/core.hpp"

namespace en::core {

namespace {

namespace pr = en::proto;

// A purpose whose answer is meant for the screen: a line someone says.
bool for_screen(pr::CallPurpose p) noexcept {
  return p == pr::CallPurpose::Reply || p == pr::CallPurpose::OwnInitiative || p == pr::CallPurpose::CallOut;
}

bool failure(pr::CallOutcome o) noexcept {
  return o == pr::CallOutcome::TimedOut || o == pr::CallOutcome::Refused || o == pr::CallOutcome::Failed;
}

// How a request's end reads when nothing of it reached the screen.
pr::CallOutcome ended_as(pr::ErrorClass e, pr::CallPurpose purpose) noexcept {
  switch (e) {
    case pr::ErrorClass::None: return for_screen(purpose) ? pr::CallOutcome::NotShown : pr::CallOutcome::Answered;
    case pr::ErrorClass::FirstTokenTimeout:
    case pr::ErrorClass::TotalTimeout: return pr::CallOutcome::TimedOut;
    case pr::ErrorClass::Refused: return pr::CallOutcome::Refused;
    case pr::ErrorClass::Cancelled:
    case pr::ErrorClass::Preempted: return pr::CallOutcome::NotShown;  // stopped: nothing was wrong with the provider
    default: return pr::CallOutcome::Failed;
  }
}

std::string model_of(const std::string& id) { return pr::shown_model(id) ? id : std::string(); }

// The longest prefix of `text` within `max` bytes that ends at a character's boundary.
std::string_view utf8_cut(std::string_view text, std::size_t max) {
  if (text.size() <= max) return text;
  std::size_t cut = max;
  while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
  return text.substr(0, cut);
}

std::int32_t add_tokens(std::int32_t have, int reported) {
  if (reported < 0) return have;
  const std::int64_t sum = static_cast<std::int64_t>(have < 0 ? 0 : have) + reported;
  return static_cast<std::int32_t>(std::min<std::int64_t>(sum, 0x7FFFFFFF));
}

std::int64_t ms_of(std::chrono::milliseconds m) { return static_cast<std::int64_t>(m.count()); }

}  // namespace

CallRecord::CallRecord(std::size_t kept) { set_kept(kept); }

void CallRecord::set_kept(std::size_t kept) {
  kept_ = std::clamp<std::size_t>(kept, 1, pr::kCallsKeptDeveloper);
  view_.kept = kept_;
  traces_.kept = kept_;
  while (trace_rows_.size() > kept_) trace_rows_.pop_back();
  rebuild();
}

CallRecord::Entry* CallRecord::by_request(pr::RequestId id) {
  for (Entry& e : entries_) {
    if (std::find(e.requests.begin(), e.requests.end(), id) != e.requests.end()) return &e;
  }
  return nullptr;
}

CallRecord::Entry* CallRecord::by_reply(pr::ConvId conv, pr::ReplyId reply) {
  for (Entry& e : entries_) {
    if (e.reply && e.reply->first == conv && e.reply->second == reply) return &e;
  }
  return nullptr;
}

CallRecord::Entry& CallRecord::make(CallStart start, std::uint64_t at_ms) {
  Entry e;
  e.row.id = next_id_++;
  e.row.at_ms = at_ms;
  e.row.slot = std::move(start.slot);
  e.row.purpose = start.purpose;
  e.row.speaker = std::move(start.speaker);
  ++view_.usage.calls;
  if (pr::line_of(e.row.purpose)) ++traces_.summary.calls;
  if (e.row.purpose == pr::CallPurpose::OwnInitiative) {
    own_initiative_.insert(std::upper_bound(own_initiative_.begin(), own_initiative_.end(), at_ms), at_ms);
    view_.usage.own_initiative_hour = static_cast<std::uint32_t>(own_initiative_.size());
  }
  // Newest first: by the time it was sent, then by its id (a row made at its end may have been sent before others).
  const auto at = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& o) {
    return o.row.at_ms < e.row.at_ms || (o.row.at_ms == e.row.at_ms && o.row.id < e.row.id);
  });
  return *entries_.insert(at, std::move(e));  // the oldest past the cap go at the rebuild, once it is settled
}

void CallRecord::sent(pr::RequestId id, CallStart start, std::uint64_t at_ms) {
  if (by_request(id) != nullptr) return;
  Entry& e = make(std::move(start), at_ms);
  e.requests.push_back(id);
  settle(e);
}

void CallRecord::note(const CallNote& n, std::uint64_t at_ms) {
  Entry* e = by_reply(n.conv, n.reply);
  if (n.what == CallNote::What::Withheld) {
    if (e != nullptr && !e->withheld) {
      e->withheld = true;
      settle(*e);
    }
    return;
  }
  if (by_request(n.request) != nullptr) return;
  if (e == nullptr) {
    e = &make(CallStart{n.slot, pr::CallPurpose::Reply, n.speaker}, at_ms);
    e->reply = std::make_pair(n.conv, n.reply);
  }
  e->requests.push_back(n.request);  // a regeneration's: its answer is the one that counts now
  e->ended = false;
  e->end = pr::ErrorClass::None;
  settle(*e);
}

void CallRecord::shown(pr::ConvId conv, pr::ReplyId reply, std::string_view text, std::uint64_t at_ms) {
  Entry* e = by_reply(conv, reply);
  if (e == nullptr) return;
  // Its first page shown, the traces': whatever the page said, so the persona's own line in place of an answer a
  // check withheld counts too (the Tarnished waited for it alike).
  const bool first = e->row.first_page_ms < 0;
  if (first) e->row.first_page_ms = static_cast<std::int64_t>(at_ms >= e->row.at_ms ? at_ms - e->row.at_ms : 0);
  if (e->withheld) {
    if (first) settle(*e);
    return;
  }
  e->shown = true;
  std::string& kept = e->row.text;
  if (!text.empty() && kept.size() + 1 < pr::kCallTextMaxBytes) {  // a page joins the last with a space, as shown
    if (!kept.empty()) kept.push_back(' ');
    kept.append(utf8_cut(text, pr::kCallTextMaxBytes - kept.size()));
  }
  settle(*e);
}

void CallRecord::not_sent(pr::RequestId id, pr::ErrorClass why, std::uint64_t at_ms) {
  Entry* e = by_request(id);
  if (e == nullptr || e->requests.back() != id || e->ended) return;
  e->ended = true;
  e->end = why;
  e->row.error = why;
  e->row.http_status = 0;
  e->row.total_ms = static_cast<std::int64_t>(at_ms >= e->row.at_ms ? at_ms - e->row.at_ms : 0);
  settle(*e);
}

void CallRecord::withdraw(pr::RequestId id) {
  const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
    return std::find(e.requests.begin(), e.requests.end(), id) != e.requests.end();
  });
  if (it == entries_.end()) return;
  if (it->requests.size() > 1) {
    std::erase(it->requests, id);
    it->ended = true;  // the earlier request's end stands: it was stopped for this one
    it->end = pr::ErrorClass::Cancelled;
    settle(*it);
    return;
  }
  // No call: what it added goes, and so does its row (its trace's too).
  untrace(*it);
  pr::CallUsage& u = view_.usage;
  u.calls -= u.calls > 0 ? 1 : 0;
  u.tokens_in -= std::min(u.tokens_in, it->tokens_in);
  u.tokens_out -= std::min(u.tokens_out, it->tokens_out);
  if (it->costed) {
    u.cost -= it->cost;
    u.cost_calls -= u.cost_calls > 0 ? 1 : 0;
  }
  if (it->failure) u.failures -= u.failures > 0 ? 1 : 0;
  if (it->row.purpose == pr::CallPurpose::OwnInitiative) {
    if (const auto t = std::find(own_initiative_.begin(), own_initiative_.end(), it->row.at_ms);
        t != own_initiative_.end()) {
      own_initiative_.erase(t);
    }
    u.own_initiative_hour = static_cast<std::uint32_t>(own_initiative_.size());
  }
  entries_.erase(it);
  rebuild();
}

void CallRecord::event(const pr::LlmEvent& ev, std::uint64_t at_ms, const CallStart* start) {
  switch (ev.kind) {
    case pr::EventKind::Started: {
      Entry* e = by_request(ev.id);
      if (e == nullptr) {
        if (start == nullptr) return;
        e = &make(*start, at_ms);
        e->requests.push_back(ev.id);
      }
      if (e->requests.back() != ev.id || e->ended) return;
      e->row.provider = ev.provider;
      e->row.model = model_of(ev.model);
      settle(*e);
      return;
    }
    case pr::EventKind::Ended: {
      if (!ev.chat) return;  // a decision call's: v0.2 sends none
      Entry* e = by_request(ev.id);
      if (e == nullptr) {
        if (start == nullptr) return;
        const std::uint64_t took = static_cast<std::uint64_t>(std::max<std::int64_t>(ms_of(ev.chat->total), 0));
        e = &make(*start, at_ms >= took ? at_ms - took : 0);
        e->requests.push_back(ev.id);
      }
      end_chat(*e, ev.id, *ev.chat, at_ms);
      return;
    }
    case pr::EventKind::ModelsEnded: {
      if (start == nullptr) return;  // a list is made a row at its end, and only by the run that asked for it
      for (const Entry& o : entries_) {
        if (o.list && *o.list == ev.id) return;
      }
      const std::uint64_t took = static_cast<std::uint64_t>(std::max<std::int64_t>(ms_of(ev.models.time), 0));
      Entry& e = make(*start, at_ms >= took ? at_ms - took : 0);
      e.list = ev.id;
      e.row.provider = ev.provider;
      end_list(e, ev.models);
      return;
    }
    default:
      return;  // Delta and Reset carry text, which no row keeps; a warm-up is no call of the record's
  }
}

void CallRecord::end_chat(Entry& e, pr::RequestId id, const pr::ChatResult& r, std::uint64_t at_ms) {
  // The queue, the traces': from the row's send to its first request's first attempt, which left that long after the
  // client began it (the client began it its total before this end). A request that never left has none.
  if (e.row.queue_ms < 0 && !e.requests.empty() && e.requests.front() == id && !r.attempts.empty()) {
    const std::int64_t since = static_cast<std::int64_t>(at_ms >= e.row.at_ms ? at_ms - e.row.at_ms : 0);
    const std::int64_t began = std::max<std::int64_t>(since - std::max<std::int64_t>(ms_of(r.total), 0), 0);
    e.row.queue_ms = began + std::max<std::int64_t>(ms_of(r.attempts.front().sent_at), 0);
  }
  // Every request of the row adds its attempts, tokens and cost; only the last one's answer counts for the rest.
  e.row.attempts += static_cast<std::uint32_t>(r.attempts.size());
  e.row.tokens_in = add_tokens(e.row.tokens_in, r.usage.prompt_tokens);
  e.row.tokens_out = add_tokens(e.row.tokens_out, r.usage.completion_tokens);
  if (r.usage.cost >= 0) e.row.cost = (e.row.cost < 0 ? 0 : e.row.cost) + r.usage.cost;
  if (e.requests.back() != id || e.ended) {
    settle(e);
    return;
  }
  e.ended = true;
  e.end = r.error;
  if (!r.provider.empty()) e.row.provider = r.provider;
  if (!r.model.empty()) e.row.model = model_of(r.model);
  e.row.fallback = r.ok() && !r.attempts.empty() &&
                   (r.attempts.front().provider != r.provider || r.attempts.front().model != r.model);
  e.row.error = r.error;
  e.row.http_status = r.error != pr::ErrorClass::None && !r.attempts.empty() ? r.attempts.back().http_status : 0;
  if (e.row.fallback) {
    // Why the first model did not answer: the first attempt that failed (one that lost a race did not).
    for (const pr::AttemptInfo& a : r.attempts) {
      if (a.error != pr::ErrorClass::None && !a.lost_race) {
        e.row.error = a.error;
        e.row.http_status = a.http_status;
        break;
      }
    }
  }
  // The times from when the row was sent: the client's own start is its end less its total.
  const std::int64_t total = std::max<std::int64_t>(ms_of(r.total), 0);
  const std::int64_t since = static_cast<std::int64_t>(at_ms >= e.row.at_ms ? at_ms - e.row.at_ms : 0);
  const std::int64_t offset = std::max<std::int64_t>(since - total, 0);
  e.row.total_ms = std::max(since, total);
  e.row.first_token_ms = ms_of(r.first_token) >= 0 ? offset + ms_of(r.first_token) : -1;
  settle(e);
}

void CallRecord::end_list(Entry& e, const pr::ModelsOutcome& m) {
  e.ended = true;
  e.end = m.error;
  e.row.error = m.error;
  e.row.http_status = m.error != pr::ErrorClass::None ? m.status : 0;
  // A list the gate or a missing transport stopped at once never left.
  const bool left = m.error != pr::ErrorClass::GateClosed && m.error != pr::ErrorClass::NoHealthyProvider;
  e.row.attempts = left ? 1 : 0;
  e.row.total_ms = std::max<std::int64_t>(ms_of(m.time), 0);
  settle(e);
}

void CallRecord::settle(Entry& e) {
  proto::CallRow& row = e.row;
  if (e.shown) {
    row.outcome = e.ended && row.fallback ? pr::CallOutcome::FallbackModel : pr::CallOutcome::Shown;
  } else if (!e.ended) {
    row.outcome = pr::CallOutcome::Waiting;
  } else if (e.withheld) {
    row.outcome = pr::CallOutcome::NotShown;  // withheld by a check: the persona's own line showed instead
  } else {
    row.outcome = ended_as(e.end, row.purpose);
  }
  // The usage moves by what the entry adds now against what it added before.
  pr::CallUsage& u = view_.usage;
  const std::uint64_t in = row.tokens_in < 0 ? 0 : static_cast<std::uint64_t>(row.tokens_in);
  const std::uint64_t out = row.tokens_out < 0 ? 0 : static_cast<std::uint64_t>(row.tokens_out);
  u.tokens_in = u.tokens_in - std::min(u.tokens_in, e.tokens_in) + in;
  u.tokens_out = u.tokens_out - std::min(u.tokens_out, e.tokens_out) + out;
  e.tokens_in = in;
  e.tokens_out = out;
  if (e.costed) {
    u.cost -= e.cost;
    u.cost_calls -= u.cost_calls > 0 ? 1 : 0;
  }
  e.costed = row.cost >= 0;
  e.cost = e.costed ? row.cost : 0;
  if (e.costed) {
    u.cost += e.cost;
    ++u.cost_calls;
  }
  const bool failed = failure(row.outcome);
  if (failed != e.failure) {
    if (failed) {
      ++u.failures;
    } else if (u.failures > 0) {
      --u.failures;
    }
    e.failure = failed;
  }
  trace(e);
  rebuild();
}

void CallRecord::sample(std::deque<std::uint32_t>& samples, std::int64_t ms) {
  samples.push_back(static_cast<std::uint32_t>(std::clamp<std::int64_t>(ms, 0, 0xFFFF'FFFF)));
  while (samples.size() > pr::kTraceSamples) samples.pop_front();
}

void CallRecord::trace(Entry& e) {
  const std::optional<pr::LineKind> kind = pr::line_of(e.row.purpose);
  if (!kind) return;
  // The summary moves by what the entry adds now against what it added before; a first page and a first token are
  // samples once each (a row's first page is kept once it showed, its first token once its last request ended).
  pr::TraceSummary& s = traces_.summary;
  const bool page = e.row.first_page_ms >= 0;
  if (page && !e.line_shown) {
    e.line_shown = true;
    ++s.lines;
    sample(pages_, e.row.first_page_ms);
    const std::vector<std::uint32_t> all(pages_.begin(), pages_.end());
    s.first_page_median_ms = pr::percentile(all, 500);
    s.first_page_p90_ms = all.size() >= pr::kTraceP90Lines ? pr::percentile(all, 900) : -1;
  }
  const bool unshown = e.ended && !page;
  if (unshown != e.line_unshown) {
    if (unshown) {
      ++s.unshown;
    } else if (s.unshown > 0) {
      --s.unshown;
    }
    e.line_unshown = unshown;
  }
  if (!e.token_sampled && e.ended && e.row.first_token_ms >= 0) {
    e.token_sampled = true;
    sample(tokens_, e.row.first_token_ms);
    s.first_token_median_ms = pr::percentile(std::vector<std::uint32_t>(tokens_.begin(), tokens_.end()), 500);
  }
  // Its row, newest first by the time it was sent, then by its id; the oldest past the view go.
  pr::TraceRow row;
  row.id = e.row.id;
  row.at_ms = e.row.at_ms;
  row.kind = *kind;
  row.slot = e.row.slot;
  row.queue_ms = e.row.queue_ms;
  row.first_token_ms = e.row.first_token_ms;
  row.first_page_ms = e.row.first_page_ms;
  row.total_ms = e.row.total_ms;
  row.verdict = pr::verdict_of(*kind, e.row.first_page_ms);
  const auto same = std::find_if(trace_rows_.begin(), trace_rows_.end(),
                                 [&](const pr::TraceRow& o) { return o.id == row.id; });
  if (same != trace_rows_.end()) {
    *same = std::move(row);
    return;
  }
  const auto at = std::find_if(trace_rows_.begin(), trace_rows_.end(), [&](const pr::TraceRow& o) {
    return o.at_ms < row.at_ms || (o.at_ms == row.at_ms && o.id < row.id);
  });
  trace_rows_.insert(at, std::move(row));
  while (trace_rows_.size() > kept_) trace_rows_.pop_back();
}

void CallRecord::untrace(const Entry& e) {
  if (!pr::line_of(e.row.purpose)) return;
  pr::TraceSummary& s = traces_.summary;
  s.calls -= s.calls > 0 ? 1 : 0;
  if (e.line_shown) s.lines -= s.lines > 0 ? 1 : 0;  // never, in fact: a call refused before it left showed nothing
  if (e.line_unshown) s.unshown -= s.unshown > 0 ? 1 : 0;
  std::erase_if(trace_rows_, [&](const pr::TraceRow& r) { return r.id == e.row.id; });
}

void CallRecord::rebuild() {
  // The oldest entries past the view and 64 more go: what they added to the usage stays counted, and an event of
  // theirs that comes later is left alone.
  while (entries_.size() > kept_ + 64) entries_.pop_back();
  view_.rows.clear();
  for (const Entry& e : entries_) {
    if (view_.rows.size() >= kept_) break;
    view_.rows.push_back(e.row);
  }
  ++view_.revision;
  traces_.rows.assign(trace_rows_.begin(), trace_rows_.end());
  ++traces_.revision;
}

void CallRecord::prune_hour(std::uint64_t at_ms) {
  while (!own_initiative_.empty() && at_ms >= pr::kCallHourMs && own_initiative_.front() <= at_ms - pr::kCallHourMs) {
    own_initiative_.pop_front();
  }
}

void CallRecord::hour(std::uint32_t requests, std::uint32_t bound, std::uint64_t at_ms) {
  prune_hour(at_ms);
  pr::CallUsage& u = view_.usage;
  const auto own = static_cast<std::uint32_t>(own_initiative_.size());
  if (u.requests_hour == requests && u.bound == bound && u.own_initiative_hour == own) return;
  u.requests_hour = requests;
  u.bound = bound;
  u.own_initiative_hour = own;
  ++view_.revision;
}

void record_step(CallRecord& calls, const Outputs& out, std::uint64_t at_ms) {
  for (const CallNote& n : out.calls) calls.note(n, at_ms);
  for (const Commit& c : out.commits) {
    if (c.kind == Commit::Kind::NpcSaid || c.kind == Commit::Kind::NpcPage) {
      calls.shown(c.conv, c.reply, c.text, at_ms);
    }
  }
}

}  // namespace en::core
