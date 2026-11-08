#!/usr/bin/env python3
"""Dry run: look at the shape of Smithbox's exports without showing any game text.

Run on the owner's own PC against exports made from their own install (ER08 §4.2, O55, O56). It reads an
NpcParam CSV (and optionally a TalkParam CSV) and one or more text JSON exports (Smithbox's "Export Selected
Container" for item, item_dlc01 and item_dlc02), and prints a report of counts and checks only: column counts,
whether the columns our tools need are there, row counts, how many rows carry a name number, how many of those
resolve to a name, split by the text wrapper that holds it. It prints no names, no game text, no field values
beyond counts and id ranges, and no paths (file names only), and it refuses to print a line that contains any
text from the exports.

Standard library only; Python 3.8 or later. Nothing is written unless --out is given.

    python smithbox_inspect.py --npcparam NpcParam.csv --text item.json item_dlc01.json item_dlc02.json
    python smithbox_inspect.py --npcparam NpcParam.csv --talkparam TalkParam.csv --text item*.json --out report.txt
"""

import argparse
import csv
import datetime
import hashlib
import json
import pathlib
import re
import statistics
import sys

# Leads, not facts (ER01 §8.2, ER08 §4.1): the FMG ids that are thought to hold NPC names and place names in the
# base game and the two expansion containers. The report shows every wrapper it finds, so a wrong lead shows up.
NPC_NAME_WRAPPERS = {18: "base", 328: "dlc01", 428: "dlc02"}
PLACE_NAME_WRAPPERS = {19: "base", 329: "dlc01", 429: "dlc02"}
MIN_GUARD_LEN = 3  # texts at least this long may never appear in the report


class Report:
    def __init__(self):
        self.lines = []

    def add(self, line=""):
        self.lines.append(str(line))

    def guarded(self, texts):
        """The report's lines, after checking that none contains a text from the exports (as a whole word where
        the text starts and ends with a letter or digit, so that a short name inside a longer word is no hit)."""
        bad = set()
        for i, line in enumerate(self.lines):
            for t in texts:
                if t not in line:
                    continue
                left = r"\b" if t[0].isalnum() else ""
                right = r"\b" if t[-1].isalnum() else ""
                if re.search(left + re.escape(t) + right, line):
                    bad.add(i + 1)
        if bad:
            raise SystemExit("refusing to print: report line(s) " + ", ".join(map(str, sorted(bad))) +
                             " would contain export text; nothing was printed")
        return "\n".join(self.lines) + "\n"


def file_facts(path):
    data = path.read_bytes()
    return {"file": path.name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()[:16]}


def sniff_separator(header_line):
    counts = {sep: header_line.count(sep) for sep in (",", ";", "|", "\t")}
    return max(counts, key=counts.get)


def read_param_csv(path, needed):
    """Rows of a Smithbox param CSV as (id, {needed field: int or None}), with checks and counts."""
    text = path.read_text(encoding="utf-8-sig")
    lines = text.splitlines()
    if not lines:
        return {"error": "empty file"}
    sep = sniff_separator(lines[0])
    reader = csv.reader(lines, delimiter=sep)
    header = next(reader)
    out = {
        "separator": {"\t": "tab"}.get(sep, sep),
        "columns": len(header),
        "header_sha256": hashlib.sha256("\x1f".join(header).encode("utf-8")).hexdigest()[:16],
        "first_two_columns": header[:2] == ["ID", "Name"],
        "has": {name: name in header for name in needed},
        "rows": 0, "bad_width": 0, "bad_id": 0, "duplicate_id": 0, "name_label_set": 0,
        "bad_value": {name: 0 for name in needed},
        "records": [],
    }
    index = {name: header.index(name) for name in needed if name in header}
    seen = set()
    for row in reader:
        if not row:
            continue
        out["rows"] += 1
        if len(row) != len(header):
            out["bad_width"] += 1
            continue
        try:
            row_id = int(row[0])
        except ValueError:
            out["bad_id"] += 1
            continue
        if row_id in seen:
            out["duplicate_id"] += 1
        seen.add(row_id)
        if len(row) > 1 and row[1] not in ("", "null"):
            out["name_label_set"] += 1  # Smithbox's own label: counted, never shown (ER08 §3.1)
        values = {}
        for name, col in index.items():
            try:
                values[name] = int(row[col])
            except ValueError:
                out["bad_value"][name] += 1
                values[name] = None
        out["records"].append((row_id, values))
    ids = [r for r, _ in out["records"]]
    out["id_min"], out["id_max"] = (min(ids), max(ids)) if ids else (None, None)
    return out


def read_text_json(path):
    """The wrappers of one Smithbox text export: {wrapper id: (wrapper name, {entry id: text})}."""
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    wrappers = {}
    for w in data.get("FmgWrappers", []):
        entries = {}
        for e in (w.get("Fmg") or {}).get("Entries", []) or []:
            if isinstance(e.get("ID"), int):
                entries[e["ID"]] = e.get("Text")
        wrappers[w.get("ID")] = (str(w.get("Name")), entries)
    meta = {"container": str(data.get("SourceContainer") or data.get("Name") or "?"),
            "has_source_container": "SourceContainer" in data}
    return meta, wrappers


def distribution(values):
    if not values:
        return "none"
    return f"min {min(values)}, median {statistics.median(values):g}, max {max(values)}"


def inspect(npc_path, talk_path, text_paths):
    rep = Report()
    guard = set()
    rep.add("EldenNet: Smithbox export dry run (numbers only; no names, no game text, no paths)")
    rep.add(f"date: {datetime.date.today().isoformat()}; python {sys.version.split()[0]}")
    rep.add()

    # Text first, so every text is in the guard before anything is printed.
    names = {}      # NPC name entry id -> the wrapper ids that hold a text for it
    npc_texts = {}  # NPC name entry id -> its text (kept in memory only, for counts and the guard)
    rep.add("== Text exports")
    for path in text_paths:
        meta, wrappers = read_text_json(path)
        f = file_facts(path)
        rep.add(f"- {f['file']}: {f['bytes']} bytes, sha256 {f['sha256']}, container {meta['container']!r}, "
                f"SourceContainer field {'present' if meta['has_source_container'] else 'missing'}")
        for wid in sorted(wrappers, key=lambda x: (x is None, x)):
            wname, entries = wrappers[wid]
            with_text = sum(1 for t in entries.values() if t)
            tag = ""
            if wid in NPC_NAME_WRAPPERS:
                tag = f"  <- NPC names ({NPC_NAME_WRAPPERS[wid]}), by lead"
            elif wid in PLACE_NAME_WRAPPERS:
                tag = f"  <- place names ({PLACE_NAME_WRAPPERS[wid]}), by lead"
            rep.add(f"    wrapper {wid} {wname!r}: {len(entries)} entries, {with_text} with text{tag}")
            for eid, t in entries.items():
                if t and len(t) >= MIN_GUARD_LEN:
                    guard.add(t)
                if wid in NPC_NAME_WRAPPERS and t:
                    names.setdefault(eid, set()).add(wid)
                    npc_texts[eid] = t
    distinct_texts = len(set(npc_texts.values()))
    rep.add(f"  NPC name numbers with text: {len(names)}; distinct name texts among them: {distinct_texts}")
    rep.add()

    rep.add("== NpcParam")
    f = file_facts(npc_path)
    npc = read_param_csv(npc_path, ["nameId"])
    rep.add(f"- {f['file']}: {f['bytes']} bytes, sha256 {f['sha256']}")
    if "error" in npc:
        rep.add(f"  error: {npc['error']}")
    else:
        rep.add(f"  separator {npc['separator']!r}; {npc['columns']} columns (header sha256 {npc['header_sha256']}); "
                f"first two columns ID, Name: {'yes' if npc['first_two_columns'] else 'NO'}; "
                f"nameId column: {'yes' if npc['has']['nameId'] else 'NO'}")
        rep.add(f"  rows {npc['rows']}; wrong width {npc['bad_width']}; bad id {npc['bad_id']}; "
                f"duplicate id {npc['duplicate_id']}; ids {npc['id_min']}..{npc['id_max']}; "
                f"rows with a Smithbox label {npc['name_label_set']} (never shown)")
        if npc["has"]["nameId"]:
            name_ids = [v["nameId"] for _, v in npc["records"] if v.get("nameId") is not None]
            positive = [n for n in name_ids if n >= 0]
            resolved = [n for n in positive if n in names]
            rep.add(f"  nameId: non-numeric {npc['bad_value']['nameId']}; negative (no name) "
                    f"{sum(1 for n in name_ids if n < 0)}; zero or more {len(positive)}; of those, resolving to an "
                    f"NPC name {len(resolved)} rows")
            per_name = {}
            for n in resolved:
                per_name[n] = per_name.get(n, 0) + 1
            rep.add(f"  distinct name numbers used by rows {len(set(positive))}; resolving {len(per_name)}; "
                    f"not resolving {len(set(positive) - set(per_name))}")
            rep.add(f"  rows per resolved name number: {distribution(list(per_name.values()))}")
            by_wrapper = {}
            for n in per_name:
                key = "+".join(NPC_NAME_WRAPPERS[w] for w in sorted(names[n]))
                by_wrapper[key] = by_wrapper.get(key, 0) + 1
            rep.add("  resolved name numbers by where their text is: " +
                    (", ".join(f"{k} {v}" for k, v in sorted(by_wrapper.items())) or "none"))
            texts_used = {npc_texts[n] for n in per_name if n in npc_texts}
            rep.add(f"  distinct name texts behind the resolved numbers: {len(texts_used)} "
                    f"(a rough upper bound on named characters, before review)")
    rep.add()

    if talk_path:
        rep.add("== TalkParam")
        f = file_facts(talk_path)
        talk = read_param_csv(talk_path, ["msgId"])
        rep.add(f"- {f['file']}: {f['bytes']} bytes, sha256 {f['sha256']}")
        if "error" in talk:
            rep.add(f"  error: {talk['error']}")
        else:
            rep.add(f"  separator {talk['separator']!r}; {talk['columns']} columns (header sha256 "
                    f"{talk['header_sha256']}); msgId column: {'yes' if talk['has']['msgId'] else 'NO'}")
            msg = [v["msgId"] for _, v in talk["records"] if v.get("msgId") is not None]
            rep.add(f"  rows {talk['rows']}; wrong width {talk['bad_width']}; bad id {talk['bad_id']}; "
                    f"duplicate id {talk['duplicate_id']}; msgId zero or more {sum(1 for m in msg if m >= 0)}")
        rep.add()

    rep.add("== Guard")
    rep.add(f"  texts held back from this report: {len(guard)}; checked: no line of this report contains one")
    return rep.guarded(guard)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--npcparam", type=pathlib.Path, required=True, help="Smithbox's NpcParam CSV export")
    ap.add_argument("--talkparam", type=pathlib.Path, help="Smithbox's TalkParam CSV export (optional)")
    ap.add_argument("--text", type=pathlib.Path, nargs="+", required=True,
                    help="Smithbox's text JSON exports (item, item_dlc01, item_dlc02)")
    ap.add_argument("--out", type=pathlib.Path, help="also write the report to this file")
    args = ap.parse_args(argv)
    report = inspect(args.npcparam, args.talkparam, args.text)
    sys.stdout.write(report)
    if args.out:
        args.out.write_text(report, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
