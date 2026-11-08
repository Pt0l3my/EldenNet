# EldenNet

An LLM-driven NPC framework for ELDEN RING on PC, for offline play.

**Status: design stage.** There is no code yet. The v1 design is under discussion on the `design/v1-proposal`
branch.

## What v1 is meant to do

- **Named NPCs and named bosses** each get a character bio of their own, written by us: a personality, an awareness of
  where they are and what is happening, and memories of the player and of the other NPCs they deal with. The player
  can hold a free-form text conversation with any of them whom the game lets them meet calmly.
- **Generic NPCs** (soldiers, militia, wanderers) know what they are, where they are and what is happening in a
  fight, and say short lines.
- **NPCs speak up on their own:** named NPCs greet the Tarnished and call them over when they have something
  for them, NPCs talk among themselves, and NPCs with a voice (named NPCs, soldiers, militia, wanderers) react
  in words to fights and, once the game's signals for them are verified, to status effects, limited only by
  pacing and per-type allowances that the player sets.
- **Dialogue only.** NPCs take no other actions in v1.

v1 is deliberately small and stable: it reads game memory, never writes it, and patches no game code. It is
dormant unless the game runs offline, without anti-cheat, with the player alone in their own world.

## Design

- [docs/design/DESIGN.md](docs/design/DESIGN.md): the master design. §0 is the design on one page, and §19
  indexes the owner's decisions and any question still open.
- [docs/DECISIONS.md](docs/DECISIONS.md): the owner's decisions in full, one entry each (O1 onwards).
- [docs/ROADMAP.md](docs/ROADMAP.md): the one roadmap and open-items list for EldenNet and GraceUI, with its page in
  docs/roadmap/.
- [docs/showcase/](docs/showcase/showcase.md): what EldenNet is, and its menu's mock, for players.
- [docs/design/architecture.md](docs/design/architecture.md), [npcs-and-dialogue.md](docs/design/npcs-and-dialogue.md)
  and [memory-and-knowledge.md](docs/design/memory-and-knowledge.md): the detail documents.
- [docs/design/game-data.md](docs/design/game-data.md): the three kinds of game data (research, the framework's data,
  live tracing) and the map of what each feature reads in play (O57).
- [docs/research/](docs/research/README.md): the research the design rests on.
- [docs/design/process/](docs/design/process/): the competing proposals, the judges' scorecards and the review.

## Ground rules

- EldenNet is a standalone native DLL with its own launcher. It depends on no mod loader, but a player may load
  it from a loader they already use.
- It ships no FromSoftware files, text, params or audio. Game text is read at runtime from the player's own
  install.
- Built on SkyrimNet with its author's approval, credited as its author wishes. Our own personas, prompts and example
  content are original.
- The project is developed with AI assistance.

## Tools for contributors

- [tools/privsweep](tools/privsweep/README.md): the privacy and safety sweep, run before every push.
- [tools/workplan](tools/workplan/README.md): `wp`, which reads the design and research documents section by
  section.
- [tools/gamedata](tools/gamedata/README.md): research tools the owner runs on their own Smithbox exports; they
  print numbers and counts only, never game text.

## Licence

[MIT](LICENSE). ELDEN RING is a trademark of FromSoftware and Bandai Namco. EldenNet is not affiliated with or
endorsed by them.
