# en-proto: the messages between threads

The messages that cross a thread or the adapter boundary in EldenNet's engine (ARCH §2.2, §2.3), as plain data with
their JSON forms, and the guard on every entry that foreign code calls (R3). C++20, MIT. It depends on the standard
library and nlohmann/json only: no I/O, no threads, no clocks, no Win32 and no game knowledge. Its static library
`en_proto` is a module the DLL links (`en_dll_module`), so it holds no static object with a dynamic initialiser or an
exit-time destructor (ARCH §3.1, §1.7).

## What it holds

- **The guard** (`ffi_guard.hpp`, DESIGN R3, ARCH §10.1). `ffi_guard(component, fallback, f)` runs `f` inside a
  catch-all handler and returns its answer; on any exception it trips the component's flag and returns `fallback`.
  It is `noexcept`, and so is every entry that starts with it; the two-argument form is for entries that answer
  nothing. The components are those whose entries foreign code calls: `DllMain`, `Task`, `Present`, `Overlay`,
  `LlmTransport` (libcurl's and WinHTTP's callbacks, WinHTTP's driver threads), `LlmWorker` (the model handle's
  threads) and `Tools` (the command's signal handler), with room for 32. Each flag is a constant-initialised atomic:
  `tripped(c)` reads it, `clear_trip(c)` clears it. A trip changes nothing by itself: what it means (a toast, a
  subsystem off) is its owner's decision.
- **The model client's messages** (`llm.hpp`): what the core asks of en-llm and what comes back. The slots' names,
  roles, message parts and messages (with `system`, `user`, `assistant`, `joined`, `has_cache_break` and
  `normalise`), purposes, `ChatRequest`, the error classes, usage, token logprobs, attempts, `ChatResult`, decision
  calls (kinds, options, records, `DecisionCall`), `DecisionPick` (how it was made and read, its probability, its
  row), `WarmUpOutcome`, request ids and epochs, `LlmRequest`, and the event kinds and `LlmEvent`. A decision call's
  rules fallback is data: `rule` is the key the caller's rules picked from the same state, and en-llm checks that it
  is one of the call's keys before anything is sent. en-llm keeps their names (`en::llm::ChatRequest` and the rest)
  with using-declarations, and keeps everything that acts on them.
- **The JSON caps** (`json.hpp`, ARCH §10.2): `parse_json(text)` parses a JSON text only once it is at most 256 KB
  and nested at most 32 deep, both checked before nlohmann/json sees it (`within_depth` counts brackets outside
  strings, without parsing), and never throws for what the text holds. `read_json<T>(text)` parses within the caps
  and reads the message, and every `from_json` throws `ProtoError`, naming the path and the problem.

## The JSON forms

Every field is written, under its own name in snake_case; a duration is `<name>_ms` in milliseconds, an absent
optional is `null`, ids are JSON integers (exact past 2^53), and enums are words (`"reply"`, `"own_initiative"`,
`"choice"`, `"yes_no"`, and the error classes and event kinds by their names, `"GateClosed"`, `"Ended"`). Reading is
strict about what is there: an object holds only known fields, each of its own type; a field left out keeps its
default. `LlmRequest` holds exactly one of `"chat"` and `"decision"`. An error names where it was and what was
wrong, and never a value:

```text
LlmEvent.chat.attempts[1].error: must be one of None, FirstTokenTimeout, …
LlmRequest.decision.options[0]: has an unknown field "txt"
the text is nested deeper than 32
```

```cpp
#include "en/proto/json.hpp"
#include "en/proto/llm.hpp"

const nlohmann::json j = event;                                      // to_json
const auto back = en::proto::read_json<en::proto::LlmEvent>(j.dump());  // within the caps; throws ProtoError

std::size_t on_body(char* data, std::size_t size, std::size_t n, void* user) noexcept {  // a library's callback
  return en::proto::ffi_guard(en::proto::Component::LlmTransport, std::size_t{0}, [&] { return take(data, size * n); });
}
```

## Tests

`cmake --build <build> --target en_proto_tests`, then run `en_proto_tests` (under Wine for the Windows build). A round
trip for every type, through JSON values and through text, with every field away from its default and ids past 2^53;
fields left out keep their defaults; malformed JSON refused with its path; each cap just under and just over; and the
guard: the answer when nothing throws, the fallback with only its own component tripped when anything is thrown.
