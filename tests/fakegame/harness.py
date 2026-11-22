#!/usr/bin/env python3
"""The fake game's harness: runs fakegame.exe (and vehprobe.dll in it) under Wine on Xvfb, or directly on Windows,
and checks each behaviour from outside: the exit code, the status lines it prints ("FAKEGAME <event> key=value ..."
and "VEHPROBE ..."), and, for --describe, the executable file itself, read here with our own small PE reader.

CTest runs one case per test (tests/fakegame/CMakeLists.txt):

    harness.py list --toolchain clang-cl
    harness.py setup --wine W --xvfb-run X --prefix P
    harness.py run CASE --toolchain T --exe fakegame.exe --vehprobe vehprobe.dll --work DIR --wine W --xvfb-run X --prefix P

Under Wine the process's exit status keeps only the low 8 bits of the Windows exit code (0xC0000005 arrives as 5).
Every name and number the cases check is the fake game's own, invented (README.md).
"""
import argparse
import json
import os
import re
import shutil
import signal
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pages"))
import browser  # noqa: E402  (tests/pages/browser.py: a browser of the tests' own, for GraceUI's server)
import fake_models  # noqa: E402  (tests/pages/fake_models.py: a fake model server of the tests' own, on loopback)

AV = 0xC0000005

# --- Cases ---------------------------------------------------------------------------------------------------------------


class Case:
    """One behaviour: the program and its arguments (`{name}` stands for a path: exe, vehprobe, work, status, or one
    given with --set name=PATH, as Windows sees it), what must come back, and what the case stands for. A case that
    `needs` a path given with --set is listed only when CMake has it (--have), so later parts (the DLL, the launcher)
    add their chain cases here: program="{launcher}", needs=("launcher", "eldennet"). `cwd`, a `{name}` path, is the
    folder the program starts in (relative paths in its arguments are read from there); else the harness's own.
    `during`, a function of the case's paths and a dictionary, runs on a thread of its own while the program runs (a
    browser of the tests' own that joins GraceUI's server, say); what it puts in the dictionary is the result's
    `during` for the check."""

    def __init__(self, name, args, exit=0, expect=(), forbid=(), check=None, env=None, toolchains=("clang-cl", "mingw"),
                 timeout=60, kill_after=None, what="", program="{exe}", needs=(), prepare=None, settle=False, cwd=None,
                 during=None):
        self.name = name
        self.during = during
        self.cwd = cwd
        self.prepare = prepare  # a launcher case's own files, made before it runs (launcher_setup)
        self.settle = settle    # wait for every process the case started (a watcher) before the prefix is stopped
        self.program = program
        self.needs = tuple(needs)
        self.args = list(args)
        self.exit = exit
        self.expect = list(expect)
        self.forbid = list(forbid)
        self.check = check
        self.env = env or {}
        self.toolchains = toolchains
        self.timeout = timeout
        self.kill_after = kill_after
        self.what = what


CASES = []


def case(*a, **k):
    CASES.append(Case(*a, **k))


def end_of(result):
    return result.last("FAKEGAME", "end")


def need(cond, text, failures):
    if not cond:
        failures.append(text)


def check_end(**want):
    """The end line's numbers: each key equal to a number, or within (lo, hi)."""
    def check(result):
        failures = []
        end = end_of(result)
        if end is None:
            return ["no end line"]
        for key, value in want.items():
            got = end.get(key)
            if got is None:
                failures.append("end line has no %s" % key)
                continue
            got = float(got)
            if isinstance(value, tuple):
                need(value[0] <= got <= value[1], "end %s=%s, wanted %s..%s" % (key, got, value[0], value[1]), failures)
            else:
                need(got == value, "end %s=%s, wanted %s" % (key, got, value), failures)
        return failures
    return check


def check_all(*checks):
    def check(result):
        failures = []
        for c in checks:
            failures += c(result)
        return failures
    return check


def check_selftest(**want):
    def check(result):
        failures = []
        st = result.last("FAKEGAME", "selftest", "registered")
        if st is None:
            return ["no selftest summary line"]
        for key, value in want.items():
            got = st.get(key)
            if isinstance(value, tuple):
                need(got is not None and value[0] <= float(got) <= value[1],
                     "selftest %s=%s, wanted %s..%s" % (key, got, value[0], value[1]), failures)
            else:
                need(got == str(value), "selftest %s=%s, wanted %s" % (key, got, value), failures)
        return failures
    return check


def check_moves(result):
    failures = []
    moved = result.events("FAKEGAME", "moved")
    need(len(moved) == 4, "%d moved lines, wanted 4" % len(moved), failures)
    for m in moved:
        for key in ("roster", "player"):
            before, _, after = m.get(key, "").partition("->")
            need(before and after and before != after, "moved %s did not change: %s" % (key, m.get(key)), failures)
    return failures


def check_order(*events):
    """The events appear in this order (each the first line of its kind after the previous one)."""
    def check(result):
        lines = result.lines
        at = 0
        for ev in events:
            found = next((i for i in range(at, len(lines)) if lines[i].startswith(ev)), None)
            if found is None:
                return ["no %r after line %d" % (ev, at)]
            at = found + 1
        return []
    return check


def check_reloads(result):
    """The first load and four more, each followed by the world with the main player at a new address."""
    phases = [e.get("_rest", "").split(" ")[0] for e in result.events("FAKEGAME", "phase")]
    players = [e.get("player") for e in result.events("FAKEGAME", "phase") if "player" in e]
    failures = []
    need(phases == ["title"] + ["loading", "world"] * 5, "phases %s" % phases, failures)
    need(len(players) == 5 and len(set(players)) == 5, "main player addresses %s" % players, failures)
    return failures


# The behaviours, each standing in for what a later part meets in a game (README.md).
case("headless", ["--no-window", "--frames", "120", "--fps", "240", "--verbose"],
     expect=[r"^FAKEGAME window none", r"^FAKEGAME fd4-unset name=Rockbound site=124 path_ok=1",
             r"^FAKEGAME ready standin=5 groups=10", r"^FAKEGAME phase title frame=1",
             r"^FAKEGAME phase loading frame=31", r"^FAKEGAME phase world frame=41"],
     check=check_end(frames=120, builtin_runs=480, foreign_tasks=0, rejected=0, loads=1, fd4_unset=1),
     what="runs with no window: the boot order and the program's own four tasks every frame")
case("window", ["--frames", "60", "--fps", "120"],
     expect=[r"^FAKEGAME window hwnd=[0-9A-Fa-fx]*[1-9A-Fa-f][0-9A-Fa-f]* visible=1"],
     check=check_order("FAKEGAME boot", "FAKEGAME window", "FAKEGAME ready", "FAKEGAME phase title", "FAKEGAME end"),
     what="opens a visible top-level window before global_hinstance and the statics (readiness 1, ARCH §3.2)")
case("rate", ["--no-window", "--frames", "90", "--fps", "60"],
     check=check_end(frames=90, frame_ms_mean=(14.0, 24.0)),
     what="keeps the frame rate it is given")
case("hitches", ["--no-window", "--frames", "100", "--fps", "200", "--hitch-every", "25", "--hitch-ms", "120"],
     check=check_end(hitches=4, frame_ms_max=(120.0, 400.0)),
     what="hitches on request: a frame that takes longer")
case("stall", ["--no-window", "--frames", "40", "--fps", "200", "--stall-at", "20", "--stall-ms", "400"],
     expect=[r"^FAKEGAME stall frame=20 ms=400"], check=check_end(frames=40, frame_ms_max=(400.0, 900.0)),
     what="one long stall of the game thread")
case("hang", ["--no-window", "--frames", "1000", "--fps", "100", "--stall-at", "10", "--stall-ms", "0"],
     exit=None, kill_after=4, expect=[r"^FAKEGAME stall frame=10 ms=0"], forbid=[r"^FAKEGAME end"],
     what="a game thread that never comes back, until it is killed from outside")
case("delays", ["--no-window", "--frames", "10", "--boot-delay-ms", "300", "--statics-delay-ms", "300"],
     check=lambda r: [] if r.time_between("window", "ready") >= 250 and r.time_between("boot", "window") >= 250
     else ["delays too short: %s, %s" % (r.time_between("boot", "window"), r.time_between("window", "ready"))],
     what="a slow start: the window late, then the statics late (WaitWindow, WaitEngine)")
case("title-only", ["--no-window", "--frames", "50", "--stay-title"],
     forbid=[r"^FAKEGAME phase loading"], check=check_end(frames=50, loads=0),
     what="stays at the title: main_player null, as at an injection at the title")
case("reload", ["--no-window", "--frames", "200", "--fps", "0", "--title-frames", "5", "--load-frames", "5",
                "--reload-every", "40"],
     check=check_reloads, what="loading screens: actors and the main player gone, then back at new addresses")
case("moves-poison", ["--no-window", "--frames", "200", "--fps", "0", "--title-frames", "5", "--load-frames", "5",
                      "--move-every", "40"],
     check=check_all(check_moves, check_end(moves=4, poisoned=64, freed=0)),
     what="statics that move: every singleton and the main player to a new address, the old one a Tombstone")
case("moves-free", ["--no-window", "--frames", "200", "--fps", "0", "--title-frames", "5", "--load-frames", "5",
                    "--move-every", "40", "--move-mode", "free"],
     check=check_all(check_moves, check_end(moves=4, poisoned=0, freed=64)),
     what="statics that move, the old place released, so a stale read faults")
case("online", ["--no-window", "--frames", "80", "--online-at", "50"],
     expect=[r"^FAKEGAME session online frame=50"], what="the session turns online mid-game (the gate's input)")
case("register", ["--no-window", "--frames", "120", "--selftest-task"],
     expect=[r"^FAKEGAME register accepted group=5 thread=other",
             r"^FAKEGAME task-active group=5 name=Fg_ActorSettle foreign=1"],
     check=check_all(check_end(foreign_tasks=1, foreign_runs=(90, 120), rejected=0),
                     check_selftest(registered=1, faulted=0, attach=1, runs=(90, 120), queries=(1, 2), wrong_thread=0,
                                    wrong_group=0, body_ok=1)),
     what="a task registered from another thread runs on the game thread every frame, in its group")
case("register-swapped", ["--no-window", "--frames", "60", "--selftest-task", "--selftest-abi", "swapped"],
     toolchains=("clang-cl",),
     expect=[r"^FAKEGAME register rejected reason=hub", r"^FAKEGAME selftest register faulted code=0xC0000005"],
     check=check_end(foreign_tasks=0, rejected=1),
     what="a call with the arguments in the wrong order faults in the caller's thread, where SEH catches it")
case("register-swapped-unguarded", ["--no-window", "--frames", "60", "--selftest-task", "--selftest-abi", "swapped"],
     toolchains=("mingw",), exit=AV, expect=[r"^FAKEGAME register rejected reason=hub"], forbid=[r"^FAKEGAME end"],
     what="the same fault with no guard (MinGW has no __try) ends the program")
case("register-fault", ["--no-window", "--frames", "60", "--selftest-task", "--register", "fault"],
     toolchains=("clang-cl",),
     expect=[r"^FAKEGAME register fault-on-request", r"^FAKEGAME selftest register faulted code=0xC0000005"],
     check=check_end(foreign_tasks=0, builtin_runs=240),
     what="registration faults on request, even with good arguments")
case("register-ignore", ["--no-window", "--frames", "60", "--selftest-task", "--register", "ignore"],
     expect=[r"^FAKEGAME register ignored"],
     check=check_all(check_end(foreign_runs=0, ignored=1), check_selftest(registered=1, runs=0, attach=0)),
     what="registration answers but the task never runs (no first tick)")
case("object-short", ["--no-window", "--frames", "60", "--selftest-task", "--selftest-object-size", "0x10"],
     check=check_selftest(registered=1, body_ok=0),
     what="a task object smaller than the hub's fields: the hub overwrites the body pointer")
case("shift-groups", ["--no-window", "--frames", "60", "--selftest-task", "--shift-groups", "2"],
     expect=[r"^FAKEGAME ready standin=7 groups=12", r"^FAKEGAME task-active group=7 name=Fg_ActorSettle"],
     check=check_selftest(wrong_group=0),
     what="the group table shifted, as by a patch: the stand-in group's index moves")
case("group-mismatch", ["--no-window", "--frames", "60", "--selftest-task", "--shift-groups", "1",
                        "--selftest-group", "5"],
     expect=[r"^FAKEGAME task-active group=5 name=Fg_ActorMove foreign=1"],
     what="a task registered at a stale index lands in another group, whose name tells (the first-tick check)")
case("exit-code", ["--no-window", "--frames", "5", "--exit-code", "42"], exit=42, check=check_end(exit=42),
     what="a clean exit with a chosen code")
case("bad-option", ["--no-window", "--bogus"], exit=2, expect=[r"^FAKEGAME error unknown option: --bogus"],
     what="an unknown option is refused")
case("help", ["--help"], expect=[r"^FAKEGAME usage "], what="lists its options")
case("missing-dll", ["--no-window", "--frames", "5", "--load", "boot:C:\\nonexistent\\absent.dll"],
     expect=[r"^FAKEGAME load moment=boot dll=absent\.dll ok=0 error=126"], what="a DLL that cannot load is reported")

CRASHES = [("av-read", AV), ("av-write", AV), ("av-exec", AV), ("div-zero", 0xC0000094), ("illegal", 0xC000001D),
           ("stack-overflow", 0xC00000FD), ("breakpoint", 0x80000003)]
for kind, code in CRASHES:
    case("crash-" + kind, ["--no-window", "--frames", "100", "--crash", kind, "--crash-at", "20", "--game-filter"],
         exit=code, expect=[r"^FAKEGAME crash kind=%s thread=game" % kind, r"^FAKEGAME filter code=0x%08X" % code],
         forbid=[r"^FAKEGAME end"], what="crashes on request; the program's own filter sees the code")
case("crash-fastfail", ["--no-window", "--frames", "100", "--crash", "fastfail", "--crash-at", "20", "--game-filter"],
     exit=0xC0000409, expect=[r"^FAKEGAME crash kind=fastfail"], forbid=[r"^FAKEGAME filter code", r"^FAKEGAME end"],
     what="a fast fail ends the process past every handler and filter")
case("crash-terminate", ["--no-window", "--frames", "100", "--crash", "terminate", "--crash-at", "20"],
     exit=0x46470099, expect=[r"^FAKEGAME crash kind=terminate"], forbid=[r"^FAKEGAME end"],
     what="the program ends itself with TerminateProcess")
case("crash-worker", ["--no-window", "--frames", "100", "--crash", "av-read", "--crash-at", "20", "--crash-thread",
                      "worker", "--game-filter"],
     exit=AV, expect=[r"^FAKEGAME crash kind=av-read thread=worker", r"^FAKEGAME filter code=0xC0000005"],
     what="a crash on a thread that is not the game's")
case("crash-no-filter", ["--no-window", "--frames", "100", "--crash", "av-read", "--crash-at", "20"],
     exit=AV, expect=[r"^FAKEGAME crash kind=av-read"], forbid=[r"^FAKEGAME filter"],
     what="a crash with no filter of the program's own")
case("filter-continue", ["--no-window", "--frames", "100", "--crash", "av-read", "--crash-at", "20", "--game-filter",
                         "--game-filter-result", "continue"],
     exit=AV, expect=[r"^FAKEGAME filter code=0xC0000005"], what="the program's filter passes the crash on")
case("filter-late", ["--no-window", "--frames", "100", "--game-filter-at", "10", "--crash", "av-read", "--crash-at",
                     "20"],
     exit=AV, check=check_order("FAKEGAME filter installed", "FAKEGAME crash kind=av-read",
                                "FAKEGAME filter code=0xC0000005"),
     what="the program installs its filter late, over any other (a game that replaces a crash filter)")
case("handled-seh", ["--no-window", "--frames", "60", "--handled-every", "10"], toolchains=("clang-cl",),
     check=check_end(handled=6, frames=60), what="access violations the program raises and handles itself")
case("handled-cxx", ["--no-window", "--frames", "60", "--handled-every", "10", "--handled-kind", "cxx"],
     check=check_end(handled=6, frames=60), what="C++ exceptions the program throws and catches itself")

VEH = ["--load", "boot:{vehprobe}"]
case("vehprobe-pass", ["--no-window", "--frames", "60", "--handled-every", "10"] + VEH, toolchains=("clang-cl",),
     env={"VEHPROBE_MODE": "pass"},
     expect=[r"^VEHPROBE installed mode=pass first=1 handler=1", r"^FAKEGAME load moment=boot dll=vehprobe\.dll ok=1",
             r"^VEHPROBE saw code=0xC0000005 .* n=6 mode=pass", r"^VEHPROBE detach seen=6 passed=6"],
     check=check_end(handled=6), what="the stand-in crash reporter sees each fault first and passes it on")
case("vehprobe-swallow", ["--no-window", "--frames", "60", "--handled-every", "10"] + VEH, toolchains=("clang-cl",),
     env={"VEHPROBE_MODE": "swallow"}, exit=86,
     expect=[r"^VEHPROBE saw code=0xC0000005 .* n=1 mode=swallow", r"^VEHPROBE swallow code=0xC0000005 exit=86"],
     forbid=[r"^FAKEGAME end"], what="the stand-in ends the process at a fault a frame handler would have caught")
case("vehprobe-ignores-cxx", ["--no-window", "--frames", "60", "--handled-every", "10", "--handled-kind", "cxx"] + VEH,
     env={"VEHPROBE_MODE": "swallow"},
     expect=[r"^VEHPROBE detach seen=0 passed=0 ignored=([6-9]|[1-9][0-9]+)"], check=check_end(handled=6),
     what="C++ exceptions are not a crash reporter's business")
case("vehprobe-crash", ["--no-window", "--frames", "100", "--crash", "av-read", "--crash-at", "20",
                        "--game-filter"] + VEH,
     env={"VEHPROBE_MODE": "pass"}, exit=AV,
     check=check_order("VEHPROBE installed", "FAKEGAME crash kind=av-read", "VEHPROBE saw code=0xC0000005",
                       "FAKEGAME filter code=0xC0000005"),
     what="a real crash: the vectored handler first, then the unhandled-exception filter")
case("vehprobe-last", ["--no-window", "--frames", "20"] + VEH, env={"VEHPROBE_MODE": "pass", "VEHPROBE_FIRST": "0"},
     expect=[r"^VEHPROBE installed mode=pass first=0 handler=1"], what="the handler goes last in the list")
case("vehprobe-off", ["--no-window", "--frames", "20", "--crash", "av-read", "--crash-at", "10"] + VEH,
     env={"VEHPROBE_MODE": "off"}, exit=AV, expect=[r"^VEHPROBE installed mode=off first=1 handler=0"],
     forbid=[r"^VEHPROBE saw"], what="loaded with no handler")
case("vehprobe-late", ["--no-window", "--frames", "60", "--load", "title:{vehprobe}"], env={"VEHPROBE_MODE": "pass"},
     check=check_order("FAKEGAME phase title", "VEHPROBE installed", "FAKEGAME load moment=title dll=vehprobe.dll ok=1"),
     what="loaded late, at the title, as a loader that waits would")
case("status-file", ["--no-window", "--frames", "10", "--status-file", "{status}"],
     check=lambda r: [] if r.status_lines and r.status_lines[-1].startswith("FAKEGAME end frames=10")
     else ["status file: %s" % r.status_lines[-3:]],
     what="the same lines in a file, for a run with no console (started by a launcher on Windows)")
case("describe", ["--describe"], check=None, what="describes every invented fact; the file agrees from outside")

# The derivation's offline round (O89): eldennet-engine probe-exe on fakegame.exe must recover what --describe says. Each
# case runs --describe first, then the tool under Wine (the Windows build R0 runs); cmd_run's probe_exe() does the rest.
case("probe-exe", [], program="{engine}", needs=("engine", "leads"),
     what="probe-exe --derive recovers the fingerprint, the FD4 singletons, the RTTI sets, the registration and the "
          "group, with the decoys refused, and its report lints clean")
case("probe-exe-slots", [], program="{engine}", needs=("engine", "leads"),
     what="given R1's slots, probe-exe cuts a unique interior pattern at a reference to each AOB static")
case("probe-exe-lint", [], program="{engine}", needs=("engine", "leads"),
     what="lint --report refuses a path, a free string and a byte run longer than a pattern")
case("probe-exe-sections", [], program="{engine}", needs=("engine", "leads"),
     what="probe-exe --sections prints the section table's numbers, as the file holds them, in numbers and fixed words "
          "only; a section the loader refuses is named by its rule, index and numbers")
case("probe-exe-loose", [], program="{engine}", needs=("engine", "leads"),
     what="a layout the loader accepts (the last section's raw data past the end of the file, SizeOfImage short of "
          "its alignment) derives as the tidy one")


# The game-access layer in game (part 4): galtest.dll, loaded into the fake game, discovers its facts from gal/ files
# that the harness writes from probe-exe's derive.json (with R1's slots of the AOB statics) and gal-layout.toml.in,
# registers its task once and reads every group; its GALTEST lines are checked against --describe. Each case first
# makes gal/ (make_gal); `{galdir}` names it.
GAL = ["--no-window", "--fps", "60", "--title-frames", "30", "--load-frames", "10"]
GAL_ENV = {"EN_GALTEST_DIR": "{galdir}"}
GAL_NEEDS = ("engine", "leads", "galtest", "gallayout")
case("gal-world", GAL + ["--frames", "900", "--load", "ready:{galtest}"], env=GAL_ENV, needs=GAL_NEEDS,
     toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "world"),
     what="the GAL in game: discovery from gal/ data, one registration, every group read at 4 Hz, the stores, the "
          "probe's slot searches and slot counts, no allocation on the game thread")
case("gal-window", GAL + ["--frames", "900", "--statics-delay-ms", "1500", "--load", "window:{galtest}"], env=GAL_ENV,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "world"),
     what="loaded at the window, before the statics: discovery runs on the image, registration waits for the hub")
case("gal-moves-poison", GAL + ["--frames", "900", "--move-every", "90", "--load", "ready:{galtest}"], env=GAL_ENV,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "moves"),
     what="every singleton and the main player move, the old places tombstones: re-resolved every tick (R5)")
case("gal-moves-free", GAL + ["--frames", "900", "--move-every", "90", "--move-mode", "free", "--load",
                              "ready:{galtest}"], env=GAL_ENV, needs=GAL_NEEDS, toolchains=("clang-cl",),
     timeout=120, check=lambda r: check_gal(r, "moves"),
     what="the same with the old places released: nothing stale is ever read")
case("gal-reload", GAL + ["--frames", "900", "--load-frames", "40", "--reload-every", "240", "--stall-at", "1",
                          "--stall-ms", "1500", "--load", "ready:{galtest}"],
     env=GAL_ENV,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "reload"),
     what="loading screens: the lifecycle subset only while loading (R11), a playthrough each time the world returns; "
          "a first frame of 1.5 s, and a run that ends loading")
case("gal-online", GAL + ["--frames", "720", "--online-at", "420", "--load", "ready:{galtest}"], env=GAL_ENV,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "online"),
     what="the session turns online: Dormant, the session and the lifecycle subset only")
case("gal-fault-camera", GAL + ["--frames", "720", "--load", "ready:{galtest}"],
     env=dict(GAL_ENV, EN_GALTEST_FAULT="camera:1"), needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120,
     check=lambda r: check_gal(r, "fault-camera"),
     what="real faults injected in an optional group: its breaker trips at the third (L3), the rest reads on")
case("gal-fault-world", GAL + ["--frames", "720", "--load", "ready:{galtest}"],
     env=dict(GAL_ENV, EN_GALTEST_FAULT="world:1"), needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120,
     check=lambda r: check_gal(r, "fault-world"),
     what="real faults in a structural group: it trips and the task goes inert (L1), reading nothing more")
case("gal-rpm", GAL + ["--frames", "900", "--load", "ready:{galtest}"],
     env=dict(GAL_ENV, EN_GALTEST_COPY="rpm", EN_GALTEST_FAULT="camera:1"), needs=GAL_NEEDS, toolchains=("clang-cl",),
     timeout=120, check=lambda r: check_gal(r, "rpm"),
     what="the ReadProcessMemory copy as the setting (S0-D): the same reads, and injected faults trip the camera")
# The same DLL with the frame service's own-thread provider (EN_GALTEST_FRAME=own; proposal 0005 §9.4, §10; O101), as
# EldenNet v1 starts: a reader thread of its own steps the task ten times a second, registers nothing with the game and
# calls nothing of it. The same truth is read, and the fake hub never sees a task of ours.
GAL_OWN = dict(GAL_ENV, EN_GALTEST_FRAME="own")
case("gal-own-world", GAL + ["--frames", "900", "--load", "ready:{galtest}"], env=GAL_OWN, needs=GAL_NEEDS,
     toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "world", own=True),
     what="the reader on a thread of our own: no registration and no call into the game, every group read at 10 Hz, "
          "the stores, no allocation on the reader")
case("gal-own-window", GAL + ["--frames", "900", "--statics-delay-ms", "1500", "--load", "window:{galtest}"],
     env=GAL_OWN, needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120,
     check=lambda r: check_gal(r, "world", own=True),
     what="our own thread, loaded at the window before the statics: the reader starts once the game has")
case("gal-own-moves-free", GAL + ["--frames", "900", "--move-every", "90", "--move-mode", "free", "--load",
                                  "ready:{galtest}"], env=GAL_OWN, needs=GAL_NEEDS, toolchains=("clang-cl",),
     timeout=120, check=lambda r: check_gal(r, "moves", own=True),
     what="our own thread while every singleton and the main player move and the old places are released: a reading "
          "out of step with the frame is dropped or its fault left unconfirmed, and nothing stale is published")
case("gal-own-reload", GAL + ["--frames", "900", "--load-frames", "40", "--reload-every", "240", "--stall-at", "1",
                              "--stall-ms", "1500", "--load", "ready:{galtest}"],
     env=GAL_OWN, needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120,
     check=lambda r: check_gal(r, "reload", own=True),
     what="our own thread across loading screens: each change of lifecycle published at its second sighting")
case("gal-own-online", GAL + ["--frames", "720", "--online-at", "420", "--load", "ready:{galtest}"], env=GAL_OWN,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "online", own=True),
     what="our own thread when the session turns online: Dormant")
case("gal-own-fault-camera", GAL + ["--frames", "720", "--load", "ready:{galtest}"],
     env=dict(GAL_OWN, EN_GALTEST_FAULT="camera:1"), needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120,
     check=lambda r: check_gal(r, "fault-camera", own=True),
     what="our own thread with real faults in an optional group: counted once they come twice running, its breaker "
          "trips, the rest reads on")
case("gal-own-fault-world", GAL + ["--frames", "720", "--load", "ready:{galtest}"],
     env=dict(GAL_OWN, EN_GALTEST_FAULT="world:1"), needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120,
     check=lambda r: check_gal(r, "fault-world", own=True),
     what="our own thread with real faults in a structural group: it trips and the reader goes inert")
case("gal-group-shift", GAL + ["--frames", "300", "--shift-groups", "1", "--load", "ready:{galtest}"], env=GAL_ENV,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "group-shift"),
     what="the group table shifted since the derivation: the first tick finds another name there and goes inert")

# The launcher and its crash watcher (part 7): eldennet-launcher starts the fake game (a copy in the case's own folder,
# with the case's own %APPDATA%), and, with galtest.dll as the DLL, injects, watches and records. Its lines are checked
# ("LAUNCHER <event> key=value"), and so are the fake game's, galtest's and vehprobe's (in the status file, as a game
# started by a launcher has no console), and the files it leaves: the appid file and its record, backups, the
# watcher's records, the spikes' logs. `launcher_setup` makes the folders; a case's `prepare` adds its own invented
# files (saves, a stale record, Steam folders). Every name, number and file here is invented, the app id excepted:
# it is the launcher's own constant, which it writes into the fake game's folder.
APPID = "1245620"
# ELDENNET_SPIKE_DIR as an earlier run might have left it in the launcher's own environment: only an observer's run
# (S0-E, S4) gives the game the variable, naming its own folder.
LEFTOVER_SPIKE_DIR = {"ELDENNET_SPIKE_DIR": "C:\\no-such-folder\\spike"}
LBASE = ["--no-dialogs", "--appdata", "{appdata}", "--gal", "{galdir}"]
LGAME = LBASE + ["--no-steam-check", "--game", "{game}", "--no-backup"]
GALTEST_ENV = {"EN_GALTEST_DIR": "{galdir}", "EN_GALTEST_STATUS": "{status}"}
LNEEDS = ("launcher",)
LGAL_NEEDS = ("launcher", "engine", "leads", "galtest", "gallayout")


def game(*extra, frames=180):
    """The fake game's own arguments, after `--`."""
    return ["--", "--frames", str(frames), "--fps", "60", "--status-file", "{status}"] + list(extra)


def lines_of(result, source, event):
    return result.events(source, event)


def want(result, source, event, /, which="last", **kv):
    """One line's values: each key equal to a string, or a number within (lo, hi); `which` first or last."""
    found = [e for e in result.events(source, event) if all(k in e for k in kv)]
    if not found:
        return ["no %s %s line with %s" % (source, event, ", ".join(sorted(kv)))]
    line = found[-1] if which == "last" else found[0]
    failures = []
    for key, value in kv.items():
        got = line.get(key)
        if isinstance(value, tuple):
            ok = got is not None and re.match(r"^-?[0-9.]+$", got) and value[0] <= float(got) <= value[1]
            need(ok, "%s %s %s=%s, wanted %s..%s" % (source, event, key, got, value[0], value[1]), failures)
        else:
            need(got == str(value), "%s %s %s=%s, wanted %s" % (source, event, key, got, value), failures)
    return failures


def no_appid_file(result):
    path = os.path.join(result.paths["gamedir"], "steam_appid.txt")
    return ["steam_appid.txt is still in the game folder"] if os.path.exists(path) else []


def crash_records(result):
    folder = os.path.join(result.paths["appdata"], "EldenNet", "crashes")
    if not os.path.isdir(folder):
        return []
    out = []
    for name in sorted(os.listdir(folder)):
        with open(os.path.join(folder, name), encoding="utf-8") as f:
            out.append(json.load(f))
    return out


def record_is_clean(rec):
    """A record holds numbers, fixed words and module file names only: no path, no user name."""
    text = json.dumps(rec)
    failures = []
    need(":\\\\" not in text and "/" not in text.replace("\\/", ""), "a record holds a path: %s" % text[:200], failures)
    need(os.environ.get("USER", "\0") not in text, "a record holds the user's name", failures)
    return failures


def check_lines(*pairs, **files):
    """Checks lines as (source, event, {key: value}) triples, then each `files` check (a function of the result)."""
    def check(result):
        failures = []
        for source, event, kv in pairs:
            failures += want(result, source, event, **kv)
        for f in files.values():
            failures += f(result)
        return failures
    return check


def with_saves(paths):
    # Two invented save folders (17 digits) and one that is no SteamID; the files' contents invented.
    saves = os.path.join(paths["appdata"], "EldenRing")
    for sid, body in (("76500000000000042", b"invented save A"), ("76500000000000077", b"invented save B")):
        os.makedirs(os.path.join(saves, sid))
        with open(os.path.join(saves, sid, "ER0000.sl2"), "wb") as f:
            f.write(body * 100)
    with open(os.path.join(saves, "76500000000000042", "ER0000.sl2.bak"), "wb") as f:
        f.write(b"invented backup")
    os.makedirs(os.path.join(saves, "not-a-steam-id"))


def with_stale_record(paths):
    # A steam_appid.txt this launcher made in an earlier session and never removed (its record says so).
    target = os.path.join(paths["gamedir"], "steam_appid.txt")
    with open(target, "w") as f:
        f.write(APPID)
    cfg = os.path.join(paths["appdata"], "EldenNet", "config")
    os.makedirs(cfg)
    with open(os.path.join(cfg, "launcher-state.toml"), "w") as f:
        f.write('schema = 1\n[appid]\nfile = "%s"\ncreated_by_us = true\ncreated_at = "20261116-101500"\n'
                % paths["as_windows"](target).replace("\\", "\\\\"))


def with_foreign_appid(paths):
    with open(os.path.join(paths["gamedir"], "steam_appid.txt"), "w") as f:
        f.write("4242")  # someone else's file: never ours to touch


def with_old_crash(paths):
    # The watcher's record of a crash in an earlier session, not explained yet: the next start explains it.
    folder = os.path.join(paths["appdata"], "EldenNet", "crashes")
    os.makedirs(folder)
    with open(os.path.join(folder, "crash-20261116-090000-77.json"), "w") as f:
        json.dump({"record": 1, "kind": "crash", "exit_code": 3221225477, "crash": {"module": "fakegame.exe"},
                   "explained": False}, f)


def with_steam(paths):
    # An invented Steam folder: libraryfolders.vdf naming a second library, which holds the app's manifest and, at
    # the path the launcher forms, a copy of the fake game standing in for the game's executable.
    steam = os.path.join(paths["case"], "Steam")
    lib = os.path.join(paths["case"], "Library Two")
    os.makedirs(os.path.join(steam, "steamapps"))
    os.makedirs(os.path.join(lib, "steamapps", "common", "Invented Install", "Game"))
    esc = lambda p: paths["as_windows"](p).replace("\\", "\\\\")
    with open(os.path.join(steam, "steamapps", "libraryfolders.vdf"), "w") as f:
        f.write('"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n\t"1"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n}\n'
                % (esc(steam), esc(lib)))
    with open(os.path.join(lib, "steamapps", "appmanifest_%s.acf" % APPID), "w") as f:
        f.write('"AppState"\n{\n\t"appid"\t\t"%s"\n\t"installdir"\t\t"Invented Install"\n}\n' % APPID)
    shutil.copy2(paths["game"], os.path.join(lib, "steamapps", "common", "Invented Install", "Game", "eldenring.exe"))
    paths["steam"] = steam


def check_spike_report_s0a(result):
    """spike-report s0a over the launcher's own log (--need 3 for the case's three runs): each run's mode is there,
    the early rules pass, and the late load, not run, leaves the verdict incomplete."""
    failures = []
    folder = os.path.join(result.paths["appdata"], "EldenNet", "spikes")
    logs = [os.path.join(folder, n) for n in sorted(os.listdir(folder)) if n.startswith("s0a-")]
    out = os.path.join(result.paths["case"], "s0a-report.json")
    code, text, _ = run_engine(result.args, result.env, ["spike-report", "s0a", "--need", "3", "--log",
                                                         result.as_win(logs[-1]), "--out", result.as_win(out)])
    need(code == 1 and "verdict=incomplete" in text, "spike-report s0a: exit %s %s" % (code, text[-300:]), failures)
    if os.path.exists(out):
        with open(out) as f:
            rep = json.load(f)
        rules = {(x["rule"], x.get("mode")): x["status"] for x in rep["rules"]}
        need(rules.get(("runs_per_mode", "suspended")) == "pass" and rules.get(("ready_or_l1", "suspended")) == "pass"
             and rules.get(("both_modes", None)) == "not_measured", "spike-report s0a's rules: %s" % rules, failures)
    return failures


def spike_log(result, spike):
    """The spike's newest log (`<word>-<stamp>.jsonl`; an observer's run's folder, `<word>-<n>`, is no log)."""
    folder = os.path.join(result.paths["appdata"], "EldenNet", "spikes")
    names = [n for n in sorted(os.listdir(folder)) if n.startswith(spike + "-") and n.endswith(".jsonl")
             and os.path.isfile(os.path.join(folder, n))] if os.path.isdir(folder) else []
    if not names:
        return None
    with open(os.path.join(folder, names[-1]), encoding="utf-8") as f:
        return [json.loads(l) for l in f if l.strip()]


def check_vanilla(result):
    failures = check_lines(("LAUNCHER", "started", {"ok": 1, "mode": "vanilla", "inject": 0, "eldennet": 0}),
                           ("FAKEGAME", "spike_dir", {"set": 0, "exists": 0}),
                           ("LAUNCHER", "appid", {"removal": "removed", "at": "window"}),
                           ("LAUNCHER", "end", {"kind": "clean", "exit_code": "0x00000000", "crash_filter": 0}),
                           ("LAUNCHER", "run", {"outcome": "game_ended", "record": 0}),
                           ("LAUNCHER", "last_crash", {"explained": 1}),
                           ("FAKEGAME", "end", {"frames": 180}))(result)
    failures += want(result, "LAUNCHER", "appid", which="first", method="file", step="created")
    failures += want(result, "LAUNCHER", "end", modules=(3, 500))
    failures += no_appid_file(result)
    recs = crash_records(result)
    need(len(recs) == 1 and recs[0]["explained"] is True, "the old record is not marked explained: %s" % recs, failures)
    state = os.path.join(result.paths["appdata"], "EldenNet", "config", "launcher-state.toml")
    need(os.path.exists(state) and "[appid]" not in open(state).read(), "launcher-state.toml still records the file",
         failures)
    return failures


def check_vanilla_crash(result):
    failures = check_lines(("LAUNCHER", "end", {"kind": "crash", "exit_code": "0xC0000005", "crash_filter": 0}),
                           ("LAUNCHER", "record", {"kept": 1, "kind": "crash"}),
                           ("FAKEGAME", "crash", {"kind": "av-read"}))(result)
    recs = crash_records(result)
    need(len(recs) == 1, "%d crash records, wanted 1" % len(recs), failures)
    if recs:
        r = recs[0]
        need(r["kind"] == "crash" and r["exit_code_hex"] == "0xC0000005" and r["crash"] is None,
             "the record: %s" % r, failures)
        need("fakegame.exe" in r["modules"] and r["mode"] == "vanilla", "the record's modules: %s" % r["modules"],
             failures)
        need(r["explained"] is False, "a new record is not yet explained", failures)
        failures += record_is_clean(r)
    return failures


def check_backup_only(result):
    failures = check_lines(("LAUNCHER", "backup", {"folders": 2, "made": 2, "unchanged": 0, "problems": 0}),
                           ("LAUNCHER", "finish", {"exit": 0}))(result)
    root = os.path.join(result.paths["appdata"], "EldenNet", "save-backups")
    for sid, files in (("76500000000000042", {"ER0000.sl2", "ER0000.sl2.bak", "manifest.toml"}),
                       ("76500000000000077", {"ER0000.sl2", "manifest.toml"})):
        stamps = os.listdir(os.path.join(root, sid)) if os.path.isdir(os.path.join(root, sid)) else []
        need(len(stamps) == 1, "%s: %d backups" % (sid, len(stamps)), failures)
        if stamps:
            got = set(os.listdir(os.path.join(root, sid, stamps[0])))
            need(got == files, "%s: %s" % (sid, sorted(got)), failures)
            man = open(os.path.join(root, sid, stamps[0], "manifest.toml")).read()
            need('name = "ER0000.sl2"' in man and "blake3 = " in man, "manifest: %s" % man[:200], failures)
    need(not os.path.exists(os.path.join(root, "not-a-steam-id")), "a folder that is no SteamID was backed up",
         failures)
    return failures


# The launcher on the fake game, with no DLL (both builds).
case("launcher-vanilla", LGAME + ["--vanilla-offline"] + game(), program="{launcher}", needs=LNEEDS,
     prepare=with_old_crash, check=check_vanilla, timeout=120, env=LEFTOVER_SPIKE_DIR,
     what="starts the fake game offline with no DLL: the appid file made, removed once the window shows, a clean "
          "end watched; the last session's crash record explained and marked so; the launcher's own "
          "ELDENNET_SPIKE_DIR not passed on")
case("launcher-vanilla-crash", LGAME + ["--vanilla-offline"] + game("--crash", "av-read", "--crash-at", "60"),
     program="{launcher}", needs=LNEEDS, check=check_vanilla_crash, timeout=120,
     what="a crash with no EldenNet in the game: the watcher keeps a record from the exit code and the module list")
case("launcher-env", LGAME + ["--vanilla-offline", "--appid-method", "env"] + game(), program="{launcher}",
     needs=LNEEDS, timeout=120, env=LEFTOVER_SPIKE_DIR,
     check=check_lines(("LAUNCHER", "appid", {"method": "env"}), ("LAUNCHER", "end", {"kind": "clean"}),
                       ("FAKEGAME", "spike_dir", {"set": 0, "exists": 0}), files=no_appid_file),
     what="the SteamAppId method (pending S0-C): no file written in the game's folder")
case("launcher-foreign-appid", LGAME + ["--vanilla-offline"] + game(), program="{launcher}", needs=LNEEDS,
     prepare=with_foreign_appid, timeout=120,
     check=lambda r: want(r, "LAUNCHER", "appid", step="foreign") + want(r, "LAUNCHER", "end", kind="clean") +
     ([] if open(os.path.join(r.paths["gamedir"], "steam_appid.txt")).read() == "4242"
      else ["someone else's steam_appid.txt was changed"]),
     what="a steam_appid.txt that is not the launcher's is left as it is; that start uses the environment")
case("launcher-backup-only", LBASE + ["--backup-only"], program="{launcher}", needs=LNEEDS, prepare=with_saves,
     check=check_backup_only, timeout=60,
     what="--backup-only: each SteamID folder's save files copied with a BLAKE3 manifest; nothing else touched")
case("launcher-cleanup", LGAME[:-1] + ["--cleanup"], program="{launcher}", needs=LNEEDS, prepare=with_stale_record,
     timeout=60,
     check=check_lines(("LAUNCHER", "appid", {"stale": 1, "removal": "removed"}),
                       ("LAUNCHER", "cleanup", {"appid": "done"}), files=no_appid_file),
     what="--cleanup removes a stale steam_appid.txt the launcher's record says it made")
case("launcher-diagnostics", LBASE + ["--steam", "{steam}", "--print-diagnostics"], program="{launcher}",
     needs=LNEEDS, prepare=with_steam, timeout=60,
     check=check_lines(("LAUNCHER", "diagnostics", {"how": "steam", "found": 1, "libraries": 2, "manifests": 1}),
                       ("LAUNCHER", "gate", {"result": "passed", "build": "3.14.15.0-ww"})),
     what="--print-diagnostics through invented Steam folders: the library list, the app manifest, the gate")
case("launcher-e02", LBASE + ["--game", "{game}", "--no-backup", "--vanilla-offline"], program="{launcher}",
     needs=LNEEDS, exit=12, timeout=60, check=check_lines(("LAUNCHER", "error", {"code": "E02", "exit": 12})),
     what="no Steam running: E02, nothing started")
def with_unknown_build(paths):
    # builds.toml naming the fake game's version with another TimeDateStamp: a build EldenNet does not know.
    with open(os.path.join(paths["galdir"], "builds.toml")) as f:
        text = re.sub(r"time_date_stamp = 0x([0-9A-F]+)",
                      lambda m: "time_date_stamp = 0x%X" % ((int(m.group(1), 16) + 1) & 0xFFFFFFFF), f.read())
    paths["galdir"] = os.path.join(paths["case"], "gal-unknown")
    os.makedirs(paths["galdir"])
    with open(os.path.join(paths["galdir"], "builds.toml"), "w") as f:
        f.write(text)


case("launcher-e05", LGAME + ["--dll", "{vehprobe}"] + game(), program="{launcher}", needs=LNEEDS, exit=15,
     timeout=60, prepare=with_unknown_build,
     check=check_lines(("LAUNCHER", "gate", {"result": "unknown"}),
                       ("LAUNCHER", "error", {"code": "E05", "answer": "no"})),
     what="a build builds.toml does not know: E05, answered no, nothing started")
case("launcher-e08", LGAME + ["--dll", "{vehprobe}"] + game(), program="{launcher}", needs=LNEEDS, exit=18,
     timeout=60, check=check_lines(("LAUNCHER", "preflight", {"dll_ours": 0}), ("LAUNCHER", "error", {"code": "E08"})),
     what="a DLL whose ProductName is not EldenNet: E08 before anything starts")
case("launcher-usage", ["--inject-at", "later"], program="{launcher}", needs=LNEEDS, exit=2, timeout=30,
     expect=[r'^LAUNCHER usage error="--inject-at is suspended, window or title"'], what="a usage error: exit 2")


def check_inject(where):
    def check(result):
        failures = check_lines(("LAUNCHER", "inject", {"dll": "galtest.dll", "ok": 1}),
                               ("LAUNCHER", "init", {"state": "ready"}),
                               ("LAUNCHER", "end", {"kind": "clean", "crash_filter": 0}),
                               ("LAUNCHER", "run", {"outcome": "ready"}),
                               ("GALTEST", "watch", {"attached": 1}),
                               ("GALTEST", "registered", {"ok": 1}),
                               ("LAUNCHER", "mark", {"n": 1}),
                               ("GALTEST", "watch", {"marks": 1}),
                               ("FAKEGAME", "pressed", {"key": "pause", "sent": 2}))(result)
        failures += want(result, "LAUNCHER", "mark", which="first", hotkey="pause", registered=1)
        failures += want(result, "LAUNCHER", "status", init="ready", snapshots=(8, 1e9), tripped="0x0")
        if where == "world":
            # en-core's slice in galtest (part 5): the world reached, and the gate open after its 5 s hold.
            failures += want(result, "GALTEST", "core", phase="in_world", level="L2", gate=1, dormant=0, badge=0,
                             why="none", playthroughs=1)
            failures += want(result, "LAUNCHER", "status", flags="0x9")  # the core's, the gate open, not dormant
        else:
            failures += want(result, "GALTEST", "core", phase="title", level="L2", gate=1, dormant=1)
        return failures + no_appid_file(result)
    return check


# The launcher with galtest.dll as the DLL (clang-cl): injected suspended, at the window or at the title; watched;
# the dev console's status lines; the Mark pressed in game (the fake game sends Pause) and seen in galtest's block.
case("launcher-inject", LGAME + ["--dll", "{galtest}", "--console"] + game("--press-pause-at", "300", frames=720),
     program="{launcher}", needs=LGAL_NEEDS, toolchains=("clang-cl",), env=GALTEST_ENV, timeout=180,
     check=check_inject("world"),
     what="injected into the suspended game: T1 to Ready, the world, the gate open after 5 s (en-core), the dev "
          "console's status, a Mark on Pause stamped into the block, a clean end")
case("launcher-inject-window", LGAME + ["--dll", "{galtest}", "--console", "--inject-at", "window"] +
     game("--press-pause-at", "300", frames=720), program="{launcher}", needs=LGAL_NEEDS, toolchains=("clang-cl",),
     env=GALTEST_ENV, timeout=180, check=check_inject("world"),
     what="injected once the window shows (a late load)")
case("launcher-inject-title", LGAME + ["--dll", "{galtest}", "--console", "--inject-at", "title", "--title-wait", "1"] +
     game("--stay-title", "--press-pause-at", "300", frames=720), program="{launcher}", needs=LGAL_NEEDS,
     toolchains=("clang-cl",), env=GALTEST_ENV, timeout=180, check=check_inject("title"),
     what="injected at the title (a late load): Dormant there, the title clause's gate open")


def check_crash(thread, filter_ran=True):
    def check(result):
        failures = want(result, "LAUNCHER", "end", kind="crash", exit_code="0xC0000005",
                        crash_filter=1 if filter_ran else 0, at_crash=1 if filter_ran else 0)
        failures += want(result, "LAUNCHER", "record", kept=1, kind="crash")
        if filter_ran:
            failures += want(result, "LAUNCHER", "crash", code="0xC0000005", thread=thread)
        else:
            need(not result.events("LAUNCHER", "crash"), "the crash filter ran, though the game replaced it", failures)
        recs = crash_records(result)
        need(len(recs) == 1, "%d crash records" % len(recs), failures)
        if recs:
            r = recs[0]
            if filter_ran:
                need(r["crash"] is not None and r["crash"]["thread"] == thread and r["modules_at_crash"],
                     "the record's crash: %s" % r.get("crash"), failures)
                need(r["crash"] is not None and r["crash"]["module"].lower() in ("fakegame.exe", "kernelbase.dll",
                                                                                 "ntdll.dll", "galtest.dll"),
                     "the record's module: %s" % (r["crash"] or {}).get("module"), failures)
            else:
                need(r["crash"] is None, "a crash record without the filter: %s" % r.get("crash"), failures)
            need("galtest.dll" in r["modules"] and r["status"]["init"] == "ready", "the record: %s" % r, failures)
            failures += record_is_clean(r)
        return failures
    return check


case("launcher-crash", LGAME + ["--dll", "{galtest}"] + game("--crash", "av-read", "--crash-at", "400", frames=720),
     program="{launcher}", needs=LGAL_NEEDS, toolchains=("clang-cl",), env=GALTEST_ENV, timeout=180,
     check=check_crash("game"),
     what="the game crashes on its game thread: EldenNet's filter writes the record, the watcher lists the modules "
          "while it waits, and keeps a scrubbed record (no dump)")
case("launcher-crash-filter-replaced", LGAME + ["--dll", "{galtest}"] +
     game("--game-filter-at", "300", "--crash", "av-read", "--crash-at", "400", frames=720), program="{launcher}",
     needs=LGAL_NEEDS, toolchains=("clang-cl",), env=GALTEST_ENV, timeout=180, check=check_crash("game", False),
     what="the game replaces EldenNet's crash filter (S0-D): the exit code and the module list remain")
case("launcher-also-vehprobe", LGAME + ["--also", "{vehprobe}", "--dll", "{galtest}"] +
     game("--crash", "av-read", "--crash-at", "400", frames=720), program="{launcher}", needs=LGAL_NEEDS,
     toolchains=("clang-cl",), env=dict(GALTEST_ENV, VEHPROBE_MODE="pass", VEHPROBE_LOG="{status}"), timeout=180,
     check=lambda r: (check_order("LAUNCHER inject dll=vehprobe.dll ok=1", "LAUNCHER inject dll=galtest.dll ok=1")(r) +
                      want(r, "VEHPROBE", "saw", code="0xC0000005") + check_crash("game")(r)),
     what="--also loads a stand-in crash reporter first; its vectored handler sees the fault, then EldenNet's filter")


def check_spike(spike, runs, **tally):
    def check(result):
        failures = want(result, "LAUNCHER", "tally", spike=spike, runs=runs, passed=1, **tally)
        log = spike_log(result, spike)
        need(log is not None and len(log) == runs + 1, "the spike's log: %s" % log, failures)
        if log:
            need(log[-1].get("tally") == 1 and log[-1].get("passed") is True, "the log's tally: %s" % log[-1], failures)
            need(all(l["spike"] == spike for l in log), "a line of another spike", failures)
        return failures + no_appid_file(result)
    return check


# The S0 spikes' loops (clang-cl, with galtest): S0-A's runs to the title, S0-D's crashes on request, S0-C's session.
case("launcher-spike-s0a", LGAME + ["--dll", "{galtest}", "--spike", "s0a", "--runs", "3", "--run-timeout", "60"] +
     ["--", "--forever", "--stay-title", "--fps", "60", "--status-file", "{status}"], program="{launcher}",
     needs=LGAL_NEEDS, toolchains=("clang-cl",), env=GALTEST_ENV, timeout=240,
     check=lambda r: check_spike("s0a", 3, ready=3, crashes=0, timeout=0)(r) + check_spike_report_s0a(r),
     what="--spike s0a --runs 3: each run starts, injects, reaches Ready at the title, is closed; the tally passes; "
          "spike-report reads the log: three early runs pass their rules, the late load not run yet")
case("launcher-spike-s0d-crash", LGAME + ["--dll", "{galtest}", "--spike", "s0d-crash", "--runs", "2",
                                          "--run-timeout", "60"] +
     ["--", "--forever", "--stay-title", "--fps", "60", "--status-file", "{status}"], program="{launcher}",
     needs=LGAL_NEEDS, toolchains=("clang-cl",), env=GALTEST_ENV, timeout=240,
     check=lambda r: check_spike("s0d-crash", 2, crashes=2, records=2, filter_records=2)(r) +
     want(r, "LAUNCHER", "crash", thread="init"),
     what="--spike s0d-crash --runs 2: a crash asked for at the title each run, every one recorded by the watcher")
case("launcher-spike-s0c", LGAME + ["--dll", "{galtest}", "--spike", "s0c"] + game(frames=300), program="{launcher}",
     needs=LGAL_NEEDS, toolchains=("clang-cl",), env=dict(GALTEST_ENV, **LEFTOVER_SPIKE_DIR), timeout=180,
     check=lambda r: (check_spike("s0c", 1, crashes=0)(r) + want(r, "FAKEGAME", "spike_dir", set=0, exists=0) +
                      (["an S0-C run made a spike folder"] if r.events("LAUNCHER", "spike_dir") else [])),
     what="--spike s0c: one watched session, the game's connections counted (none), the tally kept")
case("launcher-watch", GAL + ["--frames", "720", "--load", "ready:{galtest}", "--crash", "av-read", "--crash-at", "400"],
     needs=LGAL_NEEDS, toolchains=("clang-cl",), exit=AV, settle=True, timeout=180,
     env=dict(GALTEST_ENV, EN_GALTEST_WATCHER="{launcher}",
              EN_GALTEST_WATCHER_ARGS="--no-dialogs --appdata {appdata}"),
     check=lambda r: (want(r, "GALTEST", "watcher", started=1) + want(r, "GALTEST", "watch", which="first", attached=1) +
                      want(r, "LAUNCHER", "watch", ok=1, block_created=1) +
                      want(r, "LAUNCHER", "crash", code="0xC0000005", thread="game") +
                      want(r, "LAUNCHER", "end", kind="crash", crash_filter=1) +
                      want(r, "LAUNCHER", "record", kept=1)),
     what="loaded by another loader (the fake game's own --load): EldenNet starts the watcher (--watch <pid>), which "
          "makes the block, sees the crash and keeps its record")


# --- EldenNet's DLL (part 6): eldennet.dll, injected by the launcher into the fake game -----------------------------------
# Each case installs the DLL as a player would (`{case}/eldennet/eldennet.dll` with gal/ beside it: the derivation's for
# the fake game, as make_gal writes it), with an %APPDATA% of the case's own (ELDENNET_APPDATA, the launcher's
# --appdata), where its settings.toml, its log, its recordings and R1's reports live. Its log lines (`ELDENNET <event>
# key=value ... t=<ms>`) join the case's lines.
EN_NEEDS = ("launcher", "engine", "leads", "gallayout", "eldennet")
EN_PROBE_NEEDS = EN_NEEDS + ("eldennet_probe",)
EN_ENV = {"ELDENNET_APPDATA": "{appdata}"}


def install_eldennet(settings="", probe=False, gal_change=None, copies=(), leads_base=None, then=None):
    """A case's prepare: the DLL and its gal/ in {case}/eldennet, settings.toml in {appdata} (none when `settings` is
    None, as on a PC that never had one), `copies` of files (name, source key) beside the game for --also and --load,
    and `then`, another prepare, last."""
    def prepare(paths):
        folder = os.path.join(paths["case"], "eldennet")
        os.makedirs(folder)
        dll = os.path.join(folder, "eldennet.dll")
        shutil.copy2(paths["eldennet_probe" if probe else "eldennet"], dll)
        shutil.copytree(paths["galdir"], os.path.join(folder, "gal"))
        with open(os.path.join(paths["work"], "gal-leads.toml")) as f:
            leads = f.read()
        if leads_base is not None:
            leads = re.sub(r'(?m)^base = ".*"$', 'base = "%s"' % leads_base, leads)
        with open(os.path.join(folder, "gal", "leads.toml"), "w") as f:
            f.write(leads)
        if gal_change is not None:
            gal_change(os.path.join(folder, "gal"))
        cfg = os.path.join(paths["appdata"], "EldenNet", "config")
        os.makedirs(cfg)
        if settings is not None:
            with open(os.path.join(cfg, "settings.toml"), "w") as f:
                f.write(settings)
        paths["eldennetdll"] = dll
        paths["endir"] = folder
        for name, key in copies:
            target = os.path.join(paths["case"], name)
            shutil.copy2(paths[key], target)
            paths[name.split(".")[0].replace("-", "_").lower()] = target
        if then is not None:
            then(paths)
    return prepare


def unknown_build_for_dll(gal):
    # builds.toml naming the fake game with another TimeDateStamp, in the DLL's gal/ alone: the launcher's gate passes,
    # the DLL's refuses.
    path = os.path.join(gal, "builds.toml")
    with open(path) as f:
        text = re.sub(r"time_date_stamp = 0x([0-9A-F]+)",
                      lambda m: "time_date_stamp = 0x%X" % ((int(m.group(1), 16) + 1) & 0xFFFFFFFF), f.read())
    with open(path, "w") as f:
        f.write(text)


def any_line(result, source, event, **kv):
    """Some line of the event holds every key=value given (where `want` reads the last line that has the keys)."""
    for e in result.events(source, event):
        if all(e.get(k) == str(v) for k, v in kv.items()):
            return []
    return ["no %s %s line with %s" % (source, event, " ".join("%s=%s" % i for i in sorted(kv.items())))]


def en_log_is_clean(result):
    """The DLL's log holds numbers and words only: no path, no user name."""
    failures = []
    lines = [l for l in result.lines if l.startswith("ELDENNET ")]
    need(lines, "no ELDENNET line", failures)
    text = "\n".join(lines)
    need(":\\" not in text and "/" not in text and "\\" not in text, "the DLL's log holds a path", failures)
    need(os.environ.get("USER", "\0") not in text, "the DLL's log holds the user's name", failures)
    need("redacted" not in text, "the DLL's log redacted something it was given", failures)
    need("Wanderer" not in text, "the DLL's log holds the character's name", failures)
    return failures


def en_recordings(result):
    folder = os.path.join(result.paths["appdata"], "EldenNet", "recordings")
    return sorted(os.path.join(folder, n) for n in os.listdir(folder)) if os.path.isdir(folder) else []


def check_recordings(min_snaps):
    def check(result):
        failures = []
        files = en_recordings(result)
        need(len(files) == 1, "%d recordings" % len(files), failures)
        for path in files:
            code, out, _ = run_engine(result.args, result.env, ["lint", "--recording", result.as_win(path)])
            need(code == 0 and "the recording is clean" in out, "lint --recording: exit %s %s" % (code, out[-400:]),
                 failures)
            with open(path, encoding="utf-8") as f:
                records = [json.loads(l) for l in f if l.strip()]
            kinds = [r["k"] for r in records]
            need(kinds and kinds[0] == "header", "the first record is %s" % (kinds[:1],), failures)
            need(kinds.count("snap") >= min_snaps, "%d snapshots recorded" % kinds.count("snap"), failures)
            need("life" in kinds and "change" in kinds and "mark" in kinds, "kinds recorded: %s" % sorted(set(kinds)),
                 failures)
            lives = [r.get("life_kind") for r in records if r["k"] == "life"]
            need("data_ready" in lives, "the lifecycle records: %s" % lives, failures)
            ts = [r["t"] for r in records if r["k"] == "snap"]
            need(ts == sorted(ts), "the snapshots' times go back", failures)
            with open(path, encoding="utf-8") as f:
                text = f.read()
            need("Wanderer" not in text and "Test W" not in text, "the character's name was recorded", failures)
            need(not any(w in text for w in ("Ash Warden", "Quiet Vale", "Lantern")), "game text was recorded",
                 failures)
            # The part 9 tools on the real recording: rec-stats counts what the DLL wrote, and replay steps its inputs
            # through en-core again and answers the changes the DLL's core answered.
            stats_out, replay_out = path + ".stats.json", path + ".replay.json"
            code, _, err = run_engine(result.args, result.env, ["rec-stats", result.as_win(path), "--out",
                                                                 result.as_win(stats_out)])
            need(code == 0 and os.path.exists(stats_out), "rec-stats: exit %s %s" % (code, err[-300:]), failures)
            if code == 0 and os.path.exists(stats_out):
                with open(stats_out) as f:
                    st = json.load(f)
                need(st["snapshots"]["count"] == kinds.count("snap") and st["marks"] == kinds.count("mark") and
                     st["life"]["data_ready"] == lives.count("data_ready"), "rec-stats' counts: %s" %
                     {k: st[k] for k in ("snapshots", "marks", "life")}, failures)
            code, _, err = run_engine(result.args, result.env, ["replay", result.as_win(path), "--out",
                                                                 result.as_win(replay_out)])
            agree = False
            if os.path.exists(replay_out):
                with open(replay_out) as f:
                    rp = json.load(f)
                agree = rp["agree"] and rp["recorded"] == rp["replayed"] > 0
            need(code == 0 and agree, "replay: exit %s %s" % (code, err[-300:]), failures)
        return failures
    return check


def ready_no_call(r):
    """A start that reads on EldenNet's own thread: the reader's states and none of the game task's, and the fake
    game's registration never reached from outside (the hub's live tasks are the program's own four)."""
    states = [e.get("state") for e in r.events("ELDENNET", "init")]
    failures = []
    need("reader" in states and "first_reading" in states and "register" not in states and "first_tick" not in states,
         "T1's states: %s" % states, failures)
    need(not r.events("ELDENNET", "register") and not r.events("ELDENNET", "first_tick"),
         "the DLL registered a task with the game", failures)
    if r.events("FAKEGAME", "end"):
        failures += want(r, "FAKEGAME", "end", foreign_tasks=0, live=4, rejected=0, ignored=0)
    if r.events("FAKEGAME", "register"):
        failures.append("the fake game's registration was called")
    return failures


def check_en_ready(where):
    def check(result):
        failures = check_lines(("LAUNCHER", "inject", {"dll": "eldennet.dll", "ok": 1}),
                               ("LAUNCHER", "init", {"state": "ready"}),
                               ("LAUNCHER", "end", {"kind": "clean", "crash_filter": 0}),
                               ("ELDENNET", "safety", {"first_copy": 1, "anticheat": 0, "watched": 1, "logging": 1}),
                               ("ELDENNET", "gate", {"result": "passed"}),
                               ("ELDENNET", "discovery", {"ready": 1, "getter_calls": 0}),
                               ("ELDENNET", "reader", {"started": 1}),
                               ("ELDENNET", "first_reading", {"state": "running"}),
                               ("ELDENNET", "init", {"state": "ready"}),
                               ("ELDENNET", "connect", {"ok": 0, "outcome": "absent", "installed": 0, "looks": 1}))(
                                   result)
        failures += want(result, "LAUNCHER", "status", init="ready", snapshots=(8, 1e9), tripped="0x0")
        # The reading runs on a thread of EldenNet's own (O101): nothing was registered with the game and nothing of it
        # was called (the hub's live tasks are the program's own; ready_no_call below), and T1 never took the game
        # task's states.
        failures += ready_no_call(result)
        phases = [e.get("to") for e in result.events("ELDENNET", "change") if e.get("what") == "phase"]
        if where == "world":
            need("in_world" in phases, "the core's phases: %s" % phases, failures)
            need(any(e.get("what") == "gate_opened" for e in result.events("ELDENNET", "change")),
                 "the gate never opened", failures)
            failures += want(result, "LAUNCHER", "status", flags="0x9")  # the core's, the gate open, not dormant
        else:
            need("title" in phases and "in_world" not in phases, "the core's phases: %s" % phases, failures)
        return failures + en_log_is_clean(result) + no_appid_file(result)
    return check


EN_RECORD = 'schema = 2\ndebug = { record_sessions = true }\n'

case("launcher-eldennet", LGAME + ["--dll", "{eldennetdll}", "--console"] + game("--press-pause-at", "300", frames=900),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(EN_RECORD),
     check=lambda r: (check_en_ready("world")(r) + check_recordings(20)(r) +
                      want(r, "ELDENNET", "mark", n=1) + want(r, "LAUNCHER", "mark", n=1) +
                      want(r, "ELDENNET", "recording", which="first", open=1)),
     what="eldennet.dll injected into the suspended game: T1 from Pin to Ready, the core in the world with the gate "
          "open, the dev console's status, a Mark recorded, a numbers-only recording that lints clean, a clean exit")
case("launcher-eldennet-late", LGAME + ["--dll", "{eldennetdll}", "--console", "--inject-at", "title",
                                        "--title-wait", "1"] + game("--stay-title", frames=600),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(), check=check_en_ready("title"),
     what="injected at the title (a late load): the window there at once, Ready, the core at the title, Dormant")
case("launcher-eldennet-online", LGAME + ["--dll", "{eldennetdll}"] + game("--online-at", "600", frames=900),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(),
     check=lambda r: (any_line(r, "ELDENNET", "change", what="gate_opened") +
                      any_line(r, "ELDENNET", "change", what="gate_closed", to="not_solo") +
                      any_line(r, "ELDENNET", "change", what="badge", to="up") +
                      want(r, "LAUNCHER", "end", kind="clean")),
     what="the gate: open in a solo world, closed with the badge up when the session turns online")
case("launcher-eldennet-gate", LGAME + ["--dll", "{eldennetdll}", "--console"] + game(frames=420),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(gal_change=unknown_build_for_dll),
     check=lambda r: (check_lines(("ELDENNET", "gate", {"result": "unknown_build"}),
                                  ("ELDENNET", "banner", {"reason": "unknown_build"}),
                                  ("ELDENNET", "init", {"state": "banner"}),
                                  ("LAUNCHER", "status", {"init": "banner"}),
                                  ("LAUNCHER", "end", {"kind": "clean"}))(r) +
                      any_line(r, "ELDENNET", "change", what="banner", to="unknown_build") +
                      any_line(r, "ELDENNET", "change", what="level", to="L1") +
                      (["the DLL started its reader on an unknown build"]
                       if r.events("ELDENNET", "reader") or r.events("ELDENNET", "register") else []) +
                      en_log_is_clean(r)),
     what="a build the DLL's gal/ does not know: its own gate refuses it, L1's banner, no task, the game unharmed")
case("launcher-eldennet-anticheat", LGAME + ["--also", "{easyanticheat_eos}", "--dll", "{eldennetdll}", "--console"] +
     game(frames=420), program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",),
     env=dict(EN_ENV, VEHPROBE_MODE="off"), timeout=180,
     prepare=install_eldennet(copies=(("EasyAntiCheat_EOS.dll", "vehprobe"),)),
     check=lambda r: (check_lines(("ELDENNET", "safety", {"anticheat": 1}),
                                  ("ELDENNET", "banner", {"reason": "anticheat", "detail": "module"}),
                                  ("LAUNCHER", "status", {"init": "banner"}),
                                  ("LAUNCHER", "end", {"kind": "clean"}))(r) +
                      any_line(r, "LAUNCHER", "inject", dll="EasyAntiCheat_EOS.dll", ok=1) +
                      any_line(r, "ELDENNET", "change", what="banner", to="anticheat") +
                      (["the DLL started its reader beside anti-cheat"]
                       if r.events("ELDENNET", "reader") or r.events("ELDENNET", "register") else [])),
     what="a simulated anti-cheat hit (a module named EasyAntiCheat_EOS.dll): the Safety state's check, L1's banner, "
          "no task")
case("launcher-eldennet-twice", LGAME + ["--dll", "{eldennetdll}"] +
     game("--load", "window:{eldennet_copy}", frames=600), program="{launcher}", needs=EN_NEEDS,
     toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(copies=(("eldennet-copy.dll", "eldennet"),)),
     check=lambda r: (want(r, "ELDENNET", "init", state="ready") + want(r, "ELDENNET", "second_copy", idle=1) +
                      want(r, "FAKEGAME", "load", which="first", moment="window", ok=1) +
                      (["%d copies started" % len(r.events("ELDENNET", "safety"))]
                       if len(r.events("ELDENNET", "safety")) != 1 else []) +
                      want(r, "LAUNCHER", "end", kind="clean")),
     what="two loads (the launcher's and the game's own loader's): the second copy idles behind R16's mutex, writes "
          "nothing, and the first names it at Ready")
case("launcher-eldennet-crash", LGAME + ["--dll", "{eldennetdll}"] + game("--crash", "av-read", "--crash-at", "400",
                                                                          frames=720),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(),
     check=lambda r: (want(r, "LAUNCHER", "end", kind="crash", exit_code="0xC0000005", crash_filter=1, at_crash=1) +
                      want(r, "LAUNCHER", "crash", code="0xC0000005", thread="game") +
                      want(r, "LAUNCHER", "record", kept=1, kind="crash") +
                      [f for rec in crash_records(r) for f in record_is_clean(rec)] +
                      (["the record: %s" % crash_records(r)]
                       if len(crash_records(r)) != 1 or "eldennet.dll" not in crash_records(r)[0]["modules"] or
                       crash_records(r)[0]["status"]["init"] != "ready" else [])),
     what="the game crashes on its game thread: eldennet.dll's own filter writes the record into the block and waits "
          "for the watcher, which keeps a scrubbed record")
case("launcher-eldennet-overflow", LGAME + ["--dll", "{eldennetdll}"] +
     game("--crash", "stack-overflow", "--crash-at", "400", frames=720),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(),
     check=lambda r: (want(r, "LAUNCHER", "end", kind="crash", exit_code="0xC00000FD", crash_filter=1, at_crash=1) +
                      want(r, "LAUNCHER", "crash", code="0xC00000FD", thread="game") +
                      want(r, "LAUNCHER", "record", kept=1, kind="crash") +
                      [f for rec in crash_records(r) for f in record_is_clean(rec)]),
     what="a stack overflow on the game thread: the DLL's filter, in the little stack left, still writes the record "
          "(no module look-up) and waits for the watcher")
case("launcher-eldennet-watch", GAL[1:] + ["--frames", "720", "--load", "ready:{eldennetdll}", "--crash", "av-read",
                                           "--crash-at", "400"],
     needs=EN_NEEDS, toolchains=("clang-cl",), exit=AV, settle=True, timeout=180, env=EN_ENV,
     prepare=install_eldennet(copies=(("eldennet/eldennet-launcher.exe", "launcher"),)),
     check=lambda r: (want(r, "ELDENNET", "safety", watched=1, watcher_started=1) +
                      want(r, "LAUNCHER", "watch", ok=1, block_created=1) +
                      want(r, "LAUNCHER", "crash", code="0xC0000005", thread="game") +
                      want(r, "LAUNCHER", "end", kind="crash", crash_filter=1) +
                      want(r, "LAUNCHER", "record", kept=1)),
     what="loaded by another loader (the fake game's own --load): eldennet.dll starts the watcher beside it "
          "(--watch <pid>), which makes the block, sees the crash through the DLL's filter and keeps its record")


# --- The runs of GraceUI's observer (S0-E's present-hook half, S4): EldenNet's side ---------------------------------------
# `--spike s0e|s4`: the launcher makes the run's folder (spikes\<word>-<n>) before the game starts and names it in the
# game's ELDENNET_SPIKE_DIR (the fake game says only whether it is set and whether the folder exists); eldennet.dll
# stops at the start of Reader with the spike_run banner, before its reader is made, and calls nothing (the fake hub's live tasks stay the
# program's own four, and no registration from outside reaches it), and runs on as a stopped start does. `--spike s0e_alone` loads the --also DLLs alone.
def spike_folders(result):
    folder = os.path.join(result.paths["appdata"], "EldenNet", "spikes")
    return sorted(n for n in os.listdir(folder) if os.path.isdir(os.path.join(folder, n))) if os.path.isdir(folder) \
        else []


def no_call(r):
    """The fake game's registration never reached from outside: the hub's live tasks are the program's own four, and
    no registration of a task of ours was accepted, ignored, refused or faulted."""
    return (want(r, "FAKEGAME", "end", foreign_tasks=0, live=4, rejected=0, ignored=0) +
            (["the fake game's registration was called"] if r.events("FAKEGAME", "register") else []))


def check_spike_folder(word):
    def check(r):
        failures = check_lines(("LAUNCHER", "begin", {"spike": word}),
                               ("LAUNCHER", "spike_dir", {"spike": word, "n": 1, "made": 1}),
                               ("FAKEGAME", "spike_dir", {"set": 1, "exists": 1}))(r)
        need(spike_folders(r) == ["%s-1" % word], "the spike folders: %s" % spike_folders(r), failures)
        lines = "\n".join(l for l in r.lines if l.startswith("LAUNCHER "))
        # "spikes" as a folder's name (a package's label, 0.1.0-spikes.N, is no path)
        need(":\\" not in lines and "\\" not in lines and not re.search(r"spikes[\\/]", lines) and
             "no-such-folder" not in lines, "the launcher's lines hold a path", failures)
        return failures + check_spike(word, 1, crashes=0)(r)
    return check


def check_en_spike(word, grace=False):
    def check(r):
        failures = check_spike_folder(word)(r) + check_lines(
            ("LAUNCHER", "started", {"ok": 1, "inject": 1, "eldennet": 1}),
            ("LAUNCHER", "inject", {"dll": "eldennet.dll", "ok": 1}),
            ("ELDENNET", "safety", {"spike": word, "watched": 1}),
            ("ELDENNET", "gate", {"result": "passed"}),
            ("ELDENNET", "discovery", {"ready": 1}),
            ("ELDENNET", "reader", {"skipped": 1, "why": "spike", "spike": word}),
            ("ELDENNET", "banner", {"reason": "spike_run", "detail": word}),
            ("ELDENNET", "init", {"state": "banner"}),
            ("LAUNCHER", "status", {"init": "banner"}),
            ("LAUNCHER", "run", {"outcome": "banner"}),
            ("LAUNCHER", "end", {"kind": "clean", "crash_filter": 0, "init": "banner"}))(r)
        failures += any_line(r, "ELDENNET", "init", state="reader")
        failures += any_line(r, "ELDENNET", "change", what="banner", to="spike_run")
        failures += any_line(r, "ELDENNET", "change", what="level", to="L1")
        failures += any_line(r, "ELDENNET", "status", level="L1", phase="banner")  # T6 runs on after T1 stopped
        need(not any("started" in e for e in r.events("ELDENNET", "reader")) and not r.events("ELDENNET", "register"),
             "the DLL started its reader", failures)
        need(not r.events("ELDENNET", "first_reading") and not r.events("ELDENNET", "first_tick"),
             "the DLL waited for a first reading", failures)
        failures += no_call(r)
        if grace:
            failures += want(r, "ELDENNET", "connect", ok=1, outcome="connected", path="main", installed=1)
            failures += any_line(r, "ELDENNET", "notice", what="banner", reason="spike_run", status=0)
        else:
            failures += want(r, "ELDENNET", "connect", ok=0, outcome="absent", installed=0)
        return failures + en_log_is_clean(r)
    return check


def check_spike_recording(word):
    """The recorder runs in the stopped start: its header carries the word, its banner line the spike_run reason, and
    the file lints clean; rec-stats counts the banner."""
    def check(r):
        failures = []
        files = en_recordings(r)
        need(len(files) == 1, "%d recordings" % len(files), failures)
        for path in files:
            code, out, _ = run_engine(r.args, r.env, ["lint", "--recording", r.as_win(path)])
            need(code == 0 and "the recording is clean" in out, "lint --recording: exit %s %s" % (code, out[-400:]),
                 failures)
            with open(path, encoding="utf-8") as f:
                records = [json.loads(l) for l in f if l.strip()]
            need(records and records[0]["k"] == "header" and records[0].get("spike") == word,
                 "the header: %s" % records[:1], failures)
            need([x.get("reason") for x in records if x["k"] == "banner"] == ["spike_run"],
                 "the banners recorded: %s" % [x for x in records if x["k"] == "banner"], failures)
            need(not any(x["k"] in ("snap", "life") for x in records), "a snapshot or a lifecycle line was recorded",
                 failures)
            stats_out = path + ".stats.json"
            code, _, err = run_engine(r.args, r.env, ["rec-stats", r.as_win(path), "--out", r.as_win(stats_out)])
            need(code == 0 and os.path.exists(stats_out), "rec-stats: exit %s %s" % (code, err[-300:]), failures)
            if code == 0 and os.path.exists(stats_out):
                with open(stats_out) as f:
                    st = json.load(f)
                need(st["header"]["spike"] == word and st["banners"] == [{"reason": "spike_run", "count": 1}],
                     "rec-stats: %s" % {k: st.get(k) for k in ("header", "banners")}, failures)
        return failures
    return check


case("launcher-eldennet-spike-s0e", LGAME + ["--dll", "{eldennetdll}", "--console", "--spike", "s0e"] +
     game(frames=900), program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",),
     env=dict(EN_ENV, **LEFTOVER_SPIKE_DIR), timeout=180, prepare=install_eldennet(EN_RECORD),
     check=lambda r: check_en_spike("s0e")(r) + check_spike_recording("s0e")(r),
     what="S0-E with the release DLL: the run's folder made and named in the game's environment (the launcher's own "
          "variable replaced), the DLL stopped at Reader with the spike_run banner and no call into the game, "
          "T6 and the recorder running on, a clean end")
case("launcher-eldennet-spike-s4", LGAME + ["--dll", "{eldennetdll}", "--console", "--spike", "s4"] + game(frames=900),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=180,
     prepare=install_eldennet(), check=check_en_spike("s4"),
     what="S4 (input), as S0-E: its own folder, no call into the game, the banner")
case("launcher-eldennet-spike-s0e-alone", LGAME + ["--also", "{vehprobe}", "--console", "--spike", "s0e_alone"] +
     game(frames=300), program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",),
     env=dict(EN_ENV, VEHPROBE_MODE="pass", VEHPROBE_LOG="{status}"), timeout=180, prepare=install_eldennet(),
     check=lambda r: (check_spike_folder("s0e_alone")(r) +
                      check_lines(("LAUNCHER", "preflight", {"dll": "none"}),
                                  ("LAUNCHER", "started", {"ok": 1, "inject": 1, "also": 1, "eldennet": 0}),
                                  ("LAUNCHER", "inject", {"dll": "vehprobe.dll", "ok": 1}),
                                  ("VEHPROBE", "installed", {"mode": "pass"}),
                                  ("LAUNCHER", "run", {"outcome": "game_ended"}),
                                  ("LAUNCHER", "end", {"kind": "clean", "init": "none"}))(r) +
                      (["eldennet.dll was loaded"] if any(e.get("dll") == "eldennet.dll"
                                                          for e in r.events("LAUNCHER", "inject")) else []) +
                      (["an ELDENNET line"] if any(l.startswith("ELDENNET ") for l in r.lines) else []) +
                      want(r, "FAKEGAME", "end", foreign_tasks=0, live=4)),
     what="S0-E's baseline: the --also DLL alone (a stand-in for GraceUI's observer), no eldennet.dll in the process, "
          "the run's folder named in the game's environment")



# --- GraceUI with eldennet.dll (part 12): the GraceUI folder of a test build of GraceUI (EN_GRACEUI_DIR: graceui.dll
# with its test exports, LICENSE.txt, NOTICE.txt and the graceui.toml GraceUI's build writes of each file's size and
# SHA-256, its D52 and D165), installed as it is in the fake game's folder as GraceUI's release lays it out (its D51);
# the launcher's pre-flight checks it. The fake game presents on Direct3D 12 (--d3d12), so that GraceUI's overlay binds
# its swap chain and draws, presses F11 and Pause into its window, and reports what GraceUI's test exports say
# (--grace-report): EldenNet's connection, its notices by key, the panels made, shown and drawn, the world-state
# providers, and the pixels of the F11 view's ground read back from the frame. The view's place and ground are
# en-overlay's (src/en-overlay/include/en/overlay/view.hpp): at 24, 24, 680 by 500, ground (22, 28, 38); the probe keeps
# left of the notices at the top right.
GRACE_NEEDS = EN_NEEDS + ("graceui",)
EN_PANEL = 'schema = 2\ndebug = { panel = true, record_sessions = true }\n'
VIEW_PROBE = "30,60,640,440,22,28,38"
VIEW_AREA = (640 - 30) * (440 - 60)


def install_graceui(paths):
    """GraceUI's folder beside the fake game, copied as GraceUI's build laid it out (its D165): no file of ours in it."""
    folder = os.path.join(paths["gamedir"], "GraceUI")
    shutil.copytree(os.path.dirname(paths["graceui"]), folder, symlinks=False)
    paths["gracedll"] = os.path.join(folder, "graceui.dll")


def any_line_kv(result, source, event, kv):
    """any_line, with the values in a dictionary: for a key named as one of its parameters (`source`)."""
    for e in result.events(source, event):
        if all(e.get(k) == str(v) for k, v in kv.items()):
            return []
    return ["no %s %s line with %s" % (source, event, " ".join("%s=%s" % i for i in sorted(kv.items())))]


def want_grace(result, frame, **kv):
    """The fake game's report of GraceUI's state at `frame`: each key equal to a string, a number within (lo, hi), or
    a callable that answers whether the value is right."""
    line = next((e for e in result.events("FAKEGAME", "grace") if e.get("frame") == str(frame)), None)
    if line is None:
        return ["no FAKEGAME grace line at frame %d" % frame]
    failures = []
    for key, value in kv.items():
        got = line.get(key)
        if callable(value):
            ok = got is not None and value(got)
        elif isinstance(value, tuple):
            ok = got is not None and re.match(r"^-?[0-9.]+$", got) and value[0] <= float(got) <= value[1]
        else:
            ok = got == str(value)
        need(ok, "FAKEGAME grace frame=%d %s=%s, wanted %s" % (frame, key, got, getattr(value, "__name__", value)),
             failures)
    return failures


def graceui_version(r):
    """The version and ABI the GraceUI folder under test declares, read from its graceui.toml (GraceUI's D52), as the
    launcher's pre-flight and the connect report them: ("0.2.1", "1.1")."""
    with open(os.path.join(os.path.dirname(r.paths["graceui"]), "graceui.toml"), encoding="utf-8") as f:
        text = f.read()
    version = re.search(r'^graceui = "([0-9.]+)"$', text, re.M)
    abi = re.search(r"^abi = \[([0-9]+), ([0-9]+)\]$", text, re.M)
    if not version or not abi:
        raise ValueError("graceui.toml without its graceui and abi lines")
    return version.group(1), "%s.%s" % (abi.group(1), abi.group(2))


def check_graceui_main(r):
    version, abi = graceui_version(r)
    return (check_lines(("LAUNCHER", "preflight", {"graceui": "ok", "version": version, "abi": abi}),
                        ("ELDENNET", "connect", {"ok": 1, "outcome": "connected", "path": "main", "how": "path",
                                                 "installed": 1, "graceui": version, "view": "ready", "mark": 1,
                                                 "mark_bound": 1, "world": 1}),
                        ("ELDENNET", "init", {"state": "ready"}),
                        ("LAUNCHER", "end", {"kind": "clean"}))(r) +
            # Connected, at the title or in the world: the panel made and hidden, the atlas registered, EldenNet a
            # world-state provider, no banner (the main path has none).
            want_grace(r, 360, found=1, eldennet=1, notice_only=0, panels=1, shown=0, textures=(1, 8), providers=1,
                       banner="none", window_filter=1, presents=(100, 1e9),
                       caps=lambda c: all(w in c.split("_") for w in ("notices", "world-state", "hotkeys", "panels",
                                                                       "textures")),
                       probe=(0, VIEW_AREA // 100)) +
            # F11 (frame 420): GraceUI's hotkey, through poll; one list before the panel shows, then a list every wake;
            # GraceUI drew it on the game's frame: its ground read back over most of the probe.
            want_grace(r, 480, shown=1, submitted=(2, 1e9), refused=0, panel_frames=(1, 1e9),
                       probe=(VIEW_AREA // 2, VIEW_AREA)) +
            any_line(r, "ELDENNET", "view", presses=1, open=1) +
            # F11 again (frame 600): hidden, nothing of it on the frame.
            want_grace(r, 660, shown=0, refused=0, probe=(0, VIEW_AREA // 100)) +
            any_line(r, "ELDENNET", "view", presses=1, open=0) +
            # The Mark: GraceUI's hotkey on Pause, the launcher's console handing its ring over, then Pause (frame 540)
            # stamped by EldenNet, which the launcher's record takes from the ring.
            any_line_kv(r, "ELDENNET", "marks", {"source": "eldennet"}) +
            any_line_kv(r, "LAUNCHER", "mark", {"hotkey": "pause", "released": 1, "source": "eldennet"}) +
            want(r, "ELDENNET", "mark", n=1) + any_line_kv(r, "LAUNCHER", "mark", {"n": 1, "source": "eldennet"}) +
            (["%d Marks for one press" % len(r.events("ELDENNET", "mark"))]
             if len([e for e in r.events("ELDENNET", "mark") if "n" in e]) != 1 else []) +
            en_log_is_clean(r))


def check_graceui_before(r):
    return (check_lines(("LAUNCHER", "preflight", {"graceui": "ok"}),
                        ("ELDENNET", "gate", {"result": "unknown_build"}),
                        ("ELDENNET", "connect", {"ok": 1, "outcome": "connected", "path": "banner", "how": "name",
                                                 "installed": 1, "view": "ready"}),
                        ("ELDENNET", "notice", {"what": "banner", "reason": "unknown_build", "status": 0}),
                        ("ELDENNET", "init", {"state": "banner"}),
                        ("LAUNCHER", "end", {"kind": "clean"}))(r) +
            any_line(r, "LAUNCHER", "inject", dll="graceui.dll", ok=1) +
            # The banner: one notice under EldenNet's key, duration 0, in the warning tone, with the build's number;
            # EldenNet's space registered for the game's view alone, so GraceUI's server never starts for it (PAGES
            # §4.4, GraceUI §7.2 item 1).
            want_grace(r, 300, found=1, eldennet=1, notice_only=0,
                       banner="EldenNet_inactive:_game_build_3.14.15.0_not_supported_yet", banner_tone=1,
                       badge="none", shown=0, server=0, spaces=1) +
            any_line(r, "ELDENNET", "connect", path="banner", pages=0) +
            # F11 on the banner path too (frame 360): the view shows L1 and why.
            want_grace(r, 460, shown=1, refused=0, panel_frames=(1, 1e9), probe=(VIEW_AREA // 2, VIEW_AREA),
                       banner="EldenNet_inactive:_game_build_3.14.15.0_not_supported_yet") +
            any_line(r, "ELDENNET", "view", presses=1, open=1) +
            (["the DLL started its reader on an unknown build"]
                       if r.events("ELDENNET", "reader") or r.events("ELDENNET", "register") else []) +
            en_log_is_clean(r))


def check_graceui_unbound(r):
    return (check_lines(("FAKEGAME", "grace_other", {"found": 1, "connect": 0, "hotkey": 0}),
                        ("LAUNCHER", "preflight", {"graceui": "ok"}),
                        ("LAUNCHER", "mark", {"hotkey": "pause", "registered": 1}),
                        ("ELDENNET", "connect", {"ok": 1, "outcome": "connected", "path": "main", "how": "name",
                                                 "installed": 1, "view": "ready", "mark": 1, "mark_bound": 0}),
                        ("ELDENNET", "marks", {"source": "launcher", "bound": 0}),
                        ("LAUNCHER", "end", {"kind": "clean"}))(r) +
            # Another mod (the fake game) holds Pause in GraceUI, so EldenNet's Mark registered unbound (D164): EldenNet
            # never asks for the ring, the launcher keeps its key, and the Pause pressed is the launcher's own Mark.
            want_grace(r, 360, found=1, eldennet=1, consumers=2, panels=1, shown=0) +
            (["the launcher let Pause go"] if any("released" in e for e in r.events("LAUNCHER", "mark")) else []) +
            (["the Marks' ring was asked for"] if any(e.get("source") in ("asked", "eldennet")
                                                       for e in r.events("ELDENNET", "marks")) else []) +
            any_line(r, "LAUNCHER", "mark", n=1) +
            (["the launcher's Mark named another source"] if any("source" in e and "n" in e
                                                                for e in r.events("LAUNCHER", "mark")) else []) +
            want(r, "ELDENNET", "mark", n=1) +
            (["a press was offered to EldenNet's unbound Mark"] if any(e.get("taken") == "0"
                                                                      for e in r.events("ELDENNET", "mark")) else []) +
            en_log_is_clean(r))


def check_graceui_anticheat(r):
    text = "EldenNet_inactive:_Easy_Anti-Cheat_is_running._EldenNet_only_runs_offline;_start_the_game_with_the_" \
           "EldenNet_launcher."
    return (check_lines(("ELDENNET", "safety", {"anticheat": 1}),
                        ("ELDENNET", "connect", {"ok": 1, "outcome": "notices_only", "path": "banner"}),
                        ("ELDENNET", "notice", {"what": "banner", "reason": "anticheat", "status": 0}),
                        ("LAUNCHER", "end", {"kind": "clean"}))(r) +
            # GraceUI's own check found the same module before it hooked: notices only, and EldenNet's banner among
            # them; no panel, no hotkey, no provider.
            want_grace(r, 300, found=1, eldennet=1, notice_only=1, banner=text, panels=0, shown=0, providers=0) +
            (["the DLL started its reader beside anti-cheat"]
                       if r.events("ELDENNET", "reader") or r.events("ELDENNET", "register") else []) +
            en_log_is_clean(r))


# Augur's events in F11's view (roadmap S10.2.d): the fake game's world raises Augur's events (its place entered, the
# boss bar's fight, the talk popup's native talk at frames 600 to 659, and the lifecycle's own), and the view, opened at
# frame 720, lists them, each line after a square of its source's colour (en-overlay's view.hpp: kViewMarks, 6 by 6
# pixels at panel x 9, 4 below the line's top). Two more probes count those squares in the panel's mark column: the
# world's green and the lifecycle's blue; each square is 36 pixels, so each count is a number of lines drawn.
EVENT_MARK_PX = 36
MARKS_RECT = "30,24,46,524"
WORLD_PROBE = MARKS_RECT + ",80,190,110"
LIFE_PROBE = MARKS_RECT + ",90,140,210"
VIEW_EVENTS = 10  # en-overlay's kViewEvents


def check_graceui_events(r):
    failures = []
    # Before F11 and after it again: no square on the frame.
    failures += want_grace(r, 660, shown=0, probe2=(0, 0), probe3=(0, 0))
    failures += want_grace(r, 900, shown=0, probe2=(0, 0), probe3=(0, 0))
    # Open: the ground over most of the probe, and squares of both colours, whole squares, at most the list's ten.
    failures += want_grace(r, 780, shown=1, refused=0, panel_frames=(1, 1e9), probe=(VIEW_AREA // 2, VIEW_AREA))
    line = next((e for e in r.events("FAKEGAME", "grace") if e.get("frame") == "780"), {})
    world = int(line.get("probe2", "0") or 0)
    life = int(line.get("probe3", "0") or 0)
    need(world >= EVENT_MARK_PX, "no world event drawn: probe2=%s" % world, failures)
    need(life >= EVENT_MARK_PX, "no lifecycle event drawn: probe3=%s" % life, failures)
    need(world % EVENT_MARK_PX == 0 and life % EVENT_MARK_PX == 0,
         "the squares are not whole: probe2=%s probe3=%s" % (world, life), failures)
    need((world + life) // EVENT_MARK_PX <= VIEW_EVENTS, "more than %d events drawn" % VIEW_EVENTS, failures)
    # The DLL's side: the view opened with events in it, and the status lines (every 10 s) count Augur's events.
    opened = [e for e in r.events("ELDENNET", "view") if e.get("open") == "1"]
    need(len(opened) == 1 and int(opened[0].get("events", "0")) >= 1,
         "the view's open line: %s" % [e.get("_rest", e) for e in opened], failures)
    seen = [int(e["events"]) for e in r.events("ELDENNET", "status") if "events" in e]
    need(bool(seen) and max(seen) >= 1, "the status lines' events: %s" % seen, failures)
    return (check_lines(("ELDENNET", "connect", {"ok": 1, "outcome": "connected", "view": "ready"}),
                        ("LAUNCHER", "end", {"kind": "clean"}))(r) +
            failures + any_line(r, "ELDENNET", "view", presses=1, open=0) + en_log_is_clean(r))


# EldenNet's pages (S11.4.a; GraceUI D169, D171), at the title screen (in the world the fake game's player reads as in
# danger, which keeps GraceUI's menu shut, invariant 9): F5 opens GraceUI's menu on EldenNet's Home; Page Down past AI's
# three pages to System › Status, and once more to Logs, where Log detail's shortcut row has the focus; Enter on it opens its home, Settings ›
# Developer, with the row focused; → there changes it (a command to EldenNet, answered); F5 closes the menu. While it
# runs, a browser of the tests' own (tests/pages/browser.py) joins GraceUI's server on loopback, at the port the fake
# game's report gives, reads EldenNet's topics and sends commands.
PAGES_KEYS = "600:f5,720:next,780:next,840:next,900:next,1020:next,1140:return,1260:right,1380:f5"
PAGES_REPORTS = "360,660,960,1080,1200,1320,1440"


def browse_eldennet(paths, out):
    """The browser's part: once the fake game's report names GraceUI's server running with a space, a page load's
    cookie, a ticket and a WebSocket; each of EldenNet's topics watched and its first frame kept; three commands and
    their answers. What it got goes in `out`; nothing of it is printed but in a failure."""
    deadline = time.monotonic() + 120
    port = None
    while time.monotonic() < deadline and port is None:
        try:
            with open(paths["status"], encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            text = ""
        for line in text.splitlines():
            m = re.search(r"^FAKEGAME grace .* server=1 port=([0-9]+) spaces=([1-9])", line)
            if m:
                port = int(m.group(1))
        if port is None:
            time.sleep(0.25)
    if port is None:
        out["error"] = "no report named the server running"
        return
    view = None
    try:
        view = browser.Browser(port, timeout=10)
        out["welcome"] = view.welcome
        for topic in ("status", "errors", "system.status", "logs", "settings"):
            view.sub("eldennet", topic)
            out[topic] = view.state("eldennet", topic, timeout=10)
        out["set_ok"] = view.command("eldennet", "settings.set", {"key": "debug.record_sessions", "value": False})
        out["set_bad"] = view.command("eldennet", "settings.set", {"key": "debug.log_level", "value": "loud"})
        out["reset_unconfirmed"] = view.command("eldennet", "settings.reset",
                                                {"scope": "page", "target": "settings.developer"})
        out["settings_after"] = view.state("eldennet", "settings", timeout=10)
    except (browser.BrowserError, OSError, ValueError, KeyError) as e:
        out["error"] = repr(e)
    finally:
        if view is not None:
            view.close()


def check_graceui_pages(r):
    d = r.during
    failures = []
    if "error" in d:
        return ["the browser: %s" % d["error"]]
    body = lambda topic: d.get(topic, {}).get("body", {})
    # The browser: the welcome names EldenNet's space at its increment; every topic watched came back, built by
    # EldenNet from what it knows, in its own words; each command answered once, by EldenNet's one command path.
    spaces = {s.get("id"): s for s in d.get("welcome", {}).get("spaces", [])}
    need(spaces.get("eldennet", {}).get("increment") == "0.2" and spaces["eldennet"].get("version") == "0.1.0",
         "the welcome named %s" % sorted(spaces), failures)
    status = body("status")
    items = {i.get("label"): i.get("value") for i in status.get("items", [])}
    need(status.get("word") == "Dormant" and status.get("tone") == "glint" and status.get("page") == "system.status" and
         "title screen" in status.get("reason", ""), "the status lamp: %s" % {k: status.get(k) for k in
                                                                              ("word", "tone", "page", "reason")},
         failures)
    need(items.get("EldenNet") == "0.1.0" and items.get("Game build") == "3.14.15.0, supported" and
         items.get("GraceUI") == graceui_version(r)[0], "Home's Status card: %s" % items, failures)
    need(body("errors").get("checks") == [], "Problems: %s" % body("errors"), failures)
    parts = {c.get("label"): c.get("state") for c in body("system.status").get("checks", [])}
    need(parts.get("Start") == "pass" and parts.get("Reading the game") == "pass" and parts.get("Recording") == "pass" and
         parts.get("GraceUI") == "pass", "EldenNet's parts: %s" % parts, failures)
    need(len(body("system.status").get("rows", [])) == 11, "the read groups: %d rows" %
         len(body("system.status").get("rows", [])), failures)
    # System › Status's Compatibility tab reads the topic's `compatibility` member (GraceUI D185's props.field).
    compatibility = [c.get("label") for c in body("system.status").get("compatibility", {}).get("checks", [])]
    need("Game build 3.14.15.0" in compatibility and "EldenNet 0.1.0" in compatibility,
         "Compatibility: %s" % compatibility, failures)
    logs = d.get("logs", {})
    lines = [row.get("line", "") for row in logs.get("rows", [])]
    need(logs.get("t") == "rows" and logs.get("from_seq") == 1 and any(l.startswith("boot ") for l in lines) and
         any(l.startswith("connect ok=1") for l in lines), "the logs stream: %s" % lines[:3], failures)
    need(not any("\\" in l or "Users" in l for l in lines), "a log row holds a path", failures)
    settings = body("settings")
    need(settings.get("values", {}).get("debug.panel") is True and settings.get("schema", {}).get("number") == 2,
         "the settings topic: %s" % settings.get("values"), failures)
    need(d.get("set_ok", {}).get("ok") is True, "settings.set: %s" % d.get("set_ok"), failures)
    need(d.get("set_bad", {}).get("error") == "invalid_value", "a value refused: %s" % d.get("set_bad"), failures)
    need(d.get("reset_unconfirmed", {}).get("error") == "confirm_required",
         "a page's reset unconfirmed: %s" % d.get("reset_unconfirmed"), failures)
    need(body("settings_after").get("values", {}).get("debug.record_sessions") is False,
         "the settings after the change: %s" % body("settings_after").get("values"), failures)
    # In game: F5's menu on EldenNet's Home; System › Status and Logs; Log detail's shortcut to its home on Settings ›
    # Developer, where → sends EldenNet a command it answers; F5 closes. The menu watched the settings while open.
    failures += (want_grace(r, 360, eldennet=1, server=1, spaces=1, menu_open=0) +
                 want_grace(r, 660, menu_open=1, menu_page="home") +
                 want_grace(r, 960, menu_open=1, menu_page="system.status") +
                 want_grace(r, 1080, menu_open=1, menu_page="system.logs", menu_focused="shortcut:debug.log_level") +
                 want_grace(r, 1200, menu_open=1, menu_page="settings.developer",
                            menu_focused="setting:debug.log_level") +
                 want_grace(r, 1320, menu_open=1, menu_page="settings.developer", menu_refusals=0) +
                 want_grace(r, 1440, menu_open=0))
    failures += check_lines(("ELDENNET", "connect", {"ok": 1, "outcome": "connected", "path": "main", "pages": 0}),
                            ("LAUNCHER", "end", {"kind": "clean"}))(r)
    for name, key, result in (("settings.set", "debug.record_sessions", "ok"),
                              ("settings.set", "debug.log_level", "invalid_value"),
                              ("settings.reset", "settings.developer", "confirm_required"),
                              ("settings.set", "debug.log_level", "ok")):
        failures += any_line_kv(r, "ELDENNET", "page", {"what": "command", "name": name, "key": key, "result": result})
    failures += any_line_kv(r, "ELDENNET", "setting", {"key": "debug.log_level", "effect": "now"})
    failures += any_line(r, "ELDENNET", "page", what="watched", topic="settings")
    failures += any_line(r, "ELDENNET", "settings_write", ok=1, keys=2)
    # settings.toml as the pages left it: both changes, the player's other keys kept.
    path = os.path.join(r.paths["appdata"], "EldenNet", "config", "settings.toml")
    try:
        with open(path, encoding="utf-8") as f:
            written = f.read()
    except OSError:
        written = ""
    need(re.search(r"""log_level = ["']debug["']""", written) and re.search(r"record_sessions = false", written) and
         re.search(r"panel = true", written) and re.search(r"schema = 2", written),
         "settings.toml after the pages: %r" % written, failures)
    answered = [e for e in r.events("ELDENNET", "page") if e.get("what") == "command"]
    need(len(answered) == 4, "%d commands answered, wanted 4" % len(answered), failures)
    return failures + en_log_is_clean(r)


case("launcher-eldennet-graceui-pages", LGAME + ["--dll", "{eldennetdll}", "--console"] +
     game("--d3d12", "--stay-title", "--press-keys", PAGES_KEYS, "--grace-report", PAGES_REPORTS, frames=1560),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(EN_PANEL, then=install_graceui), during=browse_eldennet,
     check=check_graceui_pages,
     what="EldenNet's pages through GraceUI: register_pages at the connect; F5's menu on EldenNet's Home, then System › "
          "Status and Logs, a setting changed from Logs' shortcut row and answered, and Settings › Developer; a "
          "browser on GraceUI's server reading EldenNet's topics and sending commands, each answered; settings.toml "
          "written after the pages' quiet")
# EldenNet's AI pages (S11.4.a's second to fourth slices; PAGES §4.6.2), at the title screen, against a fake model
# server of the tests' own on loopback (tests/pages/fake_models.py) with invented models, which the models.toml installed
# beside the DLL names for the Cloud budget preset's provider. A browser of the tests' own adds the preset's provider and
# sets a wrong invented key on its row (GraceUI D194), whose model list is refused: the row reads Refused (D197) and
# Health and the lead say why; sets the right key, runs the self-test once the gate allows network and lists the models;
# adds a server of its own on this PC through the form (D195), refused first for a public address over http:// and for
# one holding a password, whose row reads Not needed (D197) and takes no key, and lists again (that server takes none);
# gives Prepared lines that provider (refused first for one not in the file), whose list lacks the job's model, so it
# waits for a model of its list, which it picks (D193), refused first for one not listed; sees the changed mark on that
# job's rows alone (D198); and is refused Remove on a row unconfirmed (GraceUI's server) and on providers jobs use
# (EldenNet's words). Then GraceUI's menu opens AI › Providers (F5, Page Down), which draws Set and Not needed, tries
# Remove on the server of its own (Down to it, Enter twice: D192), refused, opens Models, steps Memory's provider picker
# on to that server (Down to it, →) and its model picker on to the first of its models (Down, →), whose help ends with
# the default in words, and runs the self-test from Self-test's header action (Enter). Then the browser sees both jobs'
# rows changed, resets each job (one by its model row, one by its provider row), sees no row changed, removes the server
# of its own, chooses the NanoGPT preset, whose provider joins, and removes the unused one with its key, confirmed. Both
# keys' values must then be nowhere: no topic, log line, recording, report or file.
# On Models, Down 17 times from the preset to Memory's provider: each card's head and its four rows (GraceUI D202).
AI_KEYS = ("3120:f5,3240:next,3360:down,3390:down,3420:down,3450:down,3480:down,3600:return,3660:return,3780:next,"
           + ",".join("%d:down" % (3900 + 30 * i) for i in range(17)) + ",4500:right,4620:down,4680:left,4800:next,"
           "4920:return,5400:f5")
# The early reports name GraceUI's server to the browser (grace_port), so that its first part ends long before the menu's
# presses need the provider it adds.
AI_REPORTS = "600,900,1200,1500,1800,2460,3060,3180,3300,3540,3720,3840,4440,4560,4740,4860,5340,5460"
AI_SETTINGS = 'schema = 2\ndebug = { panel = true, record_sessions = true, log_level = "debug" }\n'
AI_MODELS = ["invented-talker-small", "invented-talker-large", "invented-keeper"]
AI_OWN = "lantern-box"  # the provider of the case's own: an invented name, on the fake server's own path
AI_OWN_MODELS = ["invented-lantern-a", "invented-lantern-b"]  # what that server, which takes no key, lists
AI_RUNS = {}  # a case's invented key and fake server, by its folder


def install_ai(paths):
    """GraceUI's folder; the fake model server, with a new invented key; models.toml beside the DLL naming it."""
    install_graceui(paths)
    key = "fk-invented-" + os.urandom(12).hex()
    wrong = key[:24] + os.urandom(6).hex()  # a key the fake server refuses, with the same fragment as the right one
    server = fake_models.FakeModels(key, AI_MODELS, open_prefix="/own/", open_models=AI_OWN_MODELS)
    AI_RUNS[paths["case"]] = {"key": key, "wrong": wrong, "server": server}
    # What the menu's report looks for (GraceUI_Test_MenuShows, at most four texts): a fragment both keys hold, which it
    # must never draw, and texts of the pages' that it must: a row's key state (D197), a card's chip and a card's live
    # status (D202).
    paths["text_ai_shows"] = "|".join((key[12:24], "Key: Not needed", "bark_pool", "Ready"))
    content = os.path.join(paths["endir"], "content", "eldenring")
    os.makedirs(content)
    with open(os.path.join(content, "models.toml"), "w") as f:
        f.write('schema = 1\n\n'
                '[preset.cloud-budget.providers.openrouter]\nflavor = "openai"\nkind = "cloud"\n'
                'base_url = "http://127.0.0.1:%d/v1"\n\n'
                '[preset.cloud-budget]\n'
                'dialogue = { provider = "openrouter", model = "invented-talker-small", '
                'fallback = "invented-talker-large" }\n'
                'memory = { provider = "openrouter", model = "invented-keeper" }\n\n'
                '[preset.cloud-nanogpt.providers.nanogpt]\nflavor = "openai"\nkind = "cloud"\n'
                'base_url = "http://127.0.0.1:%d/nano/v1"\n\n'
                '[preset.cloud-nanogpt]\n'
                'dialogue = { provider = "nanogpt", model = "invented-talker-small" }\n' % (server.port, server.port))


def grace_port(paths, deadline):
    """The port of GraceUI's server, once the fake game's report names it running with a space."""
    while time.monotonic() < deadline:
        try:
            with open(paths["status"], encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            text = ""
        for line in text.splitlines():
            m = re.search(r"^FAKEGAME grace .* server=1 port=([0-9]+) spaces=([1-9])", line)
            if m:
                return int(m.group(1))
        time.sleep(0.25)
    return None


def browse_ai(paths, out):
    run = AI_RUNS[paths["case"]]
    key = run["key"]
    port = grace_port(paths, time.monotonic() + 120)
    if port is None:
        out["error"] = "no report named the server running"
        return
    view = None
    E = "eldennet"
    rows = lambda b: b.get("rows", [])
    row_of = lambda b, pid: next((r for r in rows(b) if r.get("id") == pid), {})
    values = lambda b: b.get("values", {})
    word = lambda b, slot: b.get(slot, {}).get("word")
    changed = lambda b: [k for k in b.get("changed", []) if not k.startswith("debug.")]  # the case's settings.toml's
    try:
        view = browser.Browser(port, timeout=10)
        out["welcome"] = view.welcome
        for topic in ("settings", "ai.status", "providers", "health", "ai.jobs", "ai.budgets", "selftest",
                      "ai.choices.bark_pool", "ai.choices.providers", "ai.choices.dialogue.fallback", "pages"):
            view.sub(E, topic)
        out["pages"] = view.wait(E, "pages", lambda b: bool(b.get("pages")))
        out["first_status"] = view.wait(E, "ai.status", lambda b: "sentence" in b)
        out["first_jobs"] = view.latest.get((E, "ai.jobs"))
        out["add"] = view.command(E, "providers.add", {})
        out["added"] = view.wait(E, "providers", lambda b: len(rows(b)) == 1, timeout=15)
        # Each card's status (D202): no key yet; failing once the wrong key is refused; ready with the right one.
        out["jobs_no_key"] = view.wait(E, "ai.jobs", lambda b: word(b, "dialogue") == "No key yet", timeout=15)
        # A wrong key on the provider's row (D194): its cell says "set", never the key; the model list, once the gate
        # allows network (at the title, after 5 s of clean readings), is refused for it: the row reads "refused"
        # (D197), Health and the lead say why, and the provider is no job's choice meanwhile.
        out["set_wrong"] = view.command(E, "providers.key", {"id": "openrouter", "value": run["wrong"]})
        out["with_wrong"] = view.wait(E, "providers", lambda b: row_of(b, "openrouter").get("key") == "set", timeout=15)
        tries, answer, give_up = 0, {}, time.monotonic() + 60
        while time.monotonic() < give_up:
            tries += 1
            answer = view.command(E, "models.list", {})
            if answer.get("error") != "not_now":
                break
            time.sleep(0.5)
        out["list_wrong"], out["list_wrong_tries"] = answer, tries
        out["refused"] = view.wait(E, "providers", lambda b: row_of(b, "openrouter").get("key") == "refused",
                                   timeout=30)
        out["refused_health"] = view.wait(E, "health", lambda b: row_of(b, "openrouter").get("state", "")
                                          .startswith("Failing"), timeout=15)
        out["refused_status"] = view.wait(E, "ai.status", lambda b: "refused its key" in b.get("sentence", ""),
                                          timeout=15)
        out["refused_choices"] = view.wait(E, "ai.choices.providers", lambda b: b.get("choices") == [], timeout=15)
        out["jobs_failing"] = view.wait(E, "ai.jobs", lambda b: word(b, "dialogue") == "Failing", timeout=15)
        # The right key on its row: "set" again.
        out["set_key"] = view.command(E, "providers.key", {"id": "openrouter", "value": key})
        out["with_key"] = view.wait(E, "providers", lambda b: row_of(b, "openrouter").get("key") == "set", timeout=15)
        out["jobs_ready"] = view.wait(E, "ai.jobs", lambda b: word(b, "dialogue") == "Ready", timeout=15)
        tries, answer, give_up = 0, {}, time.monotonic() + 30
        while time.monotonic() < give_up:
            tries += 1
            answer = view.command(E, "selftest.run", {})
            if answer.get("error") != "not_now":
                break
            time.sleep(0.5)
        out["run"], out["run_tries"] = answer, tries
        out["selftest"] = view.wait(E, "selftest", lambda b: "sentence" in b and "running" not in b["sentence"],
                                    timeout=60)
        out["list"] = view.command(E, "models.list", {})
        # (The wrong key's list left a failed check: this one must pass.)
        out["listed"] = view.wait(E, "providers", lambda b: b.get("checks") and
                                  all(c.get("state") == "pass" for c in b["checks"]), timeout=30)
        # A provider of the case's own, through the form (D195): refused first, with words, then added.
        own_url = "http://127.0.0.1:%d/own/v1/" % run["server"].port
        out["own_plain"] = view.command(E, "providers.add", {"name": AI_OWN, "base_url": "http://192.0.2.7:1234/v1"})
        out["own_password"] = view.command(E, "providers.add", {"name": AI_OWN,
                                                                "base_url": "https://someone:pw@models.invalid/v1"})
        out["own"] = view.command(E, "providers.add", {"name": AI_OWN, "base_url": own_url})
        out["own_added"] = view.wait(E, "providers", lambda b: len(rows(b)) == 2, timeout=15)
        out["own_again"] = view.command(E, "providers.add", {"name": AI_OWN.upper(), "base_url": own_url})
        # A server on this PC with no key's place takes none: its row says Not needed (D197), and a key is refused.
        out["own_key"] = view.command(E, "providers.key", {"id": AI_OWN, "value": key})
        out["list_two"] = view.command(E, "models.list", {})
        out["listed_two"] = view.wait(E, "providers", lambda b: len(b.get("checks", [])) == 2 and
                                      all(c.get("state") != "wait" for c in b["checks"]), timeout=30)
        out["own_none"] = view.latest.get((E, "providers"))
        # Prepared lines' provider (a picker of the providers): one not in the file refused; the server of the case's
        # own, whose list lacks the job's model, waits on its row for one of its models, which the job's picker then
        # offers; one it did not list refused; then the job's model from it (D193): the job's rows alone changed.
        out["provider_choices"] = view.wait(E, "ai.choices.providers", lambda b: len(b.get("choices", [])) == 2,
                                            timeout=15)
        out["provider_bad"] = view.command(E, "settings.set", {"key": "slots.bark_pool.provider",
                                                               "value": "invented-nowhere"})
        out["provider"] = view.command(E, "settings.set", {"key": "slots.bark_pool.provider", "value": AI_OWN})
        out["choices"] = view.wait(E, "ai.choices.bark_pool", lambda b: [c.get("value") for c in b.get("choices", [])]
                                   == AI_OWN_MODELS, timeout=15)
        out["waiting"] = view.wait(E, "settings", lambda b: b.get("values", {}).get("slots.bark_pool.provider") ==
                                   AI_OWN, timeout=15)
        out["pick_bad"] = view.command(E, "settings.set", {"key": "slots.bark_pool.model", "value": AI_MODELS[0]})
        out["pick"] = view.command(E, "settings.set", {"key": "slots.bark_pool.model", "value": AI_OWN_MODELS[0]})
        out["changed_one"] = view.wait(E, "settings", lambda b: changed(b) == ["slots.bark_pool.provider",
                                                                          "slots.bark_pool.model"], timeout=15)
        out["picked"] = values(out["changed_one"])
        # Remove on a row: GraceUI's server refuses it unconfirmed (D192); EldenNet refuses one a job uses, saying why.
        out["remove_unconfirmed"] = view.command(E, "providers.remove", {"id": AI_OWN})
        out["remove_used"] = view.command(E, "providers.remove", {"id": "openrouter", "confirm": True})
        out["remove_own_used"] = view.command(E, "providers.remove", {"id": AI_OWN, "confirm": True})
        # Conversation's card: its fallback on the server of the case's own, whose list lacks the fallback now, so its
        # provider waits and its picker offers that server's models; one of them, written with its provider: the
        # fallback rows changed, and the job's own rows not. A job that borrows Conversation's models takes none.
        out["fallback_borrowed"] = view.command(E, "settings.set", {"key": "slots.selftest.fallback",
                                                                    "value": AI_MODELS[1]})
        out["fallback_provider"] = view.command(E, "settings.set", {"key": "slots.dialogue.fallback_provider",
                                                                    "value": AI_OWN})
        out["fallback_choices"] = view.wait(E, "ai.choices.dialogue.fallback", lambda b: [
            c.get("value") for c in b.get("choices", [])] == AI_OWN_MODELS, timeout=15)
        out["fallback"] = view.command(E, "settings.set", {"key": "slots.dialogue.fallback", "value": AI_OWN_MODELS[1]})
        out["changed_fallback"] = view.wait(E, "settings", lambda b: changed(b) == [
            "slots.dialogue.fallback_provider", "slots.dialogue.fallback", "slots.bark_pool.provider",
            "slots.bark_pool.model"], timeout=15)
        secrets = os.path.join(paths["appdata"], "EldenNet", "config", "secrets.toml")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and "secrets_written" not in out:
            try:
                with open(secrets, encoding="utf-8") as f:
                    if key in f.read():
                        out["secrets_written"] = True
            except OSError:
                pass
            time.sleep(0.2)
        out["browser_done_s"] = time.monotonic()
        # The menu's turn: its Remove on the server of the case's own, refused; Memory's provider and model from its
        # pickers, the server of the case's own and the last of its models (← from a value not in the list; → would
        # give Default first, D199); its self-test.
        out["menu_picked"] = view.wait(E, "settings", lambda b: "slots.memory.model" in changed(b) and
                                       values(b).get("slots.memory.provider") == AI_OWN and
                                       values(b).get("slots.memory.model") == AI_OWN_MODELS[1], timeout=150)
        out["selftest_menu"] = view.wait(E, "selftest", lambda b: len(b.get("rows", [])) >= 1 and
                                         "running" not in b.get("sentence", "running"), timeout=120)
        out["changed_two"] = view.wait(E, "settings", lambda b: changed(b) == [
            "slots.dialogue.fallback_provider", "slots.dialogue.fallback", "slots.bark_pool.provider",
            "slots.bark_pool.model", "slots.memory.provider", "slots.memory.model"], timeout=15)
        # Both jobs back to the preset's provider and model, one by its model row, the other by its provider row, and
        # Conversation's fallback by its picker's Default (D199): no row changed; the server of the case's own, now no
        # job's, removed; a new preset, whose provider joins; the old one, now no job's, goes with its key once
        # confirmed.
        for job in ("slots.bark_pool.model", "slots.memory.provider"):
            out["reset_" + job] = view.command(E, "settings.reset", {"scope": "key", "target": job})
        out["fallback_default"] = view.command(E, "settings.set", {"key": "slots.dialogue.fallback", "value": ""})
        out["changed_none"] = view.wait(E, "settings", lambda b: changed(b) == [], timeout=15)
        out["reset"] = values(out["changed_none"])
        out["budgets_reset"] = view.wait(E, "ai.budgets", lambda b: b.get("bark_pool", {}).get("items", [{}])[0] ==
                                         {"label": "Models", "value": "Same as Conversation"}, timeout=15)
        out["remove_own"] = view.command(E, "providers.remove", {"id": AI_OWN, "confirm": True})
        out["own_gone"] = view.wait(E, "providers", lambda b: len(rows(b)) == 1, timeout=15)
        out["preset"] = view.command(E, "settings.set", {"key": "ai.preset", "value": "cloud-nanogpt"})
        out["two"] = view.wait(E, "providers", lambda b: len(rows(b)) == 2, timeout=15)
        out["remove"] = view.command(E, "providers.remove", {"id": "openrouter", "confirm": True})
        out["one"] = view.wait(E, "providers", lambda b: len(rows(b)) == 1, timeout=15)
        out["jobs"] = view.latest.get((E, "ai.jobs"))
        out["health"] = view.latest.get((E, "health"))
    except (browser.BrowserError, OSError, ValueError, KeyError, IndexError) as e:
        out["error"] = repr(e)
    finally:
        if view is not None:
            out["received"] = list(view.received)
            view.close()


def files_holding(folder, needle):
    """Every file under `folder` whose bytes hold `needle`."""
    found = []
    for root, _, names in os.walk(folder):
        for name in names:
            path = os.path.join(root, name)
            try:
                with open(path, "rb") as f:
                    if needle in f.read():
                        found.append(os.path.relpath(path, folder))
            except OSError:
                pass
    return found


def check_graceui_ai(r):
    run = AI_RUNS.pop(r.paths["case"], None)
    if run is None:
        return ["the case's fake server was never made"]
    key, wrong = run["key"], run["wrong"]
    counts = run["server"].snapshot()
    run["server"].close()
    d = r.during
    failures = []
    if "error" in d:
        failures.append("the browser: %s" % d["error"])
    body = lambda name: d.get(name) or {}
    row_of = lambda b, pid: next((x for x in b.get("rows", []) if x.get("id") == pid), {})
    spaces = {s.get("id"): s for s in d.get("welcome", {}).get("spaces", [])}
    need(spaces.get("eldennet", {}).get("increment") == "0.2", "the welcome named %s" % sorted(spaces), failures)
    # AI › Models as the browser view has it (GraceUI's pages topic, D169): one card per job, in PAGES' order, each with
    # its slot as its chip and its live status from ai.jobs (D202), and no Jobs table.
    models = next((pg for pg in body("pages").get("pages", []) if pg.get("id") == "ai.models"), {})
    cards = [(s.get("id"), s.get("title"), s.get("chip"), (s.get("status") or {}).get("topic"),
              (s.get("status") or {}).get("field")) for s in models.get("sections", []) if "chip" in s]
    need(cards == [(slot, title, slot, "ai.jobs", slot) for slot, title in (
        ("dialogue", "Conversation"), ("bark_live", "Quick lines"), ("bark_pool", "Prepared lines"),
        ("memory", "Memory"), ("selftest", "Self-test"))], "the Models page's cards: %s" % cards, failures)
    need(body("pages").get("minor") == 3 and [s.get("id") for s in models.get("sections", [])][:2] == ["now", "preset"]
         and len(models.get("sections", [])) == 7, "the Models page's sections: %s" %
         [s.get("id") for s in models.get("sections", [])], failures)
    need(body("first_status").get("sentence", "").startswith("No provider yet"),
         "the first lead: %s" % body("first_status"), failures)
    need(not body("first_jobs"), "a card's status before any job: %s" % body("first_jobs"), failures)
    need(d.get("add", {}).get("ok") is True, "providers.add: %s" % d.get("add"), failures)
    added = body("added").get("rows", [{}])[0]
    need(added.get("id") == "openrouter" and added.get("key") == "" and added.get("kind") == "Cloud",
         "the provider added: %s" % added, failures)
    # A wrong key: set, then refused by the model list (D197's Refused), Health and the lead saying why.
    need(d.get("set_wrong", {}).get("ok") is True and row_of(body("with_wrong"), "openrouter").get("key") == "set",
         "the wrong key set on its row: %s" % d.get("set_wrong"), failures)
    need(d.get("list_wrong", {}).get("ok") is True, "models.list with the wrong key: %s after %s tries" %
         (d.get("list_wrong"), d.get("list_wrong_tries")), failures)
    need(row_of(body("refused"), "openrouter").get("key") == "refused", "the refused key's row: %s" %
         row_of(body("refused"), "openrouter"), failures)
    need(row_of(body("refused_health"), "openrouter").get("state") == "Failing: it refused its key",
         "Health on a refused key: %s" % body("refused_health"), failures)
    need(body("refused_status").get("sentence") ==
         "openrouter refused its key: paste it again on its row on Providers.",
         "the lead on a refused key: %s" % body("refused_status"), failures)
    need(body("refused_choices").get("choices") == [], "a refused provider is a job's choice: %s" %
         body("refused_choices"), failures)
    # A card's status, as its provider fails and recovers: no key yet, failing on the refused key, ready on the right one.
    need(body("jobs_no_key").get("dialogue") == {"word": "No key yet", "tone": "glint"} and
         body("jobs_no_key").get("memory") == {"word": "No key yet", "tone": "glint"},
         "the cards' status with no key: %s" % body("jobs_no_key"), failures)
    need(body("jobs_failing").get("dialogue") == {"word": "Failing", "tone": "rot"} and
         body("jobs_failing").get("selftest") == {"word": "Failing", "tone": "rot"},
         "the cards' status on a refused key: %s" % body("jobs_failing"), failures)
    need(all(body("jobs_ready").get(slot) == {"word": "Ready", "tone": "moss"} for slot in
             ("dialogue", "bark_live", "bark_pool", "memory", "selftest")),
         "the cards' status with the right key: %s" % body("jobs_ready"), failures)
    need(d.get("set_key", {}).get("ok") is True and "body" not in d.get("set_key", {}),
         "the key set on its row: %s" % d.get("set_key"), failures)
    need(row_of(body("with_key"), "openrouter").get("key") == "set", "the key's status", failures)
    need(d.get("run", {}).get("ok") is True, "selftest.run: %s after %s tries" % (d.get("run"), d.get("run_tries")),
         failures)
    st = body("selftest")
    need(re.match(r"^(Quick|Acceptable|Slow): the first words of a reply come in about [0-9.]+ s\.",
                  st.get("sentence", "")) is not None, "the verdict: %s" % st.get("sentence"), failures)
    need([c.get("state") for c in st.get("checks", [])] == ["pass", "pass", "pass"],
         "the self-test's steps: %s" % st.get("checks"), failures)
    figures = {i.get("label"): i.get("value") for i in st.get("items", [])}
    need(figures.get("Model") == "invented-talker-small" and figures.get("Provider") == "openrouter" and
         "First token" in figures and figures.get("JSON check") == "Passed", "the figures: %s" % figures, failures)
    need(d.get("list", {}).get("ok") is True, "models.list: %s" % d.get("list"), failures)
    checks = body("listed").get("checks", [])
    need(len(checks) == 1 and checks[0].get("state") == "pass" and
         checks[0].get("detail") == "Lists 3 models, every job's model among them.", "the model list: %s" % checks,
         failures)
    # The form (D195): EldenNet's own checks, in words; then the provider of the case's own, local on this PC.
    need(d.get("own_plain", {}).get("error") == "invalid_value" and
         d.get("own_plain", {}).get("detail", "").startswith("http:// goes only to this PC"),
         "a public address over http://: %s" % d.get("own_plain"), failures)
    need(d.get("own_password", {}).get("error") == "invalid_value" and
         "no name or password" in d.get("own_password", {}).get("detail", ""),
         "an address with a password: %s" % d.get("own_password"), failures)
    need(d.get("own", {}).get("ok") is True, "the provider of one's own: %s" % d.get("own"), failures)
    own = row_of(body("own_added"), AI_OWN)
    need(own.get("kind") == "Local" and own.get("key") == "none" and
         own.get("address") == "http://127.0.0.1:%d/own/v1" % run["server"].port, "its row: %s" % own, failures)
    need(d.get("own_again", {}).get("detail") == "A provider is already called that: choose another name.",
         "the same name again: %s" % d.get("own_again"), failures)
    need(d.get("own_key", {}).get("error") == "not_now" and
         d.get("own_key", {}).get("detail", "").startswith("This provider takes no key."),
         "a key on a row that needs none: %s" % d.get("own_key"), failures)
    need(row_of(body("own_none"), AI_OWN).get("key") == "none", "its row after the list: %s" %
         row_of(body("own_none"), AI_OWN), failures)
    checks = body("listed_two").get("checks", [])
    need(d.get("list_two", {}).get("ok") is True and [c.get("state") for c in checks] == ["pass", "pass"] and
         [c.get("label") for c in checks] == ["openrouter", AI_OWN], "the second list: %s" % checks, failures)
    # A job's provider (a picker of the providers): one not in the file refused; one whose list lacks the job's model
    # waits for a model of its list, which the job's picker offers (D193); one it did not list refused; the pick; the
    # changed mark on that job's rows alone (D198).
    need([c.get("value") for c in body("provider_choices").get("choices", [])] == ["openrouter", AI_OWN],
         "the providers' choices: %s" % body("provider_choices"), failures)
    need(d.get("provider_bad", {}).get("error") == "invalid_value" and
         "not in your providers file" in d.get("provider_bad", {}).get("detail", ""),
         "a provider not in the file: %s" % d.get("provider_bad"), failures)
    need(d.get("provider", {}).get("ok") is True, "Prepared lines' provider: %s" % d.get("provider"), failures)
    need([c.get("value") for c in body("choices").get("choices", [])] == AI_OWN_MODELS,
         "Prepared lines' choices: %s" % body("choices"), failures)
    waiting = [k for k in body("waiting").get("changed", []) if not k.startswith("debug.")]
    need(waiting == [], "a provider waiting for its model counts as changed: %s" % waiting, failures)
    need(d.get("pick_bad", {}).get("error") == "invalid_value" and
         "does not list that model" in d.get("pick_bad", {}).get("detail", ""), "a model not listed: %s" %
         d.get("pick_bad"), failures)
    need(d.get("pick", {}).get("ok") is True, "the pick: %s" % d.get("pick"), failures)
    need(body("picked").get("slots.bark_pool.provider") == AI_OWN and
         body("picked").get("slots.bark_pool.model") == AI_OWN_MODELS[0],
         "Prepared lines after the pick: %s" % body("picked"), failures)
    need("changed_one" in d, "the changed mark after the pick", failures)
    # GraceUI's server refuses a destructive row command unconfirmed before EldenNet hears it (D192); EldenNet refuses
    # a provider a job uses, saying which.
    need(d.get("remove_unconfirmed", {}).get("error") == "confirm_required",
         "Remove unconfirmed: %s" % d.get("remove_unconfirmed"), failures)
    need(d.get("remove_used", {}).get("error") == "not_now" and
         "use it" in d.get("remove_used", {}).get("detail", ""), "Remove of a provider in use: %s" %
         d.get("remove_used"), failures)
    need(d.get("remove_own_used", {}).get("detail", "").startswith("Prepared lines uses it:"),
         "Remove of the server Prepared lines uses: %s" % d.get("remove_own_used"), failures)
    # Conversation's fallback (its card's last two rows): refused for a job that borrows; its provider waiting, its
    # picker that provider's models; the pick; the changed mark on its fallback rows alone, beside Prepared lines'.
    need(d.get("fallback_borrowed", {}).get("error") == "not_now" and
         d.get("fallback_borrowed", {}).get("detail") ==
         "Self-test uses Conversation's models: choose a model of its own first.",
         "a fallback for a job that borrows: %s" % d.get("fallback_borrowed"), failures)
    need(d.get("fallback_provider", {}).get("ok") is True, "Conversation's fallback provider: %s" %
         d.get("fallback_provider"), failures)
    need([c.get("value") for c in body("fallback_choices").get("choices", [])] == AI_OWN_MODELS,
         "the fallback's choices: %s" % body("fallback_choices"), failures)
    need(d.get("fallback", {}).get("ok") is True, "Conversation's fallback: %s" % d.get("fallback"), failures)
    need("changed_fallback" in d, "the changed mark after the fallback's pick", failures)
    need(d.get("secrets_written") is True, "secrets.toml never held the key", failures)
    # The menu's turn, as the browser saw it: Memory on the server of the case's own, the last of its models.
    picked = body("menu_picked").get("values", {})
    need(picked.get("slots.memory.provider") == AI_OWN and picked.get("slots.memory.model") == AI_OWN_MODELS[1],
         "the menu's pick: %s" % {k: v for k, v in picked.items() if k.startswith("slots.memory.")}, failures)
    menu_run = body("selftest_menu")
    need(len(menu_run.get("rows", [])) == 1, "the menu's run: earlier runs %s" % menu_run.get("rows"), failures)
    need("changed_two" in d, "the changed mark after the menu's pick", failures)
    for job in ("slots.bark_pool.model", "slots.memory.provider"):
        need(d.get("reset_" + job, {}).get("ok") is True, "the reset of %s: %s" % (job, d.get("reset_" + job)),
             failures)
    need(d.get("fallback_default", {}).get("ok") is True, "the fallback's Default: %s" % d.get("fallback_default"),
         failures)
    need("changed_none" in d, "a row still changed after the resets", failures)
    back = body("reset")
    need(back.get("slots.memory.model") == "invented-keeper" and back.get("slots.memory.provider") == "openrouter" and
         back.get("slots.bark_pool.model") == "invented-talker-small" and
         back.get("slots.dialogue.fallback") == "invented-talker-large" and
         back.get("slots.dialogue.fallback_provider") == "openrouter",
         "the jobs after the resets: %s" % {k: v for k, v in back.items() if k.startswith("slots.")}, failures)
    need("budgets_reset" in d, "Prepared lines' card never said Same as Conversation again", failures)
    need(d.get("remove_own", {}).get("ok") is True and len(body("own_gone").get("rows", [])) == 1,
         "Remove of the server of the case's own: %s" % d.get("remove_own"), failures)
    need(d.get("preset", {}).get("ok") is True, "the NanoGPT preset: %s" % d.get("preset"), failures)
    need(len(body("two").get("rows", [])) == 2, "the preset's provider added", failures)
    need(d.get("remove", {}).get("ok") is True, "Remove: %s" % d.get("remove"), failures)
    left = body("one").get("rows", [{}])
    need(len(left) == 1 and left[0].get("id") == "nanogpt", "the provider left: %s" % left, failures)
    # The fake server: one list refused for the wrong key, every other keyed request with the right one; the server
    # of the case's own asked with no key; two runs (the second's JSON step on Memory's model, then that server's) and
    # four lists.
    need(counts["refused"] == 1 and counts["unknown"] == 0 and counts["models"] >= 3 and counts["chats"] >= 2 and
         counts["json"] >= 1 and counts["open_models"] >= 2 and counts["open_chats"] >= 1,
         "the fake server answered %s" % counts, failures)
    # In game: F5's menu on Home; AI › Providers, which draws a row's key as Not needed, never a key's fragment; Down to
    # the server of the case's own's Remove, Enter twice, refused (a job uses it); Models, Down past each card's head and
    # rows to Memory's provider picker, →, Down to its model picker, ←, the cards drawing a chip and a live status
    # (D202); Self-test's Run self-test. The menu watched the pages' topics.
    failures += (want_grace(r, 3060, eldennet=1, server=1, spaces=1, menu_open=0) +
                 want_grace(r, 3180, menu_open=1, menu_page="home") +
                 want_grace(r, 3300, menu_open=1, menu_page="ai.providers", menu_focused="action:add",
                            shows="0100") +
                 want_grace(r, 3540, menu_open=1, menu_page="ai.providers",
                            menu_focused="row:providers/%s/remove" % AI_OWN) +
                 want_grace(r, 3720, menu_open=1, menu_page="ai.providers", menu_refusals=1) +
                 want_grace(r, 3840, menu_open=1, menu_page="ai.models", menu_focused="setting:ai.preset",
                            shows="0011") +
                 want_grace(r, 4440, menu_open=1, menu_page="ai.models",
                            menu_focused="setting:slots.memory.provider") +
                 want_grace(r, 4560, menu_open=1, menu_page="ai.models", menu_refusals=1) +
                 want_grace(r, 4740, menu_open=1, menu_page="ai.models", menu_focused="setting:slots.memory.model",
                            menu_refusals=1, shows="0011") +
                 want_grace(r, 4860, menu_open=1, menu_page="ai.selftest", menu_focused="action:run") +
                 want_grace(r, 5340, menu_open=1, menu_page="ai.selftest", menu_refusals=1, shows="0000") +
                 want_grace(r, 5460, menu_open=0))
    failures += check_lines(("ELDENNET", "connect", {"ok": 1, "outcome": "connected", "path": "main", "pages": 0}),
                            ("LAUNCHER", "end", {"kind": "clean"}))(r)
    passed = [e for e in r.events("ELDENNET", "selftest") if e.get("result") == "passed"]
    need(len(passed) == 2, "%d self-tests passed in the log, wanted 2" % len(passed), failures)
    for kv in ({"what": "command", "name": "providers.add", "result": "ok"},
               {"what": "command", "name": "providers.add", "result": "invalid_value"},
               {"what": "command", "name": "providers.key", "key": "openrouter", "result": "ok"},
               {"what": "command", "name": "providers.key", "key": AI_OWN, "result": "not_now"},
               {"what": "command", "name": "selftest.run", "result": "ok"},
               {"what": "command", "name": "models.list", "result": "ok"},
               {"what": "command", "name": "settings.set", "key": "slots.bark_pool.provider", "result": "invalid_value"},
               {"what": "command", "name": "settings.set", "key": "slots.bark_pool.provider", "result": "ok"},
               {"what": "command", "name": "settings.set", "key": "slots.bark_pool.model", "result": "invalid_value"},
               {"what": "command", "name": "settings.set", "key": "slots.memory.provider", "result": "ok"},
               {"what": "command", "name": "settings.set", "key": "slots.memory.model", "result": "ok"},
               {"what": "command", "name": "settings.set", "key": "slots.selftest.fallback", "result": "not_now"},
               {"what": "command", "name": "settings.set", "key": "slots.dialogue.fallback_provider", "result": "ok"},
               {"what": "command", "name": "settings.set", "key": "slots.dialogue.fallback", "result": "ok"},
               {"what": "command", "name": "settings.reset", "key": "slots.memory.provider", "result": "ok"},
               {"what": "command", "name": "providers.remove", "key": "openrouter", "result": "not_now"},
               {"what": "command", "name": "providers.remove", "key": AI_OWN, "result": "not_now"},
               {"what": "command", "name": "providers.remove", "key": AI_OWN, "result": "ok"},
               {"what": "command", "name": "providers.remove", "key": "openrouter", "result": "ok"},
               {"what": "watched", "topic": "selftest"},
               {"what": "watched", "topic": "ai.choices.memory"},
               {"what": "watched", "topic": "ai.choices.providers"},
               {"what": "watched", "topic": "space.state"}):
        failures += any_line_kv(r, "ELDENNET", "page", kv)
    failures += any_line(r, "ELDENNET", "models_list", providers=1, passed=0)
    failures += any_line(r, "ELDENNET", "models_list", providers=2, passed=2)
    failures += any_line(r, "ELDENNET", "ai_write", file="secrets", ok=1)
    failures += any_line(r, "ELDENNET", "ai_write", file="providers", ok=1)
    failures += any_line_kv(r, "ELDENNET", "ai_config", {"result": "missing"})
    for what in ("add_own", "key", "provider", "model_reset", "remove", "fallback", "fallback_reset"):
        failures += any_line_kv(r, "ELDENNET", "ai_change", {"what": what, "ok": "1"})
    # Each key is nowhere: no frame either view could have, no log or report line, no recording, no file left (the
    # removals took the right one out of secrets.toml, which the right one replaced the wrong one in), and a fragment
    # both hold is drawn nowhere in the menu (shows' first bit). The own provider's address and name reach no log
    # line but the configuration's ids.
    for secret in (key, wrong):
        need(not any(secret in text for text in d.get("received", [])), "a frame to the browser holds a key", failures)
        need(not any(secret in line for line in r.lines), "a log or report line holds a key", failures)
        need(secret not in r.out and secret not in r.err, "the run's output holds a key", failures)
        for folder in (r.paths["case"], r.paths["appdata"]):
            held = files_holding(folder, secret.encode("utf-8"))
            need(not held, "files hold a key: %s" % held, failures)
    need(not any("/own/v1" in line or "192.0.2.7" in line or "models.invalid" in line for line in r.lines),
         "a log line holds an address typed in the form", failures)
    need(bool(en_recordings(r)), "no recording to look in", failures)
    path = os.path.join(r.paths["appdata"], "EldenNet", "config", "providers.toml")
    try:
        with open(path, encoding="utf-8") as f:
            written = f.read()
    except OSError:
        written = ""
    need(re.search(r"""preset = ["']cloud-nanogpt["']""", written) and "nanogpt" in written and
         "openrouter" not in written and AI_OWN not in written and "slots" not in written,
         "providers.toml after the pages: %r" % written, failures)
    return failures + en_log_is_clean(r)


case("launcher-eldennet-graceui-ai", LGAME + ["--dll", "{eldennetdll}", "--console"] +
     game("--d3d12", "--stay-title", "--press-keys", AI_KEYS, "--grace-report", AI_REPORTS, frames=5520),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",),
     env=dict(EN_ENV, FAKEGAME_SHOWS="{text_ai_shows}"), timeout=300,
     prepare=install_eldennet(AI_SETTINGS, then=install_ai), during=browse_ai, check=check_graceui_ai,
     what="EldenNet's AI pages through GraceUI 0.2.12 against a fake model server on loopback: AI › Models as one "
          "card per job with its chip and live status, a provider added from its preset (No key yet), a wrong key on "
          "its row refused (Refused; the cards Failing) and the right one set (Ready), a server of the case's own "
          "on this PC from the form (Not needed, no key), the self-test and the model lists, a job given that server "
          "and a model from its list, Conversation's fallback on it, the changed mark on each card's own rows alone, "
          "refusals in words; from the menu a row's Remove refused, a job's provider and model from their pickers and "
          "the self-test; then the picks reset (a fallback by its picker's Default), no row changed, a new preset, and "
          "the unused providers removed; either key in no topic, log line, recording, report or file, and drawn "
          "nowhere in the menu")
case("launcher-eldennet-graceui", LGAME + ["--dll", "{eldennetdll}", "--console"] +
     game("--d3d12", "--press-f11-at", "420,600", "--press-pause-at", "540", "--grace-report", "360,480,660,900",
          "--grace-probe", VIEW_PROBE, frames=960),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=200,
     prepare=install_eldennet(EN_PANEL, then=install_graceui),
     check=check_graceui_main,
     what="GraceUI installed in the game's folder and loaded after EldenNet, by EldenNet's own look: the launcher's "
          "pre-flight, the connect on T1, the F11 view on Dear ImGui's core shown and hidden through GraceUI's hotkey "
          "and drawn on the game's frame, the world-state provider, and the Mark on GraceUI's hotkey once the "
          "launcher's console hands its ring over")
case("launcher-eldennet-graceui-events", LGAME + ["--dll", "{eldennetdll}", "--console"] +
     game("--d3d12", "--press-f11-at", "720,840", "--grace-report", "660,780,900", "--grace-probe", VIEW_PROBE,
          "--grace-probe", WORLD_PROBE, "--grace-probe", LIFE_PROBE, frames=960),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=200,
     prepare=install_eldennet(EN_PANEL, then=install_graceui),
     check=check_graceui_events,
     what="Augur's events in F11's view: the fake game's world and the lifecycle raise Augur's events, and the view "
          "opened lists them, the newest first, each after a square of its source's colour, which two more probes "
          "count on the game's frame")
case("launcher-eldennet-graceui-before", LGAME + ["--also", "{gracedll}", "--dll", "{eldennetdll}", "--console"] +
     game("--d3d12", "--press-f11-at", "360", "--grace-report", "300,460", "--grace-probe", VIEW_PROBE, frames=540),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=200,
     prepare=install_eldennet(EN_PANEL, gal_change=unknown_build_for_dll, then=install_graceui),
     check=check_graceui_before,
     what="GraceUI loaded before EldenNet (the launcher's --also), found by its name, on the banner path (a build the "
          "DLL's gal/ does not know): ConnectB, the banner as one notice under EldenNet's key, and F11's view")
case("launcher-eldennet-graceui-unbound", LGAME + ["--also", "{gracedll}", "--dll", "{eldennetdll}", "--console"] +
     game("--d3d12", "--grace-take-pause", "--press-pause-at", "540", "--grace-report", "360", frames=720),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=200,
     prepare=install_eldennet(EN_PANEL, then=install_graceui),
     check=check_graceui_unbound,
     what="another mod holding Pause in GraceUI (the fake game, connected at boot to the GraceUI the launcher loaded): "
          "EldenNet's Mark registers unbound (GraceUI D164), the launcher is never asked for Pause and keeps it, and "
          "the Pause pressed is its own Mark")
case("launcher-eldennet-graceui-anticheat", LGAME + ["--also", "{easyanticheat_eos}", "--dll", "{eldennetdll}",
                                                     "--console"] +
     game("--d3d12", "--grace-report", "300", frames=420),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=dict(EN_ENV, VEHPROBE_MODE="off"),
     timeout=200, prepare=install_eldennet(EN_PANEL, copies=(("EasyAntiCheat_EOS.dll", "vehprobe"),),
                                           then=install_graceui),
     check=check_graceui_anticheat,
     what="a simulated anti-cheat module with GraceUI installed: GraceUI's own check puts it in the notice-only mode, "
          "connect answers inactive (anti-cheat), and the banner goes through notice alone")
case("launcher-eldennet-graceui-spike-s0e", LGAME + ["--dll", "{eldennetdll}", "--console", "--spike", "s0e"] +
     game("--d3d12", "--press-f11-at", "480", "--grace-report", "420,600", "--grace-probe", VIEW_PROBE, frames=720),
     program="{launcher}", needs=GRACE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=200,
     prepare=install_eldennet(EN_PANEL, then=install_graceui),
     check=lambda r: (check_en_spike("s0e", grace=True)(r) +
                      # The spike run's banner: one notice under EldenNet's key, in the banners' tone; F11's view on it.
                      want_grace(r, 420, found=1, eldennet=1, notice_only=0,
                                 banner="Spike_run:_EldenNet_makes_no_call_into_the_game", banner_tone=1, badge="none",
                                 shown=0) +
                      want_grace(r, 600, shown=1, refused=0, panel_frames=(1, 1e9), probe=(VIEW_AREA // 2, VIEW_AREA),
                                 banner="Spike_run:_EldenNet_makes_no_call_into_the_game") +
                      any_line(r, "ELDENNET", "view", presses=1, open=1)),
     what="S0-E with GraceUI installed: the connect on T1, the spike_run banner as GraceUI's notice, F11's view, and "
          "still no call into the game")


# --- The owner's package for S0-E and S4 (tools/spikepkg's s0e-s4 kind), each run as its .cmd file runs it -------------
# `--set pkg=<the unzipped EldenNet-Spikes folder>` (with `engine`, `leads` and `gallayout` for gal/; CMake never sets
# `pkg`, so these run by hand, before the package leaves). Each case copies the package into its own folder, with gal/
# made for the fake game in place of the package's (which is the owner's game's), installs the package's GraceUI folder
# beside the fake game and writes settings.toml, both as README-SPIKES.txt asks the owner to, adds invented saves for
# the backup, and runs the package's own launcher from the package's folder with the run's options exactly as its .cmd
# file holds them (checked against the file), and the tests' own (no dialogs, the case's %APPDATA%, the fake game, no
# Steam check). Each must end cleanly, make no call (the hub's live count, WHY§250), leave grace-spike's report in the
# run's folder, and, where EldenNet runs, its spike_run banner as GraceUI's notice.
PKG_RUNS = {
    "A1": ("s0e", "suspended", ["GraceUI\\graceui.dll", "grace-spike.dll", "grace-standin.dll"]),
    "A2": ("s0e", "window", ["GraceUI\\graceui.dll", "grace-spike.dll", "grace-standin.dll"]),
    "A3": ("s0e", "suspended", ["grace-standin.dll", "grace-spike.dll"]),
    "A4": ("s0e", "window", ["grace-standin.dll", "grace-spike.dll"]),
    "A0": ("s0e_alone", "suspended", ["GraceUI\\graceui.dll", "grace-spike.dll", "grace-standin.dll"]),
    "B": ("s4", "suspended", ["grace-spike.dll"]),
}
# README-SPIKES.txt's two PowerShell lines write this (Set-Content: one line each, CRLF): F11's view on.
OWNER_SETTINGS = "schema = 2\r\ndebug = { panel = true }\r\n"
PKG_NEEDS = ("engine", "leads", "gallayout", "pkg")
SPIKE_BANNER = "Spike_run:_EldenNet_makes_no_call_into_the_game"


def pkg_options(run):
    spike, at, also = PKG_RUNS[run]
    out = ["--console", "--spike", spike, "--inject-at", at]
    for dll in also:
        out += ["--also", dll]
    return out


def install_package(paths):
    """The unzipped package, copied into the case's folder with the fake game's gal/ and leads in place of its own; its
    GraceUI folder beside the fake game; settings.toml as the README writes it; invented saves."""
    pkg = os.path.join(paths["case"], "EldenNet-Spikes")
    shutil.copytree(paths["pkg"], pkg)
    shutil.rmtree(os.path.join(pkg, "gal"))
    shutil.copytree(paths["galdir"], os.path.join(pkg, "gal"))
    shutil.copy2(os.path.join(paths["work"], "gal-leads.toml"), os.path.join(pkg, "gal", "leads.toml"))
    shutil.copytree(os.path.join(pkg, "GraceUI"), os.path.join(paths["gamedir"], "GraceUI"))
    cfg = os.path.join(paths["appdata"], "EldenNet", "config")
    os.makedirs(cfg)
    with open(os.path.join(cfg, "settings.toml"), "w", newline="") as f:
        f.write(OWNER_SETTINGS)
    with_saves(paths)
    paths["pkgdir"] = pkg
    paths["pkglauncher"] = os.path.join(pkg, "eldennet-launcher.exe")


def check_pkg_cmd(run):
    """The run's .cmd file holds this case's options, from the package's own folder, and a pause at its end."""
    def check(r):
        with open(os.path.join(r.paths["pkg"], "run-%s.cmd" % run), "rb") as f:
            text = f.read().decode("utf-8")
        lines = text.split("\r\n")
        failures = []
        need(text.endswith("\r\n") and "\n" not in text.replace("\r\n", ""), "run-%s.cmd: not CRLF lines" % run,
             failures)
        need('cd /d "%~dp0"' in lines, "run-%s.cmd: no cd to its own folder" % run, failures)
        need("eldennet-launcher.exe " + " ".join(pkg_options(run)) in lines,
             "run-%s.cmd: the launcher's line differs: %s" % (run, [l for l in lines if "launcher.exe" in l]), failures)
        need([l for l in lines if l.strip()][-1] == "pause", "run-%s.cmd: no pause at its end" % run, failures)
        need(":\\" not in text and "%APPDATA%" not in text, "run-%s.cmd names a path" % run, failures)
        return failures
    return check


def check_grace_report(word, standin):
    """grace-spike's report in the run's folder: schema 1 lines, the header naming the spike and its connect, state
    lines, GraceUI hooked once and drawing, the mods connected (EldenNet's among them where it runs), the stand-in
    patched and its Present called where it is loaded; nothing like a path in it. (The fake game's own GraceUI report
    reads GraceUI's test state with graceui_test.h as vendored; a package's GraceUI older than the vendored header
    refuses that size, so its counters then read 0: the observer's report has them, from GraceUI's own build.)"""
    def check(r):
        failures = []
        path = os.path.join(r.paths["appdata"], "EldenNet", "spikes", "%s-1" % word, "graceui-spike.jsonl")
        if not os.path.exists(path):
            return ["no graceui-spike.jsonl in %s-1" % word]
        with open(path, encoding="utf-8") as f:
            text = f.read()
        lines = [json.loads(l) for l in text.splitlines() if l.strip()]
        need(all(l.get("report") == "graceui_spike" and l.get("schema") == 1 for l in lines),
             "a line of the report is not graceui_spike schema 1", failures)
        headers = [l for l in lines if l.get("k") == "header"]
        states = [l for l in lines if l.get("k") == "state"]
        need(len(headers) == 1 and headers[0].get("spike") == word and headers[0].get("connect") == "ok",
             "the report's header: %s" % headers[:1], failures)
        need(len(states) >= 2, "%d state lines" % len(states), failures)
        if states:
            last = states[-1]
            need(last.get("host") == "hooked" and last.get("install_runs") == 1 and last.get("frames_drawn", 0) > 0
                 and not any(st.get("render_off") for st in states),
                 "GraceUI: %s" % {k: last.get(k) for k in ("host", "install_runs", "frames_drawn", "render_off")},
                 failures)
            mods = max(st.get("consumers", 0) for st in states)
            alone = word == "s0e_alone"
            need(mods == (1 if alone else 2), "%d mods connected at most" % mods, failures)
            need(any(st.get("eldennet") == "present" for st in states) != alone, "EldenNet's presence: %s" %
                 sorted(set(st.get("eldennet") for st in states)), failures)
            if standin:
                need(last.get("standin") == "patched" and last.get("standin_presents", 0) > 0,
                     "the stand-in: %s, %s presents" % (last.get("standin"), last.get("standin_presents")), failures)
        need(":\\" not in text and "\\\\" not in text and "/" not in text, "the report holds a path", failures)
        return failures
    return check


def check_pkg_run(run):
    word, at, also = PKG_RUNS[run]
    names = [d.split("\\")[-1] for d in also] + ([] if word == "s0e_alone" else ["eldennet.dll"])

    def check(r):
        failures = check_pkg_cmd(run)(r) + check_spike_folder(word)(r)
        # GraceUI keeps its settings in its folder's user\ (its D179): in the case's copies, never in the package given.
        need(not os.path.exists(os.path.join(r.paths["pkg"], "GraceUI", "user")), "a user folder in the package's "
             "GraceUI folder", failures)
        failures += check_order(*["LAUNCHER inject dll=%s ok=1" % n for n in names])(r)
        failures += any_line(r, "LAUNCHER", "started", ok=1, mode=at, also=len(also),
                             eldennet=0 if word == "s0e_alone" else 1)
        failures += any_line(r, "LAUNCHER", "backup", folders=2, made=2)
        failures += any_line(r, "LAUNCHER", "preflight", graceui="ok") if word != "s0e_alone" else []
        failures += check_grace_report(word, "grace-standin.dll" in also)(r)
        if word == "s0e_alone":
            failures += (check_lines(("LAUNCHER", "preflight", {"dll": "none"}),
                                     ("LAUNCHER", "run", {"outcome": "game_ended"}),
                                     ("LAUNCHER", "end", {"kind": "clean", "init": "none"}))(r) +
                         (["an ELDENNET line"] if any(l.startswith("ELDENNET ") for l in r.lines) else []) +
                         no_call(r) + want_grace(r, 900, found=1, eldennet=0, banner="none"))
        else:
            failures += check_en_spike(word, grace=True)(r)
            how = "name" if "GraceUI\\graceui.dll" in also else "path"
            failures += want(r, "ELDENNET", "connect", ok=1, how=how)
            # The banner as GraceUI's notice under EldenNet's key, in the banners' tone (the notices' export reads).
            failures += want_grace(r, 900, found=1, eldennet=1, banner=SPIKE_BANNER, banner_tone=1,
                                   probe=(0, VIEW_AREA // 100))
            if word == "s0e":
                # F11 (frames 960 and 1140), as the README's step 4: the view's ground read back from the game's frame
                # beside the stand-in, then gone.
                failures += want_grace(r, 1080, probe=(VIEW_AREA // 2, VIEW_AREA))
                failures += want_grace(r, 1260, probe=(0, VIEW_AREA // 100))
                failures += any_line(r, "ELDENNET", "view", presses=1, open=1)
                failures += any_line(r, "ELDENNET", "view", presses=1, open=0)
        return failures
    return check


PKG_WHAT = {
    "A1": "S0-E, GraceUI loaded by the launcher before EldenNet, early",
    "A2": "S0-E, GraceUI loaded by the launcher before EldenNet, late (once the window shows)",
    "A3": "S0-E, GraceUI loaded by EldenNet from the game's folder, early, the stand-in first",
    "A4": "S0-E, GraceUI loaded by EldenNet from the game's folder, late, the stand-in first",
    "A0": "S0-E's baseline: GraceUI, the observer and the stand-in alone, no eldennet.dll",
    "B": "S4: the observer alone loaded by the launcher, GraceUI by EldenNet",
}
for _run in PKG_RUNS:
    _extra = ["--press-f11-at", "960,1140"] if PKG_RUNS[_run][0] == "s0e" else []
    case("launcher-package-spikes-%s" % _run.lower(),
         ["--no-dialogs", "--appdata", "{appdata}", "--no-steam-check", "--game", "{game}"] + pkg_options(_run) +
         game("--d3d12", *_extra, "--grace-report", "900,1080,1260", "--grace-probe", VIEW_PROBE, frames=1380),
         program="{pkglauncher}", cwd="{pkgdir}", needs=PKG_NEEDS, toolchains=("clang-cl",),
         env=dict(EN_ENV, GRACE_SPIKE_STATE_MS="1000"), timeout=240, prepare=install_package,
         check=check_pkg_run(_run),
         what="the owner's package, run %s (run-%s.cmd): %s" % (_run, _run, PKG_WHAT[_run]))

def check_r1_callee(p, task, gap=False):
    """Spike.6 to spike.9 (WHY§227, WHY§235, WHY§240, WHY§248): the callee's summary, from the live image, shows the fake
    registration as the wrapper of R1 run 7's shape it is: rcx loaded from the rack's static (none of its arguments),
    tested for null and branched on past its two calls (which make the rack and store it there), and, where both ways
    meet, a tail jump to the tail function with the rack in rcx, the group in edx, the task in r8 and the hub in r9. One
    level deeper the tail function bounds the group, tests the rack's shelf for it (loaded from [entry rcx + group*8 +
    the shelves' offset]) and, as in R1 run 8, moves the task into rdx and jumps directly to the group's own function,
    a tail jump past its decoded code; one more level (from the code, so with the gap too), that function with the
    entry and the task followed in (check_r1_group). Every summary's instructions are given as forms, the padding after
    the tail function's jump unreached."""
    failures = []
    c = p.get("callee_code")
    if c is None:
        return ["no callee_code in r1-probe.json"]
    task_reg, instance_reg, group_reg = p["register"]["registers"]
    rack = task["register_rack"]
    callee = p["register"]["callee"]

    def passed(k):
        return {(a["reg"], a["from"]) for a in k.get("pass", []) if not a["first_word"]}

    def src(k, reg):
        return next((x["source"] for x in k.get("pass_src", []) if x["reg"] == reg), {})

    def same(a, b):  # two sources, their loads' offsets aside
        drop = lambda v: {k: (drop(x) if isinstance(x, dict) else x) for k, x in v.items() if k != "load_at"}
        return drop(a) == drop(b)

    need(c["read"] and not c["cut"] and not c["undecodable"] and c["bytes"] == c["extent"] > 0,
         "the callee's bytes: %s" % {k: c.get(k) for k in ("read", "cut", "undecodable", "bytes", "extent")}, failures)
    calls = [k for k in c["call_sites"] if k["form"] == "direct"]
    tails = [k for k in c["call_sites"] if k["form"] == "tail"]
    need(c["call_count"] == 2 and [k.get("target") for k in calls] == [rack["alloc_rva"], rack["build_rva"]] and
         c["first_call"] == calls[0]["offset"], "the wrapper's calls: %s" % c["call_sites"], failures)
    need(len(tails) == 1 and tails[0].get("target") == rack["tail_rva"] and tails[0]["reached"],
         "the wrapper's tail jump: %s" % c["call_sites"], failures)
    need(c["branch_count"] == 1 and not c["returns_early"] and "modelled_to" not in c,
         "the wrapper's branches: %s, returns early %s, modelled to %s" %
         (c["branches"], c["returns_early"], c.get("modelled_to")), failures)
    guard = c.get("guard", [])
    ops = guard[0]["operands"] if len(guard) == 1 else []
    slot = {"src": "rip", "target": rack["slot_rva"], "width": 8}
    need(len(guard) == 1 and guard[0]["op"] == "test" and guard[0]["cond"] == "ne" and guard[0].get("width") == 8 and
         len(calls) == 2 and guard[0]["to"] > calls[1]["offset"] and len(ops) == 2 and
         all(o["form"] == "register" and o.get("reg") == 1 and "from" not in o and
             same(o.get("source", {}), slot) for o in ops), "the wrapper's guard: %s" % guard, failures)
    need(tails and {(2, group_reg), (8, task_reg), (9, instance_reg)} <= passed(tails[0]) and
         same(src(tails[0], 1), slot), "the arguments into the tail jump: %s" % (tails[0] if tails else None), failures)
    forms = c.get("forms", [])
    rips = [f for f in forms if f["class"] == "mov" and any(o["form"] == "rip" and o.get("target") == rack["slot_rva"]
                                                            and o.get("section") == "data" for o in f["operands"])]
    need(len(forms) == c["instructions"] and not any(f.get("unreached") for f in forms) and len(rips) == 2 and
         forms[-1]["class"] == "jmp" and forms[-1].get("to") == rack["tail_rva"] - callee and
         [f["offset"] for f in forms if f["class"] == "call"] == [k["offset"] for k in calls],
         "the wrapper's forms: %s" % forms, failures)
    inner = {(i["rva"], i.get("depth", 1)): i for i in c.get("inner", [])}
    tail = inner.get((rack["tail_rva"], 1), {})
    need((rack["alloc_rva"], 1) in inner and (rack["build_rva"], 1) in inner and tail and tail["tail"] and tails and
         tail["site"] == tails[0]["offset"], "the inner summaries: %s" %
         [(i["rva"], i["site"], i["tail"], i.get("depth")) for i in c.get("inner", [])], failures)
    tg = tail.get("guard", [])
    entry = tg[1]["operands"][0].get("source", {}) if len(tg) == 2 and tg[1]["operands"] else {}
    need(tail.get("extent") == 0 and tail.get("returns_early") and tail.get("call_count") == 0 and
         tail.get("branch_count") == 2 and len(tg) == 2 and tg[0]["op"] == "cmp" and tg[0]["cond"] == "a" and
         tg[0]["operands"][0].get("from") == group_reg and tg[0]["operands"][1].get("imm") == rack["bound"] and
         tg[1]["op"] == "test" and tg[1].get("width") == 8, "the tail function's guards: %s" % tg, failures)
    need(entry.get("src") == "memory" and entry.get("base_src") == {"src": "entry", "reg": 1} and
         entry.get("index_src") == {"src": "argument", "from": group_reg} and entry.get("scale") == 8 and
         entry.get("disp") == rack["shelves_off"] and entry.get("width") == 8,
         "the tail function's tested value: %s" % entry, failures)
    jumps = [k for k in tail.get("call_sites", []) if k["form"] == "tail"]
    need(len(jumps) == 1 and jumps[0]["reached"] and jumps[0].get("target") == rack["take_rva"] and
         {(2, task_reg), (8, task_reg), (9, instance_reg)} <= passed(jumps[0]) and same(src(jumps[0], 1), entry) and
         not [k for k in tail.get("call_sites", []) if k["form"] != "tail"],
         "the tail function's direct jump: %s" % tail.get("call_sites"), failures)
    tforms = tail.get("forms", [])
    need(len(tforms) == tail.get("instructions") and [f["class"] for f in tforms if f.get("unreached")] ==
         ["int3", "int3"] and tforms[-1]["class"] == "ret" and
         any(f["class"] == "jmp" and f.get("to") == rack["take_rva"] - rack["tail_rva"] for f in tforms),
         "the tail function's forms: %s" % tforms, failures)
    return failures + check_r1_group(p, task, jumps[0] if jumps else {}, entry)


def check_r1_group(p, task, jump, entry):
    """Spike.9 and spike.10 (WHY§248, WHY§289): the group's own function, the target of the tail function's direct jump,
    summarised with the group's entry in rcx and our task in rdx, of R1 run 9's shape: it reads nothing of the task,
    reads the shelf's word (passed on in r8), and tail-jumps to the stub with G, loaded from its slot, in rcx and the
    task in rdx (its one call, the ledger's lazy make, on the path where the slot is null). One level further the stub,
    8 bytes: r9 zeroed, G and the word carried, a jump to G's method at the same depth (check_r1_g_method)."""
    failures = []
    c = p["callee_code"]
    task_reg, _, _ = p["register"]["registers"]
    rack = task["register_rack"]
    inner = {(i["rva"], i.get("depth", 1)): i for i in c.get("inner", [])}
    g = inner.get((rack["take_rva"], 2), {})
    entry_src = {x["reg"]: x for x in g.get("entry_src", [])}
    need(g and g["tail"] and g.get("site") == jump.get("offset") and g.get("parent") == rack["tail_rva"] and
         g["read"] and g["extent"] > 0 and g["bytes"] == g["extent"] and "modelled_to" not in g and
         entry_src.get(1, {}).get("holds_entry") is True and entry_src.get(1, {}).get("source") == entry,
         "the group's function: %s" % {k: v for k, v in g.items() if k not in ("forms", "args", "guard")}, failures)
    need("task_read_count" not in g and "task_store_count" not in g, "the group's function reads the task: %s" %
         {k: g.get(k) for k in ("task_read_count", "task_reads", "task_store_count")}, failures)
    on_entry = {"src": "entry", "reg": 1}
    words = [r for r in g.get("entry_reads", []) if r["disp"] == rack["word_off"]]
    need(g.get("entry_read_count") == 1 and len(words) == 1 and words[0]["width"] == 4 and
         words[0]["base_src"] == on_entry and
         [(u["use"], u.get("reg"), u.get("target")) for u in words[0]["uses"]] == [("passed", 8, rack["stub_rva"])],
         "the entry's word, read and passed on: %s" % g.get("entry_reads"), failures)
    tails = [k for k in g.get("call_sites", []) if k["form"] == "tail"]
    calls = [k for k in g.get("call_sites", []) if k["form"] == "direct"]
    srcs = {x["reg"]: x["source"] for x in (tails[0].get("pass_src", []) if tails else [])}
    rcx = {k: v for k, v in srcs.get(1, {}).items() if k != "load_at"}
    r8 = {k: v for k, v in srcs.get(8, {}).items() if k != "load_at"}
    need(len(tails) == 1 and tails[0]["reached"] and tails[0].get("target") == rack["stub_rva"] and
         (2, task_reg) in {(a["reg"], a["from"]) for a in tails[0]["pass"]} and
         rcx == {"src": "rip", "target": rack["ledger_slot_rva"], "width": 8} and
         r8.get("src") == "memory" and r8.get("base_src") == on_entry and r8.get("disp") == rack["word_off"] and
         r8.get("width") == 4, "the group's function's jump to the stub: %s" % tails, failures)
    need(len(calls) == 1 and calls[0].get("target") == rack["missing_rva"], "the group's function's call: %s" % calls,
         failures)
    stub = inner.get((rack["stub_rva"], 3), {})
    sentry = {x["reg"]: x for x in stub.get("entry_src", [])}
    jumps = [k for k in stub.get("call_sites", []) if k["form"] == "tail"]
    need(stub and stub["tail"] and stub.get("parent") == rack["take_rva"] and tails and
         stub.get("site") == tails[0]["offset"] and stub["bytes"] == 8 and stub["instructions"] == 2 and
         sentry.get(1, {}).get("holds_g") is True and sentry.get(8, {}).get("holds_word") is True and
         stub.get("word_use_count") == 1 and len(jumps) == 1 and jumps[0].get("target") == rack["admit_rva"],
         "the stub: %s" % {k: v for k, v in stub.items() if k not in ("args", "guard")}, failures)
    return failures + check_r1_g_method(p, task, tails[0]["offset"] if tails else None)


def check_r1_g_method(p, task, site):
    """Spike.10 and spike.11 (WHY§289, WHY§293): G's method, reached through the stub at the same depth (3), entered with
    G in rcx, the task in rdx, the entry's word in r8 and r9 zero, of R1 run 10's shape: it never reads the task, which
    it stores into the request it builds on its stack; it reads four fields of G (its open word and its deferred byte
    compared in place, its manager stored into the request, its queue passed in rcx to the queued path's call); the
    word is stored into the request, and still in r8 at the first two calls; its three callees are the now path's
    function (with the request's stack address in rcx), the allocator and the queue's push."""
    failures = []
    c = p["callee_code"]
    rack = task["register_rack"]
    inner = {(i["rva"], i.get("depth", 1)): i for i in c.get("inner", [])}
    m = inner.get((rack["admit_rva"], 3), {})
    mentry = {x["reg"]: x for x in m.get("entry_src", [])}
    need(m and m.get("stub") == rack["stub_rva"] and m.get("zeroed") == [9] and m.get("parent") == rack["take_rva"] and
         m.get("site") == site and m["read"] and m["extent"] > 0 and m["bytes"] == m["extent"] and
         "modelled_to" not in m and mentry.get(1, {}).get("holds_g") is True and
         mentry.get(8, {}).get("holds_word") is True and "parts" not in m,
         "G's method: %s" % {k: v for k, v in m.items() if k not in ("forms", "args", "guard")}, failures)
    need("task_read_count" not in m and m.get("task_store_count") == 1 and
         m.get("task_stores", [{}])[0].get("base") == 4 and
         m.get("task_stores", [{}])[0].get("disp") == rack["request_disp"] + rack["request_task_off"],
         "G's method's task: %s" % {k: m.get(k) for k in ("task_read_count", "task_stores")}, failures)
    on_g = {"src": "entry", "reg": 1}
    greads = {r["disp"]: r for r in m.get("g_reads", [])}
    opened = greads.get(rack["ledger_open_off"], {})
    defer = greads.get(rack["ledger_defer_off"], {})
    manager = greads.get(rack["ledger_manager_off"], {})
    queue = greads.get(rack["ledger_queue_off"], {})
    need(m.get("g_read_count") == 4 and all(r.get("base_src") == on_g for r in greads.values()) and
         opened.get("direct") and opened.get("width") == 8 and
         [(u["use"], u.get("imm")) for u in opened.get("uses", [])] == [("compared", 0)] and
         defer.get("direct") and defer.get("width") == 1 and
         [(u["use"], u.get("imm")) for u in defer.get("uses", [])] == [("compared", 0)] and
         [u["use"] for u in manager.get("uses", [])] == ["stored"] and
         [(u["use"], u.get("reg"), u.get("target")) for u in queue.get("uses", [])] ==
         [("passed", 1, rack["push_rva"])], "G's reads: %s" % m.get("g_reads"), failures)
    uses = [(u["use"], u.get("base"), u.get("disp"), u.get("width"), u.get("target")) for u in m.get("word_uses", [])]
    need(m.get("word_use_count") == len(uses) == 3 and
         uses[0] == ("stored", 4, rack["request_disp"] + rack["request_word_off"], 4, None) and
         [(u[0], u[4]) for u in uses[1:]] == [("passed", rack["now_rva"]), ("passed", rack["request_alloc_rva"])],
         "the entry's word's uses (stored into the request, still in r8 at the two calls): %s" % m.get("word_uses"),
         failures)
    calls = [k for k in m.get("call_sites", []) if k["form"] == "direct"]
    rcx = next((x["source"] for x in calls[0].get("pass_src", []) if x["reg"] == 1), {}) if calls else {}
    need([k.get("target") for k in calls] == [rack["now_rva"], rack["request_alloc_rva"], rack["push_rva"]] and
         rcx == {"src": "stack", "disp": rack["request_disp"]},
         "G's method's calls: %s" % m.get("call_sites"), failures)
    return failures


def check_r1_request(p, task):
    """Spike.11 (WHY§293): the request G's method builds, as R1 read it: its vtable (the fake game's FG::TaskRequest's,
    classed, no lead's) at the method's stack place, its five fields (the vtable, the task at +8, G's manager, the word,
    the zeroed byte), the function the method calls with it, and its three slots (5-byte jumps to the now path's
    function, the cancel and the word); each slot summarised at depth 4 with the request in rcx, the now path's
    function in two parts (its entry and, past its extent in no entry, its check) with the request's fields followed:
    the task, the word, the manager; the task's kind and state read in the second part, the manager's strict flag
    tested. Since spike.12 (WHY§296) R1 run 11's shape: the task's first word read on the other branch, the manager's
    map and list read (the head, the root, the nil byte; the list's end less its begin), the note, the hand-over (twice:
    the list's and the match's), the other branch's call and a call through the task's first word; one level further
    (5) the note, the hand-over entered with the map's value, the task, the word and the manager, and the other
    branch's call with the task; and past the hand-over (6) the work."""
    failures = []
    c = p["callee_code"]
    rack = task["register_rack"]
    task_reg = p["register"]["registers"][0]
    r = p.get("request", {})
    fields = [(f["offset"], f["width"], f["field"]) for f in r.get("fields", [])]
    cls = r.get("request_class", {})
    need(r.get("vtable") == rack["request_vtable_rva"] and r.get("disp") == rack["request_disp"] and
         r.get("task_field") == rack["request_task_off"] and r.get("now") == rack["now_rva"] and
         fields == [(0, 8, "vtable"), (rack["request_task_off"], 8, "task"), (rack["request_manager_off"], 8, "manager"),
                    (rack["request_word_off"], 4, "word"), (rack["request_flag_off"], 1, "zero")] and
         r.get("fields", [{}])[0].get("source") == {"src": "address", "target": rack["request_vtable_rva"]} and
         cls.get("vtable_known") and cls.get("classed") and cls.get("match") == "none" and
         r.get("slot_count") == 3 and r.get("slots") == rack["slot_rvas"],
         "the request: %s" % r, failures)
    inner = {(i["rva"], i.get("depth", 1)): i for i in c.get("inner", [])}
    now = inner.get((rack["now_rva"], 4), {})
    parts = [(x["rva"], x["how"]) for x in now.get("parts", [])]
    nentry = {x["reg"]: x for x in now.get("entry_src", [])}
    need(now and now.get("parent") == rack["admit_rva"] and now.get("extent", 0) > 0 and
         parts == [(rack["now_rva"], "primary"), (rack["check_rva"], "unlisted")] and
         now["parts"][0]["bytes"] == now["extent"] and now["bytes"] == sum(x["bytes"] for x in now["parts"]) and
         nentry.get(1, {}).get("holds_request") is True and now.get("request_slot") == 0 and
         not now.get("returns_early") and "modelled_to" not in now, "the now path's function: %s" %
         {k: v for k, v in now.items() if k not in ("forms", "args", "guard")}, failures)
    on_request = {"src": "entry", "reg": 1}
    rreads = {(x["disp"], x["width"]) for x in now.get("request_reads", []) if x["base_src"] == on_request}
    second = rack["check_rva"] - rack["now_rva"]
    treads = sorted((x["disp"], x["width"]) for x in now.get("task_reads", []) if x["offset"] >= second)
    first_word = [x for x in now.get("task_reads", []) if x["offset"] < second]
    mreads = now.get("manager_reads", [])
    flags = [x for x in mreads if x["disp"] == rack["manager_flags_off"] and x.get("direct")]
    heads = [x for x in mreads if x["disp"] == rack["map_head_off"] and x["width"] == 8]
    need({(rack["request_task_off"], 8), (rack["request_word_off"], 4), (rack["request_manager_off"], 8)} <= rreads and
         treads == sorted([(rack["task_state_off"], 4), (rack["task_kind_off"], 4)]) and
         [(x["disp"], x["width"], x.get("direct", False)) for x in first_word] == [(0, 8, False), (8, 8, True)] and
         len(flags) == 1 and [u["use"] for u in flags[0].get("uses", [])] == ["tested"] and len(heads) == 1 and
         {(rack["map_list_off"], 8), (rack["map_list_off"] + 8, 8), (8, 8), (rack["node_nil_off"], 1)} <=
         {(x["disp"], x["width"]) for x in mreads},
         "the now path's function's reads: request %s, task %s, manager %s" %
         (now.get("request_reads"), now.get("task_reads"), mreads), failures)
    calls = [k for k in now.get("call_sites", []) if k["form"] == "direct"]
    others = [k for k in now.get("call_sites", []) if k["form"] != "direct"]
    need([k.get("target") for k in calls] == [rack["note_rva"], rack["map_take_rva"], rack["map_take_rva"],
                                              rack["task_other_rva"]] and
         [k["form"] for k in others] == ["memory"] and others[0].get("vtable_src", {}).get("disp") ==
         rack["request_task_off"],
         "the now path's function's calls (the jumps back into its entry no tail calls): %s" % now.get("call_sites"),
         failures)
    forms = now.get("forms", [])
    need(len(forms) == now.get("instructions") and
         any(f["offset"] == rack["check_rva"] - rack["now_rva"] for f in forms),
         "the now path's function's forms, its second part among them: %s" % len(forms), failures)
    for i, (slot, target) in enumerate(zip(rack["slot_rvas"], rack["slot_targets"])):
        s = inner.get((slot, 4), {})
        jumps = [k for k in s.get("call_sites", []) if k["form"] == "tail"]
        sentry = {x["reg"]: x for x in s.get("entry_src", [])}
        need(s and s.get("request_slot") == i and s.get("parent") == rack["admit_rva"] and s["bytes"] == 5 and
             len(jumps) == 1 and jumps[0].get("target") == target and
             sentry.get(1, {}).get("holds_request") is True and
             sentry.get(1, {}).get("source") == {"src": "stack", "disp": rack["request_disp"]},
             "the request's slot %d: %s" % (i, {k: v for k, v in s.items() if k not in ("forms", "args", "guard")}),
             failures)
        if target == rack["now_rva"]:
            continue
        t = inner.get((target, 4), {})
        need(t and t.get("stub") == slot and t.get("request_slot") == i and t.get("request_read_count", 0) >= 1,
             "the request's slot %d's work: %s" % (i, {k: v for k, v in t.items() if k not in ("forms", "args")}),
             failures)
    cancel = inner.get((rack["cancel_rva"], 4), {})
    need([(x["disp"], x["width"]) for x in cancel.get("request_reads", [])] == [(rack["request_task_off"], 8)],
         "the cancel's request reads: %s" % cancel.get("request_reads"), failures)
    word = inner.get((rack["word_rva"], 4), {})
    need([(x["disp"], x["width"]) for x in word.get("request_reads", [])] == [(rack["request_word_off"], 4)],
         "the word's request reads: %s" % word.get("request_reads"), failures)
    take = inner.get((rack["map_take_rva"], 5), {})
    note = inner.get((rack["note_rva"], 5), {})
    other = inner.get((rack["task_other_rva"], 5), {})
    keep = inner.get((rack["keep_rva"], 6), {})
    tentry = {x["reg"]: x for x in take.get("entry_src", [])}
    held = tentry.get(2, {}).get("source", {})
    site = p.get("map", {}).get("map_call_site")
    oentry = {x["reg"]: x for x in other.get("entry_src", [])}
    need(take and note and other and keep and
         all(x.get("parent") == rack["now_rva"] for x in (take, note, other)) and
         keep.get("parent") == rack["map_take_rva"] and take.get("site") == site and site is not None and
         tentry.get(1, {}).get("holds_value") is True and held.get("src") == "memory" and
         held.get("disp") == rack["request_task_off"] and held.get("base_src") == on_request and
         tentry.get(8, {}).get("holds_word") is True and tentry.get(9, {}).get("holds_manager") is True and
         oentry.get(1, {}).get("source", {}).get("disp") == rack["request_task_off"],
         "one level further: %s; the hand-over's entry %s" %
         ([(i["rva"], i.get("depth"), i.get("parent")) for i in c.get("inner", [])], tentry), failures)
    vreads = [(x["disp"], x["width"], x.get("direct", False)) for x in take.get("value_reads", [])]
    need(take.get("value_read_count") == 2 and vreads == [(rack["bin_group_off"], 4, False),
                                                          (rack["bin_group_off"] + 4, 4, True)] and
         "forms" in take and take["forms"] and "forms" in now and now["forms"],
         "the hand-over's reads of the map's value: %s (forms kept: %s)" %
         (take.get("value_reads"), bool(take.get("forms"))), failures)
    del task_reg
    return failures


def check_r1_method(p, task, group, gap=False, gate=False, queued=False):
    """Spike.10 (WHY§289): G's method as R1 found it (the group's function's site, G's slot, the stub, the method, the
    entry's word at its offset), and the three numbers before and after the call: G on the heap, classed but no lead's;
    not the object the hub holds at +8; the object the shelf holds at +0x58 only with --task-gate (then not walked,
    held by a root, else walked as a fourth root); the stand-in group's word its own index, and every shelf's the
    same (with --table-gap, no word for the group)."""
    failures = []
    rack = task["register_rack"]
    m = p.get("method", {})
    shelves = [g["index"] for g in task["groups"] if not (gap and g["index"] == group)]
    need(m.get("slot") == rack["ledger_slot_rva"] and m.get("stub") == rack["stub_rva"] and
         m.get("rva") == rack["admit_rva"] and m.get("word_disp") == rack["word_off"] and m.get("width") == 4 and
         m.get("hub_off") == 8 and m.get("entry_off") == 0x58 and
         m.get("g_walk") == ("held_by_root" if gate else "walked") and "site" in m,
         "G's method: %s" % {k: v for k, v in m.items() if k not in ("before", "after")}, failures)
    for key in ("before", "after"):
        r = m.get(key, {})
        cls = r.get("g_class", {})
        need(r.get("read") and r.get("g", {}).get("kind") == "heap" and cls.get("vtable_known") and
             cls.get("classed") and cls.get("match") == "none" and cls.get("type_descriptor", 0) > 0 and
             r.get("at_hub") is False and r.get("words_read") == len(shelves) and
             r.get("own_index") == len(shelves) and r.get("distinct") == len(shelves),
             "G %s the call: %s" % (key, r), failures)
        if gap:
            need("at_entry" not in r and "word" not in r, "the group's entry %s the call, with no shelf: %s" % (key, r),
                 failures)
        else:
            need(r.get("at_entry") is gate and r.get("word") == group, "the entry's word %s the call: %s" % (key, r),
                 failures)
    need(m.get("before", {}).get("at_ms") == 0 and m.get("after", {}).get("at_ms", 0) >= 2000,
         "G's reads' times: %s" % [m.get(k, {}).get("at_ms") for k in ("before", "after")], failures)
    # Spike.11 (WHY§293): every shelf's word holds its own index in its low 16 bits too, and its high 16 bits are 0.
    for key in ("before", "after"):
        r = m.get(key, {})
        need(r.get("own_low") == len(shelves) and r.get("highs") == [{"value": 0, "count": len(shelves)}],
             "the words' halves %s the call: %s" % (key, r), failures)
    return failures + check_r1_g_fields(p, task, queued)


def check_r1_g_fields(p, task, queued=False):
    """Spike.11 (WHY§293): the four fields of G its method reads, just before the call and about 50 and 250 ms and 2 s
    after it: its open word 1, its manager and its queue on the heap and classed (no lead's), its deferred byte 0 (1
    with --task-queue); the byte the method compares first is that one, and the path it took the one --describe says."""
    failures = []
    rack = task["register_rack"]
    m = p.get("method", {})
    reads = m.get("g_fields", [])
    times = [r["at_ms"] for r in reads]
    need(len(reads) == 4 and times[0] == 0 and 45 <= times[1] < 250 <= times[2] < 2000 <= times[3],
         "G's fields' reads: %s" % times, failures)
    defer = 1 if queued else 0

    def heap(x):
        return (x.get("field_value", {}).get("kind") == "heap" and x.get("field_class", {}).get("classed") and
                x.get("field_class", {}).get("match") == "none")

    for r in reads:
        f = {x["disp"]: x for x in r.get("fields", [])}
        need(r.get("read") and len(f) == 4 and all(x["read"] for x in f.values()) and
             f.get(rack["ledger_open_off"], {}).get("field_value") == {"kind": "small", "value": 1} and
             f.get(rack["ledger_defer_off"], {}).get("value") == defer and
             f.get(rack["ledger_defer_off"], {}).get("width") == 1 and
             heap(f.get(rack["ledger_manager_off"], {})) and heap(f.get(rack["ledger_queue_off"], {})),
             "G's fields at %s ms: %s" % (r.get("at_ms"), r), failures)
    need(m.get("defer_disp") == rack["ledger_defer_off"] and m.get("path_taken") == ("queued" if queued else "now"),
         "the deciding byte and the path: %s, %s" % (m.get("defer_disp"), m.get("path_taken")), failures)
    return failures


def check_r1_queue(p, task, gap=False, queued=False):
    """Spike.11 (WHY§293): G's queue (G's field passed to the queued path's call) walked as a fifth root, at the same
    four times: the two requests of the game's own that wait there for ever (their tasks the manager's, classed, their
    manager field G's, their words and flag 1), and with --task-queue ours at 50 and 250 ms (our task, G's manager, the
    group's word, flag 0), drained by 2 s."""
    failures = []
    rack = task["register_rack"]
    group = p["register"]["group_index"]
    q = p.get("queue", {})
    reads = q.get("reads", [])
    need(q.get("g_disp") == rack["ledger_queue_off"] and q.get("queue_walk") == "walked" and len(reads) == 4,
         "G's queue: %s" % {k: v for k, v in q.items() if k != "reads"}, failures)
    queued = queued and not gap
    for r in reads:
        reqs = r.get("requests", [])
        ours = [x for x in reqs if x.get("ours")]
        theirs = [x for x in reqs if x.get("ours") is False]
        early = 0 < r["at_ms"] < 2000
        need(r.get("read") and r.get("found") == len(reqs) and
             sorted(x.get("word") for x in theirs) == rack["lingering_words"] and
             all(x.get("flag") == 1 and x.get("manager") is True and x.get("task_value", {}).get("kind") == "heap" and
                 x.get("task_class", {}).get("classed") for x in theirs) and
             len(ours) == (1 if queued and early else 0) and
             all(x.get("word") == group and x.get("flag") == 0 and x.get("manager") is True for x in ours),
             "the queue at %s ms: %s" % (r.get("at_ms"), r), failures)
    return failures


def check_r1_request_search(p, task):
    """Spike.11 (WHY§293): in each whole-memory search, the game's requests: the two that wait in G's queue (private
    memory), each read as a request (not our task, its manager G's, its word and flag 1), and their tasks' two classes,
    each with its vtable's slots and 16 words (the first its vtable, classed)."""
    failures = []
    rack = task["register_rack"]
    for m in p.get("memory_search", []):
        hits = m.get("request_hits", [])
        tasks = m.get("request_tasks", [])
        need(m.get("request_found") == rack["lingering"] and len(hits) == rack["lingering"] and
             all(h["region"] == "private" and h.get("ours") is False and h.get("manager") is True and
                 h.get("flag") == 1 and h.get("task_class", {}).get("classed") for h in hits) and
             sorted(h.get("word") for h in hits) == rack["lingering_words"],
             "the game's requests: %s" % {k: m.get(k) for k in ("request_found", "own_request_found", "request_hits")},
             failures)
        need(sorted(t["vtable"] for t in tasks) == sorted(rack["chore_vtable_rvas"]) and
             all(t["count"] == 1 and t["slot_count"] >= 2 and len(t["words"]) == 16 and
                 t["words"][0].get("kind") == "game" and t["words"][0].get("word_class", {}).get("classed")
                 for t in tasks), "their tasks: %s" % tasks, failures)
    return failures


def check_r1_classes(p, gap=False):
    """Spike.11 (WHY§293): the classes met, with their bases: G's manager, the request (one base), CSTask's instance,
    the group's entry (none with --table-gap), G, and the game's two task classes (one base each, the same: the one
    shared base, listed twice)."""
    failures = []
    classes = {}
    for x in p.get("classes", []):
        classes.setdefault(x["object"], []).append(x)
    want = {"manager", "request", "task_hub", "g", "game_task"} | (set() if gap else {"group_entry"})
    need(set(classes) == want and len(classes.get("game_task", [])) == 2 and
         all(x["classed"] for v in classes.values() for x in v),
         "the classes: %s" % p.get("classes"), failures)
    request = classes.get("request", [{}])[0]
    chores = classes.get("game_task", [])
    shared = p.get("shared_bases", [])
    need(len(chores) == 2 and request.get("base_count") == 1 and all(x.get("base_count") == 1 for x in chores) and
         len({tuple(x.get("base_rvas", [])) for x in chores}) == 1 and
         shared == [{"type_descriptor": chores[0]["base_rvas"][0], "count": 2}],
         "the bases: request %s, tasks %s, shared %s" % (request, chores, shared), failures)
    return failures


def check_r1_views(p, task, gap=False):
    """Spike.10 (WHY§289): the hub's four views of its rows (run 9's paths, 1 KiB each), read again at 2 s (since
    spike.11 only then): the note the request's work made (with --table-gap none), its vtable in rdata classed as a
    class of ours, a fresh pointer, and the game thread's stamp; our calling thread's id nowhere."""
    failures = []
    rack = task["register_rack"]
    v = p.get("array_views", {})
    views = v.get("views", [])
    need([x["path"] for x in views] == rack["views"] and all(x["reached"] and x["bytes"] == 1024 for x in views),
         "the views: %s" % views, failures)
    reads = v.get("reads", [])
    times = sorted({r["at_ms"] for r in reads})
    need(len(reads) == 4 and len(times) == 1 and 2000 <= times[0],
         "the views' reads: %s" % [(r["at_ms"], r["view"]) for r in reads], failures)
    for r in reads:
        need(r["read"] and not r["caller_thread"], "a view's read: %s" % r, failures)
        if gap:
            need(r["changed"] == 0, "a view with nothing noted: %s" % r, failures)
            continue
        news = [w["new"] for w in r["words"]]
        classed = [w for w in r["words"] if w.get("new_class", {}).get("classed")]
        need(r["changed"] == (3 if r["main_thread"] else 2) and {"kind": "game", "section": "rdata", "rva": rack["note_vtable_rva"]} in news and
             any(n["kind"] == "heap" for n in news) and len(classed) == 1 and
             classed[0]["new_class"].get("match") == "none", "a view's changes: %s" % r, failures)
        need(r["main_thread"], "the game thread's stamp, at %s ms: %s" % (r["at_ms"], r), failures)
    return failures


def check_r1_sites(p, task):
    """Spike.10 (WHY§289): what the fake game's own four call sites pass as the task (rcx): loaded from the rows, the
    bench's member, an allocator's result, loaded; each base the function's own argument, no branch between."""
    failures = []
    rack = task["register_rack"]
    sites = p.get("game_sites", [])
    need([x["site"] for x in sites] == task["register_call_sites"], "the game's sites: %s" % sites, failures)
    for got, want in zip(sites, rack["site_args"]):
        need(got["decoded"] and not got["joined"] and all(got.get(k) == want[k] for k in want),
             "a call site's task: %s, wanted %s" % (got, want), failures)
    return failures


def check_r1_sibling(p, task):
    """Spike.10 (WHY§289): the registration's sibling of the same shape, summarised with its tail jump one level down."""
    failures = []
    rack = task["register_rack"]
    s = p.get("sibling", {})
    inner = {(i["rva"], i.get("depth", 1)): i for i in s.get("inner", [])}
    tails = [k for k in s.get("call_sites", []) if k["form"] == "tail"]
    need(s.get("rva") == rack["sibling_rva"] and s.get("sites") == rack["sibling_sites"] and s.get("read") and
         len(tails) == 1 and tails[0].get("target") == rack["forget_rva"] and
         inner.get((rack["forget_rva"], 1), {}).get("tail") is True,
         "the sibling: %s" % {k: v for k, v in s.items() if k not in ("forms", "args", "guard", "inner")}, failures)
    return failures


def r1_lead(result, label):
    """The index of a singleton's label in the leads R1 reads (gal/leads.toml as make_gal writes it: the leads file
    without its decoys), as R1's report gives a lead: a number. The leads' tables are read in their labels' order."""
    labels = sorted(k for k in read_leads(result.args.set_paths["leads"]).get("singletons", {})
                    if k not in ("duplicata", "strayling", "rockbound"))
    return labels.index(label) if label in labels else None


def check_r1_table(p, task, lead, gap=False):
    """Spike.8 and spike.9 (WHY§240, WHY§248): the rack the tail function reads, matched to run 7's shapes (its end run
    8's direct jump, to the group's function) and read just before the call and 2 s after it: the rack on the heap (not
    the hub), an entry for each of the hub's groups up to the bound (with --table-gap none for the stand-in group), the
    group's shelf with its vtable in the game's rdata; the classes of the rack (classed, no lead's) and of the shelf
    (the hub's lead's name with Imp); and the call's return value, 1 (0 with the gap)."""
    failures = []
    rack = task["register_rack"]
    group = p["register"]["group_index"]
    t = p.get("table", {})
    need(t.get("matched") and t.get("singleton_slot") == rack["slot_rva"] and "hop" not in t and "array" not in t and
         t.get("disp") == rack["shelves_off"] and t.get("bound") == rack["bound"] and t.get("rva") == rack["tail_rva"]
         and t.get("target") == rack["take_rva"] and "slot_disp" not in t and "site" in t, "the table's shape: %s" %
         {k: v for k, v in t.items() if k not in ("before", "after")}, failures)
    need(t.get("singleton_class") == {"vtable_known": True, "classed": True,
                                      "type_descriptor": t.get("singleton_class", {}).get("type_descriptor"),
                                      "match": "none"} and t["singleton_class"]["type_descriptor"] > 0,
         "the rack's class: %s" % t.get("singleton_class"), failures)
    if gap:
        need("entry_class" not in t, "the entry's class, with no shelf: %s" % t.get("entry_class"), failures)
    else:
        ec = t.get("entry_class", {})
        need(ec.get("vtable_known") and ec.get("classed") and ec.get("type_descriptor", 0) > 0 and
             ec.get("match") == "imp" and ec.get("lead") == lead,
             "the shelf's class against the leads: %s" % ec, failures)
    groups = [g["index"] for g in task["groups"]]
    shelves = [g for g in groups if not (gap and g == group)]
    for key in ("before", "after"):
        r = t.get(key, {})
        need(r.get("read") and r.get("singleton", {}).get("kind") == "heap" and not r.get("is_instance") and
             "singleton_lead" not in r and r.get("entries") == rack["bound"] + 1 and
             r.get("nonzero_at") == shelves and r.get("nonzero") == len(shelves),
             "the table %s the call: %s" % (key, r), failures)
        if gap:
            need(r.get("entry") == {"kind": "zero"} and "entry_vtable" not in r and "slot_value" not in r,
                 "the group's entry %s the call, with no shelf: %s" % (key, r), failures)
        else:
            need(r.get("entry", {}).get("kind") == "heap" and
                 r.get("entry_vtable") == {"kind": "game", "section": "rdata", "rva": rack["shelf_vtable_rva"]} and
                 "slot_value" not in r, "the group's entry %s the call: %s" % (key, r), failures)
    need(t.get("before", {}).get("at_ms") == 0 and t.get("after", {}).get("at_ms", 0) >= 2000,
         "the table's reads' times: %s" % [t.get(k, {}).get("at_ms") for k in ("before", "after")], failures)
    want_returned = {"kind": "zero"} if gap else {"kind": "small", "value": 1}
    need(p["register"].get("returned") == want_returned and p["register"].get("returned_low") == (0 if gap else 1),
         "the call's return value: %s, low byte %s" % (p["register"].get("returned"),
                                                       p["register"].get("returned_low")), failures)
    return failures


def check_r1_memory(p, task):
    """Spike.6 and spike.7 (WHY§227, WHY§235): two whole-memory searches after the call, each complete within its caps,
    each finding our task's address in the fake game's own memory (the hub's last_task and its tables: private
    regions), our vtable's in the hub's last_vtable and our run slot's in the hub's table of run slots, and, counted
    apart, ours (our module's data, the probe's own heap, our vtable itself), with the process's committed total."""
    failures = []
    ms = p.get("memory_search", [])
    need(len(ms) == 2 and ms[0]["at_ms"] >= 2000 and ms[1]["at_ms"] >= 20000, "the memory searches: %s" %
         [(m["at_ms"], m["stopped"]) for m in ms], failures)
    slots = p["register"]["vtable_slots"]
    for m in ms:
        game = [h for h in m["hits"] if not h["own"]]
        ours = [h for h in m["hits"] if h["own"]]
        need(m["stopped"] == "done" and m["own_known"] and m["read_bytes"] > 0 and m["regions"] >= m["read_regions"] > 0
             and m["committed_bytes"] >= m["read_bytes"] + m["unread_bytes"],
             "the memory search's walk: %s" % {k: v for k, v in m.items() if k != "hits"}, failures)
        need(m["found"] >= 2 and any(h["points_to"] == "object" and h["offset"] == 0 and h["region"] == "private"
                                     for h in game), "the game's copies: %s of %s" % (m["found"], game), failures)
        need(m["own_found"] >= 2 and
             any(h["region"] == "image" and h.get("module") == "ours" and h.get("section") == "data" for h in ours) and
             any(h["region"] == "private" for h in ours), "our own copies: %s of %s" % (m["own_found"], ours), failures)
        need(m["vtable_found"] >= 1 and any(h["points_to"] == "vtable" and h["region"] == "private" for h in game),
             "the game's copies of our vtable: %s of %s" % (m["vtable_found"], game), failures)
        need(m["slot_found"] >= 1 and m["slot_distinct"] == 1 and
             any(h["points_to"] == "slot" and h.get("slot") == task["run_slot"] for h in game),
             "the game's copies of our slots: %s, %s distinct, of %s" % (m["slot_found"], m["slot_distinct"], game),
             failures)
        need(m["own_vtable_found"] >= 1 and m["own_slot_found"] >= slots and m["own_slot_distinct"] == slots,
             "our own copies of our vtable and slots: %s" %
             {k: m[k] for k in ("own_vtable_found", "own_slot_found", "own_slot_distinct")}, failures)
        need(all(("module" in h) == (h["region"] == "image") for h in m["hits"]), "hits' modules: %s" % m["hits"],
             failures)
    return failures + check_r1_tagged(p, task)


def check_r1_tagged(p, task):
    """Spike.9 (WHY§248): in each whole-memory search, our object's address under a tag: since spike.11 (R1 run 10's
    shape: G keeps no list) none in the game's memory, nor of our vtable's."""
    failures = []
    for m in p.get("memory_search", []):
        need(m.get("tagged_found") == 0 and m.get("vtable_tagged_found") == 0 and
             not [h for h in m.get("tagged_hits", []) if not h["own"]],
             "the tagged copies of our address: %s" %
             {k: m.get(k) for k in ("tagged_found", "own_tagged_found", "vtable_tagged_found", "tagged_hits")}, failures)
    del task
    return failures


def r1_diffs(p, root):
    return [d for d in p.get("hub_diff", []) if d.get("root") == root]


def check_r1_roots(p, task, gap=False, gate=False):
    """Spike.9 to spike.11 (WHY§248, WHY§289, WHY§293): the walk and its diffs from four more roots, the rack (the
    singleton), the stand-in group's shelf (its entry), G and G's queue, each at 2 and 20 s with its control. Since
    spike.11 (R1 run 10's shape) G keeps nothing of our task, and no walk but the hub's holds anything of ours. With
    --task-gate G is the shelf's (held by a root: not walked); with --table-gap there is no entry to walk. Every control
    changes nothing."""
    failures = []
    roots = ["singleton"] + ([] if gap else ["group_entry"]) + ([] if gate else ["g_singleton"]) + ["g_queue"]
    for root in ("singleton", "group_entry", "g_singleton", "g_queue"):
        ds = r1_diffs(p, root)
        if root not in roots:
            need(not ds, "a diff from %s: %s" % (root, ds), failures)
            continue
        need(len(ds) == 2 and ds[0]["at_ms"] >= 2000 and ds[1]["at_ms"] >= 20000 and
             all(not d["capped"] and d["compared"] > 0 and "in_control_words" in d for d in ds),
             "the %s's diffs: %s" % (root, [{k: v for k, v in d.items() if k != "words"} for d in ds]), failures)
        need(all(w["new"]["kind"] not in ("task", "task_tagged", "vtable", "ours") for d in ds for w in d["words"]),
             "the %s's diffs, with nothing of ours: %s" % (root, [[(w["path"], w["new"]) for w in d["words"]]
                                                                 for d in ds]), failures)
    ctls = {d.get("root"): d for d in p.get("hub_diff_controls", [])}
    need(set(ctls) == set(roots) and all(1900 <= d["at_ms"] <= 6000 and d["changed"] == 0 for d in ctls.values()),
         "the roots' controls: %s" % [{k: v for k, v in d.items() if k != "words"} for d in ctls.values()], failures)
    need(p.get("hub_diff_control", {}).get("root") == "task_hub", "the hub's control: %s" %
         {k: v for k, v in p.get("hub_diff_control", {}).items() if k != "words"}, failures)
    return failures


def check_r1_spike10(p, task, gap=False, gate=False, queued=False, late=False, world=False):
    """Spike.10's own checks (WHY§289): G and its method, the array's views, the game's call sites, the sibling; and
    spike.11's (WHY§293): G's fields and the path, the request and its methods, G's queue, the game's requests in the
    searches, the classes and their bases; and spike.12's (WHY§296): the manager's map and list."""
    group = p["register"]["group_index"]
    return (check_r1_method(p, task, group, gap=gap, gate=gate, queued=queued) + check_r1_views(p, task, gap=gap) +
            check_r1_sites(p, task) + check_r1_sibling(p, task) + check_r1_request(p, task) +
            check_r1_queue(p, task, gap=gap, queued=queued) + check_r1_request_search(p, task) +
            check_r1_classes(p, gap=gap) + check_r1_map(p, task, gap=gap, late=late, world=world))


def check_r1_map(p, task, gap=False, late=False, world=False):
    """Spike.12 (WHY§296): the manager's map and list as R1 read them. The layout from the now path's function's
    summary: the head at the manager's offset --describe gives, the root at the head's +8, the nil byte, the 4-byte key
    and the value 8 bytes past it (the summary knows no value in rcx at the match's call, its paths disagreeing on the
    node: value_known false), the children at +0 and +16, the list's begin and end; the hand-over a match calls and the
    other branch's call. The readings: the first just before the call (at the title, the title's words, every value a
    bin of the title's class, the list one bin), at least five in all, each kept reading's keys in order with high 16
    bits 0; the stand-in group's word present at every reading (with --map-late absent at the title, with its nearest
    words below and above, and present once the world's first load added the world's words, at a reading in the
    world); with --table-gap no word looked for. The last reading after 10 s, its map the title's and the world's
    words, the world's bins of their own class; with `world` (a case whose world stays loaded) a reading in the world
    with them."""
    failures = []
    rack = task["register_rack"]
    m = p.get("map", {})
    group = p["register"]["group_index"]
    title, added = list(rack["map_title_words"]), list(rack["map_world_words"])
    if late and group in title:  # --describe runs without the case's options: with --map-late the word is the world's
        title.remove(group)
        added = sorted(added + [group])
    need(m.get("head_disp") == rack["map_head_off"] and m.get("root_disp") == 8 and
         m.get("nil_disp") == rack["node_nil_off"] and m.get("key_disp") == rack["node_key_off"] and
         m.get("key_width") == 4 and m.get("value_disp") == rack["node_value_off"] and m.get("value_known") is False and
         m.get("left_disp") == 0 and m.get("right_disp") == 16 and m.get("list_begin") == rack["map_list_off"] and
         m.get("list_end") == rack["map_list_off"] + 8 and m.get("map_call") == rack["map_take_rva"] and
         m.get("other_call") == rack["task_other_rva"] and "shape_failed" not in m,
         "the map's shape: %s" % {k: v for k, v in m.items() if k != "readings"}, failures)
    need(("word" not in m) if gap else m.get("word") == group, "the word looked for: %s" % m.get("word"), failures)
    reads = m.get("readings", [])
    need(m.get("readings_made", 0) >= 5 and 2 <= len(reads) <= 26 and m.get("changes", 0) >= 1 and
         all(r["read"] and "map_failed" not in r and "list_failed" not in r for r in reads) and
         [r["at_ms"] for r in reads] == sorted(r["at_ms"] for r in reads),
         "the map's readings: %s" % [(r.get("at_ms"), r.get("world_stage"), r.get("nodes"), r.get("word_present"))
                                     for r in reads], failures)
    if not reads:
        return failures

    def classes(r, key):
        return sorted((x["value_class"]["type_descriptor"] if x.get("value_class", {}).get("classed") else None,
                       x["count"]) for x in r.get(key, []))

    first, last = reads[0], reads[-1]
    bins = classes(first, "values")
    need(first["at_ms"] == 0 and first["world_stage"] == "title" and first["nodes"] == len(title) and
         first.get("key_lows") == sorted(title) and first.get("key_highs") == [{"value": 0, "count": len(title)}] and
         first.get("ordered") and first.get("root_parent") and first.get("head_ends") and
         first.get("count_word") == {"kind": "small", "value": len(title)} and len(bins) == 1 and
         bins[0][0] is not None and bins[0][1] == len(title) and first.get("list_count") == rack["list_count"] and
         classes(first, "list_values") == [(bins[0][0], 1)] and
         all(x["value_kind"] == {"kind": "heap"} for x in first.get("values", [])),
         "the first reading: %s" % first, failures)
    for r in reads:
        present = r.get("word_present")
        if gap:
            need(present is None and "below" not in r, "a reading with no word: %s" % r, failures)
        elif not late or r["nodes"] == len(title) + len(added):
            need(present is True and "below" not in r and "above" not in r, "the word at %s ms: %s" % (r["at_ms"], r),
                 failures)
        else:
            below = max(w for w in title if w < group)
            above = min(w for w in title if w > group)
            need(present is False and r.get("below") == below and r.get("above") == above,
                 "the word absent at %s ms: %s" % (r["at_ms"], r), failures)
        need(r.get("key_highs", [{"value": 0}])[0]["value"] == 0 and
             r["nodes"] in (len(title), len(title) + len(added)), "the keys at %s ms: %s" % (r["at_ms"], r), failures)
    full = sorted(title + added)
    need(last["at_ms"] >= 10000 and last["nodes"] == len(full) and last.get("key_lows") == full and
         len(classes(last, "values")) == 2 and sorted(n for _, n in classes(last, "values")) ==
         sorted([len(title), len(added)]),
         "the last reading: %s" % last, failures)
    need(not world or any(r["world_stage"] == "world" and r["nodes"] == len(full) for r in reads),
         "no reading in the world with the world's words: %s" %
         [(r["at_ms"], r["world_stage"], r["nodes"]) for r in reads], failures)
    return failures


def check_r1_control(p, ds):
    """Spike.8 (WHY§240): the control diff, the same walk at the Mark and 2 s later, changed the hub's frame alone, and in
    each diff across the call the frame is marked in_control and listed after the words that are not."""
    failures = []
    ctl = p.get("hub_diff_control", {})
    need(ctl and 1900 <= ctl.get("at_ms", 0) <= 6000 and not ctl["capped"] and ctl["compared"] > 0 and
         [w["path"] for w in ctl["words"]] == [[8]] and "in_control_words" not in ctl,
         "the control diff: %s" % ctl, failures)
    for d in ds:
        frame = {tuple(w["path"]): w for w in d["words"]}.get((8,), {})
        need(d.get("in_control_words") == 1 and frame.get("in_control") is True and d["words"][-1] == frame and
             all(w.get("in_control") is False for w in d["words"][:-1]),
             "the diff's control marks: %s" % [(w["path"], w.get("in_control")) for w in d["words"]], failures)
    return failures


def check_r1_diff(p, task):
    """Spike.7 (WHY§235): the hub's reachable memory kept just before the call and compared with the same walk 2 and 20 s
    after it: the hub's last_task now our task's address (not followed into, so fresh), its last_vtable our vtable, its
    table of run slots our run slot (in our module), and at 20 s those changes steady and the hub's frame not. Since
    spike.9 these are the diffs whose root is the hub (task_hub); the others are check_r1_roots's."""
    failures = []
    ds = r1_diffs(p, "task_hub")
    need(len(ds) == 2 and ds[0]["at_ms"] >= 2000 and ds[1]["at_ms"] >= 20000, "the hub's diffs: %s" %
         [d.get("at_ms") for d in ds], failures)
    if len(ds) != 2:
        return failures
    for d in ds:
        head = {k: v for k, v in d.items() if k != "words"}
        need(not d["capped"] and d["kept_bytes"] > 0 and d["compared"] > 0 and d["new_blocks"] >= 1 and
             d["changed"] >= 5 and len(d["words"]) == min(32, d["changed"]), "the diff: %s" % head, failures)
        words = {tuple(w["path"]): w for w in d["words"]}
        last = words.get((task["register_keeps_off"],), {})
        need(last.get("depth") == 0 and last.get("new", {}).get("kind") == "task" and
             last["new"].get("fresh") is True and last.get("old", {}).get("kind") in ("heap", "game"),
             "the hub's last task: %s" % last, failures)
        vt = words.get((task["register_keeps_vtable_off"],), {})
        need(vt.get("new", {}).get("kind") == "vtable" and vt["new"].get("fresh") is True and
             vt.get("old", {}).get("kind") == "game" and vt["old"].get("section") == "rdata",
             "the hub's last vtable: %s" % vt, failures)
        runs = [w for w in d["words"] if w["depth"] == 1 and w["path"][0] == task["hub_run_fns_off"]]
        need(len(runs) == 1 and runs[0]["old"]["kind"] == "zero" and runs[0]["new"]["kind"] == "ours" and
             "rva" in runs[0]["new"], "the hub's run slots: %s" % runs, failures)
    first, later = ds
    need("steady_words" not in first and all("steady" not in w for w in first["words"]),
         "the first diff's steadiness: %s" % first.get("steady_words"), failures)
    words = {tuple(w["path"]): w for w in later["words"]}
    frame = words.get((8,), {})
    need(later.get("steady_words", 0) >= 4 and words.get((task["register_keeps_off"],), {}).get("steady") is True and
         frame.get("steady") is False and later["words"][0].get("steady") is True,
         "the later diff's steadiness: %s steady, the last task %s, the frame %s" %
         (later.get("steady_words"), words.get((task["register_keeps_off"],)), frame), failures)
    return failures + check_r1_control(p, ds)


def r1_reports(result, failures):
    """R1's two reports, each linted by the engine; None unless both are there."""
    folder = os.path.join(result.paths["appdata"], "EldenNet", "probe")
    reports = {}
    for name in ("r1-derive.json", "r1-probe.json"):
        path = os.path.join(folder, name)
        if not os.path.exists(path):
            failures.append("no %s" % name)
            continue
        code, out, _ = run_engine(result.args, result.env, ["lint", "--report", result.as_win(path)])
        need(code == 0, "lint --report %s: exit %s %s" % (name, code, out[-400:]), failures)
        with open(path) as f:
            reports[name] = json.load(f)
    return reports if len(reports) == 2 else None


def check_r1(result, queued=False):
    """R1's two reports against --describe's truth: lint-clean, the checks confirmed, one call, the slot counts, our
    object's written bytes, the census's base and its run slot, the hinstance and the statics' slots. Since spike.11
    `queued`: the ledger queued the request (--task-queue), and a later frame ran it."""
    failures = []
    truth = result.truth
    reports = r1_reports(result, failures)
    if reports is None:
        return failures
    p, d = reports["r1-probe.json"], reports["r1-derive.json"]
    task = truth["task"]
    checks = {(c["item"], c.get("label", "")): c for c in p["checks"]}
    for item in ("gate", "name_layout", "register", "register_abi", "group", "task_shape"):
        need(checks.get((item, ""), {}).get("status") == "confirmed", "check %s: %s" % (item, checks.get((item, ""))),
             failures)
    need(p["register"]["call"] == "ok" and p["stage"] == "running", "the call: %s, stage %s" %
         (p["register"]["call"], p["stage"]), failures)
    need(p["register"]["callee"] == task["register_rva"], "callee 0x%X, truth 0x%X" %
         (p["register"]["callee"], task["register_rva"]), failures)
    need(p["register"]["registers"] == [1, 2, 8], "registers %s" % p["register"]["registers"], failures)
    need(p["register"]["group_index"] == task["standin"]["index"], "group %s" % p["register"]["group_index"], failures)
    calls = {s["slot"]: s["calls"] for s in p["task"]["slot_calls"]}
    run, attach, query = task["run_slot"], task["attach_slot"], task["query_slot"]
    need(calls.get(attach) == 1, "attach slot calls %s" % calls, failures)
    need(calls.get(run, 0) >= 200 and calls.get(run, 0) >= 30 * calls.get(query, 0) > 0, "slot calls %s" % calls,
         failures)
    need(0 < p["task"]["object_written"] <= task["object_size"], "object written %s, size %s" %
         (p["task"]["object_written"], task["object_size"]), failures)
    need(p["census"]["done"] and p["census"]["bases"], "the census: %s" % p["census"], failures)
    if p["census"]["bases"]:
        need(p["census"]["bases"][0].get("run_slot") == run and p["census"]["bases"][0]["derived"] >= 2,
             "the census's base: %s" % p["census"]["bases"][0], failures)
    statics = {a["target_rva"] for a in truth["aob"] if a["kind"] == "static"}
    need(len(p["hinstance"]["slots"]) == 1 and p["hinstance"]["slots"][0] in statics, "hinstance %s" % p["hinstance"],
         failures)
    found = {s["label"]: s["at"] for s in p["statics"]}
    need(all(len(v) == 1 and v[0] in statics for v in found.values()) and len(found) == 3, "statics %s" % found,
         failures)
    need(any(h["offset"] == 0x20 and h["rose"] >= 2 and h["still"] >= 1 for h in p["rising"]["hits"]),
         "the rising search: %s" % p["rising"], failures)
    need(p["allocations"] == 0, "%s allocations on the game thread" % p["allocations"], failures)
    failures += (check_r1_callee(p, task) + check_r1_table(p, task, r1_lead(result, "task_loom")) +
                 check_r1_memory(p, task) + check_r1_diff(p, task) + check_r1_roots(p, task) +
                 check_r1_spike10(p, task, queued=queued))
    # r1-derive.json: the singletons named from the live class objects, and patterns at the statics' slots.
    need(d["fd4"]["status"] == "derived" and all(s["status"] == "derived" for s in d["fd4"]["singletons"]),
         "the live derivation's singletons: %s" % [(s["label"], s["status"]) for s in d["fd4"]["singletons"]], failures)
    need(sum(1 for s in d["statics"] if s["status"] == "derived") == 4, "the statics: %s" %
         [(s["label"], s["status"]) for s in d["statics"]], failures)
    # any_line: once the statics' slots are found the derivation runs again, and its line may come after this one.
    return failures + any_line(result, "ELDENNET", "r1", stage="task_confirmed") + en_log_is_clean(result)


case("launcher-eldennet-r1", LGAME + ["--dll", "{eldennetdll}", "--console"] +
     game("--title-frames", "540", "--press-pause-at", "300", "--reload-every", "360", "--load-frames", "180",
          frames=1800),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet('probe = { round = "r1", register = "mark", settle_s = 1 }\n', probe=True,
                              leads_base="Leads::NoTaskBaseGiven"),
     check=lambda r: check_r1(r) + want(r, "LAUNCHER", "status", init="ready"),
     what="R1 in a probe build: the derivation over the live image, the checks, the one call at the Mark (Pause at the "
          "title), the slot counts, the hub census's base class and run slot, the statics' slots, two lint-clean "
          "reports")


# The read-only round (Lodestar's step L5; proposal 0005 §6.2): a probe build with `probe = { round = "read" }` names the
# managers by their instances' classes, tries the reader on them from a thread of its own, and writes a report of
# numbers and a draft table. It calls nothing: the fake hub sees no task of ours. The round's leads go by the
# catalogue's labels, so the fake game's leads are relabelled for it, and gal/ gets the repository's ids.toml.
READ_LABELS = {"task_loom": "task_hub", "actor_roster": "characters", "lobby_warden": "session",
               "mark_ledger": "event_flags", "gauge_board": "hud", "menu_desk": "menus", "veil_keeper": "loading",
               "sky_clock": "world_clock", "eye_rig": "camera", "table_vault": "param_store",
               "phrase_book": "messages", "actor": "character", "player": "player_character"}
READ_SINGLETONS = {"TaskLoom": "task_hub", "ActorRoster": "characters", "LobbyWarden": "session",
                   "MarkLedger": "event_flags", "GaugeBoard": "hud", "MenuDesk": "menus", "VeilKeeper": "loading",
                   "SkyClock": "world_clock", "EyeRig": "camera", "TableVault": "param_store", "PhraseBook": "messages"}


def read_round_gal(gal):
    path = os.path.join(gal, "leads.toml")
    with open(path) as f:
        text = f.read()
    for old, new in READ_LABELS.items():
        text = re.sub(r"(?m)^%s = " % old, "%s = " % new, text)
        text = text.replace('instance = "%s"' % old, 'instance = "%s"' % new)
    with open(path, "w") as f:
        f.write(text)
    shutil.copy2(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "gal", "ids.toml"),
                 os.path.join(gal, "ids.toml"))


def check_read(result, by_flag=False):
    """The read-only round's two files against --describe's truth, and that nothing was called. `by_flag`: the round
    was asked for by the launcher's --read, not by settings.toml."""
    failures = [] if by_flag else want(result, "ELDENNET", "settings", probe="read")
    failures += check_lines(("ELDENNET", "reader", {"started": 1, "round": "read"}),
                           ("ELDENNET", "first_reading", {"state": "running"}),
                           ("ELDENNET", "init", {"state": "ready"}),
                           ("LAUNCHER", "end", {"kind": "clean"}))(result)
    failures += want(result, "ELDENNET", "read", stage="found", known_build=1, recipe=1, statics=(11, 64))
    # The first pass comes before the game has made its managers (--statics-delay-ms): it names none. A later pass
    # names every one, and the trial then reads them.
    failures += want(result, "ELDENNET", "read", which="first", stage="named", **{"pass": "1"}, named=0)
    failures += want(result, "ELDENNET", "read", stage="named", named=11, of=11, ambiguous=0, unlisted=0)
    failures += want(result, "ELDENNET", "read", stage="trial", named=11, managers=11, readings=(20, 1e6),
                     same=(11 * 20, 1e9), other=0, fault=0, torn=0, faults=0)
    failures += want(result, "ELDENNET", "read", stage="table", entries=(13, 14), left_out=0, witnesses_dropped=0)
    failures += ready_no_call(result)
    need(not result.events("ELDENNET", "r1"), "the read-only round ran R1", failures)
    folder = os.path.join(result.paths["appdata"], "EldenNet", "probe")
    report, table = os.path.join(folder, "lodestar-found.json"), os.path.join(folder, "lodestar-table.toml")
    if not (os.path.exists(report) and os.path.exists(table)):
        return failures + ["no lodestar-found.json or lodestar-table.toml in the probe folder"]
    code, out, _ = run_engine(result.args, result.env, ["lint", "--report", result.as_win(report)])
    need(code == 0, "lint --report lodestar-found.json: exit %s %s" % (code, out[-400:]), failures)
    with open(report) as f:
        text = f.read()
    with open(table) as f:
        draft = f.read()
    d = json.loads(text)
    need(d["report"] == "lodestar" and d["schema"] == 2, "the report's kind: %s %s" % (d.get("report"), d.get("schema")),
         failures)
    # Numbers and the catalogue's labels: no name of the fake game's, in either file.
    for name in list(READ_SINGLETONS) + ["FG::", "Imp", "task_loom", "Fg_"]:
        need(name not in text and name not in draft, "a name of the game's in the round's files: %s" % name, failures)
    inst, trial = d.get("instances"), d.get("trial")
    if not inst or not trial:
        return failures + ["the report has no instances or no trial"]
    need(inst["named"] == 11 and inst["ambiguous"] == 0 and inst["passes"] >= 2 and inst["leads"] == 11,
         "instances: %s" % {k: inst[k] for k in ("named", "ambiguous", "passes", "leads", "unlisted")}, failures)
    truth = {READ_SINGLETONS[s["name"]]: s["static_rva"] for s in result.truth["fd4"]["singletons"]}
    for m in inst["managers"]:
        need(m["status"] == "derived" and m["rva"] == truth.get(m["label"]) and m["pass"] >= 2 and m["witnesses"] and
             m["vtable"] != 0, "manager %s: %s" % (m["label"], m), failures)
        need(("[item.%s]" % m["label"]) in draft, "the draft table lacks %s" % m["label"], failures)
    need(len(trial["managers"]) == 11 and trial["readings"] >= 20 and trial["fault_copy"] == 0 and
         trial["fault_stack"] == 0 and trial["fault_thread"] == 0,
         "the trial: %s" % {k: trial[k] for k in trial if k != "managers"}, failures)
    for m in trial["managers"]:
        need(m["same"] >= 20 and m["other"] == 0 and m["fault"] == 0 and m["torn"] == 0, "trial of %s: %s" %
             (m["label"], m), failures)
    need("[item.character]" in draft and "[item.player_character]" in draft and 'status = "confirmed"' not in draft and
         draft.startswith("[provenance]"), "the draft table's classes, statuses or head", failures)
    return failures + en_log_is_clean(result)


case("launcher-eldennet-read", LGAME + ["--dll", "{eldennetdll}", "--console"] +
     game("--statics-delay-ms", "6000", frames=1800),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet('probe = { round = "read", settle_s = 1 }\n', probe=True, gal_change=read_round_gal),
     check=lambda r: check_read(r) + want(r, "LAUNCHER", "status", init="ready"),
     what="the read-only round in a probe build: a first pass before the game has made its managers, a later one that "
          "names all eleven by their instances' classes, the reader's trial on them from a thread of our own, a "
          "lint-clean report of numbers and a draft table, and no call into the game")


# The read-only round as the owner runs it (README-READ.txt): `eldennet-launcher --read`, with no settings.toml and the
# save backup made first; the console's steps in plain words and no key to press; a first pass before the game has made
# its managers ("Now load your save."), a later one that names them all ("All 11 managers are named."); the game ends
# and the launcher
# checks both files.
def read_steps(result):
    return [e.get("step") for e in result.events("LAUNCHER", "read") if "step" in e]


def check_read_owner(result):
    failures = check_read(result, by_flag=True)
    steps = read_steps(result)
    # The console polls the block every 200 ms, so "reading" may pass unseen; the others are always told, in order.
    essential = [x for x in steps if x != "reading"]
    need(essential == ["asked", "load_save", "done", "reports_clean"] and
         steps == [x for x in ("asked", "reading", "load_save", "done", "reports_clean") if x in steps],
         "the console's steps: %s" % steps, failures)
    plain = [l for l in result.lines if l.startswith("EldenNet read: ")]
    need(len(plain) == len(steps) and any(l.startswith("EldenNet read: Now load your save. 0 of 11") for l in plain) and
         any(l.startswith("EldenNet read: All 11 managers are named.") for l in plain) and
         not any("Pause" in l for l in plain), "the console's plain lines: %s" % plain[:8], failures)
    need(not any(l.startswith("EldenNet R1: ") for l in result.lines) and not result.events("LAUNCHER", "r1"),
         "the read-only round spoke as R1", failures)
    for name in ("lodestar-found.json", "lodestar-table.toml"):
        failures += any_line(result, "LAUNCHER", "read", report=name, lint="clean")
    failures += want(result, "ELDENNET", "probe", asked="read", accepted=1, probe_build=1)
    failures += want(result, "ELDENNET", "settings", file=0)
    failures += want(result, "LAUNCHER", "backup", folders=2, made=2)
    return failures


case("launcher-eldennet-read-owner", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                              "--read"] + game("--statics-delay-ms", "20000", frames=2700),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(None, probe=True, gal_change=read_round_gal, then=with_saves),
     check=check_read_owner,
     what="the read-only round as the owner runs it: --read (no settings.toml), the backup first, the console's steps "
          "in plain words with no key to press, the managers named once the game has made them, the trial, no call, "
          "and both files checked clean by the launcher when the game ends")
case("launcher-eldennet-read-release", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                                "--read"] + game("--stay-title", frames=600),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=120,
     prepare=install_eldennet(None),
     check=lambda r: (want(r, "ELDENNET", "probe", asked="read", accepted=0, probe_build=0) +
                      want(r, "LAUNCHER", "init", state="exit") +
                      ([] if read_steps(r) == ["asked", "refused", "reports_missing"] else
                       ["the console's steps: %s" % read_steps(r)]) +
                      any_line(r, "LAUNCHER", "read", report="lodestar-found.json", lint="missing") +
                      ["the release DLL went on: %s" % e for e in r.events("ELDENNET", "init")
                       if e.get("state") in ("discover", "reader", "first_reading", "ready")] +
                      (["the fake game's registration was called"] if r.events("FAKEGAME", "register") else [])),
     what="--read with the release DLL: refused at Settings, nothing read and nothing called, and the console says so")


# R1 as the owner runs it (README-R1.txt): `eldennet-launcher --probe`, with no settings.toml and the save backup made
# first; the console's steps in plain words; Pause pressed at the title until it is taken (the default settle of 10 s,
# then the live derivation); the one call; "task confirmed"; a while in the world with loading screens; the game ends
# and the launcher lints both reports.
R1_STEPS = ("asked", "reading", "press_mark", "called", "task_confirmed", "reports_clean")


def r1_steps(result):
    return [e.get("step") for e in result.events("LAUNCHER", "r1") if "step" in e]


def check_r1_owner(result):
    failures = check_r1(result)
    # The console polls the block every 200 ms, so a quick step (reading, called) may pass unseen, as in a Release
    # build on the fake game; the others are always told, in order, and nothing else is.
    steps = r1_steps(result)
    essential = [x for x in steps if x not in ("reading", "called")]
    need(essential == [x for x in R1_STEPS if x not in ("reading", "called")] and
         steps == [x for x in R1_STEPS if x in steps], "the console's R1 steps: %s" % steps, failures)
    plain = [l for l in result.lines if l.startswith("EldenNet R1: ")]
    need(len(plain) == len(steps) and any("press Pause" in l for l in plain) and
         any("Task confirmed" in l for l in plain), "the console's plain lines: %s" % plain[:8], failures)
    for name in ("r1-derive.json", "r1-probe.json"):
        failures += any_line(result, "LAUNCHER", "r1", report=name, lint="clean")
    failures += want(result, "ELDENNET", "probe", asked="r1", accepted=1, probe_build=1)
    failures += want(result, "ELDENNET", "settings", file=0)
    failures += want(result, "LAUNCHER", "backup", folders=2, made=2)
    p = json.load(open(os.path.join(result.paths["appdata"], "EldenNet", "probe", "r1-probe.json")))
    checks = {c["item"]: c for c in p["checks"]}
    need(checks.get("fd4_idiom", {}).get("status") == "confirmed", "the idiom's check: %s" % checks.get("fd4_idiom"),
         failures)
    need(checks.get("group", {}).get("expected") == checks.get("group", {}).get("value") is not None,
         "the group's check against the layout: %s" % checks.get("group"), failures)
    return failures


case("launcher-eldennet-probe", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}", "--probe"] +
     game("--title-frames", "1800", "--press-pause-at", "600", "--press-pause-every", "60", "--reload-every", "360",
          "--load-frames", "180", frames=3300),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(None, probe=True, leads_base="Leads::NoTaskBaseGiven", then=with_saves),
     check=check_r1_owner,
     what="R1 as the owner runs it: --probe (no settings.toml), the backup first, the console's steps in plain words, "
          "Pause at the title until taken, the reviewed idiom and the layout's group confirmed, one call, task "
          "confirmed, and both reports linted clean by the launcher when the game ends")
# Spike.8's second case (WHY§240): the registration's rack holds no shelf for the stand-in group (--table-gap), so the
# call answers 0 and the task never runs, as in R1 run 7. R1's report says so, and the run ends as run 7's did.
def check_r1_gap(result):
    failures = []
    reports = r1_reports(result, failures)
    if reports is None:
        return failures
    p = reports["r1-probe.json"]
    task = result.truth["task"]
    checks = {(c["item"], c.get("label", "")): c for c in p["checks"]}
    need(all(checks.get((item, ""), {}).get("status") == "confirmed"
             for item in ("gate", "fd4_idiom", "register", "register_abi", "group")),
         "the checks: %s" % [(c["item"], c["status"]) for c in p["checks"]], failures)
    need(p["register"]["call"] == "ok" and p["stage"] == "registered" and p["task"]["invocations"] == 0 and
         not p["task"]["slot_calls"] and p["task"]["object_written"] == 0,
         "the call, the stage and the task: %s, %s, %s" % (p["register"]["call"], p["stage"], p["task"]), failures)
    failures += check_r1_callee(p, task, gap=True) + check_r1_table(p, task, r1_lead(result, "task_loom"), gap=True)
    failures += check_r1_roots(p, task, gap=True) + check_r1_spike10(p, task, gap=True, world=True)
    ds = r1_diffs(p, "task_hub")
    need(len(ds) == 2 and all(w["new"]["kind"] not in ("task", "vtable", "ours") for d in ds for w in d["words"]),
         "the hub's diffs, with nothing kept: %s" % [[(w["path"], w["new"]["kind"]) for w in d["words"]] for d in ds],
         failures)
    failures += check_r1_control(p, ds)
    ms = p.get("memory_search", [])
    need(len(ms) == 2 and all(m["stopped"] == "done" and m["found"] == 0 and m["vtable_found"] == 0 and
                              m["slot_found"] == 0 and m["own_found"] >= 2 and m["tagged_found"] == 0 for m in ms),
         "the memory searches, with nothing kept: %s" %
         [{k: m.get(k) for k in ("stopped", "found", "vtable_found", "slot_found", "own_found")} for m in ms], failures)
    steps = r1_steps(result)
    need("task_confirmed" not in steps and steps[:1] == ["asked"] and "press_mark" in steps and
         steps[-1:] == ["reports_clean"], "the console's R1 steps: %s" % steps, failures)
    for name in ("r1-derive.json", "r1-probe.json"):
        failures += any_line(result, "LAUNCHER", "r1", report=name, lint="clean")
    failures += any_line(result, "FAKEGAME", "rack", gap="1")
    # Spike.12 (WHY§296): no copy of our task, so the console has the owner load the save (the map is read there too).
    failures += any_line(result, "LAUNCHER", "r1", step="no_confirmation", absent="1")
    return failures + any_line(result, "ELDENNET", "r1", stage="table_after") + en_log_is_clean(result)


case("launcher-eldennet-probe-gap", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}", "--probe"] +
     game("--title-frames", "1800", "--press-pause-at", "600", "--press-pause-every", "60", "--table-gap",
          frames=3300),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(None, probe=True, leads_base="Leads::NoTaskBaseGiven", then=with_saves),
     check=check_r1_gap,
     what="R1 as the owner runs it, on a fake game whose registration table holds no entry for the group (R1 run 7's "
          "end): the call answers 0, the table read before and after it shows the group's entry null and the others "
          "set, the task never runs, and both reports lint clean when the game ends")


# Spike.9's case (WHY§248), since spike.10 R1 run 9's end (WHY§289): G's method keeps only tasks of the game's kind
# (--task-gate, G strict). Our object holds zero where the kind is, so the method answers 1 keeping nothing, the task
# never runs, and no copy of its address is made, plain or tagged: R1 run 9 reproduced. The shelf holds G where run 9's
# entries held one object, so G is not walked as a root of its own.
def check_r1_gate(result):
    failures = []
    reports = r1_reports(result, failures)
    if reports is None:
        return failures
    p = reports["r1-probe.json"]
    task = result.truth["task"]
    checks = {(c["item"], c.get("label", "")): c for c in p["checks"]}
    need(all(checks.get((item, ""), {}).get("status") == "confirmed"
             for item in ("gate", "fd4_idiom", "register", "register_abi", "group")),
         "the checks: %s" % [(c["item"], c["status"]) for c in p["checks"]], failures)
    need(p["register"]["call"] == "ok" and p["stage"] == "registered" and p["task"]["invocations"] == 0 and
         not p["task"]["slot_calls"] and p["task"]["object_written"] == 0,
         "the call, the stage and the task: %s, %s, %s" % (p["register"]["call"], p["stage"], p["task"]), failures)
    failures += (check_r1_callee(p, task) + check_r1_table(p, task, r1_lead(result, "task_loom")) +
                 check_r1_roots(p, task, gate=True) + check_r1_spike10(p, task, gate=True, world=True))
    ds = r1_diffs(p, "task_hub")
    need(len(ds) == 2 and all(w["new"]["kind"] not in ("task", "task_tagged", "vtable", "ours")
                              for d in ds for w in d["words"]),
         "the hub's diffs, with nothing of ours: %s" % [[(w["path"], w["new"]["kind"]) for w in d["words"]]
                                                        for d in ds], failures)
    failures += check_r1_control(p, ds)
    ms = p.get("memory_search", [])
    need(len(ms) == 2 and all(m["stopped"] == "done" and m["found"] == 0 and m["inside"] == 0 and
                              m["vtable_found"] == 0 and m["slot_found"] == 0 and m["own_found"] >= 2 and
                              m["tagged_found"] == 0 and m["vtable_tagged_found"] == 0 for m in ms),
         "the memory searches, no copy, plain or tagged: %s" %
         [{k: m.get(k) for k in ("stopped", "found", "inside", "vtable_found", "slot_found", "own_found",
                                 "tagged_found")} for m in ms], failures)
    steps = r1_steps(result)
    need("task_confirmed" not in steps and steps[:1] == ["asked"] and "press_mark" in steps and
         steps[-1:] == ["reports_clean"], "the console's R1 steps: %s" % steps, failures)
    for name in ("r1-derive.json", "r1-probe.json"):
        failures += any_line(result, "LAUNCHER", "r1", report=name, lint="clean")
    failures += any_line(result, "FAKEGAME", "rack", gap="0", gate="1")
    failures += any_line(result, "FAKEGAME", "register", gated=str(p["register"]["group_index"]))
    failures += any_line(result, "LAUNCHER", "r1", step="no_confirmation", absent="1")  # spike.12, as the gap's
    return failures + any_line(result, "ELDENNET", "r1", stage="classes") + en_log_is_clean(result)


case("launcher-eldennet-probe-gate", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                              "--probe"] +
     game("--title-frames", "1800", "--press-pause-at", "600", "--press-pause-every", "60", "--task-gate",
          frames=3300),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(None, probe=True, leads_base="Leads::NoTaskBaseGiven", then=with_saves),
     check=check_r1_gate,
     what="R1 as the owner runs it, on a fake game whose ledger (G) keeps only tasks of the game's kind (R1 run 9's "
          "end): the call answers 1, G's method keeps nothing of our task, the task never runs, the searches find no "
          "copy, plain or tagged, G is the shelf's and not walked, and both reports lint clean")
# Spike.11's case (WHY§293): the ledger's deferred byte set (--task-queue), G's method takes R1 run 10's other path: it
# copies the request to the heap and hands it to G's queue, where R1 finds it at 50 and 250 ms; a later frame drains it
# (after FG_QUEUE_DRAIN_MS) and runs it, so the task runs, as in the default case, a second late.
def check_r1_queued(result):
    failures = check_r1(result, queued=True)
    failures += any_line(result, "FAKEGAME", "ledger", queued=str(result.truth["task"]["standin"]["index"]))
    failures += any_line(result, "FAKEGAME", "ledger", drained=str(result.truth["task"]["standin"]["index"]))
    return failures + any_line(result, "ELDENNET", "r1", stage="queue")


case("launcher-eldennet-probe-queue", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                               "--probe"] +
     game("--title-frames", "1800", "--press-pause-at", "600", "--press-pause-every", "60", "--task-queue",
          "--reload-every", "360", "--load-frames", "180", frames=3300),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(None, probe=True, leads_base="Leads::NoTaskBaseGiven", then=with_saves),
     check=check_r1_queued,
     what="R1 as the owner runs it, on a fake game whose ledger (G) queues each request (R1 run 10's other path): the "
          "request in G's queue at 50 and 250 ms, drained and run by a later frame, the task confirmed, and both "
          "reports lint clean")
# Spike.12's case (WHY§296): R1 run 11's end. The manager's map holds the stand-in group's word only once the world is
# loaded (--map-late), so the call at the title finds no match and drops our task (answering 1), the task never runs,
# and no copy of it is found: the console, after no confirmation, has the owner load the save and stay two minutes in
# the world (README-R1.txt's steps 4 and 5 say the same). The fake world then loads, adding its words, and the map's
# readings show the word absent at the title (its nearest words below and above) and present in the world.
R1_ABSENT = ("Our task is not in the game. Load your save now, stay in the world for two minutes, then quit the game "
             "as usual.")
README_R1 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "spikepkg",
                         "README-R1.txt.in")


def check_readme_r1(result):
    """README-R1.txt's steps (its source in tools/spikepkg) against the console: the line the console shows when our
    task is not in the game, quoted in step 4 with what to do (load the save, two minutes in the world, a grace
    enough), the old way kept for every other end, "Task confirmed." before step 5's play, and the time still 5 to 10
    minutes."""
    failures = []
    with open(README_R1, encoding="utf-8") as f:
        text = f.read()
    flat = " ".join(text.split())

    def step(n):  # a numbered step of the steps' section, to the next one, its lines joined
        body = text[text.find("\nThe steps\n"):text.find("\nWhat it does\n")]
        m = re.search(r"^%d\. (.*?)(?=^%d\. )" % (n, n + 1), body, re.M | re.S)
        return " ".join(m.group(1).split()) if m else ""

    step4, step5 = step(4), step(5)
    need("No confirmation 30 seconds after the call. " + R1_ABSENT in step4 and "two minutes" in step4 and
         "grace" in step4 and "Do not load a save" in step4 and "Task confirmed." in step4 and
         "Task confirmed." in step5 and "about 5 to 10 minutes" in flat,
         "README-R1.txt's steps 4 and 5: %r / %r" % (step4[:400], step5[:200]), failures)
    plain = [l for l in result.lines if l.startswith("EldenNet R1: ")]
    need(any(l == "EldenNet R1: No confirmation 30 seconds after the call. " + R1_ABSENT for l in plain) and
         not any("Do not load a save" in l for l in plain),
         "the console's lines: %s" % plain, failures)
    return failures


def check_r1_late(result):
    failures = []
    reports = r1_reports(result, failures)
    if reports is None:
        return failures
    p = reports["r1-probe.json"]
    task = result.truth["task"]
    checks = {(c["item"], c.get("label", "")): c for c in p["checks"]}
    need(all(checks.get((item, ""), {}).get("status") == "confirmed"
             for item in ("gate", "fd4_idiom", "register", "register_abi", "group")),
         "the checks: %s" % [(c["item"], c["status"]) for c in p["checks"]], failures)
    need(p["register"]["call"] == "ok" and p["stage"] == "registered" and p["task"]["invocations"] == 0 and
         not p["task"]["slot_calls"] and p["task"]["object_written"] == 0 and
         p["register"].get("returned_low") == 1,
         "the call, the stage and the task: %s, %s, %s" % (p["register"], p["stage"], p["task"]), failures)
    failures += (check_r1_callee(p, task) + check_r1_table(p, task, r1_lead(result, "task_loom")) +
                 check_r1_roots(p, task) + check_r1_spike10(p, task, late=True, world=True))
    ms = p.get("memory_search", [])
    need(len(ms) == 2 and all(m["stopped"] == "done" and m["found"] == 0 and m["inside"] == 0 and
                              m["vtable_found"] == 0 and m["slot_found"] == 0 and m["tagged_found"] == 0 for m in ms),
         "the memory searches, no copy: %s" %
         [{k: m.get(k) for k in ("stopped", "found", "inside", "vtable_found", "slot_found")} for m in ms], failures)
    steps = r1_steps(result)
    need("task_confirmed" not in steps and steps[:1] == ["asked"] and "press_mark" in steps and
         "no_confirmation" in steps and steps[-1:] == ["reports_clean"], "the console's R1 steps: %s" % steps, failures)
    failures += any_line(result, "LAUNCHER", "r1", step="no_confirmation", absent="1")
    for name in ("r1-derive.json", "r1-probe.json"):
        failures += any_line(result, "LAUNCHER", "r1", report=name, lint="clean")
    group = p["register"]["group_index"]
    failures += any_line(result, "FAKEGAME", "map", missed=str(group))
    failures += any_line(result, "FAKEGAME", "map", world_words=str(len(task["register_rack"]["map_world_words"]) + 1))
    failures += any_line(result, "ELDENNET", "r1", stage="task_search", found="0")
    failures += any_line(result, "ELDENNET", "r1", stage="map_change", word="present", world="world")
    return failures + check_readme_r1(result) + en_log_is_clean(result)


case("launcher-eldennet-probe-map", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                             "--probe"] +
     game("--title-frames", "1800", "--press-pause-at", "600", "--press-pause-every", "60", "--map-late",
          frames=3900),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=240,
     prepare=install_eldennet(None, probe=True, leads_base="Leads::NoTaskBaseGiven", then=with_saves),
     check=check_r1_late,
     what="R1 as the owner runs it, on a fake game whose manager's map holds the stand-in group's word only in the "
          "world (R1 run 11's end): the call at the title drops our task, no copy is found, the console has the owner "
          "load the save for two minutes (as README-R1.txt's steps say), and the map's readings show the word absent "
          "at the title and present once the world is loaded; both reports lint clean")
# Spike.10 (WHY§290): R1 run 9's launcher stopped logging at the call: its console's output was suspended (a click inside
# a classic console window, LIKELY), and the watch loop with it. Here the launcher's console writer blocks for good after
# a few lines (its test hook ELDENNET_TEST_CONSOLE, with a queue of a few lines): launcher.log still carries every line,
# the status lines, the 30 s no-confirmation step, the lint and the end; the console's dropped lines are counted there.
def launcher_log_lines(result):
    path = os.path.join(result.paths["appdata"], "EldenNet", "logs", "launcher.log")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8", errors="replace") as f:
        return [l.rstrip("\r\n") for l in f]


def check_console_blocked(*events, statuses=5):
    """The console blocked (stdout held its first few lines): each event in launcher.log and not on the console, status
    lines on after the block (with the console's status lines on), the first dropped line noted, and the totals at the
    end with lines dropped."""
    def check(result):
        failures = []
        log = launcher_log_lines(result)
        out = [l.rstrip("\r") for l in result.out.splitlines() if l.startswith("LAUNCHER ")]
        need(0 < len(out) <= 4, "%d LAUNCHER lines on the console, wanted 1 to 4" % len(out), failures)
        need(all(l in log for l in out), "the console's lines, not all in the log", failures)
        for prefix in events:
            need(any(l.startswith(prefix) for l in log) and not any(l.startswith(prefix) for l in out),
                 "%r: in the log %s, on the console %s" % (prefix, any(l.startswith(prefix) for l in log),
                                                            any(l.startswith(prefix) for l in out)), failures)
        status = [l for l in log if l.startswith("LAUNCHER status ")]
        need(len(status) >= statuses, "%d status lines in the log" % len(status), failures)
        need(any(l.startswith("LAUNCHER console dropped=1 queue_full=1") for l in log), "no first drop noted", failures)
        totals = [l for l in log if l.startswith("LAUNCHER console lines=")]
        kv = dict(x.split("=", 1) for x in totals[-1].split()[2:]) if totals else {}
        need(totals and int(kv.get("dropped", 0)) > 0 and kv.get("drained") == "0",
             "the console's totals: %s" % totals, failures)
        return failures
    return check


case("launcher-eldennet-probe-console", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                                 "--probe"] +
     game("--title-frames", "1800", "--press-pause-at", "600", "--press-pause-every", "60", "--task-gate",
          frames=3300),
     program="{launcher}", needs=EN_PROBE_NEEDS, toolchains=("clang-cl",), timeout=240, settle=True,
     env=dict(EN_ENV, ELDENNET_TEST_CONSOLE="block_after=3,queue=6"),
     prepare=install_eldennet(None, probe=True, leads_base="Leads::NoTaskBaseGiven", then=with_saves),
     check=lambda r: check_r1_gate(r) + check_console_blocked(
         "LAUNCHER r1 step=no_confirmation", "LAUNCHER r1 report=r1-probe.json lint=clean",
         "LAUNCHER r1 step=reports_clean", "LAUNCHER end ", "LAUNCHER finish ")(r),
     what="R1 as run 9 (G keeps nothing, --task-gate) with the launcher's console blocked after three lines: the watch "
          "loop, the status lines, the 30 s no-confirmation step, the lint and the end go on in launcher.log, the "
          "console's dropped lines counted there")
case("launcher-crash-console", LGAME + ["--dll", "{galtest}"] + game("--crash", "av-read", "--crash-at", "400",
                                                                      frames=720),
     program="{launcher}", needs=LGAL_NEEDS, toolchains=("clang-cl",), timeout=180, settle=True,
     env=dict(GALTEST_ENV, ELDENNET_TEST_CONSOLE="block_after=2,queue=4"),
     check=lambda r: check_crash("game")(r) + check_console_blocked("LAUNCHER crash ", "LAUNCHER end ", statuses=0)(r),
     what="the game crashes while the launcher's console is blocked after two lines: the crash watcher still lists the "
          "modules, keeps its record and logs the crash and the end")

case("launcher-eldennet-probe-release", LBASE + ["--no-steam-check", "--game", "{game}", "--dll", "{eldennetdll}",
                                                 "--probe"] + game("--stay-title", "--press-pause-at", "120",
                                                                   frames=600),
     program="{launcher}", needs=EN_NEEDS, toolchains=("clang-cl",), env=EN_ENV, timeout=120,
     prepare=install_eldennet(None),
     check=lambda r: (want(r, "ELDENNET", "probe", asked="r1", accepted=0, probe_build=0) +
                      want(r, "LAUNCHER", "init", state="exit") +
                      ([] if r1_steps(r) == ["asked", "refused", "reports_missing"] else
                       ["the console's R1 steps: %s" % r1_steps(r)]) +
                      any_line(r, "LAUNCHER", "r1", report="r1-probe.json", lint="missing") +
                      [f"the release DLL went on: {e}" for e in r.events("ELDENNET", "init")
                       if e.get("state") not in ("pin", "gate", "safety", "settings", "exit")] +
                      [l for l in r.lines if l.startswith("ELDENNET register")]),
     what="--probe with the release DLL: it refuses R1 at Settings and stops there, calling nothing; the console says "
          "so, and no report is left")


# --- A small PE reader of our own, for the checks from outside ------------------------------------------------------------

class PE:
    def __init__(self, data):
        self.data = data
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        if data[lfanew:lfanew + 4] != b"PE\0\0":
            raise ValueError("not a PE file")
        coff = lfanew + 4
        _, nsec, self.time_date_stamp, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, coff)
        opt = coff + 20
        if struct.unpack_from("<H", data, opt)[0] != 0x20B:
            raise ValueError("not PE32+")
        self.image_base = struct.unpack_from("<Q", data, opt + 24)[0]
        self.size_of_image = struct.unpack_from("<I", data, opt + 56)[0]
        ndirs = struct.unpack_from("<I", data, opt + 108)[0]
        self.dirs = [struct.unpack_from("<II", data, opt + 112 + 8 * i) for i in range(ndirs)]
        self.sections = []
        for i in range(nsec):
            name, vsize, va, rawsize, rawptr = struct.unpack_from("<8sIIII", data, opt + opt_size + 40 * i)
            self.sections.append((name.rstrip(b"\0").decode("ascii"), va, vsize, rawptr, rawsize))
        self.pdata = []
        rva, size = self.dirs[3]
        for off in range(0, size, 12):
            self.pdata.append(struct.unpack_from("<III", self.read(rva + off, 12)))

    def section(self, name):
        return next(s for s in self.sections if s[0] == name)

    def section_of(self, rva):
        for name, va, vsize, _, _ in self.sections:
            if va <= rva < va + vsize:
                return name
        return None

    def read(self, rva, n):
        for _, va, vsize, rawptr, rawsize in self.sections:
            if va <= rva < va + vsize:
                off = rva - va
                chunk = self.data[rawptr + off:rawptr + min(off + n, rawsize)]
                return chunk + b"\0" * (n - len(chunk))
        raise ValueError("rva 0x%X is in no section" % rva)

    def u32(self, rva):
        return struct.unpack("<I", self.read(rva, 4))[0]

    def u64(self, rva):
        return struct.unpack("<Q", self.read(rva, 8))[0]

    def cstr(self, rva, limit=260):
        raw = self.read(rva, limit + 1)
        return raw[:raw.index(b"\0")].decode("ascii") if b"\0" in raw else None

    def section_bytes(self, name):
        _, va, vsize, _, _ = self.section(name)
        return va, self.read(va, vsize)

    def function_of(self, rva):
        for begin, end, _ in self.pdata:
            if begin <= rva < end:
                return begin, end
        return None

    def version(self):
        """FileVersion, ProductName and the translation, from the version resource (type 16, id 1)."""
        rsrc, _ = self.dirs[2]

        def entries(dir_rva):
            named, ids = struct.unpack_from("<HH", self.read(dir_rva + 12, 4))
            for i in range(named + ids):
                name, target = struct.unpack("<II", self.read(dir_rva + 16 + 8 * i, 8))
                yield name, target

        def child(dir_rva, wanted=None):
            for name, target in entries(dir_rva):
                if wanted is None or name == wanted:
                    return target
            raise ValueError("no resource %s" % wanted)

        t = child(rsrc, 16)
        n = child(rsrc + (t & 0x7FFFFFFF), 1)
        lang = child(rsrc + (n & 0x7FFFFFFF))
        data_rva, size = struct.unpack("<II", self.read(rsrc + lang, 8))
        blob = self.read(data_rva, size)
        out = {}

        def node(off):
            length, vlen, kind = struct.unpack_from("<HHH", blob, off)
            k = off + 6
            end = k
            while blob[end:end + 2] != b"\0\0":
                end += 2
            key = blob[k:end].decode("utf-16-le")
            val = (end + 2 + 3) & ~3
            vbytes = vlen * 2 if kind == 1 else vlen
            return key, val, vbytes, kind, length

        def walk(off, limit, depth):
            while off < limit:
                key, val, vbytes, kind, length = node(off)
                if length == 0:
                    break
                if key == "VS_VERSION_INFO":
                    ms, ls = struct.unpack_from("<II", blob, val + 8)
                    out["file_version"] = "%d.%d.%d.%d" % (ms >> 16, ms & 0xFFFF, ls >> 16, ls & 0xFFFF)
                elif key == "Translation":
                    out["translation"] = list(struct.unpack_from("<HH", blob, val))
                elif key == "ProductName":
                    out["product_name"] = blob[val:val + vbytes].decode("utf-16-le").rstrip("\0")
                walk((val + vbytes + 3) & ~3, off + length, depth + 1)
                off = (off + length + 3) & ~3

        walk(0, len(blob), 0)
        return out


def pattern_regex(pattern):
    parts = []
    for tok in pattern.split():
        parts.append(b"." if tok == "??" else re.escape(bytes([int(tok, 16)])))
    return re.compile(b"".join(parts), re.DOTALL)


def disp32(blob, off):
    return struct.unpack_from("<i", blob, off)[0]


def check_file(pe, d, toolchain):
    """Checks every claim of --describe against the file, from outside, as discovery would (ARCH §6.2)."""
    failures = []
    img = d["image"]
    need(img["time_date_stamp"] == pe.time_date_stamp, "TimeDateStamp differs", failures)
    need(img["size_of_image"] == pe.size_of_image, "SizeOfImage differs", failures)
    ver = pe.version()
    need(ver.get("file_version") == "3.14.15.0" == img.get("file_version"), "file version %s" % ver, failures)
    need(ver.get("product_name") == PRODUCT_NAME == img.get("product_name"), "product name %s" % ver, failures)
    need(ver.get("translation") == [0x0809, 1200] == img.get("translation"), "translation %s" % ver, failures)
    need(img.get("region") == "ww", "region %s" % img.get("region"), failures)
    need([s["name"] for s in img["sections"]] == [s[0] for s in pe.sections], "sections differ", failures)

    text_va, text = pe.section_bytes(".text")
    func_starts = {b for b, _, _ in pe.pdata}

    # AOB items: one match each, inside a .pdata function at entry + 5 or later, the capture in .data.
    for item in d["aob"]:
        hits = [text_va + m.start() for m in pattern_regex(item["pattern"]).finditer(text)]
        role = item["role"]
        need(hits == [item["hit_rva"]], "%s: pattern hits %s, wanted [%s]" % (role, hits, item["hit_rva"]), failures)
        if not hits:
            continue
        fn = pe.function_of(hits[0])
        need(fn is not None and fn[0] == item["fn_rva"], "%s: .pdata function %s" % (role, fn), failures)
        if fn is not None:
            need(hits[0] - fn[0] >= 5, "%s: anchor %d below 5" % (role, hits[0] - fn[0]), failures)
            prologue = bytes(int(t, 16) for t in item["prologue"].split())
            need(pe.read(fn[0], len(prologue)) == prologue, "%s: prologue" % role, failures)
        if item["kind"] == "static":
            cap = item["capture"]
            target = hits[0] + cap["insn_end"] + disp32(pe.read(hits[0], 64), cap["at"])
            need(target == item["target_rva"], "%s: capture 0x%X" % (role, target), failures)
            need(pe.section_of(target) == ".data" and target % 8 == 0, "%s: target placement" % role, failures)

    # The FD4 finder (ARCH §6.2): every site the idiom matches, its shape and placement checks, its path's load as
    # --describe gives it (after the head in rcx or rdx, after a format string, through a pointer, by its absolute
    # address, or hoisted before the head), and naming by the class object.
    fd4 = d["fd4"]
    listed = {s["site"]: s for e in fd4["singletons"] + fd4["decoys"] for s in e["sites"]}
    hits = sorted(text_va + m.start() for m in pattern_regex(fd4["idiom"]).finditer(text))
    need(hits == sorted(listed), "fd4 idiom hits %s, listed %s" % (hits, sorted(listed)), failures)
    found = {}
    refused = []
    for hit in hits:
        b = pe.read(hit, 24)
        rip_static = (b[2] & 0xC7) == 0x05  # mod 00, r/m 101: RIP-relative
        static = hit + 7 + disp32(b, 3)
        cls = hit + 19 + disp32(b, 15)
        getter = hit + 24 + disp32(b, 20)
        load = listed.get(hit, {}).get("path_load", {})
        insn = load.get("insn", 0)
        if load.get("form") == "pointer":
            path = pe.u64(insn + 7 + disp32(pe.read(insn, 7), 3)) - pe.image_base
        elif load.get("form") == "absolute":
            path = pe.u64(insn + 2) - pe.image_base
        else:
            path = insn + 7 + disp32(pe.read(insn, 7), 3)
        path_text = pe.cstr(path) or ""
        ok = (rip_static and pe.section_of(static) == ".data" and static % 8 == 0 and pe.section_of(cls) == ".data"
              and pe.section_of(getter) == ".text" and getter in func_starts and pe.section_of(path) == ".rdata"
              and path_text.endswith(fd4["path_suffix"]))
        need(getter == fd4["getter_rva"], "fd4 site 0x%X: getter 0x%X" % (hit, getter), failures)
        name = pe.cstr(pe.u64(cls + fd4["name_ptr_off"]) - pe.image_base, 128)
        if ok:
            found.setdefault(name, set()).add(static)
        else:
            refused.append(name)
    resolved = {n: s.pop() for n, s in found.items() if len(s) == 1}
    ambiguous = sorted(n for n, s in found.items() if len(s) > 1)
    want = {e["name"]: e["static_rva"] for e in fd4["singletons"]}
    need(resolved == want, "fd4 resolved %s, wanted %s" % (resolved, want), failures)
    need(ambiguous == ["Duplicata"], "fd4 ambiguous %s" % ambiguous, failures)
    need(sorted(refused) == ["Mimicry", "Rockbound", "Strayling"], "fd4 refused %s" % refused, failures)
    need(len(want) == 11, "%d singletons, wanted 11" % len(want), failures)
    kinds = sorted({s["path_load"]["kind"] for s in listed.values()})
    need(kinds == [1, 2, 3, 4, 5, 6, 7], "fd4 path kinds %s" % kinds, failures)

    # The registration's call sites: each a call to the function, after the hub's static was loaded into rdx.
    task = d["task"]
    hub_static = want.get("TaskLoom")
    for site in task["register_call_sites"]:
        b = pe.read(site, 5)
        need(b[0] == 0xE8 and site + 5 + disp32(b, 1) == task["register_rva"], "call site 0x%X" % site, failures)
    loads = [text_va + m.start() for m in re.finditer(rb"\x48\x8B\x15", text)
             if text_va + m.start() + 7 + disp32(text, m.start() + 3) == hub_static]
    need(len(loads) >= 4, "%d loads of the hub's static into rdx" % len(loads), failures)

    if toolchain == "clang-cl":
        failures += check_rtti(pe, d)
    return failures


def check_rtti(pe, d):
    """MSVC RTTI (ARCH §6.2's walk): each class's type descriptor in .data, its locator in .rdata (signature 1, the
    self RVA), the vtable one pointer after the locator's slot, and the actor set from the hierarchy."""
    failures = []
    data_va, data = pe.section_bytes(".data")
    rdata_va, rdata = pe.section_bytes(".rdata")
    tds = {}
    for c in d["rtti"]["classes"]:
        i = data.find(c["mangled"].encode("ascii") + b"\0")
        need(i >= 16, "%s: no type descriptor in .data" % c["mangled"], failures)
        tds[c["mangled"]] = data_va + i - 16
    actor_td = data.find(b".?AVActor@FG@@\0")
    actor_td = data_va + actor_td - 16 if actor_td >= 0 else None
    for c in d["rtti"]["classes"]:
        name = c["name"]
        col_va = pe.u64(c["vtable_rva"] - 8)
        col = col_va - pe.image_base
        need(pe.section_of(col) == ".rdata", "%s: locator not in .rdata" % name, failures)
        sig, offset, _, td, chd, self_rva = struct.unpack("<IIIIII", pe.read(col, 24))
        need(sig == 1 and self_rva == col, "%s: locator signature %d self 0x%X" % (name, sig, self_rva), failures)
        need(td == tds.get(c["mangled"]), "%s: locator's type descriptor" % name, failures)
        need(offset == c["subobject_offset"], "%s: locator offset %d" % (name, offset), failures)
        _, _, nbases, bca = struct.unpack("<IIII", pe.read(chd, 16))
        bases = [pe.u32(pe.u32(bca + 4 * i)) for i in range(nbases)]
        need((actor_td in bases) == c["actor"], "%s: actor set membership" % name, failures)
    # The walk's own search: every locator in .rdata that names an actor class.
    cols = 0
    for off in range(0, len(rdata) - 24, 4):
        sig, _, _, td, chd, self_rva = struct.unpack_from("<IIIIII", rdata, off)
        if sig == 1 and self_rva == rdata_va + off and pe.section_of(td) == ".data":
            cols += 1
    need(cols >= len(d["rtti"]["classes"]), "only %d locators in .rdata" % cols, failures)
    return failures


# --- Running -------------------------------------------------------------------------------------------------------------

class Result:
    def __init__(self, code, out, err, killed, status_text=""):
        self.code = code
        self.out = out
        self.err = err
        self.killed = killed
        self.lines = [l.rstrip("\r") for l in out.splitlines()]
        # Lines the program wrote to its status file and not to standard output (a program started by another).
        self.status_lines = [l.rstrip("\r") for l in status_text.splitlines()]
        seen = set(self.lines)
        self.lines += [l for l in self.status_lines if l not in seen]

    def events(self, source, event, first_key=None):
        found = []
        for line in self.lines:
            parts = line.split(" ")
            if len(parts) < 2 or parts[0] != source or parts[1] != event:
                continue
            kv = {"_rest": " ".join(parts[2:])}
            for p in parts[2:]:
                k, sep, v = p.partition("=")
                if sep:
                    kv[k] = v
            if first_key is None or first_key in kv:
                found.append(kv)
        return found

    def last(self, source, event, first_key=None):
        found = self.events(source, event, first_key)
        return found[-1] if found else None

    def time_between(self, a, b):
        ta = next((float(e["t"]) for e in self.events("FAKEGAME", a) if "t" in e), None)
        tb = next((float(e["t"]) for e in self.events("FAKEGAME", b) if "t" in e), None)
        return -1 if ta is None or tb is None else tb - ta


def windows_path(path, native):
    if native:
        return path
    return "Z:" + os.path.abspath(path).replace("/", "\\")


def wine_env(args):
    env = dict(os.environ)
    if not args.native:
        env["WINEPREFIX"] = args.prefix
        env["WINEDEBUG"] = "-all"
        env["WINEDLLOVERRIDES"] = "mscoree,mshtml="
        env.pop("DISPLAY", None)
        # Wine 9.0 frees the TMPDIR variable's own string while it starts and aborts ("free(): invalid pointer") when
        # TMPDIR is set, as the cloud environment has set it since 2026-11-20 (WHY§210); Wine uses /tmp without it.
        env.pop("TMPDIR", None)
    return env


def wineserver(args):
    server = os.path.join(os.path.dirname(args.wine), "wineserver")
    return server if os.path.exists(server) else "wineserver"


def stop_wineserver(args, env):
    """Ends the prefix's wineserver and every program in it, and waits until the server has gone (`-k` asks it to
    end; `-w` waits for that), so that no run meets an earlier run's server, or its programs, while it shuts down."""
    subprocess.run([wineserver(args), "-k"], env=env, check=False, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=60)
    try:
        subprocess.run([wineserver(args), "-w"], env=env, check=False, timeout=60)
    except subprocess.TimeoutExpired:
        raise RuntimeError("the wineserver did not end within 60 s of wineserver -k")


def launch(args, argv, env, timeout, kill_after=None, settle=False, cwd=None, during=None):
    if args.native:
        cmd = argv
    else:
        stop_wineserver(args, env)  # whatever an earlier run left: each run starts with a server of its own
        cmd = [args.xvfb_run, "-a", "-s", "-screen 0 1024x768x24", args.wine] + argv
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, cwd=cwd,
                            start_new_session=not args.native)
    beside = None
    if during is not None:
        beside = threading.Thread(target=during, daemon=True)
        beside.start()
    killed = False
    try:
        out, err = proc.communicate(timeout=kill_after or timeout)
    except subprocess.TimeoutExpired:
        killed = True
        if args.native:
            proc.kill()
        else:
            os.killpg(proc.pid, signal.SIGKILL)
            stop_wineserver(args, env)
        out, err = proc.communicate()
    if settle and not args.native and not killed:
        # Processes the case's program started (a watcher) end by themselves: wait for them, and the server, first.
        try:
            subprocess.run([wineserver(args), "-w"], env=env, check=False, timeout=60)
        except subprocess.TimeoutExpired:
            pass
    if not args.native:
        stop_wineserver(args, env)
    if beside is not None:
        beside.join(timeout=30)
    return proc.returncode, out.decode("utf-8", "replace"), err.decode("utf-8", "replace"), killed


# The prefix's settings for unhandled exceptions. When no filter of the program's own ends one (MinGW's runtime
# filter passes an access violation on, MSVC's every exception but C++'s), Wine starts the JIT debugger that AeDebug
# names, `winedbg --auto` by default, which attaches, prints a backtrace and detaches; the fault is raised again, and
# the process ended with exit status 0 instead of the exception's code in 14 of 140 such runs. With a debugger that
# cannot start, Wine ends the process with the exception's code every time, as Windows does on a PC with no JIT
# debugger. `Auto` 1, or Wine would ask in a message box; and no crash dialog, which on Xvfb would wait for ever.
AEDEBUG = "HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"
SETUP_STEPS = (
    ["wineboot", "--init"],
    ["reg", "add", "HKCU\\Software\\Wine\\WineDbg", "/v", "ShowCrashDialog", "/t", "REG_DWORD", "/d", "0", "/f"],
    ["reg", "add", AEDEBUG, "/v", "Debugger", "/t", "REG_SZ", "/d", "no-jit-debugger-in-tests", "/f"],
    ["reg", "add", AEDEBUG, "/v", "Auto", "/t", "REG_SZ", "/d", "1", "/f"],
    # Wine's DXGI factory starts wined3d, whose OpenGL renderer hung on Xvfb in GraceUI's harness; with its Vulkan
    # renderer (Mesa's lavapipe, as Direct3D 12 uses) it did not (GraceUI's BUILD.md). Only the GraceUI cases' fake game
    # (--d3d12) makes a DXGI factory.
    ["reg", "add", "HKCU\\Software\\Wine\\Direct3D", "/v", "renderer", "/t", "REG_SZ", "/d", "vulkan", "/f"],
)


def cmd_setup(args):
    env = wine_env(args)
    if args.native:
        return 0
    os.makedirs(os.path.dirname(os.path.abspath(args.prefix)), exist_ok=True)
    for argv in SETUP_STEPS:
        code, out, err, _ = launch(args, argv, env, timeout=240)
        if code != 0:
            print("setup step %s failed with %s\n%s\n%s" % (" ".join(argv[:3]), code, out, err))
            return 1
    print("prefix ready: no crash dialog, no JIT debugger")
    return 0


# --- probe-exe: the derivation against --describe ------------------------------------------------------------------

def read_leads(path):
    """The leads file as {table: {key: value}}: its simple form only (key = "string", key = { k = "v" or true }),
    read here with no TOML library (CMake may find a Python older than 3.11, which has no tomllib)."""
    leads = {"": {}}
    table = ""
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            m = re.match(r"^\[([a-z_0-9]+)\]$", line)
            if m:
                table = m.group(1)
                leads.setdefault(table, {})
                continue
            key, _, value = (x.strip() for x in line.partition("="))
            if value.startswith("{"):
                inner = {}
                for part in value.strip("{} ").split(","):
                    k, _, v = (x.strip() for x in part.partition("="))
                    inner[k] = v.strip('"') if v.startswith('"') else v == "true"
                leads[table][key] = inner
            else:
                leads[table][key] = value.strip('"')
    return leads


def describe_truth(args, env):
    code, out, err, killed = launch(args, [args.exe, "--describe"], env, 60)
    lines = [l for l in out.splitlines() if l.startswith("{")]
    if code != 0 or killed or not lines:
        raise ValueError("--describe failed: exit %s" % code)
    return json.loads(lines[0])


def run_engine(args, env, argv):
    """Runs eldennet-engine with paths given as Windows sees them; answers its exit code and output."""
    code, out, err, killed = launch(args, [args.set_paths["engine"]] + argv, env, 120)
    if killed:
        raise ValueError("eldennet-engine timed out")
    want_bits = 0xFF if not args.native else 0xFFFFFFFF
    return code & want_bits, out, err


PRODUCT_NAME = "EldenNet Fake Game\u2122"   # fakegame.rc's, with its trademark sign


def check_fd4_diagnostics(d, truth):
    """derive.json's FD4 diagnostics (stats.fd4) against --describe: what the site search found at each reference to
    the path, where the finder found each accepted site's path, and R0's rule over the same hits."""
    failures = []
    tfd4 = truth["fd4"]
    g = d["stats"]["fd4"]
    accepted_names = {e["name"] for e in tfd4["singletons"]} | {"Duplicata"}
    sites = [(e["name"], s) for e in tfd4["singletons"] + tfd4["decoys"] for s in e["sites"]]
    # Each reference to the path: a site where the path follows the head in code (the first two kinds and rdx), none
    # where it is hoisted or the "static" is no RIP-relative load; the pointer and the absolute address are no
    # RIP-relative references to the path. The program's own C++ also names the suffix in a few string literals, which
    # end with it too: their references find no site either.
    refs = [s for _, s in sites if s["path_ok"] and s["path_load"]["form"] == "address"]
    outcome = {c["index"]: c["references"] for c in g.get("candidates", [])}
    want_sites = sum(1 for s in refs if s["path_load"]["kind"] in (1, 2, 3))
    total = g.get("path_references", 0)
    need(total >= len(refs) and outcome.get(0) == want_sites and sum(outcome.values()) == total and
         outcome.get(5, 0) >= len(refs) - want_sites, "fd4 references %s, outcomes %s" % (total, outcome), failures)
    # The accepted sites' path loads, bucket by bucket.
    want = {}
    for name, s in sites:
        if name not in accepted_names:
            continue
        p = s["path_load"]
        key = (("anchor", p["before"]),) if p["hoisted"] else (("at", p["at"]), ("insn_end", p["insn_end"]))
        key += (("length", p["length"]), ("index", p["ordinal"]), ("slot", p["reg"]), ("value", p["opcode"]))
        want[key] = want.get(key, 0) + 1
    got = {}
    for b in g.get("idiom", {}).get("captures", []):
        key = tuple((k, b[k]) for k in ("anchor", "at", "insn_end", "length", "index", "slot", "value") if k in b)
        got[key] = b.get("hits")
    need(got == want, "fd4 path loads %s, wanted %s" % (sorted(got.items()), sorted(want.items())), failures)
    # R0's rule: the first reference into .rdata taken for the path, and no decoding of the hit.
    r0 = g.get("refused", {})
    kinds = [s["path_load"]["kind"] for name, s in sites if name in accepted_names]
    need(g.get("accepted") == sum(1 for k in kinds if k in (1, 3)) and r0.get("path_text") == 1 + kinds.count(2) +
         kinds.count(4) and r0.get("no_path") == kinds.count(5) + kinds.count(6) and r0.get("static_section") == 1
         and r0.get("unreadable") == 1,
         "fd4 under R0's rule: %s accepted, %s" % (g.get("accepted"), r0), failures)
    need(d["fd4"]["refused"].get("unreadable") == 1, "fd4 refused %s: the decoy is no idiom" % d["fd4"]["refused"],
         failures)
    return failures


def check_derive(d, truth, leads, toolchain, failures):
    """Every claim of derive.json against --describe."""
    img, timg = d["image"], truth["image"]
    need(img["time_date_stamp"] == timg["time_date_stamp"], "TimeDateStamp", failures)
    need(img["size_of_image"] == timg["size_of_image"], "SizeOfImage", failures)
    need(".".join(str(v) for v in img.get("file_version", [])) == timg["file_version"], "file version", failures)
    need(img["region"] == timg["region"] == "ww", "region %s" % img["region"], failures)
    need(img["product_name"] == "matches", "product name %s" % img["product_name"], failures)
    # It matched once its trademark sign was removed: one character fewer.
    need(img.get("names") == {"of": len(PRODUCT_NAME), "agree": len(PRODUCT_NAME) - 1}, "product name's characters %s"
         % img.get("names"), failures)
    need(img.get("translation") == timg["translation"], "translation", failures)

    fd4, tfd4 = d["fd4"], truth["fd4"]
    need(fd4["status"] == "derived", "fd4 status %s" % fd4["status"], failures)
    idiom = fd4.get("idiom", {})
    need(idiom.get("bytes") == tfd4["idiom"], "fd4 idiom %s, wanted %s" % (idiom.get("bytes"), tfd4["idiom"]), failures)
    caps = {c["capture"]: (c["at"], c["insn_end"]) for c in idiom.get("captures", [])}
    for name in ("static", "class", "getter"):
        want = tfd4["operands"][name]
        need(caps.get(name) == (want["at"], want["insn_end"]), "fd4 capture %s %s" % (name, caps.get(name)), failures)
    need("path" not in caps, "fd4 path capture %s: the sites load their paths otherwise" % caps.get("path"), failures)
    need(idiom.get("scan_resolved") is True, "fd4 idiom not resolved by the scan", failures)
    names = fd4.get("names", {})
    need(names.get("offset") == tfd4["name_ptr_off"] and names.get("encoding") == tfd4["name_encoding"],
         "name layout %s" % names, failures)
    label_of = {name: label for label, name in leads["singletons"].items()}
    got = {s["label"]: s for s in fd4["singletons"]}
    for single in tfd4["singletons"]:
        s = got.get(label_of.get(single["name"]), {})
        need(s.get("status") == "derived" and s.get("static") == single["static_rva"] and
             s.get("class") == single["class_rva"] and s.get("sites") == len(single["sites"]) and
             s.get("getter") == tfd4["getter_rva"],
             "singleton %s: %s" % (single["name"], s), failures)
    want_decoys = {"Duplicata": ("ambiguous", None), "Strayling": ("refused", "path_text"),
                   "Rockbound": ("refused", "static_section")}
    for name, (status, refusal) in want_decoys.items():
        s = got.get(label_of.get(name), {})
        need(s.get("status") == status and s.get("refusal") == refusal, "decoy %s: %s" % (name, s), failures)
    failures += check_fd4_diagnostics(d, truth)

    reg, task = d["register"], truth["task"]
    need(reg["status"] == "derived", "register status %s" % reg["status"], failures)
    best = reg["candidates"][0] if reg.get("candidates") else {}
    abi = task["register_abi"]
    need(best.get("callee") == task["register_rva"], "register callee %s" % best.get("callee"), failures)
    need((best.get("instance_register"), best.get("task_register"), best.get("group_register")) ==
         (abi["instance"], abi["task"], abi["group"]), "register ABI %s" % best, failures)
    need(best.get("callers") == task["register_call_sites"], "register call sites %s" % best.get("callers"), failures)
    pat = reg.get("pattern", {})
    need(pat.get("matches") == 1 and pat.get("scan_resolved") is True and pat.get("entry") == task["register_rva"] and
         pat.get("anchor", 0) >= 5, "register pattern %s" % pat, failures)

    groups = [c["index"] for c in d["group"].get("candidates", [])]
    need(task["standin"]["index"] in groups[:1], "group index %s, wanted %s first" % (groups, task["standin"]["index"]),
         failures)
    # The group table's entries are 16 bytes ({ index, flags, name }): found at that stride, with its ten entries and
    # the index the entry holds.
    first = (d["group"].get("candidates") or [{}])[0]
    need((first.get("length"), first.get("entries"), first.get("value"), first.get("encoding")) ==
         (16, 10, task["standin"]["index"], "utf16"), "group table %s" % first, failures)

    rtti = d["rtti"]
    if toolchain != "clang-cl":
        need(rtti["locators"] == 0, "MSVC locators in GCC's build: %s" % rtti["locators"], failures)
        return
    classes = truth["rtti"]["classes"]
    sets = {s["label"]: s for s in rtti["sets"]}
    actors = sorted(c["vtable_rva"] for c in classes if c["actor"] and c["subobject_offset"] == 0)
    sub = sorted(c["vtable_rva"] for c in classes if c["actor"] and c["subobject_offset"] != 0)
    need(sets.get("actor", {}).get("primary") == actors and sets["actor"].get("subobject") == sub,
         "actor set %s, wanted %s and %s" % (sets.get("actor"), actors, sub), failures)
    player = [c["vtable_rva"] for c in classes if c["name"] == truth["rtti"]["player"]]
    need(sets.get("player", {}).get("primary") == player, "player set %s" % sets.get("player"), failures)
    tasks = sorted(c["vtable_rva"] for c in classes if c["bases"] == leads["task"]["base"])
    t = rtti["task"]
    need(t.get("vtables") == tasks and t.get("slots_min") == task["vtable_slots"] and t.get("run_slot") ==
         task["run_slot"], "task classes %s, wanted %s" % (t, tasks), failures)
    vt = {c["name"]: c["vtable_rva"] for c in classes}
    for st in d["statics"]:
        lead = leads["statics"][st["label"]]
        if "rtti" in lead:
            need(st.get("vtable") == vt.get(lead["rtti"]), "static %s: vtable %s" % (st["label"], st.get("vtable")),
                 failures)


def probe_exe(args, c, env):
    failures = []
    truth = describe_truth(args, env)
    leads = read_leads(args.set_paths["leads"])
    as_win = lambda p: windows_path(p, args.native)
    out = os.path.join(args.work, "derive-%s.json" % args.toolchain)
    base = ["probe-exe", as_win(args.exe), "--derive", "--out", as_win(out)]
    if c.name == "probe-exe":
        code, stdout, stderr = run_engine(args, env, base + ["--leads", as_win(args.set_paths["leads"])])
        need(code == 1, "exit %s, wanted 1 (the decoys are listed, so it is incomplete)" % code, failures)
        if os.path.exists(out):
            with open(out) as f:
                check_derive(json.load(f), truth, leads, args.toolchain, failures)
        else:
            failures.append("no derive.json")
        lcode, lout, _ = run_engine(args, env, ["lint", "--report", as_win(out)])
        need(lcode == 0 and "clean" in lout, "lint exit %s: %s" % (lcode, lout.strip()), failures)
        return failures, stdout + stderr
    if c.name == "probe-exe-slots":
        # The leads without the decoys, and the AOB statics' slots as R1 would find them.
        clean = os.path.join(args.work, "derive-leads-clean.toml")
        with open(args.set_paths["leads"]) as f:
            text = "".join(l for l in f if not re.match(r"^(duplicata|strayling|rockbound) ", l))
        with open(clean, "w") as f:
            f.write(text)
        # --describe lists the four AOB statics in this order (describe.cpp's items); the leads' labels follow it.
        statics = [a for a in truth["aob"] if a["kind"] == "static"]
        labels = ["hinstance", "save_ledger", "run_warden", "field_plot"]
        if len(statics) != len(labels):
            raise ValueError("--describe lists %d AOB statics, not 4" % len(statics))
        slots = dict(zip(labels, statics))
        argv = base + ["--leads", as_win(clean)]
        for label, a in sorted(slots.items()):
            argv += ["--static", "%s=0x%X" % (label, a["target_rva"])]
        code, stdout, stderr = run_engine(args, env, argv)
        want = 0 if args.toolchain == "clang-cl" else 1  # GCC's build has no MSVC RTTI, so its classes stay missing
        need(code == want, "exit %s, wanted %s" % (code, want), failures)
        with open(out) as f:
            d = json.load(f)
        for st in d["statics"]:
            a = slots[st["label"]]
            p = st.get("pattern", {})
            need(st["status"] == "derived" and st.get("slot") == a["target_rva"] and p.get("target") == a["target_rva"]
                 and p.get("matches") == 1 and p.get("scan_resolved") is True and p.get("anchor", 0) >= 5,
                 "static %s: %s" % (st["label"], st), failures)
            print("static %s: %s at entry + %s (the AOB function's own: %s)" %
                  (st["label"], p.get("bytes"), p.get("anchor"), p.get("entry") == a["fn_rva"]))
        lcode, lout, _ = run_engine(args, env, ["lint", "--report", as_win(out)])
        need(lcode == 0, "lint exit %s: %s" % (lcode, lout.strip()), failures)
        return failures, stdout + stderr
    if c.name == "probe-exe-sections":
        return probe_exe_sections(args, env, as_win)
    if c.name == "probe-exe-loose":
        return probe_exe_loose(args, env, as_win, base, out)
    # probe-exe-lint: a report the tool wrote, then the same with a path, a free string and a long byte run.
    code, stdout, stderr = run_engine(args, env, base + ["--leads", as_win(args.set_paths["leads"])])
    with open(out) as f:
        text = f.read()
    run = " ".join(["90"] * 80)
    doctored = {
        "path": text.replace('"label": "task_loom"', '"label": "C:\\\\Games\\\\game.exe"', 1),
        "free-string": text.replace('"region": "ww"', '"region": "somewhere else"', 1),
        "byte-run": re.sub(r'"bytes": "[^"]*"', '"bytes": "%s"' % run, text, count=1),
        "unknown-key": text.replace('"complete":', '"name": "TaskLoom",\n  "complete":', 1),
    }
    for kind, bad in doctored.items():
        need(bad != text, "%s: the doctoring changed nothing" % kind, failures)
        path = os.path.join(args.work, "derive-%s-%s.json" % (args.toolchain, kind))
        with open(path, "w") as f:
            f.write(bad)
        lcode, lout, _ = run_engine(args, env, ["lint", "--report", as_win(path)])
        need(lcode == 1 and "refused" in lout, "lint on %s: exit %s: %s" % (kind, lcode, lout.strip()), failures)
        need("C:" not in lout and "somewhere" not in lout and "TaskLoom" not in lout,
             "lint on %s quoted a value: %s" % (kind, lout.strip()), failures)
    ucode, _, uerr = run_engine(args, env, ["probe-exe", as_win(args.exe), "--derive"])
    need(ucode == 2, "probe-exe without --leads and --out: exit %s, wanted 2" % ucode, failures)
    mcode, _, merr = run_engine(args, env, base + ["--leads", as_win(os.path.join(args.work, "absent.toml"))])
    need(mcode == 4 and "absent" not in merr, "a missing leads file: exit %s: %s" % (mcode, merr.strip()), failures)
    ncode, _, nerr = run_engine(args, env, ["probe-exe", as_win(args.set_paths["leads"]), "--derive", "--out",
                                            as_win(out + ".x"), "--leads", as_win(args.set_paths["leads"])])
    need(ncode == 3 and "toml" not in nerr, "a file that is no executable: exit %s: %s" % (ncode, nerr.strip()),
         failures)
    return failures, stdout + stderr


# DataPiler's shape for a line of the tool's that its job's log shows (its devkit.py), and --sections' two shapes.
SHOWN_LINE = re.compile(r"(?:probe-exe|lint): [ -~]{1,240}")
HEX = r"0x[0-9a-f]+"
SECTIONS_HEAD = re.compile(r"probe-exe: image size (%s) file size (%s) section alignment (%s)" % (HEX, HEX, HEX))
SECTION_LINE = re.compile(r"probe-exe: section (\d+) va (%s) vsize (%s) raw (%s) rawsize (%s) flags (%s)" %
                          (HEX, HEX, HEX, HEX, HEX))


def section_table(data):
    """(the table's offset, the optional header's offset, each section's (va, vsize, raw, rawsize, flags))."""
    nt = struct.unpack_from("<I", data, 0x3C)[0]
    count, opt_size = struct.unpack_from("<H", data, nt + 6)[0], struct.unpack_from("<H", data, nt + 20)[0]
    at = nt + 24 + opt_size
    rows = []
    for i in range(count):
        vsize, va, rawsize, raw = struct.unpack_from("<IIII", data, at + 40 * i + 8)
        rows.append((va, vsize, raw, rawsize, struct.unpack_from("<I", data, at + 40 * i + 36)[0]))
    return at, nt + 24, rows


def clean_line(line):
    return SHOWN_LINE.fullmatch(line) is not None and "/" not in line and "\\" not in line


def tool_lines(*outputs):
    """The tool's own lines: xvfb-run sends a program's standard error to its standard output, with Wine's and X's
    lines between them."""
    return [l for o in outputs for l in o.splitlines() if l.startswith("probe-exe: ")]


def probe_exe_sections(args, env, as_win):
    """--sections on fakegame.exe against the file read here; then on a copy whose second section starts inside the
    first: --derive refuses it with one line naming the rule, the section and its numbers, and --sections still reads."""
    failures = []
    with open(args.exe, "rb") as f:
        data = f.read()
    at, opt, rows = section_table(data)
    code, out, err = run_engine(args, env, ["probe-exe", as_win(args.exe), "--sections"])
    lines = tool_lines(out, err)
    head = SECTIONS_HEAD.fullmatch(lines[0]) if lines else None
    need(code == 0 and head is not None and int(head.group(1), 16) == struct.unpack_from("<I", data, opt + 56)[0] and
         int(head.group(2), 16) == len(data) and int(head.group(3), 16) == struct.unpack_from("<I", data, opt + 32)[0],
         "--sections: exit %s, head %r" % (code, lines[:1]), failures)
    need(len(lines) == 1 + len(rows), "--sections: %d lines for %d sections" % (len(lines), len(rows)), failures)
    for i, (line, row) in enumerate(zip(lines[1:], rows)):
        m = SECTION_LINE.fullmatch(line)
        need(m is not None and int(m.group(1)) == i and tuple(int(g, 16) for g in m.groups()[1:]) == row,
             "--sections: section %d: %r" % (i, line), failures)
    need(all(clean_line(l) for l in lines), "--sections: a line DataPiler's log would not show", failures)
    bad = bytearray(data)
    struct.pack_into("<I", bad, at + 40 + 12, rows[0][0])
    path = os.path.join(args.work, "overlap-%s.exe" % args.toolchain)
    with open(path, "wb") as f:
        f.write(bad)
    dcode, dout, derr = run_engine(args, env, ["probe-exe", as_win(path), "--derive", "--leads",
                                               as_win(args.set_paths["leads"]), "--out", as_win(path + ".json")])
    errs = tool_lines(dout, derr)
    m = re.fullmatch(r"probe-exe: the executable is not a PE32\+ image for x64: a section is misplaced: section 1 "
                     r"starts below the end of section 0, (%s); va (%s) vsize (%s) raw (%s) rawsize (%s) flags (%s) "
                     r"\(at (%s)\)" % ((HEX,) * 7), errs[0] if errs else "")
    need(dcode == 3 and len(errs) == 1 and m is not None and clean_line(errs[0]) and
         tuple(int(g, 16) for g in m.groups()[1:6]) == (rows[0][0],) + rows[1][1:] and int(m.group(7), 16) == at + 40,
         "the overlap: exit %s: %r" % (dcode, errs), failures)
    scode, sout, serr = run_engine(args, env, ["probe-exe", as_win(path), "--sections"])
    need(scode == 0 and len(tool_lines(sout, serr)) == 1 + len(rows), "--sections on the overlap: exit %s" % scode,
         failures)
    return failures, out + err + dout + derr


def probe_exe_loose(args, env, as_win, base, out):
    """fakegame.exe with its last section's SizeOfRawData running 64 KiB past the end of the file and SizeOfImage
    0x10 short of the last section's alignment, as the loader lets them be: --derive answers as on the file itself,
    and derive.json differs in those two numbers only."""
    failures = []
    with open(args.exe, "rb") as f:
        data = f.read()
    at, opt, rows = section_table(data)
    last = len(rows) - 1
    loose = bytearray(data)
    struct.pack_into("<I", loose, at + 40 * last + 16, rows[last][3] + 0x10000)
    size_of_image = struct.unpack_from("<I", data, opt + 56)[0]
    struct.pack_into("<I", loose, opt + 56, size_of_image - 0x10)
    path = os.path.join(args.work, "loose-%s.exe" % args.toolchain)
    with open(path, "wb") as f:
        f.write(loose)
    leads = ["--leads", as_win(args.set_paths["leads"])]
    tcode, tout, terr = run_engine(args, env, base + leads)
    loose_out = os.path.join(args.work, "derive-loose-%s.json" % args.toolchain)
    lcode, lout, lerr = run_engine(args, env, ["probe-exe", as_win(path), "--derive", "--out", as_win(loose_out)] + leads)
    need(tcode == 1 and lcode == tcode, "exit %s on the loose copy, %s on the file" % (lcode, tcode), failures)
    if os.path.exists(out) and os.path.exists(loose_out):
        with open(out) as f:
            tidy = json.load(f)
        with open(loose_out) as f:
            loosed = json.load(f)
        need(loosed["image"]["size_of_image"] == size_of_image - 0x10 and
             loosed["image"]["sections"][last]["raw_size"] == rows[last][3] + 0x10000, "the loose copy's image",
             failures)
        for d in (tidy, loosed):
            d["image"].pop("size_of_image")
            d["image"]["sections"][last].pop("raw_size")
        need(tidy == loosed, "the loose copy derives other than the file", failures)
    else:
        failures.append("no derive.json")
    kcode, kout, _ = run_engine(args, env, ["lint", "--report", as_win(loose_out)])
    need(kcode == 0 and "clean" in kout, "lint on the loose copy's report: exit %s" % kcode, failures)
    return failures, tout + terr + lout + lerr


# --- The GAL in game: gal/ from the derivation, and the checks of GALTEST lines -----------------------------------------

STATIC_ITEMS = (("hinstance", "module_handle"), ("save_ledger", "save_data"), ("run_warden", "game_state"),
                ("field_plot", "field_area"))
WIDE = {"rcx": "rcx", "ecx": "rcx", "rdx": "rdx", "edx": "rdx", "r8": "r8", "r8d": "r8", "r9": "r9", "r9d": "r9"}
GAL_PROVENANCE = ('provenance = { method = "own-derivation", leads = ["the fake game\'s own leads"], '
                  'evidence = "docs/ops/probes/fakegame.json", date = "2026-11-16" }')

# The layout template's offsets against --describe's: (class.field of the template, a function of describe's layout).
def _off(cls, field):
    return lambda L: L[cls][field]["off"]

TEMPLATE_TRUTH = {
    "task_hub.group_count": lambda L, T: T["group_table"]["count_off"],
    "task_hub.groups": lambda L, T: T["group_table"]["ptr_off"],
    "task_group.name": lambda L, T: T["group_table"]["name_off"],
    "characters.by_distance": lambda L, T: L["ActorRosterImp"]["near_begin"]["off"],
    "characters.player_set": lambda L, T: L["ActorRosterImp"]["players"]["off"],
    "characters.player_set_len": lambda L, T: L["ActorRosterImp"]["player_count"]["off"],
    "characters.main_player": lambda L, T: L["ActorRosterImp"]["main_player"]["off"],
    "near_entry.chr": lambda L, T: L["ActorRosterImp"]["near_entry"]["actor"],
    "near_entry.distance": lambda L, T: L["ActorRosterImp"]["near_entry"]["distance"],
    "character.on_screen": lambda L, T: L["Actor"]["flags"]["off"],
    "character.dead": lambda L, T: L["Actor"]["flags"]["off"],
    "player_character.locked_on": lambda L, T: L["PlayerActor"]["locked_on"]["off"],
    "player_character.ride_state": lambda L, T: L["PlayerActor"]["mount_state"]["off"],
    "character_parts.reaction": lambda L, T: L["ActorParts"]["react"]["off"],
    "reaction.recv_dmg_type": lambda L, T: L["ReactPart"]["recv_dmg_type"]["off"],
    "grip.throw_state": lambda L, T: L["GripPart"]["throw_state"]["off"],
    "motion.pos": lambda L, T: L["MotionPart"]["pos"]["off"],
    "save_data.player_data": lambda L, T: L["SaveLedgerImp"]["hero"]["off"],
    "save_data.play_time": lambda L, T: L["SaveLedgerImp"]["play_time_ms"]["off"],
    "settings.show_subtitles": lambda L, T: L["SaveLedgerImp"]["settings_show_subtitles"]["off"],
    "game_state.talk_cam_id": lambda L, T: L["RunWardenImp"]["talk_cam_param_id"]["off"],
    "hud.boss_bars": lambda L, T: L["GaugeBoardImp"]["bars"]["off"],
    "popup.talk_id": lambda L, T: L["MenuDeskImp"]["popup_talk_id"]["off"],
    "world_clock.minute": lambda L, T: L["SkyClockImp"]["clock"]["off"] + 2,
    "world_clock.hour": lambda L, T: L["SkyClockImp"]["clock"]["off"] + 4,
    "view.matrix": lambda L, T: L["EyeRigImp"]["view_matrix"]["off"],
    "view.fov": lambda L, T: L["EyeRigImp"]["view_fov"]["off"],
    "view.aspect": lambda L, T: L["EyeRigImp"]["view_aspect"]["off"],
    "event_flags.map": lambda L, T: L["MarkLedgerImp"]["map_head"]["off"],
    "event_flags.holder_base": lambda L, T: L["MarkLedgerImp"]["holder"]["off"],
    "param_store.count": lambda L, T: L["TableVaultImp"]["table_count"]["off"],
}
# Fields whose names the template and --describe share, by class: template class -> describe class.
SAME_NAMES = {"character": "Actor", "character_parts": "ActorParts", "vitals": "VitalsPart",
              "save_data": "SaveLedgerImp", "player_data": "HeroSheet", "game_state": "RunWardenImp",
              "field_area": "FieldPlotImp", "session": "LobbyWardenImp", "hud": "GaugeBoardImp", "menus": "MenuDeskImp",
              "loading": "VeilKeeperImp", "camera": "EyeRigImp", "event_flags": "MarkLedgerImp",
              "flag_node": "FlagNode", "messages": "PhraseBookImp", "fmg": "PhraseTable", "fmg_group": "PhraseGroup",
              "param_store": "TableVaultImp", "param_table": "ParamSheet", "param_row": "ParamRowRef"}


def template_offsets(text):
    """{"class.field": off} from the layout template: its [class.X] and [class.X.fields] tables, inline or not."""
    out = {}
    cls = None
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        m = re.match(r"^\[class\.(\w+)(?:\.fields)?\]$", line)
        if m:
            cls = m.group(1)
            continue
        if line.startswith("["):
            cls = None
            continue
        if cls is None:
            continue
        for name, off in re.findall(r"(\w+) = \{ off = (0x[0-9A-Fa-f]+)", line):
            out["%s.%s" % (cls, name)] = int(off, 16)
    return out


def check_template(text, truth):
    failures = []
    offs = template_offsets(text)
    L, T = truth["layout"], truth["task"]
    checked = 0
    for key, off in sorted(offs.items()):
        cls, field = key.split(".")
        want = None
        if key in TEMPLATE_TRUTH:
            want = TEMPLATE_TRUTH[key](L, T)
        elif cls in SAME_NAMES and field in L.get(SAME_NAMES[cls], {}):
            want = L[SAME_NAMES[cls]][field]["off"]
        if want is None:
            continue
        checked += 1
        need(off == want, "template %s at 0x%X, the fake game's at 0x%X" % (key, off, want), failures)
    need(checked >= 70, "the template's offsets checked: only %d" % checked, failures)
    return failures


def make_gal(args, env):
    """gal/ for fakegame.exe as the derivation round gives it: probe-exe --derive with the leads (decoys left out) and
    R1's slots of the AOB statics (from --describe, standing in for the in-game probe), then builds.toml, patterns.toml,
    recipes.toml (the accessor as a shape) and layouts/fakegame.toml from derive.json and the template. Answers the folder and --describe's truth."""
    truth = describe_truth(args, env)
    leads = read_leads(args.set_paths["leads"])
    as_win = lambda p: windows_path(p, args.native)
    clean = os.path.join(args.work, "gal-leads.toml")
    with open(args.set_paths["leads"]) as f:
        text = "".join(l for l in f if not re.match(r"^(duplicata|strayling|rockbound) ", l))
    with open(clean, "w") as f:
        f.write(text)
    out = os.path.join(args.work, "gal-derive-%s.json" % args.toolchain)
    statics = [a for a in truth["aob"] if a["kind"] == "static"]
    argv = ["probe-exe", as_win(args.exe), "--derive", "--out", as_win(out), "--leads", as_win(clean)]
    for (label, _), a in zip(STATIC_ITEMS, statics):
        argv += ["--static", "%s=0x%X" % (label, a["target_rva"])]
    code, _, _ = run_engine(args, env, argv)
    if code != 0:
        raise ValueError("probe-exe --derive: exit %s" % code)
    with open(out) as f:
        d = json.load(f)
    gal = os.path.join(args.work, "gal-%s" % args.toolchain)
    os.makedirs(os.path.join(gal, "layouts"), exist_ok=True)
    img = d["image"]
    version = ".".join(str(v) for v in img["file_version"])
    key = "%s-%s" % (version, img["region"])
    with open(os.path.join(gal, "builds.toml"), "w") as f:
        f.write('schema = 1\n[[build]]\nkey = "%s"\napp = "%s"\nfile_version = "%s"\nproduct_name = "%s"\n'
                'region = "%s"\ntime_date_stamp = 0x%X\nsize_of_image = 0x%X\nlayout = "layouts/fakegame.toml"\n'
                'dlrf_name_layout = true\nstatus = "supported"\n'
                % (key, version, version, leads[""]["product_name"], img["region"], img["time_date_stamp"],
                   img["size_of_image"]))
    reg = d["register"]["pattern"]
    pats = ['schema = 1']
    pats.append('[[pattern]]\nid = "register_task"\nkind = "function"\nsection = ".text"\nbytes = "%s"\nanchor_min = %d\n'
                'fn_len = [%d, %d]\n%s' % (reg["bytes"], reg["anchor"], reg["fn_length"], reg["fn_length"],
                                           GAL_PROVENANCE))
    for st in d["statics"]:
        item = dict(STATIC_ITEMS)[st["label"]]
        p = st["pattern"]
        pats.append('[[pattern]]\nid = "%s"\nkind = "static"\nsection = ".text"\nbytes = "%s"\n'
                    'capture = { at = %d, kind = "rip_rel32", insn_end = %d }\ntarget_section = ".data"\n%s'
                    % (item, p["bytes"], p["capture_at"], p["capture_insn_end"], GAL_PROVENANCE))
    idiom = d["fd4"]["idiom"]
    caps = ", ".join('%s = { at = %d, kind = "rip_rel32", insn_end = %d, section = "%s"%s%s }'
                     % (c["capture"], c["at"], c["insn_end"], c["section"],
                        ", align = %d" % c["align"] if c["align"] > 1 else "",
                        ", function = true" if c["function"] else "") for c in idiom["captures"])
    pats.append('[[pattern]]\nid = "fd4_idiom"\nkind = "idiom"\nsection = ".text"\nbytes = "%s"\n'
                'expect_matches = [1, 256]\npath_suffix = "%s"\ncaptures = { %s }\n%s'
                % (idiom["bytes"], leads["fd4"]["path_suffix"], caps, GAL_PROVENANCE))
    with open(os.path.join(gal, "patterns.toml"), "w") as f:
        f.write("\n".join(pats) + "\n")
    # Lodestar's recipe for the same accessors, as a shape with no bytes (proposal 0005 §6; O102): where the folder
    # holds it, discovery finds the singletons by it and reads no idiom. The idiom above stays for R1's own check.
    with open(os.path.join(gal, "recipes.toml"), "w") as f:
        f.write('schema = 1\n[[recipe]]\nid = "accessor"\nkind = "accessor"\n'
                'steps = ["load_slot", "test_loaded", "branch_set_short", "address_arg1", "call_relative"]\n'
                'gives = { static = 0, class = 3, getter = 4 }\npath_suffix = "%s"\n%s\n'
                % (leads["fd4"]["path_suffix"], GAL_PROVENANCE))
    best = d["register"]["candidates"][0]
    with open(args.set_paths["gallayout"]) as f:
        template = f.read()
    layout = (template.replace("@BUILD@", key)
              .replace("@GROUP_INDEX@", str(d["group"]["candidates"][0]["index"]))
              .replace("@ABI_TASK@", WIDE[best["task_register"]])
              .replace("@ABI_INSTANCE@", WIDE[best["instance_register"]])
              .replace("@ABI_GROUP@", WIDE[best["group_register"]])
              .replace("@SLOTS@", str(d["rtti"]["task"]["slots_min"]))
              .replace("@RUN_SLOT@", str(d["rtti"]["task"]["run_slot"]))
              .replace("@OBJECT_SIZE@", "0x%X" % truth["task"]["object_size"])
              .replace("@NAME_OFF@", "0x%X" % d["fd4"]["names"]["offset"]))
    with open(os.path.join(gal, "layouts", "fakegame.toml"), "w") as f:
        f.write(layout)
    failures = check_template(template, truth)
    if failures:
        raise ValueError("the layout template disagrees with --describe: %s" % "; ".join(failures[:5]))
    return gal, truth


def write_builds(folder, exe):
    """builds.toml for the fake game's own fingerprint (its invented version resource; the rest from the file)."""
    with open(exe, "rb") as f:
        pe = PE(f.read())
    os.makedirs(os.path.join(folder, "layouts"), exist_ok=True)
    with open(os.path.join(folder, "builds.toml"), "w") as f:
        f.write('schema = 1\n[[build]]\nkey = "3.14.15.0-ww"\napp = "3.14.15.0"\nfile_version = "3.14.15.0"\n'
                'product_name = "EldenNet Fake Game"\nregion = "ww"\ntime_date_stamp = 0x%X\nsize_of_image = 0x%X\n'
                'layout = "layouts/fakegame.toml"\ndlrf_name_layout = true\nstatus = "supported"\n'
                % (pe.time_date_stamp, pe.size_of_image))
    return folder


def launcher_setup(args, env, c, paths):
    """A launcher case's folder: a copy of the fake game in game/, its own %APPDATA%, gal/ (the derivation's for the
    cases with galtest, else builds.toml alone), then the case's own files. Answers --describe's truth, or None."""
    case_dir = os.path.join(args.work, "%s-%s" % (args.toolchain, c.name))
    shutil.rmtree(case_dir, ignore_errors=True)
    game_dir = os.path.join(case_dir, "game")
    os.makedirs(game_dir)
    game_exe = os.path.join(game_dir, "fakegame.exe")
    shutil.copy2(args.exe, game_exe)
    appdata = os.path.join(case_dir, "appdata")
    os.makedirs(appdata)
    paths.update(case=case_dir, game=game_exe, gamedir=game_dir, appdata=appdata)
    truth = None
    if "gallayout" in c.needs:
        paths["galdir"], truth = make_gal(args, env)
    else:
        paths["galdir"] = write_builds(os.path.join(case_dir, "gal"), args.exe)
    if c.prepare is not None:
        c.prepare(paths)
    return truth


SCREEN = {"InGame": 1, "Loading": 2, "Title": 3}
LOBBY = {"None": 1, "Hosting": 2, "Joined": 3}


def check_gal(r, kind, own=False):
    """The GALTEST lines of one run against --describe's truth (r.truth). `own`: the run reads on a thread of the
    DLL's own (EN_GALTEST_FRAME=own), so nothing is registered with the fake game and its hub runs no task of ours."""
    failures = []
    truth = r.truth
    disc = r.last("GALTEST", "discovery")
    status = r.last("GALTEST", "status")
    world = r.last("GALTEST", "world")
    stores = r.last("GALTEST", "stores")
    if not (disc and status and world and stores):
        return ["no GALTEST discovery, status, world or stores line"]
    need(r.last("GALTEST", "gate") and r.last("GALTEST", "gate").get("key") == "3.14.15.0-ww", "gate", failures)
    need(disc["ready"] == "1" and disc["items"] == "16/16" and disc["passes"] == "1", "discovery %s" % disc, failures)
    need(disc["getter_calls"] == "0" and disc["rtti_missing"] == "0" and int(disc["fd4_names"]) >= 11,
         "discovery's names and RTTI %s" % disc, failures)
    need(all(g.endswith(":none") for g in disc["groups"].split(",") if not g.startswith("status:")),
         "discovery's groups %s" % disc["groups"], failures)
    # Every item where --describe says it is.
    rva = {e["label"]: int(e["rva"], 16) for e in r.events("GALTEST", "item")}
    roles = {"TaskLoom": "task_hub", "ActorRoster": "characters", "LobbyWarden": "session", "MarkLedger": "event_flags",
             "GaugeBoard": "hud", "MenuDesk": "menus", "VeilKeeper": "loading", "SkyClock": "world_clock",
             "EyeRig": "camera", "TableVault": "param_store", "PhraseBook": "messages"}
    for single in truth["fd4"]["singletons"]:
        need(rva.get(roles[single["name"]]) == single["static_rva"], "item %s" % roles[single["name"]], failures)
    aob = {a["role"]: a for a in truth["aob"]}
    for role, item in (("global_hinstance", "module_handle"), ("GameDataMan", "save_data"), ("GameMan", "game_state"),
                       ("FieldArea", "field_area")):
        need(rva.get(item) == aob[role]["target_rva"], "item %s" % item, failures)
    need(rva.get("register_task") == aob["register_task"]["fn_rva"], "item register_task", failures)
    reg = r.last("GALTEST", "registered")
    end = end_of(r)
    frame = r.last("GALTEST", "frame")
    need(frame is not None and frame.get("provider") == ("own_thread" if own else "game_task"), "frame %s" % frame,
         failures)
    if own:
        # No call into the game: nothing registered, the hub's tasks the program's own, none of its slots ever called.
        started = r.last("GALTEST", "reader")
        need(started is not None and started.get("started") == "1" and reg is None, "reader %s, registered %s" %
             (started, reg), failures)
        need(not r.events("FAKEGAME", "register") and not r.events("FAKEGAME", "task-active", "foreign"),
             "the fake game's registration was called", failures)
        need(end is not None and end.get("foreign_tasks") == "0" and end.get("foreign_runs") == "0",
             "foreign tasks %s" % (end or {}), failures)
        need(status["slot_calls"] == "0,0,0", "the game called our slots %s" % status["slot_calls"], failures)
    else:
        need(reg is not None and reg.get("ok") == "1", "registered %s" % reg, failures)
        need(len(r.events("FAKEGAME", "register", "group")) == 1, "registered other than once", failures)
        need(end is not None and end.get("foreign_tasks") == "1", "foreign tasks %s" % (end or {}).get("foreign_tasks"),
             failures)
    groups = dict(g.lower().split(":") for g in status["groups"].split(","))
    inv = int(status["invocations"])
    snaps = int(status["snapshots"])
    slots = [int(x) for x in status["slot_calls"].split(",")]
    # The hub runs our task once a frame, from the frame it activated it to the last: counted in frames, so that a slow
    # machine or a slow first frame changes nothing; and registration came in the run's first half.
    active = r.last("FAKEGAME", "task-active", "frame")
    if own:
        pass
    elif end is not None and active is not None:
        frames, first = int(end["frames"]), int(active["frame"])
        need(first <= frames // 2 and int(end["foreign_runs"]) == frames - first + 1,
             "our task ran %s times from frame %d to %d" % (end["foreign_runs"], first, frames), failures)
    else:
        failures.append("no task-active or end line")
    if kind == "group-shift":
        need(status["state"] == "inert" and status["inert_why"] == "group_mismatch" and status["group_found_at"] == "6",
             "group check %s" % status, failures)
        need(snaps == 0, "snapshots when inert: %d" % snaps, failures)
        need(any(re.search(r"^FAKEGAME task-active group=5 name=Fg_ActorMove foreign=1", l) for l in r.lines),
             "the task joined another group", failures)
        return failures
    if kind == "fault-world":
        need(status["state"] == "inert" and status["inert_why"] == "structural", "inert %s" % status, failures)
        need(groups["world"] == "tripped" and int(status["fault_copy"]) >= 3, "world's breaker %s" % status, failures)
        need(int(stores["trip_groups"], 16) & (1 << 1), "a GroupTripped for world", failures)
        return failures
    need(status["state"] == "running" and (own or status["group_ok"] == "1"), "task %s" % status, failures)
    # 4 Hz by the clock since the first tick (R10), never more, whatever the frame rate a loaded machine gives. Under
    # Wine each ReadProcessMemory goes through the server: the builds pass 0.5 ms and the task throttles to 2 Hz, which
    # the rpm case allows (WHY§161).
    elapsed = int(status["elapsed_ms"]) / 1000.0
    built = snaps + int(status["abandoned"])
    # On a thread of our own: 10 Hz, and a reading out of step with the frame (torn, or a change of lifecycle held
    # for its second sighting) is dropped and counted, not published.
    rate = 10 if own else 4
    dropped = int(status["torn"]) + int(status["held"])
    need(own or dropped == 0, "readings dropped on the game's thread: %s" % dropped, failures)
    floor = 0.85 * (2 if kind == "rpm" else rate / 2 if own else rate) * elapsed - 3
    need(elapsed >= 4 and floor <= built + dropped and built <= rate * elapsed + 2,
         "%d snapshots (and %d dropped) in %.1f s" % (built, dropped, elapsed), failures)
    # The run slot every frame since registration; the hub's attach once; its query every 64th frame.
    # (The status line reads its counters one by one while the game runs: they may differ by a frame or two.)
    need(own or (abs(slots[1] - inv) <= 2 and slots[0] == 1 and inv // 64 - 1 <= slots[2] <= inv // 64 + 2),
         "slot calls %s for %d invocations" % (slots, inv), failures)
    need(status["allocs"] == "0" and status["frees"] == "0", "allocations on the game thread %s/%s" %
         (status["allocs"], status["frees"]), failures)
    need(int(status["task_p99"]) < 20000, "task p99 %s us" % status["task_p99"], failures)
    need(stores["data_ready"] == "1", "DataReady %s" % stores["data_ready"], failures)
    want_trips = {"fault-camera": 1, "rpm": 1}.get(kind, 0)
    need(int(stores["trips"]) == want_trips, "trips %s" % stores["trips"], failures)
    for g, state in groups.items():
        want = "tripped" if g == "status" or (g == "camera" and want_trips) else "ok"
        need(state == want, "group %s %s" % (g, state), failures)
    if want_trips:
        need(int(stores["trip_groups"], 16) == 1 << 8, "tripped groups 0x%s" % stores["trip_groups"], failures)
    if kind == "online":
        need(world["lobby"] == str(LOBBY["Joined"]) and status["mode"] == "dormant" and int(status["dormant"]) >= 4,
             "online: %s %s" % (world["lobby"], status["mode"]), failures)
        return failures
    full = snaps - int(status["loading_only"]) - int(status["dormant"])  # flag frames ride on full snapshots only
    need(int(status["flag_frames"]) >= full // 4 - 2, "flag frames %s of %d" % (status["flag_frames"], full), failures)
    # The world's values, as the fake game holds them.
    want = {"screen": SCREEN["InGame"], "lobby": LOBBY["None"], "role": 1, "player_set": 1, "chars": 6, "dead": 1,
            "level": 12, "runes": 4321, "slot": 2, "place": 6100100, "hud": 4, "subtitles": 1, "boss_bars": 1}
    if kind == "reload":
        # The run ends on a loading screen (frame 871 on), where a snapshot reads the lifecycle subset only (R11): the
        # last screen may be either, and the session and the world are those the last snapshots that read them saw.
        want["screen"] = None
        need(end is not None and end.get("phase") == "loading", "the run ended in %s" % (end or {}).get("phase"),
             failures)
        need(world["screen"] in (str(SCREEN["InGame"]), str(SCREEN["Loading"])), "screen %s" % world["screen"],
             failures)
    for k, v in want.items():
        if v is not None:
            need(world[k] == str(v), "world %s=%s, wanted %s" % (k, world[k], v), failures)
    need(world["hp"] == "1100/1200", "hp %s" % world["hp"], failures)
    need(int(world["hour"]) == 9 and int(world["play_ms"]) > 3000, "clock %s:%s, play %s" %
         (world["hour"], world["minute"], world["play_ms"]), failures)
    need(int(world["near_param"]) in range(81100, 81700, 100), "near param %s" % world["near_param"], failures)
    # The stores: the watch list 100, 7005, 4321010 and 999000; the texts and the rows asked for.
    need(stores["flag_version"] == "9" and stores["flag_valid"] == "0x7", "flags %s" % stores, failures)
    need(stores["flag_bits"] in ("0x7", "0x5"), "flag bits %s" % stores["flag_bits"], failures)
    need(stores["texts"] == "3" and stores["text_named"] == "2" and stores["text_empty"] == "1", "texts %s" % stores,
         failures)
    need(stores["params"] == "1" and stores["param_name_id"] == "1101", "params %s" % stores, failures)
    need(int(stores["playthroughs"]) >= (2 if kind == "reload" else 1) and stores["name_len"] == "13",
         "playthroughs %s, name %s" % (stores["playthroughs"], stores["name_len"]), failures)
    need(int(status["renders"]) > 100 and int(status["render_present"]) > 100, "render feed %s/%s" %
         (status["renders"], status["render_present"]), failures)
    # The probe's slot searches found each AOB static's slot.
    found = {}
    for part in stores["slots"].split(","):
        label, progress, slot = part.split(":")
        found[label] = int(slot, 16)
    for role, item in (("GameDataMan", "save_data"), ("GameMan", "game_state"), ("FieldArea", "field_area")):
        need(found.get(item) == aob[role]["target_rva"], "slot search %s: 0x%X" % (item, found.get(item, 0)), failures)
    if kind == "reload":
        need(int(status["loading_only"]) >= 4, "loading snapshots %s" % status["loading_only"], failures)
    if kind == "moves":
        need(end is not None and int(end.get("moves", 0)) >= 6, "moves %s" % (end or {}).get("moves"), failures)
    if kind == "rpm":
        need(r.last("GALTEST", "boot").get("copy") == "rpm" and int(status["fault_injected"]) >= 3,
             "rpm %s" % status, failures)
    return failures


def cmd_run(args):
    c = next((c for c in CASES if c.name == args.case), None)
    if c is None:
        print("no case %s" % args.case)
        return 1
    os.makedirs(args.work, exist_ok=True)
    paths = {"exe": args.exe, "vehprobe": args.vehprobe, "work": args.work,
             "status": os.path.join(args.work, "%s-%s.status" % (args.toolchain, c.name))}
    paths.update(dict(kv.split("=", 1) for kv in args.set))
    if os.path.exists(paths["status"]):
        os.remove(paths["status"])
    as_windows = lambda m: windows_path(paths[m.group(1)], args.native)
    if not c.name.startswith(("launcher-", "gal-")):  # those cases make their paths first
        argv = [re.sub(r"\{(\w+)\}", lambda m: paths[m.group(1)], c.program)]
        argv += [re.sub(r"\{(\w+)\}", as_windows, a) for a in c.args]
    env = wine_env(args)
    truth = None
    if c.name.startswith("launcher-"):
        args.set_paths = paths
        paths["as_windows"] = lambda p: windows_path(p, args.native)
        try:
            truth = launcher_setup(args, env, c, paths)
        except (OSError, ValueError, KeyError, IndexError) as e:
            print("case %s: its setup failed: %r" % (c.name, e))
            print("FAILED")
            return 1
        argv = [re.sub(r"\{(\w+)\}", lambda m: paths[m.group(1)], c.program)]
        argv += [re.sub(r"\{(\w+)\}", as_windows, a) for a in c.args]
    if c.name.startswith("gal-"):
        args.set_paths = paths
        try:
            paths["galdir"], truth = make_gal(args, env)
        except (OSError, ValueError, KeyError, IndexError) as e:
            print("case %s: making gal/ failed: %r" % (c.name, e))
            print("FAILED")
            return 1
        argv = [re.sub(r"\{(\w+)\}", lambda m: paths[m.group(1)], c.program)]
        argv += [re.sub(r"\{(\w+)\}", as_windows, a) for a in c.args]
    # A `{text_...}` name stands for a value of the case's own (no path): it is put in as it is.
    env.update({k: re.sub(r"\{(\w+)\}", lambda m: paths[m.group(1)] if m.group(1).startswith("text_") else
                          as_windows(m), v) for k, v in c.env.items()})
    if c.name.startswith("probe-exe"):
        args.set_paths = paths
        start = time.monotonic()
        try:
            failures, output = probe_exe(args, c, env)
        except (OSError, ValueError, KeyError, IndexError) as e:
            failures, output = ["probe-exe: %r" % e], ""
        print("case %s (%s)" % (c.name, c.what))
        print("in %.1f s" % (time.monotonic() - start))
        if failures or args.verbose:
            print("--- output\n%s" % output[-6000:])
        for f in failures:
            print("FAIL: %s" % f)
        print("PASS" if not failures else "FAILED")
        return 0 if not failures else 1
    start = time.monotonic()
    cwd = re.sub(r"\{(\w+)\}", lambda m: paths[m.group(1)], c.cwd) if c.cwd else None  # Wine maps it to its drive
    during = {}
    beside = (lambda: c.during(paths, during)) if c.during is not None else None
    code, out, err, killed = launch(args, argv, env, c.timeout, c.kill_after, c.settle, cwd, beside)
    elapsed = time.monotonic() - start
    status_text = ""
    if os.path.exists(paths["status"]):
        with open(paths["status"], encoding="utf-8", errors="replace") as f:
            status_text = f.read()
    if "case" in paths:  # a watcher started by the case writes its lines to its log only
        log = os.path.join(paths["appdata"], "EldenNet", "logs", "launcher.log")
        if c.settle and os.path.exists(log):
            with open(log, encoding="utf-8", errors="replace") as f:
                status_text += f.read()
        en_log = os.path.join(paths["appdata"], "EldenNet", "logs", "eldennet.log")  # eldennet.dll's (part 6)
        if os.path.exists(en_log):
            with open(en_log, encoding="utf-8", errors="replace") as f:
                status_text += f.read()
    result = Result(code, out, err, killed, status_text)
    result.during = during
    result.truth = truth
    result.paths = paths
    result.args = args
    result.env = env
    result.as_win = lambda p: windows_path(p, args.native)
    failures = []
    if c.kill_after is not None:
        need(killed, "it ended by itself; it should have hung", failures)
    elif killed:
        failures.append("timed out after %d s" % c.timeout)
    if c.exit is not None:
        want = c.exit if args.native else c.exit & 0xFF
        need(code == want, "exit %s, wanted %s (0x%X)" % (code, want, c.exit), failures)
    for pattern in c.expect:
        need(any(re.search(pattern, l) for l in result.lines), "no line matches %r" % pattern, failures)
    for pattern in c.forbid:
        need(not any(re.search(pattern, l) for l in result.lines), "a line matches %r" % pattern, failures)
    if c.check is not None:
        failures += c.check(result)
    if c.name == "describe":
        try:
            d = json.loads(next(l for l in result.lines if l.startswith("{")))
            need(d["self_check"]["ok"], "self_check: %s" % d["self_check"]["failures"], failures)
            need(d["fakegame"]["toolchain"] == args.toolchain, "toolchain %s" % d["fakegame"]["toolchain"], failures)
            with open(args.exe, "rb") as f:
                failures += check_file(PE(f.read()), d, args.toolchain)
            with open(os.path.join(args.work, "describe-%s.json" % args.toolchain), "w") as f:
                json.dump(d, f, indent=1, sort_keys=True)
        except (StopIteration, ValueError, KeyError) as e:
            failures.append("describe: %r" % e)
    print("case %s (%s): %s" % (c.name, c.what, " ".join(c.args)))
    print("exit %s%s in %.1f s" % (code, " (killed)" if killed else "", elapsed))
    if failures or args.verbose:
        print("--- stdout\n%s\n--- stderr\n%s" % (out[-6000:], err[-3000:]))
    for f in failures:
        print("FAIL: %s" % f)
    print("PASS" if not failures else "FAILED")
    return 0 if not failures else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("list", "setup", "run"):
        p = sub.add_parser(name)
        p.add_argument("--toolchain", choices=("clang-cl", "mingw"), default="clang-cl")
        p.add_argument("--native", action="store_true", help="run on Windows itself, with no Wine")
        p.add_argument("--wine")
        p.add_argument("--xvfb-run")
        p.add_argument("--prefix")
        p.add_argument("--have", default="", help="names given with --set, comma-separated (for list)")
        if name == "run":
            p.add_argument("--set", action="append", default=[], metavar="NAME=PATH")
            p.add_argument("case")
            p.add_argument("--exe", required=True)
            p.add_argument("--vehprobe", required=True)
            p.add_argument("--work", required=True)
            p.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    if args.command == "list":
        have = set(filter(None, args.have.split(",")))
        for c in CASES:
            if args.toolchain in c.toolchains and set(c.needs) <= have:
                print(c.name)
        return 0
    if not args.native and not (args.wine and args.xvfb_run and args.prefix):
        parser.error("--wine, --xvfb-run and --prefix are needed unless --native")
    return cmd_setup(args) if args.command == "setup" else cmd_run(args)


if __name__ == "__main__":
    sys.exit(main())
