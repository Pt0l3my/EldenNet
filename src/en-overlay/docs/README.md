# EldenNet's bundled documents (Docs; PAGES §4.6.9; roadmap S11.4.a)

The ten documents of the Docs page, drafts for the owner's review. Each is one file, `<id>.md`, in a Markdown subset
that maps one to one onto GraceUI's document block (GraceUI's proposal 0013, §1, G44). The space publishes each as a
member `docs/<id>` of its family and lists it in the master list with its title and headings; nothing in game or in the
browser fetches anything for them (PV6).

| Id | Title |
|---|---|
| `getting-started` | Getting started |
| `installing` | Installing and starting EldenNet, and updating GraceUI |
| `provider` | What is sent to your provider |
| `privacy` | Privacy |
| `spoilers` | Spoilers: how EldenNet keeps them |
| `keys` | Keys and pad |
| `troubleshooting` | Troubleshooting |
| `writing-content` | Writing content |
| `licences` | Licences and credits |
| `about` | About |

## The format

Each construct below is one block or one span of proposal 0013 §1, and nothing else is allowed.

| Markdown | Block or span |
|---|---|
| `# Title`, once, as the first line | the document's `title` |
| `## Heading`, `### Heading` (120 characters at most, no marks) | `{"heading", "level": 2}` or `3` |
| A paragraph: lines up to a blank line, joined with a space | `{"paragraph": spans}` |
| `- item` or `1. item`, one level, a continuation line indented | `{"list": [...], "ordered": false}` or `true` |
| `> **Note.** ...`, every line of it starting `> ` | `{"note": spans}`, the words "Note." kept as a strong span |
| A fence of three backticks, with no info string | `{"code": "..."}`, its line breaks kept |
| A plain table: a header row, a rule, rows; plain text in every cell | `{"table": {"columns", "rows"}}` |
| `**words**` | a span with `"strong": true` |
| `` `words` `` | a span with `"code": true` |
| `[words](doc:<id>)`, `[words](doc:<id>#<heading words>)` | a span with `"doc"`, and `"heading"`: its exact words |

A span carries at most one mark, so marks never nest. A link's heading words hold spaces, which a standard Markdown
renderer does not take as a link: the space's converter parses `(doc:...)` itself. There are no images, no URLs of any
kind (a bare `https://` names a scheme, never an address) and no HTML; a placeholder in angle brackets is always inside
a code span. Sizes: a document under 64 KiB and at most 400 blocks, a paragraph, list item or note under 2000
characters, a code block under 16 KiB. Lines are 120 characters at most.

About holds three slots that the space fills when it publishes: `{eldennet_version}`, `{graceui_version}` and
`{build}`.

## The rules they keep

Our own words, British spelling, short sentences a player can follow. No game material: no character, place, item or
event of the game and no game text; "the game" stands for it (the game's title appears only in the launcher's own
messages, quoted as built, and in the trademark line). Nothing about any person, PC or account: example paths are
relative or placeholders. No AI provider is named or recommended: providers appear as kinds (a server on this PC, one
on the home network, a cloud provider). What a version does not do is said plainly, with no date. Every word quoted
from a page, a banner, a notice or the launcher is quoted as built.
