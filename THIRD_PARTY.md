# Third-party components and credits

## Design credit

EldenNet's engine follows the logic and structure of SkyrimNet, whose author, Min, permits EldenNet to build on it
(docs/DECISIONS.md, O53 and O84). EldenNet is not SkyrimNet, and contains none of SkyrimNet's code, prompts or
content.

## Header-only libraries

inja and doctest are vendored unmodified in `third_party/`, each with its licence file beside it. nlohmann/json and
toml++ are downloaded at configure time from their original projects, with their licence files, pinned by SHA-256
(`CMakeLists.txt`), into the build tree; release packages carry every licence file. Dear ImGui's core is downloaded the
same way, in the Windows builds alone, at the very pin GraceUI uses; GraceUI's `graceui-sys` headers are vendored, each
checked against its SHA-256 when the build is configured.

| Library | Version | Licence | Use |
|---|---|---|---|
| [inja](https://github.com/pantor/inja) | 3.4.0 | MIT (`third_party/inja/LICENSE`) | Prompt templates (en-prompt) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT (downloaded with it) | JSON for templates and requests |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | MIT (downloaded with it) | Configuration files (en-llm); gal/'s files (en-gal, built there with `TOML_EXCEPTIONS=0`) |
| [doctest](https://github.com/doctest/doctest) | 2.4.11 | MIT (`third_party/doctest/LICENSE.txt`) | Unit tests only; never shipped |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.0 (tag v1.93.0, commit `c1aaac65e896a45f81f47a4d68f9959e0f86c859`; each file's SHA-256 in `CMakeLists.txt`, GraceUI's own pins) | MIT (`LICENSE.txt`, downloaded with it) | The F11 view's frames (en-overlay): the core alone (`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` and their headers), headless, with GraceUI's `gu_imgui_core` definitions; no backend (DESIGN O25, GraceUI D156) |
| GraceUI's `graceui-sys` and ABI header | GraceUI `main` at `7c26477`, GraceUI 0.2.68, protocol 1.21, ABI 1.9 (D169 to D275; it contains `3f1352b`, D164, D165, and `c781a85`, D170) | MIT (`third_party/graceui-sys/LICENSE`, GraceUI's) | `graceui.h`, `graceui_sys.h`, `graceui_sys.hpp` and `graceui_imgui.hpp`: en-overlay's connection to GraceUI (part 12) and EldenNet's space (its pages, S11.4.a); `graceui_test.h`, the test build's exports, read by the fake game in the tests alone. All five vendored unmodified from the one commit, each checked against its SHA-256 at configure time (`CMakeLists.txt`) |

## SQLite

- **SQLite 3.54.1** (O146; MEM §8.5): its amalgamation, `sqlite-amalgamation-3540100.zip`, downloaded at configure
  time from SQLite's own site (`https://www.sqlite.org/2026/`), checked by the SHA3-256 its download page publishes
  (`92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`) and our SHA-256 of the verified file
  (`0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9`), into the build tree; only `sqlite3.c` and
  `sqlite3.h` are kept (`cmake/sqlite.cmake`). Compiled as the build's one C target, its warnings off, with FTS5 and
  the options `cmake/sqlite.cmake` lists. en-store alone links it, through its C API (ARCH §2.3).
- Licence: public domain. Its authors dedicated the code to the public domain (https://www.sqlite.org/copyright.html,
  and the blessing in the source's header in place of a legal notice), read on 2026-11-27, with no clause on AI use.
  It needs no notice; it is credited here.

## System libraries (not vendored)

| Library | Licence | Use |
|---|---|---|
| [libcurl](https://curl.se/libcurl/) | curl (MIT-style) | HTTP on Linux and test builds |
| WinHTTP | Part of Windows | HTTP on Windows: the system directory's own `winhttp.dll`, loaded by its full path at run time, never imported or shipped |

## Build tools and toolchain parts (not vendored)

| Component | Version | Licence | Use |
|---|---|---|---|
| MSVC CRT and Windows SDK, through xwin | CRT 14.43.17.13 (MSVC 14.43.34808), SDK 10.0.26100 (`cmake/xwin.lock`) | Microsoft's, accepted through xwin; never committed | The clang-cl build: the static CRT (`/MT`) linked into each Windows binary, and the SDK's headers and import libraries |
| [xwin](https://github.com/Jake-Shadle/xwin) | 0.10.0 | MIT OR Apache-2.0 | Downloads and splats the CRT and SDK; a build tool, never linked or shipped |
| LLVM's compiler-rt: the sanitizers' and libFuzzer's runtimes | 18.1.3 (Ubuntu's `libclang-rt-18-dev`) | Apache-2.0 WITH LLVM-exception | Linked into the sanitizer and fuzz builds' programs only; never shipped |

Each licence was read before adoption, and none carries a clause on AI use. Dear ImGui 1.93.0's `LICENSE.txt` (its
SHA-256 above) and GraceUI's `LICENSE` were read on 2026-11-20: MIT, with no such clause.

## Test data from other projects (never shipped)

- **BLAKE3's test vectors**, from [BLAKE3](https://github.com/BLAKE3-team/BLAKE3) at tag 1.8.2,
  `test_vectors/test_vectors.json` (SHA-256 `dcb91ea8accc77e6d6e632af7cdc1a99a9f3ae78cf648da595c7d064db32f624`):
  the first 32 bytes of the `hash`, `keyed_hash` and `derive_key` outputs of 16 of its cases, and the cases' key and
  context string. Licence: CC0 1.0, or Apache 2.0, or Apache 2.0 with LLVM exceptions, at the user's choice (its
  README's "Licenses"), read on 2026-11-27, with no clause on AI use. `src/en-proto/tests/test_blake3.cpp` checks our
  own BLAKE3 against them; only the numbers the tests need are copied, and no code.
