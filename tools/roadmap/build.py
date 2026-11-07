#!/usr/bin/env python3
"""EldenNet's roadmap: reads docs/ROADMAP.md, the one source, and writes the page
docs/roadmap/index.html from tools/roadmap/page.html with the data embedded. One roadmap covers
EldenNet and GraceUI.

  python3 -I tools/roadmap/build.py           write the page
  python3 -I tools/roadmap/build.py --check   check the source and write nothing; fails on a duplicate
                                              id, a need that names no id, an unknown or misspelt
                                              status, an unknown field, a part with no id, an empty
                                              title, a need cycle, a reference that resolves to
                                              nothing, or a page that is no longer what the source
                                              builds

The source's syntax is explained in the comment at its top. References are:
  - the codes of EldenNet's wp index (tools/workplan/wplib): DESIGN§16.2, MEM§12, SN04, ER07, REVIEW;
  - the owner's decisions O1, O2 and on, the entries of docs/DECISIONS.md (DESIGN §19's rows until
    that file exists);
  - GraceUI's: GraceUI:DESIGN§n, G1 and on (its owner decisions), GraceUI:Dn (its design log) and
    GraceUI:<path>, checked against GraceUI's files when its checkout sits beside EldenNet's, else
    accepted unchecked;
  - repository paths, which must exist;
  - tasks (T-NNNN) from tools/workplan/tasks.json, once it exists.
Adapted from the roadmap tool of the owner's Skyrizz (tools/roadmap). Standard library only.
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "docs/ROADMAP.md"
PAGE = ROOT / "docs/roadmap/index.html"
TEMPLATE = pathlib.Path(__file__).resolve().parent / "page.html"
TASKS = ROOT / "tools/workplan/tasks.json"           # none yet: a part's status is written on the part
DECISIONS = "docs/DECISIONS.md"
DESIGN = "docs/design/DESIGN.md"
GRACEUI_DIRS = ("graceui", "GraceUI")                 # GraceUI's checkout, beside EldenNet's

STATUSES = ("done", "active", "next", "planned", "waiting", "idea")
FIELDS = ("needs", "refs", "gives", "status", "updated")
HEADING = re.compile(r"^(#{2,3})\s+(\S+)\s*(.*?)\s*$")
PART = re.compile(r"^[-*+]\s+(?![a-z]+\s*:)(\S+)\s*(.*?)\s*$")
FIELD = re.compile(r"^(\s*)(?:[-*+]\s+)?([A-Za-z]+)\s*:\s*(.*?)\s*$")
STATUS_TAIL = re.compile(r"\s*\[(\s*([A-Za-z]+)\s*)\]\s*$")
UNKNOWN_FIELD = re.compile(r"^\s*[-*+]\s+([A-Za-z]+)\s*:(?!//)|^[ \t]+([a-z]+):(?!//)")   # never a wrapped link
STAGE_ID = re.compile(r"^S\d+[a-z]?$")
STEP_ID = re.compile(r"^S\d+[a-z]?\.\d+$")
LETTER = re.compile(r"^[a-z]{1,2}$")
OWNER_ID = re.compile(r"O\d+")
GRACE_ID = re.compile(r"G\d+")
GRACE_LOG = re.compile(r"GraceUI:(D\d+)")
GRACE_SECTION = re.compile(r"GraceUI:DESIGN§(\d+(?:\.\d+)*)")


def split_list(value):
    return [v for v in re.split(r"[,;\s]+", value) if v and v not in ("-", "—", "none")]


def item(kind, ident, title, line):
    return {"kind": kind, "id": ident, "title": title, "status": None, "needs": [], "refs": [],
            "gives": "", "note": "", "line": line}


def near(a, b):
    """The optimal string alignment distance between a and b, at most 3 (enough to tell a typo)."""
    prev2, prev = None, list(range(len(b) + 1))
    for i in range(1, len(a) + 1):
        cur = [i] + [0] * len(b)
        for j in range(1, len(b) + 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1]))
            if i > 1 and j > 1 and a[i - 1] == b[j - 2] and a[i - 2] == b[j - 1]:
                cur[j] = min(cur[j], prev2[j - 2] + 1)
        prev2, prev = prev, cur
    return min(prev[-1], 3)


def misspelt(word):
    """The status a bracketed word was meant to be, when it is one written wrong: in another case,
    with spaces inside the brackets, or within one letter (two for five letters or more)."""
    low = word.lower()
    for st in STATUSES:
        if near(low, st) <= (1 if len(st) <= 4 else 2):
            return st
    return None


def take_status(title):
    """(title, status, problem). A last bracketed word written exactly as a status ([next]) is the
    status. One written wrong ([Planed], [next ], [DONE]) is reported and taken out of the title. Any
    other lower-case word is taken as the status, for the check to report as unknown; any other word,
    such as a page's name ([Pacing]), stays in the title."""
    m = STATUS_TAIL.search(title)
    if not m:
        return title, None, None
    inner, word, rest = m.group(1), m.group(2), title[:m.start()].rstrip()
    if inner == word and word in STATUSES:
        return rest, word, None
    meant = misspelt(word)
    if meant:
        return rest, meant, f"status [{inner}] is misspelt: write [{meant}]"
    if inner == word and word.islower():
        return rest, word, None
    return title, None, None


def parse(text):
    """The source as {meta, stages}, and the problems found while reading it: [(line, message)]."""
    meta = item("roadmap", "", "", 0)
    stages, problems = [], []
    stage = step = part = None
    last = None                    # (target, field) a continuation line extends
    in_comment = False
    for n, raw in enumerate(text.split("\n"), 1):
        line = raw.rstrip()
        if in_comment:
            in_comment = "-->" not in line
            continue
        if line.lstrip().startswith("<!--"):
            in_comment = "-->" not in line
            continue
        if not line.strip():
            last = None if last and last[1] == "note" else last
            continue
        h = HEADING.match(line)
        if h:
            ident, (title, status, bad) = h.group(2), take_status(h.group(3))
            if bad:
                problems.append((n, f"{ident}: {bad}"))
            if len(h.group(1)) == 2:
                if not STAGE_ID.match(ident):
                    problems.append((n, f"a stage's heading starts with its id (S3, S1b), not {ident!r}"))
                stage = item("stage", ident, title, n)
                stage["status"], stage["steps"] = status, []
                stages.append(stage)
                step = part = last = None
            else:
                if stage is None or not STEP_ID.match(ident) or not ident.startswith(stage["id"] + "."):
                    problems.append((n, f"a step's heading starts with its stage's id, a dot and a number, not {ident!r}"))
                step = item("step", ident, title, n)
                step["status"], step["parts"] = status, []
                if stage is not None:
                    stage["steps"].append(step)
                part = last = None
            continue
        if line.startswith("#"):
            continue                                     # the document's own title
        f = FIELD.match(line)
        if f and f.group(2).lower() in FIELDS:
            indent, key, value = len(f.group(1).expandtabs(4)), f.group(2).lower(), f.group(3)
            target = part if (indent > 0 and part is not None) else (step or stage or meta)
            if target is not part:
                part = None
            set_field(target, key, value, n, problems)
            last = (target, key)
            continue
        u = UNKNOWN_FIELD.match(line)
        if u:                                             # a field's name misspelt: reported, never a note
            problems.append((n, f"unknown field {(u.group(1) or u.group(2))!r} (one of {', '.join(FIELDS)})"))
            continue
        p = PART.match(line)
        if p and not raw.startswith((" ", "\t")):
            if step is None:
                problems.append((n, "a part outside any step"))
                continue
            ident, rest = p.group(1), p.group(2)
            if LETTER.match(ident):
                want = letter_after(step["parts"])
                if ident == want and not rest[:1].islower():
                    ident = step["id"] + "." + ident
                else:                                   # "- a new part": an article, not part a
                    problems.append((n, f"a part with no id: start it with its id ({step['id']}.{want}) or "
                                        f"its letter in sequence ({want}) and a title that is not lower case"))
                    ident, rest = f"{step['id']}.?{n}", (ident + " " + rest).strip()
            elif not ident.startswith(step["id"] + "."):
                problems.append((n, f"part {ident!r} is not under its own step {step['id']}"))
            title, status, bad = take_status(rest)
            if bad:
                problems.append((n, f"{ident}: {bad}"))
            part = item("part", ident, title, n)
            part["status"] = status
            step["parts"].append(part)
            last = (part, "title")
            continue
        if raw.startswith((" ", "\t")) and last:          # a wrapped line continues what it follows
            target, key = last
            if key == "title":
                title, status, bad = take_status((target["title"] + " " + line.strip()).strip())
                if bad:
                    problems.append((n, f"{target['id']}: {bad}"))
                target["title"] = title
                target["status"] = status or target["status"]
            else:
                set_field(target, key, line.strip(), n, problems, extend=True)
            continue
        target = step or stage or meta                     # anything else is the heading's note
        target["note"] = (target["note"] + " " + line.strip()).strip()
        last, part = (target, "note"), None
    roadmap = {"meta": meta, "stages": stages}
    for it in walk(roadmap):
        if not it["title"]:
            problems.append((it["line"], f"{it['id']} has no title"))
    return roadmap, problems


def next_letter(letter):
    """The letter after a part's letter in its step: a to z, then aa, ab and on; "" gives a."""
    if not letter:
        return "a"
    if letter[-1] != "z":
        return letter[:-1] + chr(ord(letter[-1]) + 1)
    return (next_letter(letter[:-1]) if len(letter) > 1 else "a") + "a"


def letter_after(parts):
    """The letter a step's next part takes: the one after its last part's, or, when that part had no
    id of its own, the one its place gives."""
    last = parts[-1]["id"].rsplit(".", 1)[-1] if parts else ""
    if last and not LETTER.match(last):
        last = ""
        for _ in parts:
            last = next_letter(last)
    return next_letter(last)


def set_field(target, key, value, n, problems, extend=False):
    if key in ("needs", "refs"):
        target[key] += split_list(value)
    elif key == "status":
        target["status"] = value.strip().strip("[]").lower() or None
    elif key == "gives" or key == "note":
        target[key] = (target[key] + " " + value).strip() if extend else value
    elif key == "updated":
        target["updated"] = value


def walk(roadmap):
    """Every stage, step and part, in order."""
    for stage in roadmap["stages"]:
        yield stage
        for step in stage["steps"]:
            yield step
            yield from step["parts"]


def normalise_needs(roadmap):
    """A bare letter in a part's needs is a part of its own step."""
    for stage in roadmap["stages"]:
        for step in stage["steps"]:
            for part in step["parts"]:
                part["needs"] = [step["id"] + "." + x if LETTER.match(x) else x for x in part["needs"]]


def load_tasks():
    try:
        return {t["id"]: t for t in json.loads(TASKS.read_text(encoding="utf-8"))["tasks"]}
    except (OSError, ValueError, KeyError):
        return {}


def settle_statuses(roadmap, tasks):
    """A part without a status takes its tasks' (done when all are done), else planned; a step or a
    stage without one takes its children's."""
    for stage in roadmap["stages"]:
        for step in stage["steps"]:
            for part in step["parts"]:
                if part["status"] is None:
                    own = [tasks[r]["status"] for r in part["refs"] if r in tasks and tasks[r].get("status") != "dropped"]
                    part["status"] = ("done" if own and all(s == "done" for s in own) else "planned")
                    part["from_tasks"] = bool(own)
            if step["status"] is None:
                step["status"] = derive([p["status"] for p in step["parts"]])
        if stage["status"] is None:
            stage["status"] = derive([s["status"] for s in stage["steps"]])


def derive(children):
    if not children:
        return "planned"
    s = set(children)
    if s <= {"done", "idea"} and "done" in s:
        return "done"
    if "active" in s or ("done" in s and s - {"done", "idea"}):
        return "active"
    for st in ("next", "planned", "waiting", "idea"):
        if st in s:
            return st if st != "waiting" or s <= {"waiting", "done", "idea"} else "planned"
    return "planned"


def ref_kind(ref):
    if ref.startswith("GraceUI:") or GRACE_ID.fullmatch(ref):
        return "graceui"
    if OWNER_ID.fullmatch(ref):
        return "decision"
    if "/" in ref or re.search(r"\.[A-Za-z]{1,5}$", ref):
        return "path"
    if re.fullmatch(r"T-\d{4}", ref):
        return "task"
    if "§" in ref:
        return "section"
    return "document"                        # a document's code in the index: SN04, REVIEW, CLAUDE


def workplan_index():
    sys.path.insert(0, str(ROOT / "tools/workplan"))
    from wplib.index import Index           # the wp tools' own index (built if stale)
    return Index()


# --- the owner's decisions and GraceUI's files ---------------------------------------------------

ENTRY = r"^\s*(?:(#{1,6})\s+|([-*+]\s+|\d+\.\s+|\|\s*))?(?:\*\*|`)?(%s)(?:\*\*|`)?(?=$|[\s.:,;|)\u2014\u2013-])(.*)$"


def entry_titles(text, pattern):
    """{id: title} for every entry a decisions file or a decisions table holds: a heading, a list
    item, a table row or a paragraph that starts with the id (## O12, **O12**, | O12 |). A heading wins
    over a list item or a row, and either over a paragraph, so prose that opens with an id ("O1–O29
    were decided ...") is never taken for the entry; within a form, the first line wins. The title is
    the rest of that line, a table row's next cell, or, under a heading that is the id alone, the next
    line's bold lead (**Project name** · 2026-11-06) or, failing that, the line itself."""
    rx = re.compile(ENTRY % pattern)
    best, fence, waiting = {}, False, None
    for line in text.split("\n"):
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        if waiting and line.strip():
            if not line.lstrip().startswith("#"):
                lead = re.match(r"^\s*\*\*(.+?)\*\*", line)
                best[waiting] = (0, short(lead.group(1) if lead else line.split(" \u00b7 ")[0]))
            waiting = None
        m = rx.match(line)
        if not m:
            continue
        rank = 0 if m.group(1) else 1 if m.group(2) else 2
        ident, rest = m.group(3), m.group(4)
        if ident in best and best[ident][0] <= rank:
            continue
        if line.lstrip().startswith("|"):
            rest = next((c.strip() for c in rest.split("|") if c.strip()), "")
        best[ident] = (rank, short(rest))
        if rank == 0 and not best[ident][1]:
            waiting = ident
    return {k: v[1] for k, v in best.items()}


def short(title):
    title = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", title)                 # a link's text only
    title = re.sub(r"[*`]", "", title)
    title = re.sub(r"^[\s.:,;|—–-]+", "", title)
    title = re.sub(r"^\(\d{4}-\d{2}-\d{2}[^)]*\)\s*", "", title)            # an entry's date
    title = re.sub(r"^[\s.:,;—–-]+", "", title).strip()
    return title if len(title) <= 160 else title[:157].rstrip() + "..."


def section_rows(text, number):
    """The text of one numbered section (## 19. ...) of a design document, up to the next heading of
    its level or higher; "" when it has none."""
    lines, out, level = text.split("\n"), [], None
    for line in lines:
        h = re.match(r"^(#{1,6})\s+(?:§\s*)?(\d+(?:\.\d+)*)\.?\s", line)
        if level is None:
            if h and h.group(2) == number:
                level = len(h.group(1))
            continue
        if re.match(r"^#{1,%d}\s" % level, line):
            break
        out.append(line)
    return "\n".join(out)


def owner_decisions(root=None):
    """(where, {O-id: title}): docs/DECISIONS.md's entries, or DESIGN §19's rows until it exists."""
    root = root or ROOT
    try:
        return DECISIONS, entry_titles((root / DECISIONS).read_text(encoding="utf-8"), r"O\d+")
    except OSError:
        pass
    try:
        rows = section_rows((root / DESIGN).read_text(encoding="utf-8"), "19")
    except OSError:
        return DECISIONS, {}
    return DESIGN, entry_titles(rows, r"O\d+")


def graceui_root(root=None):
    """GraceUI's checkout beside EldenNet's, or None."""
    base = (root or ROOT).parent
    for name in GRACEUI_DIRS:
        if (base / name / "docs/DESIGN.md").is_file():
            return base / name
    return None


def graceui_facts(groot):
    """What GraceUI's files hold: its owner decisions (G-ids), its design log's entries (D-ids) and
    its design's numbered sections; None when there is no checkout."""
    if groot is None:
        return None
    design = (groot / "docs/DESIGN.md").read_text(encoding="utf-8")
    try:
        owner = entry_titles((groot / "docs/DECISIONS.md").read_text(encoding="utf-8"), r"G\d+")
    except OSError:
        owner = entry_titles(section_rows(design, "12"), r"G\d+")
    return {"G": owner, "D": entry_titles(section_rows(design, "13"), r"D\d+"),
            "sections": set(re.findall(r"(?m)^#{1,6}\s+(?:§\s*)?(\d+(?:\.\d+)*)\.?\s", design))}


def graceui_ref(ref, facts, groot):
    """(ok, title) for a GraceUI reference. The title never comes from GraceUI's files, so the page is
    the same whether or not that checkout is present."""
    sec, log = GRACE_SECTION.fullmatch(ref), GRACE_LOG.fullmatch(ref)
    if GRACE_ID.fullmatch(ref):
        return (facts is None or ref in facts["G"]), f"GraceUI's owner decision {ref}"
    if sec:
        return (facts is None or sec.group(1) in facts["sections"]), f"GraceUI's design, §{sec.group(1)}"
    if log:
        return (facts is None or log.group(1) in facts["D"]), f"GraceUI's design log, {log.group(1)}"
    path = ref[len("GraceUI:"):]
    if not path or "§" in path or ".." in pathlib.PurePosixPath(path).parts or path.startswith("/"):
        return False, ""
    # A path with a suffix is a file, one without is a folder, so the title never depends on the checkout;
    # a GraceUI file with no suffix (NOTICE, LICENSE) would read as a folder.
    title = "a file in GraceUI" if pathlib.PurePosixPath(path).suffix else "a folder in GraceUI"
    if groot is None:
        return True, title
    return (groot / path).exists(), title


def describe_refs(roadmap, tasks, index):
    """{ref: {kind, title, where}} for every reference, and the ones that resolve to nothing."""
    out, missing = {}, []
    decisions = grace = None
    for it in walk_all(roadmap):
        for ref in it["refs"]:
            if ref in out or ref in missing:
                continue
            kind = ref_kind(ref)
            title, where = "", ""
            if kind == "path":
                p = ROOT / ref
                ok = ".." not in pathlib.PurePosixPath(ref).parts and not ref.startswith("/") and p.exists()
                title = "a file" if ok and p.is_file() else "a folder"
            elif kind == "task":
                ok = ref in tasks
                if ok:
                    title, where = tasks[ref].get("title", ""), "tools/workplan/tasks.json"
            elif kind == "decision":
                decisions = decisions or owner_decisions()
                ok = ref in decisions[1]
                if ok:
                    title, where = decisions[1][ref], decisions[0]
            elif kind == "graceui":
                if grace is None:
                    groot = graceui_root()
                    grace = (groot, graceui_facts(groot))
                ok, title = graceui_ref(ref, grace[1], grace[0])
                where = "GraceUI"
            else:
                ids = index.resolve(ref) if index is not None else []
                ids = ids or (index.resolve(ref + "~1") if index is not None else [])
                ok = bool(ids)
                if ok:
                    s = index.by_id[ids[0]]
                    title, where = clean_title(s["title"], ref), s["doc"]     # no line: it moves with every edit
            if ok:
                out[ref] = {"kind": kind, "title": title, "where": where}
            else:
                missing.append(ref)
    return out, missing


def clean_title(title, ref):
    title = re.sub(r"^\(start of (.+)\)$", r"\1", title)
    title = re.sub(r"[*`]", "", title)
    return title if len(title) <= 160 else title[:157].rstrip() + "..."


def walk_all(roadmap):
    yield roadmap["meta"]
    yield from walk(roadmap)


WHY_MISSING = {
    "path": "no such file or folder",
    "decision": f"no entry in {DECISIONS} (or DESIGN §19, until that file exists)",
    "graceui": "GraceUI's files have no such decision, log entry, section or file",
    "task": "no such task in tools/workplan/tasks.json",
}


def check(roadmap, problems, tasks, index):
    """Every problem: [(line, message)]."""
    out = list(problems)
    seen = {}
    for it in walk(roadmap):
        if it["id"] in seen:
            out.append((it["line"], f"{it['id']} is used twice (first on line {seen[it['id']]})"))
        else:
            seen[it["id"]] = it["line"]
    for it in walk_all(roadmap):
        if it["status"] is not None and it["status"] not in STATUSES:
            out.append((it["line"], f"{it['id'] or 'the roadmap'}: unknown status {it['status']!r} "
                                    f"(one of {', '.join(STATUSES)})"))
        for need in it["needs"]:
            if need not in seen:
                out.append((it["line"], f"{it['id'] or 'the roadmap'} needs {need!r}, which names no id"))
            elif need == it["id"]:
                out.append((it["line"], f"{it['id']} needs itself"))
    cycle = find_cycle(roadmap, seen)
    if cycle:
        out.append((0, "the needs go round in a circle: " + " -> ".join(cycle)))
    _, missing = describe_refs(roadmap, tasks, index)
    lines = {}
    for it in walk_all(roadmap):
        for ref in it["refs"]:
            lines.setdefault(ref, it["line"])
    for ref in missing:
        what = WHY_MISSING.get(ref_kind(ref), "the wp index can't resolve it")
        out.append((lines.get(ref, 0), f"reference {ref!r}: {what}"))
    return sorted(out)


def find_cycle(roadmap, ids):
    """A circle of needs among the parts and steps (a step stands for its parts), or None."""
    graph = {}
    for it in walk(roadmap):
        graph.setdefault(it["id"], set()).update(n for n in it["needs"] if n in ids)
    for stage in roadmap["stages"]:            # a step needs what its parts need outside it, and a
        for step in stage["steps"]:            # part needs what its step needs
            own, declared = {p["id"] for p in step["parts"]}, set(graph[step["id"]])
            for part in step["parts"]:
                graph[step["id"]].update(n for n in graph[part["id"]] if n not in own)
                graph[part["id"]].update(declared)
    state, path = {}, []

    def visit(node):
        state[node] = 1
        path.append(node)
        for nxt in graph.get(node, ()):
            if state.get(nxt) == 1:
                return path[path.index(nxt):] + [nxt]
            if not state.get(nxt):
                found = visit(nxt)
                if found:
                    return found
        state[node] = 2
        path.pop()
        return None

    sys.setrecursionlimit(10000)
    for node in list(graph):
        if not state.get(node):
            found = visit(node)
            if found:
                return found
    return None


def page_data(roadmap, tasks, index):
    refs, _ = describe_refs(roadmap, tasks, index)
    strip = lambda it: {k: v for k, v in it.items() if k not in ("line", "kind") and v not in ("", None, [])}
    stages = []
    for stage in roadmap["stages"]:
        s = strip({k: v for k, v in stage.items() if k != "steps"})
        s["steps"] = []
        for step in stage["steps"]:
            t = strip({k: v for k, v in step.items() if k != "parts"})
            t["parts"] = [strip(p) for p in step["parts"]]
            s["steps"].append(t)
        stages.append(s)
    meta = roadmap["meta"]
    return {"updated": meta.get("updated", ""), "note": meta["note"], "refs_top": meta["refs"],
            "stages": stages, "refs": refs, "statuses": list(STATUSES)}


def load(path=SOURCE):
    roadmap, problems = parse(path.read_text(encoding="utf-8"))
    normalise_needs(roadmap)
    return roadmap, problems


def render(data, template=TEMPLATE):
    blob = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    return template.read_text(encoding="utf-8").replace("/*ROADMAP-DATA*/null", blob, 1)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="check the source and the page; write nothing")
    ap.add_argument("--source", type=pathlib.Path, default=SOURCE)
    ap.add_argument("--out", type=pathlib.Path, default=PAGE)
    ap.add_argument("--fragment", type=pathlib.Path, help="also write the page for publishing on claude.ai: "
                    "the whole document, without its content-security line")
    args = ap.parse_args(argv)
    shown = lambda p: p.resolve().relative_to(ROOT) if p.resolve().is_relative_to(ROOT) else p.name
    roadmap, problems = load(args.source)
    tasks, index = load_tasks(), workplan_index()
    found = [(shown(args.source), n, msg) for n, msg in check(roadmap, problems, tasks, index)]
    html = None
    if not found:
        settle_statuses(roadmap, tasks)
        html = render(page_data(roadmap, tasks, index))
        if args.check and stale(html, args.out):
            found.append((shown(args.out), 0, "the page is not what the source and the template build "
                                              "now: run python3 -I tools/roadmap/build.py"))
    for where, n, msg in found:
        print(f"problem: {where}:{n}: {msg}")
    parts = sum(len(s["parts"]) for st in roadmap["stages"] for s in st["steps"])
    steps = sum(len(st["steps"]) for st in roadmap["stages"])
    print(f"{len(roadmap['stages'])} stages, {steps} steps, {parts} parts: {len(found)} problem(s)")
    if found:
        return 1
    if args.check:
        return 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(html, encoding="utf-8")
    print(f"wrote {shown(args.out)}")
    if args.fragment:
        args.fragment.parent.mkdir(parents=True, exist_ok=True)
        args.fragment.write_text(fragment(html), encoding="utf-8")
        print(f"wrote {shown(args.fragment)}")
    return 0


def fragment(html):
    """The page as claude.ai publishes it: a whole document (its publishing tools take one that starts
    with its doctype), without the content-security line, since the published page is served under
    claude.ai's own policy."""
    return re.sub(r'<meta http-equiv="Content-Security-Policy"[^>]*>\s*', "", html, count=1, flags=re.I)


def stale(html, page):
    """Whether the page on disk differs from the one built in memory. The page carries no build date
    (its "Updated" is the source's own updated: line), so the two compare as they are."""
    try:
        return page.read_text(encoding="utf-8") != html
    except OSError:
        return True


if __name__ == "__main__":
    sys.exit(main())
