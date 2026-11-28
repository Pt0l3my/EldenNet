#!/usr/bin/env python3
"""Preview screenshots of EldenNet in GraceUI, on the fake game's backdrop of our own (tools/preview/README.md).

    python3 -I tools/preview/shoot.py --build BUILD --out DIR [--size 1920x1080] [--format 24] [--check-format 28]
                                      [--only talk,events,pages,check] [--webp DIR] [--work DIR]

BUILD is a clang-cl build tree configured with EN_GRACEUI_DIR (a test build of GraceUI), whose CTest runs the fake
game's harness with GraceUI's cases; the script takes that harness command from CTest and runs cases of its own through
tests/fakegame/harness.py, as the harness runs its own: the launcher, eldennet.dll and GraceUI's folder installed as a
player would, the fake game presenting at the given size and DXGI format on its backdrop (--backdrop), its keys and
typing as the harness's GraceUI cases press them, and the frame as presented written at the frames named (--shot-at),
each after the fades have settled. Only the cases' own invented persona, model server and words appear.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "fakegame"))
import harness  # noqa: E402  (tests/fakegame/harness.py: its helpers, its cases' set-ups and its runner)

# Each run: a case of the harness's own form, and the frames it shoots, named. Every shot is at least 30 frames (500 ms
# at 60 frames a second, more when the frames come slower) after the press that made its state: past the surfaces' fades
# (120 ms in, 350 ms out) and GraceUI's page change (120 ms).
#
# talk: launcher-eldennet-graceui-talk's run (S11.3, part C): the probe DLL in the tests' realm, the test persona bound
#   to the fake game's invented row, a calm world and the fake model server's scripted reply. The hint before T (1500),
#   the box with the line typed (1560) before Enter (1700), and the reply's page in the lower third.
TALK_SHOTS = {1440: "hint", 1680: "chat", 1860: "lower-third"}
# events: launcher-eldennet-graceui-events' run: F11's view (debug.panel) opened at 720 on Augur's events.
EVENTS_SHOTS = {800: "f11"}
# pages: launcher-eldennet-graceui-pages' world (the title screen: in the world the fake player reads as in danger,
# which keeps the menu shut), with presses of our own, by the stops GraceUI 0.2.68 gives (the editor case's route to
# GraceUI's own pages): F5 opens the menu on EldenNet's Home with no focus ring, as a mouse player sees it (G42); ↓
# shows the ring; six Page Down to System › Status (past AI's four pages and Named NPCs), ← into the rail, eight ↓
# (Performance, Traces, Logs, Tools, Settings, Docs, Content packs) to GraceUI's own pages at its foot, Enter.
PAGES_KEYS = ("600:f5,720:down,840:next,880:next,920:next,960:next,1000:next,1040:next,1140:left,1200:down,1230:down,"
              "1260:down,1290:down,1320:down,1350:down,1380:down,1410:down,1440:return")
PAGES_SHOTS = {700: "menu-mouse", 800: "menu-ring", 1500: "graceui"}
# check: the talk run again at the other format, shooting the box alone, to compare with the set's.
CHECK_SHOTS = {1680: "chat"}

# The WebP files: 1280 by 720, lossless; quality 90 only where lossless passes 400 KB.
WEBP_SIZE = (1280, 720)
WEBP_LIMIT = 400 * 1024


def shots(frames, size, fmt):
    """The fake game's preview options, and a GraceUI report at each shot's frame (the state each shot shows)."""
    at = ",".join(str(f) for f in sorted(frames))
    return ["--backdrop", "--swap-size", size, "--swap-format", str(fmt), "--shot-dir", "{shots}", "--shot-at", at,
            "--grace-report", at, "--cursor-once"]


def check_shots(frames, fmt, size, states):
    """A run's check: each shot written at the size and format asked, and each frame's report in its state."""
    width, height = size.split("x")

    def check(r):
        server = harness.TALK_RUNS.pop(r.paths["case"], None)  # the talk runs' fake model server
        if server is not None:
            server.close()
        failures = []
        for frame in frames:
            line = next((e for e in r.events("FAKEGAME", "shot") if e.get("frame") == str(frame)), None)
            harness.need(line is not None and line.get("ok") == "1" and line.get("width") == width and
                         line.get("height") == height and line.get("format") == str(fmt),
                         "the shot at frame %d: %s" % (frame, line), failures)
        for frame, want in states.items():
            failures += harness.want_grace(r, frame, **want)
        failures += harness.check_lines(("LAUNCHER", "end", {"kind": "clean"}))(r)
        for e in r.events("FAKEGAME", "grace"):
            print("state at frame %s: holder=%s panels=%s shown=%s menu_open=%s menu_page=%s menu_focused=%s "
                  "menu_ring=%s" % (e.get("frame"), e.get("holder"), e.get("panels"), e.get("shown"),
                                    e.get("menu_open"), e.get("menu_page"), e.get("menu_focused"),
                                    e.get("menu_ring")))
        return failures
    return check


def define_runs(size, fmt, check_fmt):
    """The runs as harness cases (added to its CASES, named launcher-preview-*), with the frames each shoots."""
    h = harness
    runs = {}

    def talk(name, frames, fmt_):
        h.case(name, h.LGAME + ["--dll", "{eldennetdll}", "--console"] +
               h.game("--d3d12", "--calm", "--press-keys", h.TALK_KEYS, "--type-at", "1560:" + h.TALK_TYPED,
                      *shots(frames, size, fmt_), frames=2520),
               program="{launcher}", needs=h.GRACE_NEEDS + ("eldennet_probe",), toolchains=("clang-cl",),
               env=dict(h.GRACE_ENV, ELDENNET_TEST_REALM="eval"), timeout=300,
               prepare=h.install_eldennet(h.TALK_SETTINGS, probe=True, then=h.install_talk),
               check=check_shots(frames, fmt_, size, {f: {"holder": 2} for f in frames if frames[f] == "chat"}),
               what="preview shots: %s" % ", ".join(frames.values()))

    talk("launcher-preview-talk", TALK_SHOTS, fmt)
    runs["talk"] = ("launcher-preview-talk", TALK_SHOTS, "")
    h.case("launcher-preview-events", h.LGAME + ["--dll", "{eldennetdll}", "--console"] +
           h.game("--d3d12", "--press-f11-at", "720,900", *shots(EVENTS_SHOTS, size, fmt), frames=960),
           program="{launcher}", needs=h.GRACE_NEEDS, toolchains=("clang-cl",), env=h.GRACE_ENV, timeout=300,
           prepare=h.install_eldennet(h.EN_PANEL, then=h.install_graceui),
           check=check_shots(EVENTS_SHOTS, fmt, size, {}), what="preview shots: F11's view")
    runs["events"] = ("launcher-preview-events", EVENTS_SHOTS, "")
    h.case("launcher-preview-pages", h.LGAME + ["--dll", "{eldennetdll}", "--console"] +
           h.game("--d3d12", "--stay-title", "--press-keys", PAGES_KEYS, *shots(PAGES_SHOTS, size, fmt),
                  frames=1560),
           program="{launcher}", needs=h.GRACE_NEEDS, toolchains=("clang-cl",), env=h.GRACE_ENV, timeout=300,
           prepare=h.install_eldennet(h.EN_PANEL, then=h.install_graceui),
           check=check_shots(PAGES_SHOTS, fmt, size, {700: {"menu_open": 1, "menu_ring": 0, "menu_page": "home"},
                                                      800: {"menu_open": 1, "menu_ring": 1, "menu_page": "home"},
                                                      1500: {"menu_open": 1, "menu_page": "graceui.menu"}}),
           what="preview shots: F5's menu on EldenNet's pages and on GraceUI's own")
    runs["pages"] = ("launcher-preview-pages", PAGES_SHOTS, "")
    if check_fmt is not None:
        talk("launcher-preview-check", CHECK_SHOTS, check_fmt)
        runs["check"] = ("launcher-preview-check", CHECK_SHOTS, "-format%d" % check_fmt)
    return runs


def harness_command(build):
    """The harness's command line for one of GraceUI's cases, as CTest runs it in BUILD."""
    listed = subprocess.run(["ctest", "--show-only=json-v1"], cwd=build, capture_output=True, text=True, check=True)
    tests = {t["name"]: t for t in json.loads(listed.stdout).get("tests", [])}
    test = tests.get("fakegame.launcher-eldennet-graceui-talk")
    if test is None:
        raise SystemExit("%s runs no GraceUI case: configure a clang-cl tree with EN_GRACEUI_DIR (BUILD.md)" % build)
    return test["command"]


def run_args(command, case, work, shot_dir):
    """The harness's `run` arguments from CTest's command, for one of our cases, with our own work folder and the
    shots' folder as the path {shots}."""
    p = argparse.ArgumentParser(add_help=False)
    p.add_argument("case")
    p.add_argument("--toolchain", default="clang-cl")
    p.add_argument("--native", action="store_true")
    p.add_argument("--wine")
    p.add_argument("--xvfb-run")
    p.add_argument("--prefix")
    p.add_argument("--set", action="append", default=[])
    p.add_argument("--exe")
    p.add_argument("--vehprobe")
    p.add_argument("--work")
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args(command[command.index("run") + 1:])
    args.case = case
    args.work = work
    args.set = [s for s in args.set if not s.startswith("shots=")] + ["shots=" + shot_dir]
    args.verbose = False
    return args


def compare(a, b):
    """Two shots of one state at two formats: the mean difference in levels, and the share of pixels within 2 levels
    on every channel (the backdrop's dither differs by a level between 8 and 10 bits)."""
    from PIL import Image, ImageChops
    with Image.open(a) as x, Image.open(b) as y:
        diff = ImageChops.difference(x.convert("RGB"), y.convert("RGB"))
    pixels = diff.width * diff.height
    data = diff.tobytes()
    close = sum(1 for i in range(0, len(data), 3) if max(data[i], data[i + 1], data[i + 2]) <= 2)
    return sum(data) / len(data), close / pixels


def to_webp(bmp, out):
    """A shot as a 1280 by 720 WebP (LANCZOS): lossless, or quality 90 if lossless passes 400 KB. Answers the kind."""
    from PIL import Image
    with Image.open(bmp) as im:
        small = im.convert("RGB").resize(WEBP_SIZE, Image.LANCZOS)
    small.save(out, "WEBP", lossless=True, quality=100, method=6)
    if os.path.getsize(out) <= WEBP_LIMIT:
        return "lossless"
    small.save(out, "WEBP", quality=90, method=6)
    return "quality 90"


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--build", required=True, help="a clang-cl build tree configured with EN_GRACEUI_DIR")
    p.add_argument("--out", required=True, help="the folder for the shots, outside the repository")
    p.add_argument("--size", default="1920x1080", help="the swap chain's size, WxH (default 1920x1080)")
    p.add_argument("--format", type=int, choices=(24, 28), default=24,
                   help="the swap chain's DXGI format: 24 (R10G10B10A2_UNORM, the default) or 28 (R8G8B8A8_UNORM)")
    p.add_argument("--check-format", default="28", choices=("24", "28", "none"),
                   help="the format of the check run, which shoots the box again to compare (default 28)")
    p.add_argument("--only", default="", help="runs to make, comma-separated: talk, events, pages, check")
    p.add_argument("--webp", help="a folder for the 1280 by 720 WebP files (preview-NAME.webp)")
    p.add_argument("--work", help="the harness's work folder (default: BUILD/tests/fakegame/preview)")
    a = p.parse_args()
    build = os.path.abspath(a.build)
    out = os.path.abspath(a.out)
    for folder in [out] + ([os.path.abspath(a.webp)] if a.webp else []):
        if os.path.commonpath([folder, ROOT]) == ROOT:
            raise SystemExit("the shots go in a folder outside the repository, not %s" % folder)
    check_fmt = None if a.check_format == "none" else int(a.check_format)
    runs = define_runs(a.size, a.format, check_fmt)
    only = [r for r in a.only.split(",") if r] or list(runs)
    unknown = [r for r in only if r not in runs]
    if unknown:
        raise SystemExit("no run %s (the runs: %s)" % (", ".join(unknown), ", ".join(runs)))
    command = harness_command(build)
    work = os.path.abspath(a.work or os.path.join(build, "tests", "fakegame", "preview"))
    os.makedirs(work, exist_ok=True)
    first = run_args(command, "", work, out)
    if not os.path.exists(os.path.join(first.prefix, "system.reg")):
        if harness.cmd_setup(first) != 0:  # the prefix, as the harness's first test makes it
            raise SystemExit("the Wine prefix could not be made")
    made = {}
    failed = []
    for name in only:
        case, frames, suffix = runs[name]
        raw = os.path.join(out, "runs", name)
        shutil.rmtree(raw, ignore_errors=True)
        os.makedirs(raw)
        if harness.cmd_run(run_args(command, case, work, raw)) != 0:
            failed.append(name)
        for frame, state in frames.items():
            bmp = os.path.join(raw, "frame-%d.bmp" % frame)
            if os.path.exists(bmp):
                target = os.path.join(out, "%s%s.bmp" % (state, suffix))
                os.replace(bmp, target)
                made[state + suffix] = target
    if "chat" in made and "chat-format%s" % check_fmt in made:
        mean, close = compare(made["chat"], made["chat-format%s" % check_fmt])
        print("the box at formats %d and %d: a mean difference of %.2f levels, %.1f%% of pixels within 2 levels" %
              (a.format, check_fmt, mean, 100 * close))
    if a.webp:
        os.makedirs(a.webp, exist_ok=True)
        for state, bmp in sorted(made.items()):
            if "-format" in state:
                continue
            target = os.path.join(os.path.abspath(a.webp), "preview-%s.webp" % state)
            kind = to_webp(bmp, target)
            print("%s: %s, %d bytes" % (os.path.basename(target), kind, os.path.getsize(target)))
    for state, bmp in sorted(made.items()):
        print("shot %s: %s" % (state, os.path.basename(bmp)))
    if failed:
        print("runs that failed their checks: %s" % ", ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
