#!/bin/sh
# Run gate jobs one after another from a list file (or stdin), on any machine.
#   sh tools/run_queue.sh jobs.txt            each line: VARIANT SET   (# comments and blank lines skipped)
#   echo "cand real" | sh tools/run_queue.sh -
# Sets (all v the v11 build unless named):
#   real    15 bundled maps, library off, v v11u, 4 seeds      -> results/ab-VARIANT.jsonl
#   val     45 generated validation maps, v v11u, 2 seeds       -> results/val13-VARIANT.jsonl
#   lib     15 bundled maps, library ON, v the live v11, 4 seeds -> results/lib13-VARIANT.jsonl
#   roster  bundled + validation maps v agg consol early portal raider v9, 1 seed -> results/roster-VARIANT.jsonl
# Every run resumes (--resume), so a stopped queue can simply be started again. JOBS=N overrides the worker count
# (default: cores - 1). Waits for any gate already running first, so two queues never share the CPU.
cd "$(dirname "$0")/.."
PY=$(sh tools/py.sh) || exit 1
J=${JOBS:-$( (nproc || sysctl -n hw.ncpu) 2>/dev/null | head -1)}
[ -z "$JOBS" ] && J=$((J > 2 ? J - 1 : 1))
while pgrep -f "tools/gate.py" >/dev/null; do sleep 15; done
grep -v '^#' "${1:--}" | while read -r v set; do
  [ -z "$v" ] && continue
  case "$set" in
    real)   $PY tools/gate.py snapshots/var/$v --maps maps/*.map --opps snapshots/var/v11u --seeds 4 -j $J --resume \
              --out results/ab-$v.jsonl --logdir results/logs-ab/$v >> results/ab-$v.txt 2>&1 ;;
    val)    $PY tools/gate.py snapshots/var/$v --maps maps_val/*.map --opps snapshots/var/v11u --seeds 2 -j $J --resume \
              --out results/val13-$v.jsonl --logdir results/logs-val13/$v >> results/val13-$v.txt 2>&1 ;;
    lib)    $PY tools/gate.py snapshots/var/$v --maps maps/*.map --opps snapshots/v11 --seeds 4 -j $J --resume \
              --out results/lib13-$v.jsonl --logdir results/logs-lib13/$v >> results/lib13-$v.txt 2>&1 ;;
    roster) $PY tools/gate.py snapshots/var/$v --maps maps/*.map maps_val/*.map --opps agg consol early portal raider v9 \
              --seeds 1 -j $J --resume --out results/roster-$v.jsonl --logdir results/logs-roster/$v >> results/roster-$v.txt 2>&1 ;;
    *)      echo "unknown set '$set' for $v" >&2 ;;
  esac
  echo "$(date '+%H:%M') done $v $set"
done
