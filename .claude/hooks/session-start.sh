#!/bin/bash
# Every session: install the privacy and safety sweep's gates (tools/privsweep/README.md): git's pre-push gate,
# and in a cloud session the user's PreToolUse hook too. Until it is installed, the project's hook refuses pushes.
# Then warm the docs section index (tools/workplan/README.md), which rebuilds itself whenever a document changes.
set -euo pipefail

python3 -I -S "$CLAUDE_PROJECT_DIR/tools/privsweep/privsweep.py" install \
  || echo "privsweep: not installed; run python3 -I tools/privsweep/privsweep.py install" >&2

cd "$CLAUDE_PROJECT_DIR"
if [ -f tools/workplan/wp.py ]; then
  python3 -I tools/workplan/wp.py index --quiet || echo "workplan index not built; wp will build it on first use" >&2
fi
