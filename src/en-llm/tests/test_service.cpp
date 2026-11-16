// The asynchronous handle on the fake transport and a manual clock (ARCH §2.2; §1.3's C9, C10 and C15; §11.3):
// events, order, the full queue, cancels, the input epoch, a reply's precedence, one dialogue at a time, Busy, the
// gate, exactly one ending per request, backpressure, the notify callback, shutdown, and Ollama's warm-up. No test
// sleeps for a set time: each waits for what must happen, within a generous bound of real time, and moves the manual
// clock itself.
#include <doctest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "en/llm/service.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using std::chrono::milliseconds;
using std::chrono::minutes;

namespace {

// Real time: the longest a test waits for something that must happen. Only a broken handle comes near it.
constexpr Millis kPatience{60000};

// Ollama on the PC, a llama.cpp server beside it and a cloud provider, each serving one slot (invented models).
constexpr const char* kOllama = R"(
schema = 1

[[provider]]
id = "ollama"
flavor = "ollama"
kind = "local"
base_url = "http://127.0.0.1:11434/v1/"

[[provider]]
id = "llama"
flavor = "llamacpp"
kind = "local"
base_url = "http://127.0.0.1:9931/v1"

[[provider]]
id = "cloud"
base_url = "https://llm.example.invalid/api/v1"
key_ref = "secrets:cloud"

[slots.dialogue]
provider = "ollama"
model = "local-talker"

[slots.bark_live]
provider = "llama"
model = "local-barker"

[slots.memory]
provider = "cloud"
model = "cloud-memory"
)";

bool eventually(const std::function<bool()>& condition) {
  const auto give_up = std::chrono::steady_clock::now() + kPatience;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > give_up) return false;
    std::this_thread::sleep_for(milliseconds(1));
  }
  return true;
}

ChatRequest chat_request(std::string_view slot, Purpose purpose, const std::string& line = "Hello there.") {
  ChatRequest r;
  r.slot = std::string(slot);
  r.purpose = purpose;
  r.messages = {Message::system("You are an invented test speaker.", true), Message::user(line)};
  return r;
}

DecisionCall choice_call(const std::string& rule = "A") {
  DecisionCall call;
  call.task = "next_speaker";
  call.question = "Who speaks next?";
  call.options = {{"A", "the first invented speaker"}, {"B", "the second invented speaker"}, {"none", "no one"}};
  call.rule = [rule] { return rule; };
  return call;
}

FakeScript quick(const std::string& text = "A short line.") { return sse_reply({text}, "stop", Millis(10), Millis(0)); }

// A whole JSON body with its status, as Ollama's native API answers.
FakeScript json_body(int status, const std::string& body, Millis at = Millis(50)) {
  return http_error(status, body, {}, at);
}

// The place of the first event of `kind` for `id`; -1 when there is none.
long index_of(const std::vector<LlmEvent>& events, RequestId id, EventKind kind) {
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (events[i].id == id && events[i].kind == kind) return static_cast<long>(i);
  }
  return -1;
}

struct ServiceRig {
  std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
  std::shared_ptr<FakeServer> server = std::make_shared<FakeServer>(clock);
  mutable std::mutex lines_mutex;
  std::vector<std::string> lines;    // the log
  std::unique_ptr<Service> service;  // after what its threads use, so that it is destroyed first
  std::vector<LlmEvent> seen;        // every event read through next() and poll(), in order

  explicit ServiceRig(const std::string& providers = kCloud, ServiceOptions options = {}) {
    options.client.clock = clock;
    options.client.log = [this](LogLevel, const std::string& line) {
      std::lock_guard lock(lines_mutex);
      lines.push_back(line);
    };
    service = std::make_unique<Service>(make_config(providers), server, std::move(options));
  }

  SubmitResult chat(RequestId id, std::string_view slot, Purpose purpose, std::optional<Epoch> epoch = std::nullopt) {
    return service->submit(LlmRequest::chat(id, chat_request(slot, purpose), epoch));
  }

  SubmitResult decide(RequestId id, std::optional<Epoch> epoch = std::nullopt, const std::string& rule = "A") {
    return service->submit(LlmRequest::decision(id, choice_call(rule), epoch));
  }

  LlmEvent next() {
    std::optional<LlmEvent> e = service->wait(kPatience);
    REQUIRE(e.has_value());
    seen.push_back(*e);
    return *e;
  }

  std::optional<LlmEvent> poll() {
    std::optional<LlmEvent> e = service->poll();
    if (e) seen.push_back(*e);
    return e;
  }

  // The Ended event of `id`, among those read so far or the next ones.
  LlmEvent ended(RequestId id) {
    for (const LlmEvent& e : seen) {
      if (e.kind == EventKind::Ended && e.id == id) return e;
    }
    while (true) {
      LlmEvent e = next();
      if (e.kind == EventKind::Ended && e.id == id) return e;
    }
  }

  int log_count(const std::string& needle) const {
    std::lock_guard lock(lines_mutex);
    int n = 0;
    for (const auto& line : lines) n += line.find(needle) != std::string::npos ? 1 : 0;
    return n;
  }
};

ServiceOptions workers(int n) {
  ServiceOptions options;
  options.workers = n;
  return options;
}

}  // namespace

TEST_CASE("service: a reply's events are its start, its deltas in order, a reset, and one ending") {
  ServiceRig rig;
  rig.server->script("model-a", sse_reply({"Well met", ", traveller."}, "stop", Millis(100), Millis(50)));
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  const LlmEvent started = rig.next();
  CHECK(started.kind == EventKind::Started);
  CHECK(started.id == 1);
  CHECK(started.provider == "cloud");
  CHECK(started.model == "model-a");
  CHECK_FALSE(started.terminal());
  std::string text;
  LlmEvent e = rig.next();
  for (; e.kind == EventKind::Delta; e = rig.next()) text += e.text;
  CHECK(text == "Well met, traveller.");
  REQUIRE(e.kind == EventKind::Ended);
  CHECK(e.id == 1);
  CHECK(e.terminal());
  REQUIRE(e.chat.has_value());
  CHECK_FALSE(e.pick.has_value());
  CHECK(e.chat->ok());
  CHECK(e.chat->text == "Well met, traveller.");
  CHECK(e.error() == ErrorClass::None);
  CHECK_FALSE(rig.service->poll().has_value());
  CHECK(rig.service->stats().running == 0);

  // A reply that only closes its reasoning withdraws what it passed on.
  rig.server->script("model-a", sse_reply({"the model reasons here", " and more", "</think>The reply."}));
  REQUIRE(rig.chat(2, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  std::vector<EventKind> kinds;
  std::string shown;
  for (e = rig.next(); !e.terminal(); e = rig.next()) {
    kinds.push_back(e.kind);
    if (e.kind == EventKind::Delta) shown += e.text;
    if (e.kind == EventKind::Reset) shown.clear();
  }
  CHECK(kinds == std::vector<EventKind>{EventKind::Started, EventKind::Delta, EventKind::Delta, EventKind::Reset,
                                        EventKind::Delta});
  CHECK(shown == "The reply.");
  CHECK(e.chat->text == "The reply.");
}

TEST_CASE("service: a decision call has a start and ends with its pick") {
  ServiceRig rig;
  rig.server->script("model-a", quick(R"({"answer": "B"})"));
  REQUIRE(rig.decide(1) == SubmitResult::Accepted);
  const LlmEvent started = rig.next();
  CHECK(started.kind == EventKind::Started);
  CHECK(started.model == "model-a");
  const LlmEvent e = rig.next();
  REQUIRE(e.kind == EventKind::Ended);
  REQUIRE(e.pick.has_value());
  CHECK_FALSE(e.chat.has_value());
  CHECK(e.pick->key == "B");
  CHECK(e.pick->by == DecidedBy::Model);
  CHECK(e.pick->read == ReadAs::Schema);
  CHECK(e.pick->task == "next_speaker");
  CHECK(e.pick->option_keys == std::vector<std::string>{"A", "B", "none"});
}

TEST_CASE("service: what submit refuses, and the pool's size") {
  ServiceRig rig;
  CHECK(rig.service->stats().workers == 3);  // one cloud provider of two connections, plus one
  CHECK(ServiceRig(kLocal).service->stats().workers == 2);
  CHECK(ServiceRig(kTwoClouds).service->stats().workers == 5);
  CHECK(ServiceRig(kCloud, workers(1)).service->stats().workers == 1);

  CHECK(rig.chat(0, kDialogue, Purpose::Reply) == SubmitResult::Invalid);
  ChatRequest empty = chat_request(kDialogue, Purpose::Reply);
  empty.messages.clear();
  CHECK(rig.service->submit(LlmRequest::chat(1, empty)) == SubmitResult::Invalid);
  CHECK(rig.chat(1, kDialogue, Purpose::OwnInitiative) == SubmitResult::Invalid);  // without its epoch
  CHECK(rig.chat(1, kDialogue, Purpose::Reply, Epoch{3}) == SubmitResult::Invalid);
  DecisionCall no_rule = choice_call();
  no_rule.rule = nullptr;
  CHECK(rig.service->submit(LlmRequest::decision(1, no_rule)) == SubmitResult::Invalid);
  CHECK(rig.decide(1, std::nullopt, "Z") == SubmitResult::Invalid);  // a key the call does not offer
  DecisionCall failing = choice_call();
  failing.rule = []() -> std::string { throw std::runtime_error("an invented failure"); };
  CHECK(rig.service->submit(LlmRequest::decision(1, failing)) == SubmitResult::Invalid);
  CHECK(rig.log_count("refused") == 7);
  CHECK(rig.log_count("invented failure") == 0);
  CHECK(rig.server->requests().empty());
  CHECK_FALSE(rig.service->poll().has_value());  // nothing refused has an event

  // A live id is refused; once its ending has been read, it may be used again.
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(7, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  CHECK(rig.chat(7, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Duplicate);
  REQUIRE(rig.next().kind == EventKind::Started);  // sent, so that it has taken its script
  REQUIRE(rig.service->cancel(7));
  CHECK(rig.ended(7).error() == ErrorClass::Cancelled);
  rig.seen.clear();
  rig.server->script("model-a", quick());
  rig.server->set_auto_advance(true);
  REQUIRE(rig.chat(7, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  CHECK(rig.ended(7).chat->ok());

  rig.service->shutdown();
  CHECK(rig.chat(8, kDialogue, Purpose::Reply) == SubmitResult::ShutDown);
  CHECK_FALSE(rig.service->warm_up(kDialogue).has_value());
}

TEST_CASE("service: requests start by priority, first come first served within one") {
  ServiceRig rig(kCloud, workers(1));
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(100, kSelftest, Purpose::Reply) == SubmitResult::Accepted);  // holds the only worker
  REQUIRE(rig.next().kind == EventKind::Started);
  for (int i = 0; i < 8; ++i) rig.server->script("model-a", quick(R"({"answer": "A"})"));
  // The reply comes before the own-initiative work, so it cancels none of it.
  REQUIRE(rig.chat(1, kBarkPool, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.chat(3, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(rig.chat(4, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);
  REQUIRE(rig.chat(5, kDialogue, Purpose::OwnInitiative, Epoch{1}) == SubmitResult::Accepted);
  REQUIRE(rig.decide(6, Epoch{1}) == SubmitResult::Accepted);
  REQUIRE(rig.chat(7, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.chat(8, kBarkPool, Purpose::Background) == SubmitResult::Accepted);
  CHECK(rig.service->stats().queued == 8);
  REQUIRE(rig.service->cancel(100));
  rig.server->set_auto_advance(true);
  for (RequestId id = 1; id <= 8; ++id) CHECK(rig.ended(id).error() == ErrorClass::None);
  std::vector<RequestId> order;
  for (const LlmEvent& e : rig.seen) {
    if (e.kind == EventKind::Started && e.id != 100) order.push_back(e.id);
  }
  CHECK(order == std::vector<RequestId>{3, 5, 6, 4, 2, 7, 1, 8});
}

TEST_CASE("service: a full queue gives a bark_pool job's place away as Dropped and a memory job's as Preempted") {
  ServiceOptions options = workers(1);
  options.queue_capacity = 4;
  ServiceRig rig(kCloud, options);
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(100, kSelftest, Purpose::Reply) == SubmitResult::Accepted);  // running: it counts
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(1, kBarkPool, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.chat(3, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);
  // Full: a memory job outranks the pool job, which ends at once, dropped.
  REQUIRE(rig.chat(4, kMemory, Purpose::Background) == SubmitResult::Accepted);
  std::optional<LlmEvent> e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 1);
  CHECK(e->error() == ErrorClass::Dropped);
  CHECK(e->chat->provider.empty());
  // A pool job outranks nothing queued now.
  CHECK(rig.chat(5, kBarkPool, Purpose::Background) == SubmitResult::Full);
  // A reply takes the newest memory job's place; it ends as Preempted, so that the core requeues it.
  REQUIRE(rig.chat(6, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 4);
  CHECK(e->error() == ErrorClass::Preempted);
  REQUIRE(rig.chat(7, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);
  e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 2);
  CHECK(e->error() == ErrorClass::Preempted);
  // Only a reply and live requests are left, and nothing else gives way, not even to a reply.
  CHECK(rig.chat(8, kDialogue, Purpose::Reply) == SubmitResult::Full);
  CHECK_FALSE(rig.poll().has_value());
  CHECK(rig.service->stats().queued == 3);
  CHECK(rig.server->requests().size() == 1);
  CHECK(eventually([&] { return rig.log_count("gives way to a higher-priority one") == 3; }));

  rig.service->shutdown();
  std::vector<LlmEvent> rest;
  rig.service->drain(rest);
  std::set<RequestId> ids;
  for (const LlmEvent& end : rest) {
    CHECK(end.kind == EventKind::Ended);
    CHECK(end.error() == ErrorClass::Cancelled);
    ids.insert(end.id);
  }
  CHECK(ids == std::set<RequestId>{100, 3, 6, 7});
}

TEST_CASE("service: cancelling a queued request ends it at once; a running one ends once the client has stopped") {
  ServiceRig rig(kCloud, workers(1));
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.decide(3) == SubmitResult::Accepted);
  CHECK(rig.service->cancel(2));
  std::optional<LlmEvent> e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 2);
  CHECK(e->kind == EventKind::Ended);
  CHECK(e->error() == ErrorClass::Cancelled);
  CHECK_FALSE(rig.service->cancel(2));  // it has ended
  CHECK(rig.service->cancel(3));
  e = rig.poll();
  REQUIRE(e.has_value());
  REQUIRE(e->pick.has_value());
  CHECK(e->pick->key == "A");  // its rules
  CHECK(e->pick->by == DecidedBy::Rules);
  CHECK(e->pick->error == ErrorClass::Cancelled);
  CHECK(e->pick->prompt_hash.size() == 16);
  CHECK(rig.service->cancel(1));
  CHECK(rig.ended(1).error() == ErrorClass::Cancelled);
  CHECK(rig.server->requests().size() == 1);
  CHECK(rig.server->requests()[0].cancelled);
  CHECK_FALSE(rig.service->cancel(1));
  CHECK_FALSE(rig.service->cancel(99));
}

TEST_CASE("service: once a cancel has returned, nothing of the request but its ending is queued") {
  ServiceOptions options;
  options.event_capacity = 1;
  ServiceRig rig(kCloud, options);
  rig.server->script("model-a", sse_reply({"one", " two", " three"}, "stop", Millis(10), Millis(10)));
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(eventually([&] { return rig.service->stats().events == 1; }));  // its start fills the queue
  REQUIRE(rig.service->cancel(1));
  CHECK(rig.next().kind == EventKind::Started);
  const LlmEvent e = rig.next();
  CHECK(e.kind == EventKind::Ended);
  CHECK(e.error() == ErrorClass::Cancelled);
  CHECK_FALSE(rig.service->poll().has_value());
}

TEST_CASE("service: the input epoch cancels own-initiative work made before it, queued or running") {
  ServiceRig rig;  // the cloud provider takes two requests at once
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::OwnInitiative, Epoch{3}) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.decide(2, Epoch{3}) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(3, kDialogue, Purpose::OwnInitiative, Epoch{5}) == SubmitResult::Accepted);  // one dialogue at a time
  REQUIRE(rig.chat(4, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);  // no epoch; the provider is full
  REQUIRE(rig.chat(5, kMemory, Purpose::Background, Epoch{2}) == SubmitResult::Accepted);  // whatever carries one
  rig.server->script("model-a", quick());  // for 3 and 4, which start once 1 and 2 have ended
  rig.server->script("model-a", quick());
  CHECK(rig.service->cancel_before_epoch(4) == 3);
  CHECK(rig.service->cancel_before_epoch(4) == 0);
  const LlmEvent memory = rig.ended(5);
  CHECK(memory.error() == ErrorClass::Cancelled);
  CHECK(memory.chat->message == "the input epoch moved on");
  CHECK(rig.ended(1).error() == ErrorClass::Cancelled);
  const LlmEvent pick = rig.ended(2);
  REQUIRE(pick.pick.has_value());
  CHECK(pick.pick->by == DecidedBy::Rules);
  CHECK(pick.pick->error == ErrorClass::Cancelled);
  rig.server->set_auto_advance(true);
  CHECK(rig.ended(3).chat->ok());
  CHECK(rig.ended(4).chat->ok());
}

TEST_CASE("service: a reply cancels own-initiative dialogue and its decision call, and starts once they have ended") {
  ServiceRig rig(kLocal);  // one request at a time
  rig.server->set_auto_advance(false);
  rig.server->script("local-model", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::OwnInitiative, Epoch{1}) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.decide(2, Epoch{1}) == SubmitResult::Accepted);
  REQUIRE(rig.chat(3, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);  // not dialogue: it stays
  REQUIRE(eventually([&] { return rig.log_count("waits for another request to end") == 2; }));
  rig.server->script("local-model", quick("The reply."));
  rig.server->script("local-model", quick("A call-out."));
  REQUIRE(rig.chat(4, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  // The queued decision call ended at once, on its rules.
  const std::optional<LlmEvent> pick = rig.poll();
  REQUIRE(pick.has_value());
  CHECK(pick->id == 2);
  REQUIRE(pick->pick.has_value());
  CHECK(pick->pick->by == DecidedBy::Rules);
  CHECK(pick->pick->error == ErrorClass::Cancelled);
  const LlmEvent own = rig.ended(1);
  CHECK(own.error() == ErrorClass::Cancelled);
  CHECK(own.chat->message == "a reply comes first: own-initiative work gives way");
  rig.server->set_auto_advance(true);
  CHECK(rig.ended(4).chat->ok());
  CHECK(rig.ended(3).chat->ok());
  CHECK(index_of(rig.seen, 1, EventKind::Ended) < index_of(rig.seen, 4, EventKind::Started));
}

TEST_CASE("service: one dialogue request in flight, and own-initiative dialogue waits for a reply") {
  ServiceRig rig;  // three workers; the cloud provider takes two requests
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(2, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(rig.chat(3, kMemory, Purpose::Background) == SubmitResult::Accepted);
  // The memory job starts, so the second reply, which outranks it, was passed over.
  const LlmEvent started = rig.next();
  CHECK(started.kind == EventKind::Started);
  CHECK(started.id == 3);
  CHECK(rig.service->stats().queued == 1);
  REQUIRE(rig.chat(4, kDialogue, Purpose::OwnInitiative, Epoch{1}) == SubmitResult::Accepted);
  REQUIRE(rig.service->cancel(3));
  CHECK(rig.ended(3).error() == ErrorClass::Cancelled);
  rig.server->script("model-a", quick("The second reply."));
  rig.server->script("model-a", quick("A line of its own."));
  REQUIRE(rig.service->cancel(1));
  rig.server->set_auto_advance(true);
  CHECK(rig.ended(2).chat->ok());
  CHECK(rig.ended(4).chat->ok());
  CHECK(index_of(rig.seen, 1, EventKind::Ended) < index_of(rig.seen, 2, EventKind::Started));
  CHECK(index_of(rig.seen, 2, EventKind::Ended) < index_of(rig.seen, 4, EventKind::Started));
  CHECK(rig.server->requests().size() == 4);
}

TEST_CASE("service: a Busy request waits, and is tried again once another request has ended or the config changes") {
  ServiceRig rig(kLocal);
  rig.server->set_auto_advance(false);
  rig.server->script("local-model", hang());
  REQUIRE(rig.chat(1, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);  // nothing gives way to it
  REQUIRE(eventually([&] { return rig.log_count("waits for another request to end") == 1; }));
  CHECK(rig.server->requests().size() == 1);  // Busy sends nothing
  rig.server->script("local-model", quick("A journal entry."));
  REQUIRE(rig.service->cancel(1));
  rig.server->set_auto_advance(true);
  CHECK(rig.ended(2).chat->ok());
  CHECK(rig.server->requests().size() == 2);
  CHECK(index_of(rig.seen, 1, EventKind::Ended) < index_of(rig.seen, 2, EventKind::Started));

  // A reload that gives the provider a second connection lets the waiting job start beside the running one.
  rig.server->set_auto_advance(false);
  rig.server->script("local-model", hang());
  rig.server->script("local-model", hang());
  REQUIRE(rig.chat(3, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(4, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(eventually([&] { return rig.log_count("waits for another request to end") == 2; }));
  std::string wider = kLocal;
  wider.insert(wider.find("kind = \"local\"\n") + 15, "max_concurrency = 2\n");
  rig.service->reload(make_config(wider));
  REQUIRE(eventually([&] { return rig.server->active() == 2; }));
}

TEST_CASE("service: a live request that cannot start within its slot's first-token time ends as Busy; others wait") {
  ServiceRig rig(kLocal);
  rig.server->set_auto_advance(false);
  rig.server->script("local-model", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);  // nothing takes the provider from it
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(2, kBarkLive, Purpose::OwnInitiative) == SubmitResult::Accepted);  // 1.5 s
  REQUIRE(rig.decide(3) == SubmitResult::Accepted);                                   // 3 s on a local provider
  REQUIRE(rig.chat(4, kMemory, Purpose::Background) == SubmitResult::Accepted);       // no start time
  REQUIRE(eventually([&] { return rig.log_count("waits for another request to end") == 3; }));
  rig.clock->advance(milliseconds(1500));
  const LlmEvent bark = rig.ended(2);
  CHECK(bark.error() == ErrorClass::Busy);
  CHECK(bark.chat->message.find("not started within 1500 ms") != std::string::npos);
  CHECK(bark.chat->total == Millis(1500));
  rig.clock->advance(milliseconds(1500));
  const LlmEvent pick = rig.ended(3);
  REQUIRE(pick.pick.has_value());
  CHECK(pick.pick->key == "A");
  CHECK(pick.pick->by == DecidedBy::Rules);
  CHECK(pick.pick->error == ErrorClass::Busy);
  CHECK(pick.pick->latency == Millis(3000));
  CHECK(rig.service->stats().queued == 1);  // the memory job waits on
  CHECK(rig.server->requests().size() == 1);
  rig.server->script("local-model", quick());
  REQUIRE(rig.service->cancel(1));
  rig.server->set_auto_advance(true);
  CHECK(rig.ended(4).chat->ok());
}

TEST_CASE("service: closing the gate ends what waits at once and stops what runs; new requests end at once") {
  ServiceRig rig(kCloud, workers(1));
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.decide(3) == SubmitResult::Accepted);
  rig.service->set_gate_open(false);
  CHECK_FALSE(rig.service->gate_open());
  CHECK_FALSE(rig.service->client().gate_open());
  std::optional<LlmEvent> e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 2);
  CHECK(e->error() == ErrorClass::GateClosed);
  e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 3);
  REQUIRE(e->pick.has_value());
  CHECK(e->pick->error == ErrorClass::GateClosed);
  CHECK(e->pick->key == "A");
  CHECK(rig.ended(1).error() == ErrorClass::GateClosed);
  CHECK(rig.server->requests()[0].cancelled);
  REQUIRE(rig.chat(4, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  e = rig.poll();
  REQUIRE(e.has_value());
  CHECK(e->id == 4);
  CHECK(e->error() == ErrorClass::GateClosed);
  CHECK(rig.server->requests().size() == 1);
  rig.service->set_gate_open(true);
  rig.server->script("model-a", quick());
  rig.server->set_auto_advance(true);
  REQUIRE(rig.chat(5, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  CHECK(rig.ended(5).chat->ok());
}

TEST_CASE("service: exactly one ending per accepted request, under a seeded random mix of submits and cancels") {
  ServiceOptions options;
  options.queue_capacity = 10;
  options.event_capacity = 8;
  ServiceRig rig(kTwoClouds, options);
  std::mt19937 random(20261116u);
  const auto pick = [&](std::size_t n) { return static_cast<std::size_t>(random() % n); };
  for (int i = 0; i < 400; ++i) {
    switch (pick(5)) {
      case 0: rig.server->script("*", hang()); break;
      case 1: rig.server->script("*", http_error(500, R"({"error": {"message": "an invented failure"}})")); break;
      case 2: rig.server->script("*", connection_failure()); break;
      default: rig.server->script("*", sse_reply({"An invented", " line."}, "stop", Millis(20), Millis(30))); break;
    }
  }
  std::set<RequestId> accepted;
  std::vector<RequestId> submitted;
  std::vector<LlmEvent> events;
  Epoch epoch = 1;
  RequestId next = 1;
  for (int op = 0; op < 300; ++op) {
    const std::size_t r = pick(100);
    if (r < 50) {
      const RequestId id = next++;
      submitted.push_back(id);
      SubmitResult result = SubmitResult::Invalid;
      switch (pick(6)) {
        case 0: result = rig.chat(id, kDialogue, Purpose::Reply); break;
        case 1: result = rig.chat(id, kDialogue, Purpose::OwnInitiative, epoch); break;
        case 2: result = rig.chat(id, kBarkLive, Purpose::OwnInitiative); break;
        case 3: result = rig.chat(id, kMemory, Purpose::Background); break;
        case 4: result = rig.chat(id, kBarkPool, Purpose::Background); break;
        default: result = rig.decide(id, pick(2) == 0 ? std::optional<Epoch>(epoch) : std::nullopt); break;
      }
      if (result == SubmitResult::Accepted) accepted.insert(id);
    } else if (r < 75) {
      if (!submitted.empty()) rig.service->cancel(submitted[pick(submitted.size())]);
    } else if (r < 82) {
      rig.service->cancel_before_epoch(++epoch);
    } else if (r < 86) {
      rig.service->set_gate_open(false);
      rig.service->set_gate_open(true);
    } else if (r < 94) {
      rig.service->drain(events);
    } else {
      rig.clock->advance(milliseconds(pick(4000)));
    }
  }
  rig.service->shutdown();
  rig.service->drain(events);
  CHECK_FALSE(accepted.empty());

  std::map<RequestId, std::vector<EventKind>> kinds;
  for (const LlmEvent& e : events) kinds[e.id].push_back(e.kind);
  for (const auto& entry : kinds) {
    const RequestId id = entry.first;
    INFO("request " << id);
    CHECK(accepted.count(id) == 1);  // nothing refused has an event
  }
  for (const RequestId id : accepted) {
    INFO("request " << id);
    const std::vector<EventKind>& list = kinds[id];
    REQUIRE_FALSE(list.empty());
    CHECK(std::count(list.begin(), list.end(), EventKind::Ended) == 1);
    CHECK(list.back() == EventKind::Ended);
    CHECK(std::count(list.begin(), list.end(), EventKind::Started) <= 1);
    if (list.size() > 1) CHECK(list.front() == EventKind::Started);  // no text before its start
  }
  const ServiceStats stats = rig.service->stats();
  CHECK(stats.queued == 0);
  CHECK(stats.running == 0);
}

TEST_CASE("service: backpressure with a tiny event queue: every delta arrives, in order") {
  ServiceOptions options;
  options.event_capacity = 2;
  ServiceRig rig(kCloud, options);
  std::vector<std::string> pieces;
  std::string whole;
  for (int i = 0; i < 40; ++i) {
    pieces.push_back("w" + std::to_string(i) + ".");
    whole += pieces.back();
  }
  rig.server->script("model-a", sse_reply(pieces, "stop", Millis(10), Millis(5)));
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  std::vector<std::string> deltas;
  std::size_t most = 0;
  while (true) {
    // Each time, the producer fills the queue, or has finished.
    REQUIRE(eventually([&] {
      const ServiceStats s = rig.service->stats();
      most = std::max(most, s.events);
      return s.events >= 2 || s.running == 0;
    }));
    const LlmEvent e = rig.next();
    if (e.kind == EventKind::Delta) deltas.push_back(e.text);
    if (e.terminal()) {
      CHECK(e.chat->ok());
      CHECK(e.chat->text == whole);
      break;
    }
  }
  CHECK(deltas == pieces);
  CHECK(most <= 3);  // the capacity, and the ending's place beyond it
}

TEST_CASE("service: the notify callback runs on the handle's own thread when the event queue stops being empty") {
  ServiceRig rig(kCloud, workers(1));
  std::mutex m;
  std::condition_variable cv;
  int calls = 0;
  bool on_caller = false;
  const std::thread::id test_thread = std::this_thread::get_id();
  rig.service->set_notify([&] {
    std::lock_guard lock(m);
    ++calls;
    on_caller = on_caller || std::this_thread::get_id() == test_thread;
    cv.notify_all();
  });
  const auto calls_reach = [&](int n) {
    std::unique_lock lock(m);
    return cv.wait_for(lock, kPatience, [&] { return calls >= n; });
  };
  rig.server->set_auto_advance(false);
  rig.server->script("model-a", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(calls_reach(1));  // its start
  CHECK(rig.next().kind == EventKind::Started);
  // An ending the caller's own call queues: the callback still runs on the handle's thread.
  REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);
  REQUIRE(rig.service->cancel(2));
  REQUIRE(calls_reach(2));
  CHECK(rig.next().id == 2);
  // Once replaced, the first callback is not called again.
  int later = 0;
  rig.service->set_notify([&] { ++later; });
  REQUIRE(rig.service->cancel(1));
  CHECK(rig.ended(1).error() == ErrorClass::Cancelled);
  rig.service->shutdown();  // joins the handle's threads: nothing is called after it
  std::lock_guard lock(m);
  CHECK(calls == 2);
  CHECK_FALSE(on_caller);
  CHECK(later == 1);
}

TEST_CASE("service: shutdown ends queued and running requests, and the destructor never blocks on a full queue") {
  {
    ServiceRig rig(kCloud, workers(1));
    rig.server->set_auto_advance(false);
    rig.server->script("model-a", hang());
    REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
    REQUIRE(rig.next().kind == EventKind::Started);
    REQUIRE(rig.chat(2, kMemory, Purpose::Background) == SubmitResult::Accepted);
    REQUIRE(rig.decide(3) == SubmitResult::Accepted);
    rig.service->shutdown();
    std::vector<LlmEvent> rest;
    rig.service->drain(rest);
    REQUIRE(rest.size() == 3);
    std::set<RequestId> ids;
    for (const LlmEvent& e : rest) {
      CHECK(e.kind == EventKind::Ended);
      CHECK(e.error() == ErrorClass::Cancelled);
      ids.insert(e.id);
      if (e.pick) CHECK(e.pick->key == "A");
    }
    CHECK(ids == std::set<RequestId>{1, 2, 3});
    CHECK(rig.server->requests()[0].cancelled);
    const ServiceStats stats = rig.service->stats();
    CHECK(stats.queued == 0);
    CHECK(stats.running == 0);
    CHECK(rig.chat(4, kDialogue, Purpose::Reply) == SubmitResult::ShutDown);
    const auto before = std::chrono::steady_clock::now();
    CHECK_FALSE(rig.service->wait(kPatience).has_value());  // at once: nothing is left
    CHECK(std::chrono::steady_clock::now() - before < kPatience / 2);
    rig.service->shutdown();  // again: nothing happens
  }
  {
    // A reply whose deltas fill the event queue, and no one reads: the destructor still returns.
    ServiceOptions options;
    options.event_capacity = 1;
    ServiceRig rig(kCloud, options);
    rig.server->script("model-a", sse_reply({"one", " two", " three", " four"}, "stop", Millis(10), Millis(10)));
    REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
    REQUIRE(eventually([&] { return rig.service->stats().events == 1; }));
    rig.service.reset();
    CHECK(rig.server->requests()[0].cancelled);
  }
}

TEST_CASE("service: Ollama's warm-up sends the model and keep_alive to its native API, and ends with one event") {
  ServiceRig rig(kOllama);
  rig.server->script("local-talker", json_body(200, R"({"model": "local-talker", "done": true})", Millis(80)));
  const std::optional<RequestId> id = rig.service->warm_up(kDialogue);
  REQUIRE(id.has_value());
  const LlmEvent e = rig.next();
  CHECK(e.kind == EventKind::WarmUpEnded);
  CHECK(e.terminal());
  CHECK(e.id == *id);
  CHECK(e.provider == "ollama");
  CHECK(e.model == "local-talker");
  CHECK(e.warm_up.ok);
  CHECK(e.warm_up.status == 200);
  CHECK(e.warm_up.time == Millis(80));
  CHECK(e.error() == ErrorClass::None);
  const std::vector<Recorded> sent = rig.server->requests();
  REQUIRE(sent.size() == 1);
  CHECK(sent[0].method == "POST");
  CHECK(sent[0].url == "http://127.0.0.1:11434/api/generate");
  CHECK(sent[0].body == json{{"model", "local-talker"}, {"keep_alive", "10m"}});  // no prompt
  CHECK(std::any_of(sent[0].headers.begin(), sent[0].headers.end(), [](const HttpHeader& h) {
    return h.name == "Content-Type" && h.value == "application/json";
  }));
  CHECK(sent[0].authorization.empty());
  CHECK(rig.service->client().requests_last_hour() == 0);
}

TEST_CASE("service: the warm-up acts only for an idle local Ollama provider, with the gate open") {
  ServiceRig rig(kOllama);
  CHECK_FALSE(rig.service->warm_up(kBarkLive).has_value());  // llama.cpp
  CHECK_FALSE(rig.service->warm_up(kMemory).has_value());    // cloud
  rig.service->set_gate_open(false);
  CHECK_FALSE(rig.service->warm_up(kDialogue).has_value());
  rig.service->set_gate_open(true);

  // Not while a warm-up for the provider is in flight; one gives up after two minutes.
  rig.server->set_auto_advance(false);
  rig.server->script("local-talker", hang());
  const std::optional<RequestId> first = rig.service->warm_up("an-unknown-slot");  // falls through to dialogue
  REQUIRE(first.has_value());
  REQUIRE(eventually([&] { return rig.server->active() == 1; }));
  CHECK_FALSE(rig.service->warm_up(kDialogue).has_value());
  rig.clock->advance(minutes(2));
  LlmEvent e = rig.next();
  CHECK(e.id == *first);
  CHECK_FALSE(e.warm_up.ok);
  CHECK(e.warm_up.error == ErrorClass::TotalTimeout);
  CHECK(rig.server->requests()[0].cancelled);

  // Not while the provider serves a request, nor for 4 minutes after it served one.
  rig.server->script("local-talker", hang());
  REQUIRE(rig.chat(1, kDialogue, Purpose::Reply) == SubmitResult::Accepted);
  REQUIRE(rig.next().kind == EventKind::Started);
  CHECK_FALSE(rig.service->warm_up(kDialogue).has_value());
  REQUIRE(rig.service->cancel(1));
  CHECK(rig.ended(1).error() == ErrorClass::Cancelled);
  rig.clock->advance(minutes(4));
  CHECK_FALSE(rig.service->warm_up(kDialogue).has_value());
  rig.clock->advance(milliseconds(1));
  rig.server->script("local-talker", json_body(500, R"({"error": "an invented failure"})"));
  rig.server->set_auto_advance(true);
  const std::optional<RequestId> second = rig.service->warm_up(kDialogue);
  REQUIRE(second.has_value());
  e = rig.next();
  CHECK(e.id == *second);
  CHECK_FALSE(e.warm_up.ok);
  CHECK(e.warm_up.status == 500);
  CHECK(e.warm_up.error == ErrorClass::Server);

  // A failed warm-up counts in neither health nor the hour, and not as serving: the next one goes at once.
  for (const HealthReport::Pair& pair : rig.service->client().health().pairs) CHECK(pair.failures == 0);
  CHECK(rig.service->client().requests_last_hour() == 1);  // the reply
  rig.server->script("local-talker", json_body(200, R"({"done": true})"));
  const std::optional<RequestId> third = rig.service->warm_up(kDialogue);
  REQUIRE(third.has_value());
  CHECK(rig.next().warm_up.ok);
  // One that succeeded counts as serving: no other for 4 minutes.
  CHECK_FALSE(rig.service->warm_up(kDialogue).has_value());
  rig.clock->advance(minutes(4) + milliseconds(1));
  rig.server->script("local-talker", json_body(200, R"({"done": true})"));
  const std::optional<RequestId> fourth = rig.service->warm_up(kDialogue);
  REQUIRE(fourth.has_value());
  CHECK(*first < *second);
  CHECK(*third < *fourth);
  CHECK(rig.next().warm_up.ok);
  CHECK(rig.service->client().requests_last_hour() == 1);
}

TEST_CASE("service: a warm-up in flight ends when the gate closes, or when the handle shuts down") {
  ServiceRig rig(kOllama);
  rig.server->set_auto_advance(false);
  rig.server->script("local-talker", hang());
  const std::optional<RequestId> a = rig.service->warm_up(kDialogue);
  REQUIRE(a.has_value());
  REQUIRE(eventually([&] { return rig.server->active() == 1; }));
  rig.service->set_gate_open(false);
  const LlmEvent closed = rig.next();
  CHECK(closed.id == *a);
  CHECK(closed.warm_up.error == ErrorClass::GateClosed);
  rig.service->set_gate_open(true);
  rig.server->script("local-talker", hang());
  const std::optional<RequestId> b = rig.service->warm_up(kDialogue);
  REQUIRE(b.has_value());
  REQUIRE(eventually([&] { return rig.server->active() == 1; }));
  rig.service->shutdown();
  std::vector<LlmEvent> rest;
  rig.service->drain(rest);
  REQUIRE(rest.size() == 1);
  CHECK(rest[0].kind == EventKind::WarmUpEnded);
  CHECK(rest[0].id == *b);
  CHECK(rest[0].warm_up.error == ErrorClass::Cancelled);
}
