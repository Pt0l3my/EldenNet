#!/usr/bin/env python3
"""eldennet-engine's render-prompt --fixture, golden's prompt kind and lexicon, end to end (the first conversation's
part A): the prompt a fixture gives, in the goldens' form and as JSON, a refused render, usage errors; golden check and
update on a copy of a fixture, a golden changed by hand and CHANGES.md's entry; lexicon fingerprint on invented words
only (the fingerprints, their order, a refused line named by its number, the words written nowhere) and lexicon check
on the shipped pack and on a broken one. Every word and line here is invented.

    speech_cli_test.py --engine ENGINE --content PACK --fixtures DIR --work DIR
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

FAILURES = []


def need(cond, text):
    if not cond:
        FAILURES.append(text)


def run(engine, *argv, stdin=None):
    out = subprocess.run([engine] + list(argv), capture_output=True, text=True, check=False, timeout=120,
                         input=stdin)
    return out.returncode, out.stdout, out.stderr


def fingerprint(normalised):
    return hashlib.sha256(("eldennet.safety.v1:" + normalised).encode("utf-8")).hexdigest()


def render_prompt(engine, pack, fixtures):
    fixture = os.path.join(fixtures, "first_meeting.prompt.json")
    code, out, err = run(engine, "render-prompt", "--fixture", fixture, "--content", pack)
    need(code == 0, "render-prompt --fixture: exit %d (%s)" % (code, err.strip()))
    with open(os.path.join(fixtures, "golden", "first_meeting.prompt.txt"), encoding="utf-8") as f:
        need(out == f.read(), "render-prompt --fixture: the output is not the golden's")
    need(out.startswith("# prompt golden: first_meeting (format 1)\n"), "render-prompt --fixture: the header")
    need("[ cache ]" in out and "[ system ]" in out and "[ user ]" in out, "render-prompt --fixture: the markers")

    code, out, err = run(engine, "render-prompt", "--fixture", fixture, "--content", pack, "--json")
    need(code == 0, "render-prompt --fixture --json: exit %d" % code)
    data = json.loads(out)
    need(data["fixture"] == "first_meeting", "--json: the fixture's name")
    need([m["role"] for m in data["messages"]] == ["system", "user"], "--json: the roles")
    need(sum(1 for p in data["messages"][0]["parts"] if p["cache"]) == 3, "--json: three breakpoints")
    need(len(data["blocks"]) == 12 and data["blocks"][5]["block"] == "05b", "--json: the blocks")
    need(data["token_estimate"] > 0 and data["trimmed"]["exchanges"] == 0, "--json: the estimate and the trim")

    code, out, err = run(engine, "render-prompt", "--fixture", os.path.join(fixtures, "over_cap.prompt.json"),
                         "--content", pack)
    need(code == 1, "render-prompt --fixture over_cap: exit %d, not 1" % code)
    need("# refused: OverCap:" in out and "nothing would be sent" in err, "render-prompt over_cap: the refusal")

    for argv in (["--fixture", fixture, "--context", fixture], ["--fixture"], ["--content", pack, "x"]):
        code, _, _ = run(engine, "render-prompt", *argv)
        need(code == 2, "render-prompt %s: exit %d, not 2" % (" ".join(os.path.basename(a) for a in argv), code))
    code, _, err = run(engine, "render-prompt", "--fixture", fixture, "--content", os.path.join(pack, "missing"))
    need(code == 1 and "prompts" in err, "render-prompt with no pack: exit %d" % code)


def golden(engine, pack, fixtures, work):
    for name in ("first_meeting", "long_talk", "crowded_regen", "over_cap"):
        code, out, err = run(engine, "golden", "check", os.path.join(fixtures, name + ".prompt.json"), "--content", pack)
        need(code == 0 and "agree" in out, "golden check %s: exit %d (%s)" % (name, code, err.strip()))
    # A copy, to change by hand.
    dst = os.path.join(work, "golden")
    shutil.rmtree(dst, ignore_errors=True)
    os.makedirs(os.path.join(dst, "golden"))
    shutil.copy(os.path.join(fixtures, "first_meeting.prompt.json"), dst)
    fixture = os.path.join(dst, "first_meeting.prompt.json")
    code, out, err = run(engine, "golden", "check", fixture, "--content", pack)
    need(code == 1 and "golden update first" in err, "golden check with no golden: exit %d" % code)
    code, out, err = run(engine, "golden", "update", fixture, "--content", pack)
    need(code == 0 and "CHANGES.md needs a line naming `first_meeting`" in out, "golden update: exit %d" % code)
    code, out, err = run(engine, "golden", "check", fixture, "--content", pack)
    need(code == 1 and "CHANGES.md has no entry" in err, "golden check with no entry: exit %d" % code)
    _, out, _ = run(engine, "golden", "update", fixture, "--content", pack)
    digest = out.strip().split()[-1]
    with open(os.path.join(dst, "golden", "CHANGES.md"), "w", encoding="utf-8") as f:
        f.write("- `first_meeting` %s: an invented test's.\n" % digest)
    code, out, err = run(engine, "golden", "check", fixture, "--content", pack)
    need(code == 0, "golden check with its entry: exit %d (%s)" % (code, err.strip()))
    golden_file = os.path.join(dst, "golden", "first_meeting.prompt.txt")
    with open(golden_file, encoding="utf-8") as f:
        text = f.read()
    with open(golden_file, "w", encoding="utf-8") as f:
        f.write(text.replace("at dusk", "at noon"))
    code, out, err = run(engine, "golden", "check", fixture, "--content", pack)
    need(code == 1 and "-The Saltmarsh Weir, at noon" in out and "+The Saltmarsh Weir, at dusk" in out,
         "golden check on a changed golden: exit %d, the diff" % code)
    # Without --content, the nearest content/eldenring above the fixture: the repository's, for its own fixtures.
    code, _, _ = run(engine, "golden", "check", os.path.join(fixtures, "first_meeting.prompt.json"))
    need(code == 0, "golden check without --content in the repository: exit %d" % code)
    # A fixture whose block 2 passes its 120 tokens fails, and update writes nothing (WHY§388); invented words.
    with open(fixture, encoding="utf-8") as f:
        data = json.load(f)
    data["record"]["public"]["summary"] = " ".join(["The reeds bend low by the weir, and the water runs grey."] * 6)
    wordy = os.path.join(dst, "wordy.prompt.json")
    with open(wordy, "w", encoding="utf-8") as f:
        json.dump(data, f)
    for verb in ("check", "update"):
        code, _, err = run(engine, "golden", verb, wordy, "--content", pack)
        need(code == 1 and "block 2 comes to about" in err and "past its cap of 120" in err,
             "golden %s on block 2 past its cap: exit %d (%s)" % (verb, code, err.strip()))
    need(not os.path.exists(os.path.join(dst, "golden", "wordy.prompt.txt")), "golden update wrote a golden past block 2")


def lexicon(engine, pack, work):
    words = ["zorblat", "Z0RBL4T", "", "quix  vandle", "one two three four", "...", "zorblat"]
    code, out, err = run(engine, "lexicon", "fingerprint", stdin="\n".join(words) + "\n")
    need(code == 1, "lexicon fingerprint: exit %d, not 1 (two lines refused)" % code)
    want = sorted({fingerprint("zorblat"), fingerprint("quix vandle")})
    need(out.split() == want, "lexicon fingerprint: the fingerprints, sorted and without repeats")
    need("line 5" in err and "line 6" in err and "2 fingerprints" in err, "lexicon fingerprint: the refused lines")
    for w in ("zorblat", "z0rbl4t", "quix", "vandle", "four"):
        need(w not in (out + err).lower(), "lexicon fingerprint: a word is written out")
    code, out, err = run(engine, "lexicon", "fingerprint", "--toml", stdin="zorblat\r\n")
    need(code == 0 and out == '  "%s",\n' % fingerprint("zorblat"), "lexicon fingerprint --toml")
    code, out, err = run(engine, "lexicon", "fingerprint", "--help")
    need(code == 0 and "written nowhere" in out, "lexicon fingerprint --help: the words are written nowhere")
    code, _, _ = run(engine, "lexicon", "fingerprint", "--save")
    need(code == 2, "lexicon fingerprint --save: exit %d, not 2" % code)
    code, out, err = run(engine, "lexicon", "check", "--content", pack)
    need(code == 0 and "lexicon: banned " in out and "promise 12" in out, "lexicon check: exit %d (%s)" % (code, err))
    broken = os.path.join(work, "broken")
    shutil.rmtree(broken, ignore_errors=True)
    os.makedirs(os.path.join(broken, "lexicon"))
    shutil.copy(os.path.join(pack, "lexicon", "banned.toml"), os.path.join(broken, "lexicon"))
    with open(os.path.join(broken, "lexicon", "safety.toml"), "w", encoding="utf-8") as f:
        f.write('format = 1\nalways = ["inventedplainword"]\n')
    code, out, err = run(engine, "lexicon", "check", "--content", broken)
    need(code == 1 and "safety.toml:2: always[0] is not a fingerprint" in err, "lexicon check on a broken list")
    need("inventedplainword" not in err + out, "lexicon check quotes an entry")
    code, _, _ = run(engine, "lexicon")
    need(code == 2, "lexicon with no command: exit %d" % code)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--engine", required=True)
    p.add_argument("--content", required=True)
    p.add_argument("--fixtures", required=True)
    p.add_argument("--work", required=True)
    a = p.parse_args()
    os.makedirs(a.work, exist_ok=True)
    render_prompt(a.engine, a.content, a.fixtures)
    golden(a.engine, a.content, a.fixtures, a.work)
    lexicon(a.engine, a.content, a.work)
    for f in FAILURES:
        print("FAIL: " + f)
    print("speech_cli_test: %d failure%s" % (len(FAILURES), "" if len(FAILURES) == 1 else "s"))
    return 1 if FAILURES else 0


if __name__ == "__main__":
    sys.exit(main())
