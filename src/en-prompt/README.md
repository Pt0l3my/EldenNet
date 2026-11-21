# en-prompt: EldenNet's template engine

en-prompt turns a prompt template and a JSON context into chat messages for a model. It is built on
[inja](https://github.com/pantor/inja) 3.4 (MIT, vendored in `third_party/`), follows SkyrimNet's template
conventions (its message markers and its folders of numbered parts, SN03 §2–§3) and adds the strictness EldenNet's
design asks for (NPC §7.1, DESIGN §8.3, WHY§98, WHY§99): a prompt never goes out half-built.

## Using it

```cpp
#include "en/prompt/prompt.hpp"

// The tools: template names resolve from a folder on disk.
const en::prompt::Environment prompts("content");
const auto messages = prompts.render("prompts/dialogue", context);  // a folder of parts, or one file
// Each message has a role, text() and cache() (whether it ends at a cache breakpoint), and its parts: the stretches
// of text between its breakpoints, for providers with explicit cache control.

// The engine's core, which does no I/O (ARCH §2.3): the content pack's templates, already loaded, by name.
std::map<std::string, std::string> templates{{"prompts/dialogue/10_frame.j2", frame_text} /* , ... */};
const en::prompt::Environment core_prompts(
    std::make_shared<const en::prompt::MemorySource>(std::move(templates)));
```

Every name resolves through the environment's template source (`source.hpp`). A `FolderSource` reads a folder: a
name must lie inside it, and a link that leads out of it is refused. A `MemorySource` holds the names and texts it is
given, under the same name rules (a key such as `registers\calm.md` is kept as `registers/calm.md`; the folders are
the ones its names imply), and opens no file. The environment keeps the rules (names, a bare name's extensions,
folders of parts and their order, composition and its guards), and a source answers only what a name is, which files
a folder holds and what a file says, so the same templates give the same messages and the same errors from either.
Another source implements `TemplateSource`; one source may serve several environments and threads.

Every failure is an `en::prompt::TemplateError` that names the template, the line and the column, and the
`render_template` calls that led there. A failed render returns nothing. `Environment::lint(name, &context)` checks a
template, the parts of a folder and the templates they name, in every branch, without rendering. One environment may
be used from several threads.

From the command line (`eldennet-engine`, `src/en-tools/`):

```text
eldennet-engine render-prompt prompts/dialogue --context context.json [--root <dir>] [--json] [--no-merge]
eldennet-engine render-prompt prompts/dialogue --lint [--context context.json]
eldennet-engine render-prompt --list-functions [--json]    the function reference, or the lint's allowlist
```

## Templates

- **Messages.** `[ system ]`, `[ user ]` and `[ assistant ]` start a message; `[ end system ]` and the like may close
  one. `[ cache ]` puts a cache breakpoint where it stands, so at the end of a message it marks the message it ends
  (SkyrimNet's `[ cache ] … [ end cache ]` puts it at the end marker). `[ raw ] … [ end raw ]` keeps marker-like text
  as text. Every line is trimmed, blank lines and empty messages are dropped, and consecutive messages with one role
  are merged with a blank line, keeping their breakpoints. A line that looks like a marker but is not one
  (`[history]`, `[ sytem ]`) is an error.
- **Composition.** A prompt is a file (`.j2`, `.md`, `.prompt`) or a folder of numbered parts (`10_identity.j2`,
  `05b_world.j2`) rendered in numeric order. `render_template(name[, vars])` and
  `render_subcomponent(folder[, mode[, vars]])` render others by name from the root (`/` or `\` between folders);
  `object(key, value, …)` builds the vars. The included template sees the context and the vars, not the caller's
  `{% set %}` names. A render that comes back to a template being rendered, or nests deeper than 16 levels, fails.
- **Functions.** Only those in the registry (`render-prompt --list-functions`): inja's own, made strict, and ours,
  such as `capitalize`, `contains`, `empty`, `quote`, `split` and `to_json`. Each has typed parameters, a return kind,
  a one-line summary and whether its results may be reused; the reference and the allowlist are generated from it.
  The engine adds its own with `FunctionRegistry::add`.

## How it differs from stock inja

| Stock inja 3.4 | en-prompt |
|---|---|
| null prints as nothing | printing null is an error (a field left unset); `empty(x)` tests for it |
| any value is a condition, and the string "false" is true | conditions and `and`, `or`, `not` take booleans only |
| `"b" in "abc"` is false: `in` is equality on text | `in` and `contains()` test substrings, members and keys |
| `3 == "3"` is quietly false; `1 + "a"` throws with no place | the kinds must match, and every error has its line |
| `first([])`, `max([])` and `x % 0` are undefined; `int("")` throws with no place | null, or a located error |
| `loop.first`, `loop.last` and `loop.index0` fail | they work, beside `loop.is_first`, `loop.index` (from 0) and `loop.index1` |
| `not a == b` means `(not a) == b`, without a word | refused when the template loads: write `not (a == b)` |
| `##` at the start of a line opens a statement; an unclosed `{#` drops the rest | text; refused when it loads |
| `{% include %}` and `{% extends %}` open files by themselves | refused: the render functions compose, with their guards |
| `elif` fails to parse; there is no `capitalize` | `elif` works; `capitalize("")` is `""` |
| a record's `[ system ]` or `{{` is printed as it is | text printed from data stays text: its markers and braces are never read as markup |

`loop.index` counts from 0, as in inja and SkyrimNet, not from 1 as in Jinja. Not supported, because inja 3.4 lacks
them: filters (`x | upper`: write `upper(x)`), `{% raw %}` and macros. After rendering, a `{{`, `{%`, `{#`, `{name}` or
`${` left in a template's own text fails the render.

## Files

`include/en/prompt/`: `environment.hpp` (rendering, composition, lint), `source.hpp` (template sources: a folder on
disk, names and texts in memory), `messages.hpp` (markers and messages), `registry.hpp` (functions), `errors.hpp`.
`src/compile.cpp` rewrites inja's parsed tree once, at load, so that every operator, function, condition, loop and
printed value runs through checked code that knows its place in the template; `src/builtins.cpp` holds the functions;
`src/source.cpp` the sources and the one check of template names. The library holds no object of static storage with a
dynamic initialiser or an exit-time destructor (ARCH §3.1, §1.7). The tests in `tests/` use invented templates only. A
test of symbolic links runs where a link exists once made: Windows needs a privilege for one, MinGW's library cannot
make one, and Wine 9.0's `CreateSymbolicLinkW` answers success and makes nothing.