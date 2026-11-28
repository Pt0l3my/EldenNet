#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tests/pages/browser_check.py: EldenNet's pages in GraceUI's real browser view, by GraceUI's space check (D265).

    python3 -I tests/pages/browser_check.py --web <a GraceUI checkout's web/> --pages src/en-overlay/pages \
        --fixtures tests/pages --work <a folder of its own>

For each of its folders, a space folder as grace-dev takes it (pages.json, settings.json and a fixture state's
topics.json), it runs `npm run -s check:space -- <folder>` in GraceUI's web/ folder, which must hold its built view
(`npm ci --ignore-scripts`, then `npm run build`): the real view in Chromium (the Playwright the machine has) over
GraceUI's fake host, dumping this PC's browser, a paired device's and each shown_with switch turned the other way, and
failing on a console error, a CSP violation, an off-origin request or a text under 4.5:1. The dump is read from its
output, which since GraceUI 0.2.62 goes whole through a pipe (D269 (1); at 0.2.58 it was cut at 64 KiB, and this read
the --golden file instead), and is kept in <work>/<name>.dump. Of the dump, the part this check holds to
tests/pages/browser/<name>.txt (EN_UPDATE_GOLDENS=1 writes them anew): each view's rail, every page named present or
absent in it, and the text whole of Home, the AI pages, Tools › Workshop (its gate first), Settings › Developer and,
since v0.3, the memory pages (Characters › Playthrough, History › Conversations and History › Memories).
Since 0.2.62 a page is read once its every topic has come (D269 (2)), so Home's and AI's pages, of many topics, hold
still from run to run; the other pages' text is left to the in-game goldens. The folders:

  - world: the world state's fixtures as the settings' defaults leave them, Developer views off;
  - developer: the same with Developer views on by default, so that a paired device's view, with the switch on, shows
    Tools › Workshop absent by pc_only alone (GraceUI D264), and this PC's shows it behind its gate.

Exit 0 when every folder passes; 77 (CTest's skip) when the web view is not built or no Node is found; 1 otherwise.
Standard library only.
"""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys

SKIP = 77
FOLDERS = (("world", "world", False), ("developer", "world", True))
# The pages whose text the golden holds whole, with AI's: since v0.3 the memory pages too (S12.2), over the world's two
# journeys, the second in view.
WHOLE = ("home", "tools.workshop", "settings.developer", "characters.playthrough", "history.conversations",
         "history.memories")


def whole(page):
    return page in WHOLE or page.startswith("ai.")


def held(dump):
    """The part of a dump the golden holds: views, rails, each page's presence, and the whole pages' text."""
    out = []
    whole_page = False
    for line in dump.splitlines():
        if line.startswith("#") or line.startswith("== ") or line.startswith("rail:"):
            out.append(line)
            whole_page = False
        elif line.startswith("page "):
            page = line[5:].split(":", 1)[0]
            whole_page = whole(page)
            out.append(line if whole_page or line.endswith(": absent") else "page %s: present" % page)
        elif line.startswith("  "):
            if whole_page:
                out.append(line)
        else:
            out.append(line)
    return "\n".join(out) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--web", required=True, type=pathlib.Path, help="a GraceUI checkout's web/ folder")
    parser.add_argument("--pages", required=True, type=pathlib.Path, help="the folder of pages.json and settings.json")
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    parser.add_argument("--work", required=True, type=pathlib.Path)
    args = parser.parse_args(argv)
    npm = shutil.which("npm")
    if npm is None or not (args.web / "dist" / "index.html").is_file() or not (args.web / "node_modules").is_dir():
        print("browser_check: GraceUI's web view is not built (npm ci --ignore-scripts, npm run build): skipped")
        return SKIP
    update = os.environ.get("EN_UPDATE_GOLDENS") is not None
    goldens = args.fixtures / "browser"
    goldens.mkdir(exist_ok=True)
    shutil.rmtree(args.work, ignore_errors=True)
    failures = []
    for name, state, panel in FOLDERS:
        folder = args.work / name
        folder.mkdir(parents=True)
        # GraceUI 0.2.68's check:space welcomes every space at increment 0.2 (its FixtureSpace's default; no flag sets
        # another), so a page of 0.3 would be absent (D173): the copy marks EldenNet's 0.3 pages 0.2, the one change
        # made to the page data under test (a point for GraceUI: v0.3's notes).
        pages = json.loads((args.pages / "pages.json").read_text(encoding="utf-8"))
        for page in pages["pages"]:
            if page.get("increment", "0.1") > "0.2":
                page["increment"] = "0.2"
        (folder / "pages.json").write_text(json.dumps(pages, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        settings = json.loads((args.pages / "settings.json").read_text(encoding="utf-8"))
        if panel:
            for s in settings["settings"]:
                if s["key"] == "debug.panel":
                    s["default"] = True
        (folder / "settings.json").write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
        shutil.copyfile(args.fixtures / state / "topics.json", folder / "topics.json")
        golden = goldens / (name + ".txt")
        command = [npm, "run", "-s", "check:space", "--", str(folder.resolve())]
        done = subprocess.run(command, cwd=args.web, capture_output=True, text=True, encoding="utf-8")
        (args.work / (name + ".dump")).write_text(done.stdout, encoding="utf-8")
        if done.returncode == 2:
            print("browser_check: GraceUI's space check found no build: skipped (%s)" % done.stderr.strip()[-200:])
            return SKIP
        if done.returncode != 0:
            failures.append("%s: exit %d: %s" % (name, done.returncode, (done.stderr or done.stdout).strip()[-2000:]))
            continue
        text = held(done.stdout)
        if update:
            golden.write_text(text, encoding="utf-8")
        if not golden.is_file() or golden.read_text(encoding="utf-8") != text:
            failures.append("%s: the dump differs from tests/pages/browser/%s.txt (EN_UPDATE_GOLDENS=1 writes it)" %
                            (name, name))
    for f in failures:
        print("browser_check: " + f, file=sys.stderr)
    if not failures:
        print("browser_check: %d space folders pass in the browser view" % len(FOLDERS))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
