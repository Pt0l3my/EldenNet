# en-llm: the model client

The model client of EldenNet's engine (ARCH §11.1 to §11.5 and §11.8, DESIGN §11): one OpenAI-compatible
chat-completions client over the configured providers, with job slots, health, budgets, a backup request and decision
calls. C++20, MIT, no telemetry: the only traffic is the requests a caller makes, to the endpoints the configuration
names.

## What it does

- **Configuration** (`config.hpp`): `providers.toml`, the presets of `models.toml`, the `budget` table of
  `settings.toml` and the keys of `secrets.toml`, each read strictly (an unknown key or a newer schema is an error
  naming the file and line). Keys stay in memory as `Secret`, which prints `<redacted>`; no error, log line or report
  carries one. Every slot names its own provider, model and fallback, and a preset may name several providers, so
  that each use can have a provider of its own (O88); `actions` and `vision` are reserved.
- **Shared types** (en-proto's `llm.hpp`): the requests, decision calls, results and events the core and en-llm
  share are en-proto's plain data, with their JSON forms; en-llm's headers keep their names (`en::llm::ChatRequest`,
  `DecisionCall`, `LlmEvent` and the rest) with using-declarations.
- **Caps** (ARCH §10.2): a stream's line at most 64 KB and a whole response at most 1 MB (an error's body past it goes
  unread, and its status still names the error); a stream that passes 16 KB of display text is cut there, at a
  character, and cancelled, and ends as `Truncated` with the text that fitted; and every JSON payload (each stream
  event, a whole body, an error body, a decision's answer, a probe's flavour check) is parsed only once it is at most
  256 KB and 32 deep (en-proto's `parse_json`). A stream event past those caps fails its attempt as `Server`, unparsed.
- **Guards** (R3): every entry that foreign code calls is `noexcept` and starts with `ffi_guard` (en-proto):
  libcurl's callbacks and WinHTTP's status callback (component `LlmTransport`; a fault fails the exchange, which ends
  like any other), and the top of every thread en-llm starts, on 2 MB stacks (`thread.hpp`'s `StackThread`, since
  `std::thread` cannot set a stack size): the handle's workers and dispatcher (`LlmWorker`) and WinHTTP's drivers
  (`LlmTransport`). A worker's request whose client throws ends as `BadRequest`, and the caller's log and notify
  functions may throw without losing more than that call. Every cast of a C API's pointer carries a `// SAFETY:` note.
- **Requests** (`request.hpp`): messages with en-prompt's cache marks, `max_tokens` and temperature from the slot,
  streaming with usage, `response_format` with a JSON schema where the provider takes one, `logprobs`, the reasoning
  switch from `extra_body`, and the quantisation floor in OpenRouter's `provider.quantizations`.
- **Transport** (`transport.hpp`): an interface the client drives (start, wait, cancel), with two implementations that
  pass one conformance suite: libcurl behind `EN_WITH_CURL`, and the system's WinHTTP on Windows, which
  `make_default_transport_factory()` returns there. Both keep HTTP/1.1 connections alive across a factory's
  transports, follow no redirects, never send loopback through a proxy, and name a failure by its class only (could
  not connect, timed out, TLS, reset), never with the address, a header or a key. A scripted fake serves the other
  tests.
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
- **Decision calls** (`decision.hpp`): code builds the options (2 to 12 short keys), the reply is held to an enum where
  the provider takes a schema and otherwise read by meaning, yes-or-no questions are read plainly, the top-5 logprobs
  are recorded as probabilities and never cut on, and thinking stays off. The caller's rules decide whenever the model
  cannot: no healthy model, the gate closed, the hour spent, any failure (at once, no retry) or a miss. The call carries
  their pick as data, `rule`: the key the task's rules picked from the same state, which must be one of the call's keys
  (checked, with the rest of the call, before anything is sent). The result says what was picked, how it was read and
  why the rules decided, and gives the call's one row.

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
  provider's headers and its key in its own header, and returns the ids or the error class (401 and 403 are `Auth`), in
  our own words: never the key or the server's text. Model lists are read as they parse, so memory stays at what is
  kept, and they are bounded: 1000 ids of at most 256 bytes, ids with control characters left out rather than cut, at
  most 1 MB of answer for a probe or 8 MB for a provider, and no deeper than 32 levels, checked before the parser sees
  them; a flavour check's answer, parsed whole, is held to 256 KB. Both are free functions, and the caller runs them
  only while the offline gate is open.
- **Model lists on the handle** (`Service::list_models`): the same `list_models`, on one of the handle's workers, for
  the AI pages, so that the core's thread never waits for one. It ends with one `ModelsEnded` event (en-proto's
  `ModelsOutcome`: the class, the status, the ids within the limits, how many were left out), at once with
  `GateClosed` while the gate is closed; the gate's closing and shutdown stop one that runs. Like the warm-up, it
  counts in neither health nor the requests per hour, and its log line names the provider, the class and a count,
  never an id the server listed.
- **The self-test** (`selftest.hpp`, ARCH §11.7's steps 4a, 4b, 4d and 4e): a model list of each provider a slot uses,
  passed when it lists every model of those slots it serves; a short reply on the `selftest` slot for the first
  token's time and the tokens per second; a two-field JSON object on the `memory` slot against a schema; and the
  verdict on the estimated first page (the first token plus 15 tokens at the measured rate: quick to 1.6 s,
  acceptable to 3.0 s, else slow). A step on a provider that did not answer its list is skipped. The caller drives
  it without waiting (`step` after each batch of events, `take` for each event). Its report holds numbers, classes
  and the configuration's ids; the two requests are fixed words of our own, and their answers are read and dropped.
  Step 4c, the voice check through the dialogue pipeline, waits for the personas in game.
- **Edits** (`edit.hpp`) of `providers.toml` (the preset, a preset's providers added with a key of their own,
  `secrets:<id>`, providers removed with the `[slots.*]` that name them) and of `secrets.toml` (a key set or removed,
  the new text held as a `Secret`), for the AI pages: every other key kept, comments and layout not (toml++ writes the
  tables again), a file that fails its checks never written over; and the presets of `models.toml`, the providers a
  preset names that `providers.toml` lacks, the providers no slot uses, and a pasted key's cleaning.

## Configuration

The keys follow ARCH §11.1. Three were ours, where that schema left the choice open: `cache_marks` (how the provider
takes cache marks), `logprobs` (whether it returns them), and `provider` / `fallback_provider` in `[slots.*]`, which
the design took up with O88 as the way each use gets a provider of its own.

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

`load_config_dir` reads the files from a folder for the tools and the tests (`config_dir.cpp`, a file of its own so
that eldennet.dll, which reads them through its own platform layer and calls `parse_config`, links no
`std::filesystem`).

A slot without a model borrows one: `decisions` takes `bark_live`'s, every other slot `dialogue`'s. Unknown slot
names in a request fall through to `dialogue`. `settings.toml` is read for `budget.requests_per_hour` (60 to 3600)
only; its other tables belong to other modules.

A slot of `providers.toml` that names a model but no `provider` takes the provider its preset gives that slot, or
gives the slot it would borrow from (`decisions` from `bark_live`, the rest from `dialogue`), or the preset's only
provider; without a preset, the only provider there is, and otherwise it must name one. A `fallback` without a
`fallback_provider` takes its model's provider.

### Presets

A preset of `models.toml` (ARCH §11.4) names a model for each slot and the providers its slots use, in one of two
forms. Each of its providers is matched to a provider of `providers.toml` as it says: by `base_url` where it gives one,
otherwise as the first of its `kind` and `flavor`. The keys stay with `providers.toml`'s providers, by `key_ref`: a
preset never holds one, and a `key_ref` or `key` in one is refused.

```toml
# models.toml (the endpoints and models here are invented)
schema = 1

# One provider, as the presets have always named it: every slot is on it.
[preset.one-router]
provider = { flavor = "openai", kind = "cloud", base_url = "https://router.example.invalid/api/v1" }
dialogue  = { model = "talker-small", fallback = "talker-large" }
bark_live = { model = "talker-small" }

# Providers by name (O88): each slot names its own, and a fallback may be on another.
[preset.two-routers.providers.main]
flavor = "openai"
kind = "cloud"
base_url = "https://router.example.invalid/api/v1"

[preset.two-routers.providers.fast]
base_url = "https://fast.example.invalid/v1"

[preset.two-routers]
dialogue  = { provider = "main", model = "talker-small", fallback = "talker-fast", fallback_provider = "fast" }
bark_live = { provider = "fast", model = "quick-small" }
decisions = { provider = "fast", model = "quick-small" }
memory    = { provider = "main", model = "keeper-large" }
```

A slot may leave out `provider` only when its preset names one provider; `fallback_provider` defaults to the slot's
own. A name is the preset's own, but may not be the id of another provider of `providers.toml`, which would make it
stand for two (the same id for the same provider, as the wizard writes it, is fine). Refused, with the file and the
line: a preset with neither form or with both, a slot (or `all`) naming a provider the preset does not define, a slot
with no provider in a preset of several, a `fallback_provider` with no `fallback`, and one name for two providers
(TOML itself refuses a name given twice).

### Reserved slots

`actions` (NPC actions, after v1) and `vision` (letting an NPC see the screen) are reserved (O88): `[slots.actions]`,
`[slots.vision]` and a preset's `actions` and `vision` are read and checked as every slot is, and their models kept in
`Config::reserved`, so that the files keep their shape when the features arrive. They have no budget yet, and no
request uses them: one that names them falls through to `dialogue`, its models and its budget, as an unknown name
does. A preset's `all` does not reach them.

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

call.rule = "A";  // the key the task's rules picked from the same state: one of the call's keys
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

## Transports

Both take the request's connect timeout (3 s unless the request says otherwise, ARCH §11.1) and leave every later wait
to the client's own deadlines, which cancel the exchange.

Both enter our code only through callbacks, each `noexcept` and starting with `ffi_guard` (R3): a fault in one (out
of memory, in practice) trips `LlmTransport`, and its exchange fails (libcurl: the callback takes 0 bytes, which ends
the transfer as a write error; WinHTTP: the driver is told that a completion was lost, and posts the failure), so the
request ends at once like any other, and the transport goes on.

**WinHTTP** (`transport_winhttp.cpp`, Windows builds):

- **The library.** The system directory's own `winhttp.dll`, loaded once by its full path and kept for the program's
  life; its functions are looked up there, and it is never imported by name. A proxy `winhttp.dll` in the game's folder
  (some overlays and the EAC toggler ship one) would otherwise be bound in its place and see every request, the key's
  header included. Without it, or with a function missing, there is no transport (the factory is null). The build fails
  if the tests' import table names `winhttp.dll`.
- **Sessions.** Each factory opens two and shares them with every transport it opens: one with the system's proxy for
  remote hosts (WinHTTP's automatic mode, which finds the proxy as the rest of Windows does, on Windows 8.1 and later;
  its default configuration before), and one that never uses a proxy, for `127.0.0.0/8`, `localhost` and `[::1]`.
- **TLS** 1.2 or 1.3 only, through the system's certificate store, its checks never weakened. HTTP/1.1, no redirects
  (a key is never carried to another address), no cookies, no automatic logon with the player's Windows account, and
  no user agent, as with libcurl.
- **Timeouts.** Resolving the name, connecting and the TLS handshake take the request's connect timeout, as with
  libcurl: the driver fails an exchange whose request has not started to go out by then (`SENDING_REQUEST`) as a
  timeout. WinHTTP's own connect timeout is only a backstop, since on Windows it let a refused connect run the system's
  retries, about 2 s. Sending, receiving and waiting for the headers have a safety net of 5 minutes, far beyond the
  client's longest deadline (a minute).
- **Exchanges.** Each runs in WinHTTP's asynchronous mode with a driver thread of its own, on a 2 MB stack (ARCH §10.2):
  the status callback only queues what completed and records how far the exchange got (connected, sending), and the
  driver takes every next step, posts the events and closes the request handle at the end, on `cancel()` or at the
  connect deadline, so no call on a handle overlaps its closing. `cancel()` drops the exchange's events at once, and its
  memory lives until WinHTTP's last notification for the handle (`HANDLE_CLOSING`). `wait()` never calls WinHTTP, so it
  keeps to its bound.
- **Failures** are worded by `winhttp_failure()` (`transport_winhttp_failure.cpp`, built on every system, so that the
  tests check it anywhere): for https, a failure after the connection was made and before the request went out is a
  TLS failure whatever WinHTTP's code, since Windows may report a server that does not speak TLS with a code that is not
  about TLS. The connection was made once `CONNECTED_TO_SERVER` came, or if the code is a reset or a malformed reply.
- **Keys** go into the request just before it is sent, from one buffer that is cleared at once; WinHTTP holds its own
  copy for the request's life. A header with a line break, an address with a user name or password, or a scheme other
  than `http` and `https` fails before anything is sent.
- **A reply shorter than its Content-Length** fails, as libcurl reports it.

**Under Wine** (and so Proton), whose WinHTTP is its own implementation, four things differ (Wine 9.0), and the tests
check what Wine does:

- A chunked reply arrives one chunk late: Wine completes a read that ends a chunk only once the next chunk begins. A
  stream still arrives piece by piece, and the client's thread never waits for it (the driver does).
- Closing a request does not stop a receive already waiting: a cancelled exchange's connection stays open until the
  server sends or closes, or the safety net passes. The client sees nothing more of it either way.
- The automatic proxy mode means no proxy at all, and an IPv6 address in brackets does not resolve (read from Wine's
  source): give a local server as `localhost` or `127.0.0.1`.
- `CONNECTED_TO_SERVER` comes only once the TLS handshake is done, and a server that does not speak TLS fails with
  `ERROR_WINHTTP_SECURE_CHANNEL_ERROR`, a TLS code, so its failure says TLS by its code.

## The asynchronous handle

`Service` (`service.hpp`) is the handle the core drives (ARCH §2.2's `Llm::spawn` and `LlmHandle`). It is built on
the blocking client, which keeps each slot's budgets, retries, fallbacks, backup requests, health and provider
admission; the handle adds the queue, the events, the input epoch and the gate around it.

- **Requests** (`LlmRequest`): the caller's id (non-zero, unique while live), a chat request or a decision call, and,
  for own-initiative work, the input epoch it was made at (own-initiative dialogue must carry one; a reply never does).
  `submit` answers `Accepted`, `Full`, `Duplicate`, `Invalid` (named in one log line) or `ShutDown`, and never blocks. A
  decision call's rule is data, the key its rules picked, checked at submit with the rest of the call (`Invalid` when it
  is not one of the call's keys), so no worker runs the caller's code.
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
  deadlines, the notify callback and the log lines made under the handle's lock, all on 2 MB stacks (ARCH §10.2).
  Each starts with `ffi_guard` (R3): a request whose client call throws trips `LlmWorker` and ends as `BadRequest`
  with its one `Ended` event, and a log or notify callback that throws loses that call only (the notify callback stays
  registered). `shutdown` (and the destructor) cancels everything, queues every ending and joins.
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

`cmake --build <build> --target en_llm_tests`, then run `en_llm_tests` (the Windows build under Wine:
`WINEPREFIX=<dir> wine64 en_llm_tests.exe`). They use invented data and either the fake transport on a manual clock
(timeouts, cooldowns and the hour run in no time) or small servers bound to 127.0.0.1 (`tests/local_server.hpp`, on
POSIX sockets or Winsock), against which one conformance suite (`test_transport.cpp`) runs the real transports:
libcurl where `EN_WITH_CURL` is set, WinHTTP on Windows. WinHTTP's words for a failure are checked on every build
(`test_winhttp_failure.cpp`), with the codes that Windows itself may give, in each phase of an exchange.
The handle's tests run on the fake transport and the manual clock too, and none sleeps for a set time: each waits
for what must happen.

`test_presets.cpp` takes both forms of a preset's providers, each refusal at its file and line, the reserved slots,
and each slot's requests reaching its own provider, with its own key, on the fake transport.
`test_limits.cpp` takes each of ARCH §10.2's caps just under and just over, on the parser and the assembler and through
the client; `test_r3.cpp` the guards: the threads' stack sizes (where the system reports them: Windows, and Linux with
glibc), a thread's throw, a worker's request whose client throws, and the caller's callbacks throwing. The transports'
own callbacks are tested in `test_transport.cpp` with a fault of the tests' own (`fault_alloc.hpp`): the test program
replaces every form of the global `operator new` and `delete` (all twenty, so that no memory passes between its
allocator and a runtime's), so that, once armed, one allocation of a chosen size fails (a nothrow form returns null) on
any thread but the test servers', inside libcurl's or WinHTTP's callback; libcurl and WinHTTP allocate with their own
allocators and never meet it. On MSVC's CRT (the clang-cl build) `system_env` reads with `_dupenv_s`, the CRT's
thread-safe `getenv`, and the tests' `ScopedEnv` reads through `system_env`.

## Later

The handle's pool growing on `reload`; a warm-up holds a worker while the model loads (up to 2 minutes); for WinHTTP,
what only Windows itself can show: its proxy settings, TLS with real certificates, and IPv6 loopback.
