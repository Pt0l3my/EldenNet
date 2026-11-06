"""The index: every EldenNet document cut into sections, each with a stable code, its size and the codes it
cites, and the links both ways. Built in about a second; kept in data/index.json, which is generated and never
committed, and rebuilt by itself whenever an indexed file changes.

Adapted from the section index of the owner's Skyrizz workplan tool (tools/workplan/wplib/index.py in
Pt0l3my/Skyrizz), with EldenNet's document list and codes.
"""
import glob
import hashlib
import os
import pathlib
import re
import sys

from .common import CHUNK_TOKENS, DATA, ROOT, cited_codes, load, save, tokens

INDEX = DATA / "index.json"

# Which files are indexed, and the prefix each one's section codes start with.
FIXED = {
    "README.md": "README", "CLAUDE.md": "CLAUDE", "CONTRIBUTING.md": "CONTRIB",
    "docs/design/DESIGN.md": "DESIGN", "docs/design/architecture.md": "ARCH",
    "docs/design/npcs-and-dialogue.md": "NPC", "docs/design/memory-and-knowledge.md": "MEM",
    "docs/research/README.md": "RES", "tools/privsweep/README.md": "PSW", "tools/workplan/README.md": "WP",
}
SERIES = {"docs/research/skyrimnet": "SN", "docs/research/eldenring": "ER"}

HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
NUMBER = re.compile(r"^(?:§\s*)?(\d+(?:\.\d+)*)\.?(?:\s|$)")
APPENDIX = re.compile(r"^Appendix\s+([A-Z])(?:[.\s]+(\d+(?:\.\d+)*))?")


def slug(text):
    s = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return s[:40].rstrip("-") or "section"


def rel_(p):
    return str(pathlib.Path(p).resolve().relative_to(ROOT))


def doc_list():
    """(path, prefix) for every indexed file, each prefix unique."""
    out, taken = [], set()

    def add(path, prefix):
        if path not in {p for p, _ in out}:
            while prefix in taken:
                prefix += "+"
            taken.add(prefix)
            out.append((path, prefix))

    for path, prefix in FIXED.items():
        if (ROOT / path).exists():
            add(path, prefix)
    for folder, series in SERIES.items():
        for p in sorted(glob.glob(str(ROOT / folder / "*.md"))):
            m = re.match(r"(\d{2})-", pathlib.Path(p).name)
            if m:
                add(rel_(p), series + m.group(1))
    for p in sorted(glob.glob(str(ROOT / "docs/design/process/*.md"))):
        add(rel_(p), pathlib.Path(p).stem.split("-")[0].upper())
    for p in sorted(glob.glob(str(ROOT / "docs/**/*.md"), recursive=True)):
        add(rel_(p), "DOC:" + rel_(p)[len("docs/"):-3])
    return out


def split_markdown(lines, prefix, path):
    """Sections at every heading outside code fences. Returns dicts with id, title, a, b (0-based, end
    exclusive), level and parent."""
    marks = []
    fence = False
    for i, line in enumerate(lines):
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        m = HEADING.match(line)
        if not m:
            continue
        level, title = len(m.group(1)), m.group(2).strip()
        n = NUMBER.match(title)
        a = APPENDIX.match(title)
        if n:
            base = prefix + "§" + n.group(1)
        elif a:
            base = prefix + "§" + a.group(1) + ("." + a.group(2) if a.group(2) else "")
        else:
            base = prefix + "§" + slug(title)
        marks.append((i, base, title, level))
    if not marks or marks[0][0] > 0:
        marks.insert(0, (0, prefix + "§0", "(start of " + path.rsplit("/", 1)[-1] + ")", 0))
    out, used = [], {}
    for k, (i, base, title, level) in enumerate(marks):
        end = marks[k + 1][0] if k + 1 < len(marks) else len(lines)
        n = used.get(base, 0) + 1
        used[base] = n
        sid = base if n == 1 else base + "~" + str(n)
        out.append({"id": sid, "title": title, "a": i, "b": end, "level": level})
    stack = []
    for s in out:
        while stack and stack[-1]["level"] >= s["level"]:
            stack.pop()
        s["parent"] = stack[-1]["id"] if stack else None
        stack.append(s)
    return out


def chunk(section, lines, limit):
    """Cut a section larger than `limit` tokens at blank lines into pieces near the limit."""
    text_len = sum(len(l.encode("utf-8")) + 1 for l in lines[section["a"]:section["b"]])
    if tokens(text_len) <= limit:
        return [section]
    pieces, start, size = [], section["a"], 0
    for i in range(section["a"], section["b"]):
        size += len(lines[i].encode("utf-8")) + 1
        boundary = lines[i].strip() == "" or i == section["b"] - 1 or tokens(size) >= 2 * limit
        if tokens(size) >= limit and boundary:
            pieces.append((start, i + 1))
            start, size = i + 1, 0
    if start < section["b"]:
        pieces.append((start, section["b"]))
    out = []
    for k, (a, b) in enumerate(pieces, 1):
        s = dict(section, a=a, b=b, id=section["id"] + "~" + str(k) if len(pieces) > 1 else section["id"])
        s["aliases"] = [section["id"]] if len(pieces) > 1 else []
        s["part"] = [k, len(pieces)]
        s["parent"] = section["id"] if len(pieces) > 1 and k > 1 else section.get("parent")
        out.append(s)
    return out


def build(quiet=False):
    sections, docs = [], {}
    for path, prefix in doc_list():
        raw = (ROOT / path).read_text(encoding="utf-8", errors="replace")
        lines = raw.split("\n")
        count = 0
        for p in split_markdown(lines, prefix, path):
            for s in chunk(p, lines, CHUNK_TOKENS):
                text = "\n".join(lines[s["a"]:s["b"]])
                rec = {
                    "id": s["id"], "aliases": s.get("aliases", []), "doc": path, "title": s["title"],
                    "lines": [s["a"] + 1, s["b"]], "tokens": tokens(text), "cites": cited_codes(text, path),
                    "parent": s.get("parent"),
                }
                if s.get("part"):
                    rec["part"] = s["part"]
                sections.append(rec)
                count += 1
        docs[path] = {"prefix": prefix, "tokens": tokens(raw), "sections": count}
    codes, refs = {}, {}
    prefixes = {d["prefix"]: path for path, d in docs.items()}
    for s in sections:
        for c in dict.fromkeys([s["id"]] + s["aliases"]):
            codes.setdefault(c, []).append(s["id"])
        prefix = docs[s["doc"]]["prefix"]
        codes.setdefault(prefix, []).append(s["id"])            # a document's prefix names all its sections
        for c in s["cites"]:
            if c not in (s["id"], prefix) and s["id"] not in refs.get(c, []):
                refs.setdefault(c, []).append(s["id"])
    index = {"fingerprint": fingerprint(), "sections": sections, "docs": docs, "codes": codes, "refs": refs,
             "prefixes": prefixes}
    save(INDEX, index, indent=None)
    if not quiet:
        total = sum(d["tokens"] for d in docs.values())
        print(f"indexed {len(docs)} files, {len(sections)} sections, about {total:,} tokens; "
              f"{len(refs):,} cited codes -> {INDEX.relative_to(ROOT)}")
    return index


def fingerprint():
    """Sizes and change times of every indexed file and of this code: any edit, new file or branch switch
    changes it, and the index rebuilds itself."""
    h = hashlib.sha1()
    for path, _ in doc_list():
        st = (ROOT / path).stat()
        h.update(f"{path}:{st.st_size}:{int(st.st_mtime)};".encode())
    for f in sorted(pathlib.Path(__file__).parent.glob("*.py")):
        st = f.stat()
        h.update(f"{f.name}:{st.st_size}:{int(st.st_mtime)};".encode())
    return h.hexdigest()


def get(rebuild=False):
    idx = None if rebuild else load(INDEX)
    if idx and os.environ.get("WP_FROZEN") != "1" and idx.get("fingerprint") != fingerprint():
        idx = None
    return idx if idx else build(quiet=True)


class Index:
    def __init__(self, data=None):
        self.data = data or get()
        self.by_id = {s["id"]: s for s in self.data["sections"]}
        self.children = {}
        for s in self.data["sections"]:
            if s.get("parent"):
                self.children.setdefault(s["parent"], []).append(s["id"])

    def resolve(self, code, deep=False):
        """Section ids for a code: a section id (SN04§5), a split section's own id, or a document prefix
        (SN04, DESIGN: every section of it, in order); with deep=True the sections under it too."""
        ids = list(dict.fromkeys(self.data["codes"].get(code, [])))
        if not ids and code in self.by_id:
            ids = [code]
        if deep:
            out, todo = [], list(ids) + [c for c in self.children.get(code, []) if c not in ids]
            while todo:
                i = todo.pop(0)
                if i not in out:
                    out.append(i)
                    todo[0:0] = self.children.get(i, [])
            ids = out
        return ids

    def text(self, sid):
        s = self.by_id[sid]
        lines = (ROOT / s["doc"]).read_text(encoding="utf-8", errors="replace").split("\n")
        return "\n".join(lines[s["lines"][0] - 1:s["lines"][1]])


def describe(idx, ids):
    total = 0
    for sid in ids:
        s = idx.by_id[sid]
        total += s["tokens"]
        print(f"  {sid:28s} {s['tokens']:>7,}  {s['doc']}:{s['lines'][0]}  {s['title'][:60]}")
    print(f"  {len(ids)} sections, about {total:,} tokens")


def cmd_show(args):
    idx = Index()
    budget = args.max_tokens
    for code in args.codes:
        ids = idx.resolve(code, deep=args.all)
        if not ids:
            print(f"== {code}: not found (try: wp find {code})", file=sys.stderr)
            continue
        if args.list:
            print(f"== {code}")
            describe(idx, ids)
            continue
        for sid in ids:
            s = idx.by_id[sid]
            print(f"== {sid}  {s['doc']}:{s['lines'][0]}-{s['lines'][1]}  ~{s['tokens']:,} tokens")
            if budget is not None and budget - s["tokens"] < 0:
                print(f"   (skipped: over --max-tokens; {budget:,} left)")
                continue
            print(idx.text(sid))
            if budget is not None:
                budget -= s["tokens"]


def cmd_refs(args):
    idx = Index()
    for code in args.codes:
        print(f"== sections citing {code}")
        describe(idx, idx.data["refs"].get(code, []))


def search(idx, terms):
    """Section ids matching every term: in:path-or-prefix, text:regex, or a code or word in the title."""
    ids, texts = [], {}
    patterns = {t: re.compile(t[5:], re.IGNORECASE) for t in terms if t.startswith("text:")}

    def body(s):
        if s["doc"] not in texts:
            texts[s["doc"]] = (ROOT / s["doc"]).read_text(encoding="utf-8", errors="replace").split("\n")
        return "\n".join(texts[s["doc"]][s["lines"][0] - 1:s["lines"][1]])

    for s in idx.data["sections"]:
        ok = True
        for t in terms:
            if t.startswith("text:"):
                ok = bool(patterns[t].search(body(s)))
            elif t.startswith("in:"):
                ok = s["doc"].startswith(t[3:]) or s["id"].startswith(t[3:])
            else:
                ok = t in s["cites"] or t == s["id"] or t.lower() in s["title"].lower()
            if not ok:
                break
        if ok:
            ids.append(s["id"])
    return ids


SENTENCE = re.compile(r"(?<=[.;!?])\s+")


def cmd_grep(args):
    """Sentences matching a regex, each with its section and line: a sweep's cheap first pass."""
    idx = Index()
    rx = re.compile(args.regex, re.IGNORECASE)
    allowed = set(search(idx, list(args.terms))) if args.terms else None
    found, files = [], {}
    for s in idx.data["sections"]:
        if allowed is not None and s["id"] not in allowed:
            continue
        lines = files.setdefault(s["doc"], (ROOT / s["doc"]).read_text(encoding="utf-8", errors="replace").split("\n"))
        for n in range(s["lines"][0], s["lines"][1] + 1):
            line = lines[n - 1] if n - 1 < len(lines) else ""
            if rx.search(line):
                found += [(s["id"], n, t.strip()) for t in SENTENCE.split(line) if rx.search(t)]
    last = None
    for sid, n, text in found:
        if sid != last:
            print(f"== {sid}  {idx.by_id[sid]['doc']}")
            last = sid
        print(f"  :{n}  {text[:args.width]}")
    total = sum(tokens(t) for _, _, t in found)
    print(f"{len(found)} sentence(s) in {len({f[0] for f in found})} section(s), about {total:,} tokens")


def cmd_find(args):
    idx = Index()
    describe(idx, search(idx, args.terms))


def cmd_stat(args):
    idx = Index()
    docs = sorted(idx.data["docs"].items(), key=lambda kv: -kv[1]["tokens"])
    print(f"{len(docs)} files, {len(idx.data['sections'])} sections")
    for path, d in docs[:args.top]:
        print(f"  {d['tokens']:>9,}  {d['sections']:>4}  {d['prefix']:10s} {path}")
    big = sorted(idx.data["sections"], key=lambda s: -s["tokens"])[:5]
    print("largest sections: " + ", ".join(f"{s['id']} ({s['tokens']:,})" for s in big))
