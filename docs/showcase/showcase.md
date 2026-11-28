<!--
EldenNet's showcase: what EldenNet is, the features designed so far and its menu (GraceUI's
mock), in plain words for the owner and future players. This file is the one source of the page
docs/showcase/index.html; tools/showcase/build.py builds it, with the screenshots in
docs/showcase/img/. Update it as the project moves: edit this file, change "updated", rebuild,
and commit the source, the page and the screenshots together.

  # EldenNet                          the page's title, once, then the page's fields:
  - updated: 2026-11-07               the "last updated" date, YYYY-MM-DD
  - lede: one or two sentences        under the title
  - status: a few words               the status pill at the top
  - facts: A · B · C                  short facts under the lede, separated by " · "
  - roadmap-published: none           the roadmap page's published address once it has one; the
                                      published showcase links there (this page links ../roadmap/)

  ## What it is {#about}              a section: its title, then an anchor id in braces
  - kind: prose                       prose (the default), cards, gallery or status
  - nav: About                        a shorter name for the band's links (else the title)

  ### A feature                       under cards, a card; under gallery, a screenshot;
                                      under prose or status, a subheading
  - tag: Conversation                 a card's small label (required)
  - design: DESIGN§8, O45             a card's design references: wp codes (DESIGN§8.9, NPC§4),
                                      the owner's decisions (O1 on), GraceUI:DESIGN§n, G1 on, paths
  - image: home.webp                  a screenshot's file in docs/showcase/img/, 1280x720 (required)
  - from: index-1280x720.png          the GraceUI mock shot it is made from (see below)
  - alt: text                         a screenshot's alternative text (else made from its title)

Fields come straight after their heading. The rest is Markdown: paragraphs, "- " lists, **bold**,
*italic*, `code` and [links](https://...). A status section's prose is followed by the roadmap's
stages, read from docs/ROADMAP.md, so rebuild this page whenever a stage's status changes there.

No game text, no character or place names of the game (the screenshots' names are invented), and
nothing personal, no local paths and no addresses: the page is published.

Build: python3 -I tools/showcase/build.py
Check: the same with --check, which also fails when the page is no longer what the source builds.
New screenshots: shoot GraceUI's mock with its own script (mock/build/shoot.js in GraceUI) into a
folder outside both repositories, then build with --images-from FOLDER, which writes each
screenshot named here from its "from" file as lossless WebP (needs Pillow).
Tests: python3 -I -m unittest discover -s tools/showcase
Publish: build with --fragment FILE (the screenshots inlined), sweep FILE (privsweep files FILE),
then publish it to the showcase's page on claude.ai, at the same address every time, kept private:
https://claude.ai/artifact/Hq7hkTZyyvzwyvCQzZgvYa (private; first published 2026-11-07;
slug 884a0cd5-2a74-4287-a29b-a99bc5ce0693).
-->

# EldenNet

- updated: 2026-11-24
- lede: A mod for Elden Ring on PC, for offline play, that lets the game's characters talk with you in their own words, remember you, and know what is going on around them.
- status: Testing in game: reading the world
- facts: Offline, solo play · Dialogue only in version 1 · A cloud model by default, or your own · Spoiler-safe by design · Never writes to your game
- roadmap-published: https://claude.ai/artifact/1HQVhkTsKhgR1EdkqUijxk

## What it is {#about}

- nav: About

EldenNet gives the characters of Elden Ring a mind of their own, through a large language model that
you choose: a cloud service with your own key, or a model running on your own computer. Named
characters have a personality, know where they are and what is happening around them, remember you
and each other, and can hold a free conversation with you in typed text. Everyone else with a voice,
such as soldiers and wanderers, knows what they are and where they are, and says short lines in a
fight and around it.

Version 1 is deliberately small and stable. Characters speak and do nothing else: no actions, no
gestures, nothing changed in your game. EldenNet reads the game and never writes to it, and it stays
dormant unless you play offline, alone in your own world.

EldenNet builds on SkyrimNet, Min's AI framework for the characters of Skyrim, with Min's approval.
It is a project of its own, not SkyrimNet itself. Its menu and dashboard are drawn by GraceUI, a
UI framework of its own that other mods will be able to use too, and every EldenNet release
carries it.

## What version 1 does {#v1}

- nav: Version 1

- **Talk with any named character** the game lets you meet calmly: press the talk key, type, and
  they answer in character.
- **A profile for every named character and named boss** of the base game, about 140 in all, written
  by us and reviewed by hand for spoilers.
- **They remember** you, and the other characters they deal with, from one meeting to the next.
- **They know where things stand**: their own story, the stories tied to it, and the state of the
  land, as the game itself records them.
- **They speak up on their own**: greetings, quest business, news, remarks and exchanges between
  characters, held back only by the pacing you set.
- **Fights have voices**: lines for every fighter on either side, boss fights included, never over
  the game's own lines.
- **Your model, your choice**: a cloud service by default, with ready presets for OpenRouter and
  NanoGPT, or a model server on your own computer; any service that speaks the common
  OpenAI-compatible protocol works, and each use, from conversation to memory, can have its own.
- **One menu, in game and in your browser**: GraceUI opens on a hotkey in game, and in a browser on
  the same PC while the game runs.
- **Text first.** Spoken voices and talking aloud come in version 1.5; actions and gestures in a
  later version.

## Features {#features}

- kind: cards

What the design holds today. Each card says where the design describes it, for anyone who wants the
detail.

### Conversation with every profiled character

- tag: Conversation
- design: DESIGN§8, O45

Walk up to a character the game lets you meet calmly, press the talk key and type. They answer in
their own manner, in short pages paced for reading. The game never pauses: if danger comes, the chat
box closes and hands your controls back at once. EldenNet never talks over the game's own dialogue,
and a line counts as said only once you have seen it.

### Memory of you and of each other

- tag: Memory
- design: DESIGN§10.2, DESIGN§10.5, O45, O144

Every character with a profile keeps a short journal: who you are to them, and a handful of
keepsakes about you and about the other characters they deal with. How they feel about you moves a
little with each conversation and is shown in words, never as a number. Memories follow your save:
loading an earlier one rolls them back with it, asking you first when something that matters would
be forgotten, and each new cycle of the game starts a journey of its own.

### Character profiles and the roster

- tag: Profiles
- design: DESIGN§7.7, DESIGN§7.8, O43, O46

Profiles come at three depths. Every named character and named boss gets a bio of our own, in layers: what a first
meeting shows, what later steps of its story reveal once the game confirms them, how it fights beside you if the game
lets you summon it, and what has passed between you on this journey. The characters with the longest stories follow
theirs step by step, and soldiers, wanderers and their like get a card for their kind. The whole base-game roster
ships with version 1.0, the expansion's with version 1.1. You can add your own instructions to any profile, write one
of your own for a character you have met, or give your own character a short bio that others see as a stranger would.

### Quest and world awareness

- tag: Awareness
- design: DESIGN§10.6, O51

Important characters know where their own quest stands, the quests tied to it, and the stage the
world is in, read from the game's own records once each has been verified. They know only what they
could have learnt: what happened where they stand, what the whole land saw, and word that travels
along the road. News can come up in conversation when it touches them; what they have not heard of
stays out of their mouths.

### Speaking up, within pacing

- tag: Speaking up
- design: DESIGN§8.9, DESIGN§9.4, O31, O39

Characters start lines themselves when they have a reason: a greeting when you come near, quest
business, news, a remark, an answer to someone else, or a subject they like to talk about. There is
no quota of lines. What holds them back is the pacing you set (a gap between lines, a window, a
cooldown for each kind of line), what each type of character may do, and safe moments.

### Fights and status effects

- tag: Combat
- design: DESIGN§9.2, DESIGN§9.9, DESIGN§9.10, O40, O42

Every fighter can speak, on your side or against you, and so can characters fighting each other,
boss fights included; a line never plays over the game's own scripted lines. Characters also know
their own status effects and see each other's, and react in words: a warning, a plea for help, fear.
Each status joins once its signal in the game has been verified.

### Characters talk to each other

- tag: Exchanges
- design: DESIGN§9.7, O34

Two characters on the same side who meet can greet each other and trade a few lines, close enough
for you to overhear. Characters also answer what others say near them, so a camp or a crossing feels
lived in without you at the centre of it.

### Everyone with a voice knows what they are

- tag: Generic characters
- design: DESIGN§7.4, DESIGN§9.3

Soldiers, militia and wanderers know what they are, where they are and what is happening in a fight,
and say short lines in character. Their lines start from written seeds that need no network at all,
widened by lines prepared ahead in the background, so a fight never waits on a model.

### Spoilers kept out by what is said, not by bans

- tag: Spoilers
- design: DESIGN§10.4, O36, O86

Each character is told the present as it stands for you, in plain positive terms: what it knows, what it is doing and
its own part in its story at this point of your journey, rather than a list of things not to mention, which would only
put them in mind. Its memories are written from the same facts, and a few test questions for each character check what
the model itself already knows of the game. A record that has not been verified can hide something but never reveal
it. The menu keeps the same rule: nothing you have not reached is shown, counted or hinted at.

### An event engine of its own

- tag: Events
- design: DESIGN§6, O49

Elden Ring has no event system a mod can listen to, so EldenNet has its own, Augur. Four times a
second it compares what it sees of the world, watches the game's records, and turns what changed
into events (someone came near, a fight began, a character fell), each with the list of those who
could perceive it. Every reaction, memory and line starts from those events, and recorded play
replays through Augur in the tests.

### Gentle on your game

- tag: Stability
- design: DESIGN§3, DESIGN§4.4, O47, O48

EldenNet is one small native library with its own launcher. It needs no mod loader, though you may
load it through one you already use. It changes no game code, writes nothing to the game's memory
and never touches your save file; its launcher keeps backups of your saves all the same. An unknown
game version keeps EldenNet asleep, and a check that fails at start turns the part concerned off
with a notice, instead of guessing.

## Privacy and safety {#privacy}

- nav: Privacy

- **Offline only.** EldenNet works only while you play offline, alone in your own world, with no
  anti-cheat running; otherwise it stays dormant.
- **Your conversations stay yours.** Nothing is sent anywhere but to the model service you chose;
  what EldenNet measures stays on your PC and is never uploaded.
- **Keys go in and never come out.** A key you enter is never shown again, never written to a log
  and never put in a bug report.
- **The browser dashboard is for this PC.** It answers this PC only, unless you switch on your home
  network and pair a device with a code shown in game, and it refuses requests from other web pages.
- **Nothing personal in reports.** Logs and bug reports leave out your keys, your name, your folders
  and your addresses.
- **The game's own files stay the game's.** EldenNet ships no game text, art or audio; it reads what
  it needs from your own install while you play.
- **Built openly with AI assistance.**

## The menu: GraceUI {#menu}

- kind: gallery
- nav: The menu

GraceUI draws EldenNet's menu and dashboard: the same pages in game on a hotkey, and in your browser
on the same PC while the game runs. It wears its own dark theme, Grace: warm ash greys lit by a single
gold light. These are screenshots of the design mock, with placeholder data and invented names; the
menu itself is built alongside EldenNet's versions, starting with the overlay that draws it in game.

### Home

- image: home.webp
- from: index-1280x720.png

Whether EldenNet is working, who is near you, what is happening now and what was just asked of the
model. The setup card holds your model, voice and microphone, and closes itself once the self-test
has passed.

### Named NPCs

- image: named-npcs.webp
- from: characters-named-1280x720.png

The roster: every named character, listed from the start with only what a first meeting would show,
and those you have not met under the description you would know them by. Each has a general
profile, their way with news, and what they may do.

### Playthrough

- image: playthrough.webp
- from: characters-playthrough-1280x720.png

The people you have met on this journey, how they are now and their stories as you last saw them,
with the latest change in the world.

### Memories

- image: memories.webp
- from: history-memories-1280x720.png

What each character remembers of you: their journal, how they feel about you, in words, and the
keepsakes behind it, with the means to make one character, or the whole journey, forget.

### Speaking up

- image: speaking-up.webp
- from: behaviour-speaking-1280x720.png

Who may speak unasked, and on which occasions: greetings, quest business, news, exchanges between
characters and the subjects they like.

### Pacing

- image: pacing.webp
- from: behaviour-pacing-1280x720.png

How often anyone speaks unasked and how far lines reach, with three presets and a summary in plain
words. Replies to you are never held back.

### NPC types

- image: npc-types.webp
- from: characters-types-1280x720.png

Every type of character, and what each may do: speak, start lines unasked, talk in battle.
Allowances only ever narrow what a type may do; the safety and spoiler rules always apply.

### Providers

- image: providers.webp
- from: ai-providers-1280x720.png

Where the model calls go: a cloud service with your own key, or a model server on your own computer.
A key goes in and is never shown again. Since this mock was made, each use (conversation, call-outs,
memory and the rest) can name a provider of its own, "same as conversation" until you change it
(decision O88).

## In game, on a test backdrop {#preview}

- kind: gallery
- nav: In game

What EldenNet draws in game today, through GraceUI's overlay, shot from our own test program instead of the game: a
dark backdrop of our own takes the game's place, and the character, Lantern-Keeper, is invented for the tests. Each
shot was taken at 1920 by 1080 in the game's own colour format and scaled down. None of these is the game.

### Speaking to a character

- image: preview-chat.webp
- alt: The chat box at the foot of the screen, a line being typed to an invented character, on a test backdrop

Press the talk key near a character and the box opens: it names whom you speak to and who will hear you, and you type
your line. On a test backdrop, not the game.

### The answer

- image: preview-lower-third.webp
- alt: An invented character's answer under their name in the lower third, and the box below it, on a test backdrop

The answer comes in the lower third, under the speaker's name, and the box keeps the exchange: your lines in italic,
theirs upright. On a test backdrop, not the game.

### Who you can speak to

- image: preview-hint.webp
- alt: A small gold marker over the line "T Speak" and an invented character's name, on a test backdrop

A small marker and one line show whom you can speak to, and with which key. On a test backdrop, not the game.

### The menu, on F5

- image: preview-menu-ring.webp
- alt: EldenNet's Home page in GraceUI's menu, a gold ring on the Logs row, over a test program

EldenNet's pages in GraceUI's menu: Home says whether EldenNet is working, and why not when it is not. Moving with the
keys shows a gold ring on the row in focus; with the mouse it stays hidden. The game build shown is the test
program's invented number. On a test backdrop, not the game.

### GraceUI's own pages

- image: preview-graceui.webp
- alt: GraceUI's Menu page, with the menu's size, theme and comfort settings, over a test program

GraceUI's own settings sit beside every mod's pages in the same menu: here the menu's size, its theme and the comfort
options. On a test backdrop, not the game.

### The developer view, on F11

- image: preview-f11.webp
- alt: EldenNet's developer view at the top left, listing its reads and the events it has seen, on a test backdrop

For testers: what EldenNet reads, how each part is faring and the events it has seen. Here it shows the test
program's scripted run, which ends back at the title screen, not a real session. On a test backdrop, not the game.

## Where it stands {#status}

- kind: status
- nav: Status

**Testing in game: EldenNet now reads the world.** The design of version 1 is done, and so is the design of GraceUI,
which is becoming a UI framework other mods can use too. EldenNet has an engine of its own, open source, built on
SkyrimNet's proven logic and structure (decision O84): the prompt templates, the client that talks to your model, and
the calls in which the model picks one of the options code gives it. It builds for Windows the way the release will,
and is tested under Wine, with sanitizers and fuzzing, against a stand-in game of our own.

Inside the game, a series of short read-only rounds on the owner's PC has found where the game keeps what EldenNet
needs, by EldenNet's own checks, with nothing taken from other modding projects. In the latest round EldenNet's own
reader read the game as the finished mod will, once a second: which save is loaded, your character, its health and
its maximum (checked against the game's own status screen), whether you are moving, and the characters around you,
nearest first, with no fault. It only reads: it calls nothing in the game and changes nothing in it.

GraceUI's overlay runs in the game too. Test runs beside other overlays have checked its menu and pointer, the gold
ring that shows where the keyboard is, and that nothing you type in its panels reaches the game; its own heap guard
and its input counters watch every run. EldenNet's launcher starts the game offline, backs up your saves, keeps a
short record if the game ends badly, and says nothing when the game's own error at quitting comes after its window
has closed.

Next: the rest of what EldenNet reads about each character and the world (their kind, their side, whether they are
dead or on screen, your level and the area you are in), then v0.1 *Eyes*: EldenNet starting fully in your game, its
debug view showing what happens around you as events. Then v0.2 *First words*: the first conversation, with the
chat box and the subtitles already built and tested against the stand-in game. GraceUI's interactive map of your
journey is designed for version 1.1, drawn at run time from your own game.

## Credits {#credits}

- EldenNet builds on SkyrimNet, by Min, with Min's approval. It is its own project, not SkyrimNet
  itself.
- The Grace theme is our own. Its fonts are free fonts under the SIL Open Font License (Cinzel,
  EB Garamond and Noto Sans); this page uses them only if your computer has them.
- ELDEN RING is a trademark of FromSoftware and Bandai Namco. EldenNet is not affiliated with or
  endorsed by them. This page uses no game art or text, and names none of the game's characters or
  places: every name in the screenshots is invented.
- EldenNet and GraceUI are open source under the MIT licence.
