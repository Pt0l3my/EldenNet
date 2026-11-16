#!/usr/bin/env python3
"""Compares en-gal's x86-64 decoder with LLVM's disassembler (ARCH §6.2's derivation kit; O89).

    x86_objdump.py objects --checker CHECK --objdump OBJDUMP --work DIR FILE...
    x86_objdump.py random  --checker CHECK --objdump OBJDUMP --assembler CLANG --work DIR [--count N] [--seed S]

`objects` disassembles our own binaries and objects (ELF or PE) with llvm-objdump; `random` assembles random byte
strings, weighted towards prefixes, escapes, VEX, EVEX and XOP, each after a symbol of its own, and compares the first
instruction llvm-objdump decodes at each. Either way it writes one listing line
per instruction (x86_check.cpp says the format) and runs en_gal_x86_check over it, which exits 1 on any disagreement:
each is a finding. Run with python3 -I; it reads only the files it is given.
"""
import argparse
import os
import random
import re
import subprocess
import sys

LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ?)+?)\s*(?:\t(.*))?$")
IMM = re.compile(r"\$(-?)(0x[0-9a-f]+|[0-9]+)")
TARGET = re.compile(r"^\s*0x([0-9a-f]+)")
DIRECT = re.compile(r"^(0x[0-9a-f]+)")
BRANCHES = re.compile(r"^(j[a-z]*|call[a-z]*|loop[a-z]*|xbegin[a-z]*)$")
PSEUDO = re.compile(r"^(v?cmp[a-z_]+(ps|pd|ss|sd|ph|sh)|vpcmp[a-z]+|vpcom[a-z]+|v?pclmul[a-z]+)$")


def twos(value):
    return value & 0xFFFFFFFFFFFFFFFF


def listing_line(addr, buf, length, text):
    """One line of the listing from an instruction LLVM printed as `text`."""
    if "<unknown>" in text or "(bad)" in text:
        return "%x %s %d - - - unknown" % (addr, buf.hex(), length)
    text = text.replace("\t", " ").strip()
    code, _, comment = text.partition("#")
    parts = code.split(None, 1)
    mnemonic = parts[0] if parts else ""
    operands = parts[1].strip() if len(parts) > 1 else ""
    # Prefixes LLVM prints as words before the mnemonic.
    while (mnemonic in PREFIX_WORDS or mnemonic in ("{evex}", "{vex}", "{vex2}", "{vex3}")
           or re.match(r"^rex(64|\.[wrxb]+)?$", mnemonic)) and operands:
        parts = operands.split(None, 1)
        mnemonic = parts[0]
        operands = parts[1].strip() if len(parts) > 1 else ""
    rip = "-"
    if "(%rip)" in operands or "(%eip)" in operands:
        m = TARGET.search(comment)
        rip = m.group(1) if m else "r"  # LLVM gives no target when its comment says something else, or past zero
    branch = "-"
    if BRANCHES.match(mnemonic):
        m = DIRECT.match(operands)
        if m:
            branch = "%x" % int(m.group(1), 16)
    imms = []
    for sign, value in IMM.findall(operands):
        v = int(value, 16) if value.startswith("0x") else int(value)
        imms.append("%x" % twos(-v if sign else v))
    kind = "ok"
    if not imms and branch == "-" and PSEUDO.search(mnemonic):
        kind = "pseudo"
    return "%x %s %d %s %s %s %s" % (addr, buf.hex(), length, rip, branch, ",".join(imms) or "-", kind)


def parse_objdump(text):
    """(address, bytes, text) for each line of llvm-objdump's listing, with the symbol each follows."""
    insns = []
    symbol = None
    for raw in text.splitlines():
        sym = re.match(r"^[0-9a-f]+ <(.*)>:$", raw)
        if sym:
            symbol = sym.group(1)
            continue
        m = LINE.match(raw)
        if not m:
            continue
        addr = int(m.group(1), 16)
        data = bytes(int(b, 16) for b in m.group(2).split())
        insns.append((addr, data, m.group(3) or "", symbol))
    return insns


def merge_prefixes(insns):
    """LLVM prints a prefix it cannot attach (lock before xadd, a lone data16) on a line of its own: it belongs to the
    instruction that follows, which our decoder reads whole. A prefix with no instruction after it becomes unknown."""
    merged = []
    pending = None
    for addr, data, text, symbol in insns:
        words = text.split()
        prefix = bool(words) and all(w in PREFIX_WORDS or re.match(r"^rex(64|\.[wrxb]+)?$", w) for w in words)
        if pending is not None and (pending[0] + len(pending[1]) != addr or pending[3] != symbol):
            merged.append((pending[0], pending[1], "<unknown>", pending[3]))
            pending = None
        if prefix:
            pending = (pending[0], pending[1] + data, "", symbol) if pending is not None else (addr, data, "", symbol)
            continue
        if pending is not None:
            addr, data = pending[0], pending[1] + data
            pending = None
        merged.append((addr, data, text, symbol))
    if pending is not None:
        merged.append((pending[0], pending[1], "<unknown>", pending[3]))
    return merged


PREFIX_WORDS = {"lock", "rep", "repe", "repne", "repz", "repnz", "data16", "data32", "addr32", "cs", "ds", "es", "ss",
                "fs", "gs", "notrack", "bnd", "xacquire", "xrelease"}  # words LLVM prints for prefixes


def objdump_listing(objdump, path):
    out = subprocess.run([objdump, "-d", path], capture_output=True, text=True, check=False)
    if out.returncode != 0:
        raise SystemExit("llvm-objdump failed on %s: %s" % (os.path.basename(path), out.stderr[-500:]))
    insns = merge_prefixes(parse_objdump(out.stdout))
    lines = []
    for i, (addr, data, text, symbol) in enumerate(insns):
        # The bytes that follow, within the same symbol only: llvm-objdump decodes each symbol on its own, so an
        # instruction never runs into the next (a sanitizer's data before a function ends with a byte of its own).
        buf = bytearray(data)
        nxt = addr + len(data)
        j = i + 1
        while len(buf) < 16 and j < len(insns) and insns[j][0] == nxt and insns[j][3] == symbol:
            buf += insns[j][1]
            nxt += len(insns[j][1])
            j += 1
        lines.append(listing_line(addr, bytes(buf[:max(16, len(data))]), len(data), text))
    return lines


# Bytes that make the random strings interesting: prefixes, escapes, VEX, EVEX and XOP, and common opcodes.
PREFIXES = [0x66, 0x67, 0xF0, 0xF2, 0xF3, 0x2E, 0x3E, 0x26, 0x36, 0x64, 0x65] + list(range(0x40, 0x50))


def random_buffer(rng):
    buf = bytearray()
    for _ in range(rng.choice([0, 0, 0, 1, 1, 2, 3])):
        buf.append(rng.choice(PREFIXES))
    lead = rng.random()
    if lead < 0.25:
        buf.append(0x0F)
        r = rng.random()
        if r < 0.25:
            buf.append(rng.choice([0x38, 0x3A]))
    elif lead < 0.35:
        buf += bytes([0xC5, rng.randrange(256)])
    elif lead < 0.45:
        buf += bytes([0xC4, 0xE0 | rng.randrange(1, 4), rng.randrange(256)])
    elif lead < 0.55:
        buf += bytes([0x62, (rng.randrange(256) & 0xF0) | rng.choice([1, 2, 3, 5, 6]), rng.randrange(256) | 0x04,
                      rng.randrange(256)])
    elif lead < 0.58:
        buf += bytes([0x8F, 0xE0 | rng.choice([8, 9, 10]), rng.randrange(256)])
    while len(buf) < 16:
        buf.append(rng.randrange(256))
    return bytes(buf[:16])


def random_listing(objdump, assembler, work, count, seed):
    """Each random string after a symbol of its own in an object of ours, so that llvm-objdump starts afresh at each;
    the first instruction it decodes after each symbol is the one compared."""
    rng = random.Random(seed)
    buffers = [random_buffer(rng) for _ in range(count)]
    os.makedirs(work, exist_ok=True)
    source = os.path.join(work, "x86-random.s")
    obj = os.path.join(work, "x86-random.o")
    with open(source, "w") as f:
        f.write("  .text\n")
        for i, buf in enumerate(buffers):
            f.write("x86r_%d:\n  .byte %s\n" % (i, ", ".join("0x%02x" % b for b in buf)))
    subprocess.run([assembler, "-c", "-x", "assembler", source, "-o", obj], check=True)
    out = subprocess.run([objdump, "-d", obj], capture_output=True, text=True, check=True)
    lines = []
    taken = set()
    for addr, data, text, symbol in merge_prefixes(parse_objdump(out.stdout)):
        m = re.match(r"^x86r_(\d+)$", symbol or "")
        if not m or int(m.group(1)) in taken:
            continue
        index = int(m.group(1))
        taken.add(index)
        lines.append(listing_line(addr, buffers[index], len(data), text))
    return lines


def run_checker(checker, work, name, lines, random_mode=False):
    os.makedirs(work, exist_ok=True)
    path = os.path.join(work, "x86-%s.listing" % name)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    out = subprocess.run([checker] + (["--random"] if random_mode else []) + [path], capture_output=True, text=True,
                         check=False)
    sys.stdout.write(out.stdout)
    sys.stderr.write(out.stderr)
    return out.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="mode", required=True)
    o = sub.add_parser("objects")
    o.add_argument("--checker", required=True)
    o.add_argument("--objdump", required=True)
    o.add_argument("--work", required=True)
    o.add_argument("files", nargs="+")
    r = sub.add_parser("random")
    r.add_argument("--checker", required=True)
    r.add_argument("--objdump", required=True)
    r.add_argument("--assembler", required=True)
    r.add_argument("--work", required=True)
    r.add_argument("--count", type=int, default=2000)
    r.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    if args.mode == "objects":
        lines = []
        for path in args.files:
            got = objdump_listing(args.objdump, path)
            print("%s: %d instructions" % (os.path.basename(path), len(got)))
            lines += got
        return run_checker(args.checker, args.work, "objects", lines)
    lines = random_listing(args.objdump, args.assembler, args.work, args.count, args.seed)
    return run_checker(args.checker, args.work, "random", lines, random_mode=True)


if __name__ == "__main__":
    sys.exit(main())
