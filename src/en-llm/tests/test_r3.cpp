// R3 (DESIGN §2.6, ARCH §1.2): every entry from foreign code is noexcept and starts with ffi_guard. Here: threads
// started with a stack of the size asked (ARCH §10.2), whose body's throw trips their component and ends only the
// thread; the handle's threads on 2 MB stacks; a worker's request whose client throws, which trips the worker's
// component and ends as BadRequest; and the caller's own callbacks throwing on the handle's threads, which lose that
// call only. The transports' callbacks are in test_transport.cpp. All data is invented.
#include <doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "en/llm/service.hpp"
#include "en/llm/thread.hpp"
#include "en/proto/ffi_guard.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using en::proto::Component;

namespace {

constexpr Millis kPatience{60000};  // real time: only a broken handle comes near it

// Where the system can say, the stack is at least the size asked: Windows reserves exactly that, while glibc may hand
// a new thread the cached stack of one that has ended, up to four times the size. Elsewhere there is nothing to check.
void check_stack(std::optional<std::size_t> size, std::size_t asked) {
  if (!size) {
    MESSAGE("this system does not report a thread's stack size");
    return;
  }
  CHECK(*size >= asked);
}

std::optional<std::size_t> stack_of_thread(std::size_t asked) {
  std::optional<std::size_t> seen;
  StackThread thread(Component::LlmWorker, asked, [&] { seen = current_stack_size(); });
  thread.join();
  return seen;
}

ChatRequest a_chat() {
  ChatRequest r;
  r.messages = {Message::system("You are an invented test speaker.", true), Message::user("Hello there.")};
  return r;
}

// The fake server's transports, with what the threads that open them show: each one's stack, and on the first
// `throws` opens an exception, as a fault inside the client would throw on the worker.
class Watched final : public TransportFactory {
 public:
  Watched(std::shared_ptr<FakeServer> server, int throws) : server_(std::move(server)), throws_(throws) {}

  std::unique_ptr<Transport> open() override {
    {
      std::lock_guard lock(mutex_);
      stacks_.push_back(current_stack_size());
      if (throws_ > 0) {
        --throws_;
        throw std::runtime_error("an invented fault inside the client");
      }
    }
    return server_->open();
  }

  std::vector<std::optional<std::size_t>> stacks() const {
    std::lock_guard lock(mutex_);
    return stacks_;
  }

 private:
  std::shared_ptr<FakeServer> server_;
  mutable std::mutex mutex_;
  int throws_;
  std::vector<std::optional<std::size_t>> stacks_;
};

struct Handle {
  std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
  std::shared_ptr<FakeServer> server = std::make_shared<FakeServer>(clock);
  std::shared_ptr<Watched> watched;
  std::atomic<int> log_calls{0};
  std::unique_ptr<Service> service;

  explicit Handle(int throws = 0, bool log_throws = false) : watched(std::make_shared<Watched>(server, throws)) {
    ServiceOptions options;
    options.client.clock = clock;
    options.client.log = [this, log_throws](LogLevel, const std::string&) {
      ++log_calls;
      if (log_throws) throw std::runtime_error("an invented fault in the caller's log");
    };
    options.workers = 2;
    service = std::make_unique<Service>(make_config(kCloud), watched, std::move(options));
  }

  LlmEvent ended(RequestId id) {
    while (true) {
      std::optional<LlmEvent> e = service->wait(kPatience);
      REQUIRE(e.has_value());
      if (e->kind == EventKind::Ended && e->id == id) return *e;
    }
  }
};

}  // namespace

TEST_CASE("R3: a thread starts with the stack asked, and its body's throw trips its component and ends only it") {
  en::proto::clear_trip(Component::LlmWorker);
  check_stack(stack_of_thread(kNetStack), 2u << 20);
  // Past both systems' own defaults (8 MB for glibc's threads, 2 MB for this program's on Windows): the size is applied.
  check_stack(stack_of_thread(std::size_t{12} << 20), 12u << 20);

  en::proto::clear_trip(Component::LlmTransport);
  std::atomic<bool> ran{false};
  StackThread thrower(Component::LlmTransport, kNetStack, [&] {
    ran = true;
    throw std::runtime_error("an invented fault on a thread");
  });
  CHECK(thrower.joinable());
  thrower.join();
  CHECK_FALSE(thrower.joinable());
  CHECK(ran);
  CHECK(en::proto::tripped(Component::LlmTransport));
  en::proto::clear_trip(Component::LlmTransport);

  // Moved, it is still one thread; detached, it runs on and is no longer joinable; destroyed joinable, it is joined.
  std::atomic<int> runs{0};
  StackThread first(Component::LlmWorker, kNetStack, [&] { ++runs; });
  StackThread second(std::move(first));
  CHECK_FALSE(first.joinable());
  CHECK(second.joinable());
  second.join();
  std::mutex m;
  std::condition_variable cv;
  bool done = false;
  StackThread detached(Component::LlmWorker, kNetStack, [&] {
    std::lock_guard lock(m);
    done = true;
    cv.notify_all();
  });
  detached.detach();
  CHECK_FALSE(detached.joinable());
  {
    std::unique_lock lock(m);
    CHECK(cv.wait_for(lock, kPatience, [&] { return done; }));
  }
  { StackThread joined_at_end(Component::LlmWorker, kNetStack, [&] { ++runs; }); }
  CHECK(runs == 2);
  CHECK_FALSE(en::proto::tripped(Component::LlmWorker));
}

TEST_CASE("R3: the handle's workers and its dispatcher run on 2 MB stacks") {
  // The callback's state outlives the handle: its dispatcher may call the callback again until it has shut down.
  std::mutex m;
  std::condition_variable cv;
  std::optional<std::size_t> dispatcher_stack;
  bool notified = false;
  Handle h;
  h.service->set_notify([&] {
    std::lock_guard lock(m);
    dispatcher_stack = current_stack_size();
    notified = true;
    cv.notify_all();
  });
  h.server->script("model-a", sse_reply({"A short line."}));
  REQUIRE(h.service->submit(LlmRequest::chat(1, a_chat())) == SubmitResult::Accepted);
  CHECK(h.ended(1).chat->ok());
  {
    std::unique_lock lock(m);
    REQUIRE(cv.wait_for(lock, kPatience, [&] { return notified; }));
  }
  const auto stacks = h.watched->stacks();
  REQUIRE(stacks.size() == 1);
  check_stack(stacks[0], kNetStack);  // the worker that opened the transport
  std::lock_guard lock(m);
  check_stack(dispatcher_stack, kNetStack);
}

TEST_CASE("R3: a worker's request whose client throws trips the worker's component and ends as BadRequest") {
  en::proto::clear_trip(Component::LlmWorker);
  Handle h(/*throws=*/1);
  REQUIRE(h.service->submit(LlmRequest::chat(7, a_chat())) == SubmitResult::Accepted);
  const LlmEvent first = h.ended(7);
  REQUIRE(first.chat.has_value());
  CHECK(first.chat->error == ErrorClass::BadRequest);
  CHECK(first.chat->message == "the request could not be sent");
  CHECK(en::proto::tripped(Component::LlmWorker));
  CHECK(h.service->stats().workers == 2);

  // The worker goes on: the next request is served as usual, and a decision call that hits a fault takes its rules.
  h.server->script("model-a", sse_reply({"Served after the fault."}));
  REQUIRE(h.service->submit(LlmRequest::chat(8, a_chat())) == SubmitResult::Accepted);
  const LlmEvent second = h.ended(8);
  CHECK(second.chat->ok());
  CHECK(second.chat->text == "Served after the fault.");

  Handle d(/*throws=*/1);
  DecisionCall call;
  call.task = "next_speaker";
  call.question = "Who speaks next?";
  call.options = {{"A", "the first invented speaker"}, {"B", "the second invented speaker"}};
  call.rule = "B";
  REQUIRE(d.service->submit(LlmRequest::decision(9, call)) == SubmitResult::Accepted);
  const LlmEvent pick = d.ended(9);
  REQUIRE(pick.pick.has_value());
  CHECK(pick.pick->key == "B");
  CHECK(pick.pick->by == DecidedBy::Rules);
  CHECK(pick.pick->error == ErrorClass::BadRequest);
  en::proto::clear_trip(Component::LlmWorker);
}

TEST_CASE("R3: the caller's log and notify callbacks may throw on the handle's threads and lose only that call") {
  en::proto::clear_trip(Component::LlmWorker);
  Handle h(/*throws=*/1, /*log_throws=*/true);
  std::atomic<int> notifies{0};
  h.service->set_notify([&] {
    ++notifies;
    throw std::runtime_error("an invented fault in the caller's notify");
  });
  // A refused request is logged on the caller's thread: the throw stays inside the handle.
  CHECK(h.service->submit(LlmRequest::chat(0, a_chat())) == SubmitResult::Invalid);
  // The faulty request logs on a worker, the served one on the dispatcher's notify: each ends all the same.
  REQUIRE(h.service->submit(LlmRequest::chat(1, a_chat())) == SubmitResult::Accepted);
  CHECK(h.ended(1).chat->error == ErrorClass::BadRequest);
  h.server->script("model-a", sse_reply({"Still served."}));
  REQUIRE(h.service->submit(LlmRequest::chat(2, a_chat())) == SubmitResult::Accepted);
  CHECK(h.ended(2).chat->text == "Still served.");  // the client's own log lines threw too, inside the request
  CHECK(h.log_calls >= 3);
  // The notify callback stays registered: it is called again for later events.
  const int before = notifies.load();
  h.server->script("model-a", sse_reply({"Once more."}));
  REQUIRE(h.service->submit(LlmRequest::chat(3, a_chat())) == SubmitResult::Accepted);
  CHECK(h.ended(3).chat->ok());
  h.service->shutdown();
  CHECK(notifies.load() > before);
  CHECK(en::proto::tripped(Component::LlmWorker));
  en::proto::clear_trip(Component::LlmWorker);
}
