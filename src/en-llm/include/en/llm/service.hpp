#pragma once
// The asynchronous handle the core drives (ARCH §2.2: `Llm::spawn` gives an `LlmHandle` with `submit`, `cancel` and
// `events`; here the Service's constructor, its methods and its event reads). It is built on the blocking Client,
// which keeps each slot's budgets, retries, fallbacks, backup requests, health, provider admission with preemption of
// background jobs (ARCH §11.3) and the offline gate. Requests wait in a bounded queue in ARCH §11.3's order (C9), run
// on a small fixed pool of workers, and come back as events through a bounded queue (C10). Also Ollama's warm-up.
//
// The rules:
// - Every accepted request ends with exactly one Ended event, whatever happens to it: an answer, a failure, a cancel
//   while queued or running, eviction, the gate, shutdown. After a cancel has returned, nothing of that request but
//   its Ended event is queued.
// - Order: priority_of() (replies > own-initiative dialogue and the decision calls that choose it > bark_live >
//   memory > bark_pool), first come first served within a priority. Requests start one at a time in that order (a
//   start lasts until the first attempt is sent or the client answers without sending), so that a provider's last
//   free connection goes to the request that comes first, whichever worker reaches the client first.
// - One dialogue request in flight. A reply cancels own-initiative dialogue and the decision calls that carry an
//   epoch, queued or running; own-initiative work submitted while a reply is live waits until it has ended.
// - Busy. When the client answers Busy (nothing was sent: the slot, or every healthy provider, is full of work that
//   may not give way), the request goes back to its place in the queue and is tried again once another request has
//   ended. A live request (bark_live, a decision call, own-initiative dialogue) that has not started within its
//   slot's first-token time of being submitted (the local one when the slot's model is on a local provider) ends as
//   Busy, so the core takes its seed or its rules; everything else waits as long as it must.
// - A full queue (it holds `queue_capacity` live requests, waiting or running): a new request that outranks a queued
//   bark_pool or memory job takes its place, the lowest and newest first; the bark_pool job ends as Dropped, the
//   memory job as Preempted so that the core requeues it. Otherwise submit() answers Full. Nothing else is evicted.
// - The event queue holds `event_capacity` events. A worker with an event for a full queue waits (backpressure, up
//   the stream to the HTTP connection); no delta is dropped while its request runs. An Ended event never waits: it
//   may take the queue past its capacity, by at most one per live request, so that no request is left without its end
//   and shutdown never blocks.
// - The gate (C15): closing it closes the client's gate, stops running requests, and ends queued ones at once with
//   GateClosed; a request submitted while it is closed is accepted and ends at once with GateClosed.
// - A request the handle ends without running it (cancelled while queued, evicted, the gate, a Busy deadline,
//   shutdown) gets a result made here: no provider, its time the wait since submit, and for a decision call the pick
//   of its rules. A request the handle stops while it runs ends with the reason it was stopped for.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "en/llm/client.hpp"
#include "en/llm/clock.hpp"
#include "en/llm/config.hpp"
#include "en/llm/decision.hpp"
#include "en/llm/errors.hpp"
#include "en/llm/transport.hpp"
#include "en/proto/llm.hpp"

namespace en::llm {

// The requests and events are en-proto's (llm.hpp, ARCH §2.2's LlmRequest and LlmEvent), shared with the core. A
// request that carries an epoch is cancelled by cancel_before_epoch().
using en::proto::Epoch;
using en::proto::EventKind;
using en::proto::LlmEvent;
using en::proto::LlmRequest;
using en::proto::ModelsOutcome;
using en::proto::RequestId;
using en::proto::RequestKind;
using en::proto::to_string;
using en::proto::WarmUpOutcome;

enum class SubmitResult {
  Accepted,   // exactly one Ended event follows
  Full,       // the queue is full and nothing in it may give way (C9): nothing was queued
  Duplicate,  // a live request has this id
  Invalid,    // id 0, a chat with no messages, a malformed decision call or rule, or an epoch where none belongs
  ShutDown,   // the handle has shut down
};

const char* to_string(SubmitResult result);

struct ServiceOptions {
  ClientOptions client;               // the clock, the log and the poll slice, which the handle uses too
  std::size_t queue_capacity = 64;    // C9: live requests, waiting or running
  std::size_t event_capacity = 1024;  // C10
  // 0: one more than the providers' total concurrency, from 2 to 8, so that a reply always finds a worker from which
  // to take a provider's connection back from a background job. Fixed for the handle's life.
  int workers = 0;
};

struct ServiceStats {
  std::size_t queued = 0;   // accepted and waiting, those waiting after Busy included
  std::size_t running = 0;  // on a worker
  std::size_t events = 0;   // events not yet read
  int workers = 0;
};

class Service {
 public:
  // Starts the workers. The transport factory serves the client and the warm-ups.
  Service(Config config, std::shared_ptr<TransportFactory> transport, ServiceOptions options = {});
  // shutdown().
  ~Service();
  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  // Never blocks on the network or the event queue. An Invalid request is also named in one log line.
  SubmitResult submit(LlmRequest request);
  // Cancels a live request, queued (its Ended event is queued before this returns) or running (it follows once the
  // client has stopped, within a poll slice). Returns whether the id was live.
  bool cancel(RequestId id);
  // Cancels every live request that carries an epoch before `epoch`; returns how many.
  std::size_t cancel_before_epoch(Epoch epoch);

  // The events, oldest first, for one reader. wait() waits in real time, whatever the client's clock, and returns at
  // once once the handle has shut down and nothing is left.
  std::optional<LlmEvent> poll();
  std::size_t drain(std::vector<LlmEvent>& out);
  std::optional<LlmEvent> wait(Millis timeout);
  // Called on the handle's own thread, never the caller's, whenever the event queue goes from empty to non-empty: keep
  // it short (set a flag, signal a condition). It may read events; it must not call set_notify. After set_notify
  // returns, the callback it replaced is not running and is not called again; shutdown() may still call the current
  // one before it returns, and nothing is called after that.
  void set_notify(std::function<void()> notify);

  void set_gate_open(bool open);
  bool gate_open() const;

  // Ollama's warm-up (ARCH §11.3): POST <root>/api/generate {"model", "keep_alive": "10m"}, no prompt, where <root>
  // is the base URL without its /v1. It acts only when the slot's model (unknown slots fall through to dialogue) is on
  // a local provider of the ollama flavour whose keys resolved, the gate is open, that provider has served no request
  // through this handle for more than 4 minutes and has none in flight, and no warm-up for it is in flight. A warm-up
  // that succeeded counts as serving, for this rule only: it keeps the model loaded for ten minutes. Returns the
  // warm-up's id, or nothing when it does not act; the warm-up ends with one WarmUpEnded event.
  // It goes from a worker straight to the transport, not through the client's admission, and counts in neither health
  // nor the requests per hour: it generates nothing and costs nothing, a local provider is never billed, and its answer
  // comes from Ollama's own endpoint, so a failure (a proxy that serves only /v1, say) says nothing about the chat
  // completions whose health the client tracks.
  std::optional<RequestId> warm_up(std::string_view slot);

  // A provider's model list (en-llm's list_models: GET <base_url>/models with its key, ARCH §11.4 and §11.7's step 4a),
  // on one of the handle's workers, for the AI pages' self-test and model list: never on the caller's thread. Returns
  // the list's id (its own, as a warm-up's), or nothing for a provider the configuration does not name or after
  // shutdown; the list ends with one ModelsEnded event, GateClosed at once while the gate is closed, and the gate's
  // closing or shutdown stops one that runs. Like the warm-up, it counts in neither health nor the requests per hour:
  // it generates nothing, and the self-test's own requests, which go through the client, give the provider's health.
  std::optional<RequestId> list_models(std::string_view provider);

  // A changed configuration (Client::reload); requests waiting after Busy try again. The pool keeps its size.
  void reload(Config config);
  // Health, the requests in the last hour, the configuration. Change the gate through set_gate_open().
  const Client& client() const;

  ServiceStats stats() const;

  // Stops accepting; ends queued requests and pending warm-ups with Cancelled, stops running ones and joins every
  // thread. Every Ended event is queued before it returns, and can still be read. Never blocks on a full event queue.
  void shutdown();

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace en::llm
