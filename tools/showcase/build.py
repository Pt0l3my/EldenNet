#!/usr/bin/env python3
"""EldenNet's showcase: reads docs/showcase/showcase.md, the one source, and writes the page
docs/showcase/index.html from tools/showcase/page.html, with its screenshots in docs/showcase/img/.

  python3 -I tools/showcase/build.py                    write the page
  python3 -I tools/showcase/build.py --check            check and write nothing; fails on an unknown or
                                                        misplaced field, a missing required field, a bad
                                                        date or link, a design reference that resolves to
                                                        nothing, a screenshot that is missing, not 1280x720,
                                                        carries metadata or is not used, images over 4 MB
                                                        in all, a path, address or e-mail in the source, or
                                                        a page that is no longer what the source builds
  python3 -I tools/showcase/build.py --images-from DIR  first write each screenshot from the GraceUI mock
                                                        shot its "from:" names in DIR (lossless WebP; needs
                                                        Pillow), then build
  python3 -I tools/showcase/build.py --fragment FILE    also write the page for publishing on claude.ai:
                                                        no content-security line, screenshots inlined

The source's syntax is explained in the comment at its top. A status section lists the roadmap's
stages, read from docs/ROADMAP.md through the roadmap tool (tools/roadmap/build.py), which also
resolves the cards' design references: the codes of EldenNet's wp index, the owner's decisions
(O-ids), GraceUI's codes and repository paths. Standard library only, but for --images-from.
Our own code, MIT (see LICENSE).
"""
import argparse
import base64
import datetime
import html as htmlmod
import importlib.util
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "docs/showcase/showcase.md"
PAGE = ROOT / "docs/showcase/index.html"
IMAGES = ROOT / "docs/showcase/img"
TEMPLATE = pathlib.Path(__file__).resolve().parent / "page.html"
ROADMAP_TOOL = ROOT / "tools/roadmap/build.py"
ROADMAP_SOURCE = ROOT / "docs/ROADMAP.md"
ROADMAP_LINK = "../roadmap/index.html"          # from docs/showcase/ to the roadmap's page
MARKER = "<!--SHOWCASE-BODY-->"

SHOT_SIZE = (1280, 720)
IMAGES_MAX = 4_000_000                          # bytes, every screenshot together
IMAGE_NAME = re.compile(r"^[a-z0-9][a-z0-9-]*\.(webp|png)$")
FROM_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*\.png$")

KINDS = ("prose", "cards", "gallery", "status")
PAGE_FIELDS = {"updated": True, "lede": True, "status": True, "facts": False, "roadmap-published": False}
SECTION_FIELDS = ("kind", "nav")
ITEM_FIELDS = {"cards": ("tag", "design"), "gallery": ("image", "from", "alt"), "prose": (), "status": ()}
STATUS_WORDS = {"done": "Done", "active": "Under way", "next": "Next", "planned": "Planned",
                "waiting": "Waiting on the owner", "idea": "Idea"}

HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
ANCHOR = re.compile(r"\s*\{#([a-z][a-z0-9-]*)\}\s*$")
FIELD = re.compile(r"^[-*]\s+([a-z][a-z-]*)\s*:\s*(.*?)\s*$")
LIST_ITEM = re.compile(r"^([-*]|\d+\.)\s+(.*)$")
PUBLISHED = re.compile(r"^https://claude\.ai/\S+$")
# what a published page must never carry (privsweep is the real gate; this catches the obvious early)
PRIVATE = [
    (re.compile(r"(?<![\w.])/(?:home|Users|tmp|root|mnt|var)/|\b[A-Za-z]:\\"), "a local path"),
    (re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b|\blocalhost\b"), "an address"),
    (re.compile(r"\b[\w.+-]+@[\w-]+\.[\w.-]+\b"), "an e-mail address"),
]


# --- the source ----------------------------------------------------------------------------------

def node(kind, title, line):
    return {"kind": kind, "title": title, "line": line, "fields": {}, "field_lines": {}, "body": [],
            "items": [], "id": None}


def parse(text):
    """(page, problems): the page's title and fields, its sections, and their cards or screenshots.
    Line numbers are the source's own."""
    problems = []
    lines = text.split("\n")
    start = 0
    if text.lstrip().startswith("<!--"):
        end = text.find("-->")
        if end < 0:
            return node("page", "", 1), [(1, "the comment at the top is never closed")]
        start = text[:end].count("\n") + 1
    page = node("page", None, 1)
    cur, section, fields_open = page, None, True
    for n, line in enumerate(lines[start:], start + 1):
        if "<!--" in line:
            problems.append((n, "a comment is allowed only at the top of the source"))
            continue
        h = HEADING.match(line)
        if h:
            level, title = len(h.group(1)), h.group(2)
            if level == 1:
                if page["title"] is not None:
                    problems.append((n, "a second '# ' title: the page has one"))
                page["title"], page["line"], cur = title, n, page
            elif level == 2:
                a = ANCHOR.search(title)
                section = node("section", ANCHOR.sub("", title) if a else title, n)
                section["id"] = a.group(1) if a else slug(title)
                page["items"].append(section)
                cur = section
            elif level == 3:
                if section is None:
                    problems.append((n, "a '### ' heading before any '## ' section"))
                    continue
                cur = node("item", title, n)
                section["items"].append(cur)
            else:
                problems.append((n, "only '#', '##' and '###' headings are used"))
                continue
            if not cur["title"]:
                problems.append((n, "an empty heading"))
            fields_open = True
            continue
        if fields_open:
            if not line.strip():
                continue
            f = FIELD.match(line)
            if f:
                key, value = f.group(1), f.group(2)
                if key in cur["fields"]:
                    problems.append((n, f"'{key}' is given twice here"))
                cur["fields"][key], cur["field_lines"][key] = value, n
                continue
            fields_open = False
        if cur is page and line.strip() and section is None:
            problems.append((n, "text under the title belongs in a field (lede, status, facts) or a section"))
            continue
        cur["body"].append((n, line))
    if page["title"] is None:
        problems.append((1, "no '# ' title"))
    return page, problems


def slug(title):
    return re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-") or "section"


def check_fields(page):
    """Each field in its place, each required one present, each value well formed."""
    out = []
    for key, n in page["field_lines"].items():
        if key not in PAGE_FIELDS:
            out.append((n, f"unknown field '{key}' for the page (known: {', '.join(PAGE_FIELDS)})"))
    for key, required in PAGE_FIELDS.items():
        if required and not page["fields"].get(key):
            out.append((page["line"], f"the page needs '- {key}:'"))
    upd = page["fields"].get("updated")
    if upd:
        try:
            datetime.date.fromisoformat(upd)
            assert re.fullmatch(r"\d{4}-\d{2}-\d{2}", upd)
        except (ValueError, AssertionError):
            out.append((page["field_lines"]["updated"], f"'updated' must be a date, YYYY-MM-DD, not '{upd}'"))
    pub = page["fields"].get("roadmap-published", "none")
    if pub != "none" and not PUBLISHED.match(pub):
        out.append((page["field_lines"]["roadmap-published"],
                    "'roadmap-published' is 'none' or the roadmap page's https://claude.ai/ address"))
    ids = {}
    for sec in page["items"]:
        for key, n in sec["field_lines"].items():
            if key not in SECTION_FIELDS:
                out.append((n, f"unknown field '{key}' for a section (known: {', '.join(SECTION_FIELDS)})"))
        kind = sec["fields"].get("kind", "prose")
        if kind not in KINDS:
            out.append((sec["field_lines"]["kind"], f"unknown kind '{kind}' (known: {', '.join(KINDS)})"))
            kind = "prose"
        sec["kind"] = kind
        if sec["id"] in ids:
            out.append((sec["line"], f"the anchor '{sec['id']}' is used twice (first on line {ids[sec['id']]})"))
        ids.setdefault(sec["id"], sec["line"])
        allowed = ITEM_FIELDS[kind]
        for it in sec["items"]:
            for key, n in it["field_lines"].items():
                if key not in allowed:
                    where = f"a '### ' under a {kind} section"
                    out.append((n, f"unknown field '{key}' for {where}" +
                                (f" (known: {', '.join(allowed)})" if allowed else " (it takes none)")))
            if kind == "gallery":
                image = it["fields"].get("image")
                if not image:
                    out.append((it["line"], "a screenshot needs '- image:'"))
                elif not IMAGE_NAME.match(image):
                    out.append((it["field_lines"]["image"], f"'{image}' is no image name: lower case, "
                                "digits and hyphens, then .webp or .png, no folder"))
                src = it["fields"].get("from")
                if src is not None and not FROM_NAME.match(src):
                    out.append((it["field_lines"]["from"], f"'{src}' is no mock shot's file name (a .png, "
                                "no folder)"))
            if kind == "cards" and not it["fields"].get("tag"):
                out.append((it["line"], "a card needs '- tag:'"))
    if sum(s["kind"] == "status" for s in page["items"]) > 1:
        out.append((page["line"], "one status section at most: the roadmap's stages are listed once"))
    return out


def check_private(text, start_line=1):
    out = []
    for n, line in enumerate(text.split("\n"), start_line):
        for pat, what in PRIVATE:
            if pat.search(line):
                out.append((n, f"{what}: a published page carries none"))
    return out


# --- the screenshots -----------------------------------------------------------------------------

def image_info(data):
    """(width, height, metadata chunks) of a PNG or WebP, or None if it is neither."""
    if data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR":
        w, h = struct.unpack(">II", data[16:24])
        meta, i = [], 8
        while i + 8 <= len(data):
            length, kind = struct.unpack(">I4s", data[i:i + 8])
            if kind in (b"tEXt", b"iTXt", b"zTXt", b"eXIf", b"tIME"):
                meta.append(kind.decode())
            i += 12 + length
        return w, h, meta
    if data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        size, meta, i = None, [], 12
        while i + 8 <= len(data):
            kind, length = data[i:i + 4], struct.unpack("<I", data[i + 4:i + 8])[0]
            body = data[i + 8:i + 8 + length]
            if kind == b"VP8L" and body[:1] == b"\x2f" and size is None:
                b = int.from_bytes(body[1:5], "little")
                size = ((b & 0x3FFF) + 1, ((b >> 14) & 0x3FFF) + 1)
            elif kind == b"VP8X" and size is None:
                size = (int.from_bytes(body[4:7], "little") + 1, int.from_bytes(body[7:10], "little") + 1)
            elif kind == b"VP8 " and body[3:6] == b"\x9d\x01\x2a" and size is None:
                w, h = struct.unpack("<HH", body[6:10])
                size = (w & 0x3FFF, h & 0x3FFF)
            elif kind in (b"EXIF", b"XMP "):
                meta.append(kind.decode().strip())
            i += 8 + length + (length & 1)
        return (size + (meta,)) if size else None
    return None


def screenshots(page):
    return [it for sec in page["items"] if sec.get("kind") == "gallery" for it in sec["items"]
            if IMAGE_NAME.match(it["fields"].get("image", ""))]


def check_images(page, images=IMAGES):
    out, total, used = [], 0, set()
    for it in screenshots(page):
        name, n = it["fields"]["image"], it["field_lines"]["image"]
        used.add(name)
        p = images / name
        try:
            data = p.read_bytes()
        except OSError:
            out.append((n, f"no screenshot {name} in docs/showcase/img/ (build with --images-from to make it)"))
            continue
        total += len(data)
        info = image_info(data)
        if info is None:
            out.append((n, f"{name} is neither a PNG nor a WebP"))
            continue
        if (info[0], info[1]) != SHOT_SIZE:
            out.append((n, f"{name} is {info[0]}x{info[1]}, not {SHOT_SIZE[0]}x{SHOT_SIZE[1]}"))
        if info[2]:
            out.append((n, f"{name} carries metadata ({', '.join(info[2])}): write it again without"))
    if total > IMAGES_MAX:
        out.append((page["line"], f"the screenshots come to {total:,} bytes, over {IMAGES_MAX:,}"))
    if images.is_dir():
        for p in sorted(images.iterdir()):
            if p.name not in used:
                out.append((0, f"docs/showcase/img/{p.name} is used by no screenshot: delete it"))
    return out, total


def import_images(page, src_dir, images=IMAGES):
    """Write each screenshot from the mock shot its 'from:' names in src_dir: 1280x720, no metadata,
    lossless WebP (or optimised PNG for a .png name)."""
    try:
        from PIL import Image
    except ImportError:
        return [(0, "--images-from needs Pillow (python3 -m pip install pillow)")]
    out = []
    images.mkdir(parents=True, exist_ok=True)
    for it in screenshots(page):
        src = it["fields"].get("from")
        if not src:
            continue
        p = pathlib.Path(src_dir) / src
        if not p.is_file():
            out.append((it["field_lines"]["from"], f"no {src} in the folder given to --images-from"))
            continue
        with Image.open(p) as im:
            if im.size != SHOT_SIZE:
                out.append((it["field_lines"]["from"], f"{src} is {im.size[0]}x{im.size[1]}, not "
                            f"{SHOT_SIZE[0]}x{SHOT_SIZE[1]}"))
                continue
            rgb = im.convert("RGB")
        dst = images / it["fields"]["image"]
        if dst.suffix == ".webp":
            rgb.save(dst, "WEBP", lossless=True, quality=100, method=6)
        else:
            rgb.save(dst, "PNG", optimize=True)
    return out


# --- the roadmap's stages and the design references -----------------------------------------------

_roadmap = None


def roadmap_tool():
    """The roadmap tool as a module, loaded once; None if it is missing."""
    global _roadmap
    if _roadmap is None and ROADMAP_TOOL.is_file():
        spec = importlib.util.spec_from_file_location("eldennet_roadmap_build", ROADMAP_TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        _roadmap = mod
    return _roadmap


def roadmap_stages(source=ROADMAP_SOURCE):
    """({updated, stages: [{id, title, status, done, parts}]}, problems) from the roadmap's source,
    statuses settled as its page settles them."""
    rm = roadmap_tool()
    if rm is None:
        return {"updated": None, "stages": []}, [(0, "the roadmap tool tools/roadmap/build.py is missing")]
    try:
        roadmap, problems = rm.load(source)
    except OSError:
        return {"updated": None, "stages": []}, [(0, "docs/ROADMAP.md is missing")]
    if problems:
        return {"updated": None, "stages": []}, [(0, f"docs/ROADMAP.md has {len(problems)} problem(s): run python3 -I tools/roadmap/build.py --check")]
    rm.settle_statuses(roadmap, rm.load_tasks())
    out = []
    for st in roadmap["stages"]:
        parts = [p for step in st["steps"] for p in step["parts"]]
        out.append({"id": st["id"], "title": st["title"], "status": st["status"],
                    "done": sum(p["status"] == "done" for p in parts), "parts": len(parts)})
    updated = roadmap["meta"].get("updated") or None
    try:
        datetime.date.fromisoformat(updated or "")
    except ValueError:
        updated = None
    return {"updated": updated, "stages": out}, []


def design_refs(page):
    return [(it, r) for sec in page["items"] for it in sec["items"]
            for r in split_refs(it["fields"].get("design", ""))]


def split_refs(value):
    return [v for v in re.split(r"[,;\s]+", value) if v]


def check_refs(page, index=None):
    """Every design reference resolves, as the roadmap's own references do."""
    refs = design_refs(page)
    if not refs:
        return []
    rm = roadmap_tool()
    if rm is None:
        return [(0, "the roadmap tool tools/roadmap/build.py is missing: the design references go unchecked")]
    index = index if index is not None else rm.workplan_index()
    _, missing = rm.describe_refs({"meta": {"refs": [r for _, r in refs]}, "stages": []}, {}, index)
    return [(it["field_lines"]["design"], f"the design reference '{r}' resolves to nothing")
            for it, r in refs if r in missing]


def show_ref(ref):
    ref = ref.replace("GraceUI:", "GraceUI ")
    return re.sub(r"§", " §", ref, count=1)


# --- Markdown, the small part of it the source uses ----------------------------------------------

def esc(text):
    return htmlmod.escape(text, quote=True)


def typo(text):
    """Typographic quotes for text a reader sees (never inside code): game's, ‘a’, “a”."""
    text = re.sub(r"(?<=[\w.,!?)])'|'(?=\s|$)", "’", text)
    text = re.sub(r"'", "‘", text)
    text = re.sub(r'(?<=[\w.,!?)])"', "”", text)
    return text.replace('"', "“")


def show(text):
    """Plain text for the page: typographic quotes, escaped."""
    return esc(typo(text))


LINK = re.compile(r"\[([^\]]+)\]\(([^)\s]+)\)")
BOLD = re.compile(r"\*\*(?=\S)(.+?)(?<=\S)\*\*")
ITALIC = re.compile(r"(?<![\w*])\*(?=\S)(.+?)(?<=\S)\*(?![\w*])")


def inline(text, problems, n, relative_links=True):
    """Code spans, links, bold and italic, with typographic quotes; everything else escaped."""
    out = []
    for i, part in enumerate(re.split(r"(`[^`]+`)", text)):
        if i % 2:
            out.append("<code>" + esc(part[1:-1]) + "</code>")
            continue
        pieces, pos = [], 0
        for m in LINK.finditer(part):
            pieces.append(show(part[pos:m.start()]))
            label, url = show(m.group(1)), m.group(2)
            if url.startswith("https://") or url.startswith("#"):
                pieces.append(f'<a href="{esc(url)}">{label}</a>')
            elif ":" in url or url.startswith("/") or url.startswith("\\"):
                problems.append((n, f"the link '{url}' is neither https://, #anchor nor relative"))
                pieces.append(label)
            else:
                pieces.append(f'<a href="{esc(url)}">{label}</a>' if relative_links else label)
            pos = m.end()
        pieces.append(show(part[pos:]))
        s = BOLD.sub(r"<strong>\1</strong>", "".join(pieces))
        out.append(ITALIC.sub(r"<em>\1</em>", s))
    return "".join(out)


def blocks(body):
    """[(kind, [(n, text)])]: paragraphs, lists of items, '### ' subheadings never reach here."""
    out, cur = [], None
    for n, line in body:
        if not line.strip():
            cur = None
            continue
        m = LIST_ITEM.match(line)
        if m:
            kind = "ol" if m.group(1)[0].isdigit() else "ul"
            if cur is None or cur[0] != kind:
                cur = (kind, [])
                out.append(cur)
            cur[1].append((n, m.group(2).strip()))
        elif cur is None:
            cur = ("p", [(n, line.strip())])
            out.append(cur)
        else:                                     # a wrapped line of a paragraph or a list item
            k, rows = cur
            rows[-1] = (rows[-1][0], rows[-1][1] + " " + line.strip())
    return out


def prose(body, problems, relative_links=True):
    html = []
    for kind, rows in blocks(body):
        if kind == "p":
            html.append("<p>" + inline(" ".join(t for _, t in rows), problems, rows[0][0], relative_links) + "</p>")
        else:
            items = "".join("<li>" + inline(t, problems, n, relative_links) + "</li>" for n, t in rows)
            html.append(f"<{kind}>{items}</{kind}>")
    return "\n".join(html)


# --- the page --------------------------------------------------------------------------------------

def long_date(iso):
    d = datetime.date.fromisoformat(iso)
    return f"{d.day} {d.strftime('%B')} {d.year}"


def render_body(page, stages, problems, images=IMAGES, inline_images=False):
    """The page's body. inline_images: the published copy, with the screenshots inside it as data
    and no link that only works beside the repository's files."""
    rel = not inline_images
    f = page["fields"]
    nav = "".join(f'<li><a href="#{esc(s["id"])}">{show(s["fields"].get("nav") or s["title"])}</a></li>'
                  for s in page["items"])
    out = ['<a class="skip" href="#content">Skip to the content</a>',
           '<header class="band"><div class="band-in">'
           f'<a class="wordmark" href="#top">{show(page["title"])}</a>'
           f'<nav aria-label="Sections"><ul>{nav}</ul></nav></div></header>',
           '<main id="top">', '<section class="hero" id="content">',
           f'<p class="eyebrow">Showcase · updated {long_date(f["updated"])}</p>',
           f'<h1>{show(page["title"])}</h1>',
           f'<p class="lede">{inline(f["lede"], problems, page["field_lines"]["lede"], rel)}</p>']
    facts = [x.strip() for x in f.get("facts", "").split("·") if x.strip()]
    if facts:
        out.append('<ul class="facts">' + "".join(f"<li>{show(x)}</li>" for x in facts) + "</ul>")
    status_sec = next((s for s in page["items"] if s.get("kind") == "status"), None)
    more = f' <a href="#{esc(status_sec["id"])}">Where it stands</a>' if status_sec else ""
    out.append(f'<p class="standing"><span class="pill">{show(f["status"])}</span>{more}</p>')
    out.append("</section>")
    for sec in page["items"]:
        out.append(f'<section class="sec sec-{sec["kind"]}" id="{esc(sec["id"])}" aria-labelledby="h-{esc(sec["id"])}">')
        out.append(f'<h2 id="h-{esc(sec["id"])}">{show(sec["title"])}</h2><div class="lozenge"></div>')
        intro = prose(sec["body"], problems, rel)
        if intro:
            out.append(f'<div class="prose">{intro}</div>')
        if sec["kind"] == "cards":
            out.append('<div class="cards">')
            for it in sec["items"]:
                refs = split_refs(it["fields"].get("design", ""))
                design = (f'<p class="design">In the design: {" · ".join(esc(show_ref(r)) for r in refs)}</p>'
                          if refs else "")
                out.append(f'<article class="card"><p class="eyebrow">{show(it["fields"].get("tag", ""))}</p>'
                           f'<h3>{show(it["title"])}</h3><div class="prose">{prose(it["body"], problems, rel)}</div>'
                           f'{design}</article>')
            out.append("</div>")
        elif sec["kind"] == "gallery":
            out.append('<div class="shots">')
            for k, it in enumerate(sec["items"]):
                out.append(figure(it, problems, images, inline_images, lazy=k > 0))
            out.append("</div>")
        else:
            for it in sec["items"]:
                out.append(f'<div class="prose"><h3>{show(it["title"])}</h3>{prose(it["body"], problems, rel)}</div>')
        if sec["kind"] == "status":
            out.append(stage_list(stages))
            pub = f.get("roadmap-published", "none")
            if inline_images and pub == "none":
                out.append('<p class="more">Every step and part, with what each needs, is on EldenNet’s '
                           'roadmap page.</p>')
            else:
                href = pub if inline_images else ROADMAP_LINK
                out.append(f'<p class="more">Every step and part, with what each needs, is on '
                           f'<a href="{esc(href)}">EldenNet’s roadmap</a>.</p>')
        out.append("</section>")
    out.append("</main>")
    out.append(f'<footer><p>Last updated {long_date(f["updated"])}. The screenshots are of GraceUI’s design mock, '
               'with placeholder data and invented names; the menu itself is built alongside EldenNet.</p></footer>')
    return "\n".join(out)


def figure(it, problems, images, inline_images, lazy):
    name = it["fields"].get("image", "")
    alt = it["fields"].get("alt") or f"Mock screenshot of the menu’s {it['title']} page"
    if inline_images:
        try:
            data = (images / name).read_bytes()
        except OSError:
            data = b""
        mime = "image/webp" if name.endswith(".webp") else "image/png"
        src = f"data:{mime};base64,{base64.b64encode(data).decode()}"
    else:
        src = "img/" + name
    loading = ' loading="lazy"' if lazy else ""
    img = (f'<img src="{esc(src)}" width="{SHOT_SIZE[0]}" height="{SHOT_SIZE[1]}" alt="{show(alt)}"'
           f'{loading} decoding="async">')
    if not inline_images:
        img = f'<a href="img/{esc(name)}" title="Open at full size">{img}</a>'
    stem = re.sub(r"\.\w+$", "", name)
    return (f'<figure class="shot" id="shot-{esc(stem)}"><figcaption><div class="cap-head">'
            f'<h3>{show(it["title"])}</h3><span class="tag">Mock, placeholder data</span></div>'
            f'<div class="prose">{prose(it["body"], problems, not inline_images)}</div></figcaption>{img}</figure>')


def stage_list(roadmap):
    rows = []
    for st in roadmap["stages"]:
        status = st["status"] if st["status"] in STATUS_WORDS else "planned"
        parts = (f'<span class="sparts">{st["done"]} of {st["parts"]} parts done</span>' if st["parts"] else "")
        rows.append(f'<li class="st-{status}"><span class="dot" aria-hidden="true"></span>'
                    f'<span class="sid">{esc(st["id"])}</span><span class="stitle">{show(st["title"])}</span>'
                    f'<span class="sstate">{STATUS_WORDS[status]}</span>{parts}</li>')
    when = f" · updated {long_date(roadmap['updated'])}" if roadmap.get("updated") else ""
    return (f'<p class="eyebrow from-roadmap">The roadmap’s stages{when}</p>'
            '<ol class="stages">' + "".join(rows) + "</ol>")


def render(body, template=TEMPLATE):
    text = template.read_text(encoding="utf-8")
    assert text.count(MARKER) == 1, "the template has no single body marker"
    return text.replace(MARKER, body, 1)


def fragment(html):
    """The page as claude.ai publishes it: a whole document (its publishing tools take one that starts
    with its doctype), without the content-security line, since the published page is served under
    claude.ai's own policy. The same cut as the roadmap's."""
    return re.sub(r'<meta http-equiv="Content-Security-Policy"[^>]*>\s*', "", html, count=1, flags=re.I)


def stale(html, page):
    try:
        return page.read_text(encoding="utf-8") != html
    except OSError:
        return True


# --- the command -----------------------------------------------------------------------------------

def build(source=SOURCE, images=IMAGES, roadmap=ROADMAP_SOURCE, index=None, images_from=None):
    """(page, html, problems, image bytes): everything the command needs, nothing written but the
    screenshots that --images-from asks for."""
    text = source.read_text(encoding="utf-8")
    page, problems = parse(text)
    problems += check_private(text)
    if not problems:
        problems += check_fields(page)
    if problems:
        return page, None, problems, 0
    if images_from is not None:
        problems += import_images(page, images_from, images)
    found, total = check_images(page, images)
    problems += found
    problems += check_refs(page, index)
    stages, rp = (roadmap_stages(roadmap) if any(s["kind"] == "status" for s in page["items"])
                  else ({"updated": None, "stages": []}, []))
    problems += rp
    html = render(render_body(page, stages, problems, images))
    return page, html, problems, total


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="check the source, the screenshots and the page; write nothing")
    ap.add_argument("--source", type=pathlib.Path, default=SOURCE)
    ap.add_argument("--out", type=pathlib.Path, default=PAGE)
    ap.add_argument("--images", type=pathlib.Path, default=IMAGES, help=argparse.SUPPRESS)
    ap.add_argument("--roadmap", type=pathlib.Path, default=ROADMAP_SOURCE, help=argparse.SUPPRESS)
    ap.add_argument("--images-from", type=pathlib.Path, metavar="DIR",
                    help="first write the screenshots from the GraceUI mock shots in DIR (needs Pillow)")
    ap.add_argument("--fragment", type=pathlib.Path, metavar="FILE",
                    help="also write the page for publishing on claude.ai: no content-security line, screenshots inlined")
    args = ap.parse_args(argv)
    if args.check and args.images_from:
        ap.error("--check writes nothing: build with --images-from first")
    shown = lambda p: p.resolve().relative_to(ROOT) if p.resolve().is_relative_to(ROOT) else p.name
    page, html, problems, total = build(args.source, args.images, args.roadmap, images_from=args.images_from)
    if html is not None and args.check and stale(html, args.out):
        problems.append((0, "the page is not what the source, the screenshots and the roadmap build now: "
                            "run python3 -I tools/showcase/build.py"))
    for n, msg in problems:
        print(f"problem: {shown(args.source)}:{n}: {msg}" if n else f"problem: {msg}")
    secs = page["items"]
    cards = sum(len(s["items"]) for s in secs if s.get("kind") == "cards")
    print(f"{len(secs)} sections, {cards} features, {len(screenshots(page))} screenshots ({total:,} bytes): "
          f"{len(problems)} problem(s)")
    if problems:
        return 1
    if args.check:
        return 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(html, encoding="utf-8")
    print(f"wrote {shown(args.out)} ({len(html.encode('utf-8')):,} bytes)")
    if args.fragment:
        frag_problems = []
        body = render_body(page, roadmap_stages(args.roadmap)[0], frag_problems, args.images, inline_images=True)
        frag = fragment(render(body))
        args.fragment.parent.mkdir(parents=True, exist_ok=True)
        args.fragment.write_text(frag, encoding="utf-8")
        print(f"wrote {shown(args.fragment)} ({len(frag.encode('utf-8')):,} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
