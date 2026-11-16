# en-llm: the model client

The model client of EldenNet's engine (ARCH §11.1 to §11.5 and §11.8, DESIGN §11): one OpenAI-compatible
chat-completions client over the configured providers, with job slots, health, budgets, a backup request and decision
calls. C++20, MIT, no telemetry: the only traffic is the requests a caller makes, to the endpoints the configuration
names.

## What it does

- **Configuration** (`config.hpp`): `providers.toml`, the presets of `models.toml`, the `budget` table of
  `settings.toml` and the keys of `secrets.toml`, each read strictly (an unknown key or a newer schema is an error
  naming the file and line). Keys stay in memory as `Secret`, which prints `<redacted>`; no error, log line or report
  carries one.
- **Requests** (`request.hpp`): messages with en-prompt's cache marks, `max_tokens` and temperature from the slot,
  streaming with usage, `response_format` with a JSON schema where the provider takes one, `logprobs`, the reasoning
  switch from `extra_body`, and the quantisation floor in OpenRouter's `provider.quantizations`.
- **Transport** (`transport.hpp`): an interface the client drives (start, wait, cancel), libcurl behind `EN_WITH_CURL`
  (HTTP/1.1 keep-alive shared across requests, no redirects, loopback never proxied), and a scripted fake for tests.
  A WinHTTP transport is later work behind the same interface; until then a Windows build has none, and `chat` says so.
- **Streaming** (`sse.hpp`, `stream.hpp`): our own SSE parser, the reply assembled from its deltas, `<think>` stripped
  (split tags and a lone closing tag too), usage, the finish reason and logprobs recorded.
- **Errors and health** (`errors.hpp`, `health.hpp`): ARCH §11.2's classes and the client's part of their actions.
  A rejected request is retried once without the optional fields, never without the reasoning switch or the floor; a
  model that refuses reasoning off is retried once at the lowest effort and starts there for the rest of the session.
  Failover follows the slot's model list; three counted failures cool a pair for 60 s; a 429 cools its provider
  (Retry-After, at most 300 s); a rejected key disables the provider until `reload`.
- **Slots and budgets**: ARCH §11.4's budgets per slot (code, the same in every preset), concurrency per slot and per
  provider, ARCH §11.3's priorities (a reply takes a provider's connection from a memory or pool job, which ends as
  `Preempted` and requeues; nothing takes it from dialogue), and the requests per hour (default 600) past which only
  replies go out.
- **Backup request**: a cloud reply with no text 2.5 s after it was sent starts the slot's fallback beside it; the
  first to stream is kept and the other cancelled, which counts as no failure.
- **Decision calls** (`decision.hpp`): code builds the options (2 to 12 short keys), the reply is held to an enum
  where the provider takes a schema and otherwise read by meaning, yes-or-no questions are read plainly, the top-5
  logprobs are recorded as probabilities and never cut on, thinking stays off, and the caller's rules decide whenever
  the model cannot: no healthy model, the gate closed, the hour spent, any failure (at once, no retry) or a miss.
  The result says what was picked, how it was read and why the rules decided, and gives the call's one row.

What stays with the caller, as ARCH §11.2 assigns it: seeds, toasts, the authored deflection for `Refused`, showing
the complete sentences of a `Truncated` reply (`complete_sentences`), requeueing a `Preempted` or failed job, the
pause rules, and the input epoch (pass a `std::stop_token`).
- **Local probes and model lists** (`probe.hpp`): `probe_local` asks 127.0.0.1 on LM Studio's `:1234`, Ollama's
  `:11434`, llama.cpp's `:9931` and legacy `:8080` and KoboldCpp's `:5001` for `GET /v1/models`, all at once on one
  transport (500 ms to connect, 1.5 s for the whole probe, and whatever is late is cancelled). A port counts only if it
  answers 200 with an OpenAI-style model list. An empty list counts, and so does Ollama's `"data": null`, because a
  server that is up with nothing loaded is the commonest local mistake and the wizard can only say so if it sees the
  server. Each port that counts is then asked `GET /api/version` and `GET /props`. Ollama is confirmed only by a
  `version` string, and llama.cpp only by its props (`default_generation_settings`, `total_slots` and `build_info`); a
  server that answers as both is taken for neither, and LM Studio and KoboldCpp are never confirmed. An unconfirmed
  endpoint is `openai`, and its port's usual owner is only a hint. `list_models` sends `GET <base_url>/models` with the
  provider's headers and its key in its own header, and returns the ids or the error class (401 and 403 are `Auth`),
  in our own words: never the key or the server's text. Model lists are read as they parse, so memory stays at what is
  kept, and they are bounded: 1000 ids of at most 256 bytes, ids with control characters left out rather than cut, and
  at most 1 MB of answer for a probe or 8 MB for a provider. Both are free functions, and the caller runs them only
  while the offline gate is open.

## Configuration

The keys follow ARCH §11.1. Three are ours, where that schema leaves the choice open: `cache_marks` (how the provider
takes cache marks), `logprobs` (whether it returns them), and `provider` / `fallback_provider` in `[slots.*]`.

```toml
# providers.toml (the endpoint and models here are invented)
schema = 1
preset = "custom"                      # or a preset of models.toml

[[provider]]
id = "router"
flavor = "openai"                      # openai | ollama | llamacpp | lmstudio | koboldcpp
kind = "cloud"                         # cloud | local (a loopback base_url is local unless told otherwise)
base_url = "https://router.example.invalid/api/v1"
key_ref = "secrets:router"             # or "env:VARNAME"; never inline
headers = { "X-Title" = "EldenNet" }   # a value "secrets:NAME" or "env:VAR" is kept secret too
extra_body = { reasoning = { enabled = false }, provider = { sort = "latency" } }
max_concurrency = 2                    # default: cloud 2, local 1
json_mode = "json_schema"              # none | json_object | json_schema
quant_floor = "fp8"                    # cloud providers that filter upstreams by quantisation
cache_marks = "breakpoints"            # none | breakpoints | cache_prompt (default: cache_prompt for llamacpp)
logprobs = true                        # default: true for openai and llamacpp

[slots.dialogue]
model = "talker-small"
fallback = "talker-large"
```

```toml
# secrets.toml (keys only)
[keys]
router = "<the player's key>"
```

A slot without a model borrows one: `decisions` takes `bark_live`'s, every other slot `dialogue`'s. Unknown slot
names in a request fall through to `dialogue`. `settings.toml` is read for `budget.requests_per_hour` (60 to 3600)
only; its other tables belong to other modules.

## Use

```cpp
#include "en/llm/client.hpp"
#include "en/llm/decision.hpp"

en::llm::Client client(en::llm::load_config_dir(dir), en::llm::make_default_transport_factory());

en::llm::ChatRequest request;
request.slot = "dialogue";
request.purpose = en::llm::Purpose::Reply;
request.messages = en::llm::parse_messages(rendered);  // or Message::system(...), Message::user(...)
en::llm::ChatResult reply = client.chat(request, &sink, epoch_token);  // sink.on_delta() as it streams

en::llm::DecisionPick pick = client.decide(call);  // never throws for a provider's failure
```

`chat` blocks until the request has ended, and is safe from several threads at once. The commands:

```text
eldennet-engine chat   --config <dir> --slot <name> --messages <file.json> [--purpose reply|own|decision|background]
eldennet-engine decide --config <dir> --request <file.json>
```

`--messages` takes an array of messages or `{"messages": [...]}`, such as `render-prompt --json` writes. A message is
`{"role", "content"}`, where content is text or a list of `{"text"}` parts, and a cache mark is `"cache": "A"` (any
label) or `"cache": true` on a message or part, or OpenAI's `"cache_control"`. The mark means a breakpoint after it
(NPC §7.1). Parts join exactly as written, with no separator added, so a part that starts a new paragraph carries
its own blank line, as en-prompt's parts do. en-prompt's `"parts"` list, when present, holds the message and its
breakpoints (a merged message can have several), and must join to its `"content"`. `--request` takes
`{"task", "kind": "choice" | "yes_no", "subject", "question", "state": [{"name", "text"}], "options": [{"key", "text"}],
"rule"}`, where `rule` is the key the task's rules picked, and an optional `messages` rendered by en-prompt.

```text
eldennet-engine probe  [--json]
eldennet-engine models --config <dir> [--provider <id>] [--models <models.toml>] [--json]
```

`probe` needs no configuration. For each port that answered it prints the flavour, whether that was confirmed and how
many models the server lists; `--json` gives the same as data, with each base URL and the model ids. `models` lists the
models of every configured provider, or of the one named, or the provider's error class. Both exit 0 once they have
run, whatever they found, 2 on a usage or configuration error, and 1 when the build has no HTTP transport.

```cpp
#include "en/llm/probe.hpp"

// Only while the offline gate is open; `stop` ends it at once when the gate closes.
for (const en::llm::LocalEndpoint& e : en::llm::probe_local(*transport, {}, stop)) {
  // e.port, e.base_url, e.flavor, e.confirmed, e.hint (shown, never acted on), e.models
}
en::llm::ModelsResult listed = en::llm::list_models(*transport, *config.provider("router"));
```

## The asynchronous handle

`Service` (`service.hpp`) is the handle the core drives (ARCH §2.2's `Llm::spawn` and `LlmHandle`). It is built on
the blocking client, which keeps each slot's budgets, retries, fallbacks, backup requests, health and provider
admission; the handle adds the queue, the events, the input epoch and the gate around it.

- **Requests** (`LlmRequest`): the caller's id (non-zero, unique while live), a chat request or a decision call, and,
  for own-initiative work, the input epoch it was made at (own-initiative dialogue must carry one; a reply never does).
  `submit` answers `Accepted`, `Full`, `Duplicate`, `Invalid` (named in one log line) or `ShutDown`, and never blocks.
  A decision call's rule runs once, at submit, on the caller's thread: its key is checked there and stands for it, so
  no worker runs the caller's code.
- **Events** (`LlmEvent`): `Started` (the provider and model of the first attempt sent), `Delta`s in order, `Reset`
  (withdraw the text passed on so far), and exactly one `Ended` per accepted request, with the chat result or the
  decision's pick, whatever happens to it. Once `cancel` has returned, nothing of that request but its ending is
  queued. A request the handle ends without running it gets a result made there: no provider, its time the wait since
  submit, and for a decision call the pick of its rules. One the handle stops while it runs ends with the reason it
  was stopped for.
- **Order** (C9): replies > own-initiative dialogue and the decision calls that choose it > `bark_live` > `memory` >
  `bark_pool` (`priority_of`, now public), first come first served within a priority. Requests start one at a time in
  that order (until the first attempt is sent, or the client answers without sending), so that a provider's last free
  connection goes to the request that comes first.
- **One dialogue request in flight**: a reply cancels own-initiative dialogue and the decision calls that carry an
  epoch, queued or running, and starts once they have ended; own-initiative work submitted while a reply is live waits
  for it. `cancel_before_epoch(e)` cancels whatever carries an epoch before `e`.
- **Busy**: nothing was sent, so the request goes back to its place in the queue and is tried again once another
  request has ended (or the configuration is reloaded). A live request (`bark_live`, a decision call, own-initiative
  dialogue) that has not started within its slot's first-token time of being submitted (the local one when the slot's
  model is on a local provider) ends as `Busy`, so that the core takes its seed or rules; the rest wait as long as
  they must.
- **A full queue** (64 live requests, waiting or running): a request that outranks a queued `bark_pool` or `memory`
  job takes its place, the lowest and newest first. The pool job ends as `Dropped` (a new error class: it is not
  requeued), the memory job as `Preempted`, so that the core requeues it. Otherwise `submit` answers `Full`. Replies
  and live requests are never evicted.
- **The event queue** (C10, 1,024 events): a worker with an event for a full queue waits, so no delta is dropped while
  its request runs; an ending never waits (it may take the queue past its capacity, by at most one per live request),
  so no request is left without its end and shutdown never blocks. The core reads with `poll`, `drain` or `wait` (in
  real time), and `set_notify` registers a callback that the handle's own thread, never the caller's, calls when the
  queue goes from empty to non-empty.
- **The gate** (C15): `set_gate_open(false)` closes the client's gate, stops running requests and ends queued ones at
  once with `GateClosed`; a request submitted while it is closed is accepted and ends at once the same way.
- **Threads**: a fixed pool of one more worker than the providers' total concurrency (2 to 8), so that a reply always
  finds a worker from which to take a connection back from a background job, and one dispatcher thread for the start
  deadlines, the notify callback and the log lines made under the handle's lock. `shutdown` (and the destructor)
  cancels everything, queues every ending and joins.
- **Ollama's warm-up** (ARCH §11.3): `warm_up(slot)` sends `POST <root>/api/generate` with `{"model", "keep_alive":
  "10m"}` and no prompt (`<root>` is the base URL without its `/v1`), from a worker straight to the transport, and
  returns its id; it ends with one `WarmUpEnded` event (provider, model, success, status, time). It acts only when the
  slot's model is on a local `ollama` provider, the gate is open, that provider has served no request through the
  handle for more than 4 minutes and has none in flight, and no warm-up for it is in flight; a warm-up that succeeded
  counts as serving for that rule. It counts in neither health nor the requests per hour: it generates nothing and
  costs nothing, and its answer comes from Ollama's own endpoint, not the chat completions whose health the client
  tracks.

The started event comes from two small additions to the client: `StreamSink::on_attempt`, which tells a sink of each
attempt as it is sent, and a `Client::decide` that takes an observer for the same.

```cpp
#include "en/llm/service.hpp"

en::llm::Service llm(en::llm::load_config_dir(dir), en::llm::make_default_transport_factory());
llm.set_notify([&] { core_wakes.notify_one(); });  // on the handle's thread: keep it short

llm.submit(en::llm::LlmRequest::chat(17, reply));                    // the player asked
llm.submit(en::llm::LlmRequest::chat(18, own_line, input_epoch));    // an NPC's own line
llm.submit(en::llm::LlmRequest::decision(19, next_speaker, input_epoch));
llm.warm_up("dialogue");                                             // on PersonaApproached

// When the player's input epoch moves on:
llm.cancel_before_epoch(input_epoch);

std::vector<en::llm::LlmEvent> events;
llm.drain(events);  // Started, Delta, Reset, Ended (.chat or .pick), WarmUpEnded
```

## Tests

`cmake --build <build> --target en_llm_tests`, then run `en_llm_tests`. They use invented data and either the fake
transport on a manual clock (timeouts, cooldowns and the hour run in no time) or, for libcurl, a server bound to
127.0.0.1.
The handle's tests run on the fake transport and the manual clock too, and none sleeps for a set time: each waits
for what must happen.

## Later

The WinHTTP transport (agent W's part of S8.1.g, still to merge); the handle's pool growing on `reload`; a warm-up
holds a worker while the model loads (up to 2 minutes).
