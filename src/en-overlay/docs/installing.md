# Installing and starting EldenNet, and updating GraceUI

EldenNet is a mod of its own with its own launcher. It needs no mod loader, changes no game file, and runs beside
GraceUI, the menu and on-screen layer it draws through.

## What is in the download

- `eldennet-launcher.exe`: the launcher, which starts the game offline and loads EldenNet into it.
- `eldennet.dll`: EldenNet itself.
- The `gal` folder: the game builds EldenNet knows, and how it reads each.
- The `content` folder: EldenNet's files for its presets.
- The `GraceUI` folder: the GraceUI release this version of EldenNet was built with.
- The licence files: see [Licences and credits](doc:licences).

## Installing

1. Unpack the download into a folder of your own, called `<EldenNet folder>` here. Keep its files and folders
   together: the launcher looks for `eldennet.dll` and the `gal` folder beside itself.
2. Copy the `GraceUI` folder into the game's folder, the one that holds `eldenring.exe`, called `<game folder>` here.
   In Steam, choose the game, then Manage, then Browse local files, and open its `Game` folder.
3. That is all. EldenNet keeps your settings, keys, logs and save backups in `%APPDATA%\EldenNet`, which it makes
   itself the first time it runs.

## Starting the game

Run `eldennet-launcher.exe`. Steam must be running, and the game must not be. The launcher has no window of its own: it
asks its questions and reports problems in small dialogs. In order, it:

1. finds the game through Steam, or asks you to pick `eldenring.exe`, and remembers it;
2. shows its notices the first time (tick **Don't show again** once you have read them);
3. checks that it knows the game's build, and asks before it goes on if it does not;
4. checks that Steam runs, that Easy Anti-Cheat and the game do not, and that `eldennet.dll` is EldenNet's;
5. checks GraceUI's folder, file by file;
6. backs up your saves, keeping the newest 10 copies;
7. starts the game offline without Easy Anti-Cheat, and loads EldenNet into it;
8. stays in the background as the crash watcher until the game ends.

To start offline without Easy Anti-Cheat, the launcher writes a small file, `steam_appid.txt`, beside the game, and
removes it once the game's window shows. If you play with others online again, start the game from Steam as usual.

> **Note.** The game started this way cannot reach FromSoftware's servers. Never take a save you played with mods
> into online play. EldenNet's backups are copies of your saves, not a separate save.

Each problem the launcher reports has its own code, E01 to E12:
[Troubleshooting](doc:troubleshooting#The launcher's messages) lists them. The launcher's options are under
[Command-line tools](doc:troubleshooting#Command-line tools).

## With a mod loader you already use

EldenNet needs no loader, but you may load `eldennet.dll` from one you already use. Then:

- start the game offline without Easy Anti-Cheat yourself: with anti-cheat running, EldenNet stays inactive;
- keep the launcher, the `gal` folder and the `content` folder beside `eldennet.dll`: EldenNet starts the launcher as
  its crash watcher, and reads the `gal` folder beside itself;
- back up your saves yourself before you play, with `eldennet-launcher.exe --backup-only`.

System › Status › Compatibility then reads "Started by another loader; EldenNet started its crash watch itself".

## Updating EldenNet

Close the game, then replace the files in `<EldenNet folder>` with the new download's, and copy its `GraceUI` folder
as below. Your settings, keys, notes, overrides and backups are in `%APPDATA%\EldenNet`, so they stay as they are.

When the game is patched, EldenNet may not know the new build yet. The launcher asks before it starts the game, and
EldenNet then stays inactive until an update adds that build. The game itself runs as usual.

## Updating GraceUI

GraceUI is a mod of its own, with its own releases, and each EldenNet release carries the GraceUI it was built with.

1. Close the game.
2. Copy the new `GraceUI` folder over the one in `<game folder>`, replacing the files there.
3. Start the game with the launcher. It checks every file GraceUI lists for itself before the game starts.

Keep the whole folder from one release, and never mix the files of two: a file that is missing, or one of another
size or content than GraceUI's list says, is reported as error E12. GraceUI keeps its own settings in a `user` folder
inside its folder. No release holds that folder, so copying a release over keeps them.

EldenNet needs a GraceUI at least as new as the one it names on System › Status › Compatibility. With an older
GraceUI, or none, EldenNet still runs, but shows nothing: no banner, no chat box and no menu, in game or in the
browser.

## Removing EldenNet

1. Close the game.
2. Run `eldennet-launcher.exe --cleanup`, which removes a `steam_appid.txt` the launcher left beside the game.
3. Delete `<EldenNet folder>`, and the `GraceUI` folder from `<game folder>` unless another mod uses it.
4. `%APPDATA%\EldenNet` holds your settings, keys, notes, logs, crash records and save backups. Copy out any save
   backup you want to keep, then delete it too to remove everything.

## See also

[Getting started](doc:getting-started) · [Troubleshooting](doc:troubleshooting) · [Privacy](doc:privacy)
