# Getting started

EldenNet lets characters of the game hold a conversation with you. You type a line; the character answers in their
own voice, in text on screen, written by an AI model that you choose. This page takes you from a fresh install to your
first conversation.

## What this version does

- The characters listed on **Characters › Named NPCs** answer when you speak to them. In this version the list is
  short.
- Each of them has a profile EldenNet wrote in its own words, and knows what anyone would know on first meeting them.
  [Spoilers: how EldenNet keeps them](doc:spoilers) says more.
- You can add notes of your own on how a character speaks, on the same page under **Your notes**.
- Conversation is in text, in a box on screen. Characters only talk: they take no other action.
- EldenNet reads the game and never changes it. It writes nothing into the game, and it never edits your saves.

## What it does not do yet

- Other characters, soldiers and passers-by say nothing.
- Characters do not speak up on their own, greet you or talk among themselves.
- Nothing is remembered once the game closes. Within a session, a character recalls the last lines of your previous
  conversation with them.
- There are no voices: lines are text only.
- Some parts of the menu have no pages yet, and are not shown.
- Playing on a Steam Deck or under Proton is not covered by this version.

## What you need

- The game on PC, through Steam. EldenNet works only offline, alone in your own world, without Easy Anti-Cheat, and its
  launcher starts the game that way.
- GraceUI, the menu and on-screen layer EldenNet draws through. It comes with EldenNet.
- An AI provider. It can be a cloud service, with a key of your own from that service (which may charge for its use),
  or a model server on this PC, or one on another PC of your home network.

## Your first conversation

1. Install EldenNet and GraceUI as [Installing and starting EldenNet, and updating GraceUI](doc:installing) says.
2. Start the game with `eldennet-launcher.exe`. The first time, read the notices it shows before the game starts.
3. Open the menu with **F5**. The menu is also in your browser while the game runs: GraceUI › Browser view gives its
   address.
4. On **Home**, read the Status card: its State and Why rows say how EldenNet stands. It cannot read Speaking until a
   model answers.
5. On **AI › Models**, choose a preset, or keep the one shown. A preset chooses a model for each job.
6. On **AI › Providers**, press **Add the preset's provider**, then paste your key on its row. For a server of your
   own, fill in a name and a base URL under **Your own provider** instead: a server on this PC, or one on your home
   network by its private address, takes no key. Then press **List models**.
7. On **AI › Self-test**, press **Run self-test**. It takes a few seconds and works at the title screen too. Its
   verdict says whether replies will come quickly enough.
8. Load your character. When Home's state reads **Speaking**, walk up to one of the characters on Characters › Named
   NPCs. When you are close, a hint shows the talk key: press **T**, type your line and press **Enter**. **Esc**
   closes the box.

> **Note.** Model calls wait until EldenNet has confirmed that you are offline and alone in your own world. While you
> play with others online, EldenNet pauses.

## If something goes wrong

Home says what is wrong and what to do about it, and System › Status says it part by part.
[Troubleshooting](doc:troubleshooting) explains every message.

## See also

[Keys and pad](doc:keys) · [What is sent to your provider](doc:provider) · [Privacy](doc:privacy) ·
[Writing content](doc:writing-content)
