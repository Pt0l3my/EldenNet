// The request loop (ARCH §11.2 to §11.4). One request runs its attempts on one Transport: the first-token and total
// deadlines, the backup request beside a late cloud reply, the slot's failure actions (retry, fallback, the stripped
// retry), health, preemption of background jobs and the requests-per-hour budget, and it ends with one result.
#include "en/llm/client.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "en/llm/request.hpp"
#include "en/llm/sse.hpp"

namespace en::llm {

using nlohmann::json;

namespace {

enum class Action { Return, Retry, Fallback, RetryStripped };

// ARCH §11.2's actions, as far as they are the client's. The rest of the table (a seed, a toast, the authored
// deflection, a requeue, showing the complete sentences) is the caller's, done with the error this returns.
Action action_for(std::string_view slot, ErrorClass e) {
  using E = ErrorClass;
  if (slot == kDialogue) {
    switch (e) {
      case E::FirstTokenTimeout:
      case E::RateLimited:
      case E::Server:
      case E::Empty:
        return Action::Fallback;
      case E::BadRequest:
        return Action::RetryStripped;
      default:
        return Action::Return;
    }
  }
  if (slot == kBarkPool) return e == E::FirstTokenTimeout || e == E::Server ? Action::Retry : Action::Return;
  if (slot == kMemory) {
    switch (e) {
      case E::FirstTokenTimeout:
      case E::Empty:
      case E::Truncated:
        return Action::Retry;
      case E::BadRequest:
        return Action::RetryStripped;
      default:
        return Action::Return;
    }
  }
  // bark_live takes its seed, selftest and draft show the error, decisions take their rules: all at once.
  return Action::Return;
}

}  // namespace

// ARCH §11.3: replies > own-initiative lines and the decisions that choose them > bark_live > memory > bark_pool.
int priority_of(std::string_view slot, Purpose purpose) {
  if (slot == kDialogue) return purpose == Purpose::Reply ? 5 : 4;
  if (slot == kBarkLive) return 3;
  if (slot == kMemory) return 2;
  if (slot == kBarkPool) return 1;
  return 4;  // decisions, selftest and draft
}

namespace {

// Only background jobs give way; nothing ever cancels dialogue (ARCH §11.3).
bool is_preemptible(std::string_view slot) { return slot == kMemory || slot == kBarkPool; }

constexpr std::size_t kMaxErrorBody = 16 * 1024;
constexpr std::size_t kMaxWholeBody = 4u << 20;

Millis since(TimePoint from, TimePoint to) { return std::chrono::duration_cast<Millis>(to - from); }

std::string ms_text(Millis ms) { return std::to_string(ms.count()) + " ms"; }

// A JSON content type means the server ignored stream: true and sent the whole reply at once.
bool is_event_stream(const std::vector<HttpHeader>& headers) {
  for (const HttpHeader& h : headers) {
    if (h.name == "content-type") return h.value.find("application/json") == std::string::npos;
  }
  return true;
}

std::string ascii_lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

}  // namespace

// One HTTP exchange's claim on its provider's connections, which a higher-priority job may take back (ARCH §11.3).
struct InFlight {
  std::uint64_t id = 0;
  std::string provider;
  int priority = 0;
  bool preemptible = false;
  std::atomic<bool> preempted{false};
};

struct Client::Impl {
  Impl(Config cfg, std::shared_ptr<TransportFactory> factory, ClientOptions options)
      : config(std::make_shared<const Config>(std::move(cfg))),
        transport(std::move(factory)),
        clock(options.clock ? std::move(options.clock) : system_clock()),
        log(std::move(options.log)),
        slice(options.poll_slice.count() > 0 ? options.poll_slice : Millis(50)),
        health(clock),
        budget(clock, config->requests_per_hour) {}

  std::shared_ptr<const Config> snapshot() const {
    std::lock_guard lock(mutex);
    return config;
  }

  void emit(LogLevel level, const std::string& line) const {
    if (log) log(level, line);
  }

  bool acquire_slot(const std::string& slot, int limit) {
    std::lock_guard lock(mutex);
    int& running = slot_in_flight[slot];
    if (running >= limit) return false;
    ++running;
    return true;
  }

  void release_slot(const std::string& slot) {
    std::lock_guard lock(mutex);
    --slot_in_flight[slot];
  }

  // A place on the provider, or null when it is full and runs nothing of lower priority that may give way.
  std::shared_ptr<InFlight> acquire_provider(const ProviderConfig& provider, int priority, bool preemptible) {
    std::shared_ptr<InFlight> victim;
    std::shared_ptr<InFlight> ticket;
    {
      std::lock_guard lock(mutex);
      int load = 0;
      for (const auto& f : in_flight) {
        if (f->provider != provider.id || f->preempted.load()) continue;
        ++load;
        const bool lower = f->preemptible && f->priority < priority;
        if (lower && (!victim || f->priority < victim->priority || (f->priority == victim->priority && f->id > victim->id))) {
          victim = f;
        }
      }
      if (load >= provider.max_concurrency) {
        if (!victim) return nullptr;
        victim->preempted.store(true);  // its request sees this within one poll slice and ends as Preempted
      } else {
        victim.reset();
      }
      ticket = std::make_shared<InFlight>();
      ticket->id = next_ticket++;
      ticket->provider = provider.id;
      ticket->priority = priority;
      ticket->preemptible = preemptible;
      in_flight.push_back(ticket);
    }
    if (victim) emit(LogLevel::Info, "provider " + provider.id + ": a background job gives way to a higher-priority request");
    return ticket;
  }

  void release_provider(const std::shared_ptr<InFlight>& ticket) {
    if (!ticket) return;
    std::lock_guard lock(mutex);
    in_flight.erase(std::remove(in_flight.begin(), in_flight.end(), ticket), in_flight.end());
  }

  mutable std::mutex mutex;  // guards config, slot_in_flight, in_flight
  std::shared_ptr<const Config> config;
  std::shared_ptr<TransportFactory> transport;
  std::shared_ptr<Clock> clock;
  LogFn log;
  Millis slice;
  Health health;
  RequestBudget budget;
  std::atomic<bool> gate_open{true};
  std::map<std::string, int, std::less<>> slot_in_flight;
  std::vector<std::shared_ptr<InFlight>> in_flight;
  std::uint64_t next_ticket = 1;
};

namespace {

class RequestRun {
 public:
  RequestRun(Client::Impl& client, const ChatRequest& request, StreamSink* sink, StreamSink* observer,
             std::stop_token stop)
      : c_(client),
        config_(client.snapshot()),
        request_(request),
        sink_(sink),
        observer_(observer),
        stop_(std::move(stop)),
        slot_(&config_->slot(request.slot)),
        slot_name_(slot_->name),
        priority_(priority_of(slot_name_, request.purpose)),
        preemptible_(is_preemptible(slot_name_)),
        tried_(slot_->models.size(), false),
        retries_left_(slot_->budget.retries) {
    max_tokens_ = std::min(request.max_tokens.value_or(slot_->budget.max_tokens), slot_->budget.max_tokens);
    temperature_ = request.temperature.value_or(slot_->budget.temperature);
    backup_allowed_ = slot_name_ == kDialogue && request.purpose == Purpose::Reply;
  }

  ~RequestRun() {
    for (auto& a : attempts_) {
      if (a->active && transport_) transport_->cancel(a->exchange);
      c_.release_provider(a->ticket);
    }
    if (holds_slot_) c_.release_slot(slot_name_);
  }

  ChatResult run();

 private:
  enum class EndKind { Normal, LostRace, Stopped };

  struct Live {
    int exchange = 0;
    std::size_t model_index = 0;
    const ProviderConfig* provider = nullptr;
    std::string model;
    std::shared_ptr<InFlight> ticket;
    BodyMode mode = BodyMode::Full;
    bool minimal_effort = false;
    bool stripped_retry = false;
    bool used_schema = false;
    bool used_logprobs = false;
    bool backup = false;
    TimePoint sent_at;
    std::optional<TimePoint> first_content_at;
    Millis first_token_budget{0};
    Millis total_budget{0};
    int status = 0;
    std::vector<HttpHeader> headers;
    bool event_stream = true;
    std::string error_body;
    std::string whole_body;
    SseParser sse;
    ReplyAssembler reply;
    bool active = true;
    bool transport_done = false;
    bool forwarded = false;
    bool refused_reasoning = false;
    Millis cooldown{0};
    ErrorClass error = ErrorClass::None;
    std::string message;
    std::size_t info = 0;
  };

  struct Pick {
    std::optional<std::size_t> index;
    std::shared_ptr<InFlight> ticket;
    ErrorClass error = ErrorClass::None;
    std::string message;
  };

  ChatResult loop();
  Pick pick_model(bool untried_only);
  Pick pick_same(std::size_t index);
  ErrorClass start(Pick pick, BodyMode mode, bool backup, std::string& why);
  bool follow_up();
  void maybe_start_backup();
  void check_deadlines();
  void check_preemption();
  Millis next_wait() const;
  void handle(const TransportEvent& event);
  void on_done(Live& a);
  void on_http_error(Live& a);
  void apply(Live& a, const ReplyAssembler::Step& step);
  void end(Live& a, ErrorClass error, std::string message, EndKind kind = EndKind::Normal);
  void fail(Live& a, ErrorClass error, std::string message);
  void apply_health(Live& a);
  void complete(Live& a);
  void stop_all(ErrorClass error, const std::string& message);
  bool any_active() const;
  Live* find(int exchange);
  ChatResult result_from(const Live* a, ErrorClass error, const std::string& message);
  ChatResult early(ErrorClass error, const std::string& message);

  Client::Impl& c_;
  std::shared_ptr<const Config> config_;
  const ChatRequest& request_;
  StreamSink* sink_;
  StreamSink* observer_;  // told of each attempt as it is sent: the sink, or a decision call's observer
  std::stop_token stop_;
  const SlotConfig* slot_;
  std::string slot_name_;
  int priority_;
  bool preemptible_;
  std::vector<bool> tried_;
  int retries_left_;
  int max_tokens_ = 0;
  double temperature_ = 0.0;
  bool backup_allowed_ = false;
  bool fallback_used_ = false;
  bool stripped_used_ = false;
  bool holds_slot_ = false;
  std::unique_ptr<Transport> transport_;
  std::vector<std::unique_ptr<Live>> attempts_;
  Live* winner_ = nullptr;     // the attempt whose text reaches the caller: the first to stream
  Live* last_ended_ = nullptr;
  std::optional<ChatResult> done_;
  TimePoint started_;
  ChatResult result_;
};

ChatResult RequestRun::run() {
  started_ = c_.clock->now();
  if (request_.messages.empty()) throw std::invalid_argument("a chat request needs at least one message");
  if (!c_.gate_open.load()) return early(ErrorClass::GateClosed, "the offline gate is closed: nothing is sent");
  if (stop_.stop_requested()) return early(ErrorClass::Cancelled, "cancelled before it was sent");
  if (!c_.acquire_slot(slot_name_, slot_->budget.concurrency)) {
    return early(ErrorClass::Busy, "slot " + slot_name_ + " already runs " + std::to_string(slot_->budget.concurrency) +
                                       " request(s)");
  }
  holds_slot_ = true;
  transport_ = c_.transport ? c_.transport->open() : nullptr;
  if (!transport_) return early(ErrorClass::NoHealthyProvider, "this build has no HTTP transport");
  Pick first = pick_model(false);
  if (!first.index) return early(first.error, first.message);
  std::string why;
  if (const ErrorClass e = start(std::move(first), BodyMode::Full, false, why); e != ErrorClass::None) return early(e, why);
  return loop();
}

ChatResult RequestRun::loop() {
  std::vector<TransportEvent> events;
  while (true) {
    if (stop_.stop_requested()) {
      stop_all(ErrorClass::Cancelled, "cancelled");
    } else if (!c_.gate_open.load()) {
      stop_all(ErrorClass::GateClosed, "the offline gate closed");
    } else {
      check_preemption();
      if (!done_) check_deadlines();
      if (!done_) maybe_start_backup();
    }
    if (done_) return std::move(*done_);
    if (!any_active()) {
      if (!follow_up()) return result_from(last_ended_, last_ended_->error, last_ended_->message);
      continue;
    }
    events.clear();
    transport_->wait(next_wait(), events);
    for (const TransportEvent& event : events) {
      handle(event);
      if (done_) break;
    }
  }
}

// The slot's models in order, from the first that is healthy and has room on its provider (ARCH §11.2's failover).
RequestRun::Pick RequestRun::pick_model(bool untried_only) {
  Pick pick;
  bool busy = false;
  bool cooling = false;
  bool keyless = false;
  for (std::size_t i = 0; i < slot_->models.size(); ++i) {
    if (untried_only && tried_[i]) continue;
    const ModelRef& ref = slot_->models[i];
    const ProviderConfig* provider = config_->provider(ref.provider);
    if (provider == nullptr) continue;
    if (!provider->keys_resolved()) {
      keyless = true;
      continue;
    }
    if (!c_.health.pair_healthy(provider->id, ref.model)) {
      cooling = true;
      continue;
    }
    pick.ticket = c_.acquire_provider(*provider, priority_, preemptible_);
    if (!pick.ticket) {
      busy = true;
      continue;
    }
    pick.index = i;
    return pick;
  }
  if (busy) {
    pick.error = ErrorClass::Busy;
    pick.message = "every healthy provider of slot " + slot_name_ + " is at its concurrency";
  } else {
    pick.error = ErrorClass::NoHealthyProvider;
    if (slot_->models.empty()) {
      pick.message = "slot " + slot_name_ + " has no model";
    } else if (cooling || keyless) {
      pick.message = "no healthy provider for slot " + slot_name_ + (cooling ? ": cooling or disabled" : "") +
                     (keyless ? (cooling ? "; a key is missing" : ": a key is missing") : "");
    } else {
      pick.message = "no further model for slot " + slot_name_;
    }
  }
  return pick;
}

// The same model again, for the stripped retry: a BadRequest leaves its pair's health as it was.
RequestRun::Pick RequestRun::pick_same(std::size_t index) {
  Pick pick;
  const ModelRef& ref = slot_->models[index];
  const ProviderConfig* provider = config_->provider(ref.provider);
  if (provider == nullptr || !c_.health.pair_healthy(provider->id, ref.model)) {
    pick.error = ErrorClass::NoHealthyProvider;
    pick.message = "the model is no longer healthy";
    return pick;
  }
  pick.ticket = c_.acquire_provider(*provider, priority_, preemptible_);
  if (!pick.ticket) {
    pick.error = ErrorClass::Busy;
    pick.message = "provider " + provider->id + " is at its concurrency";
    return pick;
  }
  pick.index = index;
  return pick;
}

ErrorClass RequestRun::start(Pick pick, BodyMode mode, bool backup, std::string& why) {
  // Replies go out past the bound, since the player asked; everything else stops at it (ARCH §11.4).
  if (!c_.budget.take(request_.purpose == Purpose::Reply)) {
    c_.release_provider(pick.ticket);
    why = "the requests per hour are spent (" + std::to_string(c_.budget.limit()) + ")";
    return ErrorClass::BudgetSpent;
  }
  auto live = std::make_unique<Live>();
  Live& a = *live;
  const ModelRef& ref = slot_->models[*pick.index];
  a.model_index = *pick.index;
  a.provider = config_->provider(ref.provider);
  a.model = ref.model;
  a.ticket = std::move(pick.ticket);
  a.backup = backup;
  a.minimal_effort = c_.health.minimal_effort(a.provider->id, a.model);
  a.stripped_retry = mode == BodyMode::Stripped;
  // A pair that has only ever taken the stripped body starts stripped (learned this session).
  if (mode == BodyMode::Full && c_.health.full_body_refused(a.provider->id, a.model)) mode = BodyMode::Stripped;
  a.mode = mode;
  a.used_schema = mode == BodyMode::Full && request_.json_schema && a.provider->json_mode == JsonMode::JsonSchema &&
                  !c_.health.schema_refused(a.provider->id);
  a.used_logprobs = mode == BodyMode::Full && request_.top_logprobs > 0 && a.provider->logprobs &&
                    !c_.health.logprobs_refused(a.provider->id);

  BodyInputs in;
  in.provider = a.provider;
  in.model = a.model;
  in.messages = &request_.messages;
  in.max_tokens = max_tokens_;
  in.temperature = temperature_;
  in.json_schema = a.used_schema ? &*request_.json_schema : nullptr;
  in.top_logprobs = a.used_logprobs ? request_.top_logprobs : 0;
  in.mode = mode;
  in.minimal_effort = a.minimal_effort;
  const HttpRequest http = build_chat_request(*a.provider, build_chat_body(in));

  const bool local = a.provider->kind == ProviderKind::Local;
  a.first_token_budget = local ? slot_->budget.first_token_local : slot_->budget.first_token;
  a.total_budget = slot_->budget.total;
  a.sent_at = c_.clock->now();
  a.exchange = transport_->start(http);
  tried_[a.model_index] = true;

  AttemptInfo info;
  info.provider = a.provider->id;
  info.model = a.model;
  info.backup = backup;
  info.stripped = mode == BodyMode::Stripped;
  info.minimal_effort = a.minimal_effort;
  info.sent_at = since(started_, a.sent_at);
  a.info = result_.attempts.size();
  result_.attempts.push_back(std::move(info));
  c_.emit(LogLevel::Debug, "slot " + slot_name_ + ": " + (backup ? "backup request to " : "request to ") +
                               a.provider->id + " / " + a.model);
  attempts_.push_back(std::move(live));
  if (observer_ != nullptr) observer_->on_attempt(result_.attempts[a.info]);
  return ErrorClass::None;
}

// After the last running attempt failed, the slot's action for its class decides whether another one starts.
bool RequestRun::follow_up() {
  Live& last = *last_ended_;
  // Text the caller has already shown is never followed by a second reply.
  if (last.forwarded || last.error == ErrorClass::Cancelled || last.error == ErrorClass::Preempted) return false;
  Pick pick;
  BodyMode mode = BodyMode::Full;
  switch (action_for(slot_name_, last.error)) {
    case Action::Return:
      return false;
    case Action::Retry:
      if (retries_left_ <= 0) return false;
      --retries_left_;
      pick = pick_model(false);  // the same model while it is healthy, else the next
      break;
    case Action::Fallback:
      if (fallback_used_) return false;  // once per request, and a backup was that once
      fallback_used_ = true;
      pick = pick_model(true);
      break;
    case Action::RetryStripped:
      if (stripped_used_) return false;
      stripped_used_ = true;
      // A body naming reasoning means the model refused reasoning off: the retry keeps the optional fields and
      // asks for the lowest effort (noted on the pair, so the body has it). Otherwise it goes without them.
      mode = last.refused_reasoning ? BodyMode::Full : BodyMode::Stripped;
      pick = pick_same(last.model_index);
      break;
  }
  if (!pick.index) {
    last.message += "; " + pick.message;
    return false;
  }
  std::string why;
  if (start(std::move(pick), mode, false, why) != ErrorClass::None) {
    last.message += "; " + why;
    return false;
  }
  return true;
}

void RequestRun::maybe_start_backup() {
  if (!backup_allowed_ || fallback_used_ || winner_ != nullptr || attempts_.size() != 1) return;
  Live& primary = *attempts_.front();
  if (!primary.active || primary.first_content_at || primary.provider->kind != ProviderKind::Cloud) return;
  if (c_.clock->now() < primary.sent_at + config_->backup_after) return;
  Pick pick = pick_model(true);  // the slot's fallback, if that pair is healthy and has room (or a job gives way)
  if (!pick.index) return;       // looked at again on the next turn of the loop
  fallback_used_ = true;
  std::string why;
  if (start(std::move(pick), BodyMode::Full, true, why) != ErrorClass::None) {
    c_.emit(LogLevel::Debug, "slot " + slot_name_ + ": no backup request (" + why + ")");
  }
}

void RequestRun::check_deadlines() {
  const TimePoint now = c_.clock->now();
  for (auto& owned : attempts_) {
    Live& a = *owned;
    if (!a.active) continue;
    if (!a.first_content_at && now >= a.sent_at + a.first_token_budget) {
      end(a, ErrorClass::FirstTokenTimeout, "no text within " + ms_text(a.first_token_budget));
    } else if (now >= a.sent_at + a.total_budget) {
      fail(a, ErrorClass::TotalTimeout, "not finished within " + ms_text(a.total_budget));
    }
    if (done_) return;
  }
}

void RequestRun::check_preemption() {
  for (auto& owned : attempts_) {
    Live& a = *owned;
    if (a.active && a.ticket && a.ticket->preempted.load()) {
      end(a, ErrorClass::Preempted, "a higher-priority request took the provider", EndKind::Stopped);
      stop_all(ErrorClass::Preempted, "a higher-priority request took the provider; requeue the job");
      return;
    }
  }
}

Millis RequestRun::next_wait() const {
  const TimePoint now = c_.clock->now();
  TimePoint wake = now + c_.slice;
  for (const auto& owned : attempts_) {
    const Live& a = *owned;
    if (!a.active) continue;
    wake = std::min(wake, a.sent_at + a.total_budget);
    if (!a.first_content_at) wake = std::min(wake, a.sent_at + a.first_token_budget);
  }
  if (backup_allowed_ && !fallback_used_ && winner_ == nullptr && attempts_.size() == 1) {
    // Only a time still ahead: once it has passed and no backup could start, the loop looks again each slice.
    const TimePoint backup_at = attempts_.front()->sent_at + config_->backup_after;
    if (backup_at > now) wake = std::min(wake, backup_at);
  }
  return wake > now ? std::chrono::ceil<Millis>(wake - now) : Millis(0);
}

RequestRun::Live* RequestRun::find(int exchange) {
  for (auto& owned : attempts_) {
    if (owned->exchange == exchange && owned->active) return owned.get();
  }
  return nullptr;
}

bool RequestRun::any_active() const {
  return std::any_of(attempts_.begin(), attempts_.end(), [](const auto& a) { return a->active; });
}

void RequestRun::handle(const TransportEvent& event) {
  Live* a = find(event.exchange);
  if (a == nullptr) return;  // an exchange already ended or cancelled
  switch (event.kind) {
    case TransportEventKind::Head:
      a->status = event.status;
      a->headers = event.headers;
      a->event_stream = is_event_stream(event.headers);
      break;
    case TransportEventKind::Data:
      if (a->status < 200 || a->status >= 300) {
        if (a->error_body.size() < kMaxErrorBody) a->error_body.append(event.data, 0, kMaxErrorBody - a->error_body.size());
      } else if (!a->event_stream) {
        a->whole_body.append(event.data);
        if (a->whole_body.size() > kMaxWholeBody) fail(*a, ErrorClass::Server, "the reply passed 4 MB");
      } else {
        std::vector<SseEvent> sse_events;
        if (!a->sse.feed(event.data, sse_events)) {
          fail(*a, ErrorClass::Server, "a stream line passed 4 MB");
        } else {
          for (const SseEvent& e : sse_events) {
            apply(*a, a->reply.on_event_data(e.data));
            if (!a->active || done_) break;
          }
        }
      }
      break;
    case TransportEventKind::Done:
      a->transport_done = true;
      on_done(*a);
      break;
    case TransportEventKind::Failed:
      a->transport_done = true;
      fail(*a, ErrorClass::Server, "connection failed: " + event.error);
      break;
  }
}

void RequestRun::on_done(Live& a) {
  if (a.status < 200 || a.status >= 300) {
    on_http_error(a);
    return;
  }
  if (!a.event_stream) {
    apply(a, a.reply.on_whole_body(a.whole_body));
  } else {
    std::vector<SseEvent> rest;
    a.sse.finish(rest);
    for (const SseEvent& e : rest) apply(a, a.reply.on_event_data(e.data));
  }
  if (!a.active) return;
  apply(a, a.reply.finish());

  const ReplyAssembler& r = a.reply;
  ErrorClass error = ErrorClass::None;
  std::string message;
  if (r.stream_error()) {
    error = *r.stream_error();
    message = "error in the stream: " + r.stream_error_message();
  } else if (r.finish_reason() == "error") {
    error = ErrorClass::Server;
    message = "the stream ended with an error";
  } else if (r.finish_reason() == "content_filter") {
    error = ErrorClass::Refused;
    message = "stopped by the provider's content filter";
  } else if (r.text().empty()) {
    error = ErrorClass::Empty;
    message = r.saw_reasoning() ? "no text: the reply was all reasoning" : "no text in the reply";
  } else if (looks_like_refusal(r.text())) {
    error = ErrorClass::Refused;
    message = "the model refused";
  } else if (r.finish_reason() == "length" && !ends_at_sentence(r.text())) {
    error = ErrorClass::Truncated;
    message = "cut at max_tokens (" + std::to_string(max_tokens_) + ") before a full sentence";
  }
  if (error == ErrorClass::None) {
    complete(a);
    return;
  }
  fail(a, error, message);
}

void RequestRun::on_http_error(Live& a) {
  const ErrorClass error = a.status == 0 ? ErrorClass::Server : classify_http_status(a.status);
  std::string message = describe_error_body(a.error_body, a.status);
  if (error == ErrorClass::RateLimited) a.cooldown = rate_limit_cooldown(a.headers);
  if (error == ErrorClass::BadRequest) {
    const std::string& p = a.provider->id;
    a.refused_reasoning = names_reasoning(a.error_body) && !a.minimal_effort;
    if (a.refused_reasoning) {
      if (c_.health.note_minimal_effort(p, a.model)) {
        c_.emit(LogLevel::Warn, "provider " + p + ", model " + a.model + " refuses reasoning off: its requests now ask for "
                                "the lowest effort (" + minimal_effort_level(a.provider->flavor) + ")");
      }
    } else if (a.used_logprobs && ascii_lower(a.error_body).find("logprob") != std::string::npos) {
      if (c_.health.note_logprobs_refused(p)) c_.emit(LogLevel::Info, "provider " + p + " refuses logprobs: not asked again");
    } else if (a.used_schema) {
      if (c_.health.note_schema_refused(p)) {
        c_.emit(LogLevel::Info, "provider " + p + " refuses the JSON schema: decisions read plain answers from now on");
      }
    } else if (a.used_logprobs) {
      if (c_.health.note_logprobs_refused(p)) c_.emit(LogLevel::Info, "provider " + p + " refuses logprobs: not asked again");
    }
  }
  end(a, error, message);
}

void RequestRun::apply(Live& a, const ReplyAssembler::Step& step) {
  if (step.reset && a.forwarded && sink_ != nullptr) sink_->on_reset();
  if (step.delta.empty()) return;
  const TimePoint now = c_.clock->now();
  if (!a.first_content_at) {
    a.first_content_at = now;
    result_.attempts[a.info].first_token = since(a.sent_at, now);
  }
  if (winner_ == nullptr) {
    // The first attempt to stream is kept and the other is cancelled at once, which is no failure (ARCH §11.2).
    winner_ = &a;
    result_.first_token = since(started_, now);
    for (auto& other : attempts_) {
      if (other.get() != &a && other->active) {
        end(*other, ErrorClass::Cancelled, "the other request streamed first", EndKind::LostRace);
      }
    }
  }
  if (winner_ == &a && sink_ != nullptr) {
    sink_->on_delta(step.delta);
    a.forwarded = true;
  }
}

void RequestRun::end(Live& a, ErrorClass error, std::string message, EndKind kind) {
  if (!a.active) return;
  a.active = false;
  a.error = error;
  a.message = std::move(message);
  if (!a.transport_done) transport_->cancel(a.exchange);
  c_.release_provider(a.ticket);
  a.ticket.reset();
  AttemptInfo& info = result_.attempts[a.info];
  info.error = error;
  info.http_status = a.status;
  info.lost_race = kind == EndKind::LostRace;
  info.ended_at = since(started_, c_.clock->now());
  info.message = a.message;
  if (kind == EndKind::Normal) apply_health(a);
  if (kind != EndKind::LostRace) last_ended_ = &a;
}

// An attempt's failure. Text already shown ends the request with what came: no second reply follows it.
void RequestRun::fail(Live& a, ErrorClass error, std::string message) {
  end(a, error, std::move(message));
  if (a.forwarded) done_ = result_from(&a, a.error, a.message);
}

void RequestRun::apply_health(Live& a) {
  const std::string& p = a.provider->id;
  if (a.error == ErrorClass::None) {
    c_.health.record_success(p, a.model);
  } else if (counts_in_health(a.error)) {
    if (c_.health.record_failure(p, a.model)) {
      c_.emit(LogLevel::Warn, "provider " + p + ", model " + a.model + ": three failures in a row, cooling for " +
                                  ms_text(kPairCooldown));
    }
  } else if (a.error == ErrorClass::RateLimited) {
    c_.health.rate_limited(p, a.cooldown);
    c_.emit(LogLevel::Warn, "provider " + p + " is rate limited: cooling for " + ms_text(a.cooldown));
  } else if (a.error == ErrorClass::Auth) {
    c_.health.auth_failed(p);
    c_.emit(LogLevel::Warn, "provider " + p + " rejected its key: disabled until the configuration changes");
  }
}

void RequestRun::complete(Live& a) {
  end(a, ErrorClass::None, "");
  if (a.stripped_retry && c_.health.note_full_body_refused(a.provider->id, a.model)) {
    c_.emit(LogLevel::Info, "provider " + a.provider->id + ", model " + a.model +
                                " takes only the stripped body: its requests go without the optional fields from now on");
  }
  for (auto& other : attempts_) {
    if (other->active) end(*other, ErrorClass::Cancelled, "the other request finished first", EndKind::LostRace);
  }
  done_ = result_from(&a, ErrorClass::None, "");
}

void RequestRun::stop_all(ErrorClass error, const std::string& message) {
  for (auto& owned : attempts_) end(*owned, error, message, EndKind::Stopped);
  const Live* shown = winner_ != nullptr ? winner_ : last_ended_;
  done_ = result_from(shown, error, message);
}

ChatResult RequestRun::result_from(const Live* a, ErrorClass error, const std::string& message) {
  ChatResult r = std::move(result_);
  r.error = error;
  r.message = message;
  if (a != nullptr) {
    r.text = a->reply.text();
    r.raw_text = a->reply.raw_text();
    r.finish_reason = a->reply.finish_reason();
    r.provider = a->provider->id;
    r.model = a->model;
    r.served_model = a->reply.served_model();
    r.usage = a->reply.usage();
    r.logprobs = a->reply.logprobs();
    r.used_schema = a->used_schema;
    r.used_logprobs = a->used_logprobs;
    r.streamed = a->forwarded;
  }
  r.total = since(started_, c_.clock->now());
  return r;
}

ChatResult RequestRun::early(ErrorClass error, const std::string& message) {
  if (error != ErrorClass::Busy && error != ErrorClass::GateClosed) {
    c_.emit(LogLevel::Debug, "slot " + slot_name_ + ": " + message);
  }
  return result_from(nullptr, error, message);
}

}  // namespace

Client::Client(Config config, std::shared_ptr<TransportFactory> transport, ClientOptions options)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(transport), std::move(options))) {}

Client::~Client() = default;

ChatResult Client::chat(const ChatRequest& request, StreamSink* sink, std::stop_token stop) {
  return run_chat(request, sink, sink, std::move(stop));
}

ChatResult Client::run_chat(const ChatRequest& request, StreamSink* sink, StreamSink* observer, std::stop_token stop) {
  RequestRun run(*impl_, request, sink, observer, std::move(stop));
  return run.run();
}

void Client::set_gate_open(bool open) { impl_->gate_open.store(open); }

bool Client::gate_open() const { return impl_->gate_open.load(); }

void Client::reload(Config config) {
  const int per_hour = config.requests_per_hour;
  {
    std::lock_guard lock(impl_->mutex);
    impl_->config = std::make_shared<const Config>(std::move(config));
  }
  impl_->budget.set_limit(per_hour);
  impl_->health.clear_disabled();
}

HealthReport Client::health() const { return impl_->health.report(); }

int Client::requests_last_hour() const { return impl_->budget.used(); }

std::shared_ptr<const Config> Client::config() const { return impl_->snapshot(); }

}  // namespace en::llm
