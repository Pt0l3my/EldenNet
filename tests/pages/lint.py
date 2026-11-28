#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tests/pages/lint.py: GraceUI's page lint on EldenNet's space, from the pinned GraceUI (PAGES §2; GraceUI's
eldennet-changes §19 item 2).

    python3 -I tests/pages/lint.py --graceui <a GraceUI checkout> src/en-overlay/pages/space.toml

It runs `tools/pages/build.py --lint` of the GraceUI checkout given, once every file that lint reads is the pinned
commit's, by its SHA-256 (the pins below, GraceUI fc2205a, the commit third_party/graceui-sys comes from). A checkout at
another commit fails with the file that differs: move the pins with the vendored headers. Standard library only.
"""
import argparse
import hashlib
import pathlib
import subprocess
import sys

COMMIT = "fc2205a"
PINS = {
    "tools/pages/build.py": "d89f85e7e2d08d919317ecc12e87b45dacd5b3173ad310a95cf71d209ea9c339",
    "tools/protocol/generate.py": "d5bd23b02647a25e0196a42aa94f7912bbcdabf24ad40aa17c8247d3f88c2607",
    "src/grace-protocol/protocol.toml": "65b9eb0fc02af7901d58581a961f661b100a48db219c8ad2150b147b0516f309",
    # The built-in spaces, which the lint reads for the pages other spaces may link to ("graceui/<page>").
    "src/grace-pages/spaces/graceui/space.toml": "b5b18b65bb41dbfd22f4078bce44095e7108e0d93c23427b8f2248cc5d9847bf",
    "src/grace-pages/spaces/demo/space.toml": "3fc2b77fb154d8ff7c21c8643f9c7557225e08582004775bb84de92c66b7afe3",
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
