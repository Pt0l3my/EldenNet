# Proposal 0004: The character bio, fitted to Elden Ring

**Status: proposal, 2026-11-15 (roadmap S6.2), revised the same day after reading SkyrimNet 1.2.0 (§8; WHY§97, WHY§98). O46 and O85 open.** Where this text and the designs differ, the designs
hold. A bare §n is a section of this proposal; DESIGN, NPC and MEM are EldenNet's design documents. Evidence:
SkyrimNet's bio format (SN03 §5 and §6, confirmed on 2026-11-15 by two of SkyrimNet's own bio files the owner shared,
kept outside the repositories; WHY§96), the persona schema (NPC §3.1) and O46's draft (DESIGN §7.8).

## 1. What the owner asked

"We can take ideas from it, but we should think about what fits Elden Ring, and possibly a follower system where these
blocks matter more (e.g. skills may not be important until they actually join the player)." Before that: "I am not
really interested in using SN prompts, just the logic and structure that is proven to work."

## 2. What SkyrimNet proves, and what we take

| SkyrimNet's practice | We take it? |
|---|---|
| Authors write prose only, in named parts; the templates own every heading and the layout | Yes: our profile files hold prose in named fields, and our templates render them |
| Each part has an audience: public parts in others' prompts, private parts in the character's own, the voice last | Yes, as NPC §3.1 already does |
| A stranger test for public parts: what a first meeting shows, or public reputation | Yes (PL11) |
| Every bio fills every part, about 570 words | Yes, at about the same size for a speaking NPC |
| A copy of the bio per playthrough, updated cautiously from memories, never the shipped file | Yes, as the lived layer (§3.4) |
| A bio for the player's own character | Yes (§3.5) |
| Secrets in the private parts, kept back by the model | **No.** Models leak, and Elden Ring is built on mystery: an NPC is told only what is safe for the player to hear now (O36). Its later truths unlock with verified story stages |
| Skills in every bio | Moved to the companion layer, where they matter (§3.3), as the owner suggested |

## 3. The bio: five layers

### 3.1 First meeting (every named NPC that speaks)

What the NPC is, says and wants when the Tarnished first meets it; all of it passes the stranger test or is what the
NPC openly says of itself.

- **Seen** (public): `summary` (who it seems to be), `appearance` (what can be seen; Elden Ring's hoods and masks
  included), `role` (what it does: a smith, a merchant, a scholar, a knight sworn to a fallen house).
- **Self** (its own prompts): `personality`; `honesty` (truthful, guarded or liar), so that a disguise or a lie is
  played as one without the model knowing what it hides; `claims` (what it says of its past to a stranger, true or
  not); `seeks` (what it openly says it wants); `ties` (only the people it names at a first meeting).
- **Voice**: register (archaic, formal, plain, broken, chant), cadence, diction, forms of address, deflections, the
  cryptic turn many of the game's characters take: NPC §3.1's voice fields, unchanged.
- **Speaking up**: `subjects` (what it likes to talk about) and `stirs` (what makes it speak up unasked: a place, a
  name, a deed done near it, another character's words), for the scheduler's choice at hubs such as the Roundtable
  Hold, where many characters share a room.
- **Knows**: facts true at every state, and its lexicon.

### 3.2 Revealed (unlocked by verified story stages)

Later truths about its past, its aims and its ties, each behind a story stage EldenNet has verified in game (O48,
O51): `knows_when` entries for an NPC with one or two verified steps, a persona's full stage track (NPC §3.1) for one
with a mapped questline. Until its stage is verified, the truth is not in any prompt. Fates and later forms are never
written as text an NPC holds before they happen.

### 3.3 Companion (when it fights beside or travels with the Tarnished)

- `skills`: how it fights and what it brings (sorcery, a greatshield, a blade art), in our own words.
- `beside`: what it says and notices when it fights beside the Tarnished, its seeds for those moments, and how a shared
  victory or a fall is remembered.
- In v1.0 this layer serves Elden Ring's own summons: an NPC summoned to a boss fight speaks combat lines only, in its
  own voice (DESIGN §7.1 rule (c), O42), and remembers the fight (`FoughtBeside`). A follower system, characters who
  travel with the Tarnished, would use the same layer later (O85).

### 3.4 Lived (per playthrough)

What has passed between the NPC and the Tarnished, and with other NPCs: its feeling and trust towards them, what it has
learned, aims that changed. Built from its memories at quiet moments (MEM), cautiously, as SkyrimNet does: identity
rarely changes, aims and ties moderately, voice only after a sustained pattern. Stored with the playthrough, never in
the shipped file, and never holding anything the Tarnished has not reached, since it is made only from what happened.

### 3.5 The Tarnished (written by the player)

An optional bio of the player's own character, with the seen parts only: what others notice and how the Tarnished is
known. NPCs who meet the Tarnished see it as they see any stranger. Never shipped.

## 4. Who gets which layers

| Kind | First meeting | Revealed | Companion | Lived |
|---|---|---|---|---|
| A persona (six in v1.0) | Yes | Its stage track | If it can be summoned | Yes |
| A named NPC that speaks | Yes | `knows_when` once verified | If it can be summoned | Yes |
| A named boss never met calm, or a voiceless kind | Seen parts only (silent, listed in the roster) | — | — | — |
| A merchant or service without a story | Yes, short | — | — | Yes |

## 5. Size and caps

A speaking NPC's first-meeting layer: about 450 to 550 words, close to SkyrimNet's median of 570; revealed and companion
parts add what their stages and fights need. Each field keeps a cap, as O46's draft and NPC §3.1 set them; the file stays
within 16 KB and the prompt's cached prefix within its block caps (DESIGN §8.3).

## 6. What it changes

- O46's draft (DESIGN §7.8) gains `role`, `claims`, `seeks`, `ties` (first meeting), `stirs`, the companion layer and
  the lived layer; the persona schema (NPC §3.1) maps its `private.background`, `private.goals` and
  `private.relationships` onto the revealed layer of its base stage.
- The roster pipeline (DESIGN §7.8) drafts each field from our own research notes, as before; the spoiler review
  checks `claims`, `seeks` and `ties` at the opening state.
- MEM gains the lived layer's update job (S8.1.b).

## 8. After SkyrimNet 1.2.0 (2026-11-15)

Reading SkyrimNet 1.2.0 (WHY§97) confirmed the shape above and showed where SkyrimNet's own discipline slips. So:

- **Size.** SkyrimNet's prose is a median 512 words a bio (p10 430, p90 606), not 570; §5's 450 to 550 matches it.
- **Mechanical checks, not review alone.** About 3% of SkyrimNet's public summaries state the character's secret, and the
  summary is the most-copied text in the system. `summary`, `appearance`, `role`, `stirs` and `ties` are checked
  mechanically against the NPC's locked terms (the topics its research note names), then reviewed.
- **`claims` holds the cover story and its deflections.** A liar kept from the truth needs its lie written down, and
  how it turns a question aside, so that the lie holds without the model knowing what it hides.
- **The revealed layer follows SkyrimNet's world-knowledge pattern:** an entry with a condition, shown always or only
  when relevant, a shared key so that a later stage replaces an earlier one, and a span ("while the step runs" or "for
  good"). Its condition requires both the Tarnished's verified progress and that the NPC would know it.
- **Every background job gets the same layers as dialogue.** In SkyrimNet, memories, diaries and bio updates are
  written with the full bio, so a secret can enter a memory and return through recall for good. EldenNet's memory job,
  the lived layer's update and any summary job see only the layers unlocked for the journey.
- **No bio presumes the Tarnished's deeds** (100 of SkyrimNet's bios assume the player's), and generic cards are
  written without assuming a gender unless the kind has one.
- **The lived layer is per journey:** a new cycle starts a new journey, and earlier journeys are read-only and never
  injected (DESIGN §10.5).

## 7. Questions for the owner

### O46 (revised): the bio's structure

(a) **Recommended.** The five layers above: first meeting, revealed, companion, lived, and the Tarnished's own. (b)
O46's draft as it stands: no claims, aims or ties for roster NPCs, no companion or lived layer. (c) SkyrimNet's ten parts
as they are, secrets included.

### O85: A follower system?

(a) **Recommended.** Later, after v1.0, when actions open: characters who travel with the Tarnished; the bio carries the
companion layer from v1.0, so followers need no change to it. (b) Designed now, for a v1 release. (c) None: summons only.
