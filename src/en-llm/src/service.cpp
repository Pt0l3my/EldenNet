// The asynchronous handle (ARCH §2.2; §1.3's C9, C10 and C15) and Ollama's warm-up (ARCH §11.3). One mutex guards
// the requests' state and the event queue has its own; the state's is always taken first. Nothing calls out (the
// notify callback, the log) while holding either: the handle's own dispatcher thread does that.
//
// The handle's threads (the workers and the dispatcher, T7–T8) run on 2 MB stacks (ARCH §10.2) and start with
// ffi_guard (R3, thread.hpp); a request whose client call throws trips the worker's component and ends as BadRequest
// (ARCH §1.2), and a caller's callback that throws (the log, the notify) loses its call, never the thread.
#include "en/llm/service.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "en/llm/request.hpp"
#include "en/llm/thread.hpp"
#include "en/proto/ffi_guard.hpp"

namespace en::llm {

using en::proto::Component;
using en::proto::ffi_guard;
using nlohmann::json;

namespace {

constexpr Millis kWarmUpIdle = std::chrono::minutes(4);     // ARCH §11.3
constexpr Millis kWarmUpTimeout = std::chrono::minutes(2);  // impl: a cold load of a large model from a slow disk
constexpr int kMinWorkers = 2;
constexpr int kMaxWorkers = 8;

Millis since(TimePoint from, TimePoint to) { return std::chrono::duration_cast<Millis>(to - from); }

std::string ms_text(Millis ms) { return std::to_string(ms.count()) + " ms"; }

// Ollama's native API is at the server's root, its OpenAI-compatible one under /v1 (ER06 §5.2).
std::string ollama_root(std::string base) {
  while (!base.empty() && base.back() == '/') base.pop_back();
  constexpr std::string_view v1 = "/v1";
  if (base.size() >= v1.size() && base.compare(base.size() - v1.size(), v1.size(), v1) == 0) {
    base.resize(base.size() - v1.size());
  }
  return base;
}

// How long a live request may wait to start: its slot's first-token time, the local one when the slot's model is on
// a local provider.
Millis start_budget(const Config& config, const SlotConfig& slot) {
  const ProviderConfig* provider = slot.models.empty() ? nullptr : config.provider(slot.models.front().provider);
  const bool local = provider != nullptr && provider->kind == ProviderKind::Local;
  return local ? slot.budget.first_token_local : slot.budget.first_token;
}

int pool_size(const Config& config, int asked) {
  if (asked > 0) return asked;
  int total = 0;
  for (const ProviderConfig& p : config.providers) {
    if (p.keys_resolved()) total += p.max_concurrency;
  }
  return std::clamp(total + 1, kMinWorkers, kMaxWorkers);
}

ClientOptions with_clock(ClientOptions options, const std::shared_ptr<Clock>& clock) {
  options.clock = clock;
  return options;
}

// C10. A running request's events wait for room and give up once the request is stopped; an Ended event never waits.
class EventQueue {
 public:
  explicit EventQueue(std::size_t capacity) : capacity_(std::max<std::size_t>(capacity, 1)) {}

  // Returns false, queueing nothing, once `stop` is requested; `woke` says the queue was empty before.
  bool push(LlmEvent event, const std::stop_token& stop, bool& woke) {
    std::unique_lock lock(mutex_);
    not_full_.wait(lock, [&] { return stop.stop_requested() || events_.size() < capacity_; });
    if (stop.stop_requested()) return false;
    woke = put(std::move(event));
    return true;
  }

  // Returns whether the queue was empty before.
  bool push_ending(LlmEvent event) {
    std::lock_guard lock(mutex_);
    return put(std::move(event));
  }

  std::optional<LlmEvent> poll() {
    std::lock_guard lock(mutex_);
    return take();
  }

  std::size_t drain(std::vector<LlmEvent>& out) {
    std::lock_guard lock(mutex_);
    const std::size_t n = events_.size();
    for (LlmEvent& e : events_) out.push_back(std::move(e));
    events_.clear();
    if (n > 0) not_full_.notify_all();
    return n;
  }

  std::optional<LlmEvent> wait(Millis timeout) {
    std::unique_lock lock(mutex_);
    not_empty_.wait_for(lock, timeout, [&] { return !events_.empty() || closed_; });
    return take();
  }

  // Producers whose request was stopped look at their token again. Taking the lock first means that none of them is
  // between its check and its wait, and that none which found its token unstopped is still queueing.
  void wake_producers() {
    { std::lock_guard lock(mutex_); }
    not_full_.notify_all();
  }

  void close() {
    {
      std::lock_guard lock(mutex_);
      closed_ = true;
    }
    not_empty_.notify_all();
  }

  std::size_t size() const {
    std::lock_guard lock(mutex_);
    return events_.size();
  }

 private:
  bool put(LlmEvent event) {
    const bool woke = events_.empty();
    events_.push_back(std::move(event));
    not_empty_.notify_one();
    return woke;
  }

  std::optional<LlmEvent> take() {
    if (events_.empty()) return std::nullopt;
    std::optional<LlmEvent> e(std::move(events_.front()));
    events_.pop_front();
    not_full_.notify_all();  // all: a producer that wakes to find its request stopped leaves the room to another
    return e;
  }

  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
  std::deque<LlmEvent> events_;
  bool closed_ = false;
};

struct Job {
  RequestId id = 0;
  RequestKind kind = RequestKind::Chat;
  ChatRequest chat;
  DecisionCall decision;  // its rule, checked at submit, is the pick whenever the handle ends it without running it
  std::optional<Epoch> epoch;
  std::string slot;  // as resolved
  int priority = 0;
  std::uint64_t seq = 0;        // the order of submission: first come, first served within a priority
  bool dialogue = false;        // a chat on the dialogue slot: one in flight at a time
  bool reply = false;           // a dialogue reply
  bool own_initiative = false;  // own-initiative dialogue, or a decision call with an epoch: it yields to a reply
  bool evictable = false;       // memory or bark_pool: a request that outranks it may take its place in a full queue
  bool live = false;            // bark_live, a decision call, own-initiative dialogue: ends as Busy if it cannot start
  Millis start_budget{0};
  std::optional<TimePoint> start_by;
  TimePoint accepted;
  bool running = false;
  bool starting = false;                   // taken, and neither sent nor answered yet
  std::optional<std::uint64_t> parked_at;  // answered Busy when this many requests had ended
  std::string busy_message;                // the client's word on why
  std::uint64_t ends_seen = 0;             // requests that had ended when a worker took it
  std::stop_source stop;
  std::stop_token token = stop.get_token();
  ErrorClass stop_error = ErrorClass::Cancelled;  // why the handle stopped it while it ran
  std::string stop_message;
  bool started = false;                // its Started event was handled (the running worker's own)
  std::vector<std::string> providers;  // the providers its attempts went to, for the warm-up's rule
};

struct WarmUp {
  RequestId id = 0;
  std::shared_ptr<const Config> config;  // the snapshot its provider is read from
  std::string provider;
  std::string model;
};

}  // namespace

struct Service::Impl {
  Impl(Config config, std::shared_ptr<TransportFactory> factory, ServiceOptions options)
      : transport(factory),
        clock(options.client.clock ? options.client.clock : system_clock()),
        log_fn(options.client.log),
        slice(options.client.poll_slice.count() > 0 ? options.client.poll_slice : Millis(50)),
        capacity(std::max<std::size_t>(options.queue_capacity, 1)),
        worker_count(pool_size(config, options.workers)),
        client(std::move(config), std::move(factory), with_clock(std::move(options.client), clock)),
        events(options.event_capacity) {}

  void start_threads();
  void shutdown();

  SubmitResult prepare(LlmRequest& request, Job& job, std::string& why) const;
  SubmitResult admit(const std::shared_ptr<Job>& job);
  void worker_main();
  void dispatcher_main();
  std::shared_ptr<Job> take_locked();
  void run(const std::shared_ptr<Job>& job);
  void settle(const std::shared_ptr<Job>& job, LlmEvent ended);
  void run_warm_up(const WarmUp& w);
  ErrorClass exchange_warm_up(Transport& session, const HttpRequest& http, TimePoint sent, int& status);

  void insert_locked(const std::shared_ptr<Job>& job);
  void finish_locked(Job& job, LlmEvent ended);
  void stop_locked(const std::shared_ptr<Job>& job, ErrorClass error, const std::string& message);
  void cancel_own_initiative_locked();
  bool evict_for_locked(const Job& incoming);
  void expire_locked(TimePoint now);
  void end_pending_warm_ups_locked(ErrorClass error);
  std::optional<TimePoint> next_start_by_locked() const;
  std::vector<std::shared_ptr<Job>> live_in_order_locked() const;

  LlmEvent ending(const Job& job, ErrorClass error, std::string message, TimePoint now) const;
  DecisionPick rules_pick(const Job& job, ErrorClass error, TimePoint now) const;

  void emit(const Job& job, LlmEvent event);
  void note_attempt(Job& job, const std::string& provider);
  void end_start_locked(Job& job);
  void signal_notify();
  void signal_notify_locked();
  void call_notify();
  void log(LogLevel level, const std::string& line) const noexcept;
  void log_locked(LogLevel level, std::string line);

  // Fixed at construction.
  std::shared_ptr<TransportFactory> transport;
  std::shared_ptr<Clock> clock;
  LogFn log_fn;
  Millis slice;
  std::size_t capacity;
  int worker_count;
  Client client;
  EventQueue events;

  mutable std::mutex mutex;              // guards everything from here to `notify_mutex`
  std::condition_variable work_cv;        // the workers: something may be runnable, or shutdown
  std::condition_variable dispatcher_cv;  // the dispatcher: a deadline, a notify, a log line, or shutdown
  std::vector<std::shared_ptr<Job>> queue;                    // waiting: by priority, then by submission
  std::unordered_map<RequestId, std::shared_ptr<Job>> live;   // queued or running
  std::deque<WarmUp> warm_ups;                                // waiting for a worker
  std::map<std::string, int, std::less<>> provider_load;      // running requests that sent to each provider
  std::map<std::string, TimePoint, std::less<>> last_served;  // when each provider last ended one of them
  std::set<std::string, std::less<>> warming;                 // providers with a warm-up queued or in flight
  std::vector<std::pair<LogLevel, std::string>> pending_logs;
  std::uint64_t next_seq = 1;
  std::uint64_t ends = 0;  // requests ended after running (and reloads): what a request waiting after Busy waits for
  RequestId next_warm_up = 1;
  std::size_t running = 0;
  int dialogue_running = 0;
  int replies_live = 0;
  bool starting = false;  // a request is starting: the next waits for it (one start at a time, in the queue's order)
  bool closing = false;
  bool workers_done = false;
  bool notify_pending = false;
  std::atomic<bool> stopping{false};  // shutdown has begun: running warm-ups stop

  std::mutex notify_mutex;  // held while the callback runs, so that set_notify waits for it
  std::function<void()> notify;

  std::vector<StackThread> workers;
  StackThread dispatcher;
  std::once_flag shut;
};

namespace {

// What the client tells a running request, turned into its events.
class JobSink final : public StreamSink {
 public:
  JobSink(Service::Impl& service, Job& job) : s_(service), job_(job) {}

  void on_attempt(const AttemptInfo& attempt) override {
    s_.note_attempt(job_, attempt.provider);
    if (job_.started) return;
    job_.started = true;
    LlmEvent e = event(EventKind::Started);
    e.provider = attempt.provider;
    e.model = attempt.model;
    s_.emit(job_, std::move(e));
  }

  void on_delta(std::string_view text) override {
    LlmEvent e = event(EventKind::Delta);
    e.text = std::string(text);
    s_.emit(job_, std::move(e));
  }

  void on_reset() override { s_.emit(job_, event(EventKind::Reset)); }

 private:
  LlmEvent event(EventKind kind) const {
    LlmEvent e;
    e.kind = kind;
    e.id = job_.id;
    return e;
  }

  Service::Impl& s_;
  Job& job_;
};

}  // namespace

// --- Threads ---

void Service::Impl::start_threads() {
  try {
    dispatcher = StackThread(Component::LlmWorker, kNetStack, [this] { dispatcher_main(); });
    workers.reserve(static_cast<std::size_t>(worker_count));
    for (int i = 0; i < worker_count; ++i) workers.emplace_back(Component::LlmWorker, kNetStack, [this] { worker_main(); });
  } catch (...) {
    shutdown();
    throw;
  }
}

void Service::Impl::shutdown() {
  std::call_once(shut, [this] {
    {
      std::lock_guard lock(mutex);
      closing = true;
      stopping.store(true);
      for (const auto& job : live_in_order_locked()) stop_locked(job, ErrorClass::Cancelled, "shut down");
      end_pending_warm_ups_locked(ErrorClass::Cancelled);
      work_cv.notify_all();
    }
    for (StackThread& t : workers) {
      if (t.joinable()) t.join();
    }
    {
      std::lock_guard lock(mutex);
      workers_done = true;
      dispatcher_cv.notify_all();
    }
    if (dispatcher.joinable()) dispatcher.join();
    events.close();
  });
}

void Service::Impl::worker_main() {
  std::unique_lock lock(mutex);
  while (true) {
    expire_locked(clock->now());
    if (!warm_ups.empty()) {
      const WarmUp w = std::move(warm_ups.front());
      warm_ups.pop_front();
      lock.unlock();
      run_warm_up(w);
      lock.lock();
      continue;
    }
    if (const std::shared_ptr<Job> job = take_locked()) {
      lock.unlock();
      run(job);
      lock.lock();
      continue;
    }
    if (closing) return;
    work_cv.wait(lock);
  }
}

// Ends live requests whose start time is up, calls the notify callback and writes the log lines made under the lock.
void Service::Impl::dispatcher_main() {
  std::unique_lock lock(mutex);
  while (true) {
    expire_locked(clock->now());
    if (notify_pending || !pending_logs.empty()) {
      const bool wake = std::exchange(notify_pending, false);
      std::vector<std::pair<LogLevel, std::string>> lines;
      lines.swap(pending_logs);
      lock.unlock();
      for (const auto& [level, line] : lines) log(level, line);
      if (wake) call_notify();
      lock.lock();
      continue;
    }
    if (workers_done) return;
    const std::optional<TimePoint> next = next_start_by_locked();
    if (!next) {
      dispatcher_cv.wait(lock);
      continue;
    }
    // The clock may be a manual one, which no timed wait can follow: look again at least once a poll slice.
    const TimePoint now = clock->now();
    const Millis left = *next > now ? std::chrono::ceil<Millis>(*next - now) : Millis(0);
    dispatcher_cv.wait_for(lock, std::min(left, slice));
  }
}

// --- Submitting ---

// Checks the request and fills in the job; nothing is shared yet.
SubmitResult Service::Impl::prepare(LlmRequest& request, Job& job, std::string& why) const {
  if (request.id == 0) {
    why = "a request needs a non-zero id";
    return SubmitResult::Invalid;
  }
  job.id = request.id;
  job.epoch = request.epoch;
  const std::shared_ptr<const Config> config = client.config();
  if (ChatRequest* chat = std::get_if<ChatRequest>(&request.body)) {
    if (chat->messages.empty()) {
      why = "a chat request needs at least one message";
      return SubmitResult::Invalid;
    }
    const SlotConfig& slot = config->slot(chat->slot);
    const bool own = slot.name == kDialogue && chat->purpose == Purpose::OwnInitiative;
    if (own && !request.epoch) {
      why = "own-initiative dialogue needs the input epoch it was made at";
      return SubmitResult::Invalid;
    }
    if (chat->purpose == Purpose::Reply && request.epoch) {
      why = "a reply carries no epoch: the input epoch never cancels it";
      return SubmitResult::Invalid;
    }
    job.kind = RequestKind::Chat;
    job.slot = slot.name;
    job.priority = priority_of(slot.name, chat->purpose);
    job.dialogue = slot.name == kDialogue;
    job.reply = job.dialogue && chat->purpose == Purpose::Reply;
    job.own_initiative = own;
    job.evictable = slot.name == kMemory || slot.name == kBarkPool;
    job.live = own || slot.name == kBarkLive || slot.name == kDecisions;
    job.start_budget = start_budget(*config, slot);
    job.chat = std::move(*chat);
    return SubmitResult::Accepted;
  }
  DecisionCall& call = std::get<DecisionCall>(request.body);
  try {
    validate(call);  // its rule among the rest: the key must be one the call offers
  } catch (const std::invalid_argument& e) {
    why = e.what();
    return SubmitResult::Invalid;
  }
  const SlotConfig& slot = config->slot(call.slot);
  job.kind = RequestKind::Decision;
  job.slot = slot.name;
  job.priority = priority_of(slot.name, Purpose::Decision);
  job.own_initiative = request.epoch.has_value();
  job.live = true;
  job.start_budget = start_budget(*config, slot);
  job.decision = std::move(call);
  return SubmitResult::Accepted;
}

SubmitResult Service::Impl::admit(const std::shared_ptr<Job>& job) {
  std::lock_guard lock(mutex);
  if (closing) return SubmitResult::ShutDown;
  if (live.count(job->id) != 0) return SubmitResult::Duplicate;
  const TimePoint now = clock->now();
  job->accepted = now;
  job->seq = next_seq++;
  if (!client.gate_open()) {
    // Nothing goes out while the gate is closed (C15): accepted, and ended at once.
    finish_locked(*job, ending(*job, ErrorClass::GateClosed, "the offline gate is closed: nothing is sent", now));
    return SubmitResult::Accepted;
  }
  if (job->reply) cancel_own_initiative_locked();
  if (live.size() >= capacity && !evict_for_locked(*job)) return SubmitResult::Full;
  if (job->live) job->start_by = now + job->start_budget;
  live.emplace(job->id, job);
  if (job->reply) ++replies_live;
  insert_locked(job);
  work_cv.notify_one();
  if (job->start_by) dispatcher_cv.notify_one();
  return SubmitResult::Accepted;
}

// A reply comes first (ARCH §1.3, C9; DESIGN §8.9): own-initiative dialogue and its decision calls give way.
void Service::Impl::cancel_own_initiative_locked() {
  for (const auto& job : live_in_order_locked()) {
    if (job->own_initiative) stop_locked(job, ErrorClass::Cancelled, "a reply comes first: own-initiative work gives way");
  }
}

// A full queue (C9): the incoming request takes the place of the lowest and newest queued bark_pool or memory job it
// outranks. Nothing else is evicted, replies least of all.
bool Service::Impl::evict_for_locked(const Job& incoming) {
  for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
    if (!(*it)->evictable) continue;
    if ((*it)->priority >= incoming.priority) return false;
    const std::shared_ptr<Job> victim = *it;
    queue.erase(std::next(it).base());
    const bool pool = victim->slot == kBarkPool;
    log_locked(LogLevel::Info, "the request queue is full: a " + victim->slot + " request gives way to a higher-priority one");
    finish_locked(*victim, ending(*victim, pool ? ErrorClass::Dropped : ErrorClass::Preempted,
                                  pool ? "dropped: the request queue was full"
                                       : "the request queue was full and a higher-priority request took its place; "
                                         "requeue the job",
                                  clock->now()));
    return true;
  }
  return false;
}

// --- Running ---

// The first runnable request in the queue's order. Requests start one at a time (a start lasts until the first
// attempt is sent or the client answers without sending), so that when two could start together, a provider's last
// free connection goes to the one that comes first, and not to whichever worker reaches the client first.
std::shared_ptr<Job> Service::Impl::take_locked() {
  if (starting) return nullptr;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    const std::shared_ptr<Job> job = *it;
    if (job->parked_at && *job->parked_at >= ends) continue;  // answered Busy: waits for another request to end
    if (job->dialogue && dialogue_running > 0) continue;      // one dialogue request in flight
    if (job->own_initiative && replies_live > 0) continue;    // own-initiative work yields to a reply
    queue.erase(it);
    job->running = true;
    job->starting = true;
    starting = true;
    job->ends_seen = ends;
    ++running;
    if (job->dialogue) ++dialogue_running;
    return job;
  }
  return nullptr;
}

void Service::Impl::run(const std::shared_ptr<Job>& job) {
  JobSink sink(*this, *job);
  // R3: whatever the client lets out stays on this worker. Its component trips and the request ends as BadRequest
  // (ARCH §1.2), with its one Ended event like any other.
  std::optional<LlmEvent> ended = ffi_guard(Component::LlmWorker, std::optional<LlmEvent>(), [&] {
    LlmEvent e;
    e.kind = EventKind::Ended;
    e.id = job->id;
    if (job->kind == RequestKind::Chat) {
      e.chat = client.chat(job->chat, &sink, job->token);
    } else {
      e.pick = client.decide(job->decision, &sink, job->token);
    }
    return std::optional<LlmEvent>(std::move(e));
  });
  if (!ended) {
    // Checked at submit, so this is no request of ours: the class of failure is all the log says.
    log(LogLevel::Warn, "request " + std::to_string(job->id) + " (" + job->slot + "): the client failed; it ends as BadRequest");
    ended = ending(*job, ErrorClass::BadRequest, "the request could not be sent", clock->now());
  }
  settle(job, std::move(*ended));
}

// The client has answered. Busy goes back to its place in the queue, unless the request was stopped or its start time
// is up; anything else ends it.
void Service::Impl::settle(const std::shared_ptr<Job>& job, LlmEvent ended) {
  std::lock_guard lock(mutex);
  const TimePoint now = clock->now();
  Job& j = *job;
  end_start_locked(j);
  j.running = false;
  --running;
  if (j.dialogue) --dialogue_running;
  for (const std::string& p : j.providers) {
    if (--provider_load[p] <= 0) provider_load.erase(p);
    last_served[p] = now;
  }
  j.providers.clear();
  const ErrorClass error = ended.error();
  const bool busy = error == ErrorClass::Busy;
  if (j.stop.stop_requested()) {
    // Stopped by the handle: it ends with the reason it was stopped for, unless it had ended otherwise first.
    if (busy || error == ErrorClass::Cancelled) {
      if (ended.chat) {
        ended.chat->error = j.stop_error;
        ended.chat->message = j.stop_message;
      }
      if (ended.pick) ended.pick->error = j.stop_error;
    }
  } else if (busy) {
    if (ended.chat) j.busy_message = ended.chat->message;
    if (!j.start_by || now < *j.start_by) {
      j.parked_at = j.ends_seen;  // runnable again once a request has ended after the one it saw
      insert_locked(job);
      if (j.start_by) dispatcher_cv.notify_one();
      log_locked(LogLevel::Debug, "request " + std::to_string(j.id) + " (" + j.slot + ") is busy: it waits for another "
                                  "request to end");
      return;
    }
    ended = ending(j, ErrorClass::Busy, "not started within " + ms_text(j.start_budget) + " (" + j.busy_message + ")", now);
  }
  if (!busy) ++ends;
  finish_locked(j, std::move(ended));
  work_cv.notify_all();
}

void Service::Impl::insert_locked(const std::shared_ptr<Job>& job) {
  const auto at = std::find_if(queue.begin(), queue.end(), [&](const std::shared_ptr<Job>& q) {
    return q->priority < job->priority || (q->priority == job->priority && q->seq > job->seq);
  });
  queue.insert(at, job);
}

// The one way a request ends: its Ended event is queued, which never waits, and its id is free again.
void Service::Impl::finish_locked(Job& job, LlmEvent ended) {
  const auto it = live.find(job.id);
  if (it != live.end() && it->second.get() == &job) {
    live.erase(it);
    if (job.reply) --replies_live;
  }
  if (events.push_ending(std::move(ended))) signal_notify_locked();
}

// Ends a queued request at once, or stops a running one, which ends once the client has stopped (within a poll
// slice). For a running one the first reason given stands.
void Service::Impl::stop_locked(const std::shared_ptr<Job>& job, ErrorClass error, const std::string& message) {
  if (!job->running) {
    queue.erase(std::remove(queue.begin(), queue.end(), job), queue.end());
    finish_locked(*job, ending(*job, error, message, clock->now()));
    return;
  }
  if (job->stop.stop_requested()) return;
  job->stop_error = error;
  job->stop_message = message;
  job->stop.request_stop();
  events.wake_producers();
}

// A live request that has not started within its budget ends as Busy, so that the core takes its seed or rules.
void Service::Impl::expire_locked(TimePoint now) {
  for (auto it = queue.begin(); it != queue.end();) {
    const std::shared_ptr<Job> job = *it;
    if (!job->start_by || now < *job->start_by) {
      ++it;
      continue;
    }
    it = queue.erase(it);
    std::string message = "not started within " + ms_text(job->start_budget);
    if (!job->busy_message.empty()) message += " (" + job->busy_message + ")";
    log_locked(LogLevel::Debug, "request " + std::to_string(job->id) + " (" + job->slot + ") ends as Busy: " + message);
    finish_locked(*job, ending(*job, ErrorClass::Busy, message, now));
  }
}

std::optional<TimePoint> Service::Impl::next_start_by_locked() const {
  std::optional<TimePoint> next;
  for (const auto& job : queue) {
    if (job->start_by && (!next || *job->start_by < *next)) next = job->start_by;
  }
  return next;
}

std::vector<std::shared_ptr<Job>> Service::Impl::live_in_order_locked() const {
  std::vector<std::shared_ptr<Job>> jobs;
  jobs.reserve(live.size());
  for (const auto& entry : live) jobs.push_back(entry.second);
  std::sort(jobs.begin(), jobs.end(), [](const auto& a, const auto& b) { return a->seq < b->seq; });
  return jobs;
}

// A result for a request the handle ends without running it: no provider, and its time is the wait since submit.
LlmEvent Service::Impl::ending(const Job& job, ErrorClass error, std::string message, TimePoint now) const {
  LlmEvent e;
  e.kind = EventKind::Ended;
  e.id = job.id;
  if (job.kind == RequestKind::Chat) {
    ChatResult r;
    r.error = error;
    r.message = std::move(message);
    r.total = since(job.accepted, now);
    e.chat = std::move(r);
  } else {
    e.pick = rules_pick(job, error, now);
  }
  return e;
}

// The pick decide() makes on the rules, for a call the handle ends without running it.
DecisionPick Service::Impl::rules_pick(const Job& job, ErrorClass error, TimePoint now) const {
  const DecisionCall& call = job.decision;
  DecisionPick pick;
  pick.key = call.rule;
  pick.by = DecidedBy::Rules;
  pick.read = ReadAs::Rules;
  pick.fallback = true;
  pick.error = error;
  pick.latency = since(job.accepted, now);
  pick.prompt_hash = hash_messages(call.messages.empty() ? build_decision_messages(call) : call.messages);
  pick.task = call.task;
  pick.subject = call.subject;
  pick.option_keys = legal_keys(call);
  return pick;
}

void Service::Impl::emit(const Job& job, LlmEvent event) {
  bool woke = false;
  if (events.push(std::move(event), job.token, woke) && woke) signal_notify();
}

// An attempt went out: its provider is busy until the request ends, and the request's start is over.
void Service::Impl::note_attempt(Job& job, const std::string& provider) {
  std::lock_guard lock(mutex);
  end_start_locked(job);
  if (std::find(job.providers.begin(), job.providers.end(), provider) != job.providers.end()) return;
  job.providers.push_back(provider);
  ++provider_load[provider];
}

void Service::Impl::end_start_locked(Job& job) {
  if (!job.starting) return;
  job.starting = false;
  starting = false;
  work_cv.notify_all();
}

// --- Warm-up ---

void Service::Impl::run_warm_up(const WarmUp& w) {
  const ProviderConfig& provider = *w.config->provider(w.provider);
  // The provider's own headers and key, as every request to it carries. The body names the model and nothing else, so
  // the server loads it and generates nothing.
  HttpRequest http = build_chat_request(provider, json{{"model", w.model}, {"keep_alive", "10m"}});
  http.url = ollama_root(provider.base_url) + "/api/generate";
  WarmUpOutcome out;
  const TimePoint sent = clock->now();
  if (std::unique_ptr<Transport> session = transport->open()) {
    out.error = exchange_warm_up(*session, http, sent, out.status);
  } else {
    out.error = ErrorClass::NoHealthyProvider;
  }
  out.ok = out.error == ErrorClass::None;
  out.time = since(sent, clock->now());
  log(out.ok ? LogLevel::Debug : LogLevel::Info,
      "provider " + w.provider + ": warm-up of " + w.model +
          (out.ok ? " done in " + ms_text(out.time) : std::string(" failed (") + to_string(out.error) + ")"));
  LlmEvent e;
  e.kind = EventKind::WarmUpEnded;
  e.id = w.id;
  e.provider = w.provider;
  e.model = w.model;
  e.warm_up = out;
  std::lock_guard lock(mutex);
  warming.erase(w.provider);
  if (out.ok) last_served[w.provider] = clock->now();
  if (events.push_ending(std::move(e))) signal_notify_locked();
}

ErrorClass Service::Impl::exchange_warm_up(Transport& session, const HttpRequest& http, TimePoint sent, int& status) {
  const int exchange = session.start(http);
  const TimePoint give_up = sent + kWarmUpTimeout;
  std::vector<TransportEvent> got;
  while (true) {
    ErrorClass stop = ErrorClass::None;
    if (stopping.load()) {
      stop = ErrorClass::Cancelled;
    } else if (!client.gate_open()) {
      stop = ErrorClass::GateClosed;
    } else if (clock->now() >= give_up) {
      stop = ErrorClass::TotalTimeout;
    }
    if (stop != ErrorClass::None) {
      session.cancel(exchange);
      return stop;
    }
    got.clear();
    const Millis left = std::chrono::ceil<Millis>(give_up - clock->now());
    session.wait(std::clamp(left, Millis(0), slice), got);
    for (const TransportEvent& e : got) {
      if (e.exchange != exchange) continue;
      switch (e.kind) {
        case TransportEventKind::Head:
          status = e.status;
          break;
        case TransportEventKind::Data:  // the server's word on the load: not needed
          break;
        case TransportEventKind::Done:
          return status == 0 ? ErrorClass::Server : classify_http_status(status);
        case TransportEventKind::Failed:
          return ErrorClass::Server;
      }
    }
  }
}

void Service::Impl::end_pending_warm_ups_locked(ErrorClass error) {
  while (!warm_ups.empty()) {
    const WarmUp w = std::move(warm_ups.front());
    warm_ups.pop_front();
    warming.erase(w.provider);
    LlmEvent e;
    e.kind = EventKind::WarmUpEnded;
    e.id = w.id;
    e.provider = w.provider;
    e.model = w.model;
    e.warm_up.error = error;
    if (events.push_ending(std::move(e))) signal_notify_locked();
  }
}

// --- Calling out ---

void Service::Impl::signal_notify() {
  std::lock_guard lock(mutex);
  signal_notify_locked();
}

void Service::Impl::signal_notify_locked() {
  notify_pending = true;
  dispatcher_cv.notify_one();
}

void Service::Impl::call_notify() {
  std::lock_guard lock(notify_mutex);
  if (!notify) return;
  // The caller's code on the handle's thread: a fault in it trips the worker's component and loses this call only.
  if (!ffi_guard(Component::LlmWorker, false, [&] {
        notify();
        return true;
      })) {
    log(LogLevel::Warn, "the event notify callback failed; it stays registered");
  }
}

void Service::Impl::log(LogLevel level, const std::string& line) const noexcept {
  // The caller's function, on the handle's threads and the caller's: a fault in it loses the line, never the thread.
  if (log_fn) ffi_guard(Component::LlmWorker, [&] { log_fn(level, line); });
}

void Service::Impl::log_locked(LogLevel level, std::string line) {
  pending_logs.emplace_back(level, std::move(line));
  dispatcher_cv.notify_one();
}

// --- The public surface ---

const char* to_string(SubmitResult result) {
  switch (result) {
    case SubmitResult::Accepted: return "Accepted";
    case SubmitResult::Full: return "Full";
    case SubmitResult::Duplicate: return "Duplicate";
    case SubmitResult::Invalid: return "Invalid";
    case SubmitResult::ShutDown: return "ShutDown";
  }
  return "Unknown";
}

Service::Service(Config config, std::shared_ptr<TransportFactory> transport, ServiceOptions options)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(transport), std::move(options))) {
  impl_->start_threads();
}

Service::~Service() { impl_->shutdown(); }

SubmitResult Service::submit(LlmRequest request) {
  auto job = std::make_shared<Job>();
  std::string why;
  if (const SubmitResult checked = impl_->prepare(request, *job, why); checked != SubmitResult::Accepted) {
    impl_->log(LogLevel::Warn, "request " + std::to_string(request.id) + " refused: " + why);
    return checked;
  }
  return impl_->admit(job);
}

bool Service::cancel(RequestId id) {
  Impl& s = *impl_;
  std::lock_guard lock(s.mutex);
  const auto it = s.live.find(id);
  if (it == s.live.end()) return false;
  const std::shared_ptr<Job> job = it->second;
  s.stop_locked(job, ErrorClass::Cancelled, job->running ? "cancelled" : "cancelled before it was sent");
  return true;
}

std::size_t Service::cancel_before_epoch(Epoch epoch) {
  Impl& s = *impl_;
  std::lock_guard lock(s.mutex);
  std::size_t n = 0;
  for (const auto& job : s.live_in_order_locked()) {
    if (!job->epoch || *job->epoch >= epoch || job->stop.stop_requested()) continue;
    s.stop_locked(job, ErrorClass::Cancelled, "the input epoch moved on");
    ++n;
  }
  return n;
}

std::optional<LlmEvent> Service::poll() { return impl_->events.poll(); }

std::size_t Service::drain(std::vector<LlmEvent>& out) { return impl_->events.drain(out); }

std::optional<LlmEvent> Service::wait(Millis timeout) { return impl_->events.wait(timeout); }

void Service::set_notify(std::function<void()> notify) {
  std::lock_guard lock(impl_->notify_mutex);
  impl_->notify = std::move(notify);
}

void Service::set_gate_open(bool open) {
  Impl& s = *impl_;
  s.client.set_gate_open(open);  // first, so that a request a worker is about to send finds it closed
  if (open) return;
  std::lock_guard lock(s.mutex);
  for (const auto& job : s.live_in_order_locked()) {
    s.stop_locked(job, ErrorClass::GateClosed, job->running ? "the offline gate closed" : "the offline gate closed: nothing is sent");
  }
  s.end_pending_warm_ups_locked(ErrorClass::GateClosed);
}

bool Service::gate_open() const { return impl_->client.gate_open(); }

std::optional<RequestId> Service::warm_up(std::string_view slot_name) {
  Impl& s = *impl_;
  if (!s.transport) return std::nullopt;
  const std::shared_ptr<const Config> config = s.client.config();
  const SlotConfig& slot = config->slot(slot_name);
  if (slot.models.empty()) return std::nullopt;
  const ModelRef& model = slot.models.front();
  const ProviderConfig* provider = config->provider(model.provider);
  if (provider == nullptr || provider->kind != ProviderKind::Local || provider->flavor != Flavor::Ollama ||
      !provider->keys_resolved()) {
    return std::nullopt;
  }
  std::lock_guard lock(s.mutex);
  if (s.closing || !s.client.gate_open()) return std::nullopt;
  if (s.provider_load.count(provider->id) != 0 || s.warming.count(provider->id) != 0) return std::nullopt;
  const auto served = s.last_served.find(provider->id);
  if (served != s.last_served.end() && s.clock->now() - served->second <= kWarmUpIdle) return std::nullopt;
  const RequestId id = s.next_warm_up++;
  s.warming.insert(provider->id);
  s.warm_ups.push_back(WarmUp{id, config, provider->id, model.model});
  s.work_cv.notify_one();
  return id;
}

void Service::reload(Config config) {
  impl_->client.reload(std::move(config));
  std::lock_guard lock(impl_->mutex);
  ++impl_->ends;  // providers' and slots' room may have changed: requests waiting after Busy try again
  impl_->work_cv.notify_all();
}

const Client& Service::client() const { return impl_->client; }

ServiceStats Service::stats() const {
  const Impl& s = *impl_;
  std::lock_guard lock(s.mutex);
  ServiceStats stats;
  stats.queued = s.queue.size();
  stats.running = s.running;
  stats.events = s.events.size();
  stats.workers = s.worker_count;
  return stats;
}

void Service::shutdown() { impl_->shutdown(); }

}  // namespace en::llm
