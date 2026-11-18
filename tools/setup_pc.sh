#!/bin/sh
# Rebuild everything machine-specific after copying the repo to a new computer (Linux, WSL on Windows, or macOS).
#   1. install a C++ compiler (g++/clang), python3 and uv, then: uv tool install unswbc==1.2.2
#      (the version tested with; `unswbc` prints what's missing)
#   2. sh tools/setup_pc.sh            (all snapshots named in tools/variants.txt; a few minutes)
#      sh tools/setup_pc.sh core       (only what the gate's opponent roster and the v12/v13 tests need)
# Snapshot bots (.unswbc-build) are per-machine builds and param variants (snapshots/var/*) hold absolute paths,
# so both are recreated here from tools/variants.txt (python3 tools/mkvariants.py regenerates that list).
# Then tools/gate.py, tools/bench.py and tools/run_queue.sh work; see docs/PC.md.
set -e
cd "$(dirname "$0")/.."
PY=$(sh tools/py.sh)
echo "python: $PY"
if [ "$1" = core ]; then
  WANT="v8 v8x v9 v10 v11 v11c v12m v13 v13e v13g v13h v13i"
else
  WANT=$(awk '!/^#/ && NF >= 2 {print $2}' tools/variants.txt | sort -u)
  WANT="$WANT v13 v13i"
fi
for s in $WANT; do
  [ -f snapshots/$s/main.cpp ] || { echo "skip $s (no sources)"; continue; }
  rm -rf snapshots/$s/.unswbc-build
  "$PY" -c "from unswbc.run import _resolve; print(_resolve('snapshots/$s')[0][0])"
done
rm -rf snapshots/var && mkdir -p snapshots/var
n=0
while read -r name snap rest; do
  case "$name" in ''|'#'*) continue ;; esac
  [ -x snapshots/$snap/.unswbc-build/bot ] || continue
  # shellcheck disable=SC2086
  sh tools/variant.sh "$name" snapshots/$snap/.unswbc-build/bot $rest >/dev/null
  n=$((n + 1))
done < tools/variants.txt
echo "$n variants in snapshots/var"
J=$( (nproc || sysctl -n hw.ncpu) 2>/dev/null | head -1); J=$((J > 2 ? J - 1 : 1))
echo "ready. e.g.: $PY tools/gate.py snapshots/var/cand -j $J --out results/gate-cand.jsonl"
