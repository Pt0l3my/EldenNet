#!/usr/bin/env python3
"""privsweep: the mandatory privacy and safety sweep (d414). Nothing leaves a session before it passes.

Where it is enforced (tools/privsweep/README.md):
  git push   git's pre-push hook, on every repository (core.hooksPath -> ~/.config/privsweep/git-hooks): every
             object the remote doesn't have yet, so file contents and names, commit messages and identities, and
             tag messages
  uploads    Claude Code's PreToolUse hook (.claude/settings.json): Artifact publishes and asset uploads, files
             sent to the owner, GitHub writes, Claude Docs writes, web queries, and curl, scp or rsync uploads
Binaries are swept through their strings, archives (zip, 7z, tar, gzip) member by member, data: URIs decoded.

Rules. A hit blocks, and its value is never printed whole:
  personal   the owner's own details: values in $PRIVSWEEP_PRIVATE, ~/.config/privsweep/private.txt or
             tools/workplan/local/private.txt (never committed), and words whose salted hash is in personal.txt
  handle     the owner's handles, in GitHub writes outside the owner's own repositories
  identity   a commit's or tag's e-mail that is not a noreply address
  email      an e-mail address that is not a noreply or example address
  ip         a private, link-local or shared (CGNAT) IPv4 address
  userpath   a user folder: a Windows or macOS user's, or a home folder other than a generic one
  drive      a folder path on a drive other than C: (an upper-case drive letter and two folders at least)
  machine    a Windows machine name (DESKTOP- or LAPTOP- and seven characters)
  endpoint   a URL to a .local, .lan, .home or .internal host
  secret     an API key or token, a private key, a JWT, credentials in a URL, a literal bearer token
A false positive of any rule but personal is let through by a reviewed line in allow.txt.

Subcommands: hook (PreToolUse, JSON on stdin), push REMOTE URL (pre-push, refs on stdin), files PATH...,
text [WHERE] (stdin), repo DIR [--head|--all], install [--scope global|repo], hash (a value on stdin), selftest.
"""
import base64
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from bisect import bisect_right
from pathlib import Path

HERE = Path(__file__).resolve().parent
INSTALL = Path(os.environ.get("PRIVSWEEP_HOME") or (Path.home() / ".config" / "privsweep"))
HOOKS = INSTALL / "git-hooks"
OWN_ACCOUNTS = {"pt0l3my"}            # the owner's GitHub account: writes there are the owner's own
HANDLES = ("Pt0l3my", "Ptol3my")      # fine in the owner's own repositories, never in a write elsewhere
UNPACK_LIMIT = 1 << 30                # most an upload's archives may unpack to before the sweep refuses it
DEPTH_LIMIT = 4                       # archives inside archives
LISTED = 25                           # findings listed in a refusal

PRE_PUSH = """#!/bin/sh
# The privacy and safety sweep (d414; Skyrizz tools/privsweep): nothing is pushed before it passes.
refs=$(cat)
printf '%s\\n' "$refs" | python3 -I -S "$(dirname "$0")/../privsweep.py" push "$1" "$2" || exit 1
own="$(git rev-parse --git-common-dir)/hooks/pre-push"
if [ -x "$own" ]; then printf '%s\\n' "$refs" | "$own" "$@" || exit 1; fi
exit 0
"""


class SweepError(Exception):
    """Something the sweep could not read: the upload is refused (fail closed)."""


class Finding:
    __slots__ = ("rule", "where", "line", "shown", "value")

    def __init__(self, rule, where, line, shown, value):
        self.rule, self.where, self.line, self.shown, self.value = rule, where, line, shown, value

    def __str__(self):
        at = f"{self.where}:{self.line}" if self.line else self.where
        return f"{self.rule:8} {at}: {self.shown}"


def mask(v):
    v = v.replace("\n", " ")
    return f"{v[:2]}…({len(v)})" if len(v) > 4 else f"…({len(v)})"


# --- what counts as personal --------------------------------------------------------------------------------

def _private_files():
    files = [INSTALL / "private.txt"]
    for root in (os.environ.get("CLAUDE_PROJECT_DIR"), str(HERE.parent.parent)):
        if root:
            files.append(Path(root) / "tools" / "workplan" / "local" / "private.txt")
    return files


class Personal:
    """The owner's details: plain values (never committed) and salted hashes of words (personal.txt)."""

    def __init__(self, extra=()):
        values, regexes = [], []
        for part in re.split(r"[\n,]", os.environ.get("PRIVSWEEP_PRIVATE", "")):
            if part.strip():
                values.append(part.strip())
        for f in _private_files():
            if not f.exists():
                continue
            regex_file = f.parent.name == "local"              # wp check's file holds one regular expression a line
            for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if regex_file or line.startswith("re:"):
                    regexes.append(re.compile(line[3:] if line.startswith("re:") else line, re.IGNORECASE))
                else:
                    values.append(line)
        values += list(extra)
        self.plain = None
        if values:
            alts = "|".join(sorted({re.escape(v) for v in values}, key=len, reverse=True))
            self.plain = re.compile(rf"(?<![A-Za-z0-9])(?:{alts})(?![A-Za-z0-9])", re.IGNORECASE)
        self.regexes = regexes
        self.salt, self.hashes = "", set()
        for d in (HERE, INSTALL):
            f = d / "personal.txt"
            if f.exists():
                for line in f.read_text(encoding="utf-8").splitlines():
                    parts = line.split("#")[0].split()
                    if len(parts) == 2 and parts[0] == "salt":
                        self.salt = parts[1]
                    elif len(parts) == 2 and parts[0] == "sha256":
                        self.hashes.add(parts[1].lower())
                break

    def digest(self, word):
        return hashlib.sha256(f"{self.salt}:{word.lower()}".encode("utf-8")).hexdigest()

    def hits(self, text):
        if self.plain:
            for m in self.plain.finditer(text):
                yield m.start(), m.group(0)
        for rx in self.regexes:
            for m in rx.finditer(text):
                if m.group(0):
                    yield m.start(), m.group(0)
        if self.hashes:
            bad = {w for w in set(re.findall(r"[a-z0-9]{3,}", text.lower())) if self.digest(w) in self.hashes}
            if bad:
                for m in re.finditer(r"[A-Za-z0-9]{3,}", text):
                    if m.group(0).lower() in bad:
                        yield m.start(), m.group(0)


# --- the generic rules --------------------------------------------------------------------------------------

EMAIL = re.compile(r"(?<![A-Za-z0-9._%+-])[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}"
                   r"(?![A-Za-z0-9-])")
NOREPLY = re.compile(r"(?i)(noreply@anthropic\.com|(?:\d+\+)?[a-z0-9-]+(?:\[bot\])?@users\.noreply\.github\.com"
                     r"|noreply@github\.com)")
EXAMPLE_EMAIL = re.compile(r"(?i).+@(?:(?:[a-z0-9-]+\.)*example\.(?:com|org|net)|(?:[a-z0-9-]+\.)*"
                           r"(?:example|invalid|test|localhost))|git@github\.com")
FILE_ENDINGS = {"png", "jpg", "jpeg", "gif", "svg", "webp", "ico", "bmp", "dds", "tga", "css", "js", "mjs", "json",
                "md", "txt", "html", "htm", "nif", "esp", "esm", "esl", "dll", "exe", "pdb", "yaml", "yml", "xml",
                "cpp", "hpp", "py", "zip", "pex", "psc", "hkx", "wav", "ogg", "mp3", "fuz", "lip"}
IPV4 = re.compile(r"(?<![\d.])(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})(?!\.?\d)")
USERPATH = re.compile(r"(?i)([A-Z]:[\\/]+(?:Users|Documents and Settings)[\\/]+"
                      r"|(?<![\w.~-])/(?:Users|home)/+)([^\\/\s\"'<>|:*?`,;()\[\]{}]+)")
GENERIC_USERS = {"public", "default", "default user", "all users", "user", "users", "username", "yourname",
                 "your name", "yourusername", "you", "name", "me", "runner", "root", "claude", "ubuntu", "admin",
                 "administrator", "defaultapppool", "runneradmin", "runner~1", "vagrant", "pi", "deck", "steam", "node", "app", "x", "xxx",
                 "example", "someone", "somebody", "test", "dev", "build", "ci", "foo", "bar", "player"}
DRIVE = re.compile(r"(?<![A-Za-z0-9])([D-Z]):(\\\\|\\|/)+([A-Za-z0-9_$.~ -]+?)(?:\\\\|\\|/)+(?=[A-Za-z0-9_$.~-])")
MACHINE = re.compile(r"(?<![A-Za-z0-9-])(?:DESKTOP|LAPTOP)-([A-Z0-9]{7})(?![A-Za-z0-9-])")
ENDPOINT = re.compile(r"(?i)\b[a-z][a-z0-9+.-]*://[a-z0-9-]+(?:\.[a-z0-9-]+)*"
                      r"\.(?:local|lan|home|internal|localdomain|intranet|corp)(?![a-z0-9-])")
SECRETS = [
    ("API key", re.compile(r"(?<![A-Za-z0-9])sk-(?:ant-|or-|proj-|live-|test-)?[A-Za-z0-9_-]{20,}")),
    ("GitHub token", re.compile(r"(?<![A-Za-z0-9])(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})")),
    ("AWS key", re.compile(r"(?<![A-Za-z0-9])(?:AKIA|ASIA)[0-9A-Z]{16}(?![A-Za-z0-9])")),
    ("Google key", re.compile(r"(?<![A-Za-z0-9])AIza[0-9A-Za-z_-]{35}")),
    ("Slack token", re.compile(r"(?<![A-Za-z0-9])xox[abposr]-[A-Za-z0-9-]{10,}")),
    ("GitLab token", re.compile(r"(?<![A-Za-z0-9])glpat-[A-Za-z0-9_-]{20,}")),
    ("Hugging Face token", re.compile(r"(?<![A-Za-z0-9])hf_[A-Za-z0-9]{30,}")),
    ("private key", re.compile(r"-----BEGIN (?:[A-Z0-9]+ )*PRIVATE KEY-----")),
    ("JWT", re.compile(r"(?<![A-Za-z0-9])eyJ[A-Za-z0-9_-]{10,}\.eyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}")),
    ("credentials in a URL", re.compile(r"(?i)\b[a-z][a-z0-9+.-]*://[^/\s:@'\"<>]+:([^/\s@'\"<>]{3,})@[^\s'\"<>]")),
    ("bearer token", re.compile(r"(?i)\bbearer\s+([A-Za-z0-9._~+/-]{24,}=*)")),
    ("assigned secret", re.compile(r"(?i)\b(?:api[_-]?key|apikey|secret[_-]?key|client[_-]?secret|access[_-]?token"
                                   r"|auth[_-]?token|password|passwd)[\"']?\s*[:=]\s*[\"']([A-Za-z0-9_./+=-]{16,})[\"']")),
]
PLACEHOLDER = re.compile(r"(?i)x{4}|\*{3}|\.{3}|your|example|placeholder|redacted|changeme|dummy|sample|<|\$\{|\{\{")


def _looks_real(v):
    """A captured secret worth refusing: letters and digits, not a placeholder."""
    if PLACEHOLDER.search(v) or not re.search(r"[A-Za-z]", v) or not re.search(r"\d", v):
        return False
    return max(v.count(c) for c in set(v)) < len(v) / 2


def _private_ip(a, b):
    return a == 10 or (a == 172 and 16 <= b <= 31) or (a == 192 and b == 168) or (a == 169 and b == 254) \
        or (a == 100 and 64 <= b <= 127)


RULES_ALL = ("personal", "handle", "email", "ip", "userpath", "drive", "machine", "endpoint", "secret")
RULES_QUERY = ("personal", "handle", "email", "secret")       # what a web query is checked for


def _raw_hits(text, ctx, rules):
    """(rule, offset, value, shown) for each hit, before the allow list."""
    if "personal" in rules:
        for pos, v in ctx.personal.hits(text):
            yield "personal", pos, v, f"{v[:1]}…({len(v)})"
    if "handle" in rules and ctx.external:
        for m in re.finditer(r"(?<![A-Za-z0-9])(?:%s)(?![A-Za-z0-9])" % "|".join(HANDLES), text, re.IGNORECASE):
            yield "handle", m.start(), m.group(0), "the owner's handle"
    if "email" in rules:
        for m in EMAIL.finditer(text):
            v = m.group(0)
            if v.rsplit(".", 1)[1].lower() in FILE_ENDINGS:
                continue
            if not (NOREPLY.fullmatch(v) or EXAMPLE_EMAIL.fullmatch(v)):
                local, _, dom = v.partition("@")
                yield "email", m.start(), v, f"{mask(local)}@{dom}"
    if "ip" in rules:
        for m in IPV4.finditer(text):
            o = [int(x) for x in m.groups()]
            if max(o) <= 255 and _private_ip(o[0], o[1]):
                yield "ip", m.start(), m.group(0), f"{o[0]}.{o[1]}.x.x"
    if "userpath" in rules:
        for m in USERPATH.finditer(text):
            name = m.group(2)
            low = name.lower().strip(".")
            if low in GENERIC_USERS or not low or name[0] in "<%$*{[…." or set(low) <= {"x", "*", "."}:
                continue
            yield "userpath", m.start(), name, f"{m.group(1)}{mask(name)}"
    if "drive" in rules:
        for m in DRIVE.finditer(text):
            sep, first = m.group(2), m.group(3)
            if sep == "\\" and first[0] in "ntr" and (len(first) == 1 or not first[1].isalpha()):
                continue                                # an escape in JSON text ("P:\n1."), not a folder
            rest = re.match(r"[^\s\"'<>|*?]{0,40}", text[m.end():]).group(0)
            yield "drive", m.start(), m.group(0) + rest, f"{m.group(1)}:\\{mask(first + '/' + rest)}"
    if "machine" in rules:
        for m in MACHINE.finditer(text):
            if set(m.group(1)) != {"X"}:
                yield "machine", m.start(), m.group(0), f"{m.group(0)[:8]}{mask(m.group(1))}"
    if "endpoint" in rules:
        for m in ENDPOINT.finditer(text):
            yield "endpoint", m.start(), m.group(0), mask(m.group(0))
    if "secret" in rules:
        for name, rx in SECRETS:
            for m in rx.finditer(text):
                v = m.group(1) if rx.groups else m.group(0)
                if name != "private key" and not _looks_real(v):
                    continue
                yield "secret", m.start(), m.group(0), f"{name} {m.group(0)[:4]}…"


# --- the allow list -----------------------------------------------------------------------------------------

class Allow:
    """allow.txt: 'RULE VALUE-REGEX [WHERE-REGEX]' a line; never for personal."""

    def __init__(self):
        self.entries = []
        f = HERE / "allow.txt"
        if not f.exists():
            f = INSTALL / "allow.txt"
        if f.exists():
            for n, line in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split(None, 2)
                if len(parts) < 2 or parts[0] not in RULES_ALL + ("identity",) or parts[0] == "personal":
                    raise SweepError(f"allow.txt:{n}: not 'RULE VALUE-REGEX [WHERE-REGEX]' for a rule but personal")
                self.entries.append((parts[0], re.compile(parts[1]), re.compile(parts[2]) if len(parts) > 2 else None))

    def allows(self, rule, value, where):
        return any(r == rule and v.fullmatch(value) and (w is None or w.search(where))
                   for r, v, w in self.entries)


class Ctx:
    def __init__(self, external=False, extra_private=()):
        self.personal = Personal(extra_private)
        self.allow = Allow()
        self.external = external
        self.unpacked = 0


# --- sweeping -----------------------------------------------------------------------------------------------

def scan_text(text, where, ctx, rules=RULES_ALL, line_numbers=True):
    found = []
    starts = None
    for rule, pos, value, shown in _raw_hits(text, ctx, rules):
        if rule != "personal" and ctx.allow.allows(rule, value, where):
            continue
        line = 0
        if line_numbers:
            if starts is None:
                starts = [m.end() for m in re.finditer("\n", text)]
            line = bisect_right(starts, pos) + 1
        found.append(Finding(rule, where, line, shown, value))
    for m in re.finditer(r"data:[\w.+/-]*;base64,([A-Za-z0-9+/=]{64,})", text):
        try:
            data = base64.b64decode(m.group(1), validate=False)
        except ValueError:
            raise SweepError(f"{where}: a data: URI that doesn't decode")
        line = text.count("\n", 0, m.start()) + 1
        found += sweep_bytes(data, f"{where}:{line} (data: URI)", ctx, rules=rules)
    return found


def strings_of(data):
    parts = [m.group().decode("ascii") for m in re.finditer(rb"[\x20-\x7e]{4,}", data)]
    parts += [m.group().decode("utf-16-le") for m in re.finditer(rb"(?:[\x20-\x7e]\x00){4,}", data)]
    return "\n".join(parts)


def _note_unpacked(ctx, n):
    ctx.unpacked += n
    if ctx.unpacked > UNPACK_LIMIT:
        raise SweepError("its archives unpack to more than the sweep reads (1 GiB); split the upload")


def sweep_bytes(data, where, ctx, depth=0, rules=RULES_ALL):
    if depth > DEPTH_LIMIT:
        raise SweepError(f"{where}: archives nested too deep to sweep")
    head = data[:512]
    found = []
    if head[:4] in (b"PK\x03\x04", b"PK\x05\x06"):
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                for info in z.infolist():
                    found += scan_text(info.filename, f"{where}!{info.filename} (name)", ctx, rules, False)
                    if not info.is_dir():
                        _note_unpacked(ctx, info.file_size)
                        found += sweep_bytes(z.read(info), f"{where}!{info.filename}", ctx, depth + 1, rules)
        except (zipfile.BadZipFile, RuntimeError, NotImplementedError) as e:
            raise SweepError(f"{where}: a zip archive the sweep can't open ({e})")
        return found
    if head[:6] == b"7z\xbc\xaf\x27\x1c":
        return found + _sweep_7z(data, where, ctx, depth, rules)
    if head[:2] == b"\x1f\x8b" or head[:3] == b"BZh" or head[:6] == b"\xfd7zXZ\x00" or head[257:262] == b"ustar":
        try:
            with tarfile.open(fileobj=io.BytesIO(data)) as t:
                for m in t.getmembers():
                    found += scan_text(m.name, f"{where}!{m.name} (name)", ctx, rules, False)
                    if m.isfile():
                        _note_unpacked(ctx, m.size)
                        found += sweep_bytes(t.extractfile(m).read(), f"{where}!{m.name}", ctx, depth + 1, rules)
            return found
        except tarfile.TarError:
            if head[257:262] == b"ustar":
                raise SweepError(f"{where}: a tar archive the sweep can't open")
            import bz2, gzip, lzma
            unpack = gzip.decompress if head[:2] == b"\x1f\x8b" else bz2.decompress if head[:3] == b"BZh" \
                else lzma.decompress
            try:
                plain = unpack(data)
            except Exception as e:  # noqa: BLE001: anything unreadable is refused
                raise SweepError(f"{where}: a compressed file the sweep can't open ({e})")
            _note_unpacked(ctx, len(plain))
            return sweep_bytes(plain, f"{where} (unpacked)", ctx, depth + 1, rules)
    if b"\x00" in data[:8192]:
        return scan_text(strings_of(data), f"{where} (strings)", ctx, rules, False)
    return scan_text(data.decode("utf-8", errors="replace"), where, ctx, rules)


def _sweep_7z(data, where, ctx, depth, rules):
    exe = shutil.which("7z") or shutil.which("7za") or shutil.which("7zr")
    if not exe:
        raise SweepError(f"{where}: a 7z archive, and no 7z tool to open it")
    found = []
    with tempfile.TemporaryDirectory(prefix="privsweep-") as tmp:
        arc = Path(tmp) / "a.7z"
        arc.write_bytes(data)
        out = Path(tmp) / "x"
        r = subprocess.run([exe, "x", "-y", "-bd", "-bso0", "-bsp0", "-pprivsweep", f"-o{out}", str(arc)],
                           capture_output=True)
        if r.returncode != 0:
            raise SweepError(f"{where}: a 7z archive the sweep can't open")
        for p in sorted(out.rglob("*")):
            rel = p.relative_to(out).as_posix()
            found += scan_text(rel, f"{where}!{rel} (name)", ctx, rules, False)
            if p.is_file() and not p.is_symlink():
                _note_unpacked(ctx, p.stat().st_size)
                found += sweep_bytes(p.read_bytes(), f"{where}!{rel}", ctx, depth + 1, rules)
    return found


def sweep_path(path, ctx, label=None, rules=RULES_ALL):
    p = Path(path)
    label = label or str(p)
    if not p.exists():
        raise SweepError(f"{label}: not found, so it can't be swept")
    found = scan_text(p.name, f"{label} (name)", ctx, rules, False)
    if p.is_dir():
        for q in sorted(p.rglob("*")):
            if ".git" in q.relative_to(p).parts or not q.is_file():
                continue
            found += sweep_path(q, ctx, f"{label}/{q.relative_to(p).as_posix()}", rules)
        return found
    return found + sweep_bytes(p.read_bytes(), label, ctx, rules=rules)


def sweep_strings(obj, where, ctx, rules=RULES_ALL):
    """Every string in a JSON value, each on its own (escapes decoded), keys included."""
    found = []
    if isinstance(obj, str):
        found += scan_text(obj, where, ctx, rules)
    elif isinstance(obj, dict):
        for k, v in obj.items():
            found += scan_text(str(k), f"{where}.{k} (key)", ctx, rules, False)
            found += sweep_strings(v, f"{where}.{k}", ctx, rules)
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            found += sweep_strings(v, f"{where}[{i}]", ctx, rules)
    return found


# --- git ----------------------------------------------------------------------------------------------------

def git(repo, *args, data=None):
    r = subprocess.run(["git", "-C", str(repo), *args], input=data, capture_output=True)
    if r.returncode != 0:
        raise SweepError(f"git {' '.join(args[:3])}: {r.stderr.decode(errors='replace').strip()[:200]}")
    return r.stdout


def _cat_batch(repo, shas):
    out = {}
    for i in range(0, len(shas), 400):
        raw = git(repo, "cat-file", "--batch", data=("\n".join(shas[i:i + 400]) + "\n").encode())
        pos = 0
        while pos < len(raw):
            nl = raw.index(b"\n", pos)
            head = raw[pos:nl].decode().split()
            if len(head) < 3:
                raise SweepError(f"git: object {head[0][:9]} is missing")
            size = int(head[2])
            out[head[0]] = (head[1], raw[nl + 1:nl + 1 + size])
            pos = nl + 1 + size + 1
    return out


IDENT = re.compile(r"^(author|committer|tagger) (.*) <([^>]*)> \d+ [+-]\d{4}$")


def sweep_objects(repo, tips, exclude, ctx, label):
    """Every object reachable from tips and not from exclude: names, contents, messages and identities."""
    listing = git(repo, "rev-list", "--objects", *tips, "--not", *exclude).decode("utf-8", errors="replace")
    shas, paths = [], {}
    for line in listing.splitlines():
        sha, _, path = line.partition(" ")
        shas.append(sha)
        if path:
            paths[sha] = path
    found = scan_text("\n".join(sorted(set(paths.values()))), f"{label}: file names", ctx, line_numbers=False)
    objects = _cat_batch(repo, shas)
    for sha, (typ, body) in objects.items():
        if typ == "blob":
            where = f"{label}: {paths.get(sha, '?')} (blob {sha[:9]})"
            found += sweep_bytes(body, where, ctx)
        elif typ in ("commit", "tag"):
            text = body.decode("utf-8", errors="replace")
            head, _, message = text.partition("\n\n")
            where = f"{label}: {typ} {sha[:9]}"
            for line in head.split("\n"):
                m = IDENT.match(line)
                if not m:
                    continue
                kind, name, email = m.groups()
                if not NOREPLY.fullmatch(email) and not ctx.allow.allows("identity", email, where):
                    local, _, dom = email.partition("@")
                    found.append(Finding("identity", f"{where} {kind}", 0, f"{mask(local)}@{dom}", email))
                found += scan_text(name, f"{where} {kind} name", ctx, ("personal",), False)
                found += scan_text(email, f"{where} {kind} e-mail", ctx, ("personal",), False)
            if typ == "tag":
                message = message.split("-----BEGIN", 1)[0]
            found += scan_text(message, f"{where} message", ctx)
    return found


def cmd_push(argv):
    repo = Path.cwd()
    tips, have = [], []
    for line in sys.stdin.read().splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        _, lsha, _, rsha = parts
        if lsha.strip("0"):
            tips.append(lsha)
        if rsha.strip("0") and subprocess.run(["git", "cat-file", "-e", rsha], capture_output=True).returncode == 0:
            have.append(rsha)
    if not tips:
        return 0
    name = repo.name
    try:
        ctx = Ctx()
        found = sweep_objects(repo, tips, ["--remotes", *have], ctx, name)
    except SweepError as e:
        print(f"privsweep: push refused: {e}", file=sys.stderr)
        return 1
    if found:
        print(report(found, f"privsweep: push to {argv[0] if argv else 'the remote'} refused"), file=sys.stderr)
        return 1
    return 0


def report(found, title):
    lines = [f"{title}: {len(found)} finding{'s' if len(found) != 1 else ''} (d414; tools/privsweep/README.md)."]
    seen = set()
    for f in found:
        s = str(f)
        if s not in seen:
            seen.add(s)
            lines.append("  " + s)
        if len(seen) >= LISTED:
            lines.append(f"  … and {len(found) - LISTED} more")
            break
    lines.append("Remove each value. A false positive of a rule other than personal may be allowed by a reviewed line "
                 "in tools/privsweep/allow.txt.")
    return "\n".join(lines)


# --- the PreToolUse hook ------------------------------------------------------------------------------------

GIT_PUSH = re.compile(r"(?<![\w-])git(?:\s+(?:-[Cc]\s+(?:\"[^\"]*\"|'[^']*'|\S+)|--?[\w.-]+(?:=\S+)?))*\s+push(?![\w-])")
BYPASS = re.compile(r"(?i)--no-verify|core\.hookspath|(?<![\w])(?:HOME|XDG_CONFIG_HOME|GIT_CONFIG_GLOBAL|"
                    r"GIT_CONFIG_PARAMETERS|GIT_CONFIG_COUNT|GIT_CONFIG_SYSTEM|GIT_CONFIG)=")
HOOKS_CHANGE = re.compile(r"(?i)\bconfig\b(?:(?!--get|--list|\s-l\b)[^;&|\n])*core\.hookspath")


def _repos_named(command, cwd):
    dirs = [cwd]
    for m in re.finditer(r"(?:\bgit\s+-C|(?:^|[;&|(]\s*)cd)\s+(\"[^\"]*\"|'[^']*'|[^\s;&|)]+)", command):
        d = m.group(1).strip("\"'")
        d = os.path.expanduser(d)
        dirs.append(d if os.path.isabs(d) else os.path.join(cwd, d))
    return dirs


def check_bash(command, cwd, ctx):
    """(refusal, findings): a push without the gate is refused; an upload's files are swept."""
    if HOOKS_CHANGE.search(command):
        return "this command changes git's core.hooksPath, which carries the privacy sweep's pre-push gate", []
    if GIT_PUSH.search(command):
        if BYPASS.search(command):
            return ("a push may not skip or redirect the pre-push gate (--no-verify, core.hooksPath, or a HOME or "
                    "GIT_CONFIG override)"), []
        install(quiet=True)
        want = str(HOOKS)
        if not os.access(HOOKS / "pre-push", os.X_OK):
            return f"the pre-push gate is missing from {want}", []
        for d in _repos_named(command, cwd):
            if not os.path.isdir(d):
                continue
            r = subprocess.run(["git", "-C", d, "config", "--get", "core.hooksPath"], capture_output=True, text=True)
            inside = subprocess.run(["git", "-C", d, "rev-parse", "--is-inside-work-tree"], capture_output=True)
            if inside.returncode == 0 and r.stdout.strip() != want:
                return f"git in {d} doesn't run the pre-push gate (core.hooksPath is not {want})", []
        return None, []
    found = []
    if re.search(r"(?<![\w-])(?:curl|wget|scp|rsync|sftp|rclone)(?![\w-])", command):
        try:
            words = shlex.split(command, posix=True)
        except ValueError:
            words = command.split()
        local = lambda w: os.path.join(cwd, os.path.expanduser(w))
        upload, tool = False, ""
        for i, w in enumerate(words):
            prev = words[i - 1] if i else ""
            if w in ("curl", "wget", "scp", "rsync", "sftp", "rclone") or w.endswith(("/curl", "/wget")):
                tool = w.rsplit("/", 1)[-1]
                continue
            if not tool:
                continue
            if prev in ("-T", "--upload-file", "--post-file") or w.startswith(("--upload-file=", "--post-file=")):
                upload = True
                path = w.split("=", 1)[1] if w.startswith("--") and "=" in w else w
                found += sweep_path(local(path), ctx, path)
            elif tool in ("curl", "wget") and "@" in w and (prev.startswith("-") or w.startswith("-")):
                for m in re.finditer(r"@([^;,\s]+)", w):
                    if os.path.exists(local(m.group(1))):
                        upload = True
                        found += sweep_path(local(m.group(1)), ctx, m.group(1))
            elif prev in ("-d", "--data", "--data-binary", "--data-raw", "--data-urlencode", "-F", "--form",
                          "--json", "--post-data") or w.startswith(("--data", "--form", "--json", "--post-data")):
                upload = True
            elif tool in ("scp", "rsync", "sftp", "rclone") and not w.startswith("-") and ":" not in w \
                    and os.path.exists(local(w)):
                upload = True
                found += sweep_path(local(w), ctx, w)
        if upload or re.search(r"(?i)-X\s*(?:POST|PUT|PATCH)", command):
            found += scan_text(command, "the command", ctx)
    return None, found


def _resolve(p, base):
    p = os.path.expanduser(str(p))
    return p if os.path.isabs(p) else os.path.join(base, p)


def check_tool(tool, ti, cwd, ctx):
    """(what, refusal, findings) for one tool call; what is None when the call sends nothing out."""
    if tool == "Bash":
        refusal, found = check_bash(ti.get("command", ""), cwd, ctx)
        return "command", refusal, found
    found = []
    if tool == "Artifact":
        if (ti.get("action") or "publish") != "publish":
            return None, None, []
        for k in ("title", "description", "label"):
            if ti.get(k):
                found += scan_text(str(ti[k]), f"Artifact {k}", ctx)
        for i, e in enumerate(ti.get("edits") or []):
            found += scan_text(str(e.get("new_str", "")), f"Artifact edit {i + 1}", ctx)
        root = _resolve(ti["root"], cwd) if ti.get("root") else cwd
        paths = [ti["file_path"]] if ti.get("file_path") else []
        paths += list(ti.get("file_paths") or [])
        for p in paths:
            found += sweep_path(_resolve(p, cwd), ctx, str(p))
        files = ti.get("files")
        if isinstance(files, dict):
            for pub, src in files.items():
                found += scan_text(pub, "Artifact published path", ctx, line_numbers=False)
                if isinstance(src, str):
                    found += sweep_path(_resolve(src, root), ctx, src)
                elif isinstance(src, dict) and src.get("from"):
                    found += sweep_path(_resolve(src["from"], root), ctx, src["from"])
        elif isinstance(files, list):
            for e in files:
                if isinstance(e, dict) and e.get("path"):
                    found += sweep_path(_resolve(e["path"], root), ctx, e["path"])
        return "publish", None, found
    if tool == "SendUserFile":
        for p in ti.get("files") or []:
            found += sweep_path(_resolve(p, cwd), ctx, str(p))
        if ti.get("caption"):
            found += scan_text(str(ti["caption"]), "the caption", ctx)
        return "file", None, found
    if tool.startswith("mcp__github__"):
        owner = str(ti.get("owner") or "").lower()
        ctx.external = bool(owner) and owner not in OWN_ACCOUNTS
        return "GitHub call", None, sweep_strings(ti, tool.split("__")[-1], ctx)
    if tool.startswith("mcp__Claude_Docs__"):
        return "Docs write", None, sweep_strings(ti, tool.split("__")[-1], ctx)
    if tool in ("WebFetch", "WebSearch"):
        return "web query", None, sweep_strings(ti, tool, ctx, RULES_QUERY)
    return None, None, []


def _deny(reason):
    print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "permissionDecision": "deny",
                                             "permissionDecisionReason": reason}}))
    return 0


def cmd_hook(_argv):
    try:
        data = json.load(sys.stdin)
        tool = data.get("tool_name", "")
        ti = data.get("tool_input") or {}
        cwd = data.get("cwd") or os.getcwd()
        if not os.environ.get("PRIVSWEEP_SYNCED") and _sync():
            r = subprocess.run([sys.executable, "-I", "-S", str(INSTALL / "privsweep.py"), "hook"],
                               input=json.dumps(data), capture_output=True, text=True,
                               env=dict(os.environ, PRIVSWEEP_SYNCED="1"))
            if r.returncode != 0:
                return _deny(f"Privacy sweep (d414): refused, because its new copy failed ({r.stderr.strip()[-300:]}).")
            sys.stdout.write(r.stdout)
            return 0
        ctx = Ctx()
        what, refusal, found = check_tool(tool, ti, cwd, ctx)
    except SweepError as e:
        return _deny(f"Privacy sweep (d414): refused, because {e}.")
    except Exception as e:  # noqa: BLE001: fail closed on anything unexpected
        return _deny(f"Privacy sweep (d414): refused, because the sweep failed ({type(e).__name__}: {e}).")
    if refusal:
        return _deny(f"Privacy sweep (d414): refused, because {refusal}.")
    if found:
        return _deny(report(found, f"Privacy sweep refused this {what}"))
    return 0


# --- install and the other subcommands ----------------------------------------------------------------------

FILES = ("privsweep.py", "personal.txt", "allow.txt")
# The PreToolUse hook, the same in the project's settings and the user's (Claude Code runs an identical command once)
HOOK_MATCHER = "Bash|Artifact|SendUserFile|WebFetch|WebSearch|mcp__github__.*|mcp__Claude_Docs__.*"
HOOK_COMMAND = ('f="$HOME/.config/privsweep/privsweep.py"; [ -f "$f" ] || '
                'f="$CLAUDE_PROJECT_DIR/tools/privsweep/privsweep.py"; exec python3 -I -S "$f" hook')


def _source():
    """The checkout's copy the installed one follows: named, the project's, or the one recorded at install."""
    recorded = INSTALL / "source"
    for d in (os.environ.get("PRIVSWEEP_SRC"),
              os.environ.get("CLAUDE_PROJECT_DIR") and os.path.join(os.environ["CLAUDE_PROJECT_DIR"], "tools",
                                                                      "privsweep"),
              recorded.exists() and recorded.read_text().strip()):
        if d and (Path(d) / "privsweep.py").exists():
            return Path(d)
    return HERE


def _sync():
    """Bring the installed copy up to the checkout's (the next run uses it); True when anything changed."""
    src, changed = _source(), False
    try:                                              # a half-edited copy is never installed over a working one
        compile((src / "privsweep.py").read_text(encoding="utf-8"), str(src / "privsweep.py"), "exec")
    except (SyntaxError, OSError, ValueError):
        return False
    for name in FILES:
        s, d = src / name, INSTALL / name
        if s.exists() and s.resolve() != d.resolve() and (not d.exists() or d.read_bytes() != s.read_bytes()):
            shutil.copyfile(s, d)
            changed = True
    if src.resolve() != INSTALL.resolve():
        (INSTALL / "source").write_text(str(src.resolve()) + "\n")
    return changed


def claude_settings(path):
    """Put the PreToolUse hook in a Claude Code settings file, keeping everything else in it."""
    path = Path(path)
    text = path.read_text() if path.exists() else ""
    data = json.loads(text) if text.strip() else {}
    pre = data.setdefault("hooks", {}).setdefault("PreToolUse", [])
    pre[:] = [e for e in pre if not any("privsweep" in h.get("command", "") for h in e.get("hooks", []))]
    pre.append({"matcher": HOOK_MATCHER,
                "hooks": [{"type": "command", "command": HOOK_COMMAND, "timeout": 900, "onFailure": "block",
                           "statusMessage": "Privacy sweep"}]})
    new = json.dumps(data, indent=2) + "\n"
    if new != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(new)


def install(scope=None, quiet=False, claude=None):
    """Copy the sweep to ~/.config/privsweep, write the pre-push gate, point git at it and, in a cloud session,
    put the PreToolUse hook in the user's Claude Code settings too."""
    HOOKS.mkdir(parents=True, exist_ok=True)
    _sync()
    hook = HOOKS / "pre-push"
    if not hook.exists() or hook.read_text() != PRE_PUSH:
        hook.write_text(PRE_PUSH)
    hook.chmod(0o755)
    remote = os.environ.get("CLAUDE_CODE_REMOTE") == "true"
    scope = scope or ("global" if remote else "repo")
    want = str(HOOKS)
    if scope == "global":
        cur = subprocess.run(["git", "config", "--global", "--get", "core.hooksPath"], capture_output=True, text=True)
        if cur.stdout.strip() != want:
            subprocess.run(["git", "config", "--global", "core.hooksPath", want], check=True)
    else:
        repo = _source().parent.parent
        subprocess.run(["git", "-C", str(repo), "config", "core.hooksPath", want], check=True)
    if claude if claude is not None else remote:
        claude_settings(Path.home() / ".claude" / "settings.json")
    if not quiet:
        print(f"privsweep: installed in {INSTALL}; git's pre-push gate is {want} ({scope})")
    return 0


def cmd_files(argv):
    ctx = Ctx()
    try:
        found = [f for p in argv for f in sweep_path(p, ctx)]
    except SweepError as e:
        print(f"privsweep: {e}", file=sys.stderr)
        return 1
    if found:
        print(report(found, "privsweep"), file=sys.stderr)
        return 1
    print(f"privsweep: clean ({len(argv)} path{'s' if len(argv) != 1 else ''})")
    return 0


def cmd_text(argv):
    ctx = Ctx(external="--external" in argv)
    where = next((a for a in argv if not a.startswith("--")), "stdin")
    found = scan_text(sys.stdin.read(), where, ctx)
    if found:
        print(report(found, "privsweep"), file=sys.stderr)
        return 1
    print("privsweep: clean")
    return 0


def cmd_repo(argv):
    """Sweep a repository: what its local branches would push (default), HEAD's files (--head) or every object
    (--all)."""
    repo = Path(next(a for a in argv if not a.startswith("--"))).resolve()
    ctx = Ctx()
    try:
        if "--all" in argv:
            found = sweep_objects(repo, ["--all"], [], ctx, repo.name)
        elif "--head" in argv:
            found = sweep_objects(repo, ["--no-walk", "HEAD"], [], ctx, repo.name)
        else:
            found = sweep_objects(repo, ["--branches", "--tags"], ["--remotes"], ctx, repo.name)
    except SweepError as e:
        print(f"privsweep: {e}", file=sys.stderr)
        return 1
    if found:
        print(report(found, f"privsweep: {repo.name}"), file=sys.stderr)
        return 1
    print(f"privsweep: {repo.name} clean")
    return 0


def cmd_hash(_argv):
    """Print the personal.txt line for a value read from stdin (it is never echoed)."""
    p = Personal()
    for word in re.findall(r"[A-Za-z0-9]+", sys.stdin.read()):
        if len(word) >= 3:
            print(f"sha256 {p.digest(word)}")
    return 0


def cmd_selftest(_argv):
    """Synthetic samples only: no real personal value appears here."""
    os.environ["PRIVSWEEP_PRIVATE"] = "zorblax\nquim" + "bly"
    ctx = Ctx()
    ctx.personal.hashes.add(ctx.personal.digest("frobnitzer"))
    c, u, at = ":", "\\", "@"
    hit = {
        "personal": ["Hello Zorblax.", "a quimbly thing", "user FrobNitzer here"],
        "email": ["write to someone" + at + "mailhost.net now"],
        "ip": ["server " + "192.168" + ".7.9:7871", "10.2" + ".3.4", "100.64" + ".0.1"],
        "userpath": ["C" + c + u + "Users" + u + "zorblax2" + u + "x", "/home" + "/someguy/x"],
        "drive": ["E" + c + u + "Games" + u + "Skyrim"],
        "machine": ["DESKTOP-" + "4KJ2HFQ"],
        "endpoint": ["http://nas" + ".lan/x"],
        "secret": ["key sk-ant-" + "api03-aB3dE5fG7hJ9kL1mN3pQ5r", "-----BEGIN " + "RSA PRIVATE KEY-----",
                   "https://u" + c + "p4ssw0rd99" + at + "host/x", 'api_key = "' + 'Zx81kQ02mPq7Lw3nVb6"'],
    }
    clean = ["noreply" + at + "anthropic.com", "256296907+Pt0l3my" + at + "users.noreply.github.com",
             "127.0.0.1:8080", "Windows 10.0.19041.1", "C" + c + u + "Users" + u + "Public" + u + "x",
             "/home" + "/user/Skyrizz", "tagZORBLAXes", "sk-ant-" + "x" * 24, 'password = "${PASSWORD}"',
             "C" + c + u + "Program Files" + u + "x", "DESKTOP-" + "XXXXXXX", "Pt0l3my's repo", "v1.2.3.4",
             "a" + at + "2x.png", "react" + at + "18.3.1"]
    bad = 0
    for rule, samples in hit.items():
        for s in samples:
            got = {f.rule for f in scan_text(s, "t", ctx)}
            if rule not in got:
                print(f"selftest: missed {rule} in a sample of {len(s)} chars")
                bad += 1
    for s in clean:
        got = scan_text(s, "t", ctx)
        if got:
            print(f"selftest: false {got[0].rule} on sample {clean.index(s)}")
            bad += 1
    ext = Ctx(external=True)
    if not any(f.rule == "handle" for f in scan_text("by Pt0l3my", "t", ext)):
        print("selftest: missed a handle in an external write")
        bad += 1
    blob = ("x" * 20 + "C" + c + u + "Users" + u + "zorblax3" + u + "a").encode("utf-16-le") + b"\x00" * 10
    if not any(f.rule == "userpath" for f in sweep_bytes(blob, "bin", ctx)):
        print("selftest: missed a path in a binary's UTF-16 strings")
        bad += 1
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as z:
        z.writestr("inner/readme.txt", "mail someone" + at + "mailhost.net")
    if not any(f.rule == "email" and "inner/readme.txt" in f.where for f in sweep_bytes(buf.getvalue(), "z", ctx)):
        print("selftest: missed an e-mail inside a zip")
        bad += 1
    uri = "data:text/plain;base64," + base64.b64encode(("contact zorblax" + " " * 60).encode()).decode()
    if not any(f.rule == "personal" for f in scan_text(f'<img src="{uri}">', "page", ctx)):
        print("selftest: missed a value inside a data: URI")
        bad += 1
    bad += _selftest_push()
    bad += _selftest_hook()
    print("privsweep selftest: " + ("passed" if not bad else f"{bad} failed"))
    return 1 if bad else 0


def _selftest_push():
    bad = 0
    with tempfile.TemporaryDirectory(prefix="privsweep-test-") as tmp:
        env = dict(os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_NOSYSTEM="1",
                   GIT_AUTHOR_NAME="T", GIT_AUTHOR_EMAIL="noreply@anthropic.com", GIT_COMMITTER_NAME="T",
                   GIT_COMMITTER_EMAIL="noreply@anthropic.com")
        remote, work = Path(tmp) / "r.git", Path(tmp) / "w"
        run = lambda *a, cwd=work, e=env: subprocess.run(a, cwd=cwd, env=e, capture_output=True, check=True)
        subprocess.run(["git", "init", "-q", "--bare", str(remote)], env=env, check=True)
        subprocess.run(["git", "init", "-q", "-b", "main", str(work)], env=env, check=True)
        run("git", "config", "core.hooksPath", str(Path(tmp) / "hooks"))
        hooks = Path(tmp) / "hooks"
        hooks.mkdir()
        (hooks / "pre-push").write_text(PRE_PUSH.replace('"$(dirname "$0")/../privsweep.py"', f'"{Path(__file__)}"'))
        (hooks / "pre-push").chmod(0o755)
        run("git", "remote", "add", "origin", str(remote))
        (work / "a.txt").write_text("clean text\n")
        run("git", "add", ".")
        run("git", "commit", "-q", "--no-gpg-sign", "-m", "Add a clean file")
        env2 = dict(env, PRIVSWEEP_PRIVATE="zorblax")
        if subprocess.run(["git", "push", "-q", "origin", "main"], cwd=work, env=env2, capture_output=True).returncode:
            print("selftest: a clean push was refused")
            bad += 1
        (work / "b.txt").write_text("notes for Zorblax\n")
        run("git", "add", ".")
        run("git", "commit", "-q", "--no-gpg-sign", "-m", "Add notes")
        if not subprocess.run(["git", "push", "-q", "origin", "main"], cwd=work, env=env2,
                              capture_output=True).returncode:
            print("selftest: a push with a personal value went through")
            bad += 1
        run("git", "reset", "-q", "--hard", "HEAD~1")
        env3 = dict(env, GIT_AUTHOR_EMAIL="someone" + "@" + "mailhost.net")
        (work / "c.txt").write_text("clean\n")
        run("git", "add", ".")
        run("git", "commit", "-q", "--no-gpg-sign", "-m", "Add c", e=env3)
        if not subprocess.run(["git", "push", "-q", "origin", "main"], cwd=work, env=env2,
                              capture_output=True).returncode:
            print("selftest: a push with a personal author e-mail went through")
            bad += 1
    return bad


def _selftest_hook():
    bad = 0
    ctx = Ctx()
    cases = [("Bash", {"command": "git push --no-verify origin x"}, True),
             ("Bash", {"command": "git config --global core.hooksPath /dev/null"}, True),
             ("Bash", {"command": "git config --get core.hooksPath"}, False),
             ("Bash", {"command": "ls -la"}, False),
             ("mcp__github__add_issue_comment", {"owner": "someone", "repo": "x", "body": "thanks, Pt0l3my"}, True),
             ("mcp__github__add_issue_comment", {"owner": "Pt0l3my", "repo": "x", "body": "thanks, Pt0l3my"}, False),
             ("WebSearch", {"query": "zorblax skyrim"}, True)]
    for tool, ti, refused in cases:
        ctx.external = False
        _, refusal, found = check_tool(tool, ti, os.getcwd(), ctx)
        if bool(refusal or found) != refused:
            print(f"selftest: hook case {cases.index((tool, ti, refused))} {'passed' if refused else 'refused'}")
            bad += 1
    return bad


def main(argv):
    cmds = {"hook": cmd_hook, "push": cmd_push, "files": cmd_files, "text": cmd_text, "repo": cmd_repo,
            "hash": cmd_hash, "selftest": cmd_selftest,
            "install": lambda a: install(a[a.index("--scope") + 1] if "--scope" in a else None,
                                         claude=True if "--claude" in a else False if "--no-claude" in a else None),
            "settings": lambda a: claude_settings(a[0]) or 0}
    if not argv or argv[0] not in cmds:
        print(__doc__)
        return 2
    return cmds[argv[0]](argv[1:])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
