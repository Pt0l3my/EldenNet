# Research notes

These notes are the evidence base for the EldenNet design ([../design/DESIGN.md](../design/DESIGN.md)). They were written in November 2026, against Elden Ring patch 1.17.2 (exe 2.7.2.0) and SkyrimNet beta30-rc2.

Read the two summaries first. The numbered notes hold the detail and the sources.

## SkyrimNet: what it does with Skyrim and what we can borrow

| Note | Topic |
|---|---|
| [00-summary](skyrimnet/00-summary.md) | **Start here.** How SkyrimNet works, its design principles, a 44-row pattern catalogue (copy / simplify / defer / drop for EldenNet), lessons from its open bugs, default numbers and the licensing boundary |
| [01-architecture](skyrimnet/01-architecture.md) | Runtime architecture, subsystems, threading, configuration, LLM job slots, content library, APIs |
| [02-conversation-pipeline](skyrimnet/02-conversation-pipeline.md) | One conversation turn from start to finish: input, audience, speaker selection, prompt layout, streaming, commit, scheduler |
| [03-prompts-and-characters](skyrimnet/03-prompts-and-characters.md) | The template system, decorators, how character bios are structured, generic bios, filters |
| [04-memory-and-knowledge](skyrimnet/04-memory-and-knowledge.md) | Event log, memory generation and recall, diaries, world knowledge, identity, save timelines |
| [05-events-perception-triggers](skyrimnet/05-events-perception-triggers.md) | Game events, perception, triggers, combat speech, the Papyrus/ESP layer |
| [06-speech-ui-dialogue](skyrimnet/06-speech-ui-dialogue.md) | TTS/STT, audio, lip sync, subtitles, chat overlay, how it works with vanilla dialogue |
| [07-actions-decisions-gamemaster](skyrimnet/07-actions-decisions-gamemaster.md) | Actions, decisions, GameMaster, thoughts, and the extension seams EldenNet should keep |
| [08-skyrim-engine](skyrimnet/08-skyrim-engine.md) | The Creation Engine under an AI NPC mod, and how each feature maps to Elden Ring |

## Elden Ring: engine, modding, design and feasibility

| Note | Topic |
|---|---|
| [00-summary](eldenring/00-summary.md) | **Start here.** Skyrim vs Elden Ring, a feasibility matrix, the recommended stack, the rejected alternatives, the hard problems ranked by risk |
| [01-engine-internals](eldenring/01-engine-internals.md) | Engine lineage, tasks and threads, singletons, the character system, text (FMG), event flags, talk scripts (ESD), saves, patch level |
| [02-modding-ecosystem](eldenring/02-modding-ecosystem.md) | EAC and offline play, the me3 loader, fromsoftware-rs, overlays and input, patch churn, licences, community climate |
| [03-dialogue-and-presentation](eldenring/03-dialogue-and-presentation.md) | How Elden Ring talk works (ESD, TalkParam, TalkMsg, Wwise), lip movement, presentation options for LLM dialogue |
| [04-game-design-and-world](eldenring/04-game-design-and-world.md) | NPC taxonomy, talk codes, questline flag blocks, tone statistics, bark vocabulary, lore primer, spoiler risks |
| [05-game-state-and-events](eldenring/05-game-state-and-events.md) | Which state and events a mod can read (player, location, time, characters, combat, flags, menus) and how robustly |
| [06-prior-art](eldenring/06-prior-art.md) | Mantella, CHIM, SkyrimNet and others; Elden Ring precedents; lessons on latency, memory, setup friction and model defaults |
| [07-world-and-quest-leads](eldenring/07-world-and-quest-leads.md) | World-stage flags (endings, story, legacy dungeons, Great Runes, map swaps, Radahn's star, the Erdtree and the ash), boss-kill flags, the v1 personas' flag blocks, related quests and what spike S6 must settle |
| [08-game-data-sources](eldenring/08-game-data-sources.md) | Where the roster's game ids can come from: online datasets, DSMapStudio, Smithbox and the tools around them, with their licences and AI policies; which files hold which ids; how our tools read Smithbox's exports (O55), and the question of how far they go before the blueprint |
| [leads/roster-rows.json](eldenring/leads/roster-rows.json) | O59's research file: the 213 name numbers NpcParam rows carry with a real name, each with its row ids (699 rows), from DataPiler's job en-0004 on game 2.7.2.0. Numbers only, every lead unverified until EldenNet checks it in game |
| [jobs/](eldenring/jobs/) | DataPiler's job files en-0001 onwards: the roster's counts and leads (WHY§12), and the boss routes (WHY§36). Our own data: operations and numbers, no game text |

## Method and ground rules

- **Evidence labels.** FACT means taken from a primary source. LIKELY means several weaker signals agree. SPECULATION or INFERENCE means our own reasoning. Every report ends with a review or verification log from an independent fact-checking pass.
- **SkyrimNet.** We studied only what SkyrimNet openly ships and documents: devkit docs, readable content (prompts, YAML, SQL migrations), Papyrus sources, the MIT-licensed API headers, the public repository and public web pages. Nothing came from inspecting its compiled files. These notes reuse none of SkyrimNet's prompts, templates, bios or code; they record ideas and published numbers (see [skyrimnet/00-summary §7](skyrimnet/00-summary.md)). Since 2026-11-07 SkyrimNet's author has approved EldenNet building on SkyrimNet (EldenNet `CLAUDE.md`, Licences).
- **Elden Ring.** Facts about the game come from public modding sources (fromsoftware-rs, The Grand Archives CE table, Smithbox/Paramdex, ESDLang, me3 and others), cited inline. No FromSoftware files are stored in this repository.
- **Time-sensitive.** Patch levels, tool versions, licences and community policies were current on each note's research date (2026-11-06; 2026-11-08 for 08-game-data-sources). Re-check them before relying on them.
