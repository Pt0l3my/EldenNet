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
import signal
import struct
import subprocess
import sys
import time

AV = 0xC0000005

# --- Cases ---------------------------------------------------------------------------------------------------------------


class Case:
    """One behaviour: the program and its arguments (`{name}` stands for a path: exe, vehprobe, work, status, or one
    given with --set name=PATH, as Windows sees it), what must come back, and what the case stands for. A case that
    `needs` a path given with --set is listed only when CMake has it (--have), so later parts (the DLL, the launcher)
    add their chain cases here: program="{launcher}", needs=("launcher", "eldennet")."""

    def __init__(self, name, args, exit=0, expect=(), forbid=(), check=None, env=None, toolchains=("clang-cl", "mingw"),
                 timeout=60, kill_after=None, what="", program="{exe}", needs=()):
        self.name = name
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
case("gal-group-shift", GAL + ["--frames", "300", "--shift-groups", "1", "--load", "ready:{galtest}"], env=GAL_ENV,
     needs=GAL_NEEDS, toolchains=("clang-cl",), timeout=120, check=lambda r: check_gal(r, "group-shift"),
     what="the group table shifted since the derivation: the first tick finds another name there and goes inert")

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
    need(ver.get("product_name") == "EldenNet Fake Game" == img.get("product_name"), "product name %s" % ver, failures)
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

    # The FD4 finder (ARCH §6.2): every site the idiom matches, its placement checks, and naming by the class object.
    fd4 = d["fd4"]
    listed = sorted(s["site"] for e in fd4["singletons"] + fd4["decoys"] for s in e["sites"])
    hits = sorted(text_va + m.start() for m in pattern_regex(fd4["idiom"]).finditer(text))
    need(hits == listed, "fd4 idiom hits %s, listed %s" % (hits, listed), failures)
    found = {}
    refused = []
    for hit in hits:
        b = pe.read(hit, 45)
        static = hit + 7 + disp32(b, 3)
        cls = hit + 19 + disp32(b, 15)
        getter = hit + 24 + disp32(b, 20)
        path = hit + 40 + disp32(b, 36)
        path_text = pe.cstr(path) or ""
        ok = (pe.section_of(static) == ".data" and static % 8 == 0 and pe.section_of(cls) == ".data"
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
    need(sorted(refused) == ["Rockbound", "Strayling"], "fd4 refused %s" % refused, failures)
    need(len(want) == 11, "%d singletons, wanted 11" % len(want), failures)

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


def launch(args, argv, env, timeout, kill_after=None):
    if args.native:
        cmd = argv
    else:
        stop_wineserver(args, env)  # whatever an earlier run left: each run starts with a server of its own
        cmd = [args.xvfb_run, "-a", "-s", "-screen 0 1024x768x24", args.wine] + argv
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
                            start_new_session=not args.native)
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
    if not args.native:
        stop_wineserver(args, env)
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


def check_derive(d, truth, leads, toolchain, failures):
    """Every claim of derive.json against --describe."""
    img, timg = d["image"], truth["image"]
    need(img["time_date_stamp"] == timg["time_date_stamp"], "TimeDateStamp", failures)
    need(img["size_of_image"] == timg["size_of_image"], "SizeOfImage", failures)
    need(".".join(str(v) for v in img.get("file_version", [])) == timg["file_version"], "file version", failures)
    need(img["region"] == timg["region"] == "ww", "region %s" % img["region"], failures)
    need(img["product_name"] == "matches", "product name %s" % img["product_name"], failures)
    need(img.get("translation") == timg["translation"], "translation", failures)

    fd4, tfd4 = d["fd4"], truth["fd4"]
    need(fd4["status"] == "derived", "fd4 status %s" % fd4["status"], failures)
    idiom = fd4.get("idiom", {})
    need(idiom.get("bytes") == tfd4["idiom"], "fd4 idiom %s, wanted %s" % (idiom.get("bytes"), tfd4["idiom"]), failures)
    caps = {c["capture"]: (c["at"], c["insn_end"]) for c in idiom.get("captures", [])}
    for name, key in (("static", "static"), ("class", "class"), ("getter", "getter"), ("path", "path")):
        want = tfd4["operands"][key]
        need(caps.get(name) == (want["at"], want["insn_end"]), "fd4 capture %s %s" % (name, caps.get(name)), failures)
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
    R1's slots of the AOB statics (from --describe, standing in for the in-game probe), then builds.toml, patterns.toml
    and layouts/fakegame.toml from derive.json and the template. Answers the folder and --describe's truth."""
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


SCREEN = {"InGame": 1, "Loading": 2, "Title": 3}
LOBBY = {"None": 1, "Hosting": 2, "Joined": 3}


def check_gal(r, kind):
    """The GALTEST lines of one run against --describe's truth (r.truth)."""
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
    need(reg is not None and reg.get("ok") == "1", "registered %s" % reg, failures)
    need(len(r.events("FAKEGAME", "register", "group")) == 1, "registered other than once", failures)
    end = end_of(r)
    need(end is not None and end.get("foreign_tasks") == "1", "foreign tasks %s" % (end or {}).get("foreign_tasks"),
         failures)
    groups = dict(g.lower().split(":") for g in status["groups"].split(","))
    inv = int(status["invocations"])
    snaps = int(status["snapshots"])
    slots = [int(x) for x in status["slot_calls"].split(",")]
    # The hub runs our task once a frame, from the frame it activated it to the last: counted in frames, so that a slow
    # machine or a slow first frame changes nothing; and registration came in the run's first half.
    active = r.last("FAKEGAME", "task-active", "frame")
    if end is not None and active is not None:
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
    need(status["state"] == "running" and status["group_ok"] == "1", "task %s" % status, failures)
    # 4 Hz by the clock since the first tick (R10), never more, whatever the frame rate a loaded machine gives. Under
    # Wine each ReadProcessMemory goes through the server: the builds pass 0.5 ms and the task throttles to 2 Hz, which
    # the rpm case allows (WHY§161).
    elapsed = int(status["elapsed_ms"]) / 1000.0
    built = snaps + int(status["abandoned"])
    floor = 0.85 * (2 if kind == "rpm" else 4) * elapsed - 3
    need(elapsed >= 4 and floor <= built <= 4 * elapsed + 2, "%d snapshots in %.1f s" % (built, elapsed), failures)
    # The run slot every frame since registration; the hub's attach once; its query every 64th frame.
    # (The status line reads its counters one by one while the game runs: they may differ by a frame or two.)
    need(abs(slots[1] - inv) <= 2 and slots[0] == 1 and inv // 64 - 1 <= slots[2] <= inv // 64 + 2,
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
    argv = [re.sub(r"\{(\w+)\}", lambda m: paths[m.group(1)], c.program)]
    argv += [re.sub(r"\{(\w+)\}", as_windows, a) for a in c.args]
    env = wine_env(args)
    truth = None
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
    env.update({k: re.sub(r"\{(\w+)\}", as_windows, v) for k, v in c.env.items()})
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
    code, out, err, killed = launch(args, argv, env, c.timeout, c.kill_after)
    elapsed = time.monotonic() - start
    status_text = ""
    if os.path.exists(paths["status"]):
        with open(paths["status"], encoding="utf-8", errors="replace") as f:
            status_text = f.read()
    result = Result(code, out, err, killed, status_text)
    result.truth = truth
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
