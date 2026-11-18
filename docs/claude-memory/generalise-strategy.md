---
name: generalise-strategy
description: "Strategy and fixes must generalise across opponents and unseen maps; judge ideas on all evidence (FINDINGS.md, BUGS.md history, every ladder batch), never one team or one map"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 4d8ed506-1a81-4662-add8-f656fbd1449f
  modified: 2026-09-28T04:03:16.154Z
---

Build every strategy change as a general rule and back it with evidence from ALL context: the 17 games in FINDINGS.md
(GtR, SSS, RND, B60, silent teams), every ladder batch logged in docs/BUGS.md (abasdasga, computers, BitShift...),
and the current prevBattles games (Pizza, devtest). Never tune to one opponent or to one map's coordinates.

**Why:** the Sprint (1 Oct 2026, bo7) uses only unseen maps and seeding autoscrims may add new maps; the user
said "dont just use pizza as a benchmark - they are just one team" and "generalise all strategies as much as possible,
also using context from before". They want the bot to learn from maps in play, not to carry map-specific plans
(map library stays on for the ladder only).

**How to apply:** when a loss suggests a fix, state the underlying mechanic (e.g. "drops go to whoever has more mouths
nearby"), count how many different opponents/maps show it, and test with MAPLIB=0. Related: [[ladder-game-debugging]].
