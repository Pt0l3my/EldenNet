"""X1 outside the game, end to end through the command (eval_x1.hpp; NPC §12.2; O139 to O142).

`eval --set tests/x1/x1.toml --jobpack` writes the set's jobpacks; each is checked here against LLM Jobrunner 1.4's
own rules for a jobpack/1 file, as its llm_jobrunner.py reads one (parse_jobpack, _items_of, _params, _text_ok and the
default caps), mirrored in our own code below: Jobrunner is never imported or run. Then `eval --set ...
--jobrunner-results` reads our invented sample of Jobrunner's results (tests/x1/sample-results.jsonl, written by us,
never from a real run) and its outputs are checked: what it prints holds no reply, the summary no line of text, and the
refused lines are named by number alone.

Run: python3 -I x1_cli_test.py --engine <eldennet-engine> --set <tests/x1/x1.toml> --sample <sample-results.jsonl>
     --work <folder>
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

# ---- Jobrunner 1.4's jobpack/1 rules, mirrored (its README's "Jobpack format 1" and llm_jobrunner.py) ----

ID_RX = re.compile(r"[A-Za-z0-9_-]{1,64}")
SAMPLING = ("max_tokens", "temperature", "top_p", "top_k", "min_p", "seed", "stop", "thinking", "response_format")
FLOATS = {"temperature": (0.0, 2.0), "top_p": (0.0, 1.0), "min_p": (0.0, 1.0)}
INTS = {"top_k": (0, 200), "seed": (0, 2 ** 32 - 1), "repeat": (1, 1000)}
ITEM_KEYS = {"id", "server", "system", "prompt", "messages", "repeat"} | set(SAMPLING)
HEAD_KEYS = {"format", "name", "description", "server", "defaults", "items"}
CAPS = {"file_mb": 2, "items": 2000, "runs": 5000, "max_tokens": 2048}
DESC_MAX = 600
SERVERS_UP = 8  # "*" sends every item to every server that is up: the runs are checked for this many


def depth(o):
    if isinstance(o, dict):
        return 1 + max((depth(v) for v in o.values()), default=0)
    if isinstance(o, list):
        return 1 + max((depth(v) for v in o), default=0)
    return 0


def strings(o):
    if isinstance(o, str):
        yield o
    elif isinstance(o, dict):
        for k, v in o.items():
            yield k
            yield from strings(v)
    elif isinstance(o, list):
        for v in o:
            yield from strings(v)


def text_ok(o, what, problems):
    if depth(o) > 32:
        problems.append("%s is nested more than 32 deep" % what)
    for s in strings(o):
        try:
            s.encode("utf-8")
        except UnicodeEncodeError:
            problems.append("%s holds an unpaired surrogate" % what)
            return


def num(v, k, whole, lo, hi, problems, where):
    if isinstance(v, bool) or not isinstance(v, (int, float)) or (whole and not isinstance(v, int)):
        problems.append("%s: %s is not a%s number" % (where, k, " whole" if whole else ""))
    elif not lo <= v <= hi:
        problems.append("%s: %s is not %s to %s" % (where, k, lo, hi))


def params(src, where, problems):
    for k in SAMPLING:
        if k not in src:
            continue
        v = src[k]
        if k == "max_tokens":
            num(v, k, True, 1, CAPS["max_tokens"], problems, where)
        elif k in FLOATS:
            num(v, k, False, *FLOATS[k], problems, where)
        elif k in INTS:
            num(v, k, True, *INTS[k], problems, where)
        elif k == "stop":
            st = [v] if isinstance(v, str) else v
            if not isinstance(st, list) or len(st) > 4 or not all(isinstance(x, str) and 0 < len(x) <= 64 for x in st):
                problems.append("%s: stop is not up to 4 strings of up to 64 characters" % where)
        elif k == "thinking":
            if not isinstance(v, bool):
                problems.append("%s: thinking is not true or false" % where)
        elif not isinstance(v, dict) or v.get("type") not in ("json_object", "json_schema"):
            problems.append("%s: response_format is not json_object or json_schema" % where)


def check_jobpack(raw, problems):
    """Jobrunner's checks of a dropped file, and the runs it would make with SERVERS_UP servers; every file-level
    fault and every item's fault is a problem here, and so is anything Jobrunner would ignore and report."""
    if len(raw) > CAPS["file_mb"] << 20:
        problems.append("the file is over %d MB" % CAPS["file_mb"])
    try:
        text = raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        problems.append("the file is not UTF-8 text")
        return 0
    try:
        head = json.loads(text)
    except ValueError:
        problems.append("the file is not one JSON object")
        return 0
    if not isinstance(head, dict) or not isinstance(head.get("items"), list):
        problems.append("a JSON jobpack is one object with its items")
        return 0
    items = head["items"]
    text_ok({k: v for k, v in head.items() if k != "items"}, "the header", problems)
    if head.get("format", "jobpack/1") != "jobpack/1":
        problems.append("the format is not jobpack/1")
    name = head.get("name")
    if not isinstance(name, str) or not name.strip() or len(name) > 100:
        problems.append("the name is not text of 1 to 100 characters")
    desc = head.get("description")
    if desc is not None and (not isinstance(desc, str) or not desc.strip() or len(desc) > DESC_MAX
                             or re.search(r"[\x00-\x09\x0b-\x1f\x7f]", desc)):
        problems.append("the description is not text of 1 to %d characters with newlines alone" % DESC_MAX)
    unknown = sorted(str(k) for k in head if k not in HEAD_KEYS)
    if unknown:
        problems.append("ignored in the header: " + ", ".join(unknown))
    if head.get("server") != "*":
        problems.append('the file\'s server is not "*"')
    dflt = head.get("defaults", {})
    if not isinstance(dflt, dict):
        problems.append("defaults is not an object")
        dflt = {}
    if sorted(k for k in dflt if k not in SAMPLING):
        problems.append("ignored in defaults")
    params(dflt, "defaults", problems)
    if not items:
        problems.append("the file holds no items")
    if len(items) > CAPS["items"]:
        problems.append("%d items: the cap is %d" % (len(items), CAPS["items"]))
    seen, runs = set(), 0
    for n, it in enumerate(items, 1):
        where = "item %d" % n
        if not isinstance(it, dict):
            problems.append(where + ": not an object")
            continue
        if not isinstance(it.get("id"), str) or not ID_RX.fullmatch(it["id"]):
            problems.append(where + ": its id is not 1 to 64 of A-Z a-z 0-9 _ -")
            continue
        if it["id"] in seen:
            problems.append(where + ": its id is used before in this file")
        seen.add(it["id"])
        text_ok(it, where, problems)
        if sorted(k for k in it if k not in ITEM_KEYS):
            problems.append(where + ": holds keys Jobrunner ignores")
        if "server" in it and it["server"] != "*":
            problems.append(where + ": names a server")
        if "messages" in it:
            if "prompt" in it or "system" in it:
                problems.append(where + ": holds messages and a prompt")
            m = it["messages"]
            if not isinstance(m, list) or not 1 <= len(m) <= 64:
                problems.append(where + ": messages is not a list of 1 to 64")
                continue
            for x in m:
                if not isinstance(x, dict) or x.get("role") not in ("system", "user", "assistant"):
                    problems.append(where + ": a message's role is not system, user or assistant")
                elif not isinstance(x.get("content"), str) or not x["content"]:
                    problems.append(where + ": a message's content is not text")
        elif not isinstance(it.get("prompt"), str) or not it["prompt"].strip():
            problems.append(where + ": holds no prompt and no messages")
        params(it, where, problems)
        rep = it.get("repeat", 1)
        num(rep, "repeat", True, *INTS["repeat"], problems, where)
        runs += (rep if isinstance(rep, int) and not isinstance(rep, bool) else 1) * SERVERS_UP
    if runs > CAPS["runs"]:
        problems.append("the file's runs pass the cap of %d with %d servers up" % (CAPS["runs"], SERVERS_UP))
    return runs


def mirror_selftest():
    """The mirror refuses what Jobrunner refuses: a few files of our own, each with one fault."""
    good = {"format": "jobpack/1", "name": "t", "server": "*", "defaults": {"max_tokens": 140},
            "items": [{"id": "a-1", "messages": [{"role": "user", "content": "x"}]}]}
    faults = [
        dict(good, format="jobpack/2"),
        dict(good, items=[{"id": "a.1", "messages": [{"role": "user", "content": "x"}]}]),
        dict(good, items=[{"id": "a", "messages": [{"role": "tool", "content": "x"}]}]),
        dict(good, items=[{"id": "a", "messages": [{"role": "user", "content": "x"}], "repeat": 0}]),
        dict(good, defaults={"max_tokens": 4096}),
        dict(good, defaults={"stop": ["x" * 65]}),
        dict(good, description="a\tb"),
        dict(good, items=[{"id": "a", "prompt": "x"}, {"id": "a", "prompt": "y"}]),
    ]
    p = []
    check_jobpack(json.dumps(good).encode(), p)
    assert not p, p
    for f in faults:
        p = []
        check_jobpack(json.dumps(f).encode(), p)
        assert p, f


# ---- The command, end to end ----

def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", required=True)
    ap.add_argument("--set", required=True)
    ap.add_argument("--sample", required=True)
    ap.add_argument("--work", required=True)
    a = ap.parse_args()
    shutil.rmtree(a.work, ignore_errors=True)
    os.makedirs(a.work)
    failures = []

    def check(ok, what):
        if not ok:
            failures.append(what)

    mirror_selftest()
    packs = os.path.join(a.work, "packs")
    r = run([a.engine, "eval", "--set", a.set, "--jobpack", packs])
    check(r.returncode == 0, "the export exits 0: " + r.stderr.strip())
    names = sorted(os.listdir(packs)) if os.path.isdir(packs) else []
    check(names == ["x1-questions.jobpack.json", "x1-voice-1.jobpack.json", "x1-voice-2.jobpack.json"],
          "the three jobpacks: %s" % names)
    total = 0
    for n in names:
        with open(os.path.join(packs, n), "rb") as f:
            raw = f.read()
        problems = []
        runs = check_jobpack(raw, problems)
        check(not problems, "%s passes Jobrunner's checks: %s" % (n, problems[:3]))
        total += json.loads(raw)["items"].__len__()
        check(runs > 0, n + " has runs")
    check(total == 512, "512 items in all, not %d" % total)
    # The same set exports the same bytes.
    again = os.path.join(a.work, "again")
    run([a.engine, "eval", "--set", a.set, "--jobpack", again])
    for n in names:
        with open(os.path.join(packs, n), "rb") as f1, open(os.path.join(again, n), "rb") as f2:
            check(f1.read() == f2.read(), n + " is the same on a second export")
    # The import of our invented sample: six lines refused by number, nothing of the replies printed.
    out = {k: os.path.join(a.work, k + ".json") for k in ("report", "summary", "questions")}
    r = run([a.engine, "eval", "--set", a.set, "--jobrunner-results", a.sample, "--report", out["report"],
             "--summary", out["summary"], "--questions", out["questions"]])
    check(r.returncode == 1, "the import exits 1, since lines were refused: %d" % r.returncode)
    check("12 results read, 6 refused, 3 models" in r.stdout, "the import's counts: " + r.stdout[:200])
    with open(a.sample, encoding="utf-8") as f:
        sample = [json.loads(l) for l in f if l.strip() and l.strip().endswith("}")]
    for line in sample:
        reply = line.get("reply", "")
        if len(reply) > 12:
            check(reply[:20] not in r.stdout and reply[:20] not in r.stderr, "no reply in what the import prints")
    with open(out["summary"], encoding="utf-8") as f:
        summary_text = f.read()
    summary = json.loads(summary_text)
    check(summary["kind"] == "x1-summary", "the summary's kind")
    for line in sample:
        reply = line.get("reply", "")
        if len(reply) > 12:
            check(reply[:20] not in summary_text, "no reply in the summary")
    check("example.invalid" not in summary_text and "my server" not in summary_text, "no raw name in the summary")
    with open(out["questions"], encoding="utf-8") as f:
        questions = json.load(f)
    check(questions["kind"] == "x1-questions" and len(questions["replies"]) == 2, "the test questions' file")
    with open(out["report"], encoding="utf-8") as f:
        report = json.load(f)
    check(report["kind"] == "x1-local" and len(report["results"]) == 12, "the local report")
    # Usage errors.
    check(run([a.engine, "eval", "--set", a.set]).returncode == 2, "--set alone is a usage error")
    check(run([a.engine, "eval", "--set", a.set, "--jobpack", packs, "--summary", out["summary"]]).returncode == 2,
          "--jobpack with an import's output is a usage error")
    if failures:
        for f in failures:
            print("FAIL: " + f)
        return 1
    print("x1 end to end: %d jobpacks checked, the sample imported" % len(names))
    return 0


if __name__ == "__main__":
    sys.exit(main())
