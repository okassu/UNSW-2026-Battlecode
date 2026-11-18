#!/bin/sh
# Print the python that has the unswbc package (gate.py, bench.py and runlog.py import it).
# Order: $UNSWBC_PY, uv's tool environment (Linux/WSL/macOS), then the interpreter behind the `unswbc` launcher.
if [ -n "$UNSWBC_PY" ]; then echo "$UNSWBC_PY"; exit 0; fi
for p in "$HOME/.local/share/uv/tools/unswbc/bin/python" "${XDG_DATA_HOME:-$HOME/.local/share}/uv/tools/unswbc/bin/python"; do
  [ -x "$p" ] && { echo "$p"; exit 0; }
done
if command -v unswbc >/dev/null 2>&1; then
  p=$(head -1 "$(command -v unswbc)" | sed 's/^#!//')
  [ -x "$p" ] && { echo "$p"; exit 0; }
fi
echo "unswbc's python not found: install with 'uv tool install unswbc==1.2.2' or set UNSWBC_PY" >&2
exit 1
