# Troubleshooting

When EldenNet cannot do something, it says so in plain words, and says what to do. This page lists those words, as
the banner, Home, the AI pages, the chat box and the launcher show them, with what each means. Where a message holds a
value that changes, such as the game's build, it is written ‹build› here.

## Where to look first

1. **Home**: the Status card's State and Why rows say how EldenNet stands, and Problems lists what went wrong, each
   with what to do.
2. **System › Status**: Health, EldenNet's parts one by one, What EldenNet reads, and on its Compatibility tab the
   versions and how the game was started.
3. **System › Logs**: EldenNet's log of this session. Set **Log detail** to Detailed before you try something again
   for a bug report.
4. Without the menu, the log files are in `%APPDATA%\EldenNet\logs`: `eldennet.log` and `launcher.log`.

## The banner in game

When EldenNet cannot work in this session, it shows a banner that stays on screen, and Home's Problems give the same
words with what to do. The game itself runs as usual whatever the banner says.

### Game build not supported yet

**"EldenNet inactive: game build ‹build› not supported yet"**. EldenNet does not know this build of the game, which
usually means the game was patched. An update of EldenNet adds the build once it has been checked. Until then, play as
usual, or start the game from Steam.

### Files for this game build incomplete

**"EldenNet inactive: its files for game build ‹build› are incomplete. Reinstall EldenNet, or look for an update."**
EldenNet knows the build, but its own files for reading it, in the `gal` folder, are missing or damaged. Replace
`<EldenNet folder>` with a fresh copy of the download, and if that does not help, look for an update.

### Cannot read this game build yet

**"EldenNet inactive: it cannot read game build ‹build› yet. Look for an update of EldenNet."** EldenNet knows the
build, but cannot yet read all it needs of it. Look for an update of EldenNet for this build.

### The game did not finish starting

**"EldenNet inactive: the game did not finish starting"**. The game did not reach the point where EldenNet begins to
read it in time. Start the game again. If it happens every time, keep the log for a bug report.

### Easy Anti-Cheat is running

**"EldenNet inactive: Easy Anti-Cheat is running. EldenNet only runs offline; start the game with the EldenNet
launcher."** EldenNet never runs beside anti-cheat. Close the game, then start it with `eldennet-launcher.exe`, which
starts it without Easy Anti-Cheat.

### Offline solo session not confirmed

**"EldenNet inactive: it could not confirm this is an offline solo session (game build ‹build›)."** EldenNet could
not read whether you are offline and alone, so it stays off for this session. Start the game offline with the
EldenNet launcher.

### Stopped reading after an error

**"EldenNet stopped reading the game after an error. Your game is not affected."** Something EldenNet read did not pass
its checks, so it stopped reading for the rest of the session. Restart the game to try again. If it happens again, keep
the log for a bug report.

### Test run

**"EldenNet test run: it makes no call into the game"**. A test run for the developers, started on purpose. There is
nothing to do.

### Paused for multiplayer

**"EldenNet paused (multiplayer)"** shows while you play with others, or in another player's world. EldenNet pauses
until you are alone in your own world again for a few seconds, then carries on.

## Home's state

The State row, and the lamp at the top of the menu, read one of these words; the Why row says more.

- **Starting**: EldenNet is still starting, and the menu shows no lamp yet. Why names the step, such as "Waiting for
  the game's window" or "Finding what it reads in the game". Wait a moment.
- **Inactive**: EldenNet stopped for this session. Why gives the banner's words: see
  [The banner in game](doc:troubleshooting#The banner in game).
- **Observing**: EldenNet reads the game, but no model answers Conversation yet. Why reads "EldenNet reads the game.
  NPCs speak once a model answers Conversation: AI › Providers says what is missing." Open AI › Providers: its Now
  line says what is missing.
- **Speaking, in part**: "NPCs speak, but part of the game EldenNet reads has stopped: System › Status says which."
  Characters still talk. Restarting the game tries the stopped part again.
- **Speaking**: "EldenNet reads the game, and NPCs speak." All is well.
- **Dormant**: EldenNet waits, and Why says what for: "EldenNet pauses while you play with others online.", "At the
  title screen: EldenNet waits until a character is loaded.", "Loading: EldenNet waits for the world.", "EldenNet
  checks that this session is offline and solo." or "EldenNet missed a reading of the game and waits for the next."
  Each passes by itself.

The Game build row reads the build's number and "supported" once EldenNet reads the game, "recognised, not readable
yet" when EldenNet knows the build but has not read it, or "not supported yet".

## Home's problems

Besides the banner's words, Problems may list these.

- **"‹part›: EldenNet stopped reading it after faults."** One part of what EldenNet reads failed its checks. "Everything
  else goes on. Restarting the game tries it again." The parts are The session, The world, Combat, Your character,
  Progress, The game's text, The game's screens, Place, The camera, The game's tables and Statuses.
- **"Your settings file has a problem, so EldenNet uses its defaults."** "System › Logs names the setting. Correct it
  in settings.toml, then start the game again." See [Your settings file](doc:troubleshooting#Your settings file).
- **"Your providers file has a problem, so EldenNet calls no model."** The words after it say what, and on which line
  of `providers.toml`. A provider's base URL must start with `https://` or `http://`, end at its path with no ? or #,
  and hold no name or password (a key goes in `secrets.toml`, named by `key_ref`). A cloud provider, and any provider
  with a key on another computer, must use `https://`. Correct the line, then start the game again.

System › Status adds a lamp for each of EldenNet's parts: Start, Reading the game, The core, The providers file,
Anti-cheat watch, The log, Recording (when it is on) and GraceUI. Each says in a few words what it is doing, or what
failed.

## The AI pages' Now line

Each AI page starts with a line that says what the AI can do now, and why.

- **"No provider yet: on Providers, add the preset's provider or one of your own."**
- **"Conversation has no model yet: choose a preset on Models, or name one in your providers file."**
- **"‹provider› has no key yet: paste it on its row on Providers."**
- **"‹provider› asks for a key: paste it on its row on Providers."** The server asked for a key EldenNet did not have.
- **"‹provider› refused its key: paste it again on its row on Providers."** Check that the key is whole, and still
  valid at that provider.
- **"No model answers Conversation now: its models are resting after failures, and try again within a minute."**
  EldenNet rests a model for a minute after three failures in a row. If it keeps happening, look at Health on
  Providers.
- **"Model calls wait while you play with others online."**
- **"Model calls wait until EldenNet has confirmed that this session is offline and solo."**
- **"Your providers file has a problem, so EldenNet calls no model: System › Status says what."**
- **"Conversation is ready: NPCs answer when you speak to them."** All is well.
- **"NPCs speak once a model answers Conversation. The self-test checks your provider meanwhile."** None of the
  above, but no model has answered Conversation yet. Run the self-test on AI › Self-test.

Each job's card on AI › Models shows its state: Ready, No model, No key yet, Failing (its provider refused its key, or
asked for one), Cooling down (its provider asked EldenNet to wait), or Resting after failures. On Providers, Health and
the model lists say what went wrong with a provider:

- "Could not be reached.": check its address, and that the server runs.
- "Refused the key.": paste the key again on its row.
- "Asked EldenNet to wait: too many requests.": wait a little; the provider limits how often it may be asked.
- "Took too long to answer.": the model may be too large or too busy. Try a smaller or faster one.
- "Refused the request: the model may not exist there.": press List models, and choose a model the provider lists.
- "Answered with something EldenNet could not read." or "Declined to answer.": try again, or choose another model.

## Messages about memory

- **"Memory is off for this session: …"** EldenNet keeps nothing new this session; conversations still work. The words
  after the colon say why: the file is in use by another program, the disk is full, the file is damaged (its newest
  backup is used next time), it was made by a newer EldenNet, it could not be read or written, or no Elden Ring save
  folder was found.
- **"EldenNet's memory file was damaged, so its newest backup was restored."** What came after that backup is lost.
- **"Less than 200 MB of disk is free: EldenNet may stop saving memories."** Free some space.
- **"Memory is behind; recent events are not being saved."** The PC is very busy; it passes on its own.
- **"(Not yet: EldenNet is waiting for your answer about this save.)"** You loaded an older save: answer the panel in
  the middle of the screen first.

## Messages in a conversation

- **"They have nothing to say to you."** This character is not one EldenNet voices.
- **"(‹name› will not speak with you)"**, **"(‹name› is asleep.)"** or **"(‹name› does not stir.)"** The character
  cannot talk to you just now.
- **"Not now: danger is near."** or **"The moment has passed."** Wait until it is safe.
- **"Chat cannot take the keyboard: GraceUI's menu has the controls."** Close the menu first.
- **"Chat cannot take the keyboard: another mod (‹id›) has the controls."** or "...: another mod has the controls."
  Close that mod's window or panel first.
- **"Chat cannot take the keyboard: danger is near."**, "Chat cannot take the keyboard while the game loads.", "Chat
  cannot take the keyboard while a game menu is open." or "Chat cannot take the keyboard just now." Try again in a
  moment.
- **"No answer: provider timed out"**, **"No answer: the provider failed"** or **"Provider rate limit, try again
  shortly"**. Your provider did not answer in time, failed, or asked EldenNet to wait. Try again; Health on AI ›
  Providers says more.
- **"API key rejected: paste it again on AI › Providers"**. Your provider refused the key. Open the menu with F5,
  then AI › Providers, and paste the key again on its row.
- **"No answer: no model is ready"**. No model answers Conversation: see
  [The AI pages' Now line](doc:troubleshooting#The AI pages' Now line).
- **"No answer: the conversation's prompt could not be made"**. EldenNet could not build the request from its content.
  Look at Content packs for a refused file, and keep the log for a bug report.
- **"Conversation paused: part of the game EldenNet reads stopped working."** See System › Status.
- **"Conversation closed after a fault in EldenNet."** Keep the log for a bug report.

When a reply cannot be had, the character's page shows "…" instead.

## When a character does not answer

- The character must be one of those on Characters › Named NPCs.
- You must be close: within 8 metres unless you changed `range.talk_m` in your settings file.
- Nothing dangerous may be near, and no game menu may be open.
- Home's state must read Speaking, or Speaking, in part.
- `features.chat` must not be false in your settings file.
- Content packs must show your files as loaded: a refused file of yours can stop all of EldenNet's content loading.

## When nothing shows in game

- If the launcher reported **E12**, GraceUI is missing or damaged: see
  [Updating GraceUI](doc:installing#Updating GraceUI).
- GraceUI may be older than EldenNet needs. GraceUI then shows a notice naming EldenNet and both versions: update
  GraceUI from EldenNet's download.
- Your settings file may hold `enabled = false`, which turns EldenNet off, or `overlay = { connect = false }`, which
  starts it with nothing shown.
- With Easy Anti-Cheat running, only the banner shows.
- The log says what happened: look in `eldennet.log` for the line that starts `ELDENNET connect`.

## Your settings file

`%APPDATA%\EldenNet\config\settings.toml` holds your settings. EldenNet writes the changes you make on its pages there,
and keeps every other line. The file is read strictly: a key it does not know, a value of the wrong kind or one out of
its range refuses the whole file, and EldenNet uses its defaults until you correct it. System › Logs names the key,
never its value. These keys have no page yet, and may be set by hand:

```
schema = 2
enabled = true
features = { chat = true, self_awareness = true }
range = { talk_m = 8.0 }
overlay = { connect = true }
```

- `enabled`: false turns EldenNet off.
- `features.chat`: false turns conversation off.
- `features.self_awareness`: false leaves a character's wounds, and whether you struck them, out of their requests.
- `range.talk_m`: how close you must be to talk, from 3 to 16 metres.
- `overlay.connect`: false starts EldenNet with nothing shown.

## The launcher's messages

The launcher shows each problem in a dialog with its code.

### E01: the game was not found

"ELDEN RING was not found. Pick eldenring.exe, or start the launcher with `--game "<folder>"`." Press **Open
eldenring.exe…** and choose the game's executable; the launcher remembers it.

### E02: Steam is not running

"Steam is not running. Start Steam, then run the EldenNet launcher again."

### E03: the game is already running

"ELDEN RING is already running. Close it, then start it again from this launcher."

### E04: Easy Anti-Cheat is running

"Easy Anti-Cheat is running (start_protected_game.exe). Close the game, then start it from this launcher."

### E05: this game build is not supported yet

"This game build (‹build›) is not supported by this EldenNet version yet. Launch anyway? EldenNet will stay inactive
until an update arrives." **Launch anyway (EldenNet stays inactive)** starts the game with EldenNet inactive.

### E06: the appid file could not be written

"The launcher could not write steam_appid.txt into the game folder (‹reason›). Move the game to a folder you can write
to, or see the install guide." The game's folder must be one your Windows user may write to.

### E07: your save could not be backed up

"Your save could not be backed up (‹reason›). Continue without a backup?" **Continue without a backup** starts the
game anyway. Check that `%APPDATA%` has room, and that no other program holds the save open.

### E08: eldennet.dll was not found

"eldennet.dll was not found next to the launcher (or at the --dll path)." Keep `eldennet.dll` in the same folder as
`eldennet-launcher.exe`.

### E09: the game could not be started

"The game could not be started: ‹reason› (code ‹code›)." Start the game once from Steam to check that it runs, then try
the launcher again.

### E10: EldenNet could not be loaded

"EldenNet could not be loaded into the game: the ‹step› step failed (code ‹code›). The game was closed again.
Antivirus software sometimes blocks this step; see the install guide. Start the game without EldenNet instead?"
**Start without EldenNet** starts the game with nothing loaded. If your antivirus software blocked the launcher, allow
it in that software.

### E11: Proton

"This launcher must run inside the game's Proton prefix. See the Proton notes in the install guide." This version does
not cover Proton.

### E12: GraceUI is missing or damaged

"GraceUI, which shows EldenNet in the game, is missing or damaged: ‹what›. The game starts, and EldenNet runs with
nothing shown in the game. Copy the GraceUI folder from EldenNet's download into the game's folder again (see the
install guide)." See [Updating GraceUI](doc:installing#Updating GraceUI).

## Command-line tools

### eldennet-launcher

Run it from a command prompt in `<EldenNet folder>`, or add an option to a shortcut's target. `--help` lists every
option.

```
eldennet-launcher [options] [-- game arguments]
  --game PATH          the game's executable or its folder (remembered); else found through Steam
  --dll PATH           load this DLL instead of eldennet.dll beside the launcher
  --no-backup          skip the save backup
  --vanilla-offline    start offline without EAC and load nothing
  --backup-only        back up the saves and exit
  --cleanup            remove a steam_appid.txt this launcher created, if the game is not running
  --print-diagnostics  print what discovery and the pre-flight checks find, and exit
  --watch PID          watch a running game as its crash watcher (EldenNet starts this under other loaders)
```

`--backup-only`, `--cleanup`, `--print-diagnostics` and `--watch` each go alone. Anything after `--` is passed to the
game. `--help` also lists the options for development and tests apart: they change nothing unless given. The launcher
ends with 0 when the game started and ended, 2 for a mistake in the options, 3 when there was nothing it could do, and
11 to 21 for the errors E01 to E11.

### eldennet-engine

EldenNet's command-line tool for testers and content authors: it runs EldenNet's engine outside the game.
`eldennet-engine <command> --help` lists a command's options. Three help with problems:

- `eldennet-engine probe` looks for model servers on this PC, on the ports such servers usually use, and says how many
  models each lists.
- `eldennet-engine models --config <folder>` lists the models each provider of your providers file serves, or why it
  did not answer. Give it `%APPDATA%\EldenNet\config` as the folder.
- `eldennet-engine lint --recording <file>` checks that a recording holds nothing but what a recording may hold.

## Save backups

The launcher keeps the newest 10 copies of your saves in `%APPDATA%\EldenNet\save-backups`, a folder for each of your
save folders, with one folder for each backup named by its date and time. It never edits your save, and makes no new
copy when nothing changed since the last. To restore one:

1. Close the game.
2. Copy the files in your save folder, `%APPDATA%\EldenRing\<number>`, somewhere safe.
3. Copy `ER0000.sl2`, and `ER0000.sl2.bak` if the backup has it, from the backup's folder into your save folder,
   replacing the files there.

> **Note.** Restoring replaces your current save. Keep the copy from step 2 until you are sure.

## Crash records

When the game ends badly, the launcher keeps a record in `%APPDATA%\EldenNet\crashes` and explains it the next time it
starts. A record holds numbers and fixed words only, and the names of the program files the game had loaded:
[Crash records](doc:privacy#Crash records) says what is in it. Keep it, with the logs, for a bug report.

## See also

[Installing and starting EldenNet, and updating GraceUI](doc:installing) · [Keys and pad](doc:keys) ·
[What is sent to your provider](doc:provider)
