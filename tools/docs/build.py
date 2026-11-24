#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tools/docs/build.py: EldenNet's bundled documents (Docs, PAGES §4.6.9), from their drafts to GraceUI's document
blocks (GraceUI D251, eldennet-changes §77), compiled into eldennet.dll.

    python3 -I tools/docs/build.py           write src/en-overlay/src/docs_data.cpp
    python3 -I tools/docs/build.py --check   write nothing; fail if a draft breaks a rule, or if the file is not what
                                             the drafts build

The drafts are src/en-overlay/docs/<id>.md; that folder's README.md lists their ids and titles in a table, in Docs'
order, and gives the format: a Markdown subset in which each construct is one block or one span of GraceUI's document
and nothing else is allowed. This converts it, and fails, naming the file and its line, on anything outside it:

  - `# Title` once, as the first line (README's title for the id); `## ` and `### ` headings, 120 characters at most,
    with no marks;
  - a paragraph: lines up to a blank line, joined with a space; a list (`- ` or `1. `, numbered from 1 in order), one
    level, a continuation line indented; a note, every line starting `> `, its first words `**Note.**`; a fence of
    three backticks with no info string, its lines kept; a plain table (a header row, a rule, rows of the same cells,
    plain text in every cell); a blank line between blocks;
  - spans: `**words**` strong, `` `words` `` code, `[words](doc:<id>)` or `[words](doc:<id>#<heading words>)` a link,
    at most one mark a span and none nested; the link's document one of the list's and its heading one of that
    document's, word for word;
  - nothing else: no HTML or angle brackets outside code, no images, no backslash escapes, no other emphasis, no
    address of any kind (`://` followed by one), no line over 120 characters or holding a tab;
  - About's slots, `{eldennet_version}`, `{graceui_version}` and `{build}`, in a table's cells alone, which the space
    fills when it publishes;
  - GraceUI's limits, and the README's where they are stricter: a document's JSON under 64 KiB and 400 blocks at most;
    a paragraph, item or note under 2000 characters; a heading 120; a code block under 16 KiB; 100 items, 8 columns
    and 200 rows at most.

Its output, src/en-overlay/src/docs_data.cpp, holds each document's id, title, headings (joined by " · ", for the
list's search, D250) and body, `{"blocks": [...], "title": "..."}` as compact JSON with its keys sorted, the form
nlohmann/json writes, in lines of 120 characters at most. Standard library only; it reads nothing but the drafts.
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
DOCS = ROOT / "src" / "en-overlay" / "docs"
OUTPUT = ROOT / "src" / "en-overlay" / "src" / "docs_data.cpp"

MAX_LINE = 120
MAX_DOC_BYTES = 64 * 1024  # under it (D251: beyond it the reader says the document is too large to show)
MAX_BLOCKS = 400
MAX_TEXT = 2000  # under it: a paragraph's, a list item's or a note's characters (D251: 2000 at most)
MAX_HEADING = 120
MAX_CODE = 16 * 1024  # under it, in bytes
MAX_ITEMS = 100
MAX_COLUMNS = 8
MAX_ROWS = 200
SLOTS = ("{eldennet_version}", "{graceui_version}", "{build}")
DOC_ID = re.compile(r"^[a-z0-9._-]{1,64}$")  # GraceUI's document ids (its document.cpp)
ORDERED = re.compile(r"^(\d+)\. (.*)$")
TABLE_RULE = re.compile(r"^\|(\s*:?-+:?\s*\|)+$")
ADDRESS = re.compile(r"://[A-Za-z0-9\[]")
DELIMITER = ')docs"'
CHUNK = 96  # characters of a literal's text a line


class DraftError(Exception):
    def __init__(self, where, line, words):
        super().__init__(f"{where}:{line}: {words}")


def listed(readme):
    """README.md's table of ids and titles, in Docs' order: the table headed `| Id | Title |`, rows of `` `id` `` |
    Title."""
    out = []
    lines = readme.splitlines()
    if "| Id | Title |" not in lines:
        return out
    for line in lines[lines.index("| Id | Title |") + 2:]:
        m = re.match(r"^\| `([^`]+)` \| (.+?) \|$", line)
        if not m:
            break
        out.append((m.group(1), m.group(2)))
    return out


def spans(text, where, line, known=None):
    """A paragraph's, item's or note's words as GraceUI's spans: plain, strong, code or a link, one mark at most."""
    out = []
    plain = []

    def flush():
        if plain:
            words = "".join(plain)
            check_plain(words, where, line)
            out.append({"text": words})
            plain.clear()

    i = 0
    while i < len(text):
        if text.startswith("**", i):
            end = text.find("**", i + 2)
            if end < 0:
                raise DraftError(where, line, "a ** with no ** to close it")
            inner = text[i + 2:end]
            if not inner or any(m in inner for m in ("`", "[", "]", "*")):
                raise DraftError(where, line, "a strong span holds words alone, with no other mark (marks never nest)")
            check_plain(inner, where, line)
            flush()
            out.append({"strong": True, "text": inner})
            i = end + 2
        elif text[i] == "`":
            end = text.find("`", i + 1)
            if end < 0:
                raise DraftError(where, line, "a ` with no ` to close it")
            inner = text[i + 1:end]
            if not inner:
                raise DraftError(where, line, "an empty code span")
            if ADDRESS.search(inner):
                raise DraftError(where, line, "an address in a code span: no URL of any kind")
            flush()
            out.append({"code": True, "text": inner})
            i = end + 1
        elif text[i] == "[":
            if plain and plain[-1] == "!":
                raise DraftError(where, line, "an image: the format has none")
            m = re.match(r"\[([^\]\[`*]+)\]\(doc:([^)#]+)(?:#([^)]+))?\)", text[i:])
            if not m:
                raise DraftError(where, line, "a [ that is no link of the form [words](doc:<id>) or "
                                              "[words](doc:<id>#<heading words>)")
            words, doc, heading = m.group(1), m.group(2), m.group(3)
            check_plain(words, where, line)
            if not DOC_ID.match(doc):
                raise DraftError(where, line, f"a link to {doc!r}, which is no document's id")
            flush()
            span = {"doc": doc, "text": words}
            if heading is not None:
                span["heading"] = heading
            if known is not None:
                known.append((doc, heading, line))
            out.append(span)
            i += m.end()
        else:
            plain.append(text[i])
            i += 1
    flush()
    if not out:
        raise DraftError(where, line, "a block with no words")
    if sum(len(s["text"]) for s in out) >= MAX_TEXT:
        raise DraftError(where, line, f"a paragraph, item or note of {MAX_TEXT} characters or more")
    return out


def check_plain(words, where, line):
    for mark, what in (("<", "an angle bracket outside code (no HTML; a placeholder goes in a code span)"),
                       (">", "an angle bracket outside code (no HTML; a placeholder goes in a code span)"),
                       ("*", "a * outside a ** pair: other emphasis is outside the format"),
                       ("\\", "a backslash outside code: escapes are outside the format"),
                       ("]", "a ] outside a link"), ("![", "an image"), ("{", "a { outside a table's cell"),
                       ("}", "a } outside a table's cell")):
        if mark in words:
            raise DraftError(where, line, what)
    if ADDRESS.search(words):
        raise DraftError(where, line, "an address: no URL of any kind")


def check_cell(words, where, line):
    for slot in SLOTS:
        words = words.replace(slot, "")
    for mark in ("`", "**", "[", "]", "<", ">", "{", "}", "\\", "*"):
        if mark in words:
            raise DraftError(where, line, f"a table's cell holds {mark!r}: plain text in every cell, or one of the "
                                          "slots " + ", ".join(SLOTS))
    if ADDRESS.search(words):
        raise DraftError(where, line, "an address: no URL of any kind")


def column_ids(labels):
    ids = []
    for n, label in enumerate(labels):
        base = re.sub(r"[^a-z0-9]+", "_", label.lower()).strip("_") or f"c{n}"
        one, k = base, 2
        while one in ids:
            one, k = f"{base}_{k}", k + 1
        ids.append(one)
    return ids


def cells(row):
    return [c.strip() for c in row.strip()[1:-1].split("|")]


def convert(doc_id, text, title_wanted):
    """One draft: (title, blocks, headings, links), or a DraftError."""
    where = f"src/en-overlay/docs/{doc_id}.md"
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    else:
        raise DraftError(where, len(lines), "the file does not end with a line break")
    for n, line in enumerate(lines, 1):
        if len(line) > MAX_LINE:
            raise DraftError(where, n, f"a line of {len(line)} characters: {MAX_LINE} at most")
        if "\t" in line or "\r" in line:
            raise DraftError(where, n, "a tab or a carriage return")
        if line != line.rstrip():
            raise DraftError(where, n, "a space at the line's end")
    if not lines or not lines[0].startswith("# "):
        raise DraftError(where, 1, "the first line is the title, `# Title`")
    title = lines[0][2:]
    if title != title_wanted:
        raise DraftError(where, 1, f"the title {title!r} is not README.md's {title_wanted!r}")
    if len(title) > MAX_HEADING:
        raise DraftError(where, 1, f"a title over {MAX_HEADING} characters")
    check_plain(title, where, 1)
    blocks, headings, links = [], [], []
    i = 1
    previous_blank = True

    def need_blank(n):
        if not previous_blank:
            raise DraftError(where, n, "a block starts with no blank line before it")

    while i < len(lines):
        line = lines[i]
        n = i + 1
        if line == "":
            previous_blank = True
            i += 1
            continue
        if line.startswith("#"):
            need_blank(n)
            m = re.match(r"^(##|###) (\S.*)$", line)
            if not m:
                raise DraftError(where, n, "a heading is `## ` or `### `; the title alone is `# `")
            words = m.group(2)
            if any(mark in words for mark in ("*", "`", "[", "]")):
                raise DraftError(where, n, "a heading has no marks")
            check_plain(words, where, n)
            if len(words) > MAX_HEADING:
                raise DraftError(where, n, f"a heading over {MAX_HEADING} characters")
            blocks.append({"heading": words, "level": len(m.group(1))})
            headings.append(words)
            i += 1
        elif line.startswith("```"):
            need_blank(n)
            if line != "```":
                raise DraftError(where, n, "a code fence is three backticks alone, with no info string")
            end = i + 1
            while end < len(lines) and lines[end] != "```":
                if lines[end].startswith("```"):
                    raise DraftError(where, end + 1, "a code fence is three backticks alone")
                end += 1
            if end == len(lines):
                raise DraftError(where, n, "a code fence with no fence to close it")
            code = "\n".join(lines[i + 1:end])
            if not code:
                raise DraftError(where, n, "an empty code block")
            if len(code.encode("utf-8")) >= MAX_CODE:
                raise DraftError(where, n, "a code block of 16 KiB or more")
            if ADDRESS.search(code):
                raise DraftError(where, n, "an address in a code block: no URL of any kind")
            blocks.append({"code": code})
            i = end + 1
        elif line.startswith(">"):
            need_blank(n)
            start, parts = n, []
            while i < len(lines) and lines[i].startswith(">"):
                if not lines[i].startswith("> ") or lines[i] == "> ":
                    raise DraftError(where, i + 1, "every line of a note starts `> ` and has words")
                parts.append(lines[i][2:].strip())
                i += 1
            words = " ".join(parts)
            if not words.startswith("**Note.** "):
                raise DraftError(where, start, "a note starts with **Note.**")
            blocks.append({"note": spans(words, where, start, links)})
        elif line.startswith("|"):
            need_blank(n)
            if i + 1 >= len(lines) or not TABLE_RULE.match(lines[i + 1]):
                raise DraftError(where, n, "a table is a header row, then its rule, then its rows")
            labels = cells(line)
            if len(labels) > MAX_COLUMNS or not all(labels):
                raise DraftError(where, n, f"a table has 1 to {MAX_COLUMNS} columns, each with its label")
            for label in labels:
                check_cell(label, where, n)
                if any(slot in label for slot in SLOTS):
                    raise DraftError(where, n, "a slot in a table's header")
            if len(cells(lines[i + 1])) != len(labels):
                raise DraftError(where, n + 1, "the table's rule has another number of cells")
            ids = column_ids(labels)
            rows = []
            i += 2
            while i < len(lines) and lines[i].startswith("|"):
                row = cells(lines[i])
                if not lines[i].endswith("|") or len(row) != len(labels):
                    raise DraftError(where, i + 1, f"a table's row of {len(row)} cells, not {len(labels)}")
                for cell in row:
                    check_cell(cell, where, i + 1)
                rows.append(dict(zip(ids, row)))
                i += 1
            if not rows or len(rows) > MAX_ROWS:
                raise DraftError(where, n, f"a table has 1 to {MAX_ROWS} rows")
            blocks.append({"table": {"columns": [{"id": c, "label": l} for c, l in zip(ids, labels)], "rows": rows}})
        elif line.startswith("- ") or ORDERED.match(line):
            need_blank(n)
            ordered = not line.startswith("- ")
            items = []
            while i < len(lines) and lines[i] != "":
                item_line = i + 1
                head = lines[i]
                if ordered:
                    m = ORDERED.match(head)
                    if not m:
                        raise DraftError(where, item_line, "a numbered list's item starts `<n>. `")
                    if int(m.group(1)) != len(items) + 1:
                        raise DraftError(where, item_line, f"a numbered list's item {m.group(1)}, not "
                                                           f"{len(items) + 1}: numbered from 1, in order")
                    words = [m.group(2)]
                elif head.startswith("- "):
                    words = [head[2:]]
                else:
                    raise DraftError(where, item_line, "a list's items are all `- ` or all numbered")
                i += 1
                while i < len(lines) and lines[i].startswith("  "):
                    rest = lines[i].strip()
                    if rest.startswith("- ") or ORDERED.match(rest) or rest.startswith(("> ", "|", "```", "#")):
                        raise DraftError(where, i + 1, "a list has one level, and an item holds words alone")
                    words.append(rest)
                    i += 1
                if i < len(lines) and lines[i] != "" and not (lines[i].startswith("- ") or ORDERED.match(lines[i])):
                    raise DraftError(where, i + 1, "a continuation line of a list's item is indented")
                items.append(spans(" ".join(words), where, item_line, links))
            if len(items) > MAX_ITEMS:
                raise DraftError(where, n, f"a list of more than {MAX_ITEMS} items")
            blocks.append({"list": items, "ordered": ordered})
        elif line.startswith(" "):
            raise DraftError(where, n, "an indented line outside a list's item: nested blocks are outside the format")
        else:
            need_blank(n)
            start, parts = n, []
            while i < len(lines) and lines[i] != "":
                here = lines[i]
                if here.startswith(("#", "```", ">", "|", "- ", " ")) or ORDERED.match(here):
                    raise DraftError(where, i + 1, "a block starts with no blank line before it")
                parts.append(here)
                i += 1
            blocks.append({"paragraph": spans(" ".join(parts), where, start, links)})
        previous_blank = False
    if not blocks:
        raise DraftError(where, 1, "a document with no blocks")
    if len(blocks) > MAX_BLOCKS:
        raise DraftError(where, 1, f"more than {MAX_BLOCKS} blocks")
    return title, blocks, headings, links


def body_text(title, blocks):
    return json.dumps({"blocks": blocks, "title": title}, ensure_ascii=False, separators=(",", ":"), sort_keys=True)


def build():
    """Every document, checked: [(id, title, headings, body)], or a DraftError."""
    readme = (DOCS / "README.md").read_text(encoding="utf-8")
    docs = listed(readme)
    if not docs:
        raise DraftError("src/en-overlay/docs/README.md", 1, "no table of the documents' ids and titles")
    ids = [d for d, _ in docs]
    for doc_id in ids:
        if not DOC_ID.match(doc_id) or ids.count(doc_id) != 1:
            raise DraftError("src/en-overlay/docs/README.md", 1, f"{doc_id!r} is no document id, or is listed twice")
    files = sorted(p.stem for p in DOCS.glob("*.md") if p.name != "README.md")
    if sorted(ids) != files:
        raise DraftError("src/en-overlay/docs/README.md", 1, "its table lists %s, and the folder holds %s" %
                         (sorted(ids), files))
    converted = {}
    for doc_id, title in docs:
        converted[doc_id] = convert(doc_id, (DOCS / f"{doc_id}.md").read_text(encoding="utf-8"), title)
    slotted = []
    for doc_id, (title, blocks, headings, links) in converted.items():
        where = f"src/en-overlay/docs/{doc_id}.md"
        for doc, heading, line in links:
            if doc not in converted:
                raise DraftError(where, line, f"a link to {doc!r}, which is not one of the documents")
            if heading is not None and heading not in converted[doc][2]:
                raise DraftError(where, line, f"a link to {doc}'s heading {heading!r}, which it does not have")
        text = body_text(title, blocks)
        if len(text.encode("utf-8")) >= MAX_DOC_BYTES:
            raise DraftError(where, 1, "its JSON is 64 KiB or more")
        if DELIMITER in text or DELIMITER in " · ".join(headings):
            raise DraftError(where, 1, f"it holds {DELIMITER!r}, the generated literal's delimiter")
        if any(slot in text for slot in SLOTS):
            slotted.append(doc_id)
    if slotted not in ([], ["about"]):
        raise DraftError("src/en-overlay/docs/README.md", 1, "About alone holds slots, and %s do" % slotted)
    return [(d, converted[d][0], " · ".join(converted[d][2]), body_text(converted[d][0], converted[d][1]))
            for d in ids]


def literal(text, indent):
    """Text as adjacent raw string literals, each line within MAX_LINE characters."""
    pieces = [text[k:k + CHUNK] for k in range(0, len(text), CHUNK)] or [""]
    return "\n".join(" " * indent + 'R"docs(' + p + ')docs"' for p in pieces)


def cpp(docs):
    out = [
        "// SPDX-License-Identifier: MIT",
        "// Generated by tools/docs/build.py from src/en-overlay/docs/: do not edit (PAGES §4.6.9; GraceUI D251).",
        "// EldenNet's bundled documents, in Docs' order: each one's id, title, headings (joined, for the list's",
        "// search) and body, the protocol's JSON as nlohmann/json writes it, for the space's `docs` family",
        "// (docs.hpp).",
        '#include "en/overlay/docs.hpp"',
        "",
        "namespace en::overlay {",
        "",
        "namespace {",
        "",
        "constexpr BundledDoc kDocs[] = {",
    ]
    for doc_id, title, headings, body in docs:
        out.append("    {")
        out.append(literal(doc_id, 8) + ",")
        out.append(literal(title, 8) + ",")
        out.append(literal(headings, 8) + ",")
        out.append(literal(body, 8) + "},")
    out += [
        "};",
        "",
        "}  // namespace",
        "",
        "std::span<const BundledDoc> bundled_docs() noexcept { return kDocs; }",
        "",
        "}  // namespace en::overlay",
        "",
    ]
    return "\n".join(out)


def run(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if a draft breaks a rule or the output is stale")
    args = parser.parse_args(argv)
    try:
        docs = build()
    except (DraftError, OSError) as error:
        print(f"docs: {error}", file=sys.stderr)
        return 1
    text = cpp(docs)
    for n, line in enumerate(text.splitlines(), 1):
        if len(line) > MAX_LINE:
            print(f"docs: docs_data.cpp's line {n} is over {MAX_LINE} characters", file=sys.stderr)
            return 1
    blocks = sum(len(json.loads(b)["blocks"]) for _, _, _, b in docs)
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text(encoding="utf-8") != text:
            print("docs: src/en-overlay/src/docs_data.cpp is not what the drafts build: run "
                  "python3 -I tools/docs/build.py", file=sys.stderr)
            return 1
        print(f"docs: ok ({len(docs)} documents, {blocks} blocks)")
        return 0
    OUTPUT.write_text(text, encoding="utf-8")
    print(f"docs: wrote src/en-overlay/src/docs_data.cpp ({len(docs)} documents, {blocks} blocks)")
    return 0


if __name__ == "__main__":
    sys.exit(run(sys.argv[1:]))
