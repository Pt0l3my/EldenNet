# Building EldenNet's engine

The engine is C++20 with CMake and Ninja (O60, ARCH §2.6). It builds four ways: on Linux with GCC or Clang, for the
tests that need no Windows; for Windows with MinGW-w64, whose test programs run under Wine; for Windows with clang-cl
and lld-link against Microsoft's CRT and SDK, the release build; and on Linux with Clang's sanitizers and libFuzzer.
Every build has 0 warnings, and every module the DLL links keeps `en_dll_module`'s rules (no static object with a
dynamic initialiser or an exit-time destructor; ARCH §3.1, §1.7).

## Linux: GCC or Clang

```sh
cmake -S . -B build -G Ninja                      # GCC; CXX=clang++ for Clang, which also checks en_dll_module's rules
cmake --build build
build/src/en-proto/en_proto_tests && build/src/en-prompt/en_prompt_tests && build/src/en-llm/en_llm_tests \
  && build/src/en-gal/en_gal_tests && build/src/en-overlay/en_overlay_tests
(cd build && ctest -R compile_fail)               # R1's compile-fail tests: they only compile, so every build runs them
```

CTest also runs en-gal's decoder against LLVM's disassembler (`en_gal_x86_objects`: every instruction of the build's own
`eldennet-engine` and `en_gal_tests`; `en_gal_x86_random`: 20,000 seeded random byte strings, assembled by the build's
compiler), which need `llvm-objdump` and Python 3, and `en_tools_cli`, which runs `probe-exe` and `lint --report` on the
derivation tests' invented program as DataPiler's job runs them.

`en_tools_cli` also runs `lint --gal` on the repository's gal/ (each file read as the DLL and probe-exe read it, each
build's layout with load_layout's checks in a dev build's mode, a derivation note for every pattern), and the spike
tools (`rec-stats`, `replay`, `spike-report`) on recordings that `en_tools_write_recording` writes as eldennet.dll's
core thread does, through en_shell's writers and en-core's slice, with invented spike logs.

en-gal now builds three libraries: `en_gal` (offline: the PE reader, patterns, schemas and the derivation kit),
`en_gal_game` (the part that runs in the game: GuardedReader, discovery, the read groups and the task, portable C++
over `SysLayer`) and `en_gal_game_probe` (the same with `EN_PROBE`, for the tests and the test DLL; the CMake option
`EN_PROBE`, off by default, builds it for a probe DLL too). `en_gal_tests` links the probe variant and runs the in-game
part over `FakeSys`, a process of bytes of the tests' own, in every build.

## Windows with MinGW-w64, tested under Wine

```sh
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DEN_WITH_CURL=OFF
cmake --build build-win
WINEPREFIX=<dir> wine64 build-win/src/en-llm/en_llm_tests.exe     # and each other test program
```

The MinGW build links its runtime statically, uses WinHTTP (loaded from the system directory by its full path), and
builds en-gal's portable part (the PE reader, the patterns, `builds.toml` and the layouts) and the derivation kit but not
its guarded copy, which needs structured exception handling: `en_gal_tests.exe` runs the portable part's tests. It
builds `en_gal_game` too, whose tests run on `FakeSys` there as everywhere; `NativeSys` and the test DLL need
structured exception handling, so they are clang-cl's alone. Its `eldennet-engine.exe`, like clang-cl's, runs the
derivation's offline round (R0, O89):
`eldennet-engine probe-exe <the game's executable> --derive --leads gal/leads.toml --out derive.json`, with the game
closed (src/en-tools/README.md has its exit codes); `probe-exe <exe> --sections` prints the section table's numbers
for an executable it refuses. With the tests on, the fake game's harness runs it under Wine on `fakegame.exe`
(`ctest -R fakegame.probe-exe`). The package the owner runs (R0, R0b, R0c: src/en-tools/README.md) takes
`eldennet-engine.exe` from the clang-cl Release build; `fakegame.exe` carries R0c's variants of the FD4 idiom and of
the group table, so the harness checks each of them end to end.

Both Windows builds build en-overlay's connection to GraceUI (part 12) and Dear ImGui 1.93.0's core for its F11 view,
which the top-level `CMakeLists.txt` downloads at configure time at GraceUI's own pin (tag v1.93.0's commit, each file by
its SHA-256; THIRD_PARTY.md). GraceUI's `graceui-sys` headers are vendored in `third_party/graceui-sys` from GraceUI at
f51bcc7, each checked against its SHA-256 when the build is configured. `en_overlay_tests.exe` runs the connection's
tests against a fake GraceUI table of our own under Wine; on Linux, `en_overlay_tests` runs the view's lines and the
world-state claims.

## The Windows build with clang-cl

The Windows binaries are built on Linux with clang-cl and lld-link (LLVM 18) against the MSVC CRT and Windows SDK that
[xwin](https://github.com/Jake-Shadle/xwin) (MIT or Apache-2.0) downloads (O60, ARCH §2.6), pinned in
`cmake/xwin.lock`. Microsoft's licence covers those files: xwin asks for its acceptance, and they stay in a cache
outside every repository.

```sh
cargo install xwin --locked --version 0.10.0     # or its release archive for x86_64-unknown-linux-musl
xwin --accept-license --manifest-version 17 --channel release --arch x86_64 --variant desktop \
     --sdk-version 10.0.26100 --crt-version 14.43.17.13 splat --output ~/.cache/xwin
cmake -S . -B build-cl -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-xwin.cmake -DEN_WITH_CURL=OFF
cmake --build build-cl
WINEPREFIX=<dir> wine64 build-cl/src/en-gal/en_gal_tests.exe     # and each other test program
```

`en_gal_tests.exe` holds the portable part's tests and, in this build alone, the guarded copy's. It adds NativeSys's
tests (the module's image, both copy primitives, the stack table and a guard page left untouched, the calls into "the
game" under SEH, and the task object driven as a hub would drive it), and builds `tests/galtest/galtest.dll`, which the
fake game's harness loads into `fakegame.exe` (below). `ctest -R compile_fail` runs R1's compile-fail tests here too.
The same `probe-exe` harness cases run here, where the fake game's MSVC RTTI lets them check the RTTI walk too.

- `XWIN_ROOT`, a cache variable or in the environment, points elsewhere than `~/.cache/xwin`. The toolchain file
  refuses a splat whose CRT (`crtversion.h`) or SDK differs from the lock.
- The flags are ARCH §2.6's: `/EHsc` (R3), `/W4 /WX /permissive- /utf-8`, the static CRT (`/MT`) in every
  configuration, `/Brepro` at compile and link, paths in debug information and `__FILE__` relative to the source tree,
  no incremental link; a release build adds ThinLTO, `/Z7 -gline-tables-only` and a PDB beside each binary. Nothing
  comes from `%INCLUDE%` or `%LIB%`.
- The CRT is 14.43, not the manifest's newest: 14.44's standard library refuses a Clang older than 19.
- en-llm's check that no test program imports `winhttp.dll` by name runs here too, with `llvm-objdump`.

## The fake game and the Wine harness

`tests/fakegame/` is our own Windows program that stands in for a game in the tests (its README says what each part
stands in for; nothing in it is a game's), and `tests/vehprobe/` a DLL with a vectored exception handler, standing in
for a crash reporter. Both build in the two Windows builds when the tests are on. CTest runs the fake game's cases
under Wine on Xvfb, one test per case of `tests/fakegame/harness.py`, and checks each from outside: the exit code, the
status lines, and for `--describe` the executable file itself.

```sh
cmake -S . -B build-cl -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-xwin.cmake -DEN_WITH_CURL=OFF \
  -DEN_WINE_PREFIX=<dir>                                   # optional: one prefix for several build trees
cmake --build build-cl
(cd build-cl && ctest -R fakegame --output-on-failure)     # the same in build-win for MinGW
```

- It needs `wine64` (Wine 9.0 here; found in `/usr/lib/wine` or on the path, `EN_WINE`), `xvfb-run` (`EN_XVFB_RUN`)
  and Python 3. Without them the harness's tests are left out, with a line saying so; the programs still build.
- Wine 9.0 aborts at start-up ("free(): invalid pointer") when `TMPDIR` is set. The harness starts Wine without it; run
  any Wine command of your own as `env -u TMPDIR wine64 …` (WHY§210).
- The first test, `fakegame.prefix`, makes the prefix (`EN_WINE_PREFIX`, by default `<build>/wineprefix`, about 700 MB)
  and sets it up for unhandled exceptions (again each time it runs, so an older prefix is brought up to date): no crash
  dialog (with a display, an unhandled exception would otherwise wait on it for ever) and no JIT debugger. Wine
  otherwise starts the debugger that `AeDebug` names, `winedbg --auto`, which attaches to a crashing program; a crash
  that no filter of the program's own ended then exited with status 0 instead of its code in about one run in ten.
  `AeDebug`'s `Debugger` now names no program, so the process ends with the exception's code, as on a Windows PC with
  no JIT debugger. Every case runs under `xvfb-run -a`, no two at once (`RESOURCE_LOCK en_wine`), and starts and ends
  with `wineserver -k` and `wineserver -w`, so no case meets an earlier case's server while it shuts down.
- The prefix's set-up also gives wined3d its Vulkan renderer (`HKCU\Software\Wine\Direct3D`, `renderer = vulkan`), as
  GraceUI's BUILD.md does: only the GraceUI cases' fake game (`--d3d12`) makes a DXGI factory, and wined3d's OpenGL
  renderer hung on Xvfb in GraceUI's harness.
- **GraceUI's cases** (`ctest -R fakegame.launcher-eldennet-graceui`, clang-cl) need a test build of GraceUI, built from
  GraceUI's own repository as its BUILD.md says, in a build tree of its own:

  ```sh
  cmake -S <graceui> -B <graceui-build> -G Ninja -DCMAKE_TOOLCHAIN_FILE=<graceui>/cmake/clang-cl-xwin.cmake \
    -DCMAKE_BUILD_TYPE=Release -DGRACEUI_TEST_EXPORTS=ON
  cmake --build <graceui-build>
  cmake -S . -B build-cl ... -DEN_GRACEUI_DIR=<graceui-build>/bin    # graceui.dll and fonts\ are there
  ```

  Without `EN_GRACEUI_DIR` they are left out, with a line saying so. They need Mesa's lavapipe (`mesa-vulkan-drivers`)
  for Wine's Direct3D 12, as GraceUI's harness does.
- Under Wine a program's exit status keeps only the low 8 bits of its Windows exit code (0xC0000005 arrives as 5).
- About 13 minutes for the clang-cl build's 98 cases (21 of them the launcher's, 15 eldennet.dll's, three of those with
  GraceUI, below): 52 of about
  2 to 8 s each, and the ten `gal-*` cases of part 4, 12 to 25 s each, which first write gal/ from `probe-exe --derive`
  on `fakegame.exe` (with the AOB statics' slots, as R1 would find them) and `tests/fakegame/gal-layout.toml.in` (its
  offsets checked against `--describe`), then run the fake game with `galtest.dll` loaded and check its `GALTEST` lines.
  The MinGW build runs 60 (no `__try`, GCC's own RTTI, and no test DLL).
- A `gal-*` case's environment is the DLL's settings: `EN_GALTEST_DIR` (gal/), `EN_GALTEST_COPY=rpm`,
  `EN_GALTEST_FAULT=<group>:<every>` and `EN_GALTEST_FAULT_AFTER=<s>`, and `EN_GALTEST_STATUS` for a run on Windows.
- `launcher-eldennet-probe` runs R1 as the owner runs it (`--probe`, no settings.toml, invented saves backed up first,
  Pause pressed every second from the title until taken), about 65 s; `launcher-eldennet-probe-release` gives
  `--probe` to the release DLL, which refuses it and stops. The `launcher-eldennet` case also runs `rec-stats` and
  `replay` on its recording, and `launcher-spike-s0a` runs `spike-report s0a` on its log.
- The release configuration (`-DCMAKE_BUILD_TYPE=Release`, ThinLTO) passes the same cases.

## The launcher and the core's slice

`src/en-core/` holds the core's v0.1 slice (part 5): the lifecycle without the store, the offline and multiplayer
gate with its hysteresis, L1, L2 and Dormant; pure, so `en_core_tests` runs in every build. `src/en-launcher/` builds
`en_launcher_core` everywhere (Steam's library files, the save backups with their BLAKE3 manifests, the appid file and
its record, launcher.toml, the command line, the texts, the watcher's records and the spikes' tally;
`en_launcher_tests`) and, in the two Windows builds, `eldennet-launcher.exe` (part 7). The launcher shares en-proto's watch block with
EldenNet in the game. Its README has the command line and what each step does. Both link en-gal's offline part,
en-proto and the header-only libraries alone (ARCH §2.3): `en_link_rule` (`cmake/link_rules.cmake`) stops the
configuration if either reaches any other target of the build, en-core or en_shell among them, and names the chain;
`ctest -R en_link_rules` checks the rule itself on a project of its own (`tests/linkrules/`).

The launcher's end-to-end cases join the fake game's harness (`ctest -R fakegame.launcher`): 11 in both Windows builds
(no DLL), and 10 more in the clang-cl build with `tests/galtest/galtest.dll` as the DLL, which now also does EldenNet's
part of the watch block (`watch_client.cpp`) and steps en-core's slice on its snapshots. A case that starts a process
of its own (the watcher galtest starts) waits for every process of the prefix to end before the next case
(`settle`). About 3 minutes for the 21 clang-cl cases; the S0 loops and the injections are the longest (10 to 20 s).

In the cross builds CTest lists the unit-test programs too, which it cannot run on Linux: run them under Wine as
above (`en_core_tests.exe`, `en_launcher_tests.exe` among them).

## eldennet.dll

`src/eldennet/` (part 6) builds `en_shell` everywhere (settings.toml as v0.1 reads it, the scrubbed log's lines, the
channels, the recorder's numbers-only lines; `en_shell_tests`; R1's probe report is en-gal's) and, in the clang-cl
build alone, `eldennet.dll` and, with the tests or `EN_PROBE` on, `probe/eldennet.dll`, the same DLL with the probe
option (R1).
Its README has the start as built, the threads, the settings, the recorder and R1.

- `ctest -R eldennet_dll_checks` (clang-cl) runs ARCH §12.1's greps on `src/eldennet` and reads both DLLs with
  `llvm-objdump`: `eldennet_instance_marker` their only export, their imports within `src/eldennet/tests/imports.allow`
  (kernel32 and user32 only). A function a new build imports must be reviewed into that list.
- The DLL connects to GraceUI on T1 (part 12; `overlay.connect`, on by default) and, with `debug.panel = true`, shows its
  F11 debug view; src/eldennet/README.md and src/en-overlay/README.md have what each does.
- `ctest -R fakegame.launcher-eldennet` runs the DLL under Wine with the real launcher and the fake game: 12 cases,
  about 4 minutes, and with `EN_GRACEUI_DIR` three more with GraceUI, about 1 minute (`launcher-eldennet-probe` is the longest, about 65 s). Each installs the DLL in its own folder with
  gal/ beside it and gives it an %APPDATA% of its own through `ELDENNET_APPDATA`, which the DLL takes in `%APPDATA%`'s
  place (development and tests).
- `eldennet-engine lint --recording <file>` checks a recording (`*.ensnap.jsonl`), line by line.
- Each DLL's version resource is configured from `src/eldennet/win/eldennet.rc.in`: ProductVersion is the build's
  label (`EN_VERSION_LABEL`, by default the project's version), and the probe build's carries SpecialBuild "probe"
  (VS_FF_SPECIALBUILD). The label is in the log's boot line too. `eldennet-launcher --probe` asks the probe build for
  R1 through the watch block (src/en-launcher/README.md).

## The spike package (part 10)

A spike package is a development build for the owner's PC, labelled `0.1.0-spike.N` and never shipped
(docs/design/spikes/README.md has its files and the S0 runs). R1's is the first:

```sh
cmake -S . -B build-cl-rel -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-xwin.cmake -DEN_WITH_CURL=OFF \
  -DCMAKE_BUILD_TYPE=Release -DEN_PROBE=ON -DEN_VERSION_LABEL=0.1.0-spike.5
cmake --build build-cl-rel
python3 -I tools/spikepkg/build.py r1 --build build-cl-rel --out EldenNet-R1.zip \
  --engine build/src/en-tools/eldennet-engine     # a Linux engine of the same commit: lint --gal first
```

`EN_VERSION_LABEL` (the top-level CMakeLists.txt): `-DEN_VERSION_LABEL=0.1.0-spike.N` labels the launcher's log line,
`eldennet-engine version` and the DLL; any other value must look like `0.1.0` or `0.1.0-word.N`.

`build.py` refuses a tree that is not clang-cl, Release, with `EN_PROBE` and a spike label, a DLL that is not the probe
build, and a text file holding a path of this PC; the zip is the same for the same inputs. Before it leaves: unzip it,
`sha256sum -c SHA256SUMS`, run `eldennet-launcher.exe --help --no-dialogs` and `eldennet-engine.exe version` under
Wine, run the harness's `launcher-eldennet-probe` case with `--set launcher=`, `--set eldennet_probe=` and
`--set engine=` pointing at the unzipped files (the owner's steps on the fake game), and sweep it
(`python3 -I ~/.config/privsweep/privsweep.py files <zip>`).

## Checks: compile-fail tests and CI greps

R1's compile-fail tests (ARCH §10.1): CTest builds `src/en-proto/tests/compile_fail/game_thread.cpp` once as a control,
which must build, and once for each forbidden use of the game thread's token (a copy, a copy from a reference, an
assignment, a token made without `issue()`, plainly and braced), each of which must fail (`WILL_FAIL`). They take the
build tree's lock one at a time (`RESOURCE_LOCK`), and run with `ctest -R compile_fail` in the cross builds too.

R1's CI grep (ARCH §10.1, §12.1) is to allow `GameThread::issue` only in `src/en-gal/src/task.cpp` and
`src/en-tools/src/replay.cpp`, but en-proto's own tests call it too (`src/en-proto/tests/test_thread_backend.cpp` and
the compile-fail file). When the grep is written, its allowlist needs the token's tests (and later the GAL's `FakeSys`
tests), or the grep should leave the `tests/` folders out, as R3's leaves `fuzz/` out.

## Sanitizers and fuzzing

On Linux with Clang (`-DCMAKE_CXX_COMPILER=clang++`, with Clang's compiler-rt: Ubuntu's `libclang-rt-18-dev`),
`cmake/sanitizers.cmake` builds the engine for ARCH §12.1's test and fuzz stages. Configure each as `Debug` with
`-DCMAKE_CXX_FLAGS=-O1`: fast enough, and nlohmann/json keeps its own assertions (no `NDEBUG`).

- `-DEN_SANITIZE="address;undefined"`: AddressSanitizer, with LeakSanitizer at exit, and UndefinedBehaviorSanitizer,
  which halts at its first report as AddressSanitizer does.
- `-DEN_SANITIZE=thread`: ThreadSanitizer. Its static runtime defines every form of `operator new` and `delete`
  strongly and is linked whole, so a program that replaces them, as `en_llm_tests` does for its allocation faults,
  could not link. This build links the shared runtime instead, found through an rpath to Clang's runtime folder, and
  the program's own forms take precedence.
- `-DEN_FUZZ=ON`: the libFuzzer targets in `fuzz/`, built with `address;undefined`, every module compiled with
  libFuzzer's coverage (`-fsanitize=fuzzer-no-link`).

Each sanitizer ends a program it reported on with a failing status, so CTest fails on any report:

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-O1 \
  "-DEN_SANITIZE=address;undefined"
cmake --build build-asan && (cd build-asan && ctest --output-on-failure)
```

### The fuzz targets

`fuzz/<target>/` holds a target's source, a dictionary of the tokens its inputs are made of (for libFuzzer's `-dict`)
and a seed corpus of invented inputs, which also keeps the input of every finding, once fixed, as a regression seed.
CTest replays each corpus once (`en_fuzz_<target>_corpus`), and `--target en_fuzz` builds every target.

| Target | Its input | A finding, besides any sanitizer report |
|---|---|---|
| `en_fuzz_sse` | Three bytes (how the transport cuts the body into chunks, the line cap, the visible text's cap: each the built cap below `0x80`, else a small one), then a reply's body, through en-llm's SSE parser and stream assembler as the client drives them, as a stream and as one JSON body | Events, or a failure, that change with the cuts; visible text that is not what the steps delivered, passes its cap or splits a UTF-8 character; what `complete_sentences` keeps not ending a sentence; anything shaped like a key in an error's text, in the stream or as an error body |
| `en_fuzz_json` | A byte for the caps (the built 256 KB and 32 deep below `0x80`, else small ones), then a JSON text, through en-proto's `parse_json`, and `read_json` for `LlmRequest` and `LlmEvent` | A refusal other than the caps give (TooLarge, else TooDeep, else Malformed exactly when nlohmann/json refuses the text); a value nested deeper than the cap; an exception other than `ProtoError`; a message that, written back, does not read again to the same |
| `en_fuzz_template` | Up to five templates split at NUL bytes, under fixed names (a prompt, a folder of numbered parts, a template either may render), each linted with and without a context: every template it reaches is compiled, none is rendered | An exception from `lint`; a template that compiles differently once others have been compiled |
| `en_fuzz_game` | A byte choosing one of the four game messages (`WorldSnapshot`, `Lifecycle`, `GameCommand`, `RenderFeed`), then a JSON text, through `read_json` | An exception other than `ProtoError`; a container past its capacity; what is read writing invalid JSON text, or not reading back to the same JSON form |
| `en_fuzz_pe` | A byte (the layout; an unreadable 4 KiB page or none; 4 KiB or 1 MiB copies), then an image, through en-gal's `read_image` and `scan_patterns` with three invented patterns | An accepted image whose sections (the loader's rules), what a file holds of them, their bytes as loaded, function table, product name or placement disagree with the reader's rules, or whose section table reads otherwise alone; a refusal it cannot word, or a section's without its fault or longer than 192 characters; a resolved pattern whose hits break ARCH §6.2's rules |
| `en_fuzz_pattern` | A pattern's text, NUL, a byte for the chunk size, then bytes to search | An accepted pattern outside the grammar's limits, or whose canonical text reads back differently; a refusal pointing outside the text; the matcher, fed in chunks, finding other than a plain search finds, or missing a planted pattern |
| `en_fuzz_galtoml` | A byte choosing `patterns.toml`, `builds.toml` or a layout (for a build gated on an invented image), then the text | A refusal without its problems, or past the cap; an accepted file breaking its schema's rules (unique ids, own-derivation, captures within patterns, keys, layouts under `layouts/`, fields within their classes, classes named that exist) |
| `en_fuzz_x86` | Bytes, decoded at every offset (each from an exact-size copy) and as a sweep | An instruction longer than 15 bytes or than the bytes given; its parts out of order, outside it, or not ending it; a RIP-relative operand without ModRM and a 32-bit displacement; bytes after it changing it; a shorter run not refused as truncated; "truncated" with 15 bytes given |
| `en_fuzz_report` | A byte for the mode, then a text for the lint, or a run of writer calls (keys and values from the input) | A text the lint accepts that is not one JSON object of words, labels, patterns and whole numbers; a report the writer gives that the lint refuses, or that holds a string offered that is not a word or a label |
| `en_fuzz_derive` | Two bytes for a static's slot, then an executable, derived with invented leads as probe-exe does | A report the derivation writes that the lint refuses; a pattern outside 1 to 64 bytes with 4 fixed, or a capture outside its pattern; a complete run without its idiom, registration or group |
| `en_fuzz_config` | Up to four texts split at NUL bytes: providers.toml, models.toml, settings.toml and secrets.toml, through en-llm's `parse_config` | Anything but a `ConfigError` leaving it; an abort (toml++'s assertions, which en-proto's pre-check must keep it from) |

```sh
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-O1 -DEN_FUZZ=ON
cmake --build build-fuzz --target en_fuzz
mkdir -p build-fuzz/work/sse && build-fuzz/fuzz/en_fuzz_sse build-fuzz/work/sse fuzz/sse/corpus \
  -dict=fuzz/sse/sse.dict -max_total_time=120 -max_len=8192 -timeout=10
```

`en_fuzz_pe` takes `-max_len=65536` so that whole invented images fit; `en_fuzz_galtoml` builds
`src/en-gal/tests/fake_image.cpp` in, for its gated build. `en_fuzz_derive` takes `-max_len=65536`; its seed is the
derivation tests' invented program.

`en_fuzz_galtoml` and `en_fuzz_config` found inputs that reach toml++ 3.4.0's assertions, which are undefined behaviour
in a release build (src/en-gal/README.md lists them). The check is en-proto's `unsafe_for_toml` (toml_check.hpp), run
before every `toml::parse` in the engine: gal/'s files with gal/'s rules, the player's configuration files with the
player's (a letter outside ASCII may also stand inside a quoted string).

New inputs go to the first folder, so the committed corpus changes only by hand. A finding is written as
`crash-<hash>` in the current folder; once it is fixed, it joins the target's `corpus/` under a name that says what it
found. An exception that escapes is a finding too, so the targets' `LLVMFuzzerTestOneInput` entries have no
`ffi_guard`: R3's CI grep is to leave `fuzz/` out. Rendering is fuzzed once its caps on output and steps are built
(roadmap S8.1.k): until then a template's loops may run for as long as they like.
