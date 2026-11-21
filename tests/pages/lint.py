#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tests/pages/lint.py: GraceUI's page lint on EldenNet's space, from the pinned GraceUI (PAGES §2; GraceUI's
eldennet-changes §19 item 2).

    python3 -I tests/pages/lint.py --graceui <a GraceUI checkout> src/en-overlay/pages/space.toml

It runs `tools/pages/build.py --lint` of the GraceUI checkout given, once every file that lint reads is the pinned
commit's, by its SHA-256 (the pins below, GraceUI beca859, the commit third_party/graceui-sys comes from). A checkout at
another commit fails with the file that differs: move the pins with the vendored headers. Standard library only.
"""
import argparse
import hashlib
import pathlib
import subprocess
import sys

COMMIT = "beca859"
PINS = {
    "tools/pages/build.py": "eb7b1bc8bbff3972455b0fe1e49ace3596155886c527c2fdd0bc4a303ac42995",
    "tools/protocol/generate.py": "d5bd23b02647a25e0196a42aa94f7912bbcdabf24ad40aa17c8247d3f88c2607",
    "src/grace-protocol/protocol.toml": "738dad504d2f4151419113e3d7be817d20202f1abe7892f849bb980e83c66f61",
    # The built-in spaces, which the lint reads for the pages other spaces may link to ("graceui/<page>").
    "src/grace-pages/spaces/graceui/space.toml": "14e7f2b1e61bdee67e2ad631dd8ad270e82db1a47a6cf02357bc2d581142aa88",
    "src/grace-pages/spaces/demo/space.toml": "888f81b53d107b2b6eed395cd34bbd7eb7af317c1f63affb842f298c6e59423c",
}


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--graceui", required=True, type=pathlib.Path, help="a GraceUI checkout at the pinned commit")
    parser.add_argument("space", type=pathlib.Path)
    args = parser.parse_args(argv)
    for name, want in PINS.items():
        path = args.graceui / name
        try:
            have = hashlib.sha256(path.read_bytes()).hexdigest()
        except OSError:
            print(f"lint: the GraceUI checkout has no {name}", file=sys.stderr)
            return 1
        if have != want:
            print(f"lint: {name} is not GraceUI {COMMIT}'s (SHA-256 {have})", file=sys.stderr)
            return 1
    done = subprocess.run([sys.executable, "-I", str(args.graceui / "tools" / "pages" / "build.py"), "--lint",
                           str(args.space)], check=False)
    return done.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
