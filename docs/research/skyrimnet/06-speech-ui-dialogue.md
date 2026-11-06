# SkyrimNet research 06: speech, UI and dialogue presentation

**Scope:** how SkyrimNet presents AI dialogue to the player and takes the player's input. Covers text-to-speech (TTS) and the streaming sentence pipeline, audio playback inside Skyrim, lip-sync, voice assignment and voice effects, speech-to-text (STT) and push-to-talk, subtitles, the chat overlay, UI hosts (PrismaUI, Scaleform, Magelight), the web dashboard, hotkeys and the wheel menu, and how SkyrimNet lives alongside Skyrim's own dialogue (voicing player picks, unvoiced NPC lines, Dialogue Actions, the dialogue-choice hotkey, filters).

**Method and licence note:** this report uses only documentation, human-readable shipped content (YAML, Papyrus `.psc`, Spriggit ESP source, readable chat JS and the PrismaUI view HTML/CSS, which were read only to see UI features), third-party notices, and public GitHub issue and PR text. No compiled file was inspected. Some public issues quote SkyrimNet log lines and internal class names written by users. Where this report relies on those, it cites the issue, and everything built on them is marked **INFERENCE**. Only lines that reporters present as `SkyrimNet.log` output are used. Some issue authors also list format strings they say they found in `SkyrimNet.dll` (for example in #656 and #470); those are not used here. Prompt text is paraphrased. The few quotes are short and are there only to explain a design.

**Path roots used below**

| Abbrev | Root |
|---|---|
| `REL/` | SkyrimNet beta30-rc2 release archive (root) |
| `DATA/` | `REL/SKSE/Plugins/SkyrimNet/` |
| `LIB/` | `DATA/library/skyrimnet.base/` |
| `SRC/` | `REL/Source/Scripts/` |
| `PUI/` | `REL/PrismaUI/views/SkyrimNet/` |
| `DEV/` | SkyrimNet beta30-rc2 developer kit (root) |
| `GH/` | clone of github.com/MinLL/SkyrimNet-GamePlugin (root) |
| `ESP/` | `GH/spriggit/SkyrimNet/` |
| `#NNN` | Public issue or PR at `https://github.com/MinLL/SkyrimNet-GamePlugin/issues/NNN` |

---

## TL;DR

- **The LLM writes one text stream with marked-up parts.** Plain text is spoken. `*Asterisks*` mark optional narration. `<internal_thought>` blocks come after the spoken line and are never voiced. `[square-bracket]` tags are voice directions, used only for engines that accept them. A streaming splitter cuts the reply into sentences and sends each to TTS while the model is still writing (`DEV/docs/FEATURES.md`, `LIB/prompts/submodules/user_final_instructions/0500_response_format.prompt`, `0650_audio_tags.prompt`, #318).
- **There are many TTS engines, chosen per actor.** Piper (CPU, the default), PocketTTS, XTTS, Zonos, Chatterbox, Higgs, OpenAI-compatible servers, ElevenLabs, Inworld, Fateless, and Kokoro for the narrator only. Voices are keyed by Skyrim's vanilla voice type (`femalecommoner`, `maleyoungeager`, ...). Cloning engines get reference audio taken from the game's own `.fuz` files. Per-NPC custom recordings and creature samples override this (#775, #829, `REL/Sound/Voice/SkyrimNet.esp/creature_*`).
- **Audio does not go through vanilla dialogue records.** The ESP has **no** Topic or TopicInfo (DIAL/INFO) records, only three sound categories (Voice, Bards, Virtual Speakers), a 2D "dry" dialogue output model, and a `SilentVoice` voice type (`ESP/SoundCategories`, `ESP/SoundOutputModels`, `ESP/VoiceTypes`). Public logs show an "engine audio" path (`audio.useEngineAudio: true`, the default). It registers in-memory audio as a virtual resource and plays it through the game's own sound handles, attached to the speaker's 3D node (#831, #656). AI bard songs and bodyless virtual NPCs use the same engine player (#349, #801). What `useEngineAudio: false` changes is only partly known. The one log we have shows that it skips only the output-model hook (#831). That it switches to miniaudio's own device is **UNVERIFIED**.
- **Lip-sync is driven from the generated audio.** It works through face morphs, not `.lip` files. The third-party notices list NVIDIA Audio2Face-3D. Issues mention "real lip sync", a mouth left open until `MFG phoneme 0 0`, and a Papyrus face reset (`DATA/THIRD_PARTY_NOTICES.txt`, #311, #161, `SRC/SkyrimNetInternal.psc: ResetFacialAnimations`).
- **Voice effects are YAML chains of audio filters,** for example formant shift, then pitch, then distortion, then reverb. They can be hot-reloaded, set per actor from Papyrus or YAML triggers, and previewed on the dashboard (`LIB/voice_effects/*.yaml`, `SkyrimNetApi.psc: SetVoiceEffect`).
- **Speech input runs local Whisper by default.** It uses whisper.cpp and can also use Parakeet; Fateless and OpenAI are cloud options. It is push-to-talk (with press and release events and hold time) or open-mic. Sound cues play at start, end and "nothing heard". A prompt renders an STT **name hint**: nearby NPC names and places, so proper nouns come back spelled right (`LIB/prompts/stt_hints.prompt`, `DATA/assets/*.wav`).
- **Subtitles are SkyrimNet's own HTML overlay.** It replaces the vanilla HUD subtitle. Lines can float over the speaker's head or sit in a bottom box. A dialogue line outranks background chatter. The overlay shows translation lines and furigana/pinyin readings, uses the game's own font (converted at runtime), is styled from a YAML file, and hides while menus are open (`PUI/subtitles/index.html`).
- **The chat overlay replaces Skyrim's text-entry box.** It has history, `@Name` targeting with Tab completion, whisper and group modifiers, slash commands, editing and deleting past events, and a "who can hear you" strip. It runs as a PrismaUI (Ultralight) view and talks to the DLL through a JSON bridge (`DATA/chat/*.js`, `FEATURES.md`).
- **SkyrimNet extends vanilla dialogue rather than replacing it.** Player menu picks are voiced. Unvoiced NPC lines are voiced on demand. The LLM can **fire original quest and service lines** (Dialogue Actions, controlled by YAML allow/deny lists). A hotkey makes the AI highlight the best menu option. Vanilla lines are captured as conversation events, with noise filtered by regex rules (`LIB/dialogue_actions`, `LIB/filters`, `components/dialogue_action.prompt`).
- **Per-sentence TTS loses delivery context, so SkyrimNet carries it forward.** Each sentence is its own TTS request. A delivery tag such as `[whispers]` or `[angry]` is therefore repeated on every later sentence of the same reply. One-off sounds (`[laughs]`) are not repeated (`LIB/config/carry_tags_*.yaml`, #674, #782).
- **For an Elden Ring v1:** copy the stream contract, sentence streaming, the "one line per speaker, ranked" subtitle model, interrupt semantics, voice-resolution fallbacks, the bark cache and name hints. Use an ImGui overlay rather than a web view. Play audio on our own device with simple 3D attenuation rather than hooking the engine. Drop lip-sync, vanilla-dialogue firing, player voicing (the Tarnished is silent anyway) and the full dashboard.

---

## 1. Output side: from LLM tokens to a voiced, subtitled line

### 1.1 The markup contract the LLM writes

The dialogue prompt asks for a single response stream with distinct parts. All of the following is paraphrased from `LIB/prompts/submodules/user_final_instructions/0500_response_format.prompt` and `0650_audio_tags.prompt`:

| Segment | Syntax | What happens to it (FACT = stated by the docs or prompts; INFERENCE = routing) |
|---|---|---|
| Spoken dialogue | plain text | FACT: "text without asterisks is spoken dialogue". It goes to the actor's TTS voice and to subtitles. |
| Narration | `*NPC leans closer*` | FACT: optional, and only when narration is on (`is_narration_enabled()`). The prompt asks for it rarely ("roughly 1 in 4 responses"). INFERENCE: it is voiced by the narrator voice. Kokoro is listed as "narrator only" (`FEATURES.md`), a `narrator_effect` voice effect exists (`LIB/voice_effects/narrator_effect.yaml`), and the audio-tag prompt checks tag support for a separate narrator entity (`is_audio_tags_enabled(get_narrator_uuid())` in `0650_audio_tags.prompt`). So the narrator is a speaker with its own TTS settings. |
| Internal thought | `<internal_thought>…</internal_thought>` | FACT: placed strictly **after** the spoken line and never voiced. It is optional, gated by the template variable `allow_inline_internal_thoughts`. The prompt also forbids `<thinking>` or other scratchpad tags because "if any survive into the output they get spoken aloud by TTS". INFERENCE: thought blocks are removed before TTS and subtitles and kept in history. Anything the parser does not strip ends up spoken. |
| Voice-direction tags | `[angry]`, `[sfx-sigh]`, `[style-whispering]` … | FACT: only rendered when the actor's engine supports tags (`is_audio_tags_enabled(npc.UUID)`, `get_actor_tts(npc.UUID)`). The catalogue changes per engine (Higgs v3 categories, Chatterbox emotion and sound tags, Fateless/Inworld). The prompt warns that unknown tags get read aloud. |
| Translation | `<translation>` block after each sentence | FACT: when an NPC has a spoken language that differs from the subtitle language, each sentence is followed by its translation, "so a long response streams" (#788, `0760_spoken_language.prompt`). The audio uses the original and the subtitle shows the translation. |

Two supporting lists exist in the content library:

- **`kind: carry_audio_tags`** lists (`LIB/config/carry_tags_*.yaml`): twelve groups (anger, fear, pace, volume, manner, steering and so on) of tag words such as `angrily` and `slowly`. **FACT (PR #674):** because "each sentence is voiced on its own", a delivery tag matching these lists is **repeated on every later sentence of the same reply**. The next reply starts clean. Sound tags (`laughs`, `coughs`, `sighs`, `gasps`…) are left out on purpose so they play only once. The steering list (`say *`, `sound *`, `whisper *`, `reset`) was added for Fateless/Inworld TTS-2. There a direction holds until the next one or a `[reset]`, and the `[reset]` itself is carried (#782). The carry step runs in the core's stream-ingest stage, per the PR text. Lesson from #804: directions have side effects. Inworld TTS-2 spoke 11.7 characters per second with a direction against 16.9 without, so the prompt now asks for directions only at strong moments.
- **`kind: tts_rule`** filters (`LIB/filters/romanization_in_parentheses.yaml`): a regex find-and-replace applied to text before TTS. This one, off by default, strips romanizations in parentheses. A `Phonetic` config section is also listed among patchable configs (`SkyrimNetApi.psc: PatchConfig`). **INFERENCE:** these are pronunciation overrides.

**Why it matters:** one LLM call yields speech, stage directions, private reasoning and voice acting. A deterministic parser then sends each part to the right place: voice, narrator, history only, or TTS directions. That is cheaper than separate calls and keeps the voice and subtitles consistent.

### 1.2 The streaming sentence pipeline

Documented facts:

- "Text-to-speech starts speaking the first sentence while the model is still writing the second." "The game thread never waits on a network request, a database query or a TTS render." (`FEATURES.md`, "Why SkyrimNet is built this way")
- Continuous scene mode picks "the next speaker … as the current line finishes" (`FEATURES.md`).
- Users report settings named "Max TTS Segment Words: Sentence", "Max Parallel Requests" and "Stream Audio" (#776). That means segment size is configurable, several segments can be synthesised at once, and audio itself can stream in chunks.
- Bugs show the moving parts. One sentence splitter on the SSE stream split *inside* words at BPE token boundaries, which garbled both audio and subtitles while the full text stayed correct in the log (#318). Out-of-order generation increased time-to-first-audio (#654). Audio streaming truncated long sentences on PocketTTS (#776).

The flow these facts support (**INFERENCE** where marked):

```
LLM SSE tokens ──► sentence/segment splitter (word-safe boundary, size = sentence or N words)
                    │   strip <internal_thought>, split *narration*, keep/strip [tags] per engine,
                    │   apply tts_rule regexes / phonetic overrides            (INFERENCE: order)
                    │   carry delivery tags onto later sentences of the reply  (FACT: #674)
                    ▼
            per-segment TTS jobs (bounded parallelism, cache by voiceId+text)   (#470 log: "Cached TTS for voiceID")
                    ▼
            ordered per-speaker playback queue ──► audio out (engine or own device)
                    │                         └─► subtitle show/hide per segment (same id)
                    └─► face animation from the audio (lip-sync)
```

The public Papyrus API exposes the queue: `GetSpeechQueueSize()`, `GetTimeSinceLastAudioEnded()`, `PurgeDialogue(abDeferToCurrentFinished)` and `TriggerInterruptDialogue(...)`. The purge comments list exactly what is cleared: the TTS generation queue, the streaming text buffers, the per-actor dialogue queues and pending TTS tasks. They also distinguish "interrupt now" from "let the current line finish" (`SRC/SkyrimNetApi.psc`). Mods can listen for lifecycle ModEvents such as `SkyrimNet_SpeechStarted` (`FEATURES.md`, "Modder API").

**Event-driven interruption** is declared in data, not hard-coded. An event schema registered with `RegisterEventSchema(..., interrupt = true)` "can interrupt ongoing speech/actions" (`SkyrimNetApi.psc`). A YAML trigger with `interrupt: true` "purges queued dialogue/audio before responding" (`DEV/docs/modding/WORKFLOW_TRIGGERS.md`). Other mods can also mark an actor busy (`PublicSetActorBusy(formId, reason)` in `DEV/CppAPI/PublicAPI.h`; "Mark actors as busy … so SkyrimNet defers to your mod" in `FEATURES.md`).

### 1.3 TTS engines

`FEATURES.md` ("Speech") lists Piper (in process, CPU, the default, models SHA-pinned), PocketTTS (in process, live cloning), XTTS, Zonos, Chatterbox and Higgs (on a server you choose), OpenAI-compatible TTS, ElevenLabs, Inworld, Fateless (cloud), and Kokoro (narrator only). Engines can be mixed, for example Fateless for dialogue with local voices. A VastAI helper rents a GPU for heavy TTS engines. The bundled libraries match an in-process stack (`DATA/THIRD_PARTY_NOTICES.txt`, `DATA/libs`, `DATA/models`):

- ONNX Runtime (shipped as `onnxruntime_fateless.dll`): **INFERENCE**, for Piper and PocketTTS, and for the Audio2Face `network.onnx` (§1.7).
- `espeak-ng-data`: Piper's phonemizer data.
- `cpp-pinyin` and its Mandarin dictionary.
- SentencePiece.
- Signalsmith Stretch and Linear: pitch and time DSP.
- FFmpeg: decode, resample, convert.
- miniaudio: device I/O.

The engine is resolved per actor (`get_actor_tts(npc.UUID)` in `0650_audio_tags.prompt`). Virtual NPCs, which have no body, carry a `voiceId` (`SkyrimNetApi.psc: RegisterVirtualNPC`).

### 1.4 Voice assignment per NPC

Facts from shipped files and public issues:

1. **Default key = vanilla voice type.** Logs show TTS cached and spoken "for voiceID 'maleyoungeager'" and "femalecommoner", and per-voice-type utterance counts such as `femaledarkelf` and `maleorc` (#470, #775, #831). The issue author of #470 notes the voice was "resolved from the actor, not a generic".
2. **Reference samples come from the game's own lines.** A log in #775 shows "Converting FUZ audio data to WAV format (target 24000Hz)". The sample is then uploaded to a Chatterbox server as `femalecommoner.wav`. A `VoiceSampleArchive` skips silent files (#566). **INFERENCE:** for cloning engines, SkyrimNet builds a speaker reference per voice type from vanilla voice assets. The dashboard has a "Voice Samples" page (`FEATURES.md`).
3. **Per-NPC custom recording.** Characters → Voice → Upload writes a WAV under `DATA/voices/`, named after the character's **bio key** (`lydia.wav`). The engine looked it up by the lower-cased **display name**, first in a voice-type subdirectory (`voices/<voicetype>/<name>.wav`) and then in the root. In a localized game the two keys differ (Лидия vs lydia), so the recording was silently ignored. This shows why a stable key such as a UUID beats a display name (#775). A related bug: the PocketTTS `player` clone stayed cached under the same key after a new `voices/player.wav` was uploaded. There is also a per-key `VoiceSamples.<key>.selected_sample` setting (#665). The lesson is to key voice clones by a hash of the source audio, not by name alone.
4. **Creatures with no recorded lines** (skeletons, atronachs) used to fall back to the commoner voice. They now map their creature voice type to shipped samples at `REL/Sound/Voice/SkyrimNet.esp/creature_<name>/<name>.wav` (#829). The PR names `CrSkeletonVoice` and `CrAtronachFlameVoice`, with 13 more planned. The folder follows Skyrim's `Sound/Voice/<plugin>/<voicetype>/` layout but holds **reference samples**, not playable `.fuz` lines.
5. **Fallback:** a generic voice per gender (`femalecommoner` / `malecommoner`) when nothing else resolves (#775).
6. **Player voice:** the player gets a voice too, used for voiced menu picks and spoken input, and can be cloned (`player` clone in #665). There is a toggle, "Voice Player Input", described as "player input is still processed but the player character remains silent (subtitles only)" (#456). It exists because STT users disliked the player character "parroting what the user just said to his mic" (#123).

### 1.5 Voice effects (`LIB/voice_effects/*.yaml`)

Schema, from the eight shipped recipes: `id` (must equal the filename stem, per `DEV/docs/modding/CONTENT_ROOTS.md`), `name`, `sample` (always `null` in base; **INFERENCE:** an optional replacement voice sample), and `stages`, an ordered list of `{type, params}`. Recipes are capped at 64 KB, gated by the content hub, and can be hot-reloaded. Example (`LIB/voice_effects/draugr.yaml`, abridged):

```yaml
id: draugr
stages:
  - type: formant_shift   # params: semitones
  - type: pitch_shift     # params: semitones: -6.0
  - type: bitcrush        # params: bits, rate_div
  - type: distortion      # params: drive, mix
  - type: reverb          # params: room_size, decay, wet
```

Stage types found in base, with their parameters:

| Stage type | Parameters |
|---|---|
| `pitch_shift` | `semitones` |
| `formant_shift` | `semitones` |
| `eq` | `mode` (low), `freq_hz`, `q`, `gain_db` |
| `reverb` | `room_size`, `decay`, `wet` |
| `echo` | `delay_ms`, `feedback`, `wet` |
| `chorus` | `rate_hz`, `depth_ms`, `voices` |
| `distortion` | `drive`, `mix` |
| `bitcrush` | `bits`, `rate_div` |
| `ringmod` | `freq_hz`, `mix` |
| `whisper` | `noise_db`, `mix` |
| `gain` | `gain_db` |

Built-in recipes: werewolf, vampire lord, draugr, dragon priest, dremora, ghost, and the overlays `narrator_effect` and `player_thought_effect` (both a 200 ms echo).

History, from public PRs: the narrator, player-thought and telepathy echoes used to be hard-coded. They were moved into recipes, and telepathy reuses the player-thought recipe (#382). The creature recipes were first described as placeholders ("Werewolf does not sound much like a werewolf", #380). In Beta 25 they moved from a loose `original_voice_effects/` folder into the base content package (#620). In practice, a voice effect is a cheap way to mark *kinds* of speech (thought, narrator, telepathy) as well as creature types.

Ways to apply an effect:

- Papyrus: `SetVoiceEffect`, `ClearVoiceEffect`, `GetVoiceEffect`, plus `...ByUUID` versions (`SkyrimNetApi.psc`).
- YAML triggers: `activate_voice_effect` / `deactivate_voice_effect` with `effectId` (`DEV/docs/modding/WORKFLOW_TRIGGERS.md`). An example is a werewolf voice that turns on during transformation.
- The dashboard's Voice Effects editor, which has an audio preview (`FEATURES.md`).

**INFERENCE:** the effects run on the synthesised PCM after TTS and before playback. Signalsmith Stretch is a pitch and time library, which fits pitch and formant shifting.

### 1.6 How audio is played inside Skyrim

The question was whether SkyrimNet plays through engine voice playback (dummy Topic/TopicInfo, voice type, `.wav`/`.lip`) or through its own audio device. The evidence:

- **FACT (ESP):** `ESP/` contains no DIAL or INFO records. The record groups are Cells, Factions, Globals, Keywords, MagicEffects, Messages, Npcs, Packages, Perks, Quests, SoundCategories, SoundOutputModels, Spells and VoiceTypes. The audio-related records are:
  - `SkyrimNet_AudioCategoryVoice` ("SkyrimNet Voice", static volume multiplier 0.9, default menu volume 0.8), `..._BardSinging` (0.5 / 1.0) and `..._VirtualSpeakers` (0.9 / 0.8). All three are flagged `ShouldAppearOnMenu` and parented to `0EB803:Skyrim.esm`. PR #463 identifies that parent as `AudioCategoryPausedDuringMenu`, "the same as $Voice", so SkyrimNet audio gets its own sliders in Skyrim's Audio settings and pauses with menus. #463 also says the engine audio path "resolves this form by editor plugin name at runtime". If the form is missing, songs "play with no category at all and ignore any volume setting". The two volume fields multiply, which is why bards get 0.5 × 1.0.
  - `SkyrimNet_SOMDialogue2DDry`: a `DefinedSpeakerOutput` output model with `ReverbSendPercent 0`. **INFERENCE:** for non-spatial lines such as the narrator, virtual speakers, the player's own voice and telepathy.
  - `SilentVoice`: a voice type with no other fields and no references in the ESP. **INFERENCE:** a placeholder voice type that can be given to actors or virtual speakers so the engine never finds vanilla lines for them.
- **Public logs (#831, #656), quoted by users.** At start-up SkyrimNet installs audio hooks "via MinHook" on `BSResource::ID::GenerateFromPath`, `BSAudioManager::BuildSoundDataFromFile`, `BSSoundHandle::SetObjectToFollow`, `BSSoundHandle::SetOutputModel` and `DialogueItem::Ctor`. Playback logs read "EngineSoundPlayer: registered blob … dir=0x534E4554" (ASCII "SNET"), then "GetSoundHandle … ok=true", "following speaker node", and "play queued". Logs also report "game applied dialogue output model … CHANGED", which means the engine swaps reverb and attenuation models on SkyrimNet's sounds per cell. #831 names `audio.useEngineAudio: true` as "the shipped default". In the reporter's log, turning it off skips **only** the `BSSoundHandle::SetOutputModel` hook. The other hooks, including the output-model-changed callback, stayed installed, and 31 lines were still voiced.
- **Other speakers on the same path (public logs).** AI bard songs log "Using spatial audio for <Bard>" and start through an in-memory player (`PlayAudioFromMemory`). That player is invisible to the vanilla `BardSongsScript`, which breaks bard-schedule mods (#349). A bodyless **virtual NPC's** voiced line is also "served" by `EngineSoundPlayer` (#801). `FEATURES.md` adds that during AI songs "the vanilla bard voice and instrument idles are suppressed".
- Older evidence: in 2025 the VR build had no spatial audio, and voices were "played at the same volume, regardless of distance" (#36).
- Issue #732 says "Improved Facial Animations" works only on vanilla voice lines, "not those that were generated". So generated audio is not routed through TopicInfo voice files.

**Conclusion (INFERENCE, well supported).** SkyrimNet does **not** use dummy topics. Its default **engine-audio path** treats TTS PCM as an in-memory "file" under a fake directory hash. The game's `BSAudioManager` loads it as sound data, and a `BSSoundHandle` plays it. That gives the line native 3D attenuation, follows the speaker's head node, uses Skyrim's dialogue output models (reverb, interiors), its volume categories and menu pausing. What `useEngineAudio: false` does is **not established**. The only log of it (#831) shows just the output-model hook skipped, with playback otherwise unchanged. So the flag may control only output-model handling, not choose a different device. miniaudio in the notices may serve microphone capture, previews or an older or VR path. That it is a separate playback fallback is **UNVERIFIED**. The engine path is better integrated but fragile: crash #831 is attributed to the output-model hooks, and #349 shows a side effect of bypassing the game's own script pipeline.

### 1.7 Lip-sync and face

- FACT: the notices bundle "NVIDIA Audio2Face-3D v2.3 (Mark)". The face model's skin-solve basis is compiled in, and `network.onnx` is downloaded from NVIDIA (`DATA/THIRD_PARTY_NOTICES.txt`). Audio2Face turns speech audio into facial animation.
- FACT: the face was left open after speaking until "MFG phoneme 0 0" was typed in the console (#311, beta17). That points to Skyrim's MFG expression and phoneme morphs. `MFG Fix NG` is a requirement for Skyrim VR (`GH/README.md`).
- FACT: PR #161 added Papyrus that calls `QueueNiNodeUpdate`, or `RegenerateHead` when mounted. It stops vanilla lip-sync "while silent TTS facial animations play" and resets stuck faces (`SRC/SkyrimNetInternal.psc: ResetFacialAnimations`).
- FACT (user report): generated lines have "real lip sync" (#470). The same reporter counted "70 viseme changes of lip sync" on one generated line. That suggests the output reaching Skyrim's face is a stream of discrete visemes or phonemes. Whether this comes from Audio2Face, from a separate phoneme path, or from both is unknown.
- **INFERENCE:** a likely chain is audio → Audio2Face-3D (ONNX) → blendshape weights → retargeted every frame onto Skyrim's phoneme and expression morph targets. No `.lip` file is produced, which is why `.lip`-based mods such as Improved Facial Animations don't see these lines.

### 1.8 Queues, interruption, barge-in and vanilla voices

- **Barge-in:** a setting, "Interrupt On Voice Start", controls whether pressing push-to-talk cuts NPC speech. #548 found that releasing push-to-talk flushed queued NPC lines unconditionally. It also found that lines removed before the player heard them stayed in history, so later NPCs answered lines the player never heard. #691 reports that background banter is not cancelled by push-to-talk. Proposal #823 is a spoken cancel phrase ("don't send") that drops the utterance.
- **Vanilla voice coexistence:** public logs describe a `VoiceSilencerManager` that does two things (#566, #470). When a vanilla voice file starts for an actor, SkyrimNet's TTS for that actor waits (`OnVanillaVoiceStarted … vanillaUntilMs`, plus a 1 s pad). While SkyrimNet TTS plays, the actor's vanilla lines are suppressed ("Skipping silent dialogue … actor silenced (TTS playback in progress)", then "(Dialogue) Dropping suppressed line from <npc>", #670). **INFERENCE from log lines:** suppression works through the voice-resource and `DialogueItem` hooks listed in the #831 start-up log, plus a timed window ("DialogueItemCtorHook: adding 3 paths to timed suppression", #470). The reporter's own setup had `shouldSilenceActors: true`; its shipped default is UNVERIFIED. Dropping a vanilla line from the same actor during SkyrimNet speech is **intended**: #470 was closed on that basis, and #670 asks only for a narrow exception. The edge cases (silent placeholder `.fuz` files, forced greetings, click-through) show this is hard to get right.
- **Package-level suppression (ESP FACT):** the two conversation packages bound to the library script's `packageDialoguePlayer` and `packageDialogueNPC` properties (`ESP/Quests/skynet_MainController`), `SkyrimNet_PlayerDialoguePackage` (000003) and `SkyrimNet_NPCDialoguePackage` (000004), keep only `AggroRadiusBehavior` among their interrupt flags. Both use 256- and 512-unit radii around the dialogue target. The separate `SkyrimNet_TalkToPlayerPackage` (000E9D) is built on the follow-player template (radii from 200 to 600 units) and leaves `HellosToPlayer` out. At runtime, `skynet_MainController.SetActorDialogueTarget` sets a linked ref with `SkyrimNet_DialogueTargetKeyword`. It then calls `SkyrimNetApi.RegisterPackage(actor, "TalkToPlayer" | "TalkToNPC", …)`, which takes *friendly names*. Which ESP package each name resolves to is not documented. A config flag, `speech.shouldApplyTalkToPlayerPackage` (default true, toggled in `SRC/skynet_ConfigUtilityMenu.psc`), turns the package off. **INFERENCE, using Creation Kit package semantics:** NPCs in an AI conversation don't fire vanilla hellos or idle chatter over it. `SetActorDialogueTarget` also calls `SetLookAt`, so the speaker turns to look at the listener (`SRC/skynet_MainController.psc`).

---

## 2. Subtitles (PrismaUI `subtitles` view)

`PUI/subtitles/index.html` is a self-contained HTML/CSS view. Its header comments describe the features (read for UI behaviour only):

- **Replaces vanilla HUD subtitles:** it "draws the subtitle lines the plugin hands over in place of the vanilla HUD text". The layer is click-through and covers the whole screen.
- **Two placements**, set by `subtitles.position`:
  - **floating**: a card centred over the speaker, at screen positions the plugin sends every frame. It shrinks and fades with distance, cards are pushed upward so they never overlap, and a speaker who is off-screen or behind the camera falls back to the bottom.
  - **bottom**: a box anchored at the bottom centre that grows upward.
  - A line waits a short time for its speaker's first screen position so it doesn't flash at the bottom first.
  - Placement is also **per line**: each line is tagged `speaker` or `bottom`, so a line with no body to float over stays in the box even in floating mode. Which lines are tagged `bottom` (narrator, player, virtual speakers?) is not stated.
  - The plugin sends each followed speaker's position as `{x, y, d}`, where `d` is a share of the maximum distance. It can instead send `behind` (behind the camera) or null (too far or not loaded). With `subtitles.offscreenAtBottom` (default on), a floating line that is out of view shows in the bottom box. Edge flicker is avoided with hysteresis: a line goes off-screen and comes back at different on-screen shares.
  - **Cost control:** floating cards snap to whole pixels and scale and fade in fixed steps, "so a small move rewrites nothing and the text is drawn at few sizes".
- **One line per speaker, ranked.** The bottom box shows the highest-ranked line, then the newest. A "background line waits under a dialogue line".
- **Bilingual:** the line and its translation sit side by side, or stacked when long. Readings (furigana, romanized text, pinyin with optional tone colours) render as ruby. CJK fonts are preloaded so the first CJK line doesn't stall.
- **Game font:** the plugin converts "the game's font library in memory" into TrueType and sends it as base64. Fallbacks are Bahnschrift and then per-language Windows fonts.
- **Styling from `SubtitleWindow.yaml`:** scale, colours, outline, backing plate (`none` or `box`) and its opacity, box geometry, alignment (left, centre, right), growth direction (up or down), name colour, names shown over heads and over-head size. `subtitles.pinyinToneColors` is off by default.
- **Hides when a full menu opens** (inventory, map) and resumes afterwards (`subtitleMenus`).
- **Bridge (C++ → JS):** `subtitleShow`, `subtitleHide`, `subtitleClear`, `subtitleMenus`, `subtitleLayout`, `subtitleStyle`, `subtitleFonts`, `subtitleAnchors`. All take JSON. Lines are keyed by a positive integer id, and speakers by form ID.
- **Settings and engine side:** settings include `subtitles.enableNPCSubtitles`, `subtitles.enablePlayerSubtitles` and `dbvo.fixSubtitleDuringTTS` (a user's config in #656). A logged error shows a Scaleform hook at work: "[AudioHooks.cpp:1041] GFxValueObjectInterfaceInvokeHook: exception while processing Invoke". It fires 20 to 40 ms after `EngineSoundPlayer` serves a line, "which is when the line starts playing and its subtitle is shown". The reporter was running po3's Floating Subtitles 3.3.5 at the same time (#801). **INFERENCE:** SkyrimNet intercepts the vanilla HUD subtitle call so the overlay or the vanilla HUD shows its text instead. When the player turns off "Voice Player Input", player subtitles must still show; that once broke (#456). Translation for vanilla lines the game cannot localize comes from `LIB/prompts/transformers/subtitle_translation.prompt` (#788). Separate audio and subtitle languages were requested in #584.

---

## 3. Input side

### 3.1 Text: the chat overlay

`FEATURES.md` ("In-game chat overlay") lists:

- persistent history with timestamps
- `@Lydia` targeting with Tab autocomplete
- **Whisper** (`^`) and **Group** (`&`) modifiers
- slash commands: `/think`, `/transform`, `/silent`, `/monologue`, `/compose`, `/playsong`, `/summon`, `/npcthink`, `/telepathy`, and `~`
- edit or delete any earlier event
- a right-click menu on names (talk, whisper, groups, copy)
- a live strip of who can hear you
- font, opacity, anchor and width set in game

The shipped PrismaUI chat is a bundled build (`PUI/chat/`), which was not read. Its stylesheet's class names confirm an audience strip, a tab-completion popup, a context menu, a settings panel with sliders, an inline edit box, mode buttons, an OmniSight capture panel, and **IME** elements (`ime-composition`, `ime-candidate-list`). These suggest the overlay draws its own IME composition string and candidate list for CJK typing instead of relying on the OS window.

`DATA/chat/` holds a small readable version of the same view. It shows the bridge pattern clearly:

- **C++ → JS** (PrismaUI `InteropCall`): `updateMessages(json)`, `updateAudience(json)`, `setTheme`.
- **JS → C++** (PrismaUI `RegisterJSListener`): `skyrimnet.chatSubmit({text})`, `chatClose`, `eventEdit({id,data})`, `eventDelete({id})`, `requestRefresh`.
- Messages are event-history records (`speech`, `direct_narration`, `persistent_generic`, `player_thought`), styled as player, NPC or system.
- The audience entries are `{name, distance}` plus a whisper flag and radius. Clicking a name inserts `@Name`.
- The mode badge reads from the prefix: `~` Think, `!` Narrate, `@` Direct, `/silent`, `/transform`.

The prefixes differ across sources. The wheel-menu comment says `%` Think, `'` Transform, `!` Direct and `/silent`, and `FEATURES.md` uses `~` and `/think`. **INFERENCE:** the prefix grammar has changed over time; `FEATURES.md` is the current one.

Old Papyrus text input is gone: `SkyrimNetInternal.GetPlayerInput` logs "Papyrus text input removed, use PrismaUI". The release still ships a UIExtensions text-entry menu script (`SRC/uitextentrymenu.psc`, with saved position, scale, font size and opacity), apparently left over from the Scaleform era. Downstream, typed and spoken player lines are **different event types**: `dialogue_player_text` and `dialogue_player_stt` (`DEV/CppAPI/PublicAPI.h`, event callbacks). Consumers can therefore treat an STT line as less reliable. Whisper mode toggles `interaction.maxDistance` between a whisper radius and the normal radius, which shrinks who perceives you (`SkyrimNetApi.psc: TriggerToggleWhisperMode`).

### 3.2 Voice: STT, push-to-talk and open mic

- **Engines:** local Whisper (whisper.cpp; `DATA/libs/whisper.dll` with `ggml-cpu/cuda/vulkan.dll`). `FEATURES.md` lists "CUDA, Vulkan, OpenCL or CPU", but no OpenCL DLL ships in this build. Models range from `tiny` to `large-v3-turbo` and are downloaded for you. Parakeet (`parakeet.dll`, listed in the notices under whisper.cpp; selectable per #781). Cloud options: Fateless and the OpenAI Whisper API (`FEATURES.md`). CUDA 12.x speeds up STT, and the setup page has an STT test (`GH/README.md`). Lesson from #781: Parakeet sometimes transcribed English speech as Russian Cyrillic on a Russian-locale Windows. Pin the STT language rather than auto-detecting it.
- **Push-to-talk semantics** (`SkyrimNetApi.psc`). Each mode has a Pressed/Released pair: Speech, Thought, Dialogue Transform ("say this in-character for me") and Direct (narration).
  - Press plays a start cue or notification, "prepares search query cache for player input", starts recording with a timeout (200 s for speech and direct, 90 s for thought and transform), and resets the GameMaster cooldown.
  - Release passes the hold duration. Under 0.3 s on Thought or Transform means "let the character think or speak on their own" with empty input.
  - `FEATURES.md` adds that action eligibility is pre-warmed while you speak. **Pattern:** start retrieval and pre-computation on key-down, so by the time STT finishes the context is ready.
- **Open mic:** `TriggerToggleOpenMic`. Requests ask for a configurable default (#631) and for recording to continue while menus are open (#297). **INFERENCE:** voice-activity detection splits open-mic speech into utterances.
- **Audio cues:** `DATA/assets/input_start.wav`, `input_end.wav`, `no_transcription.wav` (played when the transcript is empty, per #823) and `empty_100ms.wav`.
- **Name hints:** `LIB/prompts/stt_hints.prompt` renders a short comma-separated list in the background (on cell change, else every 30 s) and on each push-to-talk press. It holds nearby NPC names, current and nearby place names, the player's name and pinned NPCs, capped at 500 characters and cut at punctuation. Hints go with push-to-talk **and open-mic** utterances. Defaults, per the template header: `STT.yaml name_hints` is **on**, but for local Whisper `whisper.name_hints` is **off**. Local Whisper reads the list as text heard just before the speech (and copies its style). Hosted services take it as a prompt, and Fateless also splits it into keywords of up to five words. STT errors still happen: proper names dropped (#827) and stray numbers inserted (#779).
- **Barge-in has two possible moments.** With "Interrupt On Voice Start" on, NPC speech is cut when the key goes down. With it off, the cut happens only when the transcript is submitted (#823). #823 also confirms the empty-transcript branch: it plays `no_transcription.wav` and dispatches nothing.
- **Reliability:** VR push-to-talk presses were silently dropped (115 of 137 in one session, #687). Many drops were quick re-presses, perhaps from a debounce, after the user heard no start cue. Make the "recording started" cue reliable and the input path idempotent.

### 3.3 Hotkeys, powers and the wheel

- **Two hotkey systems** (`SRC/skynet_Library.psc`, `skynet_McmScript.psc`).
  - **Native C++** (the default; OS-level key polling).
  - **"In-game" Papyrus** (`RegisterForKey`, `OnKeyDown`/`OnKeyUp` with `holdTime`), chosen on the MCM Hotkeys page. Turning it on disables the native hotkeys (`SetCppHotkeysEnabled(false)`). This exists for VR, gamepads and conflicts.
  - The MCM lists: Voice Recording, Voice Thought, Voice Dialogue Transform, Voice Direct Input, Text Input, Toggle GameMaster, Toggle Continuous Mode, Toggle World Events, Toggle Whisper Mode, Toggle Open Mic, Open Dashboard, Continue Narration, Capture Target (hold for the player), Generate Diary & Bio, and Interrupt Dialogue. A master on/off switch (`TriggerToggleGlobalAI`) and a "dialogue choice" hotkey (`FEATURES.md`) also exist.
- **Lesser Powers as buttons (ESP FACT):** `skynet_VoiceInput{Normal,Thought,Transform,Direct}Spell` and `skynet_WheelMenuSpell` are `LesserPower` spells with script effects that toggle recording (`SRC/skynet_VoiceInput*.psc`). They can be favourited and bound to the power button, so they work on gamepad and VR. A spell cannot see a key release, so each voice power **toggles**: the first cast starts recording, and the second calls the `...Released` function with a fixed 10 s duration, which stays clear of the under-0.3 s "think on your own" rule (`SRC/skynet_VoiceInputNormal.psc`). Only the wheel power is auto-added (`skynet_MainController.AddSpells`).
- **VR:** VRIK gesture actions are registered for push-to-talk, transform, interrupt and similar (`skynet_Library.InitVRIntegrations`).
- **Wheel menu** (`SRC/skynet_WheelMenu.psc`, UIExtensions `UIWheelMenu` with 8 fixed slots):
  - Top level: Text Input, Think to Self, Auto Roleplay (the LLM writes the player's line), Toggle Whisper, Utilities, and Open Dashboard (slot 6). Slots 5 and 7 are blank in this build; PR #542 proposed a "Voice Modes" submenu.
  - Utilities submenu: GameMaster, NPC Reactions, Actions, Continue Narration, Continuous Mode and Interrupt. A whitelist/blacklist entry appears for the actor under the crosshair, which moves them between the SkyrimNet allow and deny factions. A second pair does the same for memory allow and deny.
  - A script comment notes that "UIExtensions menus generally don't render in VR". The bindable dashboard hotkey is therefore the real VR path, and the wheel is a convenience for flat-screen play.
  - A still-open request (#162) asks for a Mass-Effect-style **dialogue wheel**: user-defined tones (Affirmative, Sarcastic, Threaten, Negotiate…), each tied to a prompt that writes the player's line. The aim is to steer "Auto Roleplay" without typing.

---

## 4. UI hosts and the web dashboard

| Host | Used for | Notes |
|---|---|---|
| **Scaleform/GFx** (vanilla) | MCM (SkyUI), UIExtensions wheel and `UIListMenu` (the config utility menu, `SRC/skynet_ConfigUtilityMenu.psc`), the legacy `UITextEntryMenu`, vanilla `Message` boxes (`skynet_DeleteHistoryMessage`, `skynet_DiaryScopeMessage`), vanilla subtitles and dialogue menu | A logged GFx `Invoke` hook fires as each line's subtitle is shown (#801). **INFERENCE:** it intercepts the HUD `ShowSubtitle` call. |
| **PrismaUI** (Nexus 148718) | chat, in-game dashboard, subtitles (`PUI/`) | An SKSE framework rendering HTML/CSS/JS with the Ultralight SDK ([PrismaUI-SKSE/framework](https://github.com/PrismaUI-SKSE/framework)). Custom licence; modified redistribution not allowed. Requires Media Keys Fix SKSE (`GH/README.md`). |
| **Magelight UI** | an alternative host for the same views (`FEATURES.md`: "drawn by Prisma UI or Magelight UI") | No public documentation found. The subtitle code notes that Magelight keeps the view at the backbuffer size. **INFERENCE:** an in-house web-view host. |

**Web dashboard** (`localhost:8080`, also in game through PrismaUI with `TriggerToggleDashboard`; `FEATURES.md`). Its pages are Home, Characters, Memories, World Knowledge, Triggers, Voice Effects, Voice Samples, OmniSight, Prompts, Actions, Diary, Agents, Content Library, Settings, Game Data Explorer, and Logs/Traces/Performance. The parts that matter for presentation are:

- the setup wizard and the "Test & Easy Setup" page (with the STT test)
- the per-character Voice card (upload a recording, "Play X's Voice")
- the Voice Effects editor with audio preview
- visual hotkey capture in Settings
- Settings › Dialogue Actions

Besides English, the dashboard ships nine locales: de, es-ES, fr, it, ja, pl, pt-BR, ru and zh-Hans. There is also an `en-XA` pseudo-locale, conventionally used to test localization (taken from `PUI/dashboard` chunk file names only). When the dashboard has input focus, Papyrus `OnKeyDown` is suppressed, so a JS "input sink" handles the close key (`SkyrimNetApi.psc: SetDashboardToggleKey`). This is a practical lesson about overlays capturing input.

---

## 5. Vanilla dialogue integration

### 5.1 Voicing the player's picks and unvoiced NPC lines

- **Player picks:** "When you choose a line in the vanilla dialogue menu, SkyrimNet speaks it in your character's voice, and prepares a voiced reply for NPC lines that never had voice acting" (`FEATURES.md`). This used to depend on the Dragonborn Voice Over mod and a Papyrus `PlayDBVOTopic` handler that waited on `IsNPCDialogueReady()` and then released the menu's topic timer (#470). Since PR #772 "the DLL holds the player's dialogue-menu pick itself". The script now only unregisters the old event (`SRC/skynet_Library.psc: Maintenance`). The natives `TriggerPlayerTTS`, `IsPlayerTTSFinished`, `PrepareNPCDialogue` and `IsNPCDialogueReady` are still in the beta30 API.
- **Unvoiced NPC lines** (`dbvo.voiceSilentNPCs`): TTS in the actor's resolved voice type, spatial, with lip-sync and subtitles (#470). Keeping it in step with the engine's dialogue clock is the hard part. Forced greetings were not synchronised (#469, #470). Fuz Ro D'oh's silent placeholder `.fuz` files distorted timing (#566), and clicking past a line dropped the next one (#670).
- **Text hygiene for vanilla lines:** text sent to TTS must be the resolved string. #139 found localization placeholders (`$FF_SayTrade` from a translation file) sent to TTS literally. #574 found underscores appearing between words in vanilla lines voiced through the player-pick path.
- **Text transforms for vanilla lines:** `transformers/player_option_translation` rewrites or translates the menu options in one call. Per `GH/changelog.d/player-dialogue-translation-prompt.md`, it gets `originalDialogue`, `lineCount`, `speechPattern` and `translationLanguage`, and answers one line per option. `transformers/native_dialogue_transformer` rewrites a vanilla line to fit a character at about the same length. `transformers/universal_translator` makes a character speak their own language with a translation underneath.

### 5.2 Dialogue Actions: firing original lines

- **Prompt side:** `components/dialogue_action.prompt` and `0740_quest_dialogue.prompt` use the decorator `available_dialogue(npc.UUID, mode)`. It returns the NPC's currently available vanilla branches, rendered as indented example exchanges and grouped under quest headings, each tagged with an 8-hex-digit code. Quest beats and services (follower, trade, lodging, and so on) are listed separately.
  - The rules (paraphrased): fire a code only once the player has actually agreed, follow the path the player chose, at most one per turn, and when unsure keep talking.
  - The model fires a `TriggerDialogueLine` action with the code as `form_id`, and "the game runs" that line, including its script fragment.
  - The quest list is repeated at the end of the reply prompt so models don't forget it (`LIB/prompts/dialogue_response.prompt`, `GH/changelog.d/embedded-actions-reminder.md`).
- **Content side** (`LIB/dialogue_actions/*.yaml`, `DEV/docs/modding/CONTENT_ROOTS.md`):
  - `kind: lists` files add `whitelist` / `blacklist` entries to the user's lists. The entries are quest editor IDs or TopicInfo fragment script names.
  - `kind: instruction` files are keyed by `key` and attach guidance to a line. They can override its `category`, one of: `quest`, `follower`, `merchant`, `trainer`, `carriage`, `innkeeper`, `bard`, `marriage`, `crime`, `other`.
  - Base ships two deny lists, each "one purpose per file" so users can switch off exactly one:

```yaml
kind: lists
name: Quest stage setter TIF__000C3698
description: Keeps the LLM from firing a line whose script sets a quest stage.
blacklist:
  - TIF__000C3698
```

### 5.3 The dialogue-choice hotkey

"A hotkey asks the AI to pick the dialogue-menu option that fits what you want to say. It only highlights the option; you still confirm it" (`FEATURES.md`). This is a light way to let free speech drive a fixed menu while leaving the final click to the player.

### 5.4 Coexistence: filters and captured vanilla lines

- **Who may speak:** `kind: actor` filters with race, faction and gender allow/deny lists (`LIB/filters/animals_dont_speak.yaml`, `mannequins_dont_speak.yaml`, `paarthurnax_speaks.yaml`). Two factions are under the player's control (`SkyrimNet_ActorWhitelistFaction` / `...BlacklistFaction`). Separate `kind: memory` lists control who forms memories.
- **Vanilla lines become events:**
  - Logs show vanilla lines recorded as "(Dialogue) <NPC>: … (Talking to Player)", including NPC-initiated forced greetings (#470). **INFERENCE:** they are captured through Skyrim's standard `TESTopicInfoEvent` sink, which CommonLibSSE exposes, plus the `DialogueItem` hook seen in the #831 start-up log.
  - `kind: dialogue_rule` regexes drop noise such as the combat-grunt subtitle strings, "(Heavy breathing - sprinting)" and "(Invisible Continues Linked to Self)" (`LIB/filters/combat_grunts.yaml` and others).
  - **INFERENCE:** these rules clean the captured vanilla subtitle text before it enters history, so LLM NPCs "heard" vanilla conversations without engine noise.
  - The `world event reactions` toggle stops NPCs reacting to other dialogue entirely (`TriggerToggleWorldEventReactions`).

---

## 6. How it maps onto Skyrim engine features

| SkyrimNet feature | Skyrim/SKSE mechanism | Evidence |
|---|---|---|
| Spatial TTS playback | `BSAudioManager` / `BSSoundHandle`, follow the actor's node, dialogue output models, custom SNCT categories under `AudioCategoryPausedDuringMenu` | #831, #463, `ESP/SoundCategories` |
| 2D lines (narrator, virtual speakers) | custom SOUM `SOMDialogue2DDry` | `ESP/SoundOutputModels` (use is INFERENCE) |
| Suppressing vanilla voice | hooks on voice resource ID creation and `DialogueItem` construction; packages with reduced interrupt flags | #470, #566, #831, `ESP/Packages` |
| Lip-sync | MFG phoneme/expression morphs, driven per frame; `QueueNiNodeUpdate` to cancel vanilla lip anim | #311, #161, notices |
| NPC faces the listener | `Actor.SetLookAt`, linked-ref keyword `SkyrimNet_DialogueTargetKeyword`, `RegisterPackage` with the friendly names TalkToPlayer/TalkToNPC; ESP conversation packages with 256/512-unit radii around the target | `SRC/skynet_MainController.psc`, `ESP/Packages` |
| Subtitles | GFx `Invoke` hook on the vanilla subtitle path (INFERENCE: `ShowSubtitle`), plus an Ultralight overlay positioned from the speaker's world position projected to the screen; game font converted from the font library | #801, `PUI/subtitles` |
| Chat and dashboard | PrismaUI (Ultralight) views with InteropCall/RegisterJSListener | `DATA/chat/app.js` |
| Hotkeys | native key polling, or Papyrus `RegisterForKey`; Lesser Powers; UIExtensions wheel; VRIK gestures | `SRC/skynet_*.psc`, `ESP/Spells` |
| Voice per NPC | VTYP voice type → voice ID; samples decoded from vanilla `.fuz` (xWMA plus lip) through FFmpeg | #470, #775 |
| Vanilla dialogue | INFERENCE: TESTopic/TESTopicInfo conditions evaluated for `available_dialogue`; firing a TopicInfo runs its fragment; dialogue menu hooks for voicing picks | `dialogue_action.prompt`, #470, #772 |
| AI bard songs | in-memory spatial playback; own SNCT `SkyrimNet_AudioCategoryBardSinging`, resolved by name at runtime (#463); vanilla bard voice and instrument idles suppressed | #349, #463, `FEATURES.md` |
| In-fiction toggles | MCM quest (SkyUI), Perks and Spells (Telepathy), Message forms | `ESP/Quests`, `ESP/Perks`, `ESP/Messages` |

---

## 7. Transferable patterns for an Elden Ring framework

The ER v1 targets are: (a) named NPCs hold free conversation; (b) generic humanoids bark short combat or ambient lines; (c) dialogue only. Elden Ring statements below come from general modding knowledge and **should be checked against the Elden Ring research stream**. They are marked *(ER, verify)*.

### 7.1 Copy as ideas (essential for v1)

1. **One stream, typed segments.** Ask for plain spoken text plus an optional trailing private thought (stripped before TTS and subtitles, kept in memory). Barks get no markup: one line, hard word cap. Leave narration and voice tags out of v1; adding them later does not change the parser's shape.
2. **Sentence streaming with three guards SkyrimNet learned the hard way.**
   - Split only on whitespace or punctuation boundaries, never on token boundaries (#318).
   - Bound TTS parallelism but play strictly **in order** (#654).
   - Let one segment-size setting choose between "first audio fast" and "fewer seams".
   - Per-sentence TTS forgets the delivery of the previous sentence. Keep a per-reply **delivery state**, carried onto every segment of that reply and reset for the next one: SkyrimNet's carry-over tags (#674), or in v1 simply the same voice, seed and speed for the whole reply.
   - Anything the parser does not strip will be spoken: unknown tags, `<thinking>` blocks, unresolved localization tokens (#139). Put a whitelist sanitizer between the parser and TTS.
3. **A presentation-neutral "Line" object** `{id, speakerId, text, rank: dialogue|bark, voiceId, audio?}` that drives audio, subtitles and the history log together. Copy the subtitle rule: **at most one visible line per speaker, and a dialogue line outranks a bark**.
4. **Commit to history only what was presented.** #548 shows the cost of storing lines that were flushed before playback, because NPCs then answer things the player never heard. Mark a line "heard" when it plays, or when its subtitle shows if TTS is off.
5. **Interrupt API with two modes:** "stop now" and "finish the current line, drop the rest", plus an optional "interrupt when the player starts talking" setting (from `PurgeDialogue`). Make interruption a **property of event types in data**, as in `RegisterEventSchema(..., interrupt)` and trigger `interrupt: true`. For ER this means "player aggroed", "NPC killed" or "boss phase" events can flush ambient chatter without hard-coded rules. Also cancel any in-flight NPC-initiated line when the player starts input (#691).
6. **Voice resolution chain with stable keys:** per-NPC override (keyed by a stable ID, never the display name, per #775), then archetype group, then sex and body fallback, then default. In ER the archetype key could come from the character model ID or NPC param row *(ER, verify)*, playing the role of Skyrim's voice type. Add a **TTS cache keyed by (voiceId, text)**. Barks repeat, so pre-render a pool of barks per archetype at load or idle time and combat barks cost no latency. If cloning is added later, key a clone by a hash of its reference audio, not by name, so a new upload is never masked by a stale clone (#665).
7. **Actor filters as small named YAML lists**, one purpose per file: "beasts don't speak", "these enemy types bark only", "these NPCs never use AI". Same union-of-contributions model as `LIB/filters`.
8. **Text input overlay** with: target = current **lock-on target**, else the nearest named NPC in range (ER's version of crosshair and `@Name`); Enter to send, Esc to close; a small "who can hear you" strip; history. This is the minimum usable surface.
9. **Subtitles overlay** with a bottom box for conversation and **floating cards over enemies for barks**, shrinking and fading with distance. Floating barks fit requirement (b) well. Hide during menus and cutscenes. Copy three small details from `PUI/subtitles`. Tag each line with its placement (`speaker` or `bottom`). Fall back to the bottom box when the speaker is off-screen, behind the camera or too far, with hysteresis at the screen edge. Snap card position, scale and opacity to steps so per-frame updates don't re-layout text.
10. **Tag player input by source** (typed vs STT) in history, as SkyrimNet does with `dialogue_player_text` and `dialogue_player_stt`, so prompts and memory can treat a transcript as less certain.

### 7.2 Simplify

- **UI host:** use **Dear ImGui** on a DX12 present hook, a common pattern in ER tools such as practice tools built on `hudhook` *(ER, verify)*, instead of an embedded browser. Keep SkyrimNet's JSON message boundary between core and UI (`updateMessages`, `subtitleShow` …) so a richer host can replace it later.
- **Audio:** play on **our own device** (miniaudio or XAudio2) with distance attenuation and stereo pan computed from speaker and camera positions read from game memory. SkyrimNet's engine path gives better integration but is its most crash-prone area (#831), and ER's Wwise pipeline is a bigger target *(ER, verify)*. Either way, AI audio is invisible to the game's own scripts, the same issue #349 hit with bard schedules. Expose our own "is speaking" state so other tools can query it. Add our own volume slider and pause audio when the game is paused or a menu is open.
- **TTS:** v1 supports one local CPU engine (Piper-class) and one OpenAI-compatible HTTP engine. Voice cloning and audio tags come later.
- **Voice effects:** keep the YAML recipe idea (it costs little and suits ER's spirits, ghosts, Omen and dragon-touched characters), but start with `pitch_shift`, `eq`, `reverb` and `gain`. As SkyrimNet does with its thought, narrator and telepathy echoes (#382), effects can also mark a *kind* of speech, for example a spirit-ash summon or a whispered aside.
- **Hotkeys:** three bindable keys plus a gamepad chord: Talk (open the overlay), Push-to-talk (later), Interrupt. Use press/release with hold time as in `TriggerRecordSpeechPressed/Released`, for example a long press to repeat the last line.
- **Settings:** a YAML config plus a log file. A tiny `localhost` status page can come later. SkyrimNet's dashboard is a product of its own.

### 7.3 Drop or defer (with reasons)

| Feature | v1 decision | Why |
|---|---|---|
| Lip-sync (Audio2Face → morphs) | **Drop** | Unknown access to ER facial rigs *(ER, verify)*; many ER NPCs are masked or hooded; costs a GPU or ONNX model. |
| Voicing player picks / player TTS | **Drop** | The Tarnished is a silent protagonist; typed or spoken input is enough. |
| Firing vanilla lines (Dialogue Actions) | **Defer to v2+** | ER dialogue runs on ESD talk scripts and event flags, which differ greatly from Skyrim's condition-evaluated TopicInfos *(ER, verify)*. It is also an *action*, out of v1 scope. |
| Dialogue-choice hotkey, option translation | **Defer** | Needs hooks into the talk menu. |
| Voicing unvoiced vanilla lines, VoiceSilencer timing | **Drop** | Most ER lines are voiced, and timing sync is SkyrimNet's buggiest area (#469, #470, #566, #670). |
| STT | **v1.5** | Independent of the game and low risk. When added, copy push-to-talk, cue sounds, `no_transcription` handling and **name hints** (Melina, Ranni, Rennala, Leyndell, Liurnia...). Pin the recognition language (#781) and choose a barge-in moment: key-down or submit (#823). Consider the cancel-phrase idea (#823). |
| Translation subtitles, ruby readings | **Defer** | Keep a `translation?` field in Line so the overlay can add it later. |
| Web dashboard, in-game dashboard, wheel menu | **Defer** | Config file plus overlay first. |
| Capturing vanilla lines as events | **v2 candidate** | High value: NPCs would "remember" what the player heard in vanilla talks. Needs a hook where ER resolves talk text *(ER, verify)*. Copy `dialogue_rule` regexes for noise. |

### 7.4 What is hard in Elden Ring, and why

| Problem | Difficulty | Notes |
|---|---|---|
| Typing while the world keeps running | Medium | ER has no real pause in normal play *(ER, verify)*. Gate the overlay to safe states (no aggro, near a friendly NPC), or ask the core research stream whether time can be slowed or paused. |
| Taking keyboard focus from the game | Medium | The overlay must swallow input (WndProc or raw-input hook). SkyrimNet's own focus edge cases (`SetDashboardToggleKey`, #313 Wheeler input bleed, #397 VR hotkeys) show the classes of bug. |
| Gamepad-only text entry | Hard | ER players often use controllers. Options: push-to-talk STT as the gamepad path, or the Steam on-screen keyboard *(verify)*. A third, typing-free option combines SkyrimNet's "Auto Roleplay" (the LLM writes the player's line) with the tone wheel proposed in #162. A radial menu of 4 to 8 intents (ask about this place, agree, refuse, threaten…) each fills in a short player utterance, shown as text only because the Tarnished is silent. |
| Floating subtitles | Medium | Needs the camera view-projection and character world positions from memory *(ER, verify)*. |
| Engine audio (Wwise) injection | Hard | Avoid in v1. Our own device gives most of the benefit. |
| Reusing the game's subtitle or talk box | Hard / unknown | An own overlay is safer. |
| Anti-cheat | Constraint | DLL mods require offline play with EAC disabled *(ER, verify)*. |

### 7.5 Suggested v1 presentation flow (ER)

```
[Talk key] → overlay opens (target = lock-on ▸ nearest named NPC) → text → core
core → LLM (stream) → splitter → Line{segments} ─┬─► subtitle overlay (bottom / floating for barks)
                                                  ├─► TTS (optional, cached, ordered) → own audio device (3D pan/atten)
                                                  └─► history.commit(when shown/played)
combat event → bark selector → cached bark audio + floating card (rate-limited, 1 per speaker)
[Interrupt key] → purge(now | after current)
```

---

## 8. Open questions and uncertainties

1. **Audio path internals.** The engine-path description comes from users quoting logs (#831, #656, #349, #801). It is consistent but secondhand. What `audio.useEngineAudio: false` switches is unclear: the only log shows just the output-model hook skipped. Whether a separate miniaudio playback path exists is UNVERIFIED. Whether narration actually uses `SOMDialogue2DDry` and `SilentVoice` is not documented.
2. **Lip-sync pipeline.** The Audio2Face-to-MFG chain is inferred from the notices and issues. It is unclear whether Audio2Face replaced an earlier amplitude- or phoneme-based method (the mouth issue in #311 predates the notice we read) and whether it runs for every engine.
3. **Narration voicing.** The prompts say narration is optional and in asterisks. Whether it is always voiced by a narrator voice, or only shown, is not stated.
4. **`carry_audio_tags` semantics: resolved.** PR #674 says delivery tags are repeated on later sentences of the same reply because each sentence is a separate TTS request. Still open: how a carried tag is handled by engines without tag support (presumably the tag prompt is simply not rendered for them).
5. **Chat prefix grammar.** The legacy view, the wheel comment and `FEATURES.md` disagree (`~`/`%`/`/think`, `!` Narrate vs Direct). `FEATURES.md` is taken as current.
6. **Dialogue Actions runtime.** How a TopicInfo is "fired" outside the dialogue menu (scene, forced say, fragment invocation) is not documented. This doesn't matter for ER v1.
7. **Magelight UI** has no public documentation.
8. **Elden Ring side (for the ER research stream):** a hookable point where talk text is resolved, for capturing vanilla lines; camera and position access for floating subtitles; whether the game can be paused or slowed while typing; facial animation access; Wwise feasibility; how per-character "voice archetypes" can be derived (model ID, NPC param, talk param).
9. Web pages for PrismaUI's documentation and an unofficial SkyrimNet docs site could not be reached from this session (hosts blocked). PrismaUI API names were taken from the readable chat JS instead.
10. **Package mapping.** Which ESP package the friendly names `TalkToPlayer`/`TalkToNPC` resolve to (000003/000004, or 000E9D for TalkToPlayer) is not documented.
11. **Lip-sync output form.** "70 viseme changes" (#470) suggests a viseme stream. Whether that comes from Audio2Face output, a phoneme path (for example espeak-ng for Piper), or both is unknown.
12. **Per-line subtitle placement.** Which speakers' lines are sent as `bottom` rather than `speaker` (narrator, player, virtual NPCs) is not stated in the view.

---

## Review notes

A completeness, accuracy and licence-compliance review was carried out on 2026-11-06. Claims were checked against the beta30-rc2 release, the devkit, the GamePlugin repo clone, and the public issue and PR texts the original author had collected. No compiled file was opened. Web search was not available during the review (session budget used up), so no new web sources were added.

**Spot-checks that passed (unchanged):** ESP sound records and their values (`ESP/SoundCategories`, `ESP/SoundOutputModels`, `ESP/VoiceTypes`); no DIAL/INFO groups in `ESP/`; `PurgeDialogue`/`TriggerInterruptDialogue` semantics and the 200 s / 90 s / 0.3 s push-to-talk rules (`SkyrimNetApi.psc`); `RegisterVirtualNPC(…, voiceId, …)`; the voice-effect schema and all eleven stage types with their parameters (`LIB/voice_effects/*.yaml`); the 64 KB recipe cap and the Dialogue Action categories (`CONTENT_ROOTS.md`); the `stt_hints.prompt` behaviour; the chat bridge functions (`DATA/chat/*.js`); the subtitle bridge functions and ranking rule (`PUI/subtitles/index.html`); the wheel layout (`skynet_WheelMenu.psc`); MCM hotkey names; VRIK gestures; LesserPower spells; the #831 hook list and the `EngineSoundPlayer` log lines in #656; #463, #548, #318, #775, #776, #829 and #772 as described.

**Corrected:**
- **Licence:** removed the claims based on #656's list of strings that its author says came from `SkyrimNet.dll` ("Rewriting ShowSubtitle…", Floating Subtitles compatibility). Also removed #470's format strings ("Suppressing voice resource '{}' (within time window)") and its handler names. The same points are now supported by real log lines (#801 GFx Invoke error, #831 hook list, #470 "adding 3 paths to timed suppression") and marked INFERENCE. A note on this was added to the method section.
- `carry_audio_tags`: the earlier INFERENCE ("map narration adverbs onto engine tags") was wrong. Per PR #674 they repeat delivery tags on later sentences because each sentence is a separate TTS request. Open question 4 is marked resolved. Steering and reset (#782) and the speed side effect (#804) were added.
- `useEngineAudio: false`: the "most likely a miniaudio device" conclusion was weakened. The only log (#831) shows just the output-model hook skipped and other hooks still installed. A separate miniaudio playback path is now marked UNVERIFIED (TL;DR, §1.6, open question 1).
- Packages: the §6 table wrongly gave "TalkToPlayer/TalkToNPC packages (256/512-unit radii)". In fact the 256/512 radii belong to `PlayerDialoguePackage`/`NPCDialoguePackage`, `TalkToPlayerPackage` uses follow-template radii (200–600), and the script applies packages by friendly name through `RegisterPackage`. `speech.shouldApplyTalkToPlayerPackage` was added.
- `shouldSilenceActors` "defaults to true" was supported only by users' configs, so it is now UNVERIFIED.
- Dashboard locales: `es` corrected to `es-ES`; the `en-XA` pseudo-locale noted.
- Whisper backends: added OpenCL as listed in `FEATURES.md` and noted that no OpenCL DLL ships.
- Custom voice upload (#775): the file is saved under the bio key and looked up by display name; this is now explicit.

**Added:** bard-song and virtual-NPC use of the engine player (#349, #801, #463); the narrator as a TTS entity (`get_narrator_uuid`); the `allow_inline_internal_thoughts` gate and the "unstripped text gets spoken" rule; event-level `interrupt` flags and the actor-busy API; voice-effect history (#380, #382, #620); the viseme count (#470); player-voice rationale (#123); clone-cache staleness (#665); per-line subtitle placement, off-screen fallback, hysteresis and step quantization; subtitle style options; typed vs STT event types; legacy `UITextEntryMenu` and `UIListMenu`; name-hint defaults (local Whisper hints off by default); Parakeet language drift (#781); the two barge-in moments (#823); VR push-to-talk drops (#687); the voice powers' toggle with a fixed 10 s release; memory allow/deny entries in the wheel; VR wheel limits; the tone-wheel request (#162); vanilla text hygiene (#139, #574); intended dropping of vanilla lines (#470, #670); matching ER patterns (§7).

**Could not verify (UNVERIFIED):**
- That a non-engine miniaudio playback path exists, and what `useEngineAudio: false` changes beyond the output-model hook.
- The shipped default of `shouldSilenceActors`, and the "Interrupt On Voice Start" default.
- Which ESP forms the friendly package names resolve to.
- Whether lip-sync visemes come from Audio2Face, a phoneme path, or both; and whether lip-sync runs for every TTS engine.
- Magelight UI internals (no public documentation found; no web search possible in this pass).
- Whether `RegisterDialogue`/`RegisterDialogueToListener` (`SkyrimNetApi.psc`) lines are voiced or only recorded as events.
- All *(ER, verify)* statements in §7, which still need the Elden Ring research stream.
