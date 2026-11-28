# What is sent to your provider

EldenNet's characters speak through an AI model that you choose, at a provider that you set up. This page says
exactly what leaves this PC, where it goes and when, and what never leaves it.

## Where your provider can be

| Kind of provider | Where your requests go | Key |
|---|---|---|
| A server on this PC | Nowhere: they stay on this PC | None, unless that server asks for one |
| A server on another PC at home | To that PC alone, by its private address, unencrypted | Never sent |
| A cloud provider | Over the internet to the provider's address, encrypted | Your key for that provider |

EldenNet sends requests only to the providers listed on **AI › Providers**, which are the ones in your providers
file. Each job on **AI › Models** names the provider it uses. A cloud provider's address must start with `https://`,
and EldenNet refuses a providers file that would send a key unencrypted to another computer.

## When EldenNet calls a model

In this version, only for these:

- **Conversation**: each line you send to a character.
- **Profile drafts**: each time you press Draft with AI on Characters › Named NPCs.
- **Self-test**: each time you press Run self-test. It also uses Memory's model, for one check.
- **Model lists**: each time you press List models, and during the self-test.

Quick lines, Prepared lines and Memory have cards on AI › Models, but they send nothing in this version, apart from the
self-test's one check. Every call waits until EldenNet has confirmed that this session is offline and solo; at the
title screen only the self-test and the model lists may go, and while you play with others online, nothing does.

## What a conversation sends

Each line you send makes one request to Conversation's model. EldenNet may send it a second time, once, if the first
reply breaks EldenNet's own rules for a reply. When a cloud provider has given no text a few seconds after a request,
the same request goes to the job's fallback as well, which may be at another provider. Each request holds:

- EldenNet's own instructions, the same for every character: how people speak in the game's world, and how to answer.
- The character's name, as the game shows it.
- The character's profile, as EldenNet wrote it: who they seem to be, their temperament, what they say of themselves,
  how they speak, and what they know at this point of your journey.
- Your own notes on the character, if you wrote some.
- This conversation so far: the lines you typed, and the character's lines that were shown to you.
- The last lines of your previous conversation with this character, in the same session.
- The time of day in the game, and how hurt the character is and whether you struck them in the last few seconds.
  With `features.self_awareness = false` in your settings file, the character's wounds and the fight are left out.
- The request's own settings: the model's name, a length limit for the reply and similar settings for the model,
  and any extra headers or settings your providers file names for that provider.

A request never holds your character's name, your Steam name or number, your Windows user name, a path or a file of
this PC, a save, any of the game's files, a picture or a sound, another provider's key, or EldenNet's logs.

## What a memory sends

After a conversation with at least two of your lines, EldenNet asks the memory job's model to write what that person
remembers: it sends what they saw and heard since their last memory, and that memory, and nothing else of your files.
What comes back is kept in your character's file in saves.

## What a profile draft sends

Only when you press Draft with AI: your rough notes (400 characters at most), a few words of EldenNet's naming the
kind of profile, and the notes and subjects you have already written for that character. It never holds the
character's private profile, the game's text, or a name you did not type yourself.

## What the self-test and the model lists send

The self-test sends two short requests, fixed words of EldenNet's own, and measures how quickly and in what form the
model answers; it reads the answers and keeps nothing of them. A model list asks a provider which models it serves. The
names it answers with fill the pickers on AI › Models and go nowhere else.

## Your keys

- A key is sent to its own provider alone, in each request's header, and never anywhere else.
- It is kept in `%APPDATA%\EldenNet\config\secrets.toml`, which only your Windows user may read, or in an environment
  variable that your providers file names.
- Once pasted, it is never shown again: the pages say only whether a key is set, refused or not needed. No log, page,
  report, recording or crash record holds it.
- To change a key, paste the new one on its row on AI › Providers. To remove it, remove the provider: its key goes
  with it.
- A device you paired at home can never set or change a key.

## What your provider sees

Like any service on the internet, a cloud provider sees the network address your connection comes from, and keeps
what its own terms allow. Read your provider's terms before you use it. EldenNet cannot see or delete what a provider
keeps.

> **Note.** What you type goes to your provider. Do not type anything into a conversation that you would not send to
> that service: your name, an address, a password or anything else about you.

## What never leaves this PC

- EldenNet has no telemetry, usage statistics, crash uploads or update checks. Its only traffic is the requests above,
  to the providers you set up.
- Crash records, logs, recordings and save backups stay on this PC.
- The menu in your browser loads nothing from outside this PC.

## About AI in EldenNet

- Characters' lines are written by an AI model as you play. They are not written by FromSoftware, and they can be
  wrong.
- The character profiles and EldenNet's instructions to the model are EldenNet's own writing, never the game's text.
- EldenNet itself is developed with AI assistance.

## See also

[Privacy](doc:privacy) · [Spoilers: how EldenNet keeps them](doc:spoilers) · [Getting started](doc:getting-started)
