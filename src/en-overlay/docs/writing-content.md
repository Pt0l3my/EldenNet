# Writing content

EldenNet's characters speak from content: their profiles, the instructions to the model, the word lists its checks
use. You can add your own notes on a character, turn characters off, and replace EldenNet's files with your own. This
page gives the file names, keys and limits this version reads. Its examples are invented.

## Where your files go

Your files go in the overrides folder, `%APPDATA%\EldenNet\overrides`. Make it if it is not there. Inside it, a file
takes the same path as EldenNet's own file of the same kind:

- a file of yours replaces EldenNet's file of the same path whole: nothing of EldenNet's version is kept, so a
  replacement must be complete;
- a file of a path EldenNet does not have is added;
- `disable.toml`, at the folder's top, turns EldenNet's own content off.

In this version EldenNet's own content is built into `eldennet.dll`.

| What | Limit |
|---|---|
| Files read | .toml, .md and .j2 files; others are passed over |
| Size of a file | 64 KB; a character's profile 16 KB; your notes on a character 8 KB |
| Number of files | 256; any more are not read |
| Folders | Up to four levels inside the overrides folder |
| Links to other folders or files | Never followed |

## When EldenNet reads them

EldenNet reads the overrides folder each time the game starts, and again after the profile editor saves your notes. A
file you change by hand takes effect the next time the game starts.

## Checking your files

Every file is read strictly: a key EldenNet does not know, a value of the wrong kind, or one past its limit is an
error. What happens then depends on the file:

- An error in your notes on a character refuses that file alone. The character keeps EldenNet's profile.
- An error in any other file refuses the whole load. If EldenNet had loaded its content earlier in the session, that
  stays in use; at the start of the game, no content is in use, and no character speaks.
- A warning loads the file, and names what to improve.

**Content packs** shows each of your files as Loaded, Refused, "Not loaded: another file was refused" or "Not read",
with a row for each problem, its line and its words. In game, the first problem shows as a notice naming its file, key
and line. Neither ever shows a value of your file.

## Your notes on a character

Your notes are a layer over EldenNet's profile of a character: four fields, each of which you may leave to EldenNet.

| Field | What it is | Limit |
|---|---|---|
| instructions | Notes on how the character speaks and behaves, last in their voice | 35 words and 210 characters |
| subjects | What they like to talk about, in place of EldenNet's list | 8 at most, each 10 words and 50 characters |
| register | Their way of speaking: one of the five below | One choice |
| reply_words_max | The longest reply, in words | 10 to 60 |

The five ways of speaking, as the editor names them: `archaic-regal` (Old and stately), `formal` (Formal and
measured), `colloquial` (Everyday, with old idiom), `broken` (Broken, in fragments) and `chant` (Chanting, in
refrains).

### In the profile editor

On **Characters › Named NPCs**, choose a character, then fill in **Your notes**. **Check** shows any problem, and
**Save** writes the file. Each save keeps the previous file beside it, ending `.bak`: **Restore** brings it back, and
**Reset** returns the character to EldenNet's profile. **Draft with AI** turns rough notes of yours, up to 400
characters, into a draft beside your fields, on your click alone and at most 20 times a session; nothing is saved
until you save. A draft goes to your provider: see
[What a profile draft sends](doc:provider#What a profile draft sends).

If EldenNet's profile of the character has changed since you saved, the editor says so. A file written by a newer
EldenNet is shown, and locked.

### By hand

The editor writes `layers/persona-<id>.toml` in the overrides folder, where `<id>` is the character's id; you may
write the same file yourself. An invented example:

```
schema = 1
target = "persona:example"
rev = 1
instructions = "Speaks slowly and kindly, and laughs at her own jokes before she reaches the end of them."
subjects = ["the river in flood", "knots, and the rope that holds them", "travellers who never pay the toll"]
register = "colloquial"
reply_words_max = 30
```

`schema` is 1, `target` names the character's id, and `rev` is a number the editor raises at each save. The editor
also writes `base_hash`, which tells it whether EldenNet's profile has changed since, and `drafted`, once you have
taken a draft.

## Turning content off

`disable.toml`, at the top of the overrides folder, turns EldenNet's own characters off by id. A character turned off
is no longer voiced. An invented example:

```
personas = ["example"]
```

An id that names no character draws a warning, and nothing else. The file also accepts the lists `cards`,
`characters`, `kb`, `triggers` and `rules`, each of ids of at most 64 characters, but in this version they turn
nothing off and draw a warning. Content packs lists what the file turned off under **Turned off**.

## Writing well

- Write in your own words, never the game's text.
- Describe what a character does, not the effect: "answers a question with a question" rather than "mysterious".
- Never quote a sample line in your notes: a model tends to repeat it word for word. Describe the manner instead.
- Write each subject as a short phrase, not a sentence, a question or an instruction.
- Keep notes short. A character's voice has a fixed room in each request, and long notes are warned about.
- Never write `{{`, `{%` or `{#` in a field: they are errors.

## Not in this version

- **Cards** for generic characters, such as soldiers and passers-by: EldenNet ships none and reads none.
- **Event rules**: EldenNet's rules for what happens in the game are built in, and none is read from your overrides
  folder.
- **Knowledge entries** for a profile's `knowledge` keys: none is read yet.
- **Profiles of your own** for characters EldenNet does not voice, and a profile of the character you play.

## See also

[Spoilers: how EldenNet keeps them](doc:spoilers) · [What is sent to your provider](doc:provider) ·
[Troubleshooting](doc:troubleshooting)
