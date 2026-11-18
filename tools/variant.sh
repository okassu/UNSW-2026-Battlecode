#!/bin/sh
# Usage: tools/variant.sh NAME BINARY [KEY=VALUE ...]
# Writes snapshots/var/NAME, an executable bot that runs BINARY with the given
# tuning environment variables (see param() in mybot/main.cpp).
name=$1; bin=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); shift 2
mkdir -p snapshots/var; out=snapshots/var/$name
{ echo '#!/bin/sh'; for kv in "$@"; do echo "export $kv"; done; echo "exec $bin"; } > "$out"
chmod +x "$out"
echo "$out"
