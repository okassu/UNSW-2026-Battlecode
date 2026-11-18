---
name: top-games-findings
description: "What 20 top ladder games (topEloBattles/game21-40, 2026-09-29) showed winners do; tools to re-run the analysis"
metadata:
  node_type: memory
  type: project
  originSessionId: 4d8ed506-1a81-4662-add8-f656fbd1449f
  modified: 2026-09-29T02:43:59.876Z
---

(Logs 21-40 were overwritten on 2026-09-29 evening by cutlery games, see [[cutlery-games]].) Full ladder logs (both teams' pings, no indicators) of top teams were analysed on 2026-09-29; notes are in
FINDINGS.md "Top ladder games 21-40". Tools: `tools/topgames.py` (two-sided macro + sonar fingerprint),
`tools/zones.py` (eats on home/contested/enemy ground by window), dumbstats `side_override`.

Winners: rush contested ground in the opening (devil: 5 of 6 heads on the centre at r25; our v12 had 0),
out-breed by r50 (16/20), start head-ons when locally ahead (4.2:1; ours ~2:1) and also trade down
(24% of their rams use a longer dragon). Carriers are built late (born r300-470), nobody sprints.
Echo-scanning team (pings 0 on all 4 directions) lost 7 of 10, so sonar scanning is not proven to matter.

Changes it drove: OPENING_ROUNDS (contested-first opening, v12c; 4/4 vs v11 on devil+trophy),
TRADE_DOWN (v12f), SPLIT_ROOM (v12d). A lower-id right-of-way rule did NOT reduce friendly head-ons:
on portals they happen between dragons 7-21 tiles apart, i.e. through blind portal crossings.

**Why:** the user asked to "completely steal" top teams' strategy; see [[generalise-strategy]] for how to judge.
**How to apply:** when new full logs arrive, run `python3 tools/topgames.py topEloBattles/gameN.txt ...` and
`python3 tools/zones.py ...` first; compare with our own bench logs (`tools/bench.py --logdir`).
