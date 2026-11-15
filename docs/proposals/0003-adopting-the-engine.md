# Proposal 0003: Adopting Min's engine

**Status: proposal, 2026-11-15 (roadmap S6.1.b). Superseded the same day: the owner chose our own engine (O84), so O80
to O83 were withdrawn. Kept as the record of the engine's evaluation.** Where this text and the designs differ, the designs
hold. A bare §n is a section of this proposal; DESIGN and ARCH are EldenNet's design documents. The facts behind it
are in WHY§95; the engine itself stays outside the repositories, as its licence and our rules require.

## 1. What arrived

On 2026-11-15 the owner supplied Min's engine (S6.1.a): FatelessAiEngine 0.2.0, the first version Min released to a
group of early adopters. It is not the source blueprint O53 expected but a closed Windows DLL, with libraries it loads,
MIT headers for its interface, a host guide and a binary licence (WHY§95). EldenNet may ship it unmodified inside its
own releases, with its licence and notices, and builds on its interface; it may not change it. Min says the content
library and content store come next.

## 2. What EldenNet adopts (O80)

Recommended (O80 (a)): the engine for everything it offers that EldenNet's design planned to build itself.

| The engine gives | It replaces in EldenNet's plan |
|---|---|
| LLM requests: variants, failover, streaming, structured output | The OpenAI-compatible streaming client (ARCH §11) |
| Typed decisions (a choice, a score, yes or no) | Decision calls built on plain requests |
| Prompt templates: Inja with SkyrimNet's additions | Our own template engine; SkyrimNet's prompts port once EldenNet answers the same functions |
| Speech to text (local whisper), voice detection, text to speech (a local model) | The STT, VAD and TTS seams (DESIGN §13) |
| Model downloads, checked by hash, on request | Our own downloader |

What stays EldenNet's: the context engine and memories, the event engine, the job slots, budgets, pacing and
per-request deadlines (the engine has no deadline; EldenNet cancels), audio playback, game access, the spoiler layers,
and every page. EldenNet keeps its own seams around the engine, so that a later engine version, or another engine,
changes one layer.

## 3. Where it runs (O81)

The engine is built to load into a host's process. It blocks for seconds when its services are created, reloaded or
released; it calls back on its own threads; and 21 of its 22 libraries keep their upstream names (`ggml.dll`,
`whisper.dll`, FFmpeg's), so another mod's copy of one could be loaded in their place inside the game.

Recommended (O81 (a)): **in EldenNet's launcher**, out of the game's process. The launcher already stays running as the
crash watcher (O71), runs code plugins outside the game (O70) and will run EldenNet's engine off game (O76). There, a
fault in a library cannot take the game down, nothing clashes with other mods' libraries, and blocking calls are
harmless. EldenNet's DLL in the game asks the launcher over a local channel, one hop per request, small beside a
model's answer. The alternative, (b), loads the engine inside the game, as the SDK is designed for.

## 4. Online services and privacy (O82)

What already follows from the owner's rules, whatever O82 says:

- **No telemetry.** EldenNet never creates the engine's telemetry service, so nothing is sent at launch or later.
- **No game audio leaves the PC.** The engine can upload a game file's voice so that its cloud clones it for every
  player; that would send the game's content off the PC, which O74 and O75 rule out.
- **Logs stay on the PC** and are written at the engine's Info level or above.

The question (O82) is Min's own online service, fateless.ai: a sign-in, Fateless's models and its cloud speech, paid
for by the player. Recommended (O82 (a)): **offered as an opt-in provider, off by default**, asking for the basic
scope only; prompts then go to fateless.ai and the models behind it, as they would to any provider the player picks.
(b) not in v1; (c) offered by default.

## 5. API keys (O83)

EldenNet keeps keys in `secrets.toml`, in the player's own folder, never logged or bundled (DESIGN §11.4). The engine
takes keys only from a plain-text provider file in its configuration folder, which it rewrites at each load.

Recommended (O83 (a)): `secrets.toml` stays the one store; EldenNet writes the engine's provider file from it at each
start, into a folder only the player's Windows account can read, never logged or shown; and the owner asks Min for a
way to pass keys in memory. (b) Only routes that need no key (local servers, and Fateless if O82 allows it) until Min
adds that.

## 6. Shipping

- The DLL and its libraries unmodified, with `LICENSE.txt` and `THIRD_PARTY_NOTICES.txt` beside them.
- The FFmpeg libraries as separate, replaceable files; their source shipped beside them or offered in writing for
  three years (LGPL-2.1). The source archive is a separate download on Min's release, still to fetch.
- The CUDA library (169 MB) left out of the main download; the engine falls back to Vulkan or the CPU.
- Before loading: a check for AVX2 and for the Visual C++ runtime 14.40 or newer, each with a plain message and
  EldenNet running without the engine's local speech if one fails; and the engine's major and minor version compared
  with the headers EldenNet was built with.
- About 135 MB without the CUDA library, before any model.

## 7. The re-base (S8)

Once O80 to O83 are answered: ARCH §11 is rewritten around the engine; the prompt files follow SkyrimNet's template
rules, which Skyrizz's notes already describe; the launcher's part grows as O81 says; DESIGN §13's voice seams name the
engine's backends. O60 stands: the engine's interface is COM, natural in C++. O53 gets a dated note: the engine itself
is closed; what Min gives in a form we may adapt (prompts, templates, configuration, the dashboard) follows O53 as
written, each under its own licence.

## 8. Questions for Min (for the owner to pass on)

1. Is the engine's source, or more of it, planned for those building on it?
2. Can keys be passed in memory rather than in the provider file? What is that file's schema?
3. The model catalogue: its hosts, the files' hashes, and the models' licences.
4. What the launch ping would send, and whether it carries an install id (EldenNet would not create it, but the
   answer belongs in the notes).
5. Do the engine's logs at Debug level hold prompts, keys or paths?
6. Are the earlier attempts that keep running during a failover billed?
7. Could a build for language models only leave out the speech libraries, and with them FFmpeg?
8. The FFmpeg source archive for 0.2.0.

## 9. Questions for the owner

### O80: What does EldenNet adopt from the engine?

(a) **Recommended.** Everything it offers that EldenNet planned to build: LLM requests and decisions, prompt
templates, speech to text, voice detection, text to speech and model downloads, behind EldenNet's own seams (§2). (b)
Language models and templates first, speech later. (c) Nothing yet: wait for the content store.

### O81: Where does the engine run?

(a) **Recommended.** In EldenNet's launcher, out of the game's process; EldenNet's DLL asks it over a local channel
(§3). (b) Inside the game's process.

### O82: Min's online service, fateless.ai?

(a) **Recommended.** An opt-in provider, off by default, basic scope only (§4). (b) Not in v1. (c) Offered by default.

### O83: How do API keys reach the engine?

(a) **Recommended.** EldenNet writes the engine's provider file from `secrets.toml` at each start, in a folder only
the player's account can read, and the owner asks Min for keys in memory (§5). (b) Keyless routes only until Min
adds that.
