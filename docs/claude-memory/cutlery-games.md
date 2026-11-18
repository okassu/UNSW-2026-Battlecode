---
name: cutlery-games
description: "topEloBattles/game1-40 (pasted 2026-09-29 evening) are all games of cutlery, ladder rank 1; how to find its side and how its survival compares to ours"
metadata:
  node_type: memory
  type: project
  originSessionId: 805eeb45-73eb-4636-af82-3b5febcf20c6
  modified: 2026-09-29T09:10:44.076Z
---

On 2026-09-29 (18:48-19:02) the user pasted into prevBattles/game1-40 (moved to topEloBattles/ on 2026-09-30) games of **cutlery (ladder rank 1)**; game3 is
empty. This overwrote the earlier top-team games 21-40 described in [[top-games-findings]] (the FINDINGS.md notes remain).
No team names are in the logs. Cutlery = the side whose sonar has the largest share of 2-direction ping rounds
(~2.0-2.9 pings per dragon-round, mostly 3 directions, rarely 4) and that sacrifices ("gave no valid action") from
r1-r30. With that rule cutlery went 22-17 (5 of 7 five-game matches; lost games 11-15 0-5 to a 4-direction scanner).

Survival per 100 moves, cutlery on the ladder vs v12l in self-play on the same 10 maps: hit itself (non-stub) 0.00 v 1.19,
no valid action 1.25 v 0.20 (both drop ceil(L/2) pearls, so partly bookkeeping), hit a body 0.04 v 0.37,
friendly head-on 0.02 v 0.18, death within 2 rounds of a portal crossing 0.06 v 0.45. Portals is our worst map.
Our self-hits and body hits that have an indicator line were almost all DOOMED (no survivable move left).
No crashes or timeouts in 234 bench logs; 0 wall deaths.

**Why:** the user asked this to decide between iterating v12 and copying cutlery's strategy before the Sprint (1 Oct).
**How to apply:** use the side rule above with `dumbstats.analyse(..., side_override=)`; scratch scripts for it are
not in the repo. See [[generalise-strategy]].
