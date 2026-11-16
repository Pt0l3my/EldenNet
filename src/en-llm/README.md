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

## Tests

`cmake --build <build> --target en_llm_tests`, then run `en_llm_tests`. They use invented data and either the fake
transport on a manual clock (timeouts, cooldowns and the hour run in no time) or, for libcurl, a server bound to
127.0.0.1.

## Later

The WinHTTP transport; Ollama's warm-up (ARCH §11.3); the asynchronous
handle (`submit`, `cancel`, events) the core will drive, built on this blocking client.
