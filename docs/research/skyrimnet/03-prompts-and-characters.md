# SkyrimNet Research 03: The Prompt Engineering Layer, Character Bios and Filters

Research note 03 for the EldenNet project, an LLM-driven NPC framework for Elden Ring.
Subject: SkyrimNet beta30-rc2 (Fateless.Ai / MinLL), the LLM NPC mod for Skyrim SE/AE.

> **Method and license note.** This note is based only on documentation, the human-readable content library
> (Inja `.prompt` templates, YAML, CSV), SQL migrations, Papyrus `.psc` sources, the public GitHub repository
> and public GitHub pages. No compiled file was inspected. **INFERENCE** marks my own reading of that evidence,
> not a confirmed fact about the closed-source DLL. SkyrimNet's prompts and bios may not be reused in other
> software, so this note describes and paraphrases them. Quotes are a few short fragments needed to show a
> design. EldenNet will write its own prompts and personas.

**Path prefixes**

| Prefix | Location |
|---|---|
| `DATA/` | `skyrimnet-beta30-rc2/SKSE/Plugins/SkyrimNet/` |
| `LIB/` | `DATA/library/skyrimnet.base/` (shipped content layer); `LIB/prompts/...` below |
| `SRC/` | `skyrimnet-beta30-rc2/Source/Scripts/` |
| `DEV/` | `skyrimnet-devkit-beta30-rc2/` |
| `GH/` | public repo `MinLL/SkyrimNet-GamePlugin` |
| `ER/` | `EldenNet/docs/research/eldenring/` (our own Elden Ring notes) |

Related notes: `01-architecture.md` (runtime, model slots, content layers) and `02-conversation-pipeline.md`
(message layout, history windowing, the "said since you last spoke" recap, and the full output contract).
This note does not repeat them. It covers the template language, how prompts are composed, the decorator
surface, the character bio system, filters, and the rules the model is given.

---

## TL;DR

- **Prompts are data.** Every LLM call is an Inja template (`.prompt`) in a layered content library. The base set has 17 top-level prompts and 58 numbered submodules, plus components, decision templates, transformers, helpers and 1,509 character bios (`LIB/prompts/`). Users and mods override any file by putting one at the same path in a higher layer (`DEV/docs/modding/MIGRATING_TO_BETA25.md`).
- **The file format is Inja with message markers.** `[ system ]`, `[ user ]`, `[ assistant ]`, `[ cache ]` and `[ raw ]` split the rendered text into chat messages and prompt-cache breakpoints. Decision templates use a second dialect (`[ state … ]`, `[ question … ]`, `[ criteria ]`) that compiles to a typed classification request instead of chat (`DEV/docs/modding/WORKFLOW_PROMPTS.md` §4.4).
- **Composition uses three functions:** `render_template(path, vars)`, `render_subcomponent(folder, mode, vars)` (concatenates a folder's numbered files in filename order) and `render_character_profile(mode, uuid)` (renders the `character_bio` folder for one actor). A **render mode** string (`full`, `stable`, `speech_style`, `dialogue_target`, `short_inline`, `interject_inline`, `thoughts`, `transform`, `bio_*`, …) lets each submodule decide what to emit for the speaker, the listener or a one-line roster entry.
- **Bios are template children.** A bio file contains only `{% block summary %}…{% endblock %}` style blocks: summary, interject_summary, background, personality, appearance, aspirations, relationships, occupation, skills, speech_style. The `character_bio` submodules are the skeleton. They supply headers, live game state and the per-mode logic, and leave holes the bio's blocks fill. All 1,507 real bios fill all ten blocks, and none contains any template logic.
- **Bio lookup is by name, with a FormID suffix.** Unique bios are `<sanitized_english_name>_<FormID & 0xFFF as 3 hex>.prompt` (1,203 files). Shared bios are `<name>_generic.prompt` (302 files) and are matched by display name, mapped through translation CSVs so localized games still resolve. `_generic` means "keyed by name only". It covers role bios (bandit, guard) and also named beings with several records (`alduin_generic`, `boethiah_generic`). An empty `default.prompt` is the floor. NPCs with no bio get one generated from game data, including their canonical vanilla dialogue lines (`helpers/generate_profile.prompt`).
- **Generic NPCs reuse the full bio machinery.** A bandit or guard gets a ~550-word shared bio. The best generic bios describe *several archetypes* and tell the model to pick one and stay consistent, so a camp of identical bandits does not sound identical. Creatures get a race-keyed "can/cannot speak, makes these sounds" line. Filters (`LIB/filters/*.yaml`) stop races such as dogs or mannequins from speaking or forming memories.
- **Hard-won rules show up in comments and changelogs.** Put the voice spec right after the persona and close to the end (recency). Turn numbers into words, because models parrot numbers. Keep spoilers out of the public `summary`. Don't prime bios with "clipped/terse", which produces tough-guy one-liners. Don't assume the player is the Dragonborn. Bios evolve only conservatively (`dynamic_bio_update.prompt`: "95% of updates should be MINIMAL or require NO CHANGE", plus a "Stranger Test" for public blocks).
- **About 130 decorators are called** across the shipped templates, in 13 groups (actor, combat/magic, equipment, location/time, events, memory, quests, config…). The bundled `DATA/docs/decorators.html` documents only a small subset. Its footer says "40 decorators, 18 variables" in 13 categories, but 8 of the 40 entries are really variables (`player`, `location`, `gameTime`…), so it describes 32 callable functions, 6 of them string utilities.
- **For Elden Ring,** copy the *structure*: numbered composable sections, render modes, a block-based persona schema with public/private separation, archetype cards for generics, words instead of numbers, and the voice spec at the end. Do the logic in native code over a game-thread snapshot rather than in template code. Drop the action, vision, telepathy, identity-possession and audio-tag layers for v1.

---

## 1. Inventory of the prompt layer

`LIB/prompts/` (beta30-rc2) has these subfolders. File counts are from the shipped tree.

| Folder | Files | Role |
|---|---|---|
| top level | 17 | One per LLM job: `dialogue_response`, `npc_thoughts`, `player_dialogue`, `player_thoughts`, `diary_entry`, `dynamic_bio_update`, `character_profile_update`, `bard_song_lyrics`, `gamemaster_*` (2), `native_action_selector*` (2), `agent_*` (3), `stt_hints`, `warmup` |
| `submodules/` | 58 | Numbered sections composed by folder: `character_bio` (18), `user_final_instructions` (13), `system_head` (8), `guidelines` (1), `cache_split` (1), `omnisight_*` (6×1), `test_decorators` (11) |
| `characters/` | 1,509 | Bios: 1,203 unique `_XXX`, 302 `_generic`, plus `default`, `player_special` and 2 odd-suffix files |
| `components/` | 15 | Reusable pieces: `event_history` (+ `_compact`, `_verbose`), `context/scene_context*`, `dialogue_action`, `memory_access`, legacy bio layouts |
| `decisions/` | 6 | Typed classification templates: speaker selection, player target, mood, memory recall gate, continue narration |
| `target_selectors/` | 2 | Chat fallbacks for the speaker and target decisions |
| `transformers/` | 4 | Rewrite or translate a given line: vanilla line in the NPC's voice, subtitle translation, player options, "universal translator" speech patterns |
| `translation/` | 2 CSV | Map (plugin, FormID/refID) to an English bio key (§5.5) |
| `helpers/` | 3 | `generate_profile` (new bio), `evaluate_mood`, `generate_search_query` |
| `memory/`, `omnisight/`, `eval/`, `web/`, `documentation/`, `dev/` | 5/7/10/2/2/1 | Memory creation and ranking, vision descriptions, LLM-judge benches, dashboard pages |

**INFERENCE:** the split mirrors the model slots in note 01. Each top-level prompt (or decision template) is one request kind sent to one variant (dialogue, meta, memory, decisions, CharacterProfileGeneration…). Submodules are shared building blocks, so a change to a bio section or a format rule reaches every request kind that composes it.

---

## 2. The `.prompt` file format

### 2.1 Inja features in use

Inja is a C++ Jinja-like engine. The devkit warns "Inja is NOT Jinja2" and lists its limits (`WORKFLOW_PROMPTS.md` §3.7; full syntax in `DEV/docs/modding/prompt-file-syntax.md`). Here is what the shipped templates actually use. Counts are files outside `characters/`.

| Feature | Example of use | Notes |
|---|---|---|
| `{{ expr }}`, `{% if/elif/else if %}`, `{% for x in list %}`, `{% for k, v in obj %}` | everywhere; `else if` in 30 files, `elif` in 24 | `loop.is_last`/`loop.index` used for separators and numbering |
| `{% set %}` with arrays and object literals | `0760_spoken_language` holds a per-language example dictionary | `append`/`remove` return new arrays (event_history keeps a list of `[actor, location]` pairs) |
| `{% block name %}{% endblock %}` | all bio submodules, `components/context/scene_context*` | the core mechanism behind bios (§5.2) |
| `{% extends "…" %}` | 3 agent prompts extend `components/agent_tools_base` | ordinary template inheritance |
| `{% raw %}…{% endraw %}` | JSON output skeletons in `helpers/generate_profile`, `memory/*` (4 files), `gamemaster_scene_planner`, `player_thoughts`, `agent_prompt_helper`, `components/agent_tools_base` | keeps literal braces. The eval prompts use the *message-level* `[ raw ]` marker instead (§2.2) |
| `{% include %}`, `super()` | documented in `prompt-file-syntax.md` | **not used** in any shipped file. Composition goes through the render functions (§3.1) |
| Pipes | `{{ subj \| capitalize }}`, `x \| default(true)` | rare |
| Whitespace control `{%- -%}` | 3 files | there is also a global `trim_blocks`/`lstrip_blocks` config (`prompt-file-syntax.md`) |
| `{# … #}` comments | almost every core file | used as **design rationale for maintainers**, never sent to the model (§10) |
| `default(x, fallback)`, `exists("v")`, `existsIn(obj,"k")`, `isString`, `length`, `at` | guards for optional context variables | the main defence against "variable not found" render failures |

The template code is heavy. `event_history.prompt` (160 lines) computes prompt-cache anchors with power-of-two steps, tracks per-speaker locations, and buckets time gaps into phrases, all in Inja. `9990_speech_style.prompt` is a 52-branch `if/else if` chain keyed on race names (43 branches begin "cannot use human words…"). It sits inside a nested `{% block speech_style_creature %}`, so a bio *could* override it, though none does. The same file appends obedience lines for summoned or reanimated allies. **INFERENCE:** this keeps everything user-editable and hot-reloadable, but it pushes real program logic into a language with no functions, no early return and no unit tests. That is why the repo ships a `test_decorators` submodule set and an MCP `validate_prompt` tool (`WORKFLOW_PROMPTS.md` Phase 7).

### 2.2 Message markers

Markers are processed **after** Inja rendering. They split the text into a message list (`WORKFLOW_PROMPTS.md` §4.4):

- `[ system ] … [ end system ]`, `[ user ] … [ end user ]`, `[ assistant ] … [ end assistant ]` become chat messages.
- `[ cache ] … [ end cache ]` marks a prompt-caching breakpoint. A provider without caching ignores it. `dialogue_response` has four in its system message and one at the last history event (layout in note 02 §4.1).
- `[ raw ] … [ end raw ]` keeps marker-like text from being parsed. It is used in 7 `eval/*` judge prompts and in `agent_prompt_helper`, which embed other prompts or text that might contain markers.
- The docs say markers are "only for top-level prompts", not submodules. Components rendered at top level may still emit them. `components/event_history` emits one `[ assistant ]` message per line *this NPC* said and a `[ user ]` message for everything else, so the history becomes real multi-turn chat with the NPC as the assistant. **INFERENCE:** the restriction applies to the bio and instruction submodules, which are rendered *inside* a message.

**Decision templates** (`prompts/decisions/*.prompt`) use another vocabulary: `[ state <field> ]` blocks of named context, and `[ question <id> choice|score|noul ]` with a `[ criteria ]` list of one-line `key: description` options. The game reads the answer by option key, together with probabilities. Mods can ship their own and call them via `SkyrimNetApi.SendCustomDecisionToLLM` (`WORKFLOW_PROMPTS.md` §4.4). The speaker selector (`decisions/dialogue_speaker_selector.prompt`) builds its candidates block out of `render_character_profile("short_inline")` and `("interject_inline")`. This is where the bio's `interject_summary` field earns its place (§5.1).

---

## 3. Composition

### 3.1 The three render functions

| Function (FACT: signatures as used) | What it does |
|---|---|
| `render_template("components\\event_history", {"cache_history": true})` | Renders one file by library-relative path (Windows separators). The optional object adds variables to the child's context. |
| `render_subcomponent("system_head", "full", {"stable_split": true, "task_at_end": true})` | Renders **every file** in `submodules/<name>/` in filename order, with `render_mode` set to the second argument and extra variables from the object. |
| `render_character_profile("short_inline", uuid)` | Renders the `character_bio` submodule folder for actor `uuid` with `actorUUID` and `render_mode` bound, and the actor's bio file supplying the `{% block %}` bodies. |

Two other renderers wrap these. `get_scene_context(npcUUID, targetUUID, "full")` combines `components/context/scene_context.prompt`, which defines blocks such as `nearbyNpcs`, `situationSummary` and `weather`, with a mode-specific layout file (`scene_context_full`, `scene_context_target_selection`) that places those blocks. **INFERENCE:** this is the same "content defines blocks, layout arranges them" pattern as bios. The Papyrus API exposes `RenderTemplate`, `ParseString` and `SendCustomPromptToLLM` so mods can render or send their own templates (`SRC/SkyrimNetApi.psc`).

A useful trick: the same submodule can be rendered twice with different flags to *move* it. `dialogue_response` renders `system_head` with `task_at_end: true`, which suppresses the "## Task" section. It then renders `system_head/0010_instructions` again on its own at the very end of the user message, so the task line sits next to the newest dialogue (`LIB/prompts/dialogue_response.prompt`, `submodules/system_head/0010_instructions.prompt`).

### 3.2 Numbered submodule folders

Files concatenate in filename order. Numbers leave gaps so mods can slot in (`DEV/docs/modding/prompts-and-decorators.md`; `WORKFLOW_PROMPTS.md` §6.1 suggests `03xx–04xx` for mod additions to bios). Where files share a number (`0010_instructions`, `0010_setting`), **INFERENCE:** they order alphabetically by full name.

| Folder | Sections in order (number → purpose) |
|---|---|
| `system_head` | 0010 task line (per mode) · 0010 setting (an empty, user-editable "# Setting" slot) · 0020 guidelines + length + grounding rules · 0100 the speaker's full profile + the dialogue target's profile · 0150 telepathy note · 0200 scene context · 0250 vision descriptions · 0400 speech-style profile, deliberately **last** |
| `character_bio` | 0010 header + vitals as words · 0050 physical activity · 0100 summary · 0130 world knowledge · 0200 background · 0300 personality · 0310 interject summary · 0320 aspirations · 0400 appearance (vision description overrides) · 0410 worn equipment · 0500 skills · 0600 relationships + travel history with player · 0610 party quests · 0700 occupation · 7000 long-term-memory hook · 7050 identity links · 7100 relevant memories · 9990 speech style (+ creature speech) |
| `guidelines` | 0500 roleplay guidelines |
| `user_final_instructions` | 0150 environment (empty) · 0160 telepathy reception · 0200 combat status + strength assessment · 0500 response format · 0650 voice tags · 0700 no-narration reminder · 0740 quest topics to raise · 0750 embedded actions · 0760 spoken language / translation · 0800 direct narration must be answered · 0805 merged events · 0810 continue-narration · 8000 recent state changes |

### 3.3 Render modes: one source, many views

Every bio submodule opens by computing which modes it serves. The mode controls both *whether* a section appears and *from whose perspective* it is written (`WORKFLOW_PROMPTS.md` §4.6).

| Mode | Used by | What the bio skeleton emits |
|---|---|---|
| `full` | speaker in dialogue | everything: vitals, activity, all blocks, equipment, quests, memories, identity links |
| `stable` / `full`+`stable_split` | cached system message / dynamic remainder | stable = authored blocks only; split full = only the volatile parts (state, equipment, travel history, quests, memories). Together they equal `full` |
| `speech_style` | appended last in the system message | only the speech-style block, the optional speech pattern, and creature speech limits |
| `dialogue_target` | the listener's profile in the speaker's prompt | header, gender, race, vitals, activity, **summary + appearance** + the target's worn equipment. Observable facts only |
| `short_inline` | nearby-people lists, selectors, GameMaster | the **summary** block only |
| `interject_inline` | speaker selection | the **interject_summary** block only |
| `thoughts`, `transform`, `book` | inner monologue, rewriting the player's line, reading a book | first-person variants with mode-specific length rules |
| `bio_summary`, `bio_background`, `bio_personality`, … | identity-link layer (7050) | exactly one block, no header |

Naming drift: the devkit's render-mode table (`WORKFLOW_PROMPTS.md` §4.6) documents a third-person observable mode called `target`. The shipped bio submodules call it `dialogue_target`, and `target` survives only in `test_decorators` and `agent_prompt_helper`. A mod author following the docs literally would write a section that never renders for listeners. Two more length variants exist beside the dialogue rules in §8: `transform` (8–45 words) and `book` (8–90 words, "brief for mundane books") (`system_head/0020_format_rules`).

The point of this design is **level of detail by audience**. The speaker sees its own private interior. Everyone else sees only the public summary and appearance. A roster line is one sentence. This is the reason behind the "Stranger Test" (§6.2): the public blocks leak into *other* NPCs' prompts.

### 3.4 Context variables

Besides decorators, the engine binds variables per request (`WORKFLOW_PROMPTS.md` "Context Variables Reference"): `npc` (speaker; `npc.UUID`, `npc.name`), `player`, `actorUUID` and `render_mode` (in submodules), `responseTarget` (`{UUID, type: player|npc}`), `triggeringEvent`, `events` (pre-filtered history), `location`, `is_indoors`, `gameTime`/`gameTimeNumeric`/`gameTimeJson`, `currentWeather`, `season`, `eligible_actions`, `embed_actions_in_dialogue`, `promptForDialogue`/`promptForThoughts`, `speech_turn` (merged events), and caller flags such as `stable_split`, `speech_cached`, `cache_history`, `combat_at_end`, `task_at_end`.

---

## 4. Decorator catalog

Decorators are native functions that templates call to pull live game state. Mods add more from Papyrus with `SkyrimNetApi.RegisterDecorator(id, script, function)`, which returns a JSON string (`SRC/SkyrimNetApi.psc`; `prompts-and-decorators.md`). One caveat is documented: a mod decorator evaluated on an actor that is neither the speaker nor the target fails with "function not found". DLLs register native decorators with `PublicRegisterDecorator(name, description, callback)` (`DEV/CppAPI/PublicAPI.h`, API v5+). The header says the callback "Must be thread-safe". It also shows that the same decorator names drive **action eligibility rules** in action YAML (`decoratorName`, `arguments`, `comparisonOperator`, `expectedValue`), so one state-query vocabulary serves both prompts and gating. **INFERENCE:** "thread-safe" means decorators run during rendering on worker threads, not on the game thread. That fits the snapshot recommendation in §12.2. `PublicGetBioTemplateName(formId)` (v3+) lets other DLLs ask which bio an actor resolved to.

**Counts (FACT):** `DATA/docs/decorators.html` has the footer "Total decorators: 40 | Total variables: 18" and 13 categories: Actor, Environment, Equipment, Faction, Game System, General, Item Customization, Mood, Player, Quest, Scene, String Utilities, World. Read closely, the 40 "decorator" entries include 8 that are variables (`currentWeather`, `player`, `moodsList`, `moodDescriptions`, `currentTime`, `gameTime`, `gameTimeJson`, `location`). That leaves **32 callable functions**: 26 game functions plus 6 string utilities. The "General" category holds no decorators, only dashboard page variables (`page_name`, `request_path`, `server_name`…). Only 16 variable entries are actually listed. The page is a rendered copy of the library's own `prompts/documentation/main.prompt`/`category.prompt` HTML templates, so the same Inja engine also builds the dashboard's documentation pages. **INFERENCE:** the bundled file is a stale export of that page, so it is a partial snapshot. Scanning every `{{ }}`/`{% %}` tag in the shipped templates (excluding bios and Inja built-ins) finds **130 distinct callable names**, including the 3 render functions. That is consistent with FEATURES.md's "over a hundred built-in decorators". The grouping below is mine, in the categories requested. Purposes come from the HTML descriptions where they exist, otherwise from how the templates use them.

| Group (count) | Decorators | Purpose |
|---|---|---|
| **Actor identity and state (22)** | `decnpc`, `isValidActor`, `get_name`, `is_player`, `actor_has_keyword`, `distance_between`, `has_line_of_sight`, `get_linked_identities`, `is_unconscious`, `is_sneaking`, `is_walking`, `is_sprinting`, `is_swimming`, `is_knocked_down`, `is_mounted`, `get_mount_race`, `get_furniture`, `is_in_package`, `get_actor_position_relative_to_actor`, `get_actor_position_relative_to_camera`, `get_actor_mood`, `mood_description` | `decnpc(uuid)` is the hub. It returns name/first/last, gender, race, class, level, pronouns (subjective/objective/possessive/reflexive), health/magicka/stamina and their maxima, all skills, bio fields (summary, background, personality, speechStyle), relationship rank (−4..4), flags (`isInCombat`, `isDead`, `isGuard`, `isEssential`, `isChild`, `isCapableOfSpeech`, `isVirtual`…), `spokenLanguage`, `universalTranslatorSpeechPattern`. The others are movement and activity predicates, spatial checks and mood |
| **Player and party (6)** | `is_follower`, `get_selected_quests`, `get_civil_war_side`, `get_crime_gold`, `is_first_person`, `is_vr` (+ `player` object variable) | party membership, tracked quests, allegiance, bounty |
| **Combat, vitals, magic (17)** | `is_in_combat`, `get_combat_targets`, `get_actor_value`, `get_base_actor_value`, `has_weapon_drawn`, `is_hostile_to_actor`, `is_summoned`, `is_reanimated`, `get_spell_list`, `has_spell`, `has_perk`, `get_all_perks`, `has_magic_effect`, `get_effect_archetype`, `has_magic_archetype`, `get_customized_spell_name`, `is_spell_enabled` | combat state, targets, AV ratios, hostility, magic-effect checks (the usual way to detect another mod's state) |
| **Equipment and items (17)** | `get_worn_equipment`, `worn_has_keyword`, `get_inventory`, `get_merchant_inventory`, `get_tempered_items`, `has_key`, `has_item`, `get_item_count`, `get_item_name`, `get_item_description`, `get_item_customization`, `is_item_enabled`, `get_item_name_for_wearer`, `get_item_description_for_wearer`, `has_item_description_for_wearer`, `get_container_contents`, `get_form_name` | worn slots with keywords, inventory, and *item customization* (user-curated names and descriptions; per-wearer vision descriptions) |
| **Factions, relationships, groups (4)** | `is_in_faction`, `get_faction_rank`, `get_relationship_rank`, `get_npc_group_members` | membership by editor ID, rank, relationship rank, user-defined NPC groups |
| **Location, world, time (10)** | `get_location`, `location_building`, `get_place_names`, `get_nearby_npc_list`, `get_visible_npc_list`, `get_nearby_references`, `get_nearby_references_for_entity`, `get_scene_context`, `short_time`, `units_to_meters` (+ variables `location`, `is_indoors`, `gameTime`, `gameTimeJson`, `gameTimeNumeric`, `currentTime`, `currentWeather`, `season`) | where an actor is, the building part of a location (used to detect "moved"), nearby and visible actors, scene block, time formatting, unit conversion |
| **Vision / OmniSight (9)** | `capture_current_scene`, `capture_screenshot`, `get_current_scene_description`, `has_current_scene_description`, `get_current_location_description`, `has_current_location_description`, `is_scene_newer_than_location`, `get_omnisight_description`, `has_omnisight_description` | screenshot-to-description cache feeding "What you currently see" and appearance |
| **Memory, knowledge, diary (4)** | `get_relevant_memories`, `get_world_knowledge`, `get_diary_entries`, `get_latest_diary_entry` | semantic memory recall (returns content, emotion, importance, tags, game_time, `isOwn`/`originName`), condition-scoped world knowledge, diaries |
| **Events and scene state (12)** | `get_recent_events`, `get_event_history_count`, `format_event`, `get_short_lived_events_count`, `get_active_short_lived_events`, `get_short_lived_events_by_type`, `get_short_lived_events_by_entity`, `track_entity_state`, `get_entity_state`, `set_entity_state`, `has_entity_state`, `is_state_recent` | event history, schema-based event rendering (`format_event(e, "verbose"\|"compact"\|"recent_events")`), TTL scene facts, and a small **state-change tracker** (`track_entity_state(uuid, key, value, windowSec)` returns `{previous, changed, isRecent, hasPrevious}`) |
| **Quests and vanilla dialogue (8)** | `is_quest_active`, `get_quest_stage`, `get_all_active_quests`, `get_quest_journal`, `get_quest_alias_actor`, `available_dialogue`, `get_intelengine_quests`, `get_script_property` | quest gating, journal text and objectives, the NPC's open vanilla topics (for "quests you could raise"), reading any Papyrus script property |
| **Game system, config, voice (17)** | `get_global_value`, `is_plugin_loaded`, `get_all_loaded_plugins`, `formid_to_uuid`, `prompt_file_exists`, `json_file_exists`, `read_json`, `papyrus_util`, `is_narration_enabled`, `is_action_enabled`, `is_caching_enabled`, `feature_enabled`, `get_narrator_uuid`, `get_speaker_selector_settings`, `get_actor_tts`, `is_audio_tags_enabled`, `get_allowed_audio_tags` | load-order checks, mod globals, JSON side files, feature flags and settings exposed *to templates*, TTS engine introspection for per-engine voice-tag rules |
| **Utility (1 + HTML)** | `to_string`; the HTML's String Utilities: `join`, `length`, `lower`, `contains`, `to_number`, `is_array` | `contains` does substring or element checks (e.g. matching furniture names) |
| **Rendering (3)** | `render_template`, `render_subcomponent`, `render_character_profile` | composition (§3.1) |

Also listed in the HTML: the Mood list `moodsList`/`moodDescriptions`, which decision templates use to enumerate options (`decisions/evaluate_mood.prompt`). `submodules/test_decorators/*` is a self-test harness that renders every decorator in a category for an actor. **INFERENCE:** it serves as the de-facto regression test for the decorator layer.

---

## 5. Character bios

### 5.1 The block schema

A bio file is ten `{% block %}` definitions and nothing else. A 1-line fragment of the format (from `characters/lydia_C94.prompt`, truncated):

```text
{% block interject_summary %}Lydia interjects when: dragons appear; the person she is sworn to protect is in danger; …{% endblock %}
```

Statistics over the 1,507 non-empty bios (my scan): every one fills every block. The median bio is about 570 words, with generic and unique bios about the same length.

| Block | Median words | Shown in modes | Audience | Content guidance (paraphrased from bios and `helpers/generate_profile`) |
|---|---|---|---|---|
| `summary` | 43 | full/stable, dialogue_target, **short_inline**, bio_summary | **public** | role and first impression. What a stranger would perceive |
| `interject_summary` | 31 | full/stable, **interject_inline** | selector | concrete topics or situations that make this person speak up unprompted |
| `background` | 82 | full/stable | private | history, origins, secrets. Where spoilers go |
| `personality` | 68 | full/stable | private | values, temperament, biases, typical behavior |
| `appearance` | 45 | full/stable, dialogue_target | **public** | visible physical traits, no equipment. Replaced by a vision description if one exists |
| `aspirations` | 40 | full/stable | private | 3–5 bulleted goals |
| `relationships` | 69 | full/stable | private | bulleted `Name (role): feeling + context` |
| `occupation` | 25 | full/stable | semi | one-line job and routine |
| `skills` | 44 | full/stable | private | bulleted practical abilities |
| `speech_style` | 59 | **speech_style** (rendered separately, last) | voice | cadence, register, accent, tonal shifts. Not catchphrases |

The skeleton also declares a `long_term_memories` block (`7000_memories_and_progression`) that no shipped bio fills. **INFERENCE:** it is an unused hook for generated or progression content.

### 5.2 How a bio becomes a profile

The `character_bio/*.prompt` submodules contain the matching empty `{% block x %}{% endblock %}` placeholders inside mode checks and headers. `0300_personality.prompt` writes the "## Personality & Psychology" header only for profile modes and then opens `{% block personality %}`. **INFERENCE:** `render_character_profile` treats the concatenated submodule folder as the parent and the actor's bio file as the child that overrides blocks, which is Inja/Jinja inheritance. The old layout files `components/character_bio_full.prompt` and `character_bio_dialogue_target.prompt` show the same idea in its earlier single-file form. The empty `character_bio_short_inline` and `_interject_inline` components suggest the per-mode layouts were folded into the submodules' mode checks.

This separation is clean. **Authors write only prose**. Engineers own headers, live state, perspective and ordering. Changing the prompt layout never touches 1,500 bio files, and a bio fix never risks the template logic. Zero bios contain `{{` or `{% if`.

### 5.3 Four bios compared (paraphrased)

- **Major NPC, Jarl Balgruuf (`balgruuf_the_greater_677.prompt`, ~790 words).** The summary gives only public facts: ruler, pragmatic, publicly neutral in the war, hostile to Thalmor meddling. The background holds the private material: secret Talos worship, an affair and its son, his Legion past with his housecarl. Relationships list seven named people with one clause of feeling each. Speech style describes authority, decades of court cadence, and how anger makes him "harder, not shorter". Note the deliberate anti-fragmentation wording.
- **Follower, Lydia (`lydia_C94.prompt`, ~630 words).** The player is never named. She serves "the person she serves" or "the Thane", so the bio stays valid for any player. Romance is described as a *conditional potential*, not a fact. Her personality includes a known in-game running joke (dry humor about carrying burdens), so the persona keeps the vanilla character's recognizable quirks.
- **Generic, `bandit_generic.prompt` (~980 words, the largest generic).** It lists five archetypes (opportunist, desperate, hot-head, weary veteran, cruel one) and tells the model to pick one per scene, stay consistent, and use the cruel one sparingly. It then sets constants for all of them. Self-preservation beats bravado, and yielding when the fight is clearly lost is normal for this tier. It notes that chiefs and elite bandits have their own bios with "higher resistance thresholds". Speech style shifts register with the archetype and turns to pleading when losing. One line sums up the intent: "A bandit pleading for their life is in character; a bandit dying with a defiant one-liner is not."
- **Generic guard, `imperial_guard_generic.prompt` (~550 words).** It is written as one specific person (posting, superior, local tensions) rather than an archetype list. It still serves every NPC whose display name maps to it. Its interject summary triggers on anti-Imperial talk and security topics, which suits a patrol NPC.
- **Non-human speaker, Paarthurnax (`paarthurnax_57D.prompt`).** The summary never says he is a dragon. It describes a revered hermit-teacher on a mountain, and the reveal stays in the private blocks. Speech style handles the non-human voice: native-language words with translations, rhetorical questions, deliberate pacing.

Only 2 of 302 generic bios (`bandit_generic`, `bandit_chief_generic`) use the explicit archetype technique. The rest describe a single representative individual.

### 5.4 Authoring conventions visible in the corpus and its history

1. **Spoiler containment.** PR #384 audited 519 bios. It moved forward-looking quest material (identity reveals, hidden allegiances) out of `summary` into `background`, because the summary surfaces "from first contact" in idle talk (https://github.com/MinLL/SkyrimNet-GamePlugin/pull/384). The Paarthurnax and Balgruuf summaries show the result.
2. **Anti-priming of speech styles.** The same PR names descriptors like "clipped", "terse", "blunt" and `Example: "…"` lines as dialogue priming that produced "period-stopped Tough-Guy One-Liner" output, and rewrote them as cadence and accent descriptions. Its stated reason is that bio-level voice instructions outweigh scene-level ones (PR text; the PR shows no test data). 93 bios now share a stock phrase about sentences keeping "their joints intact". No shipped `speech_style` block still contains `Example:`. The cleanup is **partial**, though. My heuristic scan finds 364 of 1,508 `speech_style` blocks that still use "clipped", "terse" or "blunt" without a negation ("measured rather than clipped" is not counted). That fits the PR's scope of 519 audited files. Anyone reusing the *pattern* should audit the whole corpus.
3. **Player-neutral references.** A Beta27 fix removed "Dragonborn" from about 50 bios where it only stood in for "the player". Characters whose story is about the Dragonborn keep it (`GH/changelog/beta27/player-not-assumed-dragonborn.md`).
4. **No template logic in bios.** Conditional state (quest progress, combat, follower status) is added by submodules, never by the bio.
5. **Interject summaries as trigger lists** ("X interjects when: A; B; C"), written as specific topics rather than generic sociability.

### 5.5 Selecting a bio for an actor

FACTS:

- **Unique bio key:** `{sanitized_english_name}_{FormID & 0xFFF as 3 upper-case hex}`, e.g. `lydia_C94`. The docstring of `GH/utilities/bios/partition_bios.py` states the engine builds it that way, and it describes the unique CSV's ID as the *reference* ID ("last 3 hex of refId"). 1,203 files follow the pattern, and two use a 5-digit suffix.
- **Generic bio key:** `{key}_generic`. Matching goes by the actor's *display name*. PR #573 says the name-to-bio map is "keyed only on the sanitized localized display name". As a result, Dawnguard's ambush vampires, shown as "Traveler", hand their vampire-assassin bio to any mod NPC called "Traveler". **Status:** the PR proposes an optional fourth CSV column, `matchScope` (`plugin` rows checked before plugin-agnostic ones), plus a neutral `traveler_generic.prompt`. As of the review the PR is **open, not merged** (https://github.com/MinLL/SkyrimNet-GamePlugin/pull/573). Neither change is in beta30-rc2. The shipped generic CSV has three columns, and no `traveler_generic.prompt` ships. The collision it describes is therefore current behavior.
- **Translation CSVs** (`LIB/prompts/translation/{unique,generic}/*.csv`) let a German or Russian game resolve the English-named bio, and they are "the only place a bio is tied to its plugin" (`partition_bios.py`). The two files are keyed differently:
  - `unique/00_SkyrimNet_unique.csv`: `pluginName, refId, sanitizedEnglishKey`, 2,228 rows. These are placed-reference IDs.
  - `generic/00_SkyrimNet_generic.csv`: `pluginName, formId, sanitizedEnglishKey`, 766 rows. These are base-record IDs, mapping onto the 302 generic bios. 202 rows come from `Skyrim.esm`, e.g. the base bandit record → `bandit` and the Whiterun guard base → `whiterun_guard`. The rest come from mods.
- **`_generic` is a matching mode, not a role label.** Many `_generic` files are single named beings: `alduin_generic`, `boethiah_generic`, `cassia_generic` (each bio describes one specific individual). **INFERENCE** on why: such characters appear as several records (e.g. Alduin across encounters, a Daedric Prince speaking through avatars), or come from mods whose reference IDs are unstable, so name-only matching is the robust choice. The suffix means "applies to any actor carrying this name". Conversely, the FormID suffix also gives individual bios to *same-named* soldiers: `redoran_guard_5B8` … `redoran_guard_5BF` are eight different unique bios, and `kolbjorn_guard_9C4`/`_B03` sit beside `kolbjorn_guard_generic`. So a squad of identically named NPCs can be differentiated by reference without any template logic.
- **Fallbacks and overrides:** `characters/default.prompt` has three blank blocks. `player_special.prompt` is an empty ten-block template for the player. Generated bios live in `characters/dynamic/**` or `*.dynamic.prompt`, and per-playthrough bios in `prompts/_saves/**` (`MIGRATING_TO_BETA25.md`). FEATURES.md: "NPCs without a bio get one generated from their game data."
- **Identity persistence:** each actor gets a UUID that is "a hash of display name and masked FormID" (comment in `DATA/sql/migrations/0021_identity_aliases.sql`). It is stored with `form_id`, `actor_name` and its `bio_template_name` (`0005_uuid_mappings.sql`). `0020_uuid_mappings_modfile.sql` later added `mod_file` and `local_form_id` for load-order-independent identity. Renames fork the UUID, and an alias table stitches the forks back together. GitHub issue #597 shows the failure mode: a quote character in a name put a different string in the DB than in the filename, and the wrong bio was used (https://github.com/MinLL/SkyrimNet-GamePlugin/issues/597).
- **Succession:** `LIB/identity/*.yaml` with `kind: succession` declares that two NPC records are one person over time, e.g. a character whose record is swapped when a quest advances, in `from: npc:skyrim.esm:0x… / to: npc:skyrim.esm:0x…` form (`vanilla_ulfric_stormcloak.yaml`, `vanilla_sinding.yaml`, `CONTENT_ROOTS.md`). All 12 shipped identity files are successions (Astrid, Cicero ×2, Kodlak, Ulfric, Galmar…). `kind: link` (the documented default kind, `CONTENT_ROOTS.md`) covers the richer "one being, two bodies / possession" relationships rendered by `7050_identity_links` (dominance `entity_led | host_led | contested | blended`). No shipped file uses it.

**FACT, file-layer precedence** (`DEV/docs/modding/MIGRATING_TO_BETA25.md`, "Folder layout"): highest first come per-playthrough files (`saves/`), then the player's dashboard edits (`overlay/`), then installed and external plugins in the player's chosen order, then SkyrimNet's shipped defaults. A file at the same path in a higher layer replaces the lower one. Generated bios (`characters/dynamic/**`, `*.dynamic.prompt`) are written to the active playthrough's layer (`PublicAPI.h`, §6.2).

**INFERENCE, key resolution order within those layers:** dynamic (generated or updated) bio, then authored unique bio by `name_XXX`, then name-matched generic (plus plugin-scoped generic if PR #573 lands), then generated-on-demand, then `default`. This order is not documented. Issue #795 is a feature request for several *named* bio versions per character in the overlay (disguise, memory loss). It does not report lookup ambiguity, but it shows that hand-edited overlay bios are the current way to change a persona situationally (https://github.com/MinLL/SkyrimNet-GamePlugin/issues/795).

### 5.6 Generic versus unique, summarized

| Aspect | Unique NPC | Generic NPC (guard, bandit) | Creature |
|---|---|---|---|
| Bio | own file `name_XXX` | shared `role_generic`, matched by display name | almost none ship (`hagraven_generic`, `falmer_servant_generic`). Whether `default` or an auto-generated bio then applies is UNVERIFIED |
| Identity and memories | per-actor UUID, own memory store | still a per-actor UUID (name + FormID hash), so each spawned bandit remembers separately (**INFERENCE**) | memory can be blocked by filter |
| Voice | authored speech_style | shared speech_style; archetypes for variety | race line in `9990_speech_style` ("cannot use human words, only …") |
| Speaks at all | yes | yes | filters may forbid (`animals_dont_speak`) |
| Prompt cost | full profile | same machinery, same size | small |

The notable point is that SkyrimNet does **not** have a light "bark mode" for generics. A bandit gets the full dialogue prompt and the shorter combat length rule (§8).

**Gendered generic bios.** A shared bio serves every actor with that name, but many generic bios hard-code one sex. My pronoun-frequency scan of the 302 `_generic` files classes 143 as mainly he/his/him, 61 as mainly she/her and 97 as they/their (1 has no pronouns). `whiterun_guard_generic`, for example, describes "his" heritage. The skeleton supplies gender and pronouns from `decnpc()` in the header (`0010_header`). **INFERENCE:** a female actor matched to a male-written generic bio gets contradictory cues. The bandit bios avoid this by using "they" throughout. ER lesson: write archetype cards gender-neutral and let code supply pronouns.

---

## 6. Generated and evolving bios

### 6.1 Generating a missing bio (`helpers/generate_profile.prompt`)

Inputs: name, race, sex, level, location, voice type, faction memberships with ranks, recent events (`event_history_verbose`), optional user notes, and, when available, the NPC's **actual vanilla dialogue tree** grouped by category (quest, scene, player prompt and the NPC's responses with emotion). The prompt calls that dialogue "authoritative canonical dialogue". The model must return JSON with the ten bio fields and a one-sentence description of each: summary perceptible by others, appearance without attire, speech style as cadence instructions without pet names, interject summary as specific triggers. Its guidelines: use lore confidently, don't invent lore, avoid hedging words such as "seems to", and describe knowledge rather than instruct the NPC what to say.

### 6.2 Evolving a bio over a playthrough (`dynamic_bio_update.prompt`)

Triggered by hotkey, trigger or API (`UpdateActorDynamicBio`, `TriggerGenerateDiaryBio` in `SkyrimNetApi.psc`). Inputs: the original bio, the current dynamic bio, recent events, the most significant memories, the latest diary entry, factions, an `updatableBlocks` list and optional caller instructions. Its design, paraphrased:

- **Conservatism first.** Default to preserve, and expect most events to be routine. There is a four-level threshold (routine → none, notable → minimal, significant → moderate, transformative → substantial).
- **Block tiers.** Core identity (background, personality, appearance, summary) rarely changes. Development blocks (aspirations, relationships, occupation, skills) change moderately. Expression blocks (speech style, interject summary) need sustained patterns.
- **Length management.** Each block has a target range (background 150–250 words, summary 75–100, speech style 50–100…) and pruning strategies (drop completed goals, merge redundancy, keep the 3–5 key relationships).
- **Stranger Test** for the public blocks: "Would someone meeting this character for the first time in a tavern, with no introduction, either observe this fact directly or already know it from public reputation?" Private material is routed to the private blocks, with a leaked-vs-clean example.
- **Output:** JSON `{updated_content, changes_summary}`, where `updated_content` is the full bio in block syntax. `character_profile_update.prompt` is the dashboard's user-directed edit with the same contract.

**FACT** (`DEV/CppAPI/PublicAPI.h`, `PublicRequestProfileUpdate`, API v14+): the update runs asynchronously on SkyrimNet's worker pool and never blocks the game thread. The result is persisted to "the active playthrough's content layer (`prompts/characters/dynamic/<name>.dynamic.prompt`)". If the actor has no bio file yet, an initial profile is generated first. Callers can pass free-text `additionalInstructions` and an `updatableBlocksCSV` restricting which of the ten blocks may change. Without that list, the user's configured `updatable_blocks` default applies. Only one update per actor may be in flight. The Papyrus hotkey equivalent `TriggerGenerateDiaryBio()` offers four scopes: player only, nearby actors, pinned actors, crosshair target (`SkyrimNetApi.psc`). **INFERENCE:** the dynamic file shadows the authored bio for that save only, because it lives in the per-playthrough layer, which has the highest priority (§5.5). The authored file is re-supplied every time as "Original Bio Content", so updates stay anchored to it and don't drift through repeated rewrites.

---

## 7. Filters (`LIB/filters/*.yaml`)

The schema is in `DEV/docs/modding/CONTENT_ROOTS.md`. Four kinds:

| `kind` | Fields | Purpose | Shipped examples |
|---|---|---|---|
| `actor` | `FactionWhitelist/Blacklist`, `RaceWhitelist/Blacklist`, `GenderWhitelist/Blacklist` (editor IDs), `name`, `description` | who may speak or be selected | `animals_dont_speak` (cat, dog, wolf races), `mannequins_dont_speak`, `paarthurnax_speaks` (whitelists one faction so a dragon may talk), `skyrimnet_filter_factions` (always-allow and always-block factions that scripts can add actors to) |
| `memory` | same lists | who may form memories | `animals_no_memories`, `mannequins_no_memories` |
| `dialogue_rule` | `id`, regex `pattern`, `replacement`, `priority` | rewrite or drop vanilla subtitle lines before they enter the event stream | `combat_grunts` (drops "Rrargh!"-style grunt subtitles), `heavy_breathing`, `invisible_continues` |
| `tts_rule` | same, plus `enabled` | rewrite text before speech synthesis | `romanization_in_parentheses` (off by default) |

Contributions *union* across plugins. A user disables one by toggling its file, so base keeps "one purpose per file". Lower `priority` runs first (default 100, ties broken by path). Rule `pattern`s are regexes of at most 1,024 bytes. A whitelist contribution "widens what may speak or fire" (`CONTENT_ROOTS.md`). `paarthurnax_speaks` describes itself as letting him talk "even when only speech-capable races may". **INFERENCE:** a user setting restricts speech to speech-capable races, and actor whitelists are the per-actor escape hatch. Of the 10 shipped filter files, 6 are list contributions and 4 are text rules. The canonical shape:

```yaml
kind: actor
name: Animals don't speak
description: Cats, dogs and wolves never talk.
RaceBlacklist:
  - DogRace
```

---

## 8. What the model is told: guidelines, grounding, style

The instructions are spread across sections so each request kind gets only what applies. Paraphrased by theme:

| Theme | Rule (paraphrase) | Source |
|---|---|---|
| Identity | Opening line: "You are <name>, a <gender> <race> in Skyrim." Then: embody the character fully, drawing on background, personality, goals and relationships | `dialogue_response`, `guidelines/0500` |
| Perspective | Refer to actors by name and pronouns from `decnpc()`, never a generic "you", in *section text*. The second person is kept for the instruction voice | `WORKFLOW_PROMPTS.md` §4.1 |
| Length | Dialogue 1–3 sentences, typically 8–40 words, max 60, no paragraphs. **Combat: one sentence, ≤14 words.** Thoughts 8–30 words, first sentence ≤10 | `system_head/0020_format_rules` |
| Freshness | Respond with your own thoughts, not echoes; each response must advance the conversation; vary approach | `0500_response_format` |
| Grounding (thoughts) | Use only profile, listed memories, scene and recent events. Don't invent facts, retell, or drift into a narrator's voice. Start mid-stride with no recap | `0020` |
| Addressing | Answer what was said *to you*. Lines between others were overheard and need no answer. Narration that just happened takes priority | `dialogue_response` tail (note 02 §4.3) |
| Combat | Urgent and brief, about the fight. Arguments wait. When knocked down: short, pained, about your state or a plea | `dialogue_response`, `guidelines/0500` |
| Self-knowledge as words | HP, magicka and stamina are given as phrases. Relative level becomes "your instincts tell you X is roughly your equal…". The comment explains: "Never expose literal level numbers in the prompt: weaker LLMs will parrot them in dialogue." | `0200_combat_status`, `character_bio/0010_header` |
| Quests | Quests don't expire. Don't create urgency, invent deadlines or keep reminding. Vanilla quest branches you can raise come as an example dialogue tree to express "in your own words" | `0610_party_quests`, `0740_quest_dialogue` |
| No meta | No headers or labels. Never emit `<thinking>` or any scratchpad tag, since they would be spoken. Transformers: output only the line, with no commentary, alternatives or quoting. On "continue": don't mention being asked to speak | `0500`, `transformers/*`, `0810` |
| Language | Optional spoken language with established localized lore terms, plus an optional `<translation>` block after every sentence for subtitles | `0760_spoken_language` |
| Recency placement | In the `system_head` layout the NPC's speech profile is the last block of the system message, so its voice is "the model's freshest instruction" (`0400` comment). The cache-ordered `dialogue_response` changes this. The speech profile closes the *first* cached block (identity line → stable profile → speech style). Then come three more cached blocks: guidelines, response format plus voice tags, and the action list. `system_head` is rendered in the user message with `speech_cached: true`, so the voice is not repeated there. Freshness is restored by repeating the task, the "said since you last spoke" recap, the combat line and the action names at the very end of the user message | `system_head/0400`, `dialogue_response` lines 8–30 |

### 8.1 Output contract (summary; full table in note 02 §5)

Plain spoken text. Optional `*narration*` (only the speaker's own body language, in about one of four replies). Optional per-TTS `[voice tags]` (sparingly). Optional `<internal_thought>` *after* the speech. At most one trailing `ACTION: Name PARAMS: {json}` line. A `<translation>` block per sentence if subtitles are translated. Every element is switchable by settings, and the matching instruction is rendered only when the feature is on (`is_narration_enabled()`, `is_audio_tags_enabled()`, `embed_actions_in_dialogue`…). The instructions therefore always describe exactly the grammar the parser expects.

---

## 9. Supporting prompt kinds worth knowing

- **Transformers** rewrite a *given* line rather than invent one. `native_dialogue_transformer` reshapes a vanilla line for this NPC "similar in length to the original" and returns it unchanged if it already fits. `universal_translator` applies a per-NPC, faction, race or global **speech pattern** (configured under the `translator/` content root) and/or a spoken language. Its cached system block is byte-identical to `dialogue_response`'s, so both share one cache entry.
- **Translation transformers.** `subtitle_translation` and `player_option_translation` translate a given line or a list of dialogue options. Their contract is strict: "output ONLY the line(s)", with no quotes, notes or alternatives, and one output line per input line. `[label]` menu entries are translated plainly or returned unchanged, never voiced (`transformers/player_option_translation.prompt`; `GH/changelog.d/player-dialogue-translation-prompt.md`). The `translation/` folder is *not* templates. It holds the bio-key CSVs (§5.5).
- **Helpers.** `helpers/generate_search_query` turns location, current emotion, nearby actors, conversation partners and the compact event history into a semantic-plus-keyword query for memory retrieval. `helpers/evaluate_mood` is a chat-style mood classifier over `moodsList`/`moodDescriptions` that drives "voice modulation and facial expressions". **INFERENCE:** it is the chat fallback for the typed `decisions/evaluate_mood` template, mirroring how `target_selectors/*` back up `decisions/*`.
- **Components worth knowing.** `components/memory_access` renders each recalled memory as a numbered record (summary, details, emotion, tags, location, importance out of 10). `components/context/component_npc_state_summary` and `component_recent_events` fill the `npcStateSummary` and `recentEvents` blocks of the scene context. `components/dialogue_action` is the body of the `TriggerDialogueLine` action (play a vanilla dialogue branch), active only when the `quest_actions` feature is on. `dialogue_response` renders it again among its last lines (`at_end`).
- **Thoughts** (`npc_thoughts`) use the same profile in `thoughts` mode. The output is 1–2 silent sentences in the NPC's own register, stored to memory only.
- **Warmup** (`warmup.prompt`) renders the profiles of up to 8 nearby NPCs, the scene and history while the player is still speaking. The output is discarded, and the point is to fill decorator and render caches (latency).

---

## 10. Lessons encoded in comments and changelogs

The template comments double as an engineering log. The recurring failure modes and fixes:

1. **Recency beats position.** Under a cache-friendly layout the newest line sat "thousands of tokens back" and smaller models answered an older one. Fix: repeat the task and the last ≤5 lines at the very end (`dialogue_response` comment). The same reasoning puts the speech style last and repeats action names at the end ("with nothing about actions near the end models stopped writing the ACTION line").
2. **Numbers leak.** Raw stats and levels get parroted, so they are always bucketed into words.
3. **Bio voice dominates scene guidance.** Priming words in `speech_style` override prompt-level length and style rules (PR #384).
4. **Public/private discipline.** Whatever sits in summary and appearance reaches *other* NPCs' prompts and idle chatter, so secrets go in background (PR #384, Stranger Test).
5. **Identity collisions are content bugs.** Name-keyed generic matching mis-assigns bios across mods (PR #573, fix still open), and unsanitized names break lookups (issue #597, open). Stable numeric keys matter.
6. **Plugins must respect the layout.** An override of `0500_response_format` without the `stable_split` check renders it twice (`GH/changelog.d/prompts-cache-layout.md`).
7. **Don't assume the player's role** (the Dragonborn fix).

---

## 11. How it maps onto Skyrim engine features

| SkyrimNet prompt feature | Skyrim mechanism it relies on | Elden Ring counterpart (see `ER/01-engine-internals.md`) |
|---|---|---|
| Bio key `name_XXX`, UUID = hash(name, masked FormID) | Actor base and reference FormIDs, load-order-relative indices, localized display names | `NpcParam` row ID and MSB `entityID`. `NpcParam.nameId` → NpcName FMG is language-independent, so **no name matching or translation CSV is needed** |
| Generic bios matched by display name | many NPCs sharing a display name ("Bandit", "Whiterun Guard") | many enemies sharing a `nameId` and character model (`c####`) |
| `identity/*.yaml` succession | quests swapping one actor record for another | the same person appearing as several `NpcParam`/entity rows across a questline (friendly, hostile invader, moved location). **INFERENCE:** very common in ER |
| Race filters, creature speech chain | Race records (`DogRace`, `DraugrRace`) | character model ID → creature type name list; `NpcParam.npcType` |
| Faction checks, follower status, crime gold | Faction records and ranks | no faction system. `NpcParam.teamType` (enemy, friendly, hostile NPC…) and event flags carry the same information |
| Quest stage and journal gating | Quest stages, journal objectives, aliases | **event flags** (quest progress). No player quest journal comparable to Skyrim's (**INFERENCE**, to verify) |
| HP/magicka/stamina buckets | Actor values | HP / FP / Stamina on `ChrIns` |
| `has_magic_effect` state checks | Magic effects, keywords, perks | SpEffect IDs (status effects such as poison or rot) |
| Furniture and activity lines | Furniture and AI packages | little equivalent, mostly idle animations (low value for v1) |
| Time, weather, season, indoors | calendar, climate, interior cells | time of day and weather exist. No seasons. Interior vs open field via map/block IDs |
| Vanilla dialogue tree as bio source | DIAL/INFO records | TalkMsg FMG lines reached via talk ESD scripts |
| Papyrus custom decorators | Papyrus VM | no accessible script VM. Decorators must be native code |

---

## 12. Transferable patterns for an Elden Ring framework

### 12.1 Copy as ideas (high value, low cost)

1. **Prompt files as layered, hot-reloadable data with numbered sections.** Use one template per request kind (named dialogue, generic bark, memory write) and one folder per composable section. Users can then retune tone without a rebuild. A C++ DLL can use Inja (MIT, header-only). A Rust DLL can use minijinja. **Keep the `[ system ] / [ user ] / [ cache ]` marker idea.** It is simple, readable, and maps directly to OpenAI-style message arrays with cache breakpoints.
2. **Persona blocks as a schema, with layout owned by engineers.** Personas are pure prose in named fields, and section headers and state are rendered around them. Use our *own* field set, as structured data (YAML/TOML) rather than Jinja blocks:
   - `public.summary`, `public.appearance`: what any stranger perceives. Apply the Stranger Test.
   - `private.background`, `private.personality`, `private.goals`, `private.relationships`: interior and secrets.
   - `voice.speech_style`: cadence, diction and register described as behavior.
   - `knowledge`: what this person knows about places and figures, kept short and factual.
   - `gates`: block additions unlocked by **event flags** (e.g. a revelation added to `background` only after flag N). This is how we contain spoilers in a game whose NPC questlines are built on reveals.
   - `interject`: keep the field for v2 multi-NPC scenes, but leave it unused in v1.
3. **Render modes as levels of detail.** v1 needs four: `self_stable` (cached persona), `self_dynamic` (state, memories), `observed` (public summary, appearance and visible gear, used when describing the player or another NPC) and `brief` (one line, for lists of nearby people).
4. **Public/private split with a Stranger Test** in both authored personas and any generated updates.
5. **Words, not numbers.** HP, FP and stamina ratios become phrases. Threat comparisons become an "instinct" sentence. Event times become "a while ago". Never put raw IDs, levels or percentages in prompts.
6. **Recency placement.** Put the voice spec immediately after the persona, at the end of the cached persona block. It does not have to be the very last system text (SkyrimNet's cache layout puts guidelines and format after it, §8). End the user message with the task, the newest line(s) said *to* the NPC, and the combat or urgency line. That uncached tail is where freshness really comes from.
   - **One byte-identical persona block shared across request kinds.** `dialogue_response`, `player_dialogue` and `universal_translator` all open their system message with the same cached block: identity line, `stable` profile, speech style. Different request kinds for the same NPC therefore reuse one provider cache entry (`transformers/universal_translator.prompt` comment). EldenNet's named-NPC dialogue and any later "rephrase" or "inner thought" calls should share their persona prefix the same way.
7. **Generic archetype cards with in-card variety.** The bandit pattern of several archetypes plus shared constants (self-preservation, how speech changes when losing) is exactly what ER's repeated enemy squads need. **Improvement:** pick the variant *in code*, deterministically from the entity handle, rather than asking the model to "pick one and stay consistent". Each soldier then keeps the same temperament across barks, and the cached prompt prefix stays stable per variant. Write cards **gender-neutral**, as the bandit bios are. SkyrimNet's male-written guard generics show the failure mode (§5.6). Code injects the instance's pronouns. Reserve per-instance overrides for the rare named member of a squad, as SkyrimNet does with `redoran_guard_5B8…5BF` (§5.5).
8. **Data-driven speech capability by creature type** (`can speak` / `sounds only: …` / `silent`), replacing SkyrimNet's 50-branch template chain with a YAML table keyed by character model or `NpcParam` row. Add **actor and memory filters** with the same whitelist/blacklist shape, keyed by model ID, `teamType` and `npcType` instead of race and faction editor IDs.
9. **Canonical vanilla lines as authoring input.** `generate_profile`'s use of the NPC's own dialogue tree maps to an *offline* EldenNet pipeline. Dump each named NPC's TalkMsg lines (per talk ESD) and use them to draft personas and voice notes that a human then rewrites in original words. **INFERENCE:** this gives the best voice fidelity for FromSoft's distinctive NPCs.
10. **Succession mapping:** one persona and one memory store for all the `NpcParam`/entity rows that are the same person.
11. **A decorator self-test page** that renders every snapshot field for the targeted NPC, plus a `validate` command for templates.

### 12.2 Simplify for v1

- **Logic in code, text in templates.** SkyrimNet's Inja does cache-anchor math, event filtering, time bucketing and race dispatch. For EldenNet, compute derived facts natively (`hp_phrase`, `time_since_phrase`, `lines_to_you[]`, `threat_phrase`) and pass them as variables. Templates become mostly prose with `if` guards.
- **Snapshot instead of pull decorators.** SkyrimNet's decorators read game state on demand during rendering. In ER, gather a perception snapshot struct (speaker, target, nearby, place, time, combat events) **on the game thread** at trigger time, then render and call the LLM off-thread. Note 01 records CTDs from worker-thread game reads in SkyrimNet. **INFERENCE:** a snapshot is the safer default in an engine we understand less.
- **A much smaller decorator surface:** speaker, target and player (name, pronouns from model/sex, HP/FP/stamina phrases, in-combat, hostile-to, equipped items' FMG names), place name, time of day and weather, nearby characters (name, creature type, distance band, dead/alive), recent combat events, relevant memories, persona fields, and flag-gated knowledge. Roughly 15 fields against SkyrimNet's 130.
- **Two prompt kinds only:**
  - *Named NPC dialogue:* system (cached: identity line, persona stable blocks, world-tone section, guidelines, output contract, voice last) → history as alternating user/assistant → user (dynamic state, observed player, memories, task, newest player line). Use SkyrimNet's dialogue lengths (1–3 sentences) with a lower cap.
  - *Generic bark:* system (cached *per archetype variant*: "You are a <creature type> in <region>", archetype card, tone primer, contract "one line, ≤10 words, or nothing") → user (trigger kind: spotted / struck / ally fell / low HP / player down / fleeing, plus 2–3 state phrases, plus the last bark this squad said, to avoid repeats). No history and no memories (per the v1 spec, generics know only what they are, where they are, and the fight). **INFERENCE:** because combat barks must land within about a second, consider pre-generating a small pool per (variant, trigger) at area load and spending live calls only on unusual events.
- **Output contract: speech only.** No narration asterisks, actions, voice tags or internal thoughts. Keep the "never emit reasoning tags" rule and a post-filter that strips anything bracketed. Generic barks may return an empty line, treated as silence.
- **Language:** match the game's text language. Keep SkyrimNet's instruction to use the established localized names for lore terms. Skip the dual-language subtitle machinery.

### 12.3 Drop for v1 (and why)

Embedded actions and categories (v1 is dialogue-only). OmniSight vision (cost and latency, and no screenshot pipeline). Telepathy and identity-link possession (niche). Per-TTS audio-tag catalogs and carry tags (TTS is out of scope or later). GameMaster and continuous scenes, interject summaries and speaker selection (no multi-NPC conversations yet). Quest-topic injection (no ESD integration yet). Dynamic bio rewriting (memories cover v1 continuity, and rewriting a persona is risky without a dashboard to review diffs). Decision templates (useful later for "should this NPC react?" gating).

### 12.4 Tone: making the structure carry the FromSoft voice (design notes, INFERENCE)

- SkyrimNet leaves the world `# Setting` section empty for users to fill (`system_head/0010_setting`) and hard-codes "in Skyrim" in the identity line. EldenNet needs a **shipped, central world-tone section**. It should cover elevated, archaic register, allusion over explanation, courtesy laced with menace, and NPCs who know only part of the truth. It must be written in original words, without quoting game text.
- **Expect PR #384's lesson to apply here.** Adjectives like "cryptic", "terse" or "riddling" will probably produce one-line riddles and pseudo-lore. Describe *cadence and diction* (sentence shape, honorifics, how the character deflects questions they won't answer) rather than effect words. Also add a grounding rule: allude only to what the persona's knowledge contains, and when the character doesn't know, deflect in character instead of inventing lore. Cryptic tone makes hallucinated lore harder to spot, so grounding matters more in ER than in Skyrim.
- **Don't assume the player's destiny** (the Dragonborn lesson). Address the player by the generic in-world term, and let flag-gated knowledge decide when an NPC treats them as more.
- Hostile generics need **self-consistent fear and zeal** (the bandit constants). Some ER enemy types fight to the death by nature, others are mercenaries. Each archetype card should state when, and whether, the creature yields, flees or taunts.

---

## 13. Open questions / uncertainties

1. **Exact bio key resolution order** (dynamic, unique, name-matched generic, generated, default) is not documented. Only the *file-layer* priority is (saves > overlay > plugins > base, §5.5). The key order in §5.5 is an inference. It is irrelevant to the ER design but matters if we quote SkyrimNet behavior. Also open: whether PR #573's plugin-scoped generic matching will land.
2. **How `render_character_profile` merges bio blocks into the submodule folder.** Is it true Inja inheritance with a synthesized parent, or a custom block substitution? The observable effect is clear, the mechanism is not.
3. **Decorator count.** The HTML documents 40 functions and the templates use 130 names. The authoritative list is only available at runtime (dashboard or MCP `get_decorators`), which we did not run.
4. **Do generic NPCs get separate memory stores per instance?** INFERENCE yes, since the UUID hashes the FormID, but this is unverified. It matters for whether ER generics should have *any* persistence (the v1 spec says no).
5. **ER identity keys:** is `NpcParam` row ID or MSB `entityID` the better persona key for named NPCs whose rows change across questlines? This needs an offline dump of talkID/npcParamID/entityID per map (`ER/01-engine-internals.md` §7).
6. **Using TalkMsg text as runtime context** (e.g. a few canonical lines as voice anchors) versus only as offline authoring input. Reading the user's own game files at runtime avoids shipping FromSoft text, but the effect on quality and on spoiler leakage is untested.
7. **Bark latency budget.** Can live LLM barks keep up with ER's combat pace, or is a per-area pre-generated pool required? This needs measurement with the chosen model slot.
8. **Elden Ring has no quest journal to read (to verify)**, so flag-to-meaning tables for named NPC questlines must be authored by us, which is significant content work for v1.

### Sources (web)

- PR #384, bio spoiler containment and anti-priming audit (519 files; merged 2026-05-03): https://github.com/MinLL/SkyrimNet-GamePlugin/pull/384
- PR #573, plugin-scoped generic NPC bio matching (**open, not merged** when reviewed; not in beta30-rc2): https://github.com/MinLL/SkyrimNet-GamePlugin/pull/573
- Issue #597, bio_template_name / filename mismatch for names with quote characters (open): https://github.com/MinLL/SkyrimNet-GamePlugin/issues/597
- Issue #795, feature request: several named bio versions per character in the overlay (open, enhancement): https://github.com/MinLL/SkyrimNet-GamePlugin/issues/795

---

## Review notes

A completeness, accuracy and license-compliance review was run on 2026-11-06 against the beta30-rc2 materials, the devkit and the public GitHub pages.

**Spot-checked and confirmed:**
- Inventory counts: 17 top-level prompts, 58 submodule files, 1,509 bios (1,203 `_XXX`, 302 `_generic`, `default`, `player_special`, 2 five-digit suffixes), and the component, decision, transformer and helper counts.
- Every one of the 1,507 non-empty bios fills all ten blocks, and none contains `{{` or `{% if`. Block medians match. The median raw bio is about 568 words.
- The decorator-name scan: 128 live names plus 2 commented-out `capture_*` = 130.
- Length rules in `system_head/0020_format_rules`.
- The level-number comment, which is in `0200_combat_status`.
- The `dynamic_bio_update` details: 95% rule, Stranger Test, block tiers, per-block word ranges, the JSON contract.
- `bandit_generic`: archetypes, constants and closing line. Only 2 generic bios use archetypes.
- The "joints intact" phrase in 93 bios.
- The filter schema and the shipped filter files.
- The identity YAML format.
- The decision-template markers.
- The PR #384 content (merged).
- Issue #597.

**Corrected:**
- **PR #573** had been described as if its fix shipped. It is **open/unmerged**. beta30-rc2's generic CSV has no `matchScope` column and ships no `traveler_generic.prompt` (§5.5, §10, Sources).
- **Issue #795** had been cited as evidence of lookup ambiguity. It is a feature request for multiple named bio versions in the overlay (§5.5, Sources).
- **Voice-last placement.** Speech style is last only in the `system_head` layout. In the cache-ordered `dialogue_response` it closes the first cached block, and guidelines, format, voice tags and actions follow (§8, §12.1 item 6, TL;DR).
- **decorators.html counts.** Of the 40 "decorators", 8 are variables (32 callable, 6 of them string utilities). Only 16 variable entries are listed, and "General" holds dashboard page variables. The page is a rendered copy of `prompts/documentation/*.prompt` (§4, TL;DR).
- **Usage lists.** The `{% raw %}` and `[ raw ]` usage lists were fixed (§2.1, §2.2).
- **Source attribution.** The UUID-hash quote now cites `0021_identity_aliases.sql` precisely, and `0020` (mod_file/local_form_id) is added.
- **Dynamic-bio persistence** was upgraded from INFERENCE to FACT, using `PublicAPI.h` `PublicRequestProfileUpdate` (§6.2).

**Added:**
- File-layer precedence from `MIGRATING_TO_BETA25.md`.
- The two CSV schemas: unique by refId, 2,228 rows; generic by base formId, 766 rows.
- `_generic` as a name-matching mode that is also used for named beings.
- Per-reference unique bios for same-named guards.
- Gendered pronouns in generic bios (143/61/97 split) and the ER lesson.
- Partial anti-priming coverage (364 of 1,508 speech styles still use clipped/terse/blunt).
- Documented-but-unused `include`/`super()`.
- Doc/shipped naming drift for the `target` and `dialogue_target` modes.
- `transform` and `book` length rules.
- C++ decorator thread-safety and reuse in action eligibility rules, plus `PublicGetBioTemplateName`.
- Filter rule details.
- The `speech_style_creature` nested block.
- Helper, translation-transformer and component notes.
- The shared byte-identical persona cache prefix across prompt kinds (§12.1, sub-point of item 6).
- Gender-neutral archetype cards (§12.1 item 7).

**License check:** no content in this note comes from inspecting compiled files (DLL, .pex, bundled JS). Facts about internals come from shipped templates, YAML, SQL comments, the MIT `PublicAPI.h`/`SkyrimNetApi.psc`, the public repo's Python docstring, and GitHub pages. Every quote from SkyrimNet prompts or bios is one line or a sentence fragment, well under the 15-line limit. The only multi-line excerpt is a 5-line filter YAML (config, not a prompt). No template or bio is reproduced.

**UNVERIFIED:**
- The bio key resolution order within layers (§5.5).
- Whether creatures with no bio get `default` or an auto-generated bio.
- Whether generic NPC instances keep separate memory stores (§13 item 4).
- That decorators run on worker threads (inferred from "Must be thread-safe").
- That a user setting restricts speech to speech-capable races (inferred from the `paarthurnax_speaks` description).
- The pronoun and priming counts, which come from heuristic regex scans and may be off by a few percent.
- The PR #384 claim that bio-level voice outweighs scene-level rules, which is the PR author's assertion without published test data.
