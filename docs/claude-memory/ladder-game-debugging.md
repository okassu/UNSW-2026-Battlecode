---
name: ladder-game-debugging
description: "How the user hands over ladder games (our games: prevBattles/prevN.txt; top teams: topEloBattles/gameN.txt), and how to tell which side we are"
metadata:
  node_type: memory
  type: project
  originSessionId: a3ac62dd-ecfb-4481-811a-2f955bbc702a
  modified: 2026-09-27T07:55:46.677Z
---

The user pastes ladder game logs into `prevBattles/gameN.txt` (first line names the matchup, e.g. "us vs bitshift (portals)").
Since 2026-09-29 our own games go into `prevBattles/prevN.txt` (placeholders made with touch; sometimes a first-line note with
the user's question), and other teams' full logs live in `topEloBattles/gameN.txt` (the cutlery batch moved there 2026-09-30).
From 2026-09-27 they pass games one at a time for targeted debugging, with their own diagnosis of why we lost.
Some logs are other teams' games for study (game2 on 2026-09-27: "shark++ vs 龙虎豹", no "us").
**Our side is not fixed:** we were EVEN (dragon 0) in game1 but ODD (dragon 1) in games 3-4. Only our bot prints
indicator lines (`dragon N: <LABEL>`), so the parity of those ids tells which side we are. The bundled map is identified
from pearl tiles / start positions against `maps/*.map`; some ladder maps (e.g. "portals") are not bundled.

**Why:** focused fixes per loss instead of broad re-tuning; self-play benches vs older snapshots can't reproduce ladder opponents.
**How to apply:** confirm the user's diagnosis from the log first, fix it behind an env `param()` so it can be A/B'd, check it
with `tools/runlog.py` on the same map (or a `tools/reconstruct_map.py` fixture), and log it in `docs/BUGS.md` (newest first).
