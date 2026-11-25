# Licences and credits

EldenNet is free software. This page lists what it is made of, under which licence, and whom it credits. The full
text of every licence is in the files that come with EldenNet, beside `eldennet.dll`.

## EldenNet

EldenNet's own code, its character profiles, its instructions to the model and these documents are its own work,
under the MIT License. The full text is in the `LICENSE` file.

## What EldenNet contains

| Part | Version | Licence | What it does in EldenNet |
|---|---|---|---|
| inja | 3.4.0 | MIT | Fills in the instructions sent to the model |
| nlohmann/json | 3.11.3 | MIT | Reads and writes JSON |
| toml++ | 3.4.0 | MIT | Reads EldenNet's files and yours |
| Dear ImGui (its core alone) | 1.93.0 | MIT | Draws the developer view |
| GraceUI's connection files | GraceUI 0.2.52 | MIT | Connect EldenNet to GraceUI |

Each Windows program of EldenNet also contains Microsoft's C runtime, under Microsoft's terms for its redistribution.
EldenNet uses Windows's own HTTP library for its requests, and ships no copy of it. Its checks of saves and of
GraceUI's files use hashes written by EldenNet itself, from their published specifications. Each licence was read
before its part was taken in.

## GraceUI

GraceUI is a mod of its own, under the MIT License, and EldenNet carries its release. Its folder holds its own licence
and notice files, which name the libraries and fonts it contains.

## Design credit

EldenNet's engine follows the logic and structure of SkyrimNet, whose author, Min, permits EldenNet to build on it.
EldenNet is not SkyrimNet, and contains none of SkyrimNet's code, prompts or content.

## The game

ELDEN RING is a trademark of FromSoftware and Bandai Namco. EldenNet is not affiliated with or endorsed by them.
EldenNet contains none of the game's files, text, sound or images: it reads the game's own text from your install
while you play, and stores none of it.

## AI assistance

EldenNet is developed with AI assistance. Characters' lines in game are written by the AI model you choose, as you
play: see [About AI in EldenNet](doc:provider#About AI in EldenNet).

## See also

[About](doc:about) · [What is sent to your provider](doc:provider)
