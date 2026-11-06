#!/usr/bin/env python3
"""wp: look things up in EldenNet's documents without reading them whole (tools/workplan/README.md).

  index                   build the section index (data/index.json; generated, not committed)
  show CODE...            print sections: wp show DESIGN§5, wp show SN04§5 --all, wp show ER05 --list
  refs CODE...            every section that cites a code, with sizes: wp refs ER05§8
  find TERM...            sections matching all terms: in:docs/design, text:regex, a code, a title word
  grep REGEX [TERM...]    matching sentences with their section and line: a sweep's cheap first pass
  stat                    the largest files and sections

Python 3, no dependencies. Run from anywhere: python3 tools/workplan/wp.py ...
"""
import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from wplib import index  # noqa: E402


def main(argv=None):
    ap = argparse.ArgumentParser(prog="wp", description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("index"); s.add_argument("--quiet", action="store_true")
    s = sub.add_parser("show"); s.add_argument("codes", nargs="+"); s.add_argument("--all", action="store_true")
    s.add_argument("--list", action="store_true", help="sizes only, no text")
    s.add_argument("--max-tokens", type=int, help="stop printing sections past this many tokens")
    s = sub.add_parser("refs"); s.add_argument("codes", nargs="+")
    s = sub.add_parser("find"); s.add_argument("terms", nargs="+")
    s = sub.add_parser("grep"); s.add_argument("regex"); s.add_argument("terms", nargs="*")
    s.add_argument("--width", type=int, default=220)
    s = sub.add_parser("stat"); s.add_argument("--top", type=int, default=15)
    a = ap.parse_args(argv)
    {
        "index": lambda: index.build(quiet=a.quiet), "show": lambda: index.cmd_show(a),
        "refs": lambda: index.cmd_refs(a), "find": lambda: index.cmd_find(a),
        "grep": lambda: index.cmd_grep(a), "stat": lambda: index.cmd_stat(a),
    }[a.cmd]()


if __name__ == "__main__":
    main()
