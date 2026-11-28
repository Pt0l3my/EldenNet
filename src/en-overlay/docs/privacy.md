# Privacy

EldenNet keeps what it needs on this PC, writes nothing personal into its logs and reports, and sends nothing anywhere
but the model requests that [What is sent to your provider](doc:provider) describes.

## What EldenNet keeps on this PC

Everything is in `%APPDATA%\EldenNet`, in these folders:

| Folder | What it holds |
|---|---|
| config | Your settings, your providers and your keys, and the launcher's own two files |
| overrides | Your own content and your notes on characters, if you write any |
| logs | EldenNet's and the launcher's logs: the newest of each, and one older |
| crashes | A record of each time the game ended badly: the newest 20 |
| save-backups | Copies of your saves: the newest 10 |
| recordings | Recordings of each session's numbers, only if you turn Record sessions on |
| saves | Each character's memories: what the people you met saw, heard and remember, and your conversations |

In `config`, `settings.toml` holds your settings, `providers.toml` your providers and each job's choices, and
`secrets.toml` your keys, which only your Windows user may read. The launcher's two files hold where the game is,
which of its notices you have read and whether it left a `steam_appid.txt` beside the game.

GraceUI keeps its own settings, such as the menu's size and your key bindings, in a `user` folder inside its folder in
the game's folder. While the launcher starts the game, it also leaves a small file, `steam_appid.txt`, beside the game.

## The logs

EldenNet's log holds only EldenNet's own words and numbers: never a path, your Windows user name, your character's
name, a key, what you typed or what a character said. Anything else that would reach the log is written as
`redacted`. A conversation is logged by its kinds of step and its counts alone. System › Logs shows the log of this
session, scrubbed again of anything shaped like a Steam account's number, and **Log detail** on Settings › Developer
chooses how much it writes. The launcher's log follows the same rules.

## Crash records

When the game ends badly, the launcher keeps a record: the kind of end, its codes, EldenNet's last status, the names of
the program files loaded into the game, how long the session lasted and the marks a tester made. A record holds no
memory dump, no path, no user or account name and no text of yours or the game's. It never leaves this PC, and the
launcher explains the newest one at its next start.

## Recordings

**Record sessions** on Settings › Developer is off unless you turn it on. A recording holds numbers only, by its
design: no name, no text, no path. It never captures the menu. Recordings are for the developers' replay tools, and
they stay on this PC unless you choose to share one.

## What you type, and what characters say

Lines are shown on screen: in the chat box and as the character speaks, and afterwards on History › Conversations,
which reads them from your character's file in saves. No log, title, notice history or search holds them. They go to
your provider as [What a conversation sends](doc:provider#What a conversation sends) says, and nowhere else.

With Developer views on, Tools › Workshop, in the browser, also shows the newest request as it was sent and its reply
as the model wrote it, with your keys and your character's name left out. It keeps them in memory alone, until the
next request or until the game closes, and writes them nowhere.

## Your characters' memories

Each character's file in saves holds what the people you met remember of you, on this PC alone. History › Memories
shows it, and its Forget makes one person, or everyone, forget a journey. Forgetting takes a backup first: that
backup keeps the forgotten memories until three newer backups replace it.

## Your character's name

EldenNet reads your character's name from the game to tell one journey from another, keeps it in memory while the game
runs, and writes it nowhere: no log, no request to your provider, no page in this version. A recording keeps only its
length.

## Your keys

Keys are write-only. Once pasted, a key is never shown again, and no log, page, report or recording holds one.
[Your keys](doc:provider#Your keys) says where they are kept and where they go.

## The menu in your browser

- GraceUI serves the browser view to this PC alone, and only while the game runs. The pages load nothing from outside
  this PC.
- Other devices of your home network can reach it only if you switch that on, on GraceUI › Browser view, and each one
  only after you pair it with a code shown in game. A paired device cannot add or remove a provider, set a key, change
  the developer settings, or save, reset or restore your notes on a character.
- The clipboard is used only when you click a copy button.
- GraceUI › Browser view also turns the browser view off.

## Model names a provider lists

The names of the models a provider lists fill the pickers on AI › Models, and go to no log, report or other page. A
name that looks like a path on a PC is left out, since a server on a PC may give names that hold a user name.

## See also

[What is sent to your provider](doc:provider) · [Spoilers: how EldenNet keeps them](doc:spoilers) ·
[Troubleshooting](doc:troubleshooting)
