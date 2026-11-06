# EldenNet

An LLM-driven NPC framework for ELDEN RING on PC, for offline play.

**Status: design stage.** There is no code yet. The v1 design is under discussion on the `design/v1-proposal`
branch.

## What v1 is meant to do

- **Named NPCs** (questline characters and named merchants) get a personality, an awareness of where they are
  and what is happening, and memories of earlier conversations. The player can hold a free-form text
  conversation with them.
- **Generic NPCs** (soldiers, militia, wanderers) know what they are, where they are and what is happening in a
  fight, and say short lines.
- **Dialogue only.** NPCs take no other actions in v1.

v1 is deliberately small and stable: it reads game memory, never writes it, and patches no game code. It is
dormant unless the game runs offline, without anti-cheat, with the player alone in their own world.

## Design

- [docs/design/DESIGN.md](docs/design/DESIGN.md): the master design. §0 is the design on one page, and §19
  lists the decisions still open.
- [docs/design/architecture.md](docs/design/architecture.md), [npcs-and-dialogue.md](docs/design/npcs-and-dialogue.md)
  and [memory-and-knowledge.md](docs/design/memory-and-knowledge.md): the detail documents.
- [docs/research/](docs/research/README.md): the research the design rests on.
- [docs/design/process/](docs/design/process/): the competing proposals, the judges' scorecards and the review.

## Ground rules

- EldenNet is a standalone native DLL with its own launcher. It depends on no mod loader, but a player may load
  it from a loader they already use.
- It ships no FromSoftware files, text, params or audio. Game text is read at runtime from the player's own
  install.
- All personas, prompts and example content are original.
- The project is developed with AI assistance.

## Tools for contributors

- [tools/privsweep](tools/privsweep/README.md): the privacy and safety sweep, run before every push.
- [tools/workplan](tools/workplan/README.md): `wp`, which reads the design and research documents section by
  section.

## Licence

[MIT](LICENSE). ELDEN RING is a trademark of FromSoftware and Bandai Namco. EldenNet is not affiliated with or
endorsed by them.
