"""Tests for smithbox_inspect.py, on invented exports in the shape ER08 §4.2 describes (no game data).

    python3 -I -m unittest discover -s tools/gamedata
"""

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import smithbox_inspect as si  # noqa: E402

INVENTED = ["Odrel the Lampwright", "Sister Vey", "Old Cobb", "Ash"]


def text_export(container, wrappers):
    return {"Name": container, "SourceContainer": container, "SourceCategory": "Item",
            "FmgWrappers": [{"Name": name, "ID": wid, "Fmg": {"Name": name, "Entries": [
                {"ID": eid, "Text": text} for eid, text in entries], "Version": 2, "BigEndian": False,
                "Unicode": True}} for wid, name, entries in wrappers]}


class InspectTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        d = pathlib.Path(self.tmp.name)
        self.npc = d / "NpcParam.csv"
        self.npc.write_text(
            "ID,Name,nameId,teamType,other\n"
            "1000,Odrel the Lampwright,900100,1,0\n"
            "1001,Odrel the Lampwright (hostile),900100,2,0\n"
            "1002,null,900101,1,0\n"
            "1003,,-1,1,0\n"
            "1004,Unused,900999,1,0\n"
            "1005,Short row,900102\n"
            "1006,Hash Ash,900103,1,0\n", encoding="utf-8")
        self.talk = d / "TalkParam.csv"
        self.talk.write_text("ID,Name,msgId,x\n10,null,5000,0\n11,null,-1,0\n", encoding="utf-8")
        self.base = d / "item.json"
        self.base.write_text(json.dumps(text_export("item", [
            (18, "NpcName", [(900100, INVENTED[0]), (900101, INVENTED[1]), (900102, None)]),
            (19, "PlaceName", [(7000, "Greywater Ford")]),
        ])), encoding="utf-8")
        self.dlc = d / "item_dlc01.json"
        self.dlc.write_text(json.dumps(text_export("item_dlc01", [
            (328, "NpcName_dlc01", [(900103, INVENTED[3]), (900104, INVENTED[2])]),
        ])), encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def run_main(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = si.main(list(args))
        return code, out.getvalue()

    def test_counts(self):
        code, out = self.run_main("--npcparam", str(self.npc), "--talkparam", str(self.talk),
                                  "--text", str(self.base), str(self.dlc))
        self.assertEqual(code, 0)
        self.assertIn("rows 7; wrong width 1;", out)
        self.assertIn("first two columns ID, Name: yes", out)
        self.assertIn("nameId column: yes", out)
        self.assertIn("negative (no name) 1; zero or more 5; of those, resolving to an NPC name 4 rows", out)
        self.assertIn("distinct name numbers used by rows 4; resolving 3; not resolving 1", out)
        self.assertIn("resolved name numbers by where their text is: base 2, dlc01 1", out)
        self.assertIn("wrapper 18 'NpcName': 3 entries, 2 with text  <- NPC names (base), by lead", out)
        self.assertIn("wrapper 19 'PlaceName': 1 entries, 1 with text  <- place names (base), by lead", out)
        self.assertIn("msgId column: yes", out)
        self.assertIn("msgId zero or more 1", out)

    def test_no_name_text_or_path_is_printed(self):
        _, out = self.run_main("--npcparam", str(self.npc), "--talkparam", str(self.talk),
                               "--text", str(self.base), str(self.dlc))
        for name in INVENTED[:3] + ["Greywater Ford", "Lampwright", "hostile", "Unused"]:
            self.assertNotIn(name, out)
        self.assertNotIn(self.tmp.name, out)
        self.assertIn("sha256", out)

    def test_short_text_inside_a_word_is_no_hit(self):
        # "Ash" is an export text, and "hash" is not a hit for it; the report prints "sha256" and our own words.
        code, out = self.run_main("--npcparam", str(self.npc), "--text", str(self.base), str(self.dlc))
        self.assertEqual(code, 0)

    def test_the_guard_refuses_a_line_with_export_text(self):
        rep = si.Report()
        rep.add("fine line")
        rep.add("a line naming Sister Vey")
        with self.assertRaises(SystemExit) as cm:
            rep.guarded({"Sister Vey"})
        self.assertIn("line(s) 2", str(cm.exception))
        self.assertNotIn("Sister Vey", str(cm.exception))

    def test_semicolon_separator_and_bom(self):
        self.npc.write_text("﻿ID;Name;nameId\n1;null;900100\n", encoding="utf-8")
        _, out = self.run_main("--npcparam", str(self.npc), "--text", str(self.base))
        self.assertIn("separator ';'", out)
        self.assertIn("rows 1;", out)

    def test_missing_name_column_is_reported(self):
        self.npc.write_text("ID,Name,teamType\n1,null,1\n", encoding="utf-8")
        _, out = self.run_main("--npcparam", str(self.npc), "--text", str(self.base))
        self.assertIn("nameId column: NO", out)

    def test_out_file(self):
        target = pathlib.Path(self.tmp.name) / "report.txt"
        _, out = self.run_main("--npcparam", str(self.npc), "--text", str(self.base), "--out", str(target))
        self.assertEqual(target.read_text(encoding="utf-8"), out)


if __name__ == "__main__":
    unittest.main()
