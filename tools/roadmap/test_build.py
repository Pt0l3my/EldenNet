"""Tests for tools/roadmap/build.py: the parser, the checks, the references, the statuses and the page.

  python3 -I -m unittest discover -s tools/roadmap

Adapted from the tests of the owner's Skyrizz roadmap tool, with EldenNet's references: the wp index's
codes, the owner's decisions (O-ids) and GraceUI's codes.
"""
import contextlib
import io
import json
import pathlib
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build  # noqa: E402

SAMPLE = """<!--
## S9 Not a stage, inside the comment
-->
# A roadmap

- updated: 2026-11-07
- refs: DESIGN§1

Its note.

## S0 First [done]

### S0.1 One step

- S0.1.a A part with a long title that wraps
  onto the next line [done]
  - refs: DESIGN§1, docs/x.md, O1
  - gives: something
    and more.
- b Second part
  - needs: a
  - refs: T-0001, G1, GraceUI:DESIGN§2

## S1 Second

- needs: S0

### S1.1 Two
- needs: S0.1

A step's note.

- S1.1.a Third, with a page's name [Pacing]
  - needs: S0.1.b
  - status: waiting
"""

DECISIONS = """# Owner decisions

- **O1–O2** were accepted together.

## O1

**Project name** · *2026-11-06 · accepted*

## O2

**Process model** · *2026-11-06*
"""

GRACE_DESIGN = """# GraceUI: design

## 2. Architecture

### 2.6 The in-game renderer

## 12. Decisions for the owner

| # | Question | Options | Decision |
|---|---|---|---|
| G1 | Which renderer? | (a) or (b) | (a) |

## 13. Decision log

| # | Subject | Decision |
|---|---|---|
| D5 | Fonts | OFL only |
"""


class FakeIndex:
    """Codes as the wp index resolves them: DESIGN§1, SN04, and MEM§5 only by its first chunk."""
    by_id = {"DESIGN§1": {"title": "1. Goals, scope and success criteria", "doc": "docs/design/DESIGN.md", "lines": [57, 58]},
             "SN04": {"title": "(start of 04-memory-and-knowledge.md)", "doc": "docs/research/skyrimnet/04-memory-and-knowledge.md", "lines": [1, 9]},
             "MEM§5~1": {"title": "5. Knowledge base", "doc": "docs/design/memory-and-knowledge.md", "lines": [583, 600]}}

    def resolve(self, code):
        return [code] if code in self.by_id else []


TASKS = {"T-0001": {"id": "T-0001", "status": "done", "title": "A task"},
         "T-0002": {"id": "T-0002", "status": "open", "title": "Another"}}


def parsed(text=SAMPLE):
    roadmap, problems = build.parse(text)
    build.normalise_needs(roadmap)
    return roadmap, problems


class Paths:
    """A temporary EldenNet checkout with docs/x.md and docs/DECISIONS.md, and, unless grace is False,
    GraceUI's checkout beside it."""

    def __init__(self, grace=True, decisions=True):
        self.grace, self.decisions = grace, decisions

    def __enter__(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = pathlib.Path(self.tmp.name)
        root = base / "eldennet"
        (root / "docs/design").mkdir(parents=True)
        (root / "docs/x.md").write_text("x", encoding="utf-8")
        if self.decisions:
            (root / "docs/DECISIONS.md").write_text(DECISIONS, encoding="utf-8")
        (root / "docs/design/DESIGN.md").write_text(
            "# Design\n\n## 19. Decisions for the project owner\n\n| # | Decision |\n|---|---|\n"
            "| O1 | Project name |\n| O7 | Default provider |\n\n## 20. After\n\n| O8 | Not in 19 |\n", encoding="utf-8")
        if self.grace:
            (base / "graceui/docs/research").mkdir(parents=True)
            (base / "graceui/docs/DESIGN.md").write_text(GRACE_DESIGN, encoding="utf-8")
            (base / "graceui/docs/eldennet-changes.md").write_text("x", encoding="utf-8")
        self.saved, build.ROOT = build.ROOT, root
        return root

    def __exit__(self, *exc):
        build.ROOT = self.saved
        self.tmp.cleanup()


class ParseTests(unittest.TestCase):
    def test_structure(self):
        roadmap, problems = parsed()
        self.assertEqual(problems, [])
        self.assertEqual([s["id"] for s in roadmap["stages"]], ["S0", "S1"])
        a, b = roadmap["stages"][0]["steps"][0]["parts"]
        self.assertEqual(a["title"], "A part with a long title that wraps onto the next line")
        self.assertEqual(a["status"], "done")
        self.assertEqual(a["refs"], ["DESIGN§1", "docs/x.md", "O1"])
        self.assertEqual(a["gives"], "something and more.")
        self.assertEqual(b["id"], "S0.1.b")
        self.assertEqual(b["needs"], ["S0.1.a"])               # a bare letter is a part of its own step
        self.assertEqual(b["refs"], ["T-0001", "G1", "GraceUI:DESIGN§2"])

    def test_fields_belong_to_their_heading(self):
        roadmap, _ = parsed()
        self.assertEqual(roadmap["meta"]["updated"], "2026-11-07")
        self.assertEqual(roadmap["meta"]["refs"], ["DESIGN§1"])
        self.assertEqual(roadmap["meta"]["note"], "Its note.")
        stage1 = roadmap["stages"][1]
        self.assertEqual(stage1["needs"], ["S0"])
        self.assertEqual(stage1["steps"][0]["needs"], ["S0.1"])
        self.assertEqual(stage1["steps"][0]["note"], "A step's note.")

    def test_a_page_name_is_no_status(self):
        roadmap, _ = parsed()
        c = roadmap["stages"][1]["steps"][0]["parts"][0]
        self.assertEqual(c["title"], "Third, with a page's name [Pacing]")
        self.assertEqual(c["status"], "waiting")

    def test_the_comment_is_skipped(self):
        roadmap, _ = parsed()
        self.assertNotIn("S9", [s["id"] for s in roadmap["stages"]])

    def test_bad_ids(self):
        _, problems = parsed("## Features\n\n### S3.1 x\n\n### S1.2 y\n- S2.1.a stray\n")
        msgs = " ".join(m for _, m in problems)
        self.assertIn("a stage's heading starts with its id", msgs)
        self.assertIn("not 'S1.2'", msgs)
        self.assertIn("is not under its own step", msgs)

    def test_a_graceui_reference_on_a_wrapped_line_is_no_field(self):
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- a X\n  - refs: O1,\n    GraceUI:DESIGN§10, G4\n")
        self.assertEqual(problems, [])
        self.assertEqual(roadmap["stages"][0]["steps"][0]["parts"][0]["refs"], ["O1", "GraceUI:DESIGN§10", "G4"])


class TypoTests(unittest.TestCase):
    """Typos are reported, never swallowed into a note, a title or a part's id."""

    def messages(self, text):
        return [m for _, m in parsed(text)[1]]

    def test_unknown_field_is_reported(self):
        out = self.messages("## S0 A\n### S0.1 B\n- a X\n  - need: S0.1.b\n  - refs: O1\n    nedds: c\n")
        self.assertEqual(sum("unknown field" in m for m in out), 2, out)
        self.assertTrue(any("'need'" in m for m in out) and any("'nedds'" in m for m in out), out)
        out = self.messages("## S0 A\n### S0.1 B\n- note: a stray field under a step\n- a X\n")
        self.assertTrue(any("unknown field 'note'" in m for m in out), out)

    def test_a_wrapped_line_with_a_capital_and_colon_is_no_field(self):
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- a X and\n  GraceUI: its pages\n")
        self.assertEqual(problems, [])
        self.assertEqual(roadmap["stages"][0]["steps"][0]["parts"][0]["title"], "X and GraceUI: its pages")
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- a X\n  - gives: a page at\n    https://example.org\n")
        self.assertEqual(problems, [])
        self.assertEqual(roadmap["stages"][0]["steps"][0]["parts"][0]["gives"], "a page at https://example.org")

    def test_a_tab_is_indentation(self):
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- a X\n\t- refs: O1\n\t- needs: S0\n")
        self.assertEqual(problems, [])
        step = roadmap["stages"][0]["steps"][0]
        self.assertEqual(step["parts"][0]["refs"], ["O1"])
        self.assertEqual(step["parts"][0]["needs"], ["S0"])
        self.assertEqual(step["refs"], [])

    def test_a_misspelt_status_is_reported_and_left_out_of_the_title(self):
        for written, meant in (("[Planed]", "planned"), ("[next ]", "next"), ("[DONE]", "done"),
                               ("[wating]", "waiting"), ("[ idea]", "idea"), ("[Active]", "active")):
            roadmap, problems = parsed(f"## S0 A\n### S0.1 B\n- a A title {written}\n")
            part = roadmap["stages"][0]["steps"][0]["parts"][0]
            self.assertEqual(part["title"], "A title", written)
            self.assertEqual(part["status"], meant, written)
            self.assertTrue(any(f"write [{meant}]" in m for _, m in problems), (written, problems))
        out = self.messages("## S0 A [Don]\n### S0.1 B [nxet]\n- a X\n")
        self.assertEqual(sum("misspelt" in m for m in out), 2, out)

    def test_a_page_name_or_other_word_is_no_status(self):
        for word in ("[Pacing]", "[Events]", "[Activity]", "[Queue]"):
            roadmap, problems = parsed(f"## S0 A\n### S0.1 B\n- a The pages' {word}\n")
            self.assertEqual(problems, [], word)
            self.assertEqual(roadmap["stages"][0]["steps"][0]["parts"][0]["title"], f"The pages' {word}")

    def test_an_article_is_no_part_id(self):
        out = self.messages("## S0 A\n### S0.1 B\n- a new part with no id\n")
        self.assertTrue(any("a part with no id" in m and "S0.1.a" in m for m in out), out)
        out = self.messages("## S0 A\n### S0.1 B\n- a First\n- an adapter with no id\n")
        self.assertTrue(any("a part with no id" in m and "S0.1.b" in m for m in out), out)
        roadmap, _ = parsed("## S0 A\n### S0.1 B\n- an adapter with no id\n")
        part = roadmap["stages"][0]["steps"][0]["parts"][0]
        self.assertNotIn(part["id"], ("S0.1.a", "S0.1.an"))
        self.assertEqual(part["title"], "an adapter with no id")

    def test_a_lower_case_title_needs_its_full_id(self):
        out = self.messages("## S0 A\n### S0.1 B\n- a First\n- b wp, the index\n")
        self.assertTrue(any("a part with no id" in m and "S0.1.b" in m for m in out), out)
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- a First\n- S0.1.b wp, the index\n")
        self.assertEqual(problems, [])
        self.assertEqual(roadmap["stages"][0]["steps"][0]["parts"][1]["title"], "wp, the index")

    def test_a_letter_out_of_sequence_is_no_id(self):
        out = self.messages("## S0 A\n### S0.1 B\n- a X\n- c Y\n")
        self.assertTrue(any("a part with no id" in m and "(b)" in m for m in out), out)
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- S0.1.a X\n- b Y\n- S0.1.c Z\n- d W\n")
        self.assertEqual(problems, [])
        self.assertEqual([p["id"] for p in roadmap["stages"][0]["steps"][0]["parts"]],
                         ["S0.1.a", "S0.1.b", "S0.1.c", "S0.1.d"])

    def test_letters_after_z(self):
        self.assertEqual([build.next_letter(x) for x in ("", "a", "y", "z", "az", "bz")], ["a", "b", "z", "aa", "ba", "ca"])

    def test_an_empty_title_is_reported(self):
        out = self.messages("## S0 A\n### S0.1\n- S0.1.a [done]\n- b\n")
        self.assertTrue(any("S0.1 has no title" in m for m in out), out)
        self.assertTrue(any("S0.1.a has no title" in m for m in out), out)
        self.assertTrue(any("S0.1.b has no title" in m for m in out), out)
        roadmap, problems = parsed("## S0 A\n### S0.1 B\n- S0.1.a\n  Its title, on the next line [done]\n")
        self.assertEqual(problems, [])
        self.assertEqual(roadmap["stages"][0]["steps"][0]["parts"][0]["title"], "Its title, on the next line")


class CheckTests(unittest.TestCase):
    def problems(self, text, **paths):
        roadmap, problems = parsed(text)
        with Paths(**paths):
            return [m for _, m in build.check(roadmap, problems, TASKS, FakeIndex())]

    def bad_refs(self, refs, **paths):
        out = self.problems(f"## S0 A\n### S0.1 B\n- a X\n  - refs: {refs}\n", **paths)
        return sorted(re.search(r"'(.+?)'", m).group(1) for m in out)

    def test_sample_is_clean(self):
        self.assertEqual(self.problems(SAMPLE), [])

    def test_duplicate_id(self):
        out = self.problems("## S0 A\n### S0.1 B\n- S0.1.a X\n- S0.1.a Y\n")
        self.assertTrue(any("S0.1.a is used twice" in m for m in out), out)

    def test_unknown_need(self):
        out = self.problems("## S0 A\n### S0.1 B\n- a X\n  - needs: S4.2.c, b\n")
        self.assertTrue(any("needs 'S4.2.c'" in m for m in out), out)
        self.assertTrue(any("needs 'S0.1.b'" in m for m in out), out)

    def test_unknown_status(self):
        out = self.problems("## S0 A [finished]\n### S0.1 B\n- a X\n  - status: soon\n")
        self.assertEqual(sum("unknown status" in m for m in out), 2, out)

    def test_index_codes_tasks_and_paths(self):
        self.assertEqual(self.bad_refs("DESIGN§1, SN04, MEM§5, DESIGN§99, REVIEW, T-0001, T-0999, docs/x.md, docs/y.md"),
                         ["DESIGN§99", "REVIEW", "T-0999", "docs/y.md"])

    def test_a_path_never_leaves_the_repository(self):
        self.assertEqual(self.bad_refs("../eldennet/docs/x.md, GraceUI:../eldennet/docs/x.md, GraceUI:/etc"),
                         ["../eldennet/docs/x.md", "GraceUI:../eldennet/docs/x.md", "GraceUI:/etc"])

    def test_owner_decisions_from_the_decisions_file(self):
        self.assertEqual(self.bad_refs("O1, O2, O7, O99"), ["O7", "O99"])     # O7 is only in DESIGN §19

    def test_owner_decisions_from_design_19_until_the_file_exists(self):
        self.assertEqual(self.bad_refs("O1, O7, O2, O8", decisions=False), ["O2", "O8"])   # O8 sits in §20

    def test_graceui_references_checked_against_its_checkout(self):
        self.assertEqual(self.bad_refs("G1, G11, GraceUI:DESIGN§2, GraceUI:DESIGN§2.6, GraceUI:DESIGN§7, "
                                       "GraceUI:D5, GraceUI:D99, GraceUI:docs/eldennet-changes.md, GraceUI:docs/nope.md"),
                         ["G11", "GraceUI:D99", "GraceUI:DESIGN§7", "GraceUI:docs/nope.md"])

    def test_graceui_references_accepted_without_its_checkout(self):
        self.assertEqual(self.bad_refs("G1, G11, GraceUI:DESIGN§7, GraceUI:D99, GraceUI:docs/nope.md", grace=False), [])

    def test_cycle(self):
        out = self.problems("## S0 A\n### S0.1 B\n- a X\n  - needs: b\n- b Y\n  - needs: a\n")
        self.assertTrue(any("circle" in m for m in out), out)

    def test_cycle_through_a_step(self):
        out = self.problems("## S0 A\n### S0.1 B\n- needs: S0.2\n- a X\n### S0.2 C\n- a Y\n  - needs: S0.1.a\n")
        self.assertTrue(any("circle" in m for m in out), out)

    def test_no_false_cycle_between_steps(self):
        # S0.1.b needs S0.2.a, which needs S0.1.a: two steps lean on each other, but no part does
        out = self.problems("## S0 A\n### S0.1 B\n- a X\n- b Y\n  - needs: S0.2.a\n### S0.2 C\n- a Z\n  - needs: S0.1.a\n")
        self.assertEqual(out, [])


class EntryTests(unittest.TestCase):
    """How an id's entry and title are found in a decisions file or table."""

    def test_forms(self):
        text = ("O1–O3 were decided together.\n\n## O1\n\n**Project name** · *2026-11-06*\n\n"
                "## O2 Process model\n\n- **O3** (2026-11-06) Game-access backend\n\n"
                "| O4 | How EldenNet starts | own launcher |\n\n```\n## O5 in a fence\n```\n")
        self.assertEqual(build.entry_titles(text, r"O\d+"),
                         {"O1": "Project name", "O2": "Process model", "O3": "Game-access backend",
                          "O4": "How EldenNet starts"})

    def test_a_heading_wins_over_prose_and_lists(self):
        text = "- **O1–O24** were accepted.\n\nO1 is first in prose.\n\n## O1\n\nThe project's name · more\n"
        self.assertEqual(build.entry_titles(text, r"O\d+"), {"O1": "The project's name"})

    def test_an_id_needs_a_boundary(self):
        self.assertEqual(build.entry_titles("## O12a Not an id\n## O1x\n## O10 Ten\n", r"O\d+"), {"O10": "Ten"})


class StatusTests(unittest.TestCase):
    def settle(self, text):
        roadmap, _ = parsed(text)
        build.settle_statuses(roadmap, TASKS)
        return {it["id"]: it["status"] for it in build.walk(roadmap)}

    def test_tasks_give_a_status(self):
        s = self.settle("## S0 A\n### S0.1 B\n- a X\n  - refs: T-0001\n- b Y\n  - refs: T-0001, T-0002\n"
                        "- c Z\n  - refs: T-0002\n  - status: next\n- d W\n")
        self.assertEqual((s["S0.1.a"], s["S0.1.b"], s["S0.1.c"], s["S0.1.d"]), ("done", "planned", "next", "planned"))

    def test_no_tasks_yet(self):
        roadmap, _ = parsed("## S0 A\n### S0.1 B\n- a X\n  - refs: O1\n- b Y [done]\n")
        build.settle_statuses(roadmap, {})
        self.assertEqual([p["status"] for p in roadmap["stages"][0]["steps"][0]["parts"]], ["planned", "done"])
        self.assertEqual(roadmap["stages"][0]["steps"][0]["status"], "active")

    def test_derived(self):
        d = build.derive
        self.assertEqual(d(["done", "done"]), "done")
        self.assertEqual(d(["done", "idea"]), "done")
        self.assertEqual(d(["done", "planned"]), "active")
        self.assertEqual(d(["planned", "next"]), "next")
        self.assertEqual(d(["waiting", "idea"]), "waiting")
        self.assertEqual(d(["waiting", "planned"]), "planned")
        self.assertEqual(d([]), "planned")


def page_data(text=SAMPLE, **paths):
    roadmap, _ = parsed(text)
    with Paths(**paths):
        build.settle_statuses(roadmap, TASKS)
        return build.page_data(roadmap, TASKS, FakeIndex())


class PageTests(unittest.TestCase):
    def test_page_embeds_the_data(self):
        data = page_data()
        data["note"] = "a </script> in a note"
        html = build.render(data)
        self.assertNotIn("/*ROADMAP-DATA*/", html)
        self.assertIn("<title>EldenNet Roadmap</title>", html)
        self.assertNotIn("a </script> in", html)
        blob = re.search(r"const DATA = (\{.*?\});\n", html, re.S).group(1)
        back = json.loads(blob.replace("<\\/", "</"))
        self.assertEqual(back["refs"]["O1"]["title"], "Project name")
        self.assertEqual(back["refs"]["O1"]["where"], "docs/DECISIONS.md")
        self.assertEqual(back["refs"]["T-0001"]["title"], "A task")
        self.assertEqual(back["refs"]["G1"]["kind"], "graceui")
        self.assertEqual(back["stages"][0]["steps"][0]["parts"][1]["status"], "done")

    def test_where_names_the_document_never_a_line(self):
        refs = page_data("## S0 A\n### S0.1 B\n- a X\n  - refs: DESIGN§1, MEM§5, SN04\n")["refs"]
        self.assertEqual(refs["DESIGN§1"]["where"], "docs/design/DESIGN.md")
        self.assertEqual(refs["MEM§5"]["where"], "docs/design/memory-and-knowledge.md")
        self.assertEqual(refs["SN04"]["title"], "04-memory-and-knowledge.md")
        for r in refs.values():
            self.assertNotRegex(r["where"], r":\d")

    def test_the_page_is_the_same_with_or_without_graceui(self):
        self.assertEqual(page_data(grace=True), page_data(grace=False))

    def test_a_graceui_folder_or_file_is_named_the_same_with_or_without_its_checkout(self):
        text = "## S0 A\n### S0.1 B\n- a X\n  - refs: GraceUI:docs/research, GraceUI:docs/eldennet-changes.md\n"
        for grace in (True, False):
            refs = page_data(text, grace=grace)["refs"]
            self.assertEqual(refs["GraceUI:docs/research"]["title"], "a folder in GraceUI", grace)
            self.assertEqual(refs["GraceUI:docs/eldennet-changes.md"]["title"], "a file in GraceUI", grace)

    def test_nothing_from_outside(self):
        html = build.TEMPLATE.read_text(encoding="utf-8")
        self.assertIsNone(re.search(r"<script[^>]+src=", html))
        self.assertIsNone(re.search(r"<link\b", html))
        self.assertIsNone(re.search(r"@import|url\(\s*['\"]?(?!data:|#)", html))     # url(#ar): a marker in the page
        self.assertEqual(re.findall(r"(?:href|src)=\"(https?:[^\"]+)\"", html), [])
        self.assertIn("default-src 'none'", html)

    def test_the_status_colours_read_on_a_card(self):
        html = build.TEMPLATE.read_text(encoding="utf-8")
        card = re.search(r"--card:\s*(#[0-9a-f]{6})", html).group(1)

        def lum(hex_):
            def ch(v):
                v /= 255
                return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
            r, g, b = (int(hex_[i:i + 2], 16) for i in (1, 3, 5))
            return 0.2126 * ch(r) + 0.7152 * ch(g) + 0.0722 * ch(b)

        for status in build.STATUSES:
            colour = re.search(rf"--st-{status}:\s*(#[0-9a-f]{{6}})", html).group(1)
            hi, lo = sorted((lum(colour), lum(card)), reverse=True)
            self.assertGreaterEqual((hi + 0.05) / (lo + 0.05), 4.5, status)


class StalePageTests(unittest.TestCase):
    """--check builds the page in memory and fails when the one on disk differs."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = pathlib.Path(self.tmp.name)
        self.source, self.page = root / "ROADMAP.md", root / "index.html"
        self.source.write_text("## S0 A\n### S0.1 B\n- a X\n  - refs: DESIGN§1, T-0002\n", encoding="utf-8")
        self.saved = build.workplan_index, build.load_tasks
        build.workplan_index, build.load_tasks = FakeIndex, lambda: dict(TASKS)

    def tearDown(self):
        build.workplan_index, build.load_tasks = self.saved
        self.tmp.cleanup()

    def run_main(self, *extra):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = build.main(["--source", str(self.source), "--out", str(self.page), *extra])
        return code, out.getvalue()

    def test_fresh_then_stale(self):
        self.assertEqual(self.run_main("--check")[0], 1)                # no page yet
        self.assertEqual(self.run_main()[0], 0)
        self.assertEqual(self.run_main("--check"), (0, "1 stages, 1 steps, 1 parts: 0 problem(s)\n"))
        self.source.write_text(self.source.read_text(encoding="utf-8").replace("- a X", "- a Y"), encoding="utf-8")
        code, out = self.run_main("--check")
        self.assertEqual(code, 1)
        self.assertIn("the page is not what the source", out)
        self.assertIn("1 problem(s)", out)

    def test_a_task_closed_makes_the_page_stale(self):
        self.run_main()
        build.load_tasks = lambda: {**TASKS, "T-0002": {**TASKS["T-0002"], "status": "done"}}
        self.assertEqual(self.run_main("--check")[0], 1)

    def test_check_writes_nothing(self):
        self.run_main("--check")
        self.assertFalse(self.page.exists())

    def test_no_local_path_is_printed(self):
        _, out = self.run_main()
        self.assertNotIn(self.tmp.name, out)

    def test_the_fragment_is_a_whole_document_without_the_content_security_line(self):
        frag = self.page.parent / "frag.html"
        self.assertEqual(self.run_main("--fragment", str(frag))[0], 0)
        text = frag.read_text(encoding="utf-8")
        self.assertTrue(text.lower().startswith("<!doctype html>"), text[:40])
        for tag in ("<html", "<head>", "</head>", "<body>", "</body>", "</html>", 'name="viewport"', "<title>"):
            self.assertIn(tag, text.lower())
        self.assertNotIn("content-security-policy", text.lower())
        self.assertIn("<script>", text)
        page = self.page.read_text(encoding="utf-8")
        self.assertEqual(re.sub(r'<meta http-equiv="Content-Security-Policy"[^>]*>\s*', "", page), text)


class SourceTests(unittest.TestCase):
    """The repository's own docs/ROADMAP.md checks clean, and its page is what it builds."""

    def test_the_roadmap_checks_clean(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = build.main(["--check"])
        self.assertEqual(code, 0, out.getvalue())

    def test_wp_reads_graceui_sections_as_graceui_s(self):
        sys.path.insert(0, str(build.ROOT / "tools/workplan"))
        from wplib.common import cited_codes
        self.assertEqual(cited_codes("refs: GraceUI:DESIGN§10, DESIGN§16.2"), ["DESIGN§16.2"])


if __name__ == "__main__":
    unittest.main()
