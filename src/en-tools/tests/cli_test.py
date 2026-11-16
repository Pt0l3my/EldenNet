#!/usr/bin/env python3
"""eldennet-engine's probe-exe and lint --report, run as DataPiler's job runs them, on the derivation tests' invented
program (written by en_tools_write_sample): the exit codes and messages of src/en-tools/README.md, derive.json's content,
and the lint's refusals. Nothing here is a game's.

    cli_test.py --engine ENGINE --writer WRITER --work DIR
"""
import argparse
import json
import os
import re
import subprocess
import sys

FAILURES = []


def need(cond, text):
    if not cond:
        FAILURES.append(text)


def run(engine, *argv):
    out = subprocess.run([engine] + list(argv), capture_output=True, text=True, check=False, timeout=120)
    return out.returncode, out.stdout, out.stderr


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--engine", required=True)
    p.add_argument("--writer", required=True)
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
    ]
    for argv, want in cases:
        code, so, se = run(a.engine, *argv)
        need(code == want, "%s: exit %s, wanted %s: %r" % (" ".join(x.replace(a.work, "<work>") for x in argv), code,
                                                          want, se.strip()[:200]))
        need(a.work not in so + se, "%s: a path in its messages" % argv[0])
    code, so, _ = run(a.engine, "probe-exe", "--help")
    need(code == 0 and "Safe use" in so and "What we condemn" in so, "probe-exe --help and its disclosure")

    for f in FAILURES:
        print("FAIL: %s" % f)
    print("PASS" if not FAILURES else "FAILED")
    return 0 if not FAILURES else 1


if __name__ == "__main__":
    sys.exit(main())
