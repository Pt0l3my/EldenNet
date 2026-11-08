# tools/gamedata: research tools for the game's own data

Small tools the owner runs on their own PC, against exports made from their own install with Smithbox (O55, ER08).
They are research tools, not part of EldenNet: `en-tools` (`fmg-import`, `identity-fill`, `roster-scan`) replaces
them once EldenNet is built on Min's blueprint (O54). Python 3.8 or later, standard library only.

**What never leaves the owner's PC:** the install and every game file, Smithbox's project and every export (CSV,
JSON), names and any other game text, and local paths. A report these tools print holds numbers and counts only;
`smithbox_inspect.py` refuses to print a line that contains any text from the exports.

## `smithbox_inspect.py`: the dry run

Looks at the shape of Smithbox's exports and prints counts and checks only: the columns our tools need, row counts,
how many character rows carry a name number and how many of those resolve to a name, split by base game and
expansion. It answers "does Smithbox give us what our tools expect, for this game version?" before any tool is
designed around it (O56).

### In Smithbox (once per game version)

The steps follow Smithbox's code as read in ER08 §4.2; if a menu is named differently in your build, the export is
still what counts.

1. **Switch off its local server first.** Smithbox starts a local server by default that can also write into the
   running game. Close Smithbox, open `Startup.json` in Smithbox's configuration folder under your local application
   data, set `System_Enable_Soapstone_Server` to `false`, and save (ER08 §3.1).
2. **Make a project** whose project (mod) folder is a **new, empty folder** outside the game's `Game` folder and outside
   any git checkout, with the `Game` folder as its data folder. Never make the `Game` folder itself the project folder.
   Don't use the project screen's me3 options (O47).
3. **Params:** Param Editor › Data Transfer › Export, mode File, "Selected Param", all fields: export `NpcParam`, and
   `TalkParam` if you like. Pick an export folder inside the project folder.
4. **Text:** Text Editor, your language, containers `item`, `item_dlc01` and `item_dlc02` (the last two only if the
   expansion is installed): Export Text › Export Selected Container, one JSON file per container. By default they land
   in the project folder under `.smithbox/Workflow/Exported Text/`.

### Run it

```
python smithbox_inspect.py --npcparam NpcParam.csv --talkparam TalkParam.csv --text item.json item_dlc01.json item_dlc02.json
```

Use the real paths of your exports; `--talkparam` is optional, and `--out report.txt` also writes the report to a
file. The report names files by their file name only. Paste it into the conversation as it is: it holds no names and no
game text.

### Tests

`python3 -I -m unittest discover -s tools/gamedata` (invented data only).
