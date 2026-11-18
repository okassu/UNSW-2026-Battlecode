---
name: test-harness-pending
description: "Gate harness state: v10 baseline + v11b gate ran overnight 2026-09-28 (results/); v9 profile still deferred until the user asks"
metadata:
  node_type: memory
  type: project
  originSessionId: 4d8ed506-1a81-4662-add8-f656fbd1449f
  modified: 2026-09-28T12:05:24.080Z
---

The in-depth test harness was built on 2026-09-28 night. The v9 profile is still deferred ("ill tell you to do
it later"): **run it only when the user asks.** The v10 profile was approved and started on 2026-09-28 22:45 as
the baseline for v11b: `results/overnight.sh` writes results/gate-v10.{jsonl,txt} then results/gate-v11b.{jsonl,txt}
(v11b = snapshots/v11b: side-aware early crossing + carrier threat memory; tuning runs were all within noise of v10).

How to run a profile (one bench at a time; the per-user process limit is 1333, use -j 6):
`~/.local/share/uv/tools/unswbc/bin/python tools/gate.py snapshots/var/v10u --out gate-v10.jsonl --logdir <dir> --metrics`
then the same for `snapshots/var/v9u`, and compare with `--report gate-v10.jsonl --baseline gate-v9.jsonl`.

What exists: `tools/genmap.py --grid` (size x symmetry x style incl. corridors x motif incl. home pockets,
portal field, 2x2 portal pockets x long/short starter, .json sidecars); `maps_tune/` (60, tune on these) and
`maps_val/` (45, never tune on these; the gate's default set with maps/*.map); `tools/roster.txt` (v8, v9, agg,
consol, turtle, raider, portal; param variants in snapshots/var/); `tools/gate.py` (category and opponent
table, ship rule: win >= 0.55 or longest >= +1.0 and no row down > 0.08 vs baseline); `tools/ladder.py`
(prevBattles batch report: map id, dumbstats, portal-only ground use). Smoke test passed; turtle loses every game.

**Why:** the user wants strategies that apply to any map (see [[generalise-strategy]]); the gate is how
changes get judged before submitting. Related: [[ladder-game-debugging]].
