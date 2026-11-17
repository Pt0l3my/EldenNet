#!/usr/bin/env python3
"""Builds a spike package of EldenNet for the owner's PC (the first in-game build's part 10): a development build,
labelled 0.1.0-spike.N and never shipped to players, zipped with gal/, the licences and SHA256SUMS. Run it with
`python3 -I`, from anywhere:

    build.py r1 --build build-cl-rel --out EldenNet-R1.zip [--engine build/src/en-tools/eldennet-engine]

`r1` is the derivation's in-game round (O89): the clang-cl Release tree's eldennet-launcher.exe, the probe build of
eldennet.dll (EN_PROBE) and eldennet-engine.exe, the repository's gal/ and README-R1.txt, in one folder, EldenNet-R1/.
The build tree must be a Release one with EN_PROBE on and its label set (EN_VERSION_LABEL); the label must be a spike
package's. `--engine` is a Linux eldennet-engine of the same commit, which checks gal/ (lint --gal) before it is
packed. The zip is the same for the same inputs: its entries are sorted and dated 2026-01-01.

Nothing is read from outside the repository and the build tree, and the package holds nothing personal: no path of
this PC (each text file is checked), no key. The privacy sweep still runs on the zip before it leaves.
"""
import argparse
import hashlib
import os
import re
import subprocess
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.dirname(os.path.abspath(__file__))
DATE = (2026, 1, 1, 0, 0, 0)
LABEL = re.compile(r"0\.1\.0-spike\.[0-9]+")
DISCLOSURE_SOURCE = os.path.join(REPO, "src", "en-tools", "src", "probe_exe.cpp")


def fail(text):
    print("spikepkg: %s" % text, file=sys.stderr)
    sys.exit(1)


def cache(build):
    values = {}
    with open(os.path.join(build, "CMakeCache.txt"), encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^([A-Za-z_0-9]+):[A-Z]+=(.*)$", line.rstrip("\n"))
            if m:
                values[m.group(1)] = m.group(2)
    return values


def disclosure():
    """The devkit's disclosure as probe-exe --help prints it, from its source (one text, two places)."""
    with open(DISCLOSURE_SOURCE, encoding="utf-8") as f:
        src = f.read()
    start = src.index('"Safe use.')
    end = src.index('";', start) + 1  # the literal's own end: the text holds semicolons of its own
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', src[start:end])
    text = "".join(parts).replace("\\n", "\n").replace("\\'", "'").replace('\\"', '"')
    return "\n".join("  " + line if line else "" for line in text.rstrip("\n").split("\n"))


def git_commit():
    out = subprocess.run(["git", "-C", REPO, "rev-parse", "--short=7", "HEAD"], capture_output=True, text=True,
                         check=True)
    dirty = subprocess.run(["git", "-C", REPO, "status", "--porcelain", "--untracked-files=no"], capture_output=True,
                           text=True, check=True).stdout.strip()
    return out.stdout.strip() + ("+changes" if dirty else "")


def notices(build):
    """THIRD-PARTY-NOTICES.txt: the licence of each library the programs hold, unchanged."""
    libs = [("inja 3.4.0 (https://github.com/pantor/inja)", os.path.join(REPO, "third_party", "inja", "LICENSE")),
            ("nlohmann/json 3.11.3 (https://github.com/nlohmann/json)",
             os.path.join(build, "_deps", "nlohmann", "LICENSE.MIT")),
            ("toml++ 3.4.0 (https://github.com/marzer/tomlplusplus)",
             os.path.join(build, "_deps", "tomlplusplus", "LICENSE"))]
    out = ["Third-party notices for EldenNet's programs", "============================================", "",
           "eldennet-engine.exe holds the three header-only libraries below, eldennet-launcher.exe and eldennet.dll two "
           "of them",
           "(nlohmann/json and toml++), each under the MIT licence. Their licence texts follow, unchanged. The static",
           "Microsoft C runtime (/MT) is linked into each program as well; THIRD_PARTY.md lists every component.", ""]
    for title, path in libs:
        with open(path, encoding="utf-8") as f:
            text = f.read().rstrip("\n")
        out += ["-" * 72, title, "-" * 72, "", text, ""]
    return "\n".join(out) + "\n"


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


PERSONAL = re.compile(r"(?i)(/home/|/root/|/tmp/|[A-Z]:\\Users\\|scratchpad)")


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("kind", choices=["r1"])
    p.add_argument("--build", required=True, help="the clang-cl Release tree, with EN_PROBE and a spike label")
    p.add_argument("--out", required=True, help="the zip to write")
    p.add_argument("--engine", help="a Linux eldennet-engine of the same commit, for lint --gal")
    a = p.parse_args()
    c = cache(a.build)
    label = c.get("EN_VERSION_LABEL", "")
    if c.get("CMAKE_BUILD_TYPE") != "Release" or c.get("EN_PROBE") != "ON" or not LABEL.fullmatch(label):
        fail("the build tree must be Release, with EN_PROBE=ON and EN_VERSION_LABEL=0.1.0-spike.N")
    if "clang-cl" not in c.get("CMAKE_TOOLCHAIN_FILE", ""):
        fail("the build tree must be the clang-cl one")
    gal = os.path.join(REPO, "gal")
    if a.engine:
        r = subprocess.run([a.engine, "lint", "--gal", gal], capture_output=True, text=True)
        if r.returncode != 0:
            fail("lint --gal refused gal/:\n" + r.stdout)
        print(r.stdout.strip())
    binaries = [("eldennet-launcher.exe", os.path.join(a.build, "src", "en-launcher", "eldennet-launcher.exe")),
                ("eldennet.dll", os.path.join(a.build, "src", "eldennet", "probe", "eldennet.dll")),
                ("eldennet-engine.exe", os.path.join(a.build, "src", "en-tools", "eldennet-engine.exe"))]
    files = []  # (name in the package, bytes)
    for name, path in binaries:
        with open(path, "rb") as f:
            data = f.read()
        if name == "eldennet.dll" and b"S\x00p\x00e\x00c\x00i\x00a\x00l\x00B\x00u\x00i\x00l\x00d" not in data:
            fail("eldennet.dll is not the probe build (no SpecialBuild in its version resource)")
        if label.encode("utf-16-le") not in data and label.encode() not in data:
            fail("%s does not carry the label %s" % (name, label))
        if b"ms_asmv1:" in data:  # a linker-merged manifest Windows refuses with a side-by-side error (spike.1)
            fail("%s carries a merged manifest with namespaced attributes" % name)
        if name == "eldennet-launcher.exe":
            with open(os.path.join(REPO, "src", "en-launcher", "src", "win", "launcher.manifest"), "rb") as f:
                if f.read().strip() not in data:
                    fail("eldennet-launcher.exe does not embed launcher.manifest as it stands")
        files.append((name, data))
    for root, _, names in sorted(os.walk(gal)):
        for n in sorted(names):
            path = os.path.join(root, n)
            rel = os.path.relpath(path, REPO).replace(os.sep, "/")
            with open(path, "rb") as f:
                files.append((rel, f.read()))  # byte for byte: the DLL and lint --gal read these very bytes
    with open(os.path.join(HERE, "README-R1.txt.in"), encoding="utf-8") as f:
        title = "EldenNet, round R1 (package %s)" % label
        readme = (f.read().replace("@LABEL@", label).replace("@UNDERLINE@", "=" * len(title))
                  .replace("@COMMIT@", git_commit()).replace("@DISCLOSURE@", disclosure()))
    if re.search(r"@[A-Z]+@", readme):
        fail("README-R1.txt has a placeholder left")
    files.append(("README-R1.txt", crlf(readme)))
    with open(os.path.join(REPO, "LICENSE"), encoding="utf-8") as f:
        files.append(("LICENSE.txt", crlf(f.read())))
    with open(os.path.join(REPO, "THIRD_PARTY.md"), encoding="utf-8") as f:
        files.append(("THIRD_PARTY.md", crlf(f.read())))
    files.append(("THIRD-PARTY-NOTICES.txt", crlf(notices(a.build))))
    for name, data in files:
        if not name.endswith((".exe", ".dll")) and PERSONAL.search(data.decode("utf-8")):
            fail("%s holds a path of this PC" % name)
    sums = "".join("%s  %s\n" % (hashlib.sha256(d).hexdigest(), n) for n, d in files)
    files.append(("SHA256SUMS", sums.encode()))
    folder = "EldenNet-R1"
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    tmp = a.out + ".part"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(files, key=lambda f: f[0]):
            info = zipfile.ZipInfo("%s/%s" % (folder, name), DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    os.replace(tmp, a.out)
    with open(a.out, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()
    print("spikepkg: %s written: %d files, label %s, SHA-256 %s" % (os.path.basename(a.out), len(files), label, digest))
    print(sums, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
