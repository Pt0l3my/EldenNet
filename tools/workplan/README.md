# wp: read EldenNet's documents by section, not whole

The research notes and design documents run to several hundred thousand tokens. `wp` cuts every document into
sections by heading and keeps a small index of each section's code, size and citations, so you can read exactly
the part you need. It is a slim adaptation of the section index in the owner's Skyrizz workplan tool
(`tools/workplan` in Pt0l3my/Skyrizz). Only the index is kept, with EldenNet's own documents and codes.

The index lives in `tools/workplan/data/index.json`. That file is generated and gitignored, and it rebuilds
itself whenever an indexed file changes, so there is nothing to remember. The session-start hook warms it.

## Codes

| Code | Means |
|---|---|
| `SN04`, `SN04§5.2` | SkyrimNet research note 04 (`docs/research/skyrimnet/04-*.md`), or one of its sections |
| `ER05`, `ER05§8.1` | Elden Ring research note 05 (`docs/research/eldenring/05-*.md`), or one of its sections |
| `DESIGN§7`, `ARCH§3`, `NPC§4`, `MEM§2` | The design documents in `docs/design/` |
| `P1`, `J2`, `REVIEW` | The design-panel records in `docs/design/process/` |
| `RES`, `README`, `CLAUDE`, `PSW`, `WP` | The research index, the root README, CLAUDE.md, the privsweep and wp READMEs |

A numbered heading gets its number (`SN00§4`). Any other heading gets a slug of its title
(`SN00§tl-dr`, `ER03§verification-log`). The research notes also cite each other in short forms: inside
`docs/research/skyrimnet`, `R04` means `SN04`, and inside `docs/research/eldenring`, `R05` means `ER05` and
`S08` means `SN08`. The index resolves these, so `wp refs SN04` finds every citation, whichever form it used.

## Commands

```
python3 -I tools/workplan/wp.py show ER05 --list         # a document's sections, sizes only
python3 -I tools/workplan/wp.py show DESIGN§5            # one section
python3 -I tools/workplan/wp.py show SN00§4 --all        # a section and everything under it
python3 -I tools/workplan/wp.py refs ER05§8              # every section that cites a code
python3 -I tools/workplan/wp.py find in:docs/design text:'bark pool'   # sections matching all terms
python3 -I tools/workplan/wp.py grep 'boss_fight_active' in:docs/research   # matching sentences, with lines
python3 -I tools/workplan/wp.py stat                     # the largest files and sections
```

`show` accepts `--max-tokens N` to stop before printing more than N tokens.

## The rule

Never open a whole design or research document to find something. Look it up by code, or with `find` or
`grep`, check the size, and read only that section.
