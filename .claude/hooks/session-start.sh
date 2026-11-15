#!/bin/bash
# Every session: install the privacy and safety sweep's gates: git's pre-push gate, and in a cloud session the user's
# PreToolUse hook too. The sweep lives only in the owner's private Skyrizz repository (owner, 2026-11-15), so it is
# installed from that checkout; until it is installed, the project's hook refuses every tool call it covers.
# Then warm the docs section index (tools/workplan/README.md), which rebuilds itself whenever a document changes.
set -euo pipefail

src=""
for d in "$CLAUDE_PROJECT_DIR/../skyrizz/tools/privsweep" "$CLAUDE_PROJECT_DIR/skyrizz/tools/privsweep"; do
  if [ -f "$d/privsweep.py" ]; then src="$d"; break; fi
done
if [ -n "$src" ]; then
  PRIVSWEEP_SRC="$src" python3 -I -S "$src/privsweep.py" install \
    || echo "privsweep: not installed; run python3 -I tools/privsweep/privsweep.py install in the Skyrizz checkout" >&2
elif [ -f "$HOME/.config/privsweep/privsweep.py" ]; then
  python3 -I -S "$HOME/.config/privsweep/privsweep.py" install || true
else
  echo "privsweep: not installed - attach the owner's private Skyrizz repository to this session" >&2
fi

cd "$CLAUDE_PROJECT_DIR"
if [ -f tools/workplan/wp.py ]; then
  python3 -I tools/workplan/wp.py index --quiet || echo "workplan index not built; wp will build it on first use" >&2
fi
