"""Shared paths, file helpers and the citation codes the EldenNet documents use.

Adapted from the owner's Skyrizz workplan tool (tools/workplan in Pt0l3my/Skyrizz): only the section index is
kept here, with EldenNet's own documents and codes.
"""
import json
import os
import pathlib
import re

WP = pathlib.Path(__file__).resolve().parent.parent          # tools/workplan
ROOT = WP.parent.parent                                        # the repository
DATA = WP / "data"                                             # generated, not committed
CHUNK_TOKENS = 6000                                            # sections larger than this are cut into pieces


def load(path, default=None):
    p = pathlib.Path(path)
    if not p.exists():
        return default
    return json.loads(p.read_text(encoding="utf-8"))


def save(path, obj, indent=1):
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_name(p.name + f".{os.getpid()}.tmp")       # atomic, so a parallel reader never sees half a file
    tmp.write_text(json.dumps(obj, ensure_ascii=False, indent=indent) + "\n", encoding="utf-8")
    os.replace(tmp, p)


def tokens(text_or_bytes):
    """A rough token count: four bytes a token, the usual rule for English text."""
    n = len(text_or_bytes.encode("utf-8")) if isinstance(text_or_bytes, str) else int(text_or_bytes)
    return max(1, n // 4)


# --- citation codes ---------------------------------------------------------------------------------------------
#
# Canonical forms:  SN04, SN04§5.2   SkyrimNet research note 04 (docs/research/skyrimnet/04-*.md)
#                   ER05, ER05§8.1   Elden Ring research note 05 (docs/research/eldenring/05-*.md)
#                   DESIGN§7, ARCH§3, NPC§4, MEM§2   the design documents (docs/design/)
# The research notes also cite each other in short forms that depend on where they sit: inside
# docs/research/skyrimnet "R04" means SN04; inside docs/research/eldenring "R05" means ER05 and "S08" means SN08.

NOTE = re.compile(r"(?<![\w#/-])(SN|ER|R|S)(0\d)(?![\w-])(?:\s?§\s?(\d+(?:\.\d+)*[a-z]?))?")
DOC_SECTION = re.compile(r"(?<![\w-])(DESIGN|ARCH|NPC|MEM)\s?§\s?(\d+(?:\.\d+)*)")


def aliases_for(path):
    """Which short prefixes mean which note series, for a document at `path` (relative to the root)."""
    if path.startswith("docs/research/skyrimnet/"):
        return {"SN": "SN", "ER": "ER", "R": "SN"}
    if path.startswith("docs/research/eldenring/"):
        return {"SN": "SN", "ER": "ER", "R": "ER", "S": "SN"}
    return {"SN": "SN", "ER": "ER"}


def cited_codes(text, path=""):
    """Every code a text cites, in order of first appearance, without repeats. A note cited with a section
    yields both the section (SN04§5) and the note (SN04), so `refs SN04` finds every citation of the note."""
    alias = aliases_for(path)
    found = []
    for m in NOTE.finditer(text):
        series = alias.get(m.group(1))
        if not series:
            continue
        note = series + m.group(2)
        found.append((m.start(), note))
        if m.group(3):
            found.append((m.start(), note + "§" + m.group(3).rstrip(".")))
    for m in DOC_SECTION.finditer(text):
        found.append((m.start(), m.group(1) + "§" + m.group(2).rstrip(".")))
    seen, out = set(), []
    for _, c in sorted(found):
        if c not in seen:
            seen.add(c)
            out.append(c)
    return out
