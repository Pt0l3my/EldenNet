#!/usr/bin/env python3
"""eldennet-engine's probe-exe and lint --report, run as DataPiler's job runs them, on the derivation tests' invented
program (written by en_tools_write_sample): the exit codes and messages of src/en-tools/README.md, derive.json's content,
the lint's refusals, probe-exe --sections, a refused section's line, and a layout the loader accepts (raw data past the
end of the file, SizeOfImage short of the last section's alignment) deriving as the tidy one; and lint --gal, on the
repository's own gal/ and on invented folders. Nothing here is a game's (the repository's gal/ is read, never changed).

    cli_test.py --engine ENGINE --writer WRITER --work DIR [--gal DIR]
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys

FAILURES = []


def need(cond, text):
    if not cond:
        FAILURES.append(text)


def run(engine, *argv):
    out = subprocess.run([engine] + list(argv), capture_output=True, text=True, check=False, timeout=120)
    return out.returncode, out.stdout, out.stderr


# DataPiler's shape for a line it shows in a job's log (its devkit.py): the command's prefix, then at most 240 printable
# ASCII characters, with no path sign.
SHOWN = re.compile(r"(?:probe-exe|lint): [ -~]{1,240}")
HEX = r"0x[0-9a-f]+"
SECTIONS_HEAD = re.compile(r"probe-exe: image size %s file size %s section alignment %s" % (HEX, HEX, HEX))
SECTION_LINE = re.compile(r"probe-exe: section (\d+) va (%s) vsize (%s) raw (%s) rawsize (%s) flags (%s)" %
                          (HEX, HEX, HEX, HEX, HEX))


def table(data):
    """The section table, read plainly: (where the table starts, the optional header's offset, the sections as
    (va, vsize, raw, rawsize, flags))."""
    nt = struct.unpack_from("<I", data, 0x3C)[0]
    count, opt_size = struct.unpack_from("<H", data, nt + 6)[0], struct.unpack_from("<H", data, nt + 20)[0]
    opt = nt + 24
    at = opt + opt_size
    rows = []
    for i in range(count):
        vsize, va, rawsize, raw = struct.unpack_from("<IIII", data, at + 40 * i + 8)
        flags = struct.unpack_from("<I", data, at + 40 * i + 36)[0]
        rows.append((va, vsize, raw, rawsize, flags))
    return at, opt, rows


def shown_cleanly(line):
    return SHOWN.fullmatch(line) is not None and "/" not in line and "\\" not in line


def check_sections(engine, exe, data, failures_name):
    """probe-exe --sections on a file: its lines, numbers and fixed words only, agree with the table read plainly."""
    code, so, se = run(engine, "probe-exe", exe, "--sections")
    need(code == 0 and not se, "%s --sections: exit %s %r" % (failures_name, code, se))
    lines = so.splitlines()
    _, opt, rows = table(data)
    size_of_image, align = struct.unpack_from("<I", data, opt + 56)[0], struct.unpack_from("<I", data, opt + 32)[0]
    need(lines and lines[0] == "probe-exe: image size 0x%x file size 0x%x section alignment 0x%x" %
         (size_of_image, len(data), align), "%s --sections: the head line %r" % (failures_name, lines[:1]))
    need(len(lines) == 1 + len(rows), "%s --sections: %d lines for %d sections" % (failures_name, len(lines), len(rows)))
    for i, (line, row) in enumerate(zip(lines[1:], rows)):
        m = SECTION_LINE.fullmatch(line)
        need(m is not None and int(m.group(1)) == i and tuple(int(g, 16) for g in m.groups()[1:]) == row,
             "%s --sections: section %d's line %r" % (failures_name, i, line))
    need(all(SECTIONS_HEAD.fullmatch(l) or SECTION_LINE.fullmatch(l) for l in lines) and
         all(shown_cleanly(l) for l in lines), "%s --sections: a line of another shape" % failures_name)
    return rows


INVENTED_BUILDS = """schema = 1
[[build]]
key = "9.8.7.0-ww"
app = "4.5.6"
file_version = "9.8.7.0"
product_name = "INVENTED GAME"
region = "ww"
time_date_stamp = 0x12345678
size_of_image = 0x40000
layout = "layouts/9.8.7.0-ww.toml"
dlrf_name_layout = false
status = "supported"
"""
INVENTED_LAYOUT = """schema = 1
build = "9.8.7.0-ww"
derived_from = "probe"
probe_record = "docs/ops/probes/invented.md"
[task]
group = { name = "Invented_Group", index = 3 }
status = "lead"
"""
INVENTED_PATTERNS = """schema = 1
[[pattern]]
id = "invented_idiom"
kind = "idiom"
section = ".text"
bytes = "?? 8B ?? ?? ?? ?? ?? ?? 85 C0 74 ??"
path_suffix = "Invented.h"
captures = { static = { at = 3, kind = "rip_rel32", insn_end = 7, section = ".data", align = 8 } }
provenance = { method = "own-derivation", leads = ["none"], evidence = "docs/ops/probes/invented.md", date = "2026-11-17" }
"""


# Lodestar's catalogue and a build's table (proposal 0005 §4, §5), invented: two ids, and where they lie in the build.
INVENTED_IDS = """schema = 1
[[id]]
n = 1001
label = "lantern"
kind = "static"
means = "An invented slot."
needs = "world"
required = true
[[id]]
n = 2001
label = "ring_bell"
kind = "function"
means = "An invented function."
needs = "task_service"
required = false
"""
INVENTED_TABLE = """schema = 2
build = "9.8.7.0-ww"
derived_from = "probe"
probe_record = "docs/ops/probes/invented.md"
[provenance]
method = "own-derivation"
tool = "0.1.0-test.1"
tool_sha256 = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
round = "l3"
date = "2026-11-21"
[item.lantern]
id = 1001
kind = "static"
rva = 0x30000
found_by = ["accessor", "instance_class"]
witnesses = [0x1010, 0x1200]
checks = ["placement", "agree", "witness"]
status = "confirmed"
[item.ring_bell]
id = 2001
kind = "function"
rva = 0x2000
length = 93
found_by = ["relation"]
verdict = "removes"
checks = ["placement", "verdict"]
"""


# Lodestar's recipes (proposal 0005 §6; O102): the accessor as a shape, with no bytes, and its note.
INVENTED_RECIPES = """schema = 1
[[recipe]]
id = "invented_accessor"
kind = "accessor"
steps = ["load_slot", "test_loaded", "branch_set_short", "address_arg1", "call_relative"]
gives = { static = 0, class = 3, getter = 4 }
path_suffix = "Invented.h"
provenance = { method = "own-derivation", leads = ["none"], evidence = "docs/ops/probes/invented.md", date = "2026-11-21" }
"""


def write_gal(folder, builds=INVENTED_BUILDS, layout=INVENTED_LAYOUT, patterns=INVENTED_PATTERNS, note=True, ids=None,
              recipes=None, recipe_note=True):
    os.makedirs(os.path.join(folder, "layouts"), exist_ok=True)
    os.makedirs(os.path.join(folder, "derivations"), exist_ok=True)
    for name, text in (("builds.toml", builds), ("layouts/9.8.7.0-ww.toml", layout), ("patterns.toml", patterns),
                       ("ids.toml", ids), ("recipes.toml", recipes),
                       ("derivations/invented_accessor.md", "An invented note.\n" if recipes and recipe_note else None),
                       ("derivations/invented_idiom.md", "An invented note.\n" if note else None)):
        path = os.path.join(folder, name)
        if text is None:
            if os.path.exists(path):
                os.remove(path)
            continue
        with open(path, "w") as f:
            f.write(text)
    return folder


def check_gal(a):
    """lint --gal: the repository's gal/ clean; invented folders refused for each rule, each problem with its file and
    place, never a value; no builds.toml a usage error."""
    if a.gal:
        code, so, se = run(a.engine, "lint", "--gal", a.gal)
        need(code == 0 and "gal/ is clean" in so, "lint --gal on the repository's gal/: exit %s %r %r" % (code, so, se))
    base = os.path.join(a.work, "gal-cases")
    code, so, _ = run(a.engine, "lint", "--gal", write_gal(os.path.join(base, "clean")))
    need(code == 0 and "patterns 1 (confirmed in no build: 1)" in so and "ids 0" in so,
         "lint --gal, invented: exit %s %r" % (code, so))
    # Lodestar's catalogue and a build's table: clean together, and the table refused without or against its catalogue.
    code, so, _ = run(a.engine, "lint", "--gal",
                      write_gal(os.path.join(base, "table"), layout=INVENTED_TABLE, ids=INVENTED_IDS))
    need(code == 0 and "ids 2" in so, "lint --gal, an invented table: exit %s %r" % (code, so))
    # Lodestar's recipes: a shape with its note is clean, also in a folder with no patterns.toml at all.
    code, so, _ = run(a.engine, "lint", "--gal", write_gal(os.path.join(base, "recipes"), recipes=INVENTED_RECIPES))
    need(code == 0 and "recipes 1" in so, "lint --gal, an invented recipe: exit %s %r" % (code, so))
    code, so, _ = run(a.engine, "lint", "--gal",
                      write_gal(os.path.join(base, "recipes-only"), patterns=None, note=False, recipes=INVENTED_RECIPES))
    need(code == 0 and "patterns 0" in so and "recipes 1" in so,
         "lint --gal, recipes and no patterns.toml: exit %s %r" % (code, so))
    cases = [
        ("recipe-no-note", dict(recipes=INVENTED_RECIPES, recipe_note=False),
         "recipes.toml: recipe[0]: has no derivation note, derivations/invented_accessor.md"),
        ("recipe-bytes", dict(recipes=INVENTED_RECIPES.replace("path_suffix", 'bytes = "48 8B ?? ??"\npath_suffix')),
         'recipes.toml: recipe[0]: has an unknown key "bytes"'),
        ("recipe-unknown-form", dict(recipes=INVENTED_RECIPES.replace('"test_loaded"', '"test"')),
         "recipes.toml: recipe[0].steps[1]: must be one of"),
        ("recipe-unknown-build", dict(recipes=INVENTED_RECIPES.replace("path_suffix", 'builds = ["1.2.3.4-jp"]\npath_suffix')),
         "recipes.toml: recipe[0].builds: names a build builds.toml lacks"),
        ("table-no-catalogue", dict(layout=INVENTED_TABLE), "layouts/9.8.7.0-ww.toml: item: needs the catalogue"),
        ("table-other-number", dict(layout=INVENTED_TABLE, ids=INVENTED_IDS.replace("n = 1001", "n = 1002")),
         "layouts/9.8.7.0-ww.toml: item.lantern.id: is not the catalogue's number for this label"),
        ("table-outside-image", dict(layout=INVENTED_TABLE.replace("rva = 0x30000", "rva = 0x40000"), ids=INVENTED_IDS),
         "layouts/9.8.7.0-ww.toml: item.lantern.rva: lies outside the build's image"),
        ("table-bytes", dict(layout=INVENTED_TABLE.replace("length = 93", 'length = 93\nbytes = "48 8B"'),
                             ids=INVENTED_IDS),
         'layouts/9.8.7.0-ww.toml: item.ring_bell: has an unknown key "bytes"'),
        ("ids-renumbered", dict(ids=INVENTED_IDS.replace("n = 2001", "n = 1000")),
         "ids.toml: id[1].n: lies outside its kind's block"),
        ("no-note", dict(note=False), "patterns.toml: pattern[0]: has no derivation note, derivations/invented_idiom.md"),
        ("other-build", dict(layout=INVENTED_LAYOUT.replace('build = "9.8.7.0-ww"', 'build = "9.8.7.1-jp"')),
         "layouts/9.8.7.0-ww.toml: build: is not the build the gate matched"),
        ("unknown-confirmed", dict(patterns=INVENTED_PATTERNS.replace('path_suffix', 'builds = ["1.2.3.4-jp"]\npath_suffix')),
         "patterns.toml: pattern[0].builds: names a build builds.toml lacks"),
        ("bad-build", dict(builds=INVENTED_BUILDS.replace('region = "ww"', 'region = "xx"')), "builds.toml: build[0].region"),
        ("no-layout", dict(layout=None), "layouts/9.8.7.0-ww.toml: cannot be read"),
        ("half-task", dict(layout=INVENTED_LAYOUT.replace('status = "lead"', 'vtable_slots = 4')),
         "task.object_size: is missing"),
        ("guessed-provenance", dict(patterns=INVENTED_PATTERNS.replace('own-derivation', 'copied')),
         "patterns.toml: pattern[0].provenance.method"),
    ]
    for name, change, problem in cases:
        folder = write_gal(os.path.join(base, name), **change)
        code, so, _ = run(a.engine, "lint", "--gal", folder)
        need(code == 1 and problem in so and "gal/ is refused" in so and "Invented_Group" not in so and
             folder not in so, "lint --gal, %s: exit %s %r" % (name, code, so))
    empty = os.path.join(base, "empty")
    os.makedirs(empty, exist_ok=True)
    code, so, se = run(a.engine, "lint", "--gal", empty)
    need(code == 2 and "no builds.toml" in se and empty not in se, "lint --gal on a folder without builds.toml: %s %r" %
         (code, se))


def check_lodestar(a, exe, leads):
    """lodestar find, on the derivation tests' invented program: the report written, complete, clean under the lint, its
    numbers as the unit tests give them, and nothing in it but numbers, booleans, fixed words and labels; refusals for
    a folder without its files, an executable that is none, and a usage error, none of them repeating a path."""
    base = os.path.join(a.work, "lodestar")
    gal = os.path.join(base, "gal")
    os.makedirs(gal, exist_ok=True)
    recipes = INVENTED_RECIPES.replace("invented_accessor", "accessor").replace("Invented.h", "Lone.h")
    with open(leads) as f:
        leads_text = f.read()
    for name, text in (("leads.toml", leads_text), ("recipes.toml", recipes), ("builds.toml", INVENTED_BUILDS),
                       ("ids.toml", INVENTED_IDS)):
        with open(os.path.join(gal, name), "w") as f:
            f.write(text)
    out = os.path.join(base, "found.json")
    code, so, se = run(a.engine, "lodestar", "find", exe, "--gal", gal, "--out", out)
    need(code == 0 and "complete" in so and a.work not in so + se, "lodestar find: exit %s %r %r" % (code, so, se))
    with open(out) as f:
        text = f.read()
    d = json.loads(text)
    need(d["report"] == "lodestar" and d["schema"] == 5 and d["complete"] is True, "the lodestar report's head")
    acc = d["accessor"]
    need(acc["recipe"] and acc["hits"] == 8 and acc["accepted"] == 6 and acc["expect"] == "within",
         "the accessor's counts: %s" % {k: acc[k] for k in ("hits", "accepted", "expect")})
    need(acc["pattern"] == {"cut": True, "hits": acc["pattern"]["hits"], "accepted": 6, "both": 6, "only_shape": 0,
                            "only_pattern": 0, "statics_found": acc["statics_found"], "why": [],
                            "only_pattern_statics": []}, "the comparison with the pattern: %s" % acc["pattern"])
    need(len(acc["statics"]) == acc["statics_found"] and sum(s["sites"] for s in acc["statics"]) == 6 and
         all(1 <= len(s["witnesses"]) <= 8 for s in acc["statics"]), "the statics and their witnesses")
    # The invented builds.toml holds this version with another fingerprint: the gate's own word for it.
    need(d["image"]["build"] == "fingerprint_differs" and d["table"]["status"] == "derived" and
         all(c["status"] == "derived" and c["vtables"] for c in d["classes"]), "the image, the table and the classes")
    # What the image holds under each lead's name, in counts: one entry a lead (the sample's seven), under the lead's
    # own label where the invented catalogue does not hold it, each count a whole number, and never a name.
    lc = d["lead_classes"]
    need(len(lc) == 7 and all(set(c) == {"id", "label", "named", "named_imp", "vtable_count", "texts", "related"} and
                              all(isinstance(c[k], int) for k in c if k != "label") for c in lc),
         "the leads' classes: %s" % lc[:2])
    need("leads have a class of their name" in so, "the console's line names the leads' classes: %r" % so)
    # The invented folder holds the accessor's recipe alone: no variant is counted, and no second line is printed.
    need(acc["variants"] == [] and "other shapes" not in so, "no variants without their recipes: %r" % so)
    # Nothing but numbers, booleans, fixed words and labels: no string longer than a word, none with a byte's look.
    strings = re.findall(r'"([^"]*)"\s*[,\]}\n]', text)
    need(all(re.fullmatch(r"[a-z][a-z0-9_]{0,47}", v) for v in strings), "a string that is no word or label: %s" %
         [v for v in strings if not re.fullmatch(r"[a-z][a-z0-9_]{0,47}", v)][:3])
    need(not re.search(r"\b[0-9A-F]{2} [0-9A-F?]{2} ", text), "something with a pattern's look in the report")
    code, so, _ = run(a.engine, "lint", "--report", out)
    need(code == 0 and "clean" in so, "lint on the lodestar report: exit %s %r" % (code, so))
    # A count outside the recipe's range: written, to read first (1).
    with open(os.path.join(gal, "recipes.toml"), "w") as f:
        f.write(recipes.replace("path_suffix", "expect_matches = [1, 4]\npath_suffix"))
    code, so, _ = run(a.engine, "lodestar", "find", exe, "--gal", gal, "--out", out)
    need(code == 1 and "to read first" in so, "lodestar find, a count out of range: exit %s %r" % (code, so))
    with open(out) as f:
        need(json.load(f)["accessor"]["expect"] == "above", "the count's word")
    # Refusals: a folder without its recipes (4), an executable that is none (3), usage (2); never a path repeated.
    os.remove(os.path.join(gal, "recipes.toml"))
    code, _, se = run(a.engine, "lodestar", "find", exe, "--gal", gal, "--out", out)
    need(code == 4 and "recipes.toml cannot be read" in se and a.work not in se, "no recipes.toml: %s %r" % (code, se))
    with open(os.path.join(gal, "recipes.toml"), "w") as f:
        f.write(recipes.replace("own-derivation", "copied"))
    code, _, se = run(a.engine, "lodestar", "find", exe, "--gal", gal, "--out", out)
    need(code == 4 and "recipes.toml: recipe[0].provenance.method" in se, "a refused recipes.toml: %s %r" % (code, se))
    with open(os.path.join(gal, "recipes.toml"), "w") as f:
        f.write(recipes)
    code, _, se = run(a.engine, "lodestar", "find", os.path.join(gal, "leads.toml"), "--gal", gal, "--out", out)
    need(code == 3 and "not a PE32+ image" in se and a.work not in se, "an executable that is none: %s %r" % (code, se))
    code, _, se = run(a.engine, "lodestar", "find", exe, "--gal", gal)
    need(code == 2 and a.work not in se, "lodestar find without --out: %s %r" % (code, se))
    code, _, se = run(a.engine, "lodestar", "check", exe)
    need(code == 2 and "find is the command built so far" in se, "a command not built yet: %s %r" % (code, se))
    code, so, _ = run(a.engine, "lodestar", "find", "--help")
    need(code == 0 and "Safe use" in so and "never a running game" in so, "lodestar find --help")


def spike_log(path, spike, runs):
    """A launcher's spike log (en-launcher's record.cpp: a line per run, then the tally), runs as (mode, outcome, end,
    record_kept, connections_max). Invented numbers."""
    with open(path, "w") as f:
        for i, (mode, outcome, end, kept, conns) in enumerate(runs, 1):
            f.write(json.dumps({"spike": spike, "run": i, "mode": mode, "outcome": outcome,
                                "status": {"init": "ready", "phase": 4, "level": 3, "banner": 0, "flags": 10,
                                           "items_resolved": 16, "items_total": 16, "discovery_passes": 1 + i % 2,
                                           "ready_ms": 9000 + i * 10, "task_us_p99": 300, "snapshots": 4,
                                           "faults": 0, "tripped": 0},
                                "end": end, "exit_code": 0, "record_kept": kept, "crash_filter": kept,
                                "run_ms": 30000, "connections_max": conns}) + "\n")
        f.write(json.dumps({"spike": spike, "tally": 1, "runs": len(runs), "passed": True}) + "\n")
    return path


def report_of(a, argv, path):
    code, so, se = run(a.engine, *(argv + ["--out", path]))
    data = None
    if os.path.exists(path):
        with open(path) as f:
            data = json.load(f)
        lcode, lso, _ = run(a.engine, "lint", "--report", path)
        need(lcode == 0 and "clean" in lso, "lint --report on %s: %s %r" % (os.path.basename(path), lcode, lso))
    return code, so, se, data


def rules_of(data):
    return {(r["rule"], r.get("mode")): r for r in (data or {}).get("rules", [])}


def check_spike_tools(a):
    """rec-stats, replay and spike-report (the plan's part 9) on recordings written as eldennet.dll writes them and on
    invented spike logs: their numbers, the replay's agreement, each spike's rules and verdict, and a tampered
    recording refused or disagreeing. Every report lints clean and holds no path."""
    work = os.path.join(a.work, "spike")
    os.makedirs(work, exist_ok=True)
    subprocess.run([a.recordings, work], check=True, timeout=120)
    rec = {n: os.path.join(work, "%s.ensnap.jsonl" % n) for n in ("s0b", "s0b-slow", "s0c", "s0d")}
    # rec-stats: the co-op session's numbers.
    code, _, _, st = report_of(a, ["rec-stats", rec["s0c"]], os.path.join(work, "s0c-stats.json"))
    need(code == 0 and st and st["snapshots"]["count"] == 74 and st["gate"]["opened"] == 2 and
         st["gate"]["closed"] == 1 and st["marks"] == 1 and st["life"]["playthrough_ready"] == 1 and
         st["header"]["spike"] == "s0c" and st["faults"] == 0, "rec-stats on s0c: %s %s" % (code, st))
    # replay: every recording agrees; one with a change line taken out does not, and says where.
    for n, path in rec.items():
        code, _, se, rp = report_of(a, ["replay", path], os.path.join(work, "%s-replay.json" % n))
        need(code == 0 and rp and rp["agree"] and rp["recorded"] == rp["replayed"] > 0 and "agrees" in se,
             "replay on %s: %s %r %s" % (n, code, se, rp and {k: rp[k] for k in ("recorded", "replayed", "agree")}))
    with open(rec["s0c"]) as f:
        lines = f.read().splitlines(True)
    closing = [i for i, l in enumerate(lines) if '"what":"gate_closed"' in l]
    tampered = os.path.join(work, "s0c-tampered.ensnap.jsonl")
    with open(tampered, "w") as f:
        f.writelines(l for i, l in enumerate(lines) if i != closing[0])
    code, _, se, rp = report_of(a, ["replay", tampered], os.path.join(work, "tampered-replay.json"))
    need(code == 1 and rp and not rp["agree"] and "first_difference" in rp and "disagrees" in se,
         "replay on a tampered recording: %s %r" % (code, se))
    check_golden_tools(a, work, rec["s0c"])
    # spike-report s0a: 20 runs early and 20 late pass; 19 early fail; early alone is incomplete; a crash fails.
    ok = [("suspended", "ready", "closed", False, 0)] * 20 + [("title", "banner", "closed", False, 0)] * 20
    code, so, _, sp = report_of(a, ["spike-report", "s0a", "--log", spike_log(os.path.join(work, "s0a.jsonl"), "s0a",
                                                                               ok)], os.path.join(work, "s0a.json"))
    r = rules_of(sp)
    need(code == 0 and sp["verdict"] == "pass" and r[("runs_per_mode", "suspended")]["value"] == 20 and
         r[("both_modes", None)]["status"] == "pass" and len(sp["modes"]) == 2 and
         "verdict=pass" in so, "spike-report s0a: %s %r" % (code, so))
    for name, runs, verdict in (("short", ok[1:], "fail"), ("early", ok[:20], "incomplete"),
                                ("crash", ok[:39] + [("title", "banner", "crash", True, 0)], "fail")):
        code, so, _, sp = report_of(a, ["spike-report", "s0a", "--log",
                                        spike_log(os.path.join(work, "s0a-%s.jsonl" % name), "s0a", runs)],
                                    os.path.join(work, "s0a-%s.json" % name))
        need(code == 1 and sp and sp["verdict"] == verdict, "spike-report s0a %s: %s %s" % (name, code, sp and sp["verdict"]))
    # s0b: the two hours pass with the session's log; the slow one fails on its p99 and its fault.
    log_b = spike_log(os.path.join(work, "s0b.jsonl"), "s0b", [("suspended", "ready", "clean", False, 0)])
    code, so, _, sp = report_of(a, ["spike-report", "s0b", "--recording", rec["s0b"], "--log", log_b],
                                os.path.join(work, "s0b.json"))
    need(code == 0 and sp and sp["verdict"] == "pass" and rules_of(sp)[("task_p99_us", None)]["value"] == 310,
         "spike-report s0b: %s %r" % (code, so))
    code, _, _, sp = report_of(a, ["spike-report", "s0b", "--recording", rec["s0b-slow"], "--log", log_b],
                               os.path.join(work, "s0b-slow.json"))
    r = rules_of(sp)
    need(code == 1 and sp["verdict"] == "fail" and r[("task_p99_us", None)]["status"] == "fail" and
         r[("faults", None)]["value"] == 1, "spike-report s0b, slow: %s %s" % (code, sp))
    # s0c: the gate closed for the co-op visit; no connection; the online menu is the owner's to see. Without the
    # closing change line, the co-op snapshots count against the gate; a connection fails too.
    log_c = spike_log(os.path.join(work, "s0c.jsonl"), "s0c", [("suspended", "ready", "clean", False, 0)])
    code, _, _, sp = report_of(a, ["spike-report", "s0c", "--recording", rec["s0c"], "--log", log_c],
                               os.path.join(work, "s0c.json"))
    r = rules_of(sp)
    need(code == 0 and sp["verdict"] == "pass" and r[("online_menu", None)]["status"] == "not_measured" and
         sp["gate"]["opened"] == 2, "spike-report s0c: %s %s" % (code, sp))
    code, _, _, sp = report_of(a, ["spike-report", "s0c", "--recording", tampered, "--log", log_c],
                               os.path.join(work, "s0c-tampered.json"))
    need(code == 1 and sp["verdict"] == "fail" and rules_of(sp)[("gate_closed_unless_solo", None)]["value"] == 10,
         "spike-report s0c, tampered: %s %s" % (code, sp and rules_of(sp)))
    log_c2 = spike_log(os.path.join(work, "s0c-online.jsonl"), "s0c", [("suspended", "ready", "clean", False, 2)])
    code, _, _, sp = report_of(a, ["spike-report", "s0c", "--recording", rec["s0c"], "--log", log_c2],
                               os.path.join(work, "s0c-online.json"))
    need(code == 1 and sp["verdict"] == "fail" and sp["connections_max"] == 2, "spike-report s0c, online: %s" % code)
    # s0d: 25 faults caught, camera's breaker tripped, the session clean, and five forced crashes all recorded.
    log_d = spike_log(os.path.join(work, "s0d.jsonl"), "s0d", [("suspended", "ready", "clean", False, 0)])
    log_dc = spike_log(os.path.join(work, "s0d-crash.jsonl"), "s0d-crash",
                       [("suspended", "ready", "crash", True, 0)] * 5)
    code, _, _, sp = report_of(a, ["spike-report", "s0d", "--recording", rec["s0d"], "--log", log_d, "--log", log_dc],
                               os.path.join(work, "s0d.json"))
    r = rules_of(sp)
    need(code == 0 and sp["verdict"] == "pass" and r[("faults_caught", None)]["value"] == 25 and
         r[("crashes_recorded", None)]["value"] == 5 and sp["records"] == 5, "spike-report s0d: %s %s" % (code, sp))
    log_dc2 = spike_log(os.path.join(work, "s0d-crash-lost.jsonl"), "s0d-crash",
                        [("suspended", "ready", "crash", True, 0)] * 4 + [("suspended", "ready", "crash", False, 0)])
    code, _, _, sp = report_of(a, ["spike-report", "s0d", "--recording", rec["s0d"], "--log", log_d, "--log",
                                   log_dc2], os.path.join(work, "s0d-lost.json"))
    need(code == 1 and sp["verdict"] == "fail", "spike-report s0d, a crash not recorded: %s" % code)
    # Refusals: a recording with a path in it, a log that is not the launcher's, usage; no path in any message.
    bad = os.path.join(work, "bad.ensnap.jsonl")
    with open(bad, "w") as f:
        f.write(lines[0].replace('"region":"ww"', '"region":"C:\\\\Games"'))
    junk = os.path.join(work, "junk.jsonl")
    with open(junk, "w") as f:
        f.write("not a launcher line\n")
    for argv, want in ((["rec-stats", bad], 2), (["replay", bad], 2), (["spike-report", "s0b", "--recording", bad], 2),
                       (["spike-report", "s0a", "--log", junk], 2), (["spike-report", "s9"], 2),
                       (["rec-stats"], 2), (["spike-report", "s0a", "--need", "0"], 2)):
        code, so, se = run(a.engine, *argv)
        need(code == want and work not in so + se and "Games" not in so + se,
             "%s: exit %s, wanted %s: %r" % (argv[0], code, want, se[-200:]))


def check_golden_tools(a, work, recording):
    """replay --events and golden (part 11) on a recording written as eldennet.dll writes it, and on an invented
    scenario: the events in the golden files' form, update then check, a golden file changed by hand or without its
    CHANGES.md entry refused with our own diff, a scenario's failed expectation, and usage errors. No path in any
    message."""
    events = os.path.join(work, "s0c-events.jsonl")
    code, _, se, rp = report_of(a, ["replay", recording, "--events", events], os.path.join(work, "s0c-replay2.json"))
    need(code == 0 and rp and rp["events"] > 0 and os.path.exists(events), "replay --events: %s %r" % (code, se))
    if os.path.exists(events):
        with open(events) as f:
            lines = f.read().splitlines()
        head = json.loads(lines[0])
        kinds = {k["event"]: k["count"] for k in rp["event_kinds"]}
        need(head["golden"] == 1 and head["events"] == rp["events"] == len(lines) - 1 and
             kinds.get("GateChanged", 0) == 3 and kinds.get("DataReady") == 1,
             "replay's events: %r %r" % (head, kinds))
    gold = os.path.join(work, "golden")
    os.makedirs(gold, exist_ok=True)
    target, changes = os.path.join(gold, "s0c.events.jsonl"), os.path.join(gold, "CHANGES.md")
    for old in (target, os.path.join(gold, "unmet.events.jsonl")):
        if os.path.exists(old):
            os.remove(old)  # an earlier run's
    with open(changes, "w") as f:
        f.write("# changes\n")
    code, so, se = run(a.engine, "golden", "update", recording, "--golden", target)
    hash_ = re.search(r"fnv1a64:[0-9a-f]{16}", so)
    need(code == 0 and hash_ and "a new golden file" in so, "golden update: %s %r %r" % (code, so, se))
    code, so, se = run(a.engine, "golden", "check", recording, "--golden", target)
    need(code == 1 and "CHANGES.md has no entry" in se, "golden check without its entry: %s %r" % (code, se))
    if hash_:
        with open(changes, "a") as f:
            f.write("- `s0c` %s: invented\n" % hash_.group(0))
    code, so, se = run(a.engine, "golden", recording, "--golden", target)
    need(code == 0 and "agree" in so, "golden check: %s %r %r" % (code, so, se))
    with open(target) as f:
        text = f.read()
    with open(target, "w") as f:
        f.write(text.replace('"kind":"GateChanged"', '"kind":"GateOpened"', 1))
    code, so, se = run(a.engine, "golden", "check", recording, "--golden", target)
    need(code == 1 and so.startswith("@@ golden line") and "-" in so and "differ" in se,
         "golden check on a changed file: %s %r" % (code, so[:200]))
    scn = os.path.join(work, "unmet.scn")
    with open(scn, "w") as f:
        f.write("route unmet\nat 0 spawn s1 team=6 pos=40,0,0\nexpect event Engaged subject=s1\n")
    code, so, se = run(a.engine, "golden", "update", scn, "--golden", os.path.join(gold, "unmet.events.jsonl"))
    need(code == 1 and "line 3: no Engaged matched" in se and not os.path.exists(os.path.join(gold, "unmet.events.jsonl")),
         "golden update with an unmet expectation: %s %r" % (code, se))
    for argv in (["golden"], ["golden", "x.txt"], ["golden", "check", recording, "--frob"]):
        code, so, se = run(a.engine, *argv)
        need(code == 2 and work not in so + se, "%s: exit %s %r" % (argv, code, se[-200:]))


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--engine", required=True)
    p.add_argument("--writer", required=True)
    p.add_argument("--gal", help="the repository's gal/ folder, which lint --gal must pass")
    p.add_argument("--recordings", help="en_tools_write_recording, for rec-stats, replay and spike-report")
    p.add_argument("--work", required=True)
    a = p.parse_args()
    os.makedirs(a.work, exist_ok=True)
    subprocess.run([a.writer, a.work], check=True)
    exe, leads, clean = (os.path.join(a.work, n) for n in ("sample.exe", "leads.toml", "leads-clean.toml"))
    with open(os.path.join(a.work, "slots.txt")) as f:
        slots = [l.strip() for l in f if l.strip()]
    out = os.path.join(a.work, "derive.json")

    # The leads with decoys: written, incomplete (1).
    code, so, se = run(a.engine, "probe-exe", exe, "--derive", "--leads", leads, "--out", out)
    need(code == 1, "probe-exe with decoys: exit %s, wanted 1: %s" % (code, se))
    need("incomplete" in so and a.work not in so + se, "probe-exe's summary: %r %r" % (so, se))
    with open(out) as f:
        d = json.load(f)
    need(d["report"] == "derive" and d["schema"] == 1 and d["complete"] is False, "derive.json's head")
    single = {s["label"]: s["status"] for s in d["fd4"]["singletons"]}
    need(single == {"absent": "missing", "clock": "derived", "dup": "ambiguous", "hub": "derived", "rock": "refused",
                    "roster": "derived", "stray": "refused"}, "singletons %s" % single)
    need(d["register"]["status"] == "derived" and d["group"]["candidates"][0]["index"] == 2, "register and group")
    code, so, _ = run(a.engine, "lint", "--report", out)
    need(code == 0 and "clean" in so, "lint on derive.json: exit %s %r" % (code, so))

    # The clean leads with R1's slot: complete (0); the static cut at a reference.
    argv = ["probe-exe", exe, "--derive", "--leads", clean, "--out", out]
    for s in slots:
        argv += ["--static", s]
    code, so, se = run(a.engine, *argv)
    need(code == 0 and "complete" in so, "probe-exe complete: exit %s %r %r" % (code, so, se))
    with open(out) as f:
        d = json.load(f)
    st = d["statics"][0]
    need(st["status"] == "derived" and st["pattern"]["matches"] == 1 and st["pattern"]["scan_resolved"],
         "the static's pattern %s" % st)

    # R0c's variants: every accessor accepted whatever its path's load, the decoy refused as no idiom, a table of
    # 16-byte entries, a product name that matches without its trademark sign, and the diagnostics within schema 1.
    vexe, vleads = (os.path.join(a.work, n) for n in ("variants.exe", "leads-variants.toml"))
    vout = os.path.join(a.work, "derive-variants.json")
    code, so, se = run(a.engine, "probe-exe", vexe, "--derive", "--leads", vleads, "--out", vout)
    need(code == 1, "probe-exe on the variants: exit %s, wanted 1 (no registration): %s" % (code, se))
    with open(vout) as f:
        v = json.load(f)
    need(v["image"]["product_name"] == "matches" and v["image"]["names"] == {"of": 13, "agree": 12},
         "the variants' product name %s %s" % (v["image"]["product_name"], v["image"].get("names")))
    vsingle = {s["label"]: s["status"] for s in v["fd4"]["singletons"]}
    need(set(vsingle.values()) == {"derived"} and len(vsingle) == 6, "the variants' singletons %s" % vsingle)
    need(v["fd4"]["refused"]["unreadable"] == 1 and v["fd4"]["idiom"]["scan_resolved"] is True,
         "the variants' idiom %s" % v["fd4"]["refused"])
    g = v["group"]["candidates"][0]
    need((g["length"], g["entries"], g["index"], g["offset"], g["value"], g["encoding"]) == (16, 5, 2, 8, 12, "utf16"),
         "the variants' group table %s" % g)
    sf = v["stats"]["fd4"]
    need(len(sf["idiom"]["captures"]) == 6 and sf["accepted"] == 2 and sf["refused"]["no_path"] == 2,
         "the variants' diagnostics %s" % sf)
    need(v["stats"]["sections"][0]["section"] == ".text" and v["stats"]["sections"][0]["path_references"] == 4,
         "the variants' census %s" % v["stats"]["sections"])
    need({s["label"]: s["matches"] for s in v["statics"]} == {"moved": 1, "plain": 0}, "the variants' statics")
    code, so, _ = run(a.engine, "lint", "--report", vout)
    need(code == 0 and "clean" in so, "lint on the variants' derive.json: exit %s %r" % (code, so))

    # The lint refuses what is outside the vocabulary, and quotes none of it.
    with open(out) as f:
        text = f.read()
    for kind, bad in {
        "path": text.replace('"label": "ledger"', '"label": "C:\\\\Games\\\\x.exe"', 1),
        "free-string": text.replace('"region": "ww"', '"region": "a place"', 1),
        "byte-run": re.sub(r'"bytes": "[^"]*"', '"bytes": "%s"' % " ".join(["CC"] * 65), text, count=1),
        "float": text.replace('"fixups": ', '"fixups": 0.5, "x": ', 1),
    }.items():
        path = os.path.join(a.work, "bad-%s.json" % kind)
        with open(path, "w") as f:
            f.write(bad)
        code, so, _ = run(a.engine, "lint", "--report", path)
        need(code == 1 and "refused" in so, "lint on %s: exit %s %r" % (kind, code, so))
        need("Games" not in so and "a place" not in so and "CC CC" not in so, "lint on %s quoted it: %r" % (kind, so))

    # lint --recording: every line a report of the kind "recording" (eldennet.dll's recorder, en_shell's record.hpp),
    # floats where its keys take them; a path on one line is refused with that line's number, and not quoted.
    rec_lines = [
        '{"report":"recording","schema":1,"t":1,"k":"header","ensnap":1,"region":"ww","file":1}',
        '{"report":"recording","schema":1,"t":2,"k":"snap","seq":1,"chars":[{"handle":7,"dist_m":3.25,'
        '"pos":[1.5,-2,3]}]}',
        '{"report":"recording","schema":1,"t":3,"k":"mark","n":1}',
    ]
    rec = os.path.join(a.work, "sample.ensnap.jsonl")
    with open(rec, "w") as f:
        f.write("\n".join(rec_lines) + "\n")
    code, so, _ = run(a.engine, "lint", "--recording", rec)
    need(code == 0 and "the recording is clean" in so, "lint --recording: exit %s %r" % (code, so))
    code, so, _ = run(a.engine, "lint", "--report", rec)
    need(code == 1, "lint --report on a recording: exit %s %r" % (code, so))
    bad_rec = os.path.join(a.work, "bad.ensnap.jsonl")
    with open(bad_rec, "w") as f:
        f.write("\n".join([rec_lines[0], rec_lines[1].replace('"seq":1', '"seq":"C:\\\\Games\\\\x.exe"')]) + "\n")
    code, so, _ = run(a.engine, "lint", "--recording", bad_rec)
    need(code == 1 and "line 2: seq" in so and "Games" not in so, "lint --recording on a path: exit %s %r" % (code, so))

    # Usage and input errors: their exit codes, and no path in their messages.
    cases = [
        (["probe-exe"], 2),
        (["probe-exe", exe, "--leads", leads, "--out", out], 2),  # no --derive
        (["probe-exe", exe, "--derive", "--leads", leads], 2),    # no --out
        (["probe-exe", exe, "--derive", "--leads", leads, "--out", out, "--static", "nobody=0x10"], 2),
        (["probe-exe", exe, "--derive", "--leads", leads, "--out", out, "--static", "ledger=zz"], 2),
        (["probe-exe", exe, "--derive", "--leads", os.path.join(a.work, "absent.toml"), "--out", out], 4),
        (["probe-exe", exe, "--derive", "--leads", exe, "--out", out], 4),  # leads that are not TOML
        (["probe-exe", leads, "--derive", "--leads", leads, "--out", out], 3),  # an executable that is not one
        (["probe-exe", os.path.join(a.work, "absent.exe"), "--derive", "--leads", leads, "--out", out], 3),
        (["probe-exe", exe, "--derive", "--leads", leads, "--out", os.path.join(a.work, "no", "such", "dir.json")], 6),
        (["lint"], 2),
        (["lint", "--report", os.path.join(a.work, "absent.json")], 2),
        (["lint", "--report", out, "--report", out], 2),
        (["lint", "--recording", rec, "--report", out], 2),
    ]
    for argv, want in cases:
        code, so, se = run(a.engine, *argv)
        need(code == want, "%s: exit %s, wanted %s: %r" % (" ".join(x.replace(a.work, "<work>") for x in argv), code,
                                                          want, se.strip()[:200]))
        need(a.work not in so + se, "%s: a path in its messages" % argv[0])
    code, so, _ = run(a.engine, "probe-exe", "--help")
    need(code == 0 and "Safe use" in so and "What we condemn" in so, "probe-exe --help and its disclosure")
    need("--sections" in so, "probe-exe --help names --sections")

    # --sections: the table's numbers, and nothing else; refused with the other options, and on a file that is no PE.
    with open(exe, "rb") as f:
        data = f.read()
    rows = check_sections(a.engine, exe, data, "the sample")
    for argv, want in [(["probe-exe", exe, "--sections", "--derive", "--leads", leads, "--out", out], 2),
                       (["probe-exe", "--sections"], 2),
                       (["probe-exe", leads, "--sections"], 3)]:
        code, so, se = run(a.engine, *argv)
        need(code == want and a.work not in so + se, "%s: exit %s, wanted %s" % (argv[1:], code, want))

    # A section the loader refuses (the second over the first): exit 3, one line naming the rule, the section by index
    # and its numbers, of the shape DataPiler's log shows; --sections still prints the table.
    at, opt, _ = table(data)
    bad = bytearray(data)
    struct.pack_into("<I", bad, at + 40 + 12, rows[0][0])
    bad_exe = os.path.join(a.work, "overlap.exe")
    with open(bad_exe, "wb") as f:
        f.write(bad)
    code, so, se = run(a.engine, "probe-exe", bad_exe, "--derive", "--leads", leads, "--out", out + ".bad")
    errs = se.splitlines()
    need(code == 3 and len(errs) == 1 and not os.path.exists(out + ".bad"), "the overlap: exit %s %r" % (code, se))
    m = re.fullmatch(r"probe-exe: the executable is not a PE32\+ image for x64: a section is misplaced: section 1 "
                     r"starts below the end of section 0, (%s); va (%s) vsize (%s) raw (%s) rawsize (%s) flags (%s) "
                     r"\(at (%s)\)" % (HEX, HEX, HEX, HEX, HEX, HEX, HEX), errs[0] if errs else "")
    need(m is not None and tuple(int(g, 16) for g in m.groups()[1:6]) == (rows[0][0],) + rows[1][1:] and
         int(m.group(7), 16) == at + 40, "the overlap's line: %r" % errs)
    need(bool(errs) and shown_cleanly(errs[0]), "the overlap's line is not one DataPiler's log shows: %r" % errs)
    check_sections(a.engine, bad_exe, bytes(bad), "the overlap")

    # A layout the loader accepts and the reader once refused: the last section's raw data running past the end of the
    # file, and SizeOfImage short of its alignment. It derives as the tidy one, but for those two numbers.
    loose = bytearray(data)
    last = len(rows) - 1
    struct.pack_into("<I", loose, at + 40 * last + 16, rows[last][3] + 0x10000)
    size_of_image = struct.unpack_from("<I", loose, opt + 56)[0]
    struct.pack_into("<I", loose, opt + 56, size_of_image - 0x10)
    loose_exe = os.path.join(a.work, "loose.exe")
    with open(loose_exe, "wb") as f:
        f.write(loose)
    tidy_out, loose_out = out + ".tidy", out + ".loose"
    tcode, _, tse = run(a.engine, "probe-exe", exe, "--derive", "--leads", leads, "--out", tidy_out)
    lcode, lso, lse = run(a.engine, "probe-exe", loose_exe, "--derive", "--leads", leads, "--out", loose_out)
    need(tcode == 1 and lcode == 1, "the loose layout: exit %s, the tidy %s: %r %r" % (lcode, tcode, lse, tse))
    if os.path.exists(tidy_out) and os.path.exists(loose_out):
        with open(tidy_out) as f:
            tidy = json.load(f)
        with open(loose_out) as f:
            loosed = json.load(f)
        need(loosed["image"]["size_of_image"] == size_of_image - 0x10 and
             loosed["image"]["sections"][last]["raw_size"] == rows[last][3] + 0x10000, "the loose layout's image")
        for d in (tidy, loosed):
            d["image"].pop("size_of_image")
            d["image"]["sections"][last].pop("raw_size")
        need(tidy == loosed, "the loose layout derives other than the tidy one")
    code, so, _ = run(a.engine, "lint", "--report", loose_out)
    need(code == 0, "lint on the loose layout's report: exit %s %r" % (code, so))

    check_gal(a)
    check_lodestar(a, exe, leads)
    if a.recordings:
        check_spike_tools(a)

    for f in FAILURES:
        print("FAIL: %s" % f)
    print("PASS" if not FAILURES else "FAILED")
    return 0 if not FAILURES else 1


if __name__ == "__main__":
    sys.exit(main())
