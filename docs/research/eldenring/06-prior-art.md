# Prior art and lessons: LLM NPC mods, frameworks and demos, distilled for an Elden Ring v1

> **Scope:** a survey of LLM-driven NPC systems in other games (Skyrim, Fallout 4, Bannerlord, Kenshi, RimWorld, Minecraft, Cyberpunk, commercial titles and vendor demos). It also covers every Elden Ring / FromSoftware AI-dialogue or voice experiment we could find. From these it draws design lessons for **EldenNet v1**: named NPCs get personality, awareness and memory with free-form chat; generic enemies get short barks; dialogue only, no NPC actions.
> **Research date:** 2026-11-06. **Game state:** ELDEN RING 1.17.2 (see [01-engine-internals.md](01-engine-internals.md)).
> **Evidence tags:** **FACT** = sourced from code, config or a primary page. **LIKELY** = several weaker signals agree, or the only source is a search-result snippet. **SPECULATION** = my inference, untested.
> **Access limits:** this session could read GitHub (clones, raw files, search API) and web-search snippets. It could **not** open nexusmods.com, youtube.com, reddit, soulsmodding.com or vendor sites, because those hosts are not allowed in this session's network. Every claim about a Nexus page, YouTube video or Reddit thread therefore rests on search snippets and is tagged LIKELY. The repositories were read only, never built or run. Mantella is AGPL-3.0 and CHIM's server is GPL-3.0-only, so this report describes their designs and does not copy their code.

> **Fact-check pass (2026-11-06, adversarial).** Every flagged claim was re-checked against upstream sources. `git ls-remote` showed the Mantella, HerikaServer and SkyrimNet clones still match upstream HEAD. New sources were found on GitHub (see the Verification log at the end). These corrections change the design: CHIM's default bark cooldown is **30 s, not 90 s**. llama.cpp's default port is now **9931**. Pocket TTS `serve` is **not** OpenAI-compatible. SkyrimNet now sells its own **paid inference (Fateless)**. EldenEncouragement's source is public. And there is **an in-process Elden Ring LLM bridge** (game-assistant / Attack on Elden Ring), so the old claim that "all ER AI companions are out-of-process" is wrong. The checker had the same network limits as the author, plus an exhausted web-search budget: nexusmods.com, openrouter.ai, developer.nvidia.com and blog.player2.game could not be opened. Claims that depend on those sites stay LIKELY or Unverified.

**Local clones read (shallow read-only clones of the public repositories):**

| Prefix | Repository | Commit / date |
|---|---|---|
| `MAN/` | `art-from-the-machine/Mantella` (main) | `c0c1ae6`, 2026-07-16 ("Log time to tts response") |
| `MAND/` | `art-from-the-machine/Mantella` (gh-pages docs) | shallow clone, 2026-11-06 |
| `CHIM/` | `Dwemer-Dynamics/HerikaServer` (CHIM server) | `6b45d9f`, 2026-10-30 |
| `SN/` | `MinLL/SkyrimNet-GamePlugin` | `ae49a63`, 2026-11-04 |
| `LRA/` | `lraty-li/Elden-Ring-Mods` (Chat with Melina) | already cloned for report 03 |
| `EEN/` | `mmahoney09110/EldenRingEncouragement` (source of Nexus 7999; MIT) | `d82ae15`, 2026-09-19 (fact-check) |
| `GA/` | `mpere056/game-assistant` (LLM companion; MIT) + `mpere056/attack-on-elden-ring` (its in-process bridge) | `b35c36a`, 2026-10-17 / `07ec7d8` (fact-check) |
| `PTTS/` | `kyutai-labs/pocket-tts` | shallow clone, 2026-11-06 (fact-check) |

---

## TL;DR

- **No LLM *NPC-dialogue* framework exists for Elden Ring, Dark Souls, Sekiro or Bloodborne (LIKELY; GitHub-only search, because Nexus, Bilibili and YouTube could not be searched).** We found four kinds of thing:
  1. *Companion overlays* that run as external programs. EldenEncouragement and its successor Un-Alone (Nexus 7999/8046) read the game with `ReadProcessMemory` and get text from an author-hosted proxy (`…onrender.com/api/OpenRouter`). By default they play pre-generated static voice clips; Azure TTS is optional with the user's own key (FACT, MIT source at `mmahoney09110/EldenRingEncouragement`).
  2. *An LLM companion with an in-process bridge* (corrected: earlier text said none existed). `mpere056/game-assistant` (2026-10) is a Claude-backed "fairy" assistant. It reads ER through the me3-loaded *Attack on Elden Ring* bridge DLL, which publishes nearby characters (NpcParam id, position, HP, hostility) to shared memory. It uses local kokoro-onnx TTS and faster-whisper STT. It is an assistant, not NPC dialogue.
  3. *Scripted* dialogue mods with no LLM (Chat with Melina).
  4. *Out-of-game* lore bots and overlays (RAG bots, Hakko AI, Questbot, GraceGuide).

  EldenNet would be the first framework that gives the game's **own NPCs** LLM dialogue from inside the game. The in-process bridge pattern itself already exists.
- **The Skyrim lineage has tried all three process models.** Mantella uses a Python exe, a FastAPI server on port 4999 and the SKSE HTTP plugin. Herika → AI-FF → CHIM uses a PHP + PostgreSQL server inside a WSL distro ("DwemerDistro"). SkyrimNet is one native DLL with no server. MinAI was a CHIM/Mantella add-on whose author went on to build SkyrimNet. Snippets say its repo pointed users to SkyrimNet, but that repo is no longer publicly reachable (Unverified). Users praise SkyrimNet for needing no WSL or server, and they complain most about CHIM's install. *Simple distribution wins users.* SkyrimNet now leads its README with its maker's **paid inference service, Fateless** ("plans start at $10 a month", no API keys). BYO cloud keys and local models come second and third. SkyrimNet is "not open source" (FACT, `SN/README.md`).
- **Setup friction is the biggest adoption killer.** Player2 reports that only about **1 in 20 downloads** of an AI-NPC Minecraft mod reached gameplay, because players had to get API keys and edit localhost settings (LIKELY, Player2 blog snippet). Mantella's FAQ is dominated by setup problems: firewall or antivirus blocking port 4999, Program Files permissions, OneDrive paths, MO2 shortcuts and provider quotas (FACT, `MAND/docs/pages/issues_qna.md`). CHIM's support load is similar (LIKELY; user snippets only).
- **Latency is the most common complaint.** Users report 10–15 s waits and silent failures with Mantella (LIKELY, Reddit snippets). An LLM-narrative game study found 40% of players frustrated by about 15 s lag. Mantella's own guide says to target **< 0.5 s to the first LLM response** and lists measured savings: about 0.5 s from playing the first line directly, about 0.5 s from lazy lip-sync, and near-zero STT time from "proactive" transcription (FACT, `MAND/docs/pages/real_time_npcs.md`). PUBG Ally (KRAFTON + NVIDIA ACE) runs an on-device SLM because cloud round-trips were "too slow" for squad talk (LIKELY). The model size is uncertain: a later NVIDIA snippet says a 2B Minitron, while CES-2025 materials, as the checker recalls them, named an 8B one.
- **Memory: simple rolling summaries go a long way.** Mantella keeps one plain-text summary file per NPC. It writes a summary when a conversation ends, and only if there were at least 5 messages. When the summaries pass 30% of the model's context it re-summarizes them into a new file (FACT, `MAN/src/remember/summaries.py`). CHIM and SkyrimNet layer an event log, embeddings and diaries on top, at a large cost in complexity. For Elden Ring's ~60–100 talkable named NPCs, Mantella-style summaries plus an event log are enough for v1.
- **Barks must not depend on a live LLM round-trip.** CHIM rate-limits combat barks with one **cooldown timestamp shared across all NPCs** (`COMBAT_BARK_LAST_TIMESTAMP`). It then makes a live LLM call with a random "cue" picked from 15 variants (FACT, `CHIM/main.php`, `CHIM/prompts/prompts.php`). *Corrected:* the period is **30 s** in the seeded profile default and the "Default" preset, 100 s in "Local LLM" and 20 s in "Follower". 90 s is only `main.php`'s fallback when the setting is unset (FACT, `lib/core/core_profiles.class.php`, `lib/core/settings_presets.php`). SkyrimNet caps combat lines at one sentence of ≤ 14 words and gives them a dedicated fast model slot (FACT, internal report). For Elden Ring, cache **per-archetype bark pools** and refill them in the background. Money is not the binding limit (≈ $0.00006 per LLM bark on a budget model); latency and free-tier rate limits are.
- **Small models need narrow jobs.** Mantella's FAQ blames NPCs "describing actions out loud", failed actions and weak summaries on small models (FACT). CHIM ships a T5 helper "to assist smaller weight LLMs" (FACT, `CHIM/conf/conf.sample.php: $MINIME_T5`). RimTalk users recommend Gemma or Mistral Nemo for "strict instruction following" (LIKELY). Mantella's own Skyrim fine-tune is now "very outdated and performs worse than newer, non fine-tuned models" (FACT). **Do not fine-tune for v1. Keep outputs plain text with tiny budgets.**
- **Lore and abuse.** Where Winds Meet players get quest rewards from LLM NPCs by claiming puzzles are solved or that missing relatives "suddenly appeared" (LIKELY, press). LLMs confabulate FromSoftware lore: ChatGPT summarized Dark Souls as a quest to "defeat a demon king" (LIKELY, Automaton 2023). Ubisoft reports that richer persona metadata reduced hallucinations in *Teammates* (LIKELY). For EldenNet: never let the LLM gate anything (v1 has no actions, which helps), and store player *claims* as claims, not as facts.
- **Current defaults (Nov 2026), from the projects themselves.**
  - Mantella: `google/gemma-4-26b-a4b-it:free` on OpenRouter, with Piper TTS and Moonshine STT. A `:free` model is subject to OpenRouter's free-tier daily caps.
  - CHIM: OpenRouter JSON connectors and **Pocket TTS**, served locally by audio.cpp at `127.0.0.1:8086`. Its default STT, "whisper", is **OpenAI's cloud Whisper API** (`api.openai.com/v1/audio/transcriptions`), not a local model.
  - SkyrimNet: the budget preset rotates Gemma 4 31B and 26B-A4B for dialogue and combat (FACT, configs). Local embeddings started from all-MiniLM-L6-v2 and have been selectable since Beta27 (FACT per release notes, internal report `../skyrimnet/04`).
  - Local hardware: Elden Ring uses about 5 GB VRAM at 1440p with max RT (LIKELY, one benchmark). That leaves room for a 4B-class Q4 model on a 12 GB card but almost nothing on 8 GB.
- **Ecosystem climate.** me3 bans LLM use in its project spaces, and fromsoftware-rs uses NON-AI licence files (report 02). Nexus Mods made AI disclosure mandatory for every mod on about 2026-11-03 (LIKELY, press). Expect to disclose and to get a cool reception. Do not clone the voice actors.

---

## 1. Landscape at a glance

| Project | Game | Process model | Default LLM path | Memory | Voice (TTS / STT) | Who pays | Status (Nov 2026) |
|---|---|---|---|---|---|---|---|
| **SkyrimNet** | Skyrim SE/AE/VR | **One in-process SKSE DLL** + native helper DLLs; dashboard on :8080 | OpenRouter presets; per-job "variants" | Event log → LLM scene memories (JSON) → ONNX embeddings + HNSW, recency/importance | Piper default; many engines / whisper.cpp, Parakeet | User: **Fateless subscription (from $10/month, promoted first)**, BYO key, or local | Beta30-rc2; very active; closed source ("not open source", README) (FACT, `SN/`, report `../skyrimnet/01-architecture.md`) |
| **Mantella** v0.14 | Skyrim, Fallout 4 (+VR) | **Sidecar Python exe**: FastAPI on :4999 ↔ Papyrus spell + SKSE_HTTP plugin; an SKSE launcher auto-starts the exe | `google/gemma-4-26b-a4b-it:free` via OpenRouter; OpenAI, NanoGPT, KoboldCpp, textgenwebui or any OpenAI-compatible URL | Per-NPC rolling summary text files, re-summarized at 30% of context | Piper default; xVASynth, XTTS, OpenAI-compatible / Moonshine (default), Whisper | User | Active; AGPL-3.0 (FACT, `MAN/`) |
| **Herika → AI-FF → CHIM** | Skyrim | **Sidecar server**: PHP + Apache + PostgreSQL in a WSL distro ("DwemerDistro"); `AIAgent.dll` SKSE plugin + Papyrus + PrismaUI client | `openrouterjson`, `openaijson`, `koboldcppjson`; llama.cpp on :8007 | Postgres event log, txtai embeddings (:8082), time-bucket summaries, diaries, dynamic profiles | Pocket TTS default (via audio.cpp :8086), about 26 engines / OpenAI cloud Whisper API default, local Whisper, Parakeet, Deepgram… (also a Player2 connector) | User | Active; server GPL-3.0-only (FACT, `CHIM/`) |
| MinAI | Skyrim | Add-on to CHIM/Mantella | — | — | — | — | Deprecated; repo said to point to SkyrimNet (Unverified: `MinLL/MinAI` is no longer publicly reachable, and `git ls-remote` asks for credentials) |
| Inworld Calradia / Bloc's ChatGPT mod | Bannerlord | In-game C# mod → Inworld SaaS / OpenAI or a local LLM | Inworld character engine | Vendor memory "not ready on Inworld's side" | Text box | Inworld account / OpenAI | Old (2023); reportedly still runs on 1.2.x (LIKELY) |
| Sentient Sands | Kenshi | DLL via RE_Kenshi + KenshiLib, **plus a local AI server** started by the mod | Player2 or any OpenAI-compatible provider | — | — | User / Player2 free tier | Active 2026 (LIKELY, itch/Nexus snippets) |
| RimTalk | RimWorld | C# mod, direct HTTP | Google Gemini free key; OpenAI-compatible and local | — | Text bubbles | User (free tier) | 2025– (LIKELY) |
| Player2 ecosystem (SecondBrain, Player2NPC, ChatClef) | Minecraft and others | Mod → local Player2 app on `localhost:4315` (OpenAI-compatible) | Player2's hosted models | — | Player2 STT/TTS | Free daily allowance; developers get a revenue share | Active (LIKELY) |
| NightCityNet | Cyberpunk 2077 | "Sister project … built on the Fateless engine" (SkyrimNet's core) | — | — | — | — | Mentioned in SkyrimNet README (FACT); otherwise not found on the web |
| game-assistant + Attack on Elden Ring | **Elden Ring** (+ Minecraft) | **Hybrid**: me3-loaded bridge DLL → shared memory → Python assistant | Claude Haiku (BYO Anthropic key, $3/h cap) | — | Local kokoro-onnx / faster-whisper | User | Oct 2026, WIP; adapter tests pass on 1.17.1 (FACT, READMEs) |
| Interactive-LLM-Powered-NPCs | "Any game" | **Fully external**: mic → LLM → TTS → SadTalker face video pasted over the game's face pixels | Cohere (GPT-4 optional) | Vector stores + "pre-conversation" iconic lines | TTS + lip-sync video | User | 2023 demo (FACT, README) |
| PUBG Ally (KRAFTON + NVIDIA ACE) | PUBG | **On-device**: ASR → Mistral-NeMo-Minitron SLM (2B per snippet; size unverified) → custom TTS; behaviour tree executes actions | Local | — | Local | Developer | Public beta Jun 2025 (LIKELY) |
| Ubisoft *Teammates* | Internal FPS demo | Cloud: Gemini + ElevenLabs; LLM only "fills gaps" left by behaviour trees | Cloud | — | ElevenLabs | Developer | Closed playtest, Nov 2025 (LIKELY) |
| *Where Winds Meet* | MMO (NetEase) | Cloud chatbot NPCs gate side quests | Cloud | — | Text | Developer | Live; widely exploited (LIKELY) |
| *Suck Up!* | Standalone game | Cloud; every NPC interaction spends an "AI token" | Cloud | — | Cloud TTS | **Player buys tokens** | Steam "Mixed" (LIKELY) |

---

## 2. The Skyrim / Fallout lineage

### 2.1 SkyrimNet (brief; full detail in `../skyrimnet/`)

FACT (from our SkyrimNet reports 01–06): SkyrimNet is "a native Windows DLL that loads inside Skyrim itself… no separate program to run, no virtual machine and no server window". It ships whisper.cpp/ggml, ONNX Runtime and FFmpeg as companion DLLs. Its core is **event-sourced**: per-playthrough SQLite, audience-tagged events and short-lived TTL events. Speech goes through a **five-class priority speech scheduler**. Each job type has its own **model slot** (`default`, `combat`, `memory`, `decisions`…), with failover and first-token hedging. Prompts are hot-reloadable Inja templates, and config is YAML.

The parts most relevant here:

- **Model presets are data.** `SN/model-presets/recommended-models.yaml` (last changed 2026-09-26) lists per-slot recommendations. Dialogue: Claude Sonnet 4.6, DeepSeek V3.2, Qwen3-235B-A22B-2507, GLM-4.7, Gemma 4 31B and 26B-A4B. Meta and translation: DeepSeek V4/V4.1 Flash, Gemini 2.5 Flash. Combat: Gemma 4 26B-A4B first. Decisions: `typesafe/jev-1.13`. The budget preset rotates `gemma-4-31b-it` and `gemma-4-26b-a4b-it` at temperature 1.0 for `default`, `combat` and `action_evaluation`. Rotation was added 2026-09-26 (`51a9cf3`). The preset also sets an fp8 quantization floor on OpenRouter routing, added 2026-10-04 (`57f333d`) (`presets/openrouter-budget.yaml`). The budget **and** Balanced presets use `qwen/qwen3-235b-a22b-2507` for `memory`, character profiles and diaries, and DeepSeek V4 Flash for `meta` and translation. This is the best available signal of which models "work for NPCs" in late 2026. It is still only one project's choice and was not measured by us.
- **Fateless (FACT, README, changed 2026-10-30).** The README offers three paths. The first is "Fateless: the easiest way": one account for dialogue, voices, STT and songs, "no API keys", from $10/month, Beta29+. The second is BYO OpenRouter or OpenAI-compatible keys with Budget/Balanced/Performance presets. The third is local (LM Studio, Ollama, KoboldCpp, llama.cpp), and the README names "NVIDIA Nemotron 3.5 Bolt" as a starter model (that model name is unverified by us). The most successful in-process mod has thus moved toward a **first-party hosted subscription**. This matters for §5.8.
- **The in-process cost.** Public crash reports show that worker threads reading live game data still cause CTDs (GH #819, #830, #831; report 01). Being in-process is a *discipline* problem: game reads must be marshalled onto the game thread.
- **Users like the packaging.** In a Reddit comparison, users call CHIM's separate server "relatively slower" than SkyrimNet, and describe the install as "a pain as you have to install a local server". SkyrimNet's Patreon says its "only mandatory external dependency is OpenRouter (or a local solution)" (LIKELY, snippets).

### 2.2 Mantella (Skyrim, Fallout 4)

**Architecture (FACT).** `MAN/main.py` (version `0.14`) starts a FastAPI/uvicorn server (`MAN/src/http/http_server.py`, port default 4999 in `other_definitions.py`). The game side is a Papyrus "Mantella Spell" that talks HTTP through Leidtier's SKSE_HTTP plugin. A separate "Mantella SKSE Launcher" starts `Mantella.exe` when Skyrim launches, and mikastamm's plugin hooks vanilla dialogue (Mantella-Spell README). Before HTTP, Mantella talked to the game through files; `file_communication_compatibility.py` still exists. This explains the old FAQ entry where "NPCs keep repeating the same line" when "the next voiceline is being activated before the voiceline file is ready" (`MAND/.../issues_qna.md`).

**Content (FACT).** Characters come from a CSV: `data/Skyrim/skyrim_characters.csv` has 3,203 rows with a median bio of about 1,190 characters, and the Fallout 4 CSV has 631 rows. Columns include `bio`, `voice_model`, `race`, `gender`, `ref_id`, `base_id` and **per-character overrides for `llm_service`, `model` and `tts_service`**. Bios are adapted from the Fandom Elder Scrolls wiki under CC BY-SA (README "Attributions"). There are also `bio_templates` with "awareness addendum" lines keyed to player achievements, for example a master alchemist.

**Prompt shape (FACT, paraphrased from `prompt_definitions.py`).** Short sections: Overview (who you are and who you are talking to, including their equipment), Background (bio), History (summaries), Current Scene (location, time, weather), Rules ("only spoken dialogue", "no quotation marks", "stay in character"). The prompt is small and readable. That makes it a reasonable shape for small models.

**Memory (FACT, `MAN/src/remember/summaries.py`).**

- When a conversation ends, each NPC gets a summary **of only the messages it heard**, worked out from a join/leave participation log. NPCs that heard identical threads share one LLM call.
- Minimum thread length: 5 messages normally, 2 on reload, 0 for "radiant" (NPC↔NPC) conversations. This keeps trivial "Hello. Goodbye." summaries out.
- Summaries are appended to `<conversation_folder>/<world_id>/<Name> - <ref_id>/<Name>_summary_<N>.txt`. A legacy `<Name>/` folder is still read if it exists, and `world_id` separates player characters. After each append, if the latest file (old text plus the new summary) exceeds `int(token_limit × summary_limit_pct)` tokens, the whole file is re-summarized into `_summary_<N+1>.txt`. `token_limit` is the **main** LLM client's limit, and `summary_limit_pct` defaults to 0.3 and is not overridden in `game_manager.py`. This is a rolling-compaction scheme. A failed re-summary retries every 5 s in an unbounded loop, which is a robustness smell to avoid.
- "Memory sharing": an NPC can pass a prefixed summary to another NPC.
- A separate summary model can be configured (`summary_llm_api`).

**Latency playbook (FACT, `MAND/docs/pages/real_time_npcs.md`, ranked by impact).**

1. Pick an LLM that returns its first response in **< 0.5 s**. Use fast providers (Cerebras, Groq) or set OpenRouter's provider sort to latency or throughput.
2. Lazy lip-sync files: about 0.5 s saved.
3. "Fast Response Mode": the exe plays the **first sentence itself** instead of waiting for the game's audio path, saving about 0.5 s (with an optional streamed variant).
4. Lower the mic pause threshold and turn on "Proactive Mode". STT then runs continuously while the player speaks, so its latency "effectively becomes zero".

There is also a Claude prompt-caching connector for OpenRouter (`MAN/src/llm/claude_cache_connector.py`).

**What users say (LIKELY, snippets).** The praise: depth, immersion, characters that stay in character, and a feeling of agency and replayability. A 2026 Springer chapter uses Mantella as its case study and reports "increased naturalness… emergence of unique NPC personalities", along with "response delays, coherence issues, and the need for clear guardrails". The complaints:

- 10–15 s waits.
- "Error out and just stop responding".
- A game crash loses the conversation in progress.
- Voices "as if recorded through a phone call".
- Small models narrate actions.
- Free-tier quotas run out mid-session (`issues_qna.md` "LLM API Error… free requests have reached their daily limit").

Mantella's own Skyrim Llama-3-8B fine-tune is flagged as "very outdated and performs worse than newer, non fine-tuned models" (FACT, `MAND/docs/pages/fine_tuned_models.md`).

### 2.3 Herika → AI Follower Framework → CHIM

- **Lineage (LIKELY, Reddit/Nexus snippets).** "Herika – The ChatGPT Companion" (2023) was a single AI follower backed by a server "middle man". AI-FF (September 2024) extended it to any NPC, with per-NPC LLM config files, NPC↔NPC talk and diaries. Its docs note that its JSON mode "is not supported by all models". CHIM is the current product. Development moved to `Dwemer-Dynamics/HerikaServer` and the `Dwemer-Dynamics/CHIM` client monorepo (FACT, READMEs).
- **Architecture (FACT, `CHIM/docs/agent-guide.md`, `docs/building.md`).**
  - The server is a PHP/PostgreSQL backend: "the application is PHP, not a native DLL", running in "the PHP/Apache/PostgreSQL environment matching the supported DwemerDistro installation" (a WSL instance: `lib/dwemerdistro_llm.php` says "the DwemerDistro engine in the same WSL instance". The Windows `DwemerDistro.exe` launcher (`Dwemer-Dynamics/CHIM-Launcher`) starts it and proxies port 7513. The client tries `127.0.0.1:7135` launcher discovery, then `127.0.0.1:8081/HerikaServer/comm.php`).
  - The client, `AIAgent.dll` (CommonLibSSE-NG) plus Papyrus and PrismaUI, owns "Skyrim/SKSE, Papyrus, microphone capture, game actions and playback".
  - Requests: `comm.php` → `main.php` → `processor/` → `prompts/` → `connector/` (LLM) → `functions/` (actions) → `stream.php`/`streamv2.php`.
  - Background workers do memory and profile updates.
- **Defaults (FACT, `CHIM/conf/conf.sample.php`).** `$CONNECTORS=["openrouterjson","openaijson","koboldcppjson"]`, `$TTSFUNCTION="pockettts"` (endpoint `127.0.0.1:8086`, audio.cpp), `$STTFUNCTION="whisper"` (= **OpenAI cloud** `api.openai.com/v1/audio/transcriptions` with `$STT["WHISPER"]["API_KEY"]`; local Whisper is the separate `localwhisper` option at `127.0.0.1:9876`), llama.cpp at `127.0.0.1:8007`, txtai embedding service at `127.0.0.1:8082`, and `$MINIME_T5=false` ("Assists smaller weight LLMs with action and memory functions").
- **Memory knobs (FACT, same file).**
  - `MEMORY_TIME_DELAY=12` minutes before a memory can be used.
  - `MEMORY_CONTEXT_SIZE=1`: only **one** recalled record goes into the prompt.
  - Automatic summaries are packed per time bucket (`AUTO_CREATE_SUMMARY_INTERVAL=10` ≈ 2.4 in-game hours, minimum 5 events).
  - An NPC-scoped memory is created after 3 global summaries involve that NPC.
  - `MEMORY_BIAS_A=33` is the minimum similarity threshold.
- **Combat barks (FACT).** In `CHIM/prompts/prompts.php`, the `combatbark` entry has 15 cue variants ("shouts a battle cry", "taunts their enemy", "calls out enemy positions"…). `CHIM/main.php` applies `COMBAT_BARK_COOLDOWN` with a single DB timestamp (`conf_opts.COMBAT_BARK_LAST_TIMESTAMP`) that is **global across all NPCs** ("global across all NPCs in the party", `ui/core/tmpl/metadata_json_editor.php`). *Corrected default:* profiles are seeded with **30 s** (`core_profiles.class.php`). The presets use 30 s (Default), 100 s (Local LLM) and 20 s (Follower), and the UI allows 10–600 s. `main.php` falls back to 90 s only when no value is set. The period is read from the speaking NPC's profile, but the timestamp is shared. Background-life enrolment excludes "combat barks" from relationship counting.
- **Hard-won robustness rules (FACT, agent guide).**
  - No line sent to the game may exceed **900 bytes**, because "a 1000-byte subtitle has crashed it". Oversized lines are split at sentence, then whitespace, then UTF-8 boundaries.
  - TTS output is cached by text hash.
  - Provider diagnostics record time-to-first-token (`ttft_ms`) and primary/fallback health with cooldowns.
- **What users say (LIKELY, snippets).** CHIM is "the most developed currently". But "the install is a pain as you have to install a local server", support for general issues was weak, and one update "murdered performance for many", with "the discord… flooded with unanswered problem reports".

### 2.4 Cross-project patterns

| Theme | Evidence | Lesson for EldenNet |
|---|---|---|
| Distribution friction | CHIM WSL complaints; Player2's 1-in-20 figure; Mantella FAQ about Program Files permissions | One mod-manager-installable DLL plus one config file. No WSL, no Python, no admin rights. |
| Masked failure | On an LLM exception, Mantella plays an error sound and speaks "I can't find the right words at the moment" in character, retrying up to a limit (`src/output_manager.py`). The player cannot tell a dead key from a slow provider | Run a startup self-test with a visible in-game toast and a clear log line. In-character fallbacks are fine, but always also surface the real cause. |
| Game crash loses state | Mantella users lose the conversation in progress on a CTD | Persist the event log incrementally (WAL SQLite) and summarize on the next load. |
| Length crashes | CHIM's 1,000-byte subtitle crash | Hard byte caps on every string that reaches native code (report 03 already recommends sanitizing). |
| Small-model failures | Mantella FAQ; CHIM T5 helper; AI-FF JSON-mode caveat | Plain-text output, no tool calls, one job per call. |
| Model churn | Mantella's fine-tune went stale; SkyrimNet's presets changed in Sep 2026 | Model lists live in data files and are user-editable. Never hard-code. |

---

## 3. Other games, commercial systems and research

- **Bannerlord.** Bloc's 2023 ChatGPT mod supported the OpenAI API and a local model, but it relied on an "overloaded service, causing long load times between exchanges" (Hackaday). Bloc's "Inworld Calradia" moved to Inworld's character engine. It needed an Inworld account, worked in English only, and its memory system was "not ready on Inworld's side", so conversations did not carry over to another day (LIKELY, Exputer / Nexus 5273). **Lesson:** a SaaS character engine is a dependency you cannot fix. Inworld itself later moved from "Studio" to the composable "Runtime" (Inworld docs, LIKELY). Keep memory and persona in our own store.
- **Kenshi – Sentient Sands.** A DLL loaded through RE_Kenshi + KenshiLib starts "its own local AI server". Backends are Player2 or any OpenAI-compatible provider. If the game is not started through `RE_Kenshi.exe`, "the AI server will not start". The authors disclose that it was "made with the assistance of LLM coding agents" (LIKELY, itch.io / Nexus snippets). This is the closest analogue to EldenNet: a native loader DLL in a game with no official modding API.
- **RimWorld – RimTalk (2025).** The default is a free Google Gemini key; "all AI processing is done in the cloud", so the PC barely feels it. Users report local LM Studio setups failing with "Response failed", and recommend Gemma or Mistral Nemo for "strict instruction following". The docs tell players to raise the Talk Interval when the free quota pauses dialogue (LIKELY, GameBusiness.jp, Steam comments).
- **Minecraft / Player2.** The Player2 desktop app exposes an OpenAI-compatible API on `localhost:4315` with no API key. It gives players a free daily allowance and gives developers a revenue share. Integrations must send a game-ID header and a health ping each minute. The authors moved to this model after finding that about **1 in 20** downloads of their AI-NPC mod reached gameplay. Their advice: AI NPCs should be "easy to set up and free for casual use while still resisting abuse" (LIKELY, Player2 / Elefant blogs).
- **Cyberpunk 2077.** CyberAI (2023) is only an OpenAI bridge for Lua and Redscript (Tom's Hardware). NightCityNet is named in SkyrimNet's README as a Fateless-engine sister project (FACT). We found no public build.
- **Game-agnostic overlays.** Interactive-LLM-Powered-NPCs identifies the speaker by face recognition on screen captures and pastes a SadTalker lip-sync video over the NPC's face. It adds a `pre-conversation.json` of each character's **iconic lines**, sampled into prompts to anchor style (FACT, README). Overlay companions like Hakko AI watch the screen and answer by voice "within a few seconds". One review says that delay "barely registers" in Elden Ring's slow fights. Hakko charges $9.99–19.99/month and its Play-store rating is 2.7 (LIKELY, review snippet).
- **NVIDIA ACE / PUBG Ally.** On-device ASR → Mistral-NeMo-Minitron (**2B** per snippet; CES-2025 materials named 8B, so unverified) → custom TTS. Its quantized model fits "GPUs with as little as 8 GB total VRAM" alongside PUBG. Actions go to a **behaviour tree**, which keeps "fast reactions off the language model". Cloud LLMs were rejected because network plus inference latency "often made responses feel too slow". inZOI's Smart Zoi runs ACE "entirely on the client side". NVIDIA lists Nemotron Nano models at 0.6B, 4B and 8B (LIKELY, NVIDIA / KRAFTON pages via snippets).
- **Ubisoft *Teammates* (Nov 2025).** Gemini + ElevenLabs. Latency ranged from quick to "a couple of seconds", and spiked when "Gemini 3 is out, the servers are getting hammered". Characters sometimes hallucinated, and Ubisoft found "deeper personality descriptions and more in-depth metadata tags" helped. The LLM only covers gaps in the behaviour trees (LIKELY, GamesBeat).
- ***Where Winds Meet* (2025).** LLM chatbot NPCs gate side-quest rewards. Players hand over obtuse replies until the NPC gives up the reward, claim puzzles are already solved, tell an NPC its "missing brothers suddenly appeared", steer NPCs with bracketed emoji, and push sexual content (LIKELY, PC Gamer, Automaton, OpenCritic). **Lesson:** an LLM that controls rewards or state will be socially engineered.
- ***Suck Up!*** Every NPC interaction spends an "AI token". 10,000 tokens ≈ 40–50 h. The developers said "local hardware is simply not powerful enough". Steam reviews are Mixed, and a roundup claims AI quality fell between Early Access and 1.0 (LIKELY). **Lesson:** a developer-paid or token-metered cloud model is a business. A free mod must make the user bring their own model.
- **Research.** An LLM-narrative game survey (arXiv 2404.17027) found 40% of players frustrated by about 15 s lag, while 54% named the fluid NPC interaction as the best part. An Åbo Akademi thesis (2026; Terraria, Stardew, Skyrim) reported "inaccurate responses, repetitive dialogue, and response delays". It also found that local LLMs raise CPU, GPU, RAM and VRAM use during NPC interactions. A VR interrogation study averaged a 7 s cycle that grew with context length (arXiv 2507.10469) (LIKELY, snippets).

---

## 4. Elden Ring and FromSoftware-specific prior art

Searches (Nov 2026): GitHub repository search for `elden ring llm/chatgpt/gpt/tts/mantella`, `dark souls llm`, `sekiro llm`, `melina ai`. Web searches for "Elden Ring AI NPC", "ChatGPT mod", "Melina AI companion", "Elden Ring Mantella", "Sekiro AI", "Dark Souls GPT", "AI voice mod", plus Reddit and YouTube phrasings.

| Item | What it is | Architecture | AI / voice | Evidence |
|---|---|---|---|---|
| **EldenEncouragement** (Nexus 7999; source `mmahoney09110/EldenRingEncouragement`, MIT, last commit 2026-09-19) | Companion "maiden" that comments on and encourages your play. **11 characters** in `settings.ini`: Melina, Ranni, Blaidd, Millicent, Messmer, Sellen, Malenia, and the Nightreign Recluse, Executor and Duchess, plus "Rot Goddess". Relationship ("bond") tiers move with play | **External C# WPF exe** (`EldenOverlay/`). Reads memory with `ReadProcessMemory` pointer chains (`MemoryScanner.cs`, `DynamicBases.cs`) and **event flags** (`EventFlagMan.cs`). Launches a Cheat-Engine-built helper `EldenHelper.exe` (plus `EldenHelper.CT`). Overlay subtitles only show when ER is fullscreen. Optional ME2/**me3** launch (`UseME3`, `Unalone_1.17.me3`, added 2026-09-19 with "1.17 compatibility" fixes) | Each event POSTs a form (prompt text + character id) to the **author's proxy `https://chat-proxy-r2t8.onrender.com/api/OpenRouter`** (`MaidenReader.cs:241`). The endpoint name suggests OpenRouter now; the README still says OpenAI. On failure it shows "I'm with you, Tarnished." The prompt carries player name, level, location, current target enemy, last enemy died to, and the event. **Default voice: 260 pre-generated static WAV clips** (`Audio/`). **Optional Azure TTS with the user's own key** (`azure_key`, `EldenTTS.cs`). README: "Response time ~ 3 seconds" | **FACT** (source code). Nexus page text not re-checked |
| **Un-Alone** (Nexus 8046) | Companion mod with a bond system. Labelled "AI-Generated Content" | External helper (`EldenHelper.exe`); a voice-only install exists; settings for comment frequency and self-interruption (`AIGeneralResponse`, `Interrupt`); Mod Engine 3 | Server-generated responses. The developer **rejects local models**, citing hardware demands and reduced variety. Subtitles show the full AI response. Short pre-recorded clips match the first sentence's emotion (report 03). *Resolved:* the Azure TTS "conflict" is not one. Static clips are the default, and Azure is an opt-in BYO-key alternative ("turn off [Voice] if using Azure TTS or it will supersede your Azure Voice", `settings.ini`) | LIKELY that Un-Alone ships from the same codebase (`Unalone_1.17.me3` is referenced in EEN's settings); feature list from snippets |
| **game-assistant** + **Attack on Elden Ring** (`mpere056`, MIT, 2026-10) | "An AI assistant you talk to while you play": a glowing "fairy" companion that answers questions from the game's own data. Later phases add route-walking and orders. **Not NPC dialogue** | **Hybrid. An in-process bridge DLL loaded through me3 v0.13.0** (App 1.17.1 / exe 2.7.1.0 only, "other versions are refused") publishes shared memory `Local\AoER_bridge_v1`. It holds a state block (camera, player, area id), an **entity table of characters within 80 m** (NpcParam id, model, position, hitbox, HP, hostile, dead) and a ray mailbox served "within about 1 ms per frame" through the game's own collision. Params are read via `SoloParamRepository` (`NPC_PARAM_ST`, `BONFIRE_WARP_PARAM_ST`), with layouts from fromsoftware-rs. "It never writes game memory." Python assistant runs out of process | Claude Haiku (hosted, BYO Anthropic key, "$3/hour spending cap"); **local** kokoro-onnx TTS and faster-whisper STT; push-to-talk F9 | **FACT** (`GA/README.md`, `GA/docs/games/eldenring.md`, AoER README). Status per its own docs: adapter tests 9/9 pass; voice and fairy "built … waiting on a game test" |
| Questbot (`sosatyler39-arch/questbot`), GraceGuide (`schizo16/graceguide`) | Overlay Q&A assistants | Electron / Python overlays over the game window | Questbot: Gemini paid tier + pgvector RAG + Discord OAuth/Stripe billing. GraceGuide: local Ollama + RAG | FACT (READMEs) |
| ⚠️ `eldenring-shadow-of-npcs-edition/.github` | Claims an "Advanced NPC AI … NPCs remember player choices" edition of ER | Tells users to download an archive and run "App Launcher.exe" from an external page | — | **Treat as a malware/piracy lure** (fact-check). Not opened beyond the README. Shows that "Elden Ring AI NPC" is already an impersonation target |
| **Chat with Melina** (Nexus 1587/5983; `LRA/`) | Summon Melina at a Site of Grace and pick chat topics | **No LLM.** Edits ESD talk scripts `t000001000`/`t000003000` and adds text ids 70000000+ that replay original lines | Uses vanilla audio. It passes the player's menu choice through **5 event flags used as scratch state**, which are reset when the chat ends. Closing the game mid-chat can leave them set, and "your game save's progress may be weird" | FACT (README). Lesson: never use real event flags as scratch storage |
| RAG lore bots (`Gr33nOps/EldenLore-RAG`, `CanerCakal/elden-oracle`, `J0hnBloodborne/EldenRAG` with local Qwen 2.5, `dsuther12/GideonGPT-RAG`) | Out-of-game lore Q&A | Web or desktop apps | Cloud (Groq) or on-device | FACT (GitHub search metadata, 2025–2026) |
| `joshua-hew/MaleniaGPT` (2024), character.ai "Melina" (Steam thread), `minkofg/pixel-melina` (2026, Electron + node-llama-cpp desktop pet) | Persona chatbots | Out-of-game | Cloud / local | FACT (GitHub search) / LIKELY |
| `alidhl/dialogue-generator` (2024) | LSTM trained on an Elden Ring dialogue CSV, with a Gradio UI | Out-of-game toy | — | FACT (`alidhl/dialogue-generator` README) |
| `magichear/GPT-SoVITS-EldenRing` (2024) | Apparent voice-clone experiment of the cast | — | GPT-SoVITS | FACT (repo exists; README is only a title) |
| "Elden Ring Dublado por IA" (itch.io, yelore) and ERPT-BR (`lorepamplona/ERPT-BR`) | PT-BR dubbing projects. ERPT-BR 0.9.7 rebuilds Wwise banks for 1.17.1 (BuildID 25080141) | Offline bank patching | The itch.io page says "por IA". We could not confirm ERPT-BR's voice source | LIKELY / FACT (README) |
| Hakko AI, Questie | Commercial screen-watching voice companions that list Elden Ring | Screen capture + overlay | Cloud | LIKELY |
| Dark Souls × ChatGPT (Automaton, 2023) | A streamer followed ChatGPT's advice. It got the developer right but described the plot as defeating a "demon king" and contradicted itself about starting gifts | — | — | LIKELY |
| Sekiro / Bloodborne / Armored Core | No in-game LLM projects found. Only out-of-game RAG bots (e.g. `renty551-tech/sekiro-rag-assistant`, `dsuther12/GideonGPT-RAG`) | — | — | FACT (GitHub repository search, re-run in the fact-check) |

**Interpretation.**

- **Corrected (was FACT "every ER AI companion runs out of process").** EldenEncouragement/Un-Alone are fully external `ReadProcessMemory` readers, though they *do* read event flags as well as stats. game-assistant is **hybrid**: a me3-loaded bridge DLL does the game-thread work (entity table, param reads, raycasts) and exports snapshots over shared memory to an external LLM process. That is close to the "adapter in-process, brain out-of-process" split this report recommends (§5.1), and it shows that per-frame snapshot export works on 1.17.1. Neither project calls `MsgRepository` or drives NPC talk/subtitles.
- **FACT (EEN) / LIKELY (Un-Alone):** the EldenEncouragement line uses an **author-hosted proxy** (`onrender.com`), which is a sustainability, privacy and availability risk (free-tier hosts sleep). game-assistant is BYO-key with a spending cap. Questbot is a paid subscription.
- **SPECULATION:** EldenNet's differentiators are LLM speech **by the game's own NPCs**, named-NPC identity from `NpcParam`/`NpcName`, flag-gated quest knowledge, and barks for generic enemies. None of the prior art has these. game-assistant already does in-process perception of nearby characters.
- Community and policy context (FACT, report 02): me3's `AI_CODE_POLICY.md` bans "all forms of LLM use" in its spaces, and fromsoftware-rs ships NON-AI licence files. Nexus Mods made AI disclosure mandatory, with categories for AI-generated, AI-assisted and AI promotional media, a 60-day grace period for existing mods and no ban (LIKELY; reported around 2026-11-03 by Inven Global, VGC, PC Gamer).

---

## 5. Design lessons for a minimal, stable v1

### 5.1 In-process vs out-of-process

| Factor | In-process DLL (SkyrimNet) | Sidecar server (Mantella, CHIM, Sentient Sands) | External memory reader (EldenEncouragement, Un-Alone) |
|---|---|---|---|
| Crash isolation | Poor. Any bug, including in native ML libraries, kills the game. Off-thread game reads cause CTDs (SkyrimNet #819/#830/#831) | Good for the "brain". The game-side shim can still crash | Best. The game never runs our code |
| Perception depth | Full: game thread, `ChrIns`, event flags, `MsgRepository` | Full, but only through a shim DLL plus IPC | Shallow: stats and event flags via `ReadProcessMemory` (EEN), fragile pointer chains, no game calls. A hybrid bridge DLL (game-assistant) gets full depth while keeping the brain external |
| Dev velocity | Every change means restarting ER (about a minute plus save load). Mitigate with hot-reloadable prompts and config and a headless test harness | High. Restart the brain without restarting the game; any language | High, but the feature ceiling is low |
| Distribution | Best: one DLL plus config through me3 `[[natives]]` | Worst: Python, WSL or a server to install, ports, firewall, auto-launcher | Medium: an extra exe to launch, overlays break in exclusive fullscreen |
| Users' verdict | "No WSL, no server" is the selling point | "Install is a pain"; performance regressions | Works, but only as a commentator |

**Elden Ring-specific facts (reports 01–03).** Elden Ring has no Papyrus-like script VM for DLL mods, so a native in-process adapter is needed either way. me3 loads natives after game init. Online play requires EAC, which forbids DLLs, so EldenNet is offline-only whatever the process model.

**Recommendation (SPECULATION, but the evidence points one way).**

1. Ship **one in-process DLL** that contains the game adapter and the core: event log, memory, prompt builder, scheduler, HTTP client.
2. Keep **all ML out of process in v1**. LLM, TTS and STT are reached over HTTP at OpenAI-compatible endpoints: cloud, or local apps the user already runs (LM Studio, Ollama, llama.cpp `llama-server`, KoboldCpp). For TTS, use a `/v1/audio/speech` server such as Kokoro-FastAPI (unverified here), **Ollama ≥ v0.40.8**, which now serves `/v1/audio/speech` and `/v1/audio/transcriptions` (FACT, `ollama/docs/api/openai-compatibility.mdx`), audio.cpp as CHIM uses it, or a third-party OpenAI wrapper around Pocket TTS. *Corrected:* Pocket TTS's own `serve` is **not** OpenAI-compatible. It exposes `POST /tts` (multipart form: `text`, optional `voice_url`/`voice_wav`, streamed audio) and `/health` on port 8000 (FACT, `PTTS/pocket_tts/main.py`). Either support it as a second TTS adapter or recommend the wrapper. This avoids the riskiest native code in-process (CUDA/ONNX/ggml DLL conflicts, GPU-driver crashes) and avoids linking GPL code: Piper moved to `OHF-Voice/piper1-gpl`, and espeak-ng is GPL.
3. Make the adapter↔core boundary **serializable messages from day one** (perception snapshot in, speech commands out). The core can then also run as a **headless binary** that replays recorded sessions, for development speed, and later as an optional sidecar if crash isolation becomes worth it.
4. All game-memory access stays on the game thread (`CSTaskImp` tasks). Workers only see copied snapshots. This is SkyrimNet's rule, enforced harder.

### 5.2 Local vs cloud defaults: one OpenAI-compatible client

Every surveyed project converged on "OpenAI-compatible endpoint + model id + key" as the single abstraction (FACT: Mantella `llm_api` accepts a free-edit URL; CHIM connectors; SkyrimNet providers; Sentient Sands; RimTalk).

| Backend | Default base URL | Notes that matter for NPCs | Source |
|---|---|---|---|
| OpenRouter | `https://openrouter.ai/api/v1` | `provider.sort` = `latency` / `throughput` / `price`; `:nitro` = throughput sort, `:floor` = price. `:free` models: 20 req/min and **50/day** (or 1,000/day after $10 of lifetime credits) | OpenRouter docs (LIKELY, snippets; the fact-check could not open openrouter.ai to re-verify); Mantella real-time guide (FACT) |
| LM Studio | `http://localhost:1234/v1` | GUI model manager; users already have it | RimTalk / Mantella usage (LIKELY) |
| Ollama | `http://localhost:11434/v1` | Supports `response_format`, `reasoning_effort`/`reasoning` and the `think` extension. **Keeps a model loaded for 5 minutes after use by default**, so the first line after a lull pays a cold load. *Caveat:* `keep_alive` is documented only on the native `/api/generate` and `/api/chat`, not on the `/v1` OpenAI endpoints. Warm or pin models with a native `/api/generate` call (`keep_alive: -1` or a duration) or with the `OLLAMA_KEEP_ALIVE` env var. Also serves `/v1/embeddings`, `/v1/audio/speech` (v0.40.8+) and `/v1/audio/transcriptions` | FACT, `ollama/docs/api/openai-compatibility.mdx`, `docs/faq.mdx` (fetched 2026-11-06) |
| llama.cpp `llama-server` | *Corrected:* `http://localhost:9931/v1` on current builds. The default changed from 8080 on **2026-10-09** (`b42b7e6d`, "server: use port 9931 by default (#30159)"). Older builds still use 8080, which is also SkyrimNet's dashboard port | `cache_prompt` on by default (prefix KV reuse), `json_schema`/GBNF grammars, `--reasoning off` / `--reasoning-budget 0`, `--slot-save-path` | FACT, `ggml-org/llama.cpp/tools/server/README.md`, `common/common.h` (master `18ab62a`, 2026-11-06) |
| KoboldCpp | `http://localhost:5001/v1` | Popular with Skyrim users (Mantella and CHIM defaults list it) | FACT (configs) |
| Player2 | `http://localhost:4315` (desktop app; docs at `/docs`) or the hosted `https://api.player2.game/v1` (OAuth/key) | Free tier, no key locally; needs the game-ID header and pings. CHIM already ships `connector/player2*.php`. An anonymous community investigation (`Pl2I/pl2i.github.io`) alleges a shift toward paywalled access, such as ~1 h testing for non-patrons (Unverified) | FACT for ports/URLs (`elefant-ai/Player2APIDemo-AIGods` README, `elefant-ai/player2-sdk-ts`); terms LIKELY/Unverified |

**Defaults for v1 (SPECULATION).** First-run choice between "Cloud (OpenRouter key)" and "Local". Local detection should probe LM Studio :1234, Ollama :11434, llama.cpp **:9931 and legacy :8080**, and KoboldCpp :5001, then confirm by calling `/v1/models`. Never assume a port's owner, because 8080 is shared with other tools. Ship model presets as a data file, as SkyrimNet does. **Always disable reasoning/thinking** for dialogue and barks: it adds seconds and leaks `<think>` text (llama.cpp and Ollama both expose switches, and SkyrimNet and CHIM both strip reasoning).

### 5.3 Small-model friendliness

- **One job per call** (FACT pattern). SkyrimNet splits dialogue, combat, memory and decisions into slots. CHIM adds a T5 helper. Mantella can use a separate summary model.
- **Plain text out.** Dialogue should be raw spoken text with at most a speaker prefix. Use JSON only for background jobs (memory extraction), and on local llama.cpp enforce it with `json_schema`. AI-FF's JSON mode excluded many models (LIKELY).
- **Short, sectioned prompts.** Mantella's prompt is about 15 lines plus bio, summary and scene (FACT). FromSoftware's terse, archaic register fits short outputs: 1–3 sentences, ≤ 60 words for named NPCs, one line of ≤ 12–14 words for barks (SkyrimNet's combat cap is 14; report `../skyrimnet/02`).
- **Sanitize instead of trusting instruction-following.** Strip `*actions*`, quotation marks, `<think>`, markup and over-length text (Mantella FAQ; CHIM 900-byte rule; report 03).
- **No fine-tuning in v1.** Mantella's fine-tune was overtaken by general models within about two years (FACT).

### 5.4 Memory designs that work

| Design | Who | Strengths | Weaknesses |
|---|---|---|---|
| Rolling per-NPC summary text, re-summarized at 30% of context | Mantella | Trivial to build, inspect and edit; robust; one LLM call per conversation | Details fade with each compaction; no relevance selection; small models write weak summaries (Mantella warns) |
| Diaries | AI-FF, SkyrimNet | Human-readable long-term arc | Costly; the LLM writes fiction about itself |
| Event log + time-bucket summaries + embeddings (txtai) + 1 recalled item | CHIM | Recall by relevance | Needs a Postgres/WSL stack; many knobs; only one memory injected by default |
| Event log + scene memories (JSON: emotion, importance, tags) + ONNX embeddings + HNSW + recall gate | SkyrimNet | State of the art in mods | Large build; migrations show repeated re-embedding churn (report `../skyrimnet/04`) |
| Vendor memory | Inworld Calradia | None for us | Broke on the vendor's side |

**For Elden Ring v1 (SPECULATION, grounded in the above).**

- Named NPCs are few (about 60–100 talkable), are met at fixed places, and are talked to for minutes per playthrough. Vector recall is **not needed in v1**.
- Use an **append-only event log** (SQLite) plus a **per-NPC rolling summary**, Mantella-style: summarize at conversation end if there were ≥ 4–5 exchanges, compact at about 25–30% of the context budget, and record only what the NPC perceived.
- Add a tiny **structured facts table** fed by the game, not the LLM: met the player, the player's name/class, quest-stage flags seen, last place met, attacked by the player, gifts. Facts beat summaries for spoiler gating and for small models.
- **Store player claims as claims.** The summarizer must write "the Tarnished claimed to have slain Godrick" unless the event flag confirms it. This is the Where Winds Meet lesson applied to memory.
- Leave a hook for embeddings later. Brute-force cosine over a few thousand rows is enough (report `../skyrimnet/01`). Key the vectors with an embedding-model fingerprint, as SkyrimNet does, so a model change triggers re-embedding.
- Generic enemies get **no persistent memory**. They respawn, so a bark like "you again!" breaks the fiction (report 04).

### 5.5 Latency playbook (ranked)

| Technique | Evidence | Expected effect |
|---|---|---|
| Fast provider/model; target < 0.5 s TTFT | Mantella guide (FACT); CHIM logs `ttft_ms` (FACT) | Largest single factor |
| Stream tokens → split sentences → show and voice the first sentence immediately | SkyrimNet, CHIM (`lib/sentence_boundaries.php`), Mantella (FACT) | The player sees text after first-sentence time, not full-reply time |
| Play the first audio directly instead of through the engine path | Mantella "Fast Response Mode" ≈ 0.5 s (FACT) | v1 plays TTS through its own mixer anyway (report 03, D1) |
| Prompt-prefix caching: stable persona and lore first, volatile scene last | llama.cpp `cache_prompt` (FACT); Mantella's Claude cache connector (FACT); SkyrimNet's cache-ordered prompts (report `../skyrimnet/02`) | Cuts prefill cost and time for repeat turns with the same NPC |
| Disable reasoning; small `max_tokens` | llama.cpp / Ollama switches (FACT) | Avoids multi-second thinking |
| Keep local models warm | Ollama 5-min default unload (FACT) | Send a warm-up request when entering an NPC's area. For Ollama, use the native `/api/generate` with `keep_alive`, because the `/v1` endpoints do not document it |
| Pre-warm while the player types | SkyrimNet pre-warms memory search on key press (report `../skyrimnet/02`) | Build the prompt during typing; send on Enter |
| Hedged / fallback requests | SkyrimNet first-token hedging; CHIM primary/fallback health (FACT) | Bounds tail latency on cloud spikes (Ubisoft's Gemini 3 day) |
| On-device SLM for real-time loops | PUBG Ally 2B (LIKELY) | Only if VRAM allows (§6) |
| Acknowledgement filler | Mantella plays an error "cough" when nothing is heard (FACT, `play_cough_sound`). SPECULATION: a short in-character "Hm…" subtitle after 1.2 s without a token | Hides 1–2 s in the vanilla-like rhythm |

### 5.6 Keeping barks cheap

- **Neither SkyrimNet nor CHIM runs a true bark cache for enemies.** SkyrimNet sends combat lines through the dialogue pipeline with combat modules. CHIM makes a live LLM call per bark behind one global cooldown, 30 s by default and 20–100 s across its presets (FACT; corrected from "90 s"). Both are built for followers, not hordes. Elden Ring's density of hostile humanoids (camps, Leyndell, the Shadow Keep) would multiply calls.
- **Cost is not the binding constraint.** A bark prompt of about 800 input tokens and 20 output tokens on Gemma 4 26B-A4B at about $0.07 / $0.34 per M tokens costs ≈ $0.00006. One bark every 10 s for an hour of combat ≈ $0.02 (prices LIKELY, OpenRouter page snippets, volatile). **The binding constraints are latency** (a line spoken 3 s after the swing is worse than none), **free-tier rate limits** (50 requests a day) and **local GPU contention** during combat.
- **Design (SPECULATION).**
  1. **Archetype × trigger pools.** Key by `(archetype from NpcParam/model id, trigger {aggro, lost_target, ally_died, player_low_hp, player_healed, player_fled, enemy_killed_by_player…}, language, model fingerprint)`, with about 12–24 lines per key. Generate in one batched call the first time an archetype is seen, or pre-bake on first launch for the common archetypes. Cache on disk.
  2. **Serve instantly from the pool**, using a recency ring buffer per key so lines do not repeat within N uses. Refill in the background when a pool is below 50% unused, and never during a boss fight.
  3. **Rate limits:** per actor (about 20 s), per squad (about 8 s), global (about 4–6 barks a minute), plus a probability per trigger. These match report `../skyrimnet/05`'s proposal. CHIM ships global gaps of 20 s (Follower), 30 s (Default) and 100 s (Local LLM). That is a data point on how quiet "acceptable" can be for *followers*. Hostile-camp barks from a local pool can be denser, because they cost no LLM call.
  4. **Optional live bark** only for rare, high-salience events, with a hard 1.5 s deadline. On a miss, use the pool line or stay silent.
  5. **Slot-fill only safe tokens** (time of day, weather, the player's weapon class) through templates, never free text.

### 5.7 Avoiding lore breaks, spoilers and abuse

- **The LLM's prior knowledge is the spoiler source.** Elden Ring is heavily covered by wikis, so models "know" the endings and identity reveals. They also confabulate (the Dark Souls "demon king"). Content after their training cutoff (Shadow of the Erdtree 2024, Tarnished Pack 2026) may be missing. Give facts explicitly and gate them by event flags (report 04 §7). Add a post-generation named-entity check against a spoiler lexicon keyed to flags (SPECULATION).
- **Ground style in the NPC's own words without redistributing them.** Interactive-LLM-NPCs anchors style with iconic lines (FACT). For Elden Ring, extract the NPC's TalkMsg lines from the **user's own install** at runtime (copyright: report 04). Use only lines whose flags show **the player has already heard them** as few-shot style samples (SPECULATION).
- **Bios: licence matters.** Mantella's bios are CC BY-SA from Fandom (FACT). The Elden Ring Fandom wiki is likewise CC BY-SA (LIKELY). Fextralife content is not openly licensed (LIKELY). Write our own short bios with attribution where adapted.
- **Make vagueness a virtue (SPECULATION).** FromSoftware NPCs are cryptic and evasive by design. Telling them to answer unknowns with in-world deflection ("I know naught of that") cuts hallucinations and fits the setting.
- **No authority for the LLM.** v1 is dialogue-only, so Where Winds Meet-style reward exploits do not apply. Memory pollution does (§5.4). Never let the LLM set flags, give items or change hostility. Put any v2 action behind deterministic validators, as PUBG Ally and *Teammates* use behaviour trees (LIKELY).
- **Content filtering.** Elden Ring ships an `ngword` list (report 03). Players will try sexual and abusive content (PC Gamer on Where Winds Meet). Offer a safety level setting and keep a refusal-in-character fallback.

### 5.8 Cost and sustainability models

| Model | Example | Verdict for EldenNet |
|---|---|---|
| Author pays for a hosted server | EldenEncouragement (FACT, `onrender.com` proxy), Un-Alone (LIKELY) | Unsustainable and a privacy issue. Avoid |
| First-party paid subscription | SkyrimNet's Fateless (from $10/month, FACT README); Questbot (Stripe, FACT README) | Shows users will pay for zero-setup. It is a business, not a mod feature, and out of scope for v1. Keep the provider layer open so a third-party service can plug in |
| Player buys tokens | *Suck Up!* (LIKELY) | Not appropriate for a free mod |
| Vendor free tier with a revenue share | Player2 (LIKELY) | Optional provider; good for "zero-setup" trials |
| BYO cloud key | Mantella, CHIM, SkyrimNet (FACT) | Default |
| BYO local server | Same (FACT) | First-class option. The game's 60 FPS cap (press, launch spec sheet) leaves GPU headroom on strong cards (SPECULATION; measure frame-time spikes) |

Rough named-NPC turn cost (SPECULATION; prices LIKELY and volatile): ~2,500 input + ~80 output tokens ≈ **$0.0002** on Gemma 4 26B-A4B ($0.07/$0.34 per M), or ≈ $0.0004 on DeepSeek V4 Flash (~$0.14/$0.28 per M). That is about 2,500–5,000 turns per US$1 before caching. Premium models such as SkyrimNet's Sonnet 4.6 preset cost one to two orders of magnitude more per token (LIKELY; check current pricing).

---

## 6. Current (Nov 2026) defaults for real-time NPCs — use with caution

Model availability and pricing change monthly. Everything here must live in an editable data file.

**Cloud LLM (FACT that these are the projects' current picks; quality claims LIKELY).**

- Dialogue, budget: `google/gemma-4-26b-a4b-it` (MoE, about 3.8B active, 256K context, Apache-2.0) and `google/gemma-4-31b-it`. These are the defaults or first recommendations of both Mantella and SkyrimNet. Gemma 4 launched around 2026-03-31 / 04-02, with a 12B "Unified" model added 2026-06-03 (Google release log via snippets).
- Background jobs. *Corrected:* SkyrimNet's built-in Budget and Balanced presets use `qwen/qwen3-235b-a22b-2507` for `memory`, diaries and profiles, and `deepseek/deepseek-v4-flash` for `meta` and translation. Its *recommended* list adds `deepseek-v3.2` for memory and `v4.1-flash`/`gemini-2.5-flash` for meta (FACT, `SN/model-presets/`).
- Premium dialogue: `anthropic/claude-sonnet-4.6`, `qwen/qwen3-235b-a22b-2507`, `z-ai/glm-4.7` (SkyrimNet presets).
- Barks: the same budget model, through a separate slot with `max_tokens` ≈ 40 and a short timeout.

**Local LLM (LIKELY; verify on target hardware).**

- VRAM budget: Elden Ring itself uses about 5 GB at 1440p max RT and < 8 GB at 4K (one source, PC Optimized Settings).
  - **8 GB GPU:** a local LLM on the same GPU is impractical beyond about 1–2B models. Recommend cloud, CPU inference, or a second machine.
  - **12 GB:** a 4B-class Q4 model (~2.5–3 GB plus KV cache), such as **Gemma 4 E4B** or **Qwen3.5-4B** (March 2026; thinking off by default on the small models).
  - **16 GB+:** Qwen3.5-9B Q4 (~5.5–6 GB).
- Gemma 4 26B-A4B needs about 15+ GB at Q4 because all experts must be resident, though llama.cpp can keep experts in system RAM (SPECULATION on footprint).
- Community 2026 picks for NPC lines: Llama 3.2 3B "fastest coherent… barks" and Qwen3 4B for persona consistency, measured on an RTX 4060 (LIKELY, vendor-blog benchmarks).

**Embeddings (later than v1).**

- `all-MiniLM-L6-v2`: SkyrimNet's starting model. Tiny and fast, but English-only and limited to 256 wordpieces. SkyrimNet's own Beta27 recall test (right memory in top 5) gave MiniLM / Granite / Bekko: English 65 / 71 / 76%, **Russian 46 / 88 / 93%**. Do not default to MiniLM for a multi-language game (FACT per `../skyrimnet/04`, GH releases).
- **EmbeddingGemma-300M** (arXiv 2509.20354): multilingual (100+ languages), 2K context, and per its paper beats Qwen3-Embedding-0.6B on several MTEB task types. Elden Ring ships in many languages.
- `Qwen3-Embedding-0.6B`: when long context is needed.

All three run through ONNX or as an OpenAI-compatible `/v1/embeddings` endpoint (LIKELY).

**TTS (optional in v1; report 03 D1).**

- **Pocket TTS** (Kyutai; tech report dated 2026-01-13; MIT code). 100M parameters, CPU-only on 2 cores, about **200 ms to first audio chunk** (vendor figure), about 6× real time on an M4 Air. Streaming, voice cloning, 7 languages. October 2026 added 18.2M / 6.9M / 1.9M-parameter English models, "up to 25x faster than real time on one CPU thread" (blog 2026-10-20) (FACT, `PTTS/README.md`). It is now CHIM's default, via audio.cpp. *Caveats added in the fact-check:*
  - The `serve` mode is `POST /tts` on :8000, not OpenAI `/v1/audio/speech`.
  - The **voice-cloning weights sit in the gated HF repo `kyutai/pocket-tts`**: the user must accept terms and run `hf auth login`, and an unauthenticated fetch returns "Access … is restricted". Non-cloning weights load from `kyutai/pocket-tts-without-voice-cloning` (FACT, `pocket_tts/config/*.yaml`). Weight licences differ per voice (HF `kyutai/tts-voices`).
  - **Standalone `pocket-tts` binaries for Windows x86 need neither Python nor PyTorch**. That makes an optional bundled local TTS feasible without breaking the "no Python" rule.
  - The README lists **Sonorus**, a Hogwarts Legacy mod to "talk to any named character … with their original voice". It is more prior art for LLM NPCs outside Bethesda games, and for cast-voice cloning (LIKELY; Nexus page not opened).
- **Piper**: SkyrimNet's and Mantella's CPU default. Development moved to the GPL `piper1-gpl` (FACT).
- **Kokoro-82M**: Apache-2.0. Measured about 2–5× real time on CPU (LIKELY, Reddit benchmarks).
- GPU / cloud: Chatterbox (Turbo), XTTS, ElevenLabs, Inworld TTS (Realtime TTS-2, 2026) (LIKELY).

**STT (not needed in v1; the Tarnished is silent and typed input is the default).**

- Moonshine: Mantella's default (`moonshine/tiny/quantized`). Its 2026 "Moonshine Voice" toolkit is on-device and built for live streaming (FACT, README).
- whisper.cpp / Parakeet: SkyrimNet.
- Parakeet TDT 0.6B v3 is batch-oriented and degrades when chunked (arXiv 2604.14493, LIKELY).

---

## Implications for the AI framework

**v1 (minimal, stable)**

1. **Packaging:** one `eldennet.dll` loaded through me3 `[[natives]]` (other loaders also supported, report 02), plus `eldennet/config.toml`, `eldennet/models.yaml` (presets) and `eldennet/data/` (bios, bark pools, DB). No Python, no WSL, no services we ship. **Offline only** (EAC).
2. **Threading:** the game-thread task copies a perception snapshot (nearby `ChrIns`, flags, place, time) into a channel. The core runs on worker threads. Nothing touches game memory off-thread. Every string that reaches native UI is byte-capped and sanitized.
3. **Model slots:** `dialogue`, `bark`, `summary`, each with provider, model, temperature, `max_tokens`, timeout and fallback. One OpenAI-compatible client with streaming, `stream_options`, reasoning disabled and per-slot hedging.
4. **Named NPC pipeline:** persona card (own text plus a few heard-lines style samples extracted locally) + flag-gated knowledge + facts table + rolling summary + the last N turns → stream → sentence split → subtitle (and optional TTS). Walking away beyond 10–12 m ends the conversation (report 03).
5. **Memory:** WAL SQLite: `events`, `npc_facts`, `npc_summaries` (versioned, compacted at about 25–30% of the slot's context budget), keyed by SteamID + slot + character fingerprint (report 01). Summaries are written at conversation end and on next load after a crash. Player claims stay claims.
6. **Barks:** archetype × trigger pools, generated in batches and cached, a no-repeat ring, per-actor / squad / global cooldowns, an optional live bark with a 1.5 s deadline. No memory for generics.
7. **Diagnostics from day one:** a self-test at load (endpoint reachable, model exists, TTFT measured), visible failure toasts, a log of TTFT and first-sentence time per call (CHIM's `ttft_ms` pattern), and an option to disable each feature.
8. **Never use real event flags as scratch state** (Chat with Melina's corruption warning). Keep all EldenNet state in our own DB, and read flags only.
9. **Study game-assistant / AoER before building the adapter** (MIT). Its shared-memory snapshot (entity table of characters within 80 m with NpcParam id, HP and hostility; param reads via `SoloParamRepository`; game-thread raycasts in about 1 ms/frame) is a working 1.17.1 reference for the perception half of v1. Re-validate everything on 1.17.2, where `WorldChrMan` fields shifted (report 01).
10. **Content and licensing:** our own bios (CC BY-SA attribution where adapted from Fandom), no redistributed FromSoftware text or audio, no cloned cast voices, an AI-disclosure section ready for Nexus.

**Later (v1.x–v2)**

- Embedding recall (EmbeddingGemma or MiniLM via ONNX or an endpoint) once summaries prove insufficient. Scene-based structured memories, SkyrimNet-style.
- Optional sidecar mode for the core (crash isolation, hot reload), reusing the message boundary.
- Optional local TTS bundle: the Pocket TTS standalone Windows binary (no Python) with non-cloning weights, or Ollama's `/v1/audio/speech`. Voice effects for helmets and masks. STT push-to-talk with name hints.
- Actions only through deterministic validators and a whitelist, never LLM-authoritative.
- Player2 as a zero-setup provider if its terms fit.

---

## Risks

- **Setup friction and support load.** Even a single DLL needs a model endpoint. Player2's 1-in-20 figure is a warning. A first-run wizard and a self-test are not optional.
- **Latency on cloud spikes.** Shared provider load (Ubisoft's Gemini 3 day) and free-tier caps (50 requests a day) will cause stalls. Hedging and bark pools reduce the impact but do not remove it.
- **Local GPU contention.** LLM bursts during combat may cause frame-time spikes even under the 60 FPS cap. This is unmeasured.
- **Spoilers and lore drift** from model priors. Gating reduces this but post-cutoff DLC knowledge gaps remain.
- **Memory pollution** from player social engineering (Where Winds Meet).
- **In-process crash risk.** It remains for any adapter bug, and patches move offsets and structs (report 01).
- **Community and policy.** Anti-AI stances in core tooling (me3, fromsoftware-rs NON-AI) and the new Nexus disclosure rules. Copyleft: Mantella AGPL and CHIM GPL mean their code cannot be reused without adopting those licences.
- **Voice and IP.** The cloning experiments (GPT-SoVITS-EldenRing) show the temptation. Cloning FromSoftware's cast is a legal and community risk.
- **Source quality.** Most user-sentiment, pricing and benchmark claims here come from search snippets of Reddit, Nexus and blogs that could not be opened in this session.
- **Impersonation and malware.** "Elden Ring AI NPC" is already used as a lure (`eldenring-shadow-of-npcs-edition`, which asks users to run "App Launcher.exe"). Publish only from one canonical repo or Nexus page, sign releases, and warn users about look-alikes.
- **Local-endpoint drift.** Local servers change defaults: llama.cpp moved 8080 → 9931 on 2026-10-09, and Ollama keeps adding endpoints. Hard-coded ports or feature assumptions will break silently. Probe and verify instead.
- **Third-party provider terms.** Player2's business practices are disputed (anonymous, unverified investigation). Free-tier caps on OpenRouter `:free` change without notice.

---

## Open questions

1. *Partly answered by the fact-check.* EldenEncouragement's source (`MaidenReader.cs`) sends a form with the prompt text and a character id. The prompt contains player name, level, location, current target enemy, last enemy died to and event descriptions. No screenshot upload was seen in the code we read. Azure TTS is optional with the user's own key. Still open: whether the Nexus Un-Alone build matches this repo, and what the proxy (`OpenAI-Proxy-Server`) logs or retains.
2. Is there an unpublished or Discord-only Elden Ring LLM NPC project, for example among me3 / ?ServerName? modders or Chinese communities (Bilibili)? Nexus, Bilibili and YouTube searches were not possible here.
3. How much frame-time variance does a local 4B Q4 model on the same GPU add while ER runs at its 60 FPS cap, on 8 / 12 / 16 GB cards?
4. Can Elden Ring's vanilla enemy grunts and battle cries (Wwise events) be triggered alongside text barks for "voiced" generics without TTS? Report 03 has the Wwise path.
5. Which current budget model best holds FromSoftware's archaic register in ≤ 60 words? This needs a small eval set (SPECULATION: Gemma 4 vs DeepSeek V4 Flash vs Qwen3.5-9B local).
6. Is the Elden Ring Fandom wiki's CC BY-SA licence confirmed, with per-page exceptions? Is fextralife definitely unusable?
7. Are Player2's terms compatible with an offline single-player mod that sends a "game ID" header? What are its real quotas?
8. Does OpenRouter's 50 requests/day free cap (if confirmed) make free models useless for anything but named-NPC chat? Should the wizard discourage `:free` for barks?
9. Is NightCityNet (Fateless engine for Cyberpunk) public, and does its existence imply a reusable game-neutral core licence we could learn from? (The README says it is "independently operated, not by us", and SkyrimNet itself is closed source.)
10. Does game-assistant's AoER bridge (1.17.1-only) already work on 1.17.2, and would its author accept collaboration on a shared, MIT-licensed perception bridge?
11. Which Pocket TTS deployment is acceptable to ship: the standalone Windows binary with non-cloning weights, or the gated cloning weights that need an HF login?

---

## Sources

**Repositories (read locally or via raw.githubusercontent.com)**
- Mantella: https://github.com/art-from-the-machine/Mantella (`main.py`, `src/remember/summaries.py`, `src/http/http_server.py`, `src/llm/claude_cache_connector.py`, `src/config/definitions/{llm,tts,stt,other,prompt}_definitions.py`, `data/Skyrim/skyrim_characters.csv`, `LICENSE` AGPL-3.0); docs branch `gh-pages`: `docs/pages/{real_time_npcs,issues_qna,fine_tuned_models}.md`; site https://art-from-the-machine.github.io/Mantella/
- Mantella Spell: https://github.com/art-from-the-machine/Mantella-Spell (README); SKSE_HTTP https://github.com/Leidtier/SKSE_HTTP
- CHIM server: https://github.com/Dwemer-Dynamics/HerikaServer (`README.md`, `docs/agent-guide.md`, `docs/building.md`, `conf/conf.sample.php`, `main.php`, `prompts/prompts.php`, `lib/memory_helper_vectordb.php`, `lib/dwemerdistro_llm.php`, `LICENSING.md`); legacy https://github.com/abeiro/HerikaServer; client https://github.com/Dwemer-Dynamics/CHIM (README)
- SkyrimNet: https://github.com/MinLL/SkyrimNet-GamePlugin (`model-presets/recommended-models.yaml`, `model-presets/presets/openrouter-budget.yaml`); internal reports `../skyrimnet/01-architecture.md` … `06-speech-ui-dialogue.md`
- Chat with Melina: https://github.com/lraty-li/Elden-Ring-Mods (`Chat-with-Melina/README.md`)
- EldenEncouragement source (fact-check): https://github.com/mmahoney09110/EldenRingEncouragement (`README.md`, `EldenOverlay/settings.ini`, `MaidenReader.cs`, `EldenTTS.cs`, `EventFlagMan.cs`, `CheatEngineHelper.cs`, `Audio/`) · proxy: https://github.com/mmahoney09110/OpenAI-Proxy-Server
- game-assistant / Attack on Elden Ring (fact-check): https://github.com/mpere056/game-assistant (`README.md`, `docs/games/eldenring.md`) · https://github.com/mpere056/attack-on-elden-ring (`README.md`)
- Other ER overlays found in the fact-check: https://github.com/sosatyler39-arch/questbot · https://github.com/schizo16/graceguide · lure repo (do not download): https://github.com/eldenring-shadow-of-npcs-edition/.github
- SkyrimNet README (Fateless, closed source, NightCityNet): https://github.com/MinLL/SkyrimNet-GamePlugin/blob/main/README.md · presets: `model-presets/index.yaml`, `presets/openrouter-default.yaml`
- CHIM bark cooldown defaults: HerikaServer `main.php` (L1069–1100), `lib/core/core_profiles.class.php`, `lib/core/settings_presets.php`, `ui/core/tmpl/metadata_json_editor.php`; STT default `stt/stt-whisper.php`; Pocket TTS adapter `tts/tts-pockettts.php`; WSL note `lib/dwemerdistro_llm.php`; launcher https://github.com/Dwemer-Dynamics/CHIM-Launcher
- llama.cpp port change: commit `b42b7e6d` "server: use port 9931 by default (#30159)", `common/common.h`
- Player2 ports/APIs: https://github.com/elefant-ai/Player2APIDemo-AIGods · https://github.com/elefant-ai/player2-sdk-ts · community investigation (unverified): https://github.com/Pl2I/pl2i.github.io
- Interactive LLM Powered NPCs: https://github.com/AkshitIreddy/Interactive-LLM-Powered-NPCs
- Elden Ring repos found by GitHub search: https://github.com/Gr33nOps/EldenLore-RAG · https://github.com/CanerCakal/elden-oracle · https://github.com/J0hnBloodborne/EldenRAG · https://github.com/dsuther12/GideonGPT-RAG · https://github.com/joshua-hew/MaleniaGPT · https://github.com/minkofg/pixel-melina · https://github.com/alidhl/dialogue-generator · https://github.com/magichear/GPT-SoVITS-EldenRing · https://github.com/lorepamplona/ERPT-BR
- llama.cpp server: https://github.com/ggml-org/llama.cpp/blob/master/tools/server/README.md
- Ollama: https://github.com/ollama/ollama/blob/main/docs/api/openai-compatibility.mdx · https://github.com/ollama/ollama/blob/main/docs/faq.mdx
- Pocket TTS: https://github.com/kyutai-labs/pocket-tts · https://kyutai.org/blog/2026-01-13-pocket-tts
- Kokoro: https://github.com/hexgrad/kokoro · Piper: https://github.com/rhasspy/piper → https://github.com/OHF-Voice/piper1-gpl
- Moonshine: https://github.com/moonshine-ai/moonshine

**Elden Ring / FromSoftware (search snippets)**
- https://www.nexusmods.com/eldenring/mods/7999 (Elden Encouragement) · https://www.nexusmods.com/eldenring/mods/8046 (Un-Alone) · https://www.nexusmods.com/eldenring/mods/1587 · https://www.nexusmods.com/eldenring/mods/5983 (Chat with Melina)
- https://www.youtube.com/watch?v=0-g2ltVDR-c (Un-Alone video, not opened)
- https://steamcommunity.com/app/1245620/discussions/0/3790379982478837347 (character.ai Melina)
- https://yelore.itch.io/elden-ring-dublado
- https://www.hakko.ai/game-case/elden-ring/ · https://aiinsightsnews.net/hakko-ai-review/ · https://www.questie.ai/gaming-ai/elden-ring
- https://www.geeky-gadgets.com/elden-ring-ai-assistant/
- https://automaton-media.com/en/news/20230330-18306/ (ChatGPT and Dark Souls)
- https://pcoptimizedsettings.com/elden-ring-optimized-settings-2024-every-graphics-option-benchmarked/ (VRAM)
- https://www.pcgamer.com/elden-ring-will-have-a-maximum-framerate-of-60fps-on-pc/ (60 FPS cap)

**Other games and commercial**
- Bannerlord: https://exputer.com/news/games/mount-and-blade-2-bannerlord-mod/ · https://nexusmods.com/mountandblade2bannerlord/mods/5273 · https://www.nexusmods.com/mountandblade2bannerlord/mods/6514 · https://hackaday.com/2023/02/08/with-chatgpt-game-npcs-get-a-lot-more-interesting · https://inworld.ai/blog/best-ai-mods-for-better-ai-npcs
- Inworld: https://inworld.ai/blog/origins-unreal-engine-demo · https://docs.inworld.ai/guides/runtime-character
- Kenshi: https://harvicus.itch.io/sentientsands · https://www.nexusmods.com/kenshi/mods/1872
- RimWorld: https://www.gamebusiness.jp/article/2025/08/26/24927.html · https://steamcommunity.com/sharedfiles/filedetails/comments/3551203752
- Player2: https://blog.player2.game/p/ai-npc-games-were-too-hard-for-players · https://blog.elefant.gg/p/building-ai-npcs-with-player2-api · https://player2.game/ · https://modrinth.com/project/secondbrain
- Cyberpunk: https://tomshardware.com/news/mod-enables-chatgpt-fueled-cyberai-mod-in-cyberpunk-2077
- NVIDIA ACE / PUBG Ally: https://developer.nvidia.com/blog/how-krafton-built-pubg-ally-a-co-playable-character-powered-by-nvidia-ace/ · https://press.krafton.com/KRAFTON-REVEALS-PLAYTEST-PLANS-FOR-PUBG-ALLY-BUILT-WITH-NVIDIA-ACE · https://developer.nvidia.com/ace-for-games · https://blogs.nvidia.com/blog/ai-decoded-gamescom-ace-nemotron-instruct
- Ubisoft Teammates: https://gamesbeat.com/ubisofts-teammates-demo-showcases-the-future-of-ai-npcs-with-some-growing-pains/ · https://news.ubisoft.com/en-gb/article/3mWlITIuWuu0MoVuR6o8ps
- Where Winds Meet: https://www.pcgamer.com/games/rpg/wuxia-mmo-where-winds-meet-is-full-of-ai-chatbot-npcs-and-people-are-doing-all-the-standard-obscene-stuff-to-them-i-made-him-think-that-my-character-was-pregnant-with-his-child · https://automaton-media.com/articles/newsjp/where-winds-meet-20251128-368771/ · https://opencritic.com/news/24007/where-winds-meet-players-are-now-manipulating-ai-chatbots-to-help-fully-skip-side-quests
- Suck Up!: https://store.steampowered.com/app/2726370/Suck_Up/ · https://themagicrain.com/?p=42121
- Mantella and CHIM user sentiment (Reddit mirrors): https://reddit.birdcat.cafe/r/skyrimvr/comments/1kmcgyt/do_people_use_mantela_for_an_actual_play_through · https://reddit.birdcat.cafe/r/skyrimvr/comments/1kevxq2/installed_the_chim_ai_mod_immediately_pissed_off · https://www.uploadvr.com/playing-skyrim-vr-with-chatgpt-powering-npc-conversations/ · https://lr.sudovanilla.org/r/skyrimmods/comments/1f7o3mj/mod_release_ai_follower_framework
- SkyrimNet community docs: https://goncalo22.github.io/SkyrimNet-GamePlugin/Features/overview · https://www.patreon.com/skyrimnet/about
- Nexus AI policy: https://www.invenglobal.com/articles/28097/nexus-mods-makes-ai-disclosure-mandatory-for-every-mod-but-wont-ban-ai-content · https://www.videogameschronicle.com/news/nexus-mods-rules-out-full-ai-ban-despite-community-backlash/

**Research**
- https://arxiv.org/pdf/2404.17027 (player-driven emergence; latency survey) · https://arxiv.org/abs/2507.10469 (VR interrogation; 7 s cycle) · https://www.springerprofessional.de/en/llm-powered-npcs/51779742 (Mantella case study) · https://www.doria.fi/handle/10024/194912 (Åbo Akademi thesis)

**Models, pricing, TTS/STT benchmarks (volatile; LIKELY)**
- Gemma 4: https://ai.google.dev/gemma/docs/releases · https://simonwillison.net/b/9404
- Qwen3.5 small: https://www.producthunt.com/p/qwen3/qwen3-5-small · https://emelia.io/hub/qwen-35-9b-review
- OpenRouter: https://openrouter.ai/models/google/gemma-4-26b-a4b-it · https://openrouter.ai/docs/guides/routing/provider-selection · https://www.truefoundry.com/es/blog/openrouter-rate-limits-your-free-tier-depends-on-what-youve-already-paid
- DeepSeek V4 Flash pricing: https://modelcompare.dev/models/deepseek/v4-flash · https://pricepertoken.com/pricing-page/model/deepseek-deepseek-v4-flash
- Local model guides: https://bestllmfor.com/verticals/local-llm-for-game-dev/ · https://lmsa.app/blog/best-lm-studio-roleplay-models-for-8gb-vram-2026-guide/
- Embeddings: https://arxiv.org/pdf/2509.20354 (EmbeddingGemma) · https://surrealdb.com/blog/embedding-models-comparison.md
- TTS benchmarks: https://localaimaster.com/blog/best-tts-without-gpu · https://gigagpu.com/tts-latency-benchmarks · https://dev.to/andrew-ooo/pocket-tts-review-kyutais-100m-cpu-voice-cloner-2026-40lb
- STT: https://arxiv.org/html/2604.14493v1 · https://openwhispr.com/blog/parakeet-vs-whisper-vs-nemotron
- Inworld TTS: https://www.producthunt.com/products/inworld-ai/launches/inworld-tts

**Internal cross-references:** [01-engine-internals.md](01-engine-internals.md) (patch level, threading, saves) · [02-modding-ecosystem.md](02-modding-ecosystem.md) (me3, licences, EAC) · [03-dialogue-and-presentation.md](03-dialogue-and-presentation.md) (subtitles, TTS D1, Un-Alone presentation) · [04-game-design-and-world.md](04-game-design-and-world.md) (NPC roster, spoiler gating §7, copyright) · `../skyrimnet/01`–`06`.

---

## Verification log

Adversarial fact-check, 2026-11-06. "Confirmed" means re-read in an upstream primary source; for the three main repos, `git ls-remote` showed the clone is still upstream HEAD. "Corrected" means the report text was changed. "Unverified" means no independent primary source could be reached: nexusmods.com, openrouter.ai, developer.nvidia.com and blog.player2.game do not resolve in this session, and the web-search budget was exhausted. "Refuted" means the claim is wrong as stated.

| # | Claim | Verdict | Evidence (URL / repo path) |
|---|---|---|---|
| 1 | Mantella v0.14's default LLM is `google/gemma-4-26b-a4b-it:free` on OpenRouter | **Confirmed** (also the default for summary, function and profile models; vision defaults to `gemma-3-27b-it:free`) | https://github.com/art-from-the-machine/Mantella/blob/main/src/config/definitions/llm_definitions.py (L47, L55, L193; main = `c0c1ae6`) · `src/ui/settings_ui_constructor.py` L314–334 · `main.py` (`mantella_version = '0.14'`) |
| 2 | Mantella re-summarizes when the summary file exceeds 30% of the token limit; ≥ 5 messages by default | **Confirmed**, with detail corrected: 2 on reload, 0 for radiant; the limit is the *main* client's `token_limit`; the path is `<world_id>/<Name> - <ref_id>/<Name>_summary_<N>.txt`; the retry loop is unbounded | https://github.com/art-from-the-machine/Mantella/blob/main/src/remember/summaries.py (L32, L121, L230–275, L340–371) · `src/game_manager.py` L43 |
| 3 | FastAPI/uvicorn server, default port 4999; Skyrim side uses Leidtier's SKSE_HTTP | **Confirmed** | `src/http/http_server.py` L3–4 · `src/config/definitions/other_definitions.py` L106 · https://github.com/art-from-the-machine/Mantella-Spell (README) · Mantella FAQ "cannot connect to port 4999" |
| 4 | CHIM HerikaServer = PHP/PostgreSQL in the DwemerDistro WSL instance; defaults `pockettts`, `whisper`, `openrouterjson/openaijson/koboldcppjson` | **Confirmed**, with a caveat added: "whisper" is OpenAI's **cloud** Whisper API | https://github.com/Dwemer-Dynamics/HerikaServer `docs/building.md` L5, `lib/dwemerdistro_llm.php` L2 ("same WSL instance"), `conf/conf.sample.php` L169/L344/L527, `stt/stt-whisper.php` L19–21 · https://github.com/Dwemer-Dynamics/CHIM-Launcher |
| 5 | CHIM default combat-bark cooldown is 90 s, global across NPCs | **Corrected.** Global: yes. Default: **30 s** (seeded profile + Default preset); 100 s Local LLM; 20 s Follower; 90 s is only the unset fallback | HerikaServer `main.php` L1069–1100 · `lib/core/core_profiles.class.php` L51 · `lib/core/settings_presets.php` L66/L93/L164 · `ui/core/tmpl/metadata_json_editor.php` L45–48, L214 |
| 6 | Agent guide: a 1000-byte subtitle crashed Skyrim; 900-byte cap | **Confirmed** | HerikaServer `docs/agent-guide.md` L62 · `lib/sentence_boundaries.php` L50 (`CHIM_SPEECH_SEGMENT_MAX_BYTES = 900`) |
| 7 | SkyrimNet budget preset rotates gemma-4-31b-it / gemma-4-26b-a4b-it at temp 1.0 for default and combat | **Confirmed** (also `action_evaluation`). Rotation added 2026-09-26; fp8 floor added 2026-10-04. Corrected: memory slot = Qwen3-235B, not DeepSeek | https://github.com/MinLL/SkyrimNet-GamePlugin `model-presets/presets/openrouter-budget.yaml` (`57f333d`), `recommended-models.yaml` (`51a9cf3`), `openrouter-default.yaml` |
| 8 | No public in-process LLM NPC dialogue mod for ER/DS/Sekiro/BB as of 2026-11-06 | **Confirmed only for *NPC dialogue*, and only on GitHub**. **Refuted as broadened** in §4 ("every ER AI companion is out-of-process"): game-assistant uses an in-process me3 bridge DLL. Nexus, Bilibili and YouTube were not searchable | GitHub repo search (fact-check queries: "elden ring llm/chatgpt/ai npc/ollama/openai/assistant/claude", "dark souls llm", "sekiro llm", "bloodborne llm", "melina ai") · https://github.com/mpere056/game-assistant · https://github.com/mpere056/attack-on-elden-ring |
| 9 | EldenEncouragement = external exe reading memory, calling an author-hosted OpenAI-backed server, static voice lines not TTS | **Corrected / confirmed from source.** External C# WPF exe; `ReadProcessMemory` + event flags + a Cheat-Engine helper; author proxy `chat-proxy-r2t8.onrender.com/api/OpenRouter` (README says OpenAI); 260 static WAVs by default **plus optional Azure TTS with the user's own key**; 11 characters | https://github.com/mmahoney09110/EldenRingEncouragement (`EldenOverlay/MaidenReader.cs` L241, `EldenTTS.cs`, `settings.ini`, `Audio/`) · https://github.com/mmahoney09110/OpenAI-Proxy-Server |
| 10 | Player2: ~1 in 20 downloads of its AI-NPC Minecraft mod reached gameplay due to setup friction | **Unverified** (blog.player2.game unreachable; stays LIKELY). Ports confirmed: `localhost:4315` app, `api.player2.game/v1` hosted | https://blog.player2.game/p/ai-npc-games-were-too-hard-for-players (not opened) · https://github.com/elefant-ai/Player2APIDemo-AIGods · https://github.com/elefant-ai/player2-sdk-ts |
| 11 | Pocket TTS: MIT, released 2026-01-13, 100M CPU model, ~200 ms first chunk, 1.9–18.2M English models in Oct 2026 | **Confirmed** (vendor figures). Caveats added: `serve` is `POST /tts` on :8000 (not OpenAI); voice-cloning weights are in a **gated** HF repo; standalone Windows binary exists | https://github.com/kyutai-labs/pocket-tts (`README.md`, `LICENSE`, `pocket_tts/main.py` L85/L147, `pocket_tts/config/*.yaml`) · https://kyutai.org/blog/2026-10-20-pocket-tts-small (not opened) · HF `kyutai/pocket-tts` returns "Access … is restricted" |
| 12 | Ollama unloads models after 5 min idle by default unless `keep_alive` is set | **Confirmed.** Caveat added: `keep_alive` is documented only for native `/api/generate` and `/api/chat`; use `OLLAMA_KEEP_ALIVE` or a native warm-up for `/v1` clients | https://github.com/ollama/ollama/blob/main/docs/faq.mdx ("How do I keep a model loaded…") · `docs/api/openai-compatibility.mdx` |
| 13 | Mantella docs: Llama-3-8B Skyrim fine-tune now worse than newer non-fine-tuned models, no longer recommended | **Confirmed** (docs branch last updated 2026-04-18) | https://github.com/art-from-the-machine/Mantella/blob/gh-pages/docs/pages/fine_tuned_models.md L32 |
| 14 | llama.cpp `llama-server` default base URL `localhost:8080/v1` | **Refuted for current builds.** Default port is **9931** since 2026-10-09 (`b42b7e6d`, PR #30159); 8080 only on older builds | https://github.com/ggml-org/llama.cpp/blob/master/tools/server/README.md (`--port`, default 9931) · `common/common.h` L632 |
| 15 | Pocket TTS `serve` is a `/v1/audio/speech` server | **Refuted.** Third-party wrappers or audio.cpp provide OpenAI compatibility | `pocket_tts/main.py` · README "Projects using Pocket TTS" (teddybear082 wrapper) · CHIM `tts/tts-pockettts.php` L50–51 |
| 16 | SkyrimNet: "who pays = user (BYO key or local)" | **Corrected.** README promotes the first-party paid **Fateless** inference (from $10/month) first; SkyrimNet is "not open source" | https://github.com/MinLL/SkyrimNet-GamePlugin/blob/main/README.md ("Get started", "License") |
| 17 | MinAI's repo now points to SkyrimNet | **Unverified.** `MinLL/MinAI` is no longer publicly reachable; an old fork only points to LoversLab | https://github.com/programotter/MinAI |
| 18 | Mantella falls back silently | **Corrected** to "masked failure": an error sound plus an in-character line, with retries | `src/output_manager.py` L476–490 |
| 19 | Chat with Melina "can corrupt quest flags" | **Confirmed and sharpened:** it uses 5 event flags as scratch state | https://github.com/lraty-li/Elden-Ring-Mods/blob/main/Chat-with-Melina/README.md L17 |
| 20 | CHIM 15 combat-bark cues; memory knobs (12 min delay, 1 record, interval 10, min 5 events, threshold 3, bias 33) | **Confirmed.** `MEMORY_BIAS_A` is commented as "Minimal distance to offer memory", so its exact semantics are unverified | HerikaServer `prompts/prompts.php` L106–124 · `conf/conf.sample.php` L605–617 |
| 21 | Mantella CSV 3,203 rows, median bio ≈ 1,190 chars; FO4 631 rows; Fandom CC BY-SA bios | **Confirmed** (median 1,186) | `data/Skyrim/skyrim_characters.csv`, `data/Fallout4/fallout4_characters.csv`, Mantella `README.md` "Attributions" |
| 22 | Mantella latency guide (< 0.5 s; lazy lip-sync ≈ 0.5 s; Fast Response Mode ≈ 0.5 s; Proactive STT) | **Confirmed** | https://github.com/art-from-the-machine/Mantella/blob/gh-pages/docs/pages/real_time_npcs.md L6–46 |
| 23 | me3 bans LLM use in its spaces; fromsoftware-rs ships NON-AI licence files | **Confirmed** | me3 `AI_CODE_POLICY.md` L3 · fromsoftware-rs `LICENSE-MIT-NON-AI`, `LICENSE-ASL2-NON-AI` |
| 24 | Piper moved to GPL `piper1-gpl`; Kokoro Apache-2.0 | **Confirmed** | https://github.com/rhasspy/piper (README) · https://github.com/OHF-Voice/piper1-gpl/blob/main/COPYING (GPLv3) · https://github.com/hexgrad/kokoro/blob/main/LICENSE |
| 25 | Interactive-LLM-Powered-NPCs: face recognition, SadTalker, `pre-conversation.json`, Cohere | **Confirmed** | https://github.com/AkshitIreddy/Interactive-LLM-Powered-NPCs (README L5–45) |
| 26 | OpenRouter `:free`: 20 rpm, 50/day (1,000/day after $10) | **Unverified** (openrouter.ai unreachable; matches the checker's recollection of the 2025 docs) | https://openrouter.ai/docs/api/reference/limits (not opened) |
| 27 | PUBG Ally uses an on-device Mistral-NeMo-Minitron **2B** | **Unverified.** CES 2025 materials (checker's recollection) named an 8B Minitron model, so the size may have changed or be misreported | developer.nvidia.com (unreachable) |
| 28 | Nexus AI-disclosure rule (~2026-11-03), Ubisoft *Teammates*, *Where Winds Meet*, *Suck Up!*, Hakko, Sentient Sands, RimTalk, user sentiment | **Unverified** (press and snippet-only; no reachable primary source; left as LIKELY) | as cited in Sources |
| 29 | Moonshine 2026 "Moonshine Voice" on-device streaming toolkit | **Confirmed** (now also does TTS) | https://github.com/moonshine-ai/moonshine (README) |
| 30 | llama.cpp `cache_prompt` default on, `--reasoning off`, `--reasoning-budget 0`, `--slot-save-path`, `json_schema` | **Confirmed** | llama.cpp `tools/server/README.md` (L223–239, L581, L597) |
