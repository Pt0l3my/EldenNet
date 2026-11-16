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
build/src/en-proto/en_proto_tests && build/src/en-prompt/en_prompt_tests && build/src/en-llm/en_llm_tests
```

## Windows with MinGW-w64, tested under Wine

```sh
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DEN_WITH_CURL=OFF
cmake --build build-win
WINEPREFIX=<dir> wine64 build-win/src/en-llm/en_llm_tests.exe     # and each other test program
```

The MinGW build links its runtime statically, uses WinHTTP (loaded from the system directory by its full path), and
skips en-gal, whose guarded copy needs structured exception handling.

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

- `XWIN_ROOT`, a cache variable or in the environment, points elsewhere than `~/.cache/xwin`. The toolchain file
  refuses a splat whose CRT (`crtversion.h`) or SDK differs from the lock.
- The flags are ARCH §2.6's: `/EHsc` (R3), `/W4 /WX /permissive- /utf-8`, the static CRT (`/MT`) in every
  configuration, `/Brepro` at compile and link, paths in debug information and `__FILE__` relative to the source tree,
  no incremental link; a release build adds ThinLTO, `/Z7 -gline-tables-only` and a PDB beside each binary. Nothing
  comes from `%INCLUDE%` or `%LIB%`.
- The CRT is 14.43, not the manifest's newest: 14.44's standard library refuses a Clang older than 19.
- en-llm's check that no test program imports `winhttp.dll` by name runs here too, with `llvm-objdump`.

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

```sh
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-O1 -DEN_FUZZ=ON
cmake --build build-fuzz --target en_fuzz
mkdir -p build-fuzz/work/sse && build-fuzz/fuzz/en_fuzz_sse build-fuzz/work/sse fuzz/sse/corpus \
  -dict=fuzz/sse/sse.dict -max_total_time=120 -max_len=8192 -timeout=10
```

New inputs go to the first folder, so the committed corpus changes only by hand. A finding is written as
`crash-<hash>` in the current folder; once it is fixed, it joins the target's `corpus/` under a name that says what it
found. An exception that escapes is a finding too, so the targets' `LLVMFuzzerTestOneInput` entries have no
`ffi_guard`: R3's CI grep is to leave `fuzz/` out. Rendering is fuzzed once its caps on output and steps are built
(roadmap S8.1.k): until then a template's loops may run for as long as they like.
