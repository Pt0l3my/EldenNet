# Third-party components and credits

## Design credit

EldenNet's engine follows the logic and structure of SkyrimNet, whose author, Min, permits EldenNet to build on it
(docs/DECISIONS.md, O53 and O84). EldenNet is not SkyrimNet, and contains none of SkyrimNet's code, prompts or
content.

## Header-only libraries

inja and doctest are vendored unmodified in `third_party/`, each with its licence file beside it. nlohmann/json and
toml++ are downloaded at configure time from their original projects, with their licence files, pinned by SHA-256
(`CMakeLists.txt`), into the build tree; release packages carry every licence file.

| Library | Version | Licence | Use |
|---|---|---|---|
| [inja](https://github.com/pantor/inja) | 3.4.0 | MIT (`third_party/inja/LICENSE`) | Prompt templates (en-prompt) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT (downloaded with it) | JSON for templates and requests |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | MIT (downloaded with it) | Configuration files |
| [doctest](https://github.com/doctest/doctest) | 2.4.11 | MIT (`third_party/doctest/LICENSE.txt`) | Unit tests only; never shipped |

## System libraries (not vendored)

| Library | Licence | Use |
|---|---|---|
| [libcurl](https://curl.se/libcurl/) | curl (MIT-style) | HTTP on Linux and test builds |
| WinHTTP | Part of Windows | HTTP on Windows: the system directory's own `winhttp.dll`, loaded by its full path at run time, never imported or shipped |

Each licence was read before adoption, and none carries a clause on AI use.
