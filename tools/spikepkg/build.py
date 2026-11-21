#!/usr/bin/env python3
"""Builds a spike package of EldenNet for the owner's PC (the first in-game build's part 10): a development build,
labelled 0.1.0-spike.N (0.1.0-spikes.N for GraceUI's spikes) and never shipped to players, zipped with gal/, the
licences and SHA256SUMS. Run it with `python3 -I`, from anywhere:

    build.py r1 --build build-cl-rel --out EldenNet-R1.zip [--engine build/src/en-tools/eldennet-engine]
    build.py s0e-s4 --build build-cl-rel --graceui <graceui>/build-cl-test --out EldenNet-Spikes.zip
    build.py find --build build-cl-rel --out EldenNet-Find.zip [--engine build/src/en-tools/eldennet-engine]

`r1` is the derivation's in-game round (O89): the clang-cl Release tree's eldennet-launcher.exe, the probe build of
eldennet.dll (EN_PROBE) and eldennet-engine.exe, the repository's gal/ and README-R1.txt, in one folder, EldenNet-R1/.
The build tree must be a Release one with EN_PROBE on and its label set (EN_VERSION_LABEL); the label must be a spike
package's. `--engine` is a Linux eldennet-engine of the same commit, which checks gal/ (lint --gal) before it is
packed. The zip is the same for the same inputs: its entries are sorted and dated 2026-01-01.

`s0e-s4` is GraceUI's in-game spikes S0-E and S4 with EldenNet (GraceUI D175; EldenNet's side WHY§249 to §251): the
clang-cl Release tree's eldennet-launcher.exe and release eldennet.dll (EN_PROBE off, a 0.1.0-spikes.N label), gal/,
GraceUI's test build (`--graceui`, a clang-cl Release tree of GraceUI with GRACEUI_TEST_EXPORTS: its GraceUI folder as
the build laid it out, every file checked against its graceui.toml, and its grace-spike.dll and grace-standin.dll),
one .cmd file for each run (RUNS) and README-SPIKES.txt, in one folder, EldenNet-Spikes/.

`find` is Lodestar's offline finding run (proposal 0005, step L3; O100 to O102): the clang-cl Release tree's
eldennet-engine.exe (a 0.1.0-lodestar.N label; EN_PROBE either way), gal/ (with recipes.toml and no patterns.toml),
find.cmd, onto which the owner drops the game's executable file, and README-FIND.txt, in one folder, EldenNet-Find/.
It holds no launcher and no DLL: nothing in it starts or enters the game.

Nothing is read from outside the repository and the build trees (GraceUI's commit from its own source tree), and the
package holds nothing personal: no path of this PC (each text file is checked), no key. The privacy sweep still runs
on the zip before it leaves.
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
SPIKES_LABEL = re.compile(r"0\.1\.0-spikes\.[0-9]+")
FIND_LABEL = re.compile(r"0\.1\.0-lodestar\.[0-9]+")
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


def git_commit(repo=REPO):
    out = subprocess.run(["git", "-C", repo, "rev-parse", "--short=7", "HEAD"], capture_output=True, text=True,
                         check=True)
    dirty = subprocess.run(["git", "-C", repo, "status", "--porcelain", "--untracked-files=no"], capture_output=True,
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


def spikes_notices(build):
    """THIRD-PARTY-NOTICES.txt for the s0e-s4 package: the libraries eldennet-launcher.exe and eldennet.dll hold, their
    licences unchanged, and where GraceUI's own files keep theirs."""
    libs = [("nlohmann/json 3.11.3 (https://github.com/nlohmann/json)",
             os.path.join(build, "_deps", "nlohmann", "LICENSE.MIT")),
            ("toml++ 3.4.0 (https://github.com/marzer/tomlplusplus)",
             os.path.join(build, "_deps", "tomlplusplus", "LICENSE")),
            ("Dear ImGui 1.93.0, its core (https://github.com/ocornut/imgui, tag v1.93.0)",
             os.path.join(build, "_deps", "imgui-1.93.0", "LICENSE.txt")),
            ("GraceUI's graceui-sys and ABI headers (GraceUI, MIT)",
             os.path.join(REPO, "third_party", "graceui-sys", "LICENSE"))]
    out = ["Third-party notices for EldenNet's programs", "============================================", "",
           "eldennet-launcher.exe and eldennet.dll hold nlohmann/json and toml++, and eldennet.dll also Dear ImGui's core",
           "(for the F11 view, with no backend) and GraceUI's graceui-sys headers, each under the MIT licence. Their",
           "licence texts follow, unchanged. Dear ImGui's core includes stb_rect_pack, stb_textedit and stb_truetype, by",
           "Sean Barrett (taken under the MIT licence, of their choice of it or the public domain), and the ProggyClean",
           "and ProggyForever fonts (MIT licence); GraceUI\\NOTICE.txt names each copyright. The static Microsoft C",
           "runtime (/MT) is linked into each program as well; THIRD_PARTY.md lists every component.", "",
           "GraceUI's files in this package (the GraceUI folder, grace-spike.dll and grace-standin.dll) are GraceUI's,",
           "under its own MIT licence, GraceUI\\LICENSE.txt, with the notices of everything they hold (Dear ImGui with",
           "two of its backends, MinHook, and the fonts under the SIL Open Font License) in GraceUI\\NOTICE.txt and",
           "GraceUI\\fonts.", ""]
    for title, path in libs:
        with open(path, encoding="utf-8") as f:
            text = f.read().rstrip("\n")
        out += ["-" * 72, title, "-" * 72, "", text, ""]
    return "\n".join(out) + "\n"


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


PERSONAL = re.compile(r"(?i)(/home/|/root/|/tmp/|[A-Z]:\\Users\\|scratchpad)")


def pack(files, folder, out, label):
    """SHA256SUMS of every file, then the zip: one folder, entries sorted and dated DATE, so the same inputs give the
    same zip."""
    sums = "".join("%s  %s\n" % (hashlib.sha256(d).hexdigest(), n) for n, d in files)
    files.append(("SHA256SUMS", sums.encode()))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tmp = out + ".part"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(files, key=lambda f: f[0]):
            info = zipfile.ZipInfo("%s/%s" % (folder, name), DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    os.replace(tmp, out)
    with open(out, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()
    print("spikepkg: %s written: %d files, label %s, SHA-256 %s" % (os.path.basename(out), len(files), label, digest))
    print(sums, end="")
    return 0


def launcher_checks(name, data, label):
    """What both kinds check in eldennet-launcher.exe and eldennet.dll: the label, no merged manifest, and the
    launcher's own manifest embedded as written."""
    if label.encode("utf-16-le") not in data and label.encode() not in data:
        fail("%s does not carry the label %s" % (name, label))
    if b"ms_asmv1:" in data:  # a linker-merged manifest Windows refuses with a side-by-side error (spike.1)
        fail("%s carries a merged manifest with namespaced attributes" % name)
    if name == "eldennet-launcher.exe":
        with open(os.path.join(REPO, "src", "en-launcher", "src", "win", "launcher.manifest"), "rb") as f:
            if f.read().strip() not in data:
                fail("eldennet-launcher.exe does not embed launcher.manifest as it stands")


def gal_files():
    files = []
    gal = os.path.join(REPO, "gal")
    for root, _, names in sorted(os.walk(gal)):
        for n in sorted(names):
            path = os.path.join(root, n)
            rel = os.path.relpath(path, REPO).replace(os.sep, "/")
            with open(path, "rb") as f:
                files.append((rel, f.read()))  # byte for byte: the DLL and lint --gal read these very bytes
    return files


# GraceUI's in-game spikes (its tools/grace-spike/README.md, "The runs, in order"): each run's file and the launcher's
# options, which name nothing but the package's own files. --console gives the launcher's lines to the run's console
# window (the launcher is a Windows program, which writes to no console without it).
RUNS = [
    ("A1", "S0-E with EldenNet: GraceUI loaded by the launcher before EldenNet, injected early",
     ["--console", "--spike", "s0e", "--inject-at", "suspended", "--also", "GraceUI\\graceui.dll",
      "--also", "grace-spike.dll", "--also", "grace-standin.dll"]),
    ("A2", "S0-E with EldenNet: GraceUI loaded by the launcher before EldenNet, injected late",
     ["--console", "--spike", "s0e", "--inject-at", "window", "--also", "GraceUI\\graceui.dll",
      "--also", "grace-spike.dll", "--also", "grace-standin.dll"]),
    ("A3", "S0-E with EldenNet: GraceUI loaded by EldenNet itself after the window, injected early",
     ["--console", "--spike", "s0e", "--inject-at", "suspended", "--also", "grace-standin.dll",
      "--also", "grace-spike.dll"]),
    ("A4", "S0-E with EldenNet: GraceUI loaded by EldenNet itself after the window, injected late",
     ["--console", "--spike", "s0e", "--inject-at", "window", "--also", "grace-standin.dll",
      "--also", "grace-spike.dll"]),
    ("A0", "S0-E with GraceUI alone: GraceUI, the observer and the stand-in, no EldenNet, injected early",
     ["--console", "--spike", "s0e_alone", "--inject-at", "suspended", "--also", "GraceUI\\graceui.dll",
      "--also", "grace-spike.dll", "--also", "grace-standin.dll"]),
    ("B", "S4, input: with EldenNet, injected early; EldenNet loads GraceUI",
     ["--console", "--spike", "s4", "--inject-at", "suspended", "--also", "grace-spike.dll"]),
]


def run_cmd(run, what, options, label):
    """One run's .cmd file: from the package's own folder (%~dp0), the launcher with the run's options, then a pause so
    that its console stays readable."""
    return crlf("\n".join([
        "@echo off",
        "rem EldenNet's spike package %s, run %s: %s." % (label, run, what),
        "rem README-SPIKES.txt has the run's steps. This file names nothing outside the package's own folder.",
        "setlocal",
        "cd /d \"%~dp0\"",
        "eldennet-launcher.exe " + " ".join(options),
        "echo.",
        "echo Run %s has ended (the launcher's exit code: %%ERRORLEVEL%%). README-SPIKES.txt says what to send back."
        % run,
        "pause", ""]))


def graceui_files(tree):
    """GraceUI's test build: its GraceUI folder as the build laid it out, every file as its graceui.toml lists it (no
    file missing, changed or extra; its user/ folder of kept settings never), graceui.dll with its test exports, and
    the two spike tools from its bin. Answers the files and GraceUI's version and commit."""
    import tomllib  # Python 3.11 and later
    c = cache(tree)
    if c.get("CMAKE_BUILD_TYPE") != "Release" or c.get("GRACEUI_TEST_EXPORTS") != "ON":
        fail("GraceUI's tree must be Release, with GRACEUI_TEST_EXPORTS=ON")
    if "clang-cl" not in c.get("CMAKE_TOOLCHAIN_FILE", ""):
        fail("GraceUI's tree must be its clang-cl one")
    folder = os.path.join(tree, "GraceUI")
    with open(os.path.join(folder, "graceui.toml"), "rb") as f:
        raw = f.read()
    manifest = tomllib.loads(raw.decode("utf-8"))
    listed = manifest.get("files", {})
    found = set()
    for root, _, names in os.walk(folder):
        for n in names:
            found.add(os.path.relpath(os.path.join(root, n), folder).replace(os.sep, "/"))
    found.discard("graceui.toml")
    # GraceUI keeps the player's settings in user/ (its D179), which a release never carries and its manifest skips: a
    # build's folder gets one once a test run has used it. Never packed.
    user = sorted(n for n in found if n.startswith("user/"))
    if any(n.startswith("user/") for n in listed):
        fail("GraceUI's graceui.toml lists a file under user/, which a release never carries")
    if user:
        print("spikepkg: GraceUI\\user\\ left out (%d file(s): the settings a test run kept, D179)" % len(user))
    found -= set(user)
    if found != set(listed):
        fail("GraceUI's folder and its graceui.toml differ: %s" % sorted(found ^ set(listed)))
    files = [("GraceUI/graceui.toml", raw)]
    for name in sorted(listed):
        with open(os.path.join(folder, *name.split("/")), "rb") as f:
            data = f.read()
        if len(data) != listed[name]["size"] or hashlib.sha256(data).hexdigest() != listed[name]["sha256"]:
            fail("GraceUI\\%s is not the file its graceui.toml lists" % name)
        files.append(("GraceUI/" + name, data))  # byte for byte: the launcher's pre-flight checks these very bytes
    dll = dict(files)["GraceUI/graceui.dll"]
    if b"GraceUI_Test_GetState" not in dll:
        fail("GraceUI\\graceui.dll is not a test build (no GraceUI_Test_GetState): the observer reads its counters")
    for name in ("grace-spike.dll", "grace-standin.dll"):
        with open(os.path.join(tree, "bin", name), "rb") as f:
            files.append((name, f.read()))
    version = manifest.get("graceui", "")
    if version != c.get("CMAKE_PROJECT_VERSION"):
        fail("graceui.toml names GraceUI %s, its tree %s" % (version, c.get("CMAKE_PROJECT_VERSION")))
    return files, version, git_commit(c["CMAKE_HOME_DIRECTORY"])


def spikes(a):
    """The s0e-s4 kind: GraceUI's in-game spikes with the release DLL."""
    c = cache(a.build)
    label = c.get("EN_VERSION_LABEL", "")
    if c.get("CMAKE_BUILD_TYPE") != "Release" or c.get("EN_PROBE") == "ON" or not SPIKES_LABEL.fullmatch(label):
        fail("the build tree must be Release, with EN_PROBE off and EN_VERSION_LABEL=0.1.0-spikes.N")
    if "clang-cl" not in c.get("CMAKE_TOOLCHAIN_FILE", ""):
        fail("the build tree must be the clang-cl one")
    if not a.graceui:
        fail("s0e-s4 needs --graceui, GraceUI's clang-cl test build")
    if a.engine:
        r = subprocess.run([a.engine, "lint", "--gal", os.path.join(REPO, "gal")], capture_output=True, text=True)
        if r.returncode != 0:
            fail("lint --gal refused gal/:\n" + r.stdout)
        print(r.stdout.strip())
    files = []
    for name, path in [("eldennet-launcher.exe", os.path.join(a.build, "src", "en-launcher", "eldennet-launcher.exe")),
                       ("eldennet.dll", os.path.join(a.build, "src", "eldennet", "eldennet.dll"))]:
        with open(path, "rb") as f:
            data = f.read()
        if name == "eldennet.dll" and b"S\x00p\x00e\x00c\x00i\x00a\x00l\x00B\x00u\x00i\x00l\x00d" in data:
            fail("eldennet.dll is the probe build (SpecialBuild in its version resource): these runs take the release")
        launcher_checks(name, data, label)
        files.append((name, data))
    files += gal_files()
    grace, grace_version, grace_commit = graceui_files(a.graceui)
    files += grace
    for run, what, options in RUNS:
        files.append(("run-%s.cmd" % run, run_cmd(run, what, options, label)))
    with open(os.path.join(HERE, "README-SPIKES.txt.in"), encoding="utf-8") as f:
        title = "EldenNet with GraceUI, the spikes S0-E and S4 (package %s)" % label
        readme = (f.read().replace("@LABEL@", label).replace("@UNDERLINE@", "=" * len(title))
                  .replace("@COMMIT@", git_commit()).replace("@GRACE_VERSION@", grace_version)
                  .replace("@GRACE_COMMIT@", grace_commit))
    if re.search(r"@[A-Z_]+@", readme):
        fail("README-SPIKES.txt has a placeholder left")
    files.append(("README-SPIKES.txt", crlf(readme)))
    with open(os.path.join(REPO, "LICENSE"), encoding="utf-8") as f:
        files.append(("LICENSE.txt", crlf(f.read())))
    with open(os.path.join(REPO, "THIRD_PARTY.md"), encoding="utf-8") as f:
        files.append(("THIRD_PARTY.md", crlf(f.read())))
    files.append(("THIRD-PARTY-NOTICES.txt", crlf(spikes_notices(a.build))))
    for name, data in files:
        if name.endswith((".txt", ".md", ".cmd", ".toml", ".html")) and PERSONAL.search(data.decode("utf-8")):
            fail("%s holds a path of this PC" % name)
    return pack(files, "EldenNet-Spikes", a.out, label)


def find_cmd(label):
    """find.cmd: from the package's own folder, the finder on the one file dropped onto it, then the lint on what it
    wrote, then a pause. It names nothing outside the package's folder but the file it is given, which it never
    repeats."""
    return crlf("\n".join([
        "@echo off",
        "rem EldenNet's package %s: Lodestar's finding run. README-FIND.txt has the steps." % label,
        "rem Drop the game's executable file onto this file. It is read, never changed, and the game is not started.",
        "setlocal",
        "cd /d \"%~dp0\"",
        "if \"%~1\"==\"\" goto :nofile",
        "if not \"%~2\"==\"\" goto :nofile",
        "echo Reading the file. This takes a minute or two.",
        "eldennet-engine.exe lodestar find \"%~1\" --gal gal --out lodestar-found.json",
        "set FOUND=%ERRORLEVEL%",
        "if %FOUND% LEQ 1 eldennet-engine.exe lint --report lodestar-found.json",
        "echo.",
        "echo The run has ended (exit code: %FOUND%). README-FIND.txt says what to send back.",
        "pause",
        "exit /b %FOUND%",
        ":nofile",
        "echo Drop one file onto find.cmd: the game's executable, from the Game folder of your install.",
        "echo README-FIND.txt has the steps.",
        "pause",
        "exit /b 2", ""]))


def find(a):
    """The package for Lodestar's offline finding run (proposal 0005, step L3): the engine, gal/, find.cmd and its
    README. No launcher and no DLL: nothing in it starts or enters the game."""
    c = cache(a.build)
    label = c.get("EN_VERSION_LABEL", "")
    if c.get("CMAKE_BUILD_TYPE") != "Release" or not FIND_LABEL.fullmatch(label):
        fail("the build tree must be Release, with EN_VERSION_LABEL=0.1.0-lodestar.N")
    if "clang-cl" not in c.get("CMAKE_TOOLCHAIN_FILE", ""):
        fail("the build tree must be the clang-cl one")
    gal = os.path.join(REPO, "gal")
    if a.engine:
        r = subprocess.run([a.engine, "lint", "--gal", gal], capture_output=True, text=True)
        if r.returncode != 0:
            fail("lint --gal refused gal/:\n" + r.stdout)
        print(r.stdout.strip())
    if not os.path.isfile(os.path.join(gal, "recipes.toml")) or os.path.exists(os.path.join(gal, "patterns.toml")):
        fail("gal/ must hold recipes.toml and no patterns.toml (O102)")
    with open(os.path.join(a.build, "src", "en-tools", "eldennet-engine.exe"), "rb") as f:
        engine = f.read()
    if label.encode() not in engine:
        fail("eldennet-engine.exe does not carry the label %s" % label)
    if b"lodestar find" not in engine:
        fail("eldennet-engine.exe has no lodestar command")
    files = [("eldennet-engine.exe", engine), ("find.cmd", find_cmd(label))]
    files += gal_files()
    with open(os.path.join(HERE, "README-FIND.txt.in"), encoding="utf-8") as f:
        title = "EldenNet, Lodestar's finding run (package %s)" % label
        readme = (f.read().replace("@LABEL@", label).replace("@UNDERLINE@", "=" * len(title))
                  .replace("@COMMIT@", git_commit()).replace("@DISCLOSURE@", disclosure()))
    if re.search(r"@[A-Z]+@", readme):
        fail("README-FIND.txt has a placeholder left")
    files.append(("README-FIND.txt", crlf(readme)))
    with open(os.path.join(REPO, "LICENSE"), encoding="utf-8") as f:
        files.append(("LICENSE.txt", crlf(f.read())))
    with open(os.path.join(REPO, "THIRD_PARTY.md"), encoding="utf-8") as f:
        files.append(("THIRD_PARTY.md", crlf(f.read())))
    files.append(("THIRD-PARTY-NOTICES.txt", crlf(notices(a.build))))
    for name, data in files:
        if not name.endswith((".exe", ".dll")) and PERSONAL.search(data.decode("utf-8")):
            fail("%s holds a path of this PC" % name)
    return pack(files, "EldenNet-Find", a.out, label)


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("kind", choices=["r1", "s0e-s4", "find"])
    p.add_argument("--build", required=True, help="the clang-cl Release tree, with EN_PROBE and a spike label (r1), or "
                   "with EN_PROBE off and a 0.1.0-spikes.N label (s0e-s4)")
    p.add_argument("--graceui", help="s0e-s4: GraceUI's clang-cl Release tree with GRACEUI_TEST_EXPORTS")
    p.add_argument("--out", required=True, help="the zip to write")
    p.add_argument("--engine", help="a Linux eldennet-engine of the same commit, for lint --gal")
    a = p.parse_args()
    if a.kind == "s0e-s4":
        return spikes(a)
    if a.kind == "find":
        return find(a)
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
        launcher_checks(name, data, label)
        files.append((name, data))
    files += gal_files()
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
    return pack(files, "EldenNet-R1", a.out, label)


if __name__ == "__main__":
    sys.exit(main())
