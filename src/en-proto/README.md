# en-proto: the messages between threads

The messages that cross a thread or the adapter boundary in EldenNet's engine (ARCH §2.2, §2.3), as plain data with
their JSON forms, the game access layer's types (the snapshot, the lifecycle messages, the read-only commands, the
game thread's token and the backend interface), and the guard on every entry that foreign code calls (R3). C++20, MIT.
It depends on the standard library and nlohmann/json only: no I/O, no threads, no clocks, no Win32 and no game
knowledge. Its static library `en_proto` is a module the DLL links (`en_dll_module`), so it holds no static object with
a dynamic initialiser or an exit-time destructor (ARCH §3.1, §1.7).

## What it holds

- **The guard** (`ffi_guard.hpp`, DESIGN R3, ARCH §10.1). `ffi_guard(component, fallback, f)` runs `f` inside a
  catch-all handler and returns its answer; on any exception it trips the component's flag and returns `fallback`.
  It is `noexcept`, and so is every entry that starts with it; the two-argument form is for entries that answer
  nothing. The components are those whose entries foreign code calls: `DllMain`, `Task`, `Present`, `Overlay`,
  `LlmTransport` (libcurl's and WinHTTP's callbacks, WinHTTP's driver threads), `LlmWorker` (the model handle's
  threads), `Tools` (the command's signal handler) and `Launcher` (eldennet-launcher's `wWinMain` and the system's
  callbacks into it), with room for 32. Each flag is a constant-initialised atomic:
  `tripped(c)` reads it, `clear_trip(c)` clears it. A trip changes nothing by itself: what it means (a toast, a
  subsystem off) is its owner's decision.
- **`Expected<T, E>`** (`expected.hpp`, ARCH §2.2): a value or the error that stands in for it, shaped like C++23's
  `std::expected` with no exceptions: `return Unexpected{why};`, `return {};` for `Expected<void, E>`, `has_value()`,
  `*`, `->`, `error()` and `value_or()`. The accessors have preconditions and callers check first (ARCH §2.4); there is
  no `value()` that throws. A value type whose constructor is private, such as en-gal's `BuildProfile` (R7), works,
  since nothing default-constructs it. It is trivially copyable when both sides are.
- **Fixed capacities** (`fixed.hpp`, ARCH §2.3): `FixedVector<T, N>` and `FixedString<N>` hold their items inline and
  never allocate; a full one refuses (`try_push_back`, `try_assign`) and says so, and slots past the end hold nothing
  of what they held. A `FixedString` holds well-formed UTF-8 with no NUL and nothing else (`valid_utf8`,
  `assign_prefix` cuts at a whole character). Flag sets are enum classes with the bitwise operators, opted in through
  `FlagSetTraits`, whose `~` keeps to the bits the set defines. Walks go through `std::span`, with no pointer
  arithmetic (ARCH §2.4).
- **The snapshot** (`snapshot.hpp`, ARCH §7.1, DESIGN §5.2): `WorldSnapshot` and its parts (`Session`, `Life`, `Ui`,
  `Player`, `Place`, `Clock`, `Boss`, `Chr`, `FlagFrame`, `TextEntry`, `ParamRowInfo`), the identities
  (`ChrHandle`, `BlockId` packed as `0xAABBCCDD`, `BuildKey`, `RegionKey`), the enums with our own names (the layout
  maps the game's raw values to them; `Unknown` means not read), the statuses and `StatusSet`, the eleven read groups
  with `GroupSet`, `ReadHealth` (per group `Untested`, `Ok` or `Tripped`, phase B passed, faults this tick; the task's
  last and p99 times) and `Capabilities` (the groups discovery resolved and phase A passed). The whole snapshot is
  heap-free and trivially copyable, about 13 KB.
- **The render feed** (`render.hpp`, ARCH §7.4) and **the lifecycle messages** (`lifecycle.hpp`, ARCH §9, C2):
  `DataReady{build, caps}`, `PlaythroughReady{slot, name, archetype, keepsake, play_time_ms}` and
  `GroupTripped{group}`, trivially copyable for their ring. The name is the player's character's: a recording replaces
  it with a keyed hash (ARCH §7.5), and nothing else carries it.
- **The read-only commands** (`command.hpp`, DESIGN §2.3): `SetWatchFlags` (a shared list of at most 2,048 ids),
  `SetTracked`, `ResolveText`, `ResolveParams` and `Noop`, the default. `ResolveParams` names its param besides its
  ids: the core asks for rows of NpcParam and of the grace and place-name params, whose ids overlap, which DESIGN
  §2.3's sketch left out.
- **The token and the backend** (`thread.hpp`, `backend.hpp`, ARCH §6.4, R1): `GameThread` is empty, cannot be copied,
  assigned or made but by `GameThread::issue()`, and only moves; a CI grep keeps `issue` to the task's trampoline and
  the replay harness (and the tests of the token and of backends). `GameBackend` takes a `const GameThread&` for every
  call: `probe`, `snapshot`, `render_feed` and `apply`, which answer `Expected<void, ReadError>` (`Inert`, `Loading`,
  `BudgetSpent`, `Faulted`, `Ended`) and `Expected<void, ApplyError>` (`Inert`, `Invalid`, `Unsupported`).
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
WorldSnapshot.chars: has more than 32 items
WorldSnapshot.health.groups: has an unknown group "weather"
the text is nested deeper than 32
```

The game messages follow the same rules (`game_json.cpp`), and:

- handles, blocks and build keys are integers; the snapshot's enums are their names (`"InGame"`, `"PopupMenu"`), the
  groups lower case as the design writes them (`"session"`), and a flag set the array of its members' names;
- a float that is not finite is `null`, and `null` reads back as NaN (a `Chr`'s `bearing_deg` without the camera);
- a flag frame's bit sets are hex strings, bit i in bit i % 8 of byte i / 8, trailing zero bytes left out;
- `ReadHealth.groups` is an object keyed by the groups' names; a param row's fields are `[index, value]` pairs;
- `Lifecycle` holds exactly one of `"data_ready"`, `"playthrough_ready"` and `"group_tripped"`, and `GameCommand`
  exactly one of `"set_watch_flags"`, `"set_tracked"`, `"resolve_text"`, `"resolve_params"` and `"noop"`;
- a container past its capacity, a string past its bytes, with a NUL or not UTF-8, and a region key other than
  lower-case letters, digits and `_` are refused.

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

`cmake --build <build> --target en_proto_tests`, then run `en_proto_tests` (under Wine for the Windows builds). A round
trip for every type, through JSON values and through text, with every field away from its default, every container
full and ids past 2^53; fields left out keep their defaults; malformed JSON refused with its path; each cap just under
and just over; `Expected` through copies, moves and assignments between its two states with no object leaked, and
with a value whose constructor is private; the fixed containers at and past their capacity, UTF-8 cut at whole
characters, the flag sets' bits; a backend of the test's own driven through the interface with a token; and the guard:
the answer when nothing throws, the fallback with only its own component tripped when anything is thrown.

CTest also runs R1's compile-fail tests (`tests/compile_fail/game_thread.cpp`, in every build, the cross builds too:
they only compile): copying a token, copying one from a reference, assigning one and making one without `issue()`,
plainly and braced, must each fail to build (`WILL_FAIL`), and the same file without them, the control, must build.

The fuzz target `en_fuzz_game` (`fuzz/game/`) reads arbitrary text as each of the four game messages and checks that
only `ProtoError` escapes, that what is read stays within its capacities and writes valid JSON, and that it reads back
to the same JSON form.
