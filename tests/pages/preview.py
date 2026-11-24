#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tests/pages/preview.py: EldenNet's pages in grace-dev, GraceUI's fake host (GraceUI's eldennet-changes §26).

    python3 -I tests/pages/preview.py --grace-dev <grace-dev> --pages src/en-overlay/pages --fixtures tests/pages \
        --work <a folder of its own>

For each fixture state (tests/pages/<state>/topics.json, the bodies en_overlay_tests builds from the state's invented
facts), a space folder of pages.json, settings.json and that topics.json, which grace-dev checks as register_pages
does; then grace-dev serves it on loopback, and a browser of our own (browser.py) joins: the welcome names EldenNet's
space at the slice's increment, every topic watched comes back as the fixture holds it, and a settings command is
answered by the settings description's rules (one accepted, one refused). Then each page's in-game structure
(`grace-dev --dump eldennet <page> --size 1920x1080`) is compared with tests/pages/golden/<page>-1920x1080.txt
(EN_UPDATE_GOLDENS=1 writes them anew). Exit 0 when everything matches.
"""
import argparse
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import browser  # noqa: E402

# Each page's golden, dumped over the fixture state named with it (since GraceUI 0.2.6, D184, the dump shows the
# components with that state's topics): the title screen, where the fake game's pages case runs, and for the AI pages
# a state with a provider set up and a self-test run.
# A page dumped over a second state has its golden named for it too (ai.selftest.banner and home.banner: greyed at L1,
# GraceUI 0.2.7's space.state, D188; ai.providers.world: a key refused, GraceUI 0.2.9's D197; ai.models.world: the
# cards' status on a provider cooling down, GraceUI 0.2.12's D202).
# Characters › Named NPCs over the title (the first row chosen: a character with no notes of the player's), over the world
# (the draft blocked) and over the starting state (no content loaded yet: the list's own words, GraceUI D215).
# Content packs (PAGES §4.6.9) over the title (the start's load: a layer of the player's refused alone, a persona and a
# card turned off), over the world (a reload refused at a line, the other files waiting, one left out) and over the
# starting state (nothing loaded yet: each section's own words).
# Docs (PAGES §4.6.9) over the title (its list and the first document, Getting started, chosen: GraceUI D251's reader);
# Tools › Bug report (PAGES §4.6.8) over the title (nothing typed: the five parts) and over the world (the player's
# words of the fixture's own, and the content left out: drawn in the muted ink, its fields hidden).
PAGES = (("home", "title"), ("system.status", "title"), ("system.logs", "title"), ("settings.developer", "title"),
         ("ai.providers", "title"), ("ai.models", "title"), ("ai.selftest", "title"), ("ai.selftest", "banner"),
         ("ai.providers", "starting"), ("home", "banner"), ("ai.providers", "world"), ("ai.models", "world"),
         ("characters.named", "title"), ("characters.named", "world"), ("characters.named", "starting"),
         ("content", "title"), ("content", "world"), ("content", "starting"), ("docs", "title"),
         ("tools.report", "title"), ("tools.report", "world"))
SIZE = "1920x1080"
INCREMENT = "0.2"  # en-overlay's kIncrement: the AI pages'


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def collect(stream, into):
    for line in stream:
        into.append(line)


def serve(grace_dev, folder, state, topics, failures):
    port = free_port()
    process = subprocess.Popen([grace_dev, "--space-dir", str(folder), "--no-scenario", "--port", str(port),
                                "--increment", INCREMENT],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")
    out, err = [], []
    readers = [threading.Thread(target=collect, args=(process.stdout, out), daemon=True),
               threading.Thread(target=collect, args=(process.stderr, err), daemon=True)]
    for reader in readers:
        reader.start()
    view = None
    try:
        deadline = time.monotonic() + 15
        while not any(line.startswith("Ready: http://127.0.0.1:%d/" % port) for line in out):
            if process.poll() is not None or time.monotonic() > deadline:
                failures.append("%s: grace-dev did not take the space:\n%s%s" % (state, "".join(out), "".join(err)))
                return
            time.sleep(0.05)
        view = browser.Browser(port)
        spaces = {s["id"]: s for s in view.welcome.get("spaces", [])}
        if spaces.get("eldennet", {}).get("increment") != INCREMENT:
            failures.append("%s: the welcome names no eldennet space at %s: %s" % (state, INCREMENT, sorted(spaces)))
        for topic, want in topics.items():
            # GraceUI's server counts every message of a connection, a sub too, against 20 a second (its §6.8) and drops
            # the rest unread: with the AI pages' topics there are more than 20, so a sub and its unsub go no faster
            # than 16 messages a second. Each topic is let go once read: a connection watches 32 at most (its limits'
            # `subscriptions`), and the fixtures hold more since Docs and Bug report.
            time.sleep(1 / 8)
            view.sub("eldennet", topic)
            got = view.state("eldennet", topic)
            have = got.get("body") if got["t"] == "state" else [
                {k: v for k, v in row.items() if k != "seq"} for row in got.get("rows", [])]
            if got["t"] == "rows":
                want = [{k: v for k, v in row.items() if k != "seq"} for row in want]
            if have != want:
                failures.append("%s: %s came back otherwise than its fixture" % (state, topic))
            view.unsub("eldennet", topic)
        view.sub("eldennet", "settings")
        settings = view.state("eldennet", "settings")["body"]
        if settings.get("values", {}).get("debug.log_level") != "info":
            failures.append("%s: the settings' values are not their defaults" % state)
        ok = view.command("eldennet", "settings.set", {"key": "debug.log_level", "value": "debug"})
        refused = view.command("eldennet", "settings.set", {"key": "debug.log_level", "value": "loud"})
        if ok.get("ok") is not True or refused.get("error") != "invalid_value":
            failures.append("%s: settings.set answered %s and %s" % (state, ok, refused))
    except (browser.BrowserError, OSError, ValueError, KeyError) as e:
        failures.append("%s: the browser: %r\n%s%s" % (state, e, "".join(out), "".join(err)))
    finally:
        if view is not None:
            view.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for reader in readers:
            reader.join(timeout=5)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--grace-dev", required=True)
    parser.add_argument("--pages", required=True, type=pathlib.Path, help="the folder of pages.json and settings.json")
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    parser.add_argument("--work", required=True, type=pathlib.Path)
    args = parser.parse_args(argv)
    failures = []
    states = sorted(p.parent.name for p in args.fixtures.glob("*/topics.json"))
    if not states:
        print("preview: no fixture states", file=sys.stderr)
        return 1
    shutil.rmtree(args.work, ignore_errors=True)
    for state in states:
        folder = args.work / state
        folder.mkdir(parents=True)
        for name in ("pages.json", "settings.json"):
            shutil.copyfile(args.pages / name, folder / name)
        shutil.copyfile(args.fixtures / state / "topics.json", folder / "topics.json")
        topics = json.loads((folder / "topics.json").read_text(encoding="utf-8"))
        serve(args.grace_dev, folder, state, topics, failures)
    # The in-game structure of each page, as GraceUI's menu lays it out (grace-dev --dump: the fake face, settings at
    # their defaults, the components with the state's topics).
    update = os.environ.get("EN_UPDATE_GOLDENS") is not None
    goldens = args.fixtures / "golden"
    goldens.mkdir(exist_ok=True)
    for page, state in PAGES:
        if state not in states:
            failures.append("--dump %s: no fixture state %s" % (page, state))
            continue
        done = subprocess.run([args.grace_dev, "--dump", "eldennet", page, "--size", SIZE, "--space-dir",
                               str(args.work / state)], capture_output=True, text=True, encoding="utf-8")
        if done.returncode != 0:
            failures.append("--dump %s: exit %d: %s" % (page, done.returncode, done.stderr.strip()))
            continue
        first = next(s for p, s in PAGES if p == page)
        golden = goldens / ("%s-%s.txt" % (page if state == first else "%s.%s" % (page, state), SIZE))
        if update:
            golden.write_text(done.stdout, encoding="utf-8")
        if not golden.is_file() or golden.read_text(encoding="utf-8") != done.stdout:
            failures.append("--dump %s differs from tests/pages/golden/%s (EN_UPDATE_GOLDENS=1 writes it)" %
                            (page, golden.name))
    for f in failures:
        print("preview: " + f, file=sys.stderr)
    if not failures:
        print("preview: %d states served, %d pages match their goldens" % (len(states), len(PAGES)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
