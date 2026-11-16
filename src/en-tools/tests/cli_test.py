#!/usr/bin/env python3
"""eldennet-engine's probe-exe and lint --report, run as DataPiler's job runs them, on the derivation tests' invented
program (written by en_tools_write_sample): the exit codes and messages of src/en-tools/README.md, derive.json's content,
the lint's refusals, probe-exe --sections, a refused section's line, and a layout the loader accepts (raw data past the
end of the file, SizeOfImage short of the last section's alignment) deriving as the tidy one. Nothing here is a game's.

    cli_test.py --engine ENGINE --writer WRITER --work DIR
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

    for f in FAILURES:
        print("FAIL: %s" % f)
    print("PASS" if not FAILURES else "FAILED")
    return 0 if not FAILURES else 1


if __name__ == "__main__":
    sys.exit(main())
