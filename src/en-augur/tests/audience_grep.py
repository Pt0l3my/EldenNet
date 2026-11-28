"""MEM §1.3's CI greps (since C++ has no module-private members): audiences are Augur's.

1. Nothing outside en-augur reads an event's audience rows (`.audience`): the core reads an event only through
   `visible_to` and writes its audience through `stored_audience`. The tests and the scenario tool's assertions may
   look; so may the F11 view, a developer's view of counts and the first row's numbers (PAGES SP8), and en-store's
   events file, which keeps the rows.
2. No SQL outside en-store's `events.cpp` names `event_audience` (its migrations and its tests may).

Run: python3 -I audience_grep.py --root <repository>. Exit 0 when clean, 1 with each finding on its own line.
"""
import argparse
import pathlib
import re
import sys

READ = re.compile(r"\.audience\b")
TABLE = re.compile(r"\bevent_audience\b")

# (folder, files allowed to read audiences there)
READERS_SCOPE = [
    ("src/en-core/src", set()),
    ("src/en-store/src", {"events.cpp"}),  # the store writes and reads the rows of event_audience, as stored rows
    ("src/en-overlay/src", {"view.cpp"}),  # the F11 view's own row: `audience` is a count there
    ("src/en-llm/src", set()),
    ("src/en-prompt/src", set()),
    ("src/en-tools/src", set()),
    ("src/eldennet/src", set()),
    ("src/eldennet/win", {"overlay.cpp"}),  # the F11 view: a count and the first row's numbers (SP8)
]
TABLE_ALLOWED = {
    "src/en-store/src/events.cpp",
    "src/en-store/src/migrations/pt/0001_init.sql",
}


def sources(folder: pathlib.Path):
    for p in sorted(folder.rglob("*")):
        if p.is_file() and p.suffix in {".cpp", ".hpp", ".h", ".sql"}:
            yield p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    root = pathlib.Path(ap.parse_args().root)
    found = []
    for rel, allowed in READERS_SCOPE:
        folder = root / rel
        if not folder.is_dir():
            continue
        for p in sources(folder):
            if p.name in allowed:
                continue
            for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                if READ.search(line.split("//", 1)[0]):
                    found.append(f"{p.relative_to(root)}:{n}: reads an event's audience outside en-augur")
    for p in sources(root / "src"):
        rel = p.relative_to(root).as_posix()
        if rel in TABLE_ALLOWED or rel.startswith("src/en-store/tests/") or rel.startswith("src/en-augur/tests/"):
            continue
        for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            code = line.split("//", 1)[0].split("--", 1)[0] if p.suffix == ".sql" else line.split("//", 1)[0]
            if TABLE.search(code):
                found.append(f"{rel}:{n}: names event_audience outside en-store's events file")
    for f in found:
        print(f)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
