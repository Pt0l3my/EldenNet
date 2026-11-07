"""Tests for tools/showcase/build.py: the parser, the checks, the screenshots, the page and its
published copy.

  python3 -I -m unittest discover -s tools/showcase

Our own code, MIT (see LICENSE).
"""
import contextlib
import io
import pathlib
import re
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build  # noqa: E402

SAMPLE = """<!--
# Not the title, inside the comment
- updated: not a date, inside the comment
-->
# Testnet

- updated: 2026-11-07
- lede: A lede with the player's 'quoted' words.
- status: Design stage
- facts: One · Two
- roadmap-published: none

## What it is {#about}

- nav: About

A paragraph that wraps
onto a second line, with **bold**, *italic*, `co'de` and [a link](https://example.org/a?b=1&c=2).

- an item
  that wraps
- another item

## Features {#features}

- kind: cards

### A feature

- tag: Thing
- design: DESIGN§1, O1

Its text.

## The menu {#menu}

- kind: gallery

### Home

- image: home.webp
- from: home-1280x720.png

Its caption.

## Where it stands {#status}

- kind: status

Text before the stages.
"""

ROADMAP = """# R

- updated: 2026-11-06

## S0 First [done]

### S0.1 One

- S0.1.a A part [done]
- b Another [done]

## S1 Second

### S1.1 Two

- S1.1.a Waiting [waiting]
- b Next [next]
"""


def png(w, h, text=False):
    """A minimal valid PNG of w x h, grey, optionally with a tEXt chunk."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + b"\x40" * w for _ in range(h))
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    if text:
        out += chunk(b"tEXt", b"Comment\x00somewhere")
    return out + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


def webp_vp8l(w, h, exif=False):
    bits = (w - 1) | ((h - 1) << 14)
    body = b"\x2f" + bits.to_bytes(4, "little") + b"\x00" * 3
    chunks = b"VP8L" + struct.pack("<I", len(body)) + body + (b"\x00" if len(body) & 1 else b"")
    if exif:
        chunks += b"EXIF" + struct.pack("<I", 4) + b"abcd"
    return b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WEBP" + chunks


def webp_vp8x(w, h):
    body = b"\x00" * 4 + (w - 1).to_bytes(3, "little") + (h - 1).to_bytes(3, "little")
    chunks = b"VP8X" + struct.pack("<I", len(body)) + body
    return b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WEBP" + chunks


def webp_vp8(w, h):
    body = b"\x00\x00\x00" + b"\x9d\x01\x2a" + struct.pack("<HH", w, h)
    chunks = b"VP8 " + struct.pack("<I", len(body)) + body
    return b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WEBP" + chunks


class FakeIndex:
    """Codes as the wp index resolves them: DESIGN§1 only."""
    by_id = {"DESIGN§1": {"title": "1. Goals", "doc": "docs/design/DESIGN.md", "lines": [1, 2]}}

    def resolve(self, code):
        return [code] if code in self.by_id else []


def parsed(text=SAMPLE):
    page, problems = build.parse(text)
    return page, problems + build.check_fields(page)


class ParseTests(unittest.TestCase):
    def test_structure(self):
        page, problems = parsed()
        self.assertEqual(problems, [])
        self.assertEqual(page["title"], "Testnet")
        self.assertEqual(page["fields"]["updated"], "2026-11-07")
        self.assertEqual([s["id"] for s in page["items"]], ["about", "features", "menu", "status"])
        self.assertEqual([s["kind"] for s in page["items"]], ["prose", "cards", "gallery", "status"])
        self.assertEqual(page["items"][1]["items"][0]["fields"], {"tag": "Thing", "design": "DESIGN§1, O1"})
        self.assertEqual(page["items"][2]["items"][0]["fields"]["image"], "home.webp")

    def test_the_comment_is_skipped_and_lines_stay_true(self):
        page, _ = parsed()
        self.assertEqual(page["line"], 5)
        text = SAMPLE.split("\n")
        self.assertEqual(text[page["field_lines"]["lede"] - 1], "- lede: A lede with the player's 'quoted' words.")

    def test_fields_only_straight_after_their_heading(self):
        page, problems = parsed(SAMPLE.replace("Its text.", "Its text.\n\n- tag: not a field here"))
        self.assertEqual(problems, [])
        card = page["items"][1]["items"][0]
        self.assertEqual(card["fields"]["tag"], "Thing")
        self.assertIn("- tag: not a field here", [t for _, t in card["body"]])

    def test_a_list_of_bold_items_is_no_field(self):
        page, problems = parsed(SAMPLE.replace("- nav: About\n", "- **Bold:** an item\n"))
        self.assertEqual(problems, [])
        self.assertEqual(page["items"][0]["fields"], {})

    def test_unknown_and_misplaced_fields(self):
        text = SAMPLE.replace("- facts: One · Two", "- colour: gold").replace("- tag: Thing", "- tag: Thing\n- image: x.webp")
        _, problems = parsed(text)
        msgs = " | ".join(m for _, m in problems)
        self.assertIn("unknown field 'colour' for the page", msgs)
        self.assertIn("unknown field 'image' for a '### ' under a cards section", msgs)

    def test_required_fields(self):
        text = SAMPLE.replace("- status: Design stage\n", "").replace("- tag: Thing\n", "").replace("- image: home.webp\n", "")
        _, problems = parsed(text)
        msgs = " | ".join(m for _, m in problems)
        self.assertIn("the page needs '- status:'", msgs)
        self.assertIn("a card needs '- tag:'", msgs)
        self.assertIn("a screenshot needs '- image:'", msgs)

    def test_bad_values(self):
        text = (SAMPLE.replace("2026-11-07", "7 November").replace("roadmap-published: none", "roadmap-published: http://x")
                .replace("kind: cards", "kind: tiles").replace("image: home.webp", "image: img/Home.WEBP")
                .replace("from: home-1280x720.png", "from: ../shots/a.png"))
        _, problems = parsed(text)
        msgs = " | ".join(m for _, m in problems)
        for bit in ("'updated' must be a date", "'roadmap-published' is 'none'", "unknown kind 'tiles'",
                    "is no image name", "is no mock shot's file name"):
            self.assertIn(bit, msgs)

    def test_duplicate_anchor_and_second_status(self):
        text = SAMPLE + "\n## Again {#about}\n\n- kind: status\n"
        _, problems = parsed(text)
        msgs = " | ".join(m for _, m in problems)
        self.assertIn("the anchor 'about' is used twice", msgs)
        self.assertIn("one status section at most", msgs)

    def test_headings_and_comments_out_of_place(self):
        _, problems = build.parse("# T\n\n### Early\n\n#### Deep\n\n## S\n\n<!-- later -->\n")
        msgs = " | ".join(m for _, m in problems)
        self.assertIn("before any '## ' section", msgs)
        self.assertIn("only '#', '##' and '###'", msgs)
        self.assertIn("a comment is allowed only at the top", msgs)

    def test_text_under_the_title_belongs_in_a_field(self):
        _, problems = build.parse("# T\n\n- updated: 2026-11-07\n\nStray words.\n")
        self.assertIn("belongs in a field", problems[0][1])

    def test_an_unclosed_comment(self):
        _, problems = build.parse("<!-- never closed\n# T\n")
        self.assertEqual(problems, [(1, "the comment at the top is never closed")])


class MarkdownTests(unittest.TestCase):
    def test_inline(self):
        problems = []
        html = build.inline("a <b> & **bold** and *it* `x<y's` [l](https://e.org/?a=1&b=2) it's", problems, 1)
        self.assertEqual(problems, [])
        self.assertIn("a &lt;b&gt; &amp; <strong>bold</strong> and <em>it</em>", html)
        self.assertIn("<code>x&lt;y&#x27;s</code>", html)                  # no typography inside code
        self.assertIn('<a href="https://e.org/?a=1&amp;b=2">l</a>', html)  # nor inside an address
        self.assertIn("it\u2019s", html)

    def test_quotes(self):
        self.assertEqual(build.typo("the player's 'own' \"words\""), "the player\u2019s \u2018own\u2019 \u201cwords\u201d")
        self.assertEqual(build.typo("players' turn"), "players\u2019 turn")

    def test_links(self):
        problems = []
        self.assertIn('href="#features"', build.inline("[f](#features)", problems, 1))
        self.assertIn('href="../roadmap/index.html"', build.inline("[r](../roadmap/index.html)", problems, 1))
        self.assertEqual(build.inline("[r](../roadmap/index.html)", problems, 1, relative_links=False), "r")
        self.assertEqual(problems, [])
        self.assertEqual(build.inline("[x](javascript:alert(1))", problems, 7), "x)")
        self.assertEqual(build.inline("[y](/etc/passwd)", problems, 8), "y")
        self.assertEqual([n for n, _ in problems], [7, 8])

    def test_paragraphs_and_lists(self):
        body = list(enumerate(["one", "two", "", "- a", "  wrapped", "- b", "", "1. first", "2. second"], 1))
        html = build.prose(body, [])
        self.assertEqual(html, "<p>one two</p>\n<ul><li>a wrapped</li><li>b</li></ul>\n<ol><li>first</li><li>second</li></ol>")

    def test_long_date(self):
        self.assertEqual(build.long_date("2026-11-07"), "7 November 2026")


class PrivateTests(unittest.TestCase):
    def test_paths_addresses_and_mail_are_caught(self):
        for line in ("see /home/someone/x", "C:\\Users\\x", "at 203.0.113.5", "localhost:8080", "write to someone@example.com"):
            self.assertTrue(build.check_private(line), line)

    def test_versions_and_repository_paths_pass(self):
        for line in ("version 1.5 and v0.1 Eyes", "docs/showcase/img/", "tools/showcase/build.py", "O45, DESIGN§10.6"):
            self.assertEqual(build.check_private(line), [], line)


class ImageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_sizes_and_metadata(self):
        self.assertEqual(build.image_info(png(3, 2)), (3, 2, []))
        self.assertEqual(build.image_info(png(3, 2, text=True)), (3, 2, ["tEXt"]))
        self.assertEqual(build.image_info(webp_vp8l(1280, 720)), (1280, 720, []))
        self.assertEqual(build.image_info(webp_vp8l(1280, 720, exif=True)), (1280, 720, ["EXIF"]))
        self.assertEqual(build.image_info(webp_vp8x(1280, 720)), (1280, 720, []))
        self.assertEqual(build.image_info(webp_vp8(640, 360)), (640, 360, []))
        self.assertIsNone(build.image_info(b"GIF89a"))

    def check(self, **files):
        for name, data in files.items():
            (self.dir / name.replace("_", "-").replace("-webp", ".webp").replace("-png", ".png")).write_bytes(data)
        page, _ = parsed()
        return build.check_images(page, self.dir)

    def test_a_good_screenshot(self):
        problems, total = self.check(home_webp=webp_vp8l(1280, 720))
        self.assertEqual(problems, [])
        self.assertEqual(total, len(webp_vp8l(1280, 720)))

    def test_missing_wrong_size_metadata_and_stray(self):
        problems, _ = self.check()
        self.assertIn("no screenshot home.webp", problems[0][1])
        problems, _ = self.check(home_webp=webp_vp8l(1280, 800, exif=True), old_png=png(2, 2))
        msgs = " | ".join(m for _, m in problems)
        self.assertIn("is 1280x800, not 1280x720", msgs)
        self.assertIn("carries metadata (EXIF)", msgs)
        self.assertIn("img/old.png is used by no screenshot", msgs)

    def test_the_size_limit(self):
        saved = build.IMAGES_MAX
        build.IMAGES_MAX = 10
        try:
            problems, _ = self.check(home_webp=webp_vp8l(1280, 720))
        finally:
            build.IMAGES_MAX = saved
        self.assertIn("bytes, over 10", problems[0][1])

    def test_import_from_mock_shots(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow is not installed")
        shots = self.dir / "shots"
        shots.mkdir()
        (shots / "home-1280x720.png").write_bytes(png(1280, 720, text=True))
        page, _ = parsed()
        out = self.dir / "img"
        self.assertEqual(build.import_images(page, shots, out), [])
        data = (out / "home.webp").read_bytes()
        self.assertEqual(build.image_info(data), (1280, 720, []))
        with Image.open(out / "home.webp") as a, Image.open(shots / "home-1280x720.png") as b:
            self.assertEqual(a.convert("RGB").tobytes(), b.convert("RGB").tobytes())     # lossless
        (shots / "home-1280x720.png").write_bytes(png(640, 360))
        self.assertIn("is 640x360, not 1280x720", build.import_images(page, shots, out)[0][1])


class RefTests(unittest.TestCase):
    def test_design_references_resolve_as_the_roadmap_resolves_them(self):
        page, _ = parsed(SAMPLE.replace("design: DESIGN§1, O1", "design: DESIGN§1, DESIGN§99, O1, O9999"))
        problems = build.check_refs(page, FakeIndex())
        self.assertEqual(sorted(m for _, m in problems),
                         ["the design reference 'DESIGN§99' resolves to nothing",
                          "the design reference 'O9999' resolves to nothing"])

    def test_shown_references(self):
        self.assertEqual(build.show_ref("DESIGN§10.6"), "DESIGN §10.6")
        self.assertEqual(build.show_ref("GraceUI:DESIGN§3"), "GraceUI DESIGN §3")
        self.assertEqual(build.show_ref("O45"), "O45")


class PageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = pathlib.Path(self.tmp.name)
        self.source, self.page, self.img, self.roadmap = root / "showcase.md", root / "index.html", root / "img", root / "ROADMAP.md"
        self.source.write_text(SAMPLE.replace("- design: DESIGN§1, O1\n", ""), encoding="utf-8")
        self.img.mkdir()
        (self.img / "home.webp").write_bytes(webp_vp8l(1280, 720))
        self.roadmap.write_text(ROADMAP, encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def run_main(self, *extra):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = build.main(["--source", str(self.source), "--out", str(self.page), "--images", str(self.img),
                               "--roadmap", str(self.roadmap), *extra])
        return code, out.getvalue()

    def test_the_page(self):
        code, out = self.run_main()
        self.assertEqual(code, 0, out)
        html = self.page.read_text(encoding="utf-8")
        self.assertTrue(html.startswith("<!doctype html>"))
        self.assertIn('<meta http-equiv="Content-Security-Policy"', html)
        self.assertNotIn("<script", html.lower())
        self.assertIsNone(re.search(r"""(?:src|href)=["']?(?:https?:)?//""", html.replace('href="https://example.org', "")))
        self.assertIn("<h1>Testnet</h1>", html)
        self.assertIn("Showcase · updated 7 November 2026", html)
        self.assertIn('<li><a href="#about">About</a></li>', html)
        self.assertIn('<a href="img/home.webp" title="Open at full size"><img src="img/home.webp" width="1280" height="720"', html)
        self.assertIn('<span class="tag">Mock, placeholder data</span>', html)
        self.assertIn('href="../roadmap/index.html"', html)
        self.assertEqual(html.count('<li class="st-'), 2)
        self.assertIn('<li class="st-done">', html)
        self.assertIn("2 of 2 parts done", html)
        self.assertIn("The roadmap\u2019s stages · updated 6 November 2026", html)
        self.assertIn('<li class="st-next">', html)                                  # waiting beside next reads next
        self.assertNotIn("Not the title", html)                                      # the comment stays out

    def test_stage_words(self):
        html = build.stage_list({"updated": "2026-11-08", "stages": [
            {"id": "S6", "title": "Owner's turn", "status": "waiting", "done": 0, "parts": 0},
            {"id": "S9", "title": "Odd", "status": "unknown", "done": 1, "parts": 3}]})
        self.assertIn("The roadmap\u2019s stages · updated 8 November 2026", html)
        self.assertIn('<span class="stitle">Owner’s turn</span><span class="sstate">Waiting on the owner</span></li>', html)
        self.assertIn('<li class="st-planned">', html)
        self.assertIn("1 of 3 parts done", html)

    def test_fresh_then_stale(self):
        self.run_main()
        self.assertEqual(self.run_main("--check")[0], 0)
        self.source.write_text(self.source.read_text(encoding="utf-8").replace("Its caption.", "A new caption."), encoding="utf-8")
        code, out = self.run_main("--check")
        self.assertEqual(code, 1)
        self.assertIn("the page is not what the source", out)

    def test_a_roadmap_change_makes_the_page_stale(self):
        self.run_main()
        self.roadmap.write_text(ROADMAP.replace("- b Next [next]", "- b Next [done]"), encoding="utf-8")
        self.assertEqual(self.run_main("--check")[0], 1)

    def test_check_writes_nothing_and_refuses_images_from(self):
        code, _ = self.run_main("--check")
        self.assertEqual(code, 1)
        self.assertFalse(self.page.exists())
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.run_main("--check", "--images-from", str(self.img))

    def test_problems_stop_the_build(self):
        self.source.write_text(SAMPLE.replace("- tag: Thing\n", "").replace("2026-11-07", "soon"), encoding="utf-8")
        code, out = self.run_main()
        self.assertEqual(code, 1)
        self.assertFalse(self.page.exists())
        self.assertIn("problem: showcase.md:", out)

    def test_no_local_path_is_printed(self):
        _, out = self.run_main()
        self.assertNotIn(self.tmp.name, out)

    def test_the_fragment(self):
        frag = pathlib.Path(self.tmp.name) / "fragment.html"
        self.assertEqual(self.run_main("--fragment", str(frag))[0], 0)
        html = frag.read_text(encoding="utf-8")
        self.assertTrue(html.lower().startswith("<!doctype html>"))
        self.assertIn("<title>EldenNet Showcase</title>", html)
        for kept in ("<html", "<head>", "<body>", "</html>", 'name="viewport"'):
            self.assertIn(kept, html)
        self.assertNotIn("Content-Security-Policy", html)
        self.assertIn('src="data:image/webp;base64,', html)
        self.assertNotIn('src="img/', html)
        self.assertNotIn('href="img/', html)
        self.assertNotIn("../roadmap/", html)
        self.assertIn("is on EldenNet\u2019s roadmap page.", html)

    def test_the_fragment_links_the_published_roadmap(self):
        self.source.write_text(self.source.read_text(encoding="utf-8").replace(
            "roadmap-published: none", "roadmap-published: https://claude.ai/public/artifacts/abc"), encoding="utf-8")
        frag = pathlib.Path(self.tmp.name) / "fragment.html"
        self.run_main("--fragment", str(frag))
        self.assertIn('<a href="https://claude.ai/public/artifacts/abc">EldenNet\u2019s roadmap</a>', frag.read_text(encoding="utf-8"))
        self.assertIn('href="../roadmap/index.html"', self.page.read_text(encoding="utf-8"))


class LookTests(unittest.TestCase):
    """GraceUI's U7 rule on this page: every readable text 4.5:1 or better on the grounds it sits on."""

    @staticmethod
    def ratio(a, b):
        def lum(hex_):
            def ch(v):
                v /= 255
                return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
            r, g, b_ = (int(hex_[i:i + 2], 16) for i in (1, 3, 5))
            return 0.2126 * ch(r) + 0.7152 * ch(g) + 0.0722 * ch(b_)
        hi, lo = sorted((lum(a), lum(b)), reverse=True)
        return (hi + 0.05) / (lo + 0.05)

    def test_contrast(self):
        css = build.TEMPLATE.read_text(encoding="utf-8")
        var = dict(re.findall(r"--([a-z0-9-]+):\s*(#[0-9a-f]{6})", css))
        inks = ["ink-hi", "ink", "ink-mute", "ink-faint", "gold", "gold-hi", "glint"] + [f"st-{s}" for s in build.STATUS_WORDS]
        for ground in ("ground-0", "ground-1", "ground-2", "ground-3"):
            for ink in inks:
                self.assertGreaterEqual(self.ratio(var[ink], var[ground]), 4.5, f"{ink} on {ground}")

    def test_nothing_from_outside_and_no_motion_loops(self):
        css = build.TEMPLATE.read_text(encoding="utf-8")
        self.assertNotIn("@import", css)
        self.assertNotIn("@font-face", css)
        self.assertNotIn("url(", css)
        self.assertNotIn("<script", css.lower())
        self.assertNotIn("animation", css)
        self.assertEqual(css.count(build.MARKER), 1)


class RepositoryTests(unittest.TestCase):
    def test_the_showcase_checks_clean(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = build.main(["--check"])
        self.assertEqual(code, 0, out.getvalue())


if __name__ == "__main__":
    unittest.main()
