#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tools/docs/test_build.py: the documents' converter (tools/docs/build.py), on invented drafts of our own.

    python3 -I tools/docs/test_build.py

Every construct of the format becomes its block or span; a draft outside the format fails, naming its line; and the
bundled drafts themselves convert within GraceUI's limits (D251), their links to documents and headings that exist.
Standard library only.
"""
import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build  # noqa: E402

GOOD = """# A keeper's guide

The lamp burns all night. Read **Oil** first, or `oil.toml`, or [the storms](doc:storms#Wind) and
[the guide](doc:storms).

## Lighting

- Clean the glass.
- Fill the oil to the line,
  never above it.

### The match

1. Strike it away from the glass.
2. Light the wick at its edge.

> **Note.** Never leave a lit lamp alone in its
> first quarter hour.

```
dusk  trimmed
night lit
```

| Hour | Task |
|---|---|
| 6 | Trim |
| Dusk | Light |
"""


def convert(text, title="A keeper's guide"):
    return build.convert("guide", text, title)


class Converts(unittest.TestCase):
    def test_every_construct(self):
        title, blocks, headings, links = convert(GOOD)
        self.assertEqual(title, "A keeper's guide")
        self.assertEqual(headings, ["Lighting", "The match"])
        self.assertEqual(links, [("storms", "Wind", 3), ("storms", None, 3)])
        self.assertEqual(blocks[0], {"paragraph": [
            {"text": "The lamp burns all night. Read "}, {"strong": True, "text": "Oil"}, {"text": " first, or "},
            {"code": True, "text": "oil.toml"}, {"text": ", or "},
            {"doc": "storms", "heading": "Wind", "text": "the storms"}, {"text": " and "},
            {"doc": "storms", "text": "the guide"}, {"text": "."}]})
        self.assertEqual(blocks[1], {"heading": "Lighting", "level": 2})
        self.assertEqual(blocks[2], {"list": [[{"text": "Clean the glass."}],
                                              [{"text": "Fill the oil to the line, never above it."}]],
                                     "ordered": False})
        self.assertEqual(blocks[3], {"heading": "The match", "level": 3})
        self.assertEqual(blocks[4]["ordered"], True)
        self.assertEqual(len(blocks[4]["list"]), 2)
        self.assertEqual(blocks[5], {"note": [{"strong": True, "text": "Note."},
                                              {"text": " Never leave a lit lamp alone in its first quarter hour."}]})
        self.assertEqual(blocks[6], {"code": "dusk  trimmed\nnight lit"})
        self.assertEqual(blocks[7], {"table": {"columns": [{"id": "hour", "label": "Hour"},
                                                           {"id": "task", "label": "Task"}],
                                               "rows": [{"hour": "6", "task": "Trim"},
                                                        {"hour": "Dusk", "task": "Light"}]}})

    def test_slots_in_cells(self):
        _, blocks, _, _ = convert("# A\n\n| Part | Version |\n|---|---|\n| EldenNet | {eldennet_version} |\n", "A")
        self.assertEqual(blocks[0]["table"]["rows"], [{"part": "EldenNet", "version": "{eldennet_version}"}])


class Refuses(unittest.TestCase):
    CASES = [
        ("no title", "Words.\n", 1),
        ("another title", "# B\n\nWords.\n", 1),
        ("a fourth level", "# A\n\n#### Deep\n", 3),
        ("a heading's marks", "# A\n\n## **Loud**\n", 3),
        ("an image", "# A\n\n![lamp](lamp.png)\n", 3),
        ("a web link", "# A\n\n[a site](https://example.invalid/x)\n", 3),
        ("a bare address", "# A\n\nSee example.invalid://host.\n", 3),
        ("an address in code", "# A\n\nOpen `http://127.0.0.1:8642/`.\n", 3),
        ("HTML", "# A\n\nA <b>bold</b> word.\n", 3),
        ("a placeholder outside code", "# A\n\nThe <folder> here.\n", 3),
        ("one star", "# A\n\nAn *emphasis*.\n", 3),
        ("a backslash's escape", "# A\n\nA \\_ mark.\n", 3),
        ("nested marks", "# A\n\n**`both`**\n", 3),
        ("an open strong", "# A\n\n**never closed\n", 3),
        ("a nested list", "# A\n\n- one\n  - two\n", 4),
        ("a list numbered from 2", "# A\n\n2. two\n", 3),
        ("a list out of order", "# A\n\n1. one\n3. three\n", 4),
        ("mixed lists", "# A\n\n- one\n1. two\n", 4),
        ("a note without its words", "# A\n\n> Careful.\n", 3),
        ("a fence with an info string", "# A\n\n```toml\nx = 1\n```\n", 3),
        ("an open fence", "# A\n\n```\nx = 1\n", 3),
        ("a table without its rule", "# A\n\n| a | b |\n| 1 | 2 |\n", 3),
        ("a cell with marks", "# A\n\n| a |\n|---|\n| **b** |\n", 5),
        ("a row of other cells", "# A\n\n| a | b |\n|---|---|\n| 1 |\n", 5),
        ("a slot in a paragraph", "# A\n\nVersion {build}.\n", 3),
        ("no blank line before a block", "# A\n\nWords.\n- an item\n", 4),
        ("an indented paragraph", "# A\n\n    code by indent\n", 3),
        ("a long line", "# A\n\n" + "w" * 121 + "\n", 3),
        ("a tab", "# A\n\nA\tword.\n", 3),
        ("a space at a line's end", "# A\n\nWords. \n", 3),
        ("no final line break", "# A\n\nWords.", 3),
        ("a link to no document id", "# A\n\n[x](doc:Not An Id)\n", 3),
        ("a paragraph too long", "# A\n\n" + ("word " * 19 + "word\n") * 21, 3),
    ]

    def test_each(self):
        for what, text, line in self.CASES:
            with self.subTest(what):
                with self.assertRaises(build.DraftError) as caught:
                    convert(text, "A")
                self.assertIn(f"guide.md:{line}:", str(caught.exception))


class Bundled(unittest.TestCase):
    def test_the_drafts(self):
        docs = build.build()
        ids = [d for d, _, _, _ in docs]
        self.assertEqual(len(ids), 10)
        self.assertEqual(ids[0], "getting-started")
        self.assertEqual(ids[-1], "about")
        headings = {}
        for doc_id, title, joined, body in docs:
            parsed = json.loads(body)
            self.assertEqual(parsed["title"], title)
            self.assertLess(len(body.encode("utf-8")), 64 * 1024)
            self.assertLessEqual(len(parsed["blocks"]), 400)
            headings[doc_id] = [b["heading"] for b in parsed["blocks"] if "heading" in b]
            self.assertEqual(joined, " · ".join(headings[doc_id]))
        for doc_id, _, _, body in docs:
            for block in json.loads(body)["blocks"]:
                for spans in [block.get("paragraph"), block.get("note")] + list(block.get("list", [])):
                    for span in spans or []:
                        if "doc" in span:
                            self.assertIn(span["doc"], headings, doc_id)
                            if "heading" in span:
                                self.assertIn(span["heading"], headings[span["doc"]], doc_id)

    def test_the_generated_file(self):
        self.assertEqual(build.OUTPUT.read_text(encoding="utf-8"), build.cpp(build.build()))


if __name__ == "__main__":
    unittest.main()
