#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tests/pages/browser_check.py: EldenNet's pages in GraceUI's real browser view, by GraceUI's space check (D265).

    python3 -I tests/pages/browser_check.py --web <a GraceUI checkout's web/> --pages src/en-overlay/pages \
        --fixtures tests/pages --work <a folder of its own>

For each of its folders, a space folder as grace-dev takes it (pages.json, settings.json and a fixture state's
topics.json), it runs `npm run -s check:space -- <folder> --golden <work>/<name>.dump --update` in GraceUI's web/ folder, which must
hold its built view (`npm ci --ignore-scripts`, then `npm run build`): the real view in Chromium (the Playwright the
machine has) over GraceUI's fake host, dumping this PC's browser, a paired device's and each shown_with switch turned
the other way, and failing on a console error, a CSP violation, an off-origin request or a text under 4.5:1. The dump
is read from the file the tool writes, not from its output, which a pipe cuts at 64 KiB at GraceUI 0.2.58 (the tool
exits before its output is flushed; asked of Grace). Of the dump, the part this
check holds to tests/pages/browser/<name>.txt (EN_UPDATE_GOLDENS=1 writes them anew): each view's rail, every page
named present or absent in it, and Tools › Workshop's and Settings › Developer's text whole (Workshop's gate first). The
other pages' text is left to the in-game goldens: at GraceUI 0.2.58 a page of many topics (Home, AI's) is dumped before
all its topics have come, so its text differs from run to run (asked of Grace). The folders:

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
WHOLE = ("tools.workshop", "settings.developer")  # the pages whose text the golden holds whole


def held(dump):
    """The part of a dump the golden holds: views, rails, each page's presence, and WHOLE's pages' text."""
    out = []
    whole = False
    for line in dump.splitlines():
        if line.startswith("#") or line.startswith("== ") or line.startswith("rail:"):
            out.append(line)
            whole = False
        elif line.startswith("page "):
            page = line[5:].split(":", 1)[0]
            whole = page in WHOLE
            out.append(line if whole or line.endswith(": absent") else "page %s: present" % page)
        elif line.startswith("  "):
            if whole:
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
        shutil.copyfile(args.pages / "pages.json", folder / "pages.json")
        settings = json.loads((args.pages / "settings.json").read_text(encoding="utf-8"))
        if panel:
            for s in settings["settings"]:
                if s["key"] == "debug.panel":
                    s["default"] = True
        (folder / "settings.json").write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
        shutil.copyfile(args.fixtures / state / "topics.json", folder / "topics.json")
        golden = goldens / (name + ".txt")
        dump = (args.work / (name + ".dump")).resolve()
        command = [npm, "run", "-s", "check:space", "--", str(folder.resolve()), "--golden", str(dump), "--update"]
        done = subprocess.run(command, cwd=args.web, capture_output=True, text=True, encoding="utf-8")
        if done.returncode == 2:
            print("browser_check: GraceUI's space check found no build: skipped (%s)" % done.stderr.strip()[-200:])
            return SKIP
        if done.returncode != 0:
            failures.append("%s: exit %d: %s" % (name, done.returncode, (done.stderr or done.stdout).strip()[-2000:]))
            continue
        text = held(dump.read_text(encoding="utf-8"))
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
