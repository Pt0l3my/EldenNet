# EldenNet events: what exists, what we added, what I would add (2026-11-07)

An **event** is something EldenNet notices in the game, such as a fight starting, the Tarnished dying or a named
NPC coming near. Some events are only remembered (they feed memory and awareness), some only start or stop other
things, and some are **reactions**: they can make an NPC say something. For every reaction you get a setting (NPCs
react to it or not) and a cooldown (how long before the same reaction can happen again). The only other limits are
pacing (time between lines, lines per minute) and what each NPC type is allowed to do.

"Reaction setting" below means you can switch NPCs' reaction to that event on or off and set its cooldown.

## 1. Events that already existed

### The world

| Event | In plain words | Reaction setting? | Note |
|---|---|---|---|
| Place entered | The Tarnished reaches a new named place | Not yet | Later: a companion could remark on arrival |
| Rested at a grace / stood up | The Tarnished sits at a grace, or gets up | No | Used for safe moments and background work |
| Grace lit | A new grace is lit | Not yet | Later: a remark |
| Flag changed | A game flag we track changes | No | Plumbing: quest stages and the world's stage are read from flags |
| Boss fight started / ended | A boss health bar appears or goes (victory, death or leaving) | Yes, the end: "remarks on a great foe's fall it saw" | Boss fights also decide when lines may play (section 4) |

### Named NPCs (personas)

| Event | In plain words | Reaction setting? | Note |
|---|---|---|---|
| Named NPC seen | A named NPC is within 10 m for 2 s | Not yet | Counted in memory ("you have seen this Tarnished pass by") |
| Named NPC approached | The Tarnished comes within 6 m | No | Warms up a local model; now re-arms beyond 10 m |
| Named NPC attacked | The Tarnished hits a named NPC | Not yet | The game plays its own "attacked" lines here, so we wait for the talk-observer spike first |
| Named NPC died | A named NPC dies | Not yet | Remembered by witnesses |
| Fought beside | A named NPC was summoned at a boss's fall | No | Remembered |
| Native talk started / ended | The game's own conversation with a named NPC | No | Our lines always give way to it; it also tells us when the Tarnished has spoken with an NPC |

### The Tarnished

| Event | In plain words | Reaction setting? | Note |
|---|---|---|---|
| The Tarnished died | Death, or the death banner | Yes: "gloats as the Tarnished falls" | Only this line may play in the 1.2 s after a death |
| Healed | Someone heals 20% or more in a second, in a fight | Yes: "a foe heals" | Now covers NPCs too, not only the Tarnished |
| Mounted or dismounted | Torrent is summoned or dismissed | Not yet | Later: a remark |

### Fights

| Event | In plain words | Reaction setting? | Note |
|---|---|---|---|
| Struck | A hit lands; "hard" if it takes 30% of health | Yes: "takes a heavy blow" | Now any attacker and any victim, NPC against NPC included |
| Badly hurt | Health drops below 25% | Yes: "badly hurt" | Re-arms above 35% |
| Engaged / disengaged | Two sides start or stop fighting; "fled" if one runs off | Yes: "enters a fight", "a foe breaks away" | Now any pair: the Tarnished and an enemy, or two NPCs on hostile sides |
| Ally died | Someone on an NPC's side falls | Yes: "sees someone on its side fall" | Now any side, the Tarnished's side included |
| Critical hit | The Tarnished lands a riposte or backstab | Not yet | Later: an ally's remark |
| Killed near | The Tarnished kills an enemy near a named NPC | Yes: "fells a foe" (for allies in the fight), "remarks on a kill it saw" (for a named NPC watching) | — |
| Idle near | Every 30 s, a calm NPC near the Tarnished may remark | Yes: "idle remark" | Pacing keeps it sparse (2 a minute by default) |

### Speech and conversations

| Event | In plain words | Reaction setting? | Note |
|---|---|---|---|
| The Tarnished said | A line the player typed | No | Always answered by the NPC addressed |
| NPC said | A line an NPC said | Yes: "answers a line it heard" | Now every NPC line, not only conversation lines, so NPCs can answer each other |
| Call-out sent / replied | The player calls out to a soldier at range | No | A reply, not a reaction; the player may now call again |
| Conversation started / ended | Our own chat with a named NPC | No | It now records whether the NPC opened it by speaking first |

### Start-up, loading and safety

Data ready, playthrough ready, loading started and ended, title screen, a new journey (NG+), the multiplayer gate,
a read group switching off, the safety ladder. None of these speaks and none is a reaction; loading and the gate
silence everything.

## 2. What the quest and world-awareness work added

No new event. It added records to the end of each conversation (what the NPC knew) and three memory facts (what each
NPC has heard of the world, when, and which news it has raised), and the **news reaction**: a named NPC may bring up
news on its own when it touches its own affairs, when the Tarnished told it of the deed, or (for news-hungry NPCs)
when it is the talk of its own district. Which of these count is still your open question O30.

## 3. What this change adds

| Event | In plain words | Reaction setting? | Note |
|---|---|---|---|
| Came near | Two characters come within talking range (8 m by default) | Yes: "greets the Tarnished", "greets another NPC", and quest business | The greeting and quest-business motives start here |
| Autonomy tick | Every 2 minutes (your setting), each NPC in range may be asked to speak | Yes: "speaks up on its own" | An NPC is only asked when pacing would let a line through, so calls stay bounded |
| Stage changed | A named NPC's quest stage moves on | No | Lets us tell whether the Tarnished has spoken with the NPC since its stage changed |
| Foe killed | An NPC kills another NPC | Yes: "fells a foe", "sees someone on its side fall" | Fights between NPCs, for example a soldier and your spirit summon |
| Status started / ended / burst | Frostbite, poison, rot, sleep (lasting); bleeding, madness, death blight (bursts) | Yes: "suffers a status", "asks for help", "warns someone", "remarks on a foe's status" | Live only once the new status spike verifies each one; spoken only in v1 |
| Scripted line heard | The game itself is voicing a line for an NPC | No | Our lines wait or fade; in boss fights this is what lets combat lines play safely |
| Quest business | A named NPC has something for the Tarnished at its current stage | Yes: "quest business" | Only for stages the flag spike has verified; the stage still moves only through the game's own talk |

## 4. All reactions you can set, with their defaults

| Reaction | Default | Cooldown | Counted per |
|---|---|---|---|
| Enters a fight | on | 20 s | fighting group |
| Takes a heavy blow | on | 20 s | NPC |
| Badly hurt | on | 60 s | NPC |
| Sees someone on its side fall | on | 20 s | fighting group |
| Fells a foe | on | 20 s | NPC |
| Gloats as the Tarnished falls | on | none | — |
| A foe breaks away | on | 30 s | NPC |
| A foe heals | on | 30 s | NPC |
| Suffers a status | on | 30 s | NPC and status |
| Asks its side for help with a status | on | 45 s | NPC |
| Warns someone of a status | on | 20 s | NPC and the one warned |
| Remarks on a foe's status | on | 20 s | NPC and the foe |
| Idle remark | on | 90 s | NPC |
| Greets the Tarnished | on | 10 min | NPC |
| Greets another NPC | on | 5 min | the pair |
| Answers a line it heard | on | 20 s | NPC |
| Quest business | on | 5 min | named NPC and stage |
| Raises news | on | 3 h of play | named NPC and piece of news |
| Remarks on a kill it saw | on | 60 s | named NPC |
| Remarks on a great foe's fall it saw | on | 10 min | named NPC |
| Speaks up on its own (autonomy) | on | 2 min (the autonomy period) | NPC |

Pacing defaults: 2.5 s between any two lines (8 s between two lines of one fighting group); per minute at most 6
lines in a fight, 2 idle remarks and 4 lines of talk (greetings, quest business, exchanges, autonomy). Combat
dialogue is on for every type, boss fights included, and plays only when the NPC is not voicing a scripted line.

## 5. What I would add later

- **An arrival remark** when a companion reaches a new place, and a remark at a grace.
- **A remark about Torrent** when the Tarnished mounts up near a named NPC.
- **An ally's cheer on a critical hit.**
- **A witness's words when a named NPC dies.**
- **Reactions to attacking a named NPC**, once we can see how they sit beside the game's own attacked lines.
- **A glance when a named NPC first sees the Tarnished.**
- **An ally's warning when the Tarnished is badly hurt.**
- **A warning as a status builds up**, before it takes hold (the gauge, not the effect).
- **Acts**: fleeing, healing and gestures, through the action seams in a later version (your decision: none in v1).
- **Voices for bosses, invaders and creatures** (open question O43; I recommend after v1.0).
