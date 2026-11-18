# Bugs found in local testing (log this convention: newest first, keep fixed ones for history)
(2026-09-30: the cutlery batch that entries below call prevBattles/game1-40 now lives in topEloBattles/game1-40; our own
ladder games are prevBattles/prevN.txt.)

## 2026-09-30 (evening) — bug hunt in v15's 50 ladder games (tools/analysis/newborn.py, carrierkills.py)
- **BUG (fixed, v16f NECK_PORTAL 1):** a newborn has no body history, and ownChain only follows segments in view, so a
  child split off a parent straddling a portal thought its neck was not there and crossed the portal into it: 279
  split children and 30 U-turn children died "hit itself" on their first move (244 on portals, then slithery,
  default, queen, schooltime). Fix: a 2+ cell dragon whose neck is not in view does not cross the head's portals.
  Portals map, 8 games each: first-move self-hits of split children with a free neighbour 386 -> 34.
  But real maps v v15: library off identical (0 of 120 games changed), library on 0.592 v 0.625 (4 better, 8 worse;
  portals 5/8 from 8/8). In portal pockets the child's instant death drops a pearl and frees the pocket (a sacrifice
  in effect); kept alive it blocks it. NECK_PORTAL left in the code, default 0 (v15 behaviour).
- CPU (judge sandbox, big_empty 64x64, seed 1, points per turn, limit 100M): v13 / v14 / v15 with the library p50 17.6M,
  p99 22.5M, max 36.8M; v15 **without** the library (Sprint conditions, snapshots/v15nolib) p50 20.2M, p99 51.6M, max
  55.4M (v11 without the library: max 24.4M). Cause: CONTEST_WALK's imageDist (a Dijkstra over the map per candidate
  symmetry, refreshed every 16 rounds; up to 3 candidates until symmetry is confirmed off the library): with
  CONTEST_WALK 0 p99 24.3M, max 25.7M. Within the limit (45% headroom on the largest bundled map).
  -> v16g CONTEST_SPREAD 1 (one distance map per turn, stalest candidate first; straight-line distance for candidates
  not computed yet): without the library p50 21.7M, p99 34.9M, max 37.4M (the library-on level). Regression runs:
  results/ab-cs_u, val13-cs_u v val13-v15u.
- A newborn's first move happens in the round it is split off and the ladder log prints no indicator for it (1263
  "unlabelled" deaths were all such first moves: 18% of 2-cell U-turn children, 6% of 2-cell split children die then;
  the U-turn ones are mostly boxed in by slithery's 2x2 loops).
- Not bugs: engine rejections are only our sacrifices; 0 wall deaths; carriers crossed a portal once in 13908 moves
  (DOOMED); lunges hit enemies 741 times, allies 2. Of 273 deaths on a plain MOVE into a body, 272 were blind portal
  crossings; all 81 rams of our own newborns by our own dragons came through portals (69 on portals). Blind crossings
  are the remaining leak (the exit is never in view); fixing them needs crossing coordination.

## 2026-09-30 (afternoon) — v15 (v30) ladder games prevBattles/prev148-198
v15 23-27 (v14 30-33-1, within noise); devtest (prev148-157) 5-5 (v14 3-7, v12 / v13 2-8). Combined offline check:
library off 0.625 +3.81, library on 0.625 (v14 0.633 / 0.617). By map v15 (v14+v15): autarky 5-0 (8-3), stronghold 4-1
(7-5), schooltime 3-2, slithery 3-2, portals 3-2, default 2-3, dilemma 2-3, devil 1-4 (6-5), trophy 0-5 (2-10),
queen 0-5 (0-12). Trophy losses per game: we eat mostly contested (r0-25 1 home / 3 contested, r25-50 4 / 9, r50-100
11 / 16) while they harvest home (4, 11, 29) then raid ours (26 on our side r50-100); dragons r100 8 v 25. Queen: food is
behind portals (1.62 of ~2 pearls/round); we eat as much there but stall at 5 dragons by r100 (them 11).
**Every ladder map is in the library**, so all ladder games so far test the library-on play (rush, split mode 1); the
Sprint's unseen maps run library-off play (no rush, split mode 2), tested only against v11 so far. snapshots/v15nolib
(MAPLIB 0 default, otherwise v15) is ready for a ladder test of that play; not submitted (user: not for now).
- v15's carrier changes on the ladder (tools/analysis/carrierkills.py, 10+ dragons dead head-on): ours 0.52 per game
  (v14) -> 0.60 (v15), 4-5 step killers 4 -> 2 (none at 5); theirs 0.47 -> 0.54 (1 by a 4-step lunge of ours). Noise.
- Slithery length losses (prev175 42 v 83, prev185 43 v 83): their longest 26 / 17 at r200 and 29 / 45 at r300; ours
  6-10 until r350 in all four slithery games. No dragon of ours held the carrier role in r50-199 (claims open at
  CLAIM_ANY_FROM 200), then 9-17 different carriers per 50 rounds at 8-13 cells, U-turning and dying in the 2x2 loops.
- The king (id 0/1, carrier by default from CARRIER_FROM 60) splits like everyone before that and dies as a 2-cell stub:
  dead by r6 in all 33 v14/v15 games on autarky (L14), dilemma (L11) and slithery (L7); dead before r100 in most others.
  KING_KEEP_LEN (v16): carrier from round 1 -> on slithery the start boxes a 7-cell dragon in, it U-turns at once, the
  child U-turns again and the role ends (the early handover also needed claims before r60); reworked as "a long-start
  king does not split before CARRIER_FROM" (v16c) -> on slithery still forced to U-turn at round 0. Tests: kk7_* and
  early_* (CLAIM_ANY_FROM 120 + GROW_FROM 150), results/ab-*, lib13-*.
  Real maps library off v v15 (0.625): KING_KEEP_LEN 7 0.617 (2 better, 3 worse; autarky 5/8 from 3/8 +12 longest,
  slithery 5/8 from 8/8); early claims + growers 0.483 (9 better, 26 worse; the third time early carrier timing lost).
  Library on: KING_KEEP_LEN 7 0.617 v 0.625 (only slithery changed, worse: kings still U-turn at the start). Both off.
- Queen (0-12 in v14/v15), 12 games: r0-150 deaths ours 309 v theirs 239; head-ons 115 v 75. All rounds: friendly
  head-ons 48 v 18 (0.34 v 0.04 per 100 moves), deaths within 2 rounds of a crossing 0.73 v 0.29 per 100 moves. 44 of
  52 friendly pairs: one dragon crossed a portal blind while a teammate stepped onto the exit tile (8 plain steps).
  -> v16d MOUTH_AVOID (a step onto a portal mouth without a pearl costs MOUTH_AVOID). Tests: results/*-ma400*.
  MOUTH_AVOID 400, library on v the live v11: friendly head-ons 1063 -> 271 (queen 65 -> 8, portals 647 -> 229),
  crossing deaths 0.108 -> 0.069 per 100 moves, but 0.592 v 0.625 (12 better, 16 worse; queen 3/8 from 5/8, trophy,
  schooltime down; slithery, trauma, stronghold up): to cross, a dragon must first step onto the mouth, so the penalty
  also keeps it from portals (queen's food is behind them). Trying 100.
  Library off, 400: 0.417 v 0.625 (8 better, 33 worse): off the library portals must be found and crossed to learn them.
  Any mouth penalty must be library-only.
  MOUTH_AVOID 100, library on: 0.608 v 0.625 (12 better, 14 worse); friendly head-ons 1063 -> 379, queen 7/8 from 5/8
  (the target), but portals 4/8 from 8/8 (every pocket tile is a mouth), schooltime and trophy down.
  -> v16e MOUTH_ROUTE: evaluate() records whether each direction's best target is reached through a portal crossed
  straight from the step's tile (bfsVia); the penalty then applies only to standing on a mouth, not to crossing.
  Tests: results/*-mr400*. MOUTH_AVOID 400 + MOUTH_ROUTE, library on: 0.608 (10 better, 12 worse), friendly head-ons
  1063 -> 924 only, queen unchanged: the teammate on the mouth is usually about to cross itself, so these are two of
  ours using one portal from both ends in the same round. Needs crossing coordination (direction by round parity breaks
  because a dragon cannot wait on a mouth). All MOUTH_* off; mybot = v15 behaviour (new params default off).

## 2026-09-30 (midday) — v14 (v29) ladder games prevBattles/prev84-147; v15 carrier depth
v14 30-33-1 (v13: 23-36-1 over prev24-83). Split parents dead within 2 rounds 2-9% in most games (v13 20-60%).
By map v14 / v13: autarky 3-3 / 0-6, dilemma 4-1-1 / 1-4, devil 5-1 / 3-2, stronghold 3-4 / 1-5-1, schooltime 3-3 /
2-4, trophy 2-5 / 1-5, slithery 3-3 / 3-3, portals 4-2 / 5-1, default 3-4 / 4-3, queen 0-7 / 3-3 (v13's queen wins came
against teams eating 0-6 per 25 rounds there, v14's losses against 17-31; offline v14 5/8 v v13 2/8 on queen). Devtest
(prev94-103) 3-7 (v12, v13 2-8). Losses: 17 eliminations before r500 (trophy, queen, dilemma, default: out-bred by
r100), 16 on length at r500 (slithery, stronghold, schooltime, portals).
- Stronghold v the 0x9000 team (prev122, prev134): 94-102 cells of mass in 16-24 dragons at r499 but longest 10-12
  (theirs 28 from 31-44): 8-13 distinct dragons marked carrier per 25 rounds in r350-499, 9-21 U-turns and 4-16
  sacrifices per window. The carrier walks into maze dead ends that pass the L + 2 check, U-turns out (2 cells, new id)
  and the feeders chase a new target each time. Schooltime prev90: carrier killed head-on four times (r402-466, 15-26).
-> v15a CARRIER_DEPTH (carrier needs a path of 2L + 2 for CARRIER_DEEP_BONUS): real maps library off 0.625 v v14's
   0.633, 1 of 120 games changed, carrier U-turns unchanged. Off. Of 210 carrier U-turns after r250 in 10 stronghold
   ladder games, 196 were by "carriers" of 4-11 cells (late claims from CLAIM_ANY_FROM 200 at CLAIM_LATE_LEN 6):
   CLAIM_LATE_LEN 10 real library off 0.583 (7 better, 13 worse; slithery 3/8 from 8/8, longest 34 v 37.7). Kept at 6.
- Carriers of 10+ killed head-on in v14's 64 ladder games: 17, all on a plain move (we were rammed). The killer's
  move: 1 step 7, 2-3 steps 5, 4-5 steps 4 (NNEES, WWWSS, EEESS, WWWS: sprints past the threat map's 3-step cap).
  -> v15b CARRIER_REACH (carrier-only threat layer to min(L - 1, CARRIER_REACH) steps). CARRIER_REACH 6: real library off
  0.642 v 0.633 (3 better, 2 worse), library on 0.617 v 0.617 (1/1): harmless against v11, which never sprints past 3;
  its benefit can only show on the ladder. -> in v15.
  v15c LUNGE_MAX (our lunges sprint up to LUNGE_MAX steps at enemies of LUNGE_FAR_MIN 10+): LUNGE_MAX 5 real library off
  0.625 +4.16 v 0.633 +3.48 (2 better, 3 worse; v11's longest 31.8 -> 30.6), library on 0.633 v 0.617 (2 better, 0
  worse). -> in v15.
  Home guards HOME_MOD 3: library on 0.608 +1.48 v 0.617 (14/15; stronghold 4/8 from 1/8, default_small 1/8 from 6/8,
  trophy unchanged), library off 0.483 v 0.633 (12 better, 30 worse). Off.
**v15 (snapshots/v15, mybot) = v14 + CARRIER_REACH 6 + LUNGE_MAX 5.** Combined check: results/ab-v15u, lib13-v15lib.
Submitted as "sworm v15" (server v30), 2026-09-30 ~13:55, on the user's request. Rollback: snapshots/v14.
- Trophy, v14's five losses, eats home / contested / their side: r50-100 us 69 / 83 / 24, them 155 / 88 / 132 (they
  raid our home while we sit in the harvested middle); r0-50 even. v11 in self-play does it too (r50-100 120 on our side
  v our 44 on theirs). -> v15d home guards (HOME_MOD, HOME_W, HOME_FAR). Tests: results/*-hg3*.

## 2026-09-30 (morning) — v13 (v28) ladder games prevBattles/prev54-83
All v13 (no sacrifice before r360 in any game). 12-17-1: 0x0764 4-direction team 4-6, 0xd7a5 team 1-3-1, a 4-direction
team with varying tags 3-2, 0x9000 team 4-6. v13 over 60 ladder games (prev24-83): 23-36-1. By map: autarky 0-6,
trophy 1-5, stronghold 1-5-1, dilemma 1-4, schooltime 2-4, queen 3-3, slithery 3-3, devil 3-2, default 4-3, portals 5-1.
Split parents dead within 2 rounds by map (tools/analysis/splitdeaths.py): dilemma 62%, autarky 42%, slithery 40%,
portals 33%, queen / stronghold 20%, devil 15%, schooltime 11%, trophy 10%, default 8%: the two worst maps lose the
most split parents (SPLIT_PARENT_SAFE went 8/8 on both in the library-on test). Trophy is out-bred by r100 without it.

## 2026-09-30 (morning) — v14: stranded split parents
v13 v devtest (prev34-43), r0-100: we out-ate them 1559 v 1153 and out-split them 668 v 506 but died 501 v 329, so
they had more dragons at r50 in most games. tools/analysis/bodyhits.py (bodies rebuilt from the log): 522 of our 544
body-hit deaths were into teammates' bodies, 22 into the enemy's. By our labels (30 v13 games): 6459 normal splits,
the 2-cell parent dead within 2 rounds 1760 times (27%; devtest's halvings ~15%), ~1450 of them DOOMED (no legal
move): SPLIT > TIGHT > DOOMED 579, TIGHT > SPLIT > DOOMED 396, MOVE > SPLIT > DOOMED 374. Dragons up to 2 * SPLIT_LEN
split with no check on the parent ("the split is how the swarm gets back out" of dead-end farms). The child died 14%.
-> SPLIT_PARENT_SAFE (v14a): split only if the parent has a first step with a path of its length + 2, the child's
cells counted as walls. Smoke: library on, beat v11 on autarky (r208). Tests: results/ab-sp_u, lib13-sp_lib, val13-sp_u.
Results v v13 (same games): real maps library off 0.617 +3.40 v 0.583 (19 better, 15 worse); library on 0.617 v 0.558
(17/10; autarky, dilemma, portals 8/8; stronghold 1/8, longest -28); generated 0.522 v 0.575 (21/31; maze 0.458 v
0.667, portal pockets 0.388 v 0.625). Split parents dead within 2 rounds 23% -> 8%, but U-turns 62 -> 106 per game and
22% fewer splits on stronghold. SPLIT_PARENT_DEPTH 2 (parent needs only 2 steps): generated 0.531 (16/24) - blocking
these splits hurts mazes and pockets whatever the depth. Likely the dead-end farm cycle (eat into a pocket, split, the
stub stays) is part of the 27%. Mode 2 (SPLIT_PARENT_SAFE 2, v14c): check only when the whole dragon still has a safe
move; a trapped dragon splits as before. Tests: results/*-sp3*.
Mode 2: real library off 0.633 +3.48 (15 better, 9 worse v v13), generated 0.558 (10/13), but library on 0.508 (8/14;
autarky 4/8, dilemma 5/8 v mode 1's 8/8). Depth 2 real library off 0.592. With the library, due respawns
(pocketPearls) pull dragons into dead-end pockets and the stranded-split treadmill (game 41's autarky diagnosis) is
where mode 1 pays; off the library blocking those splits costs mazes and pockets.
**v14 (snapshots/v14, mybot): v13 + SPLIT_PARENT_SAFE 3 = mode 1 on library maps, mode 2 elsewhere.** Expected from
the offline runs: library on ~0.617 (v13 0.558), library off ~0.633 real / ~0.558 generated. Smoke: beat v11 on
dilemma (library on, r89). Needs ladder games (autarky 0-6 and dilemma 1-4 for v13).
Submitted as "sworm v14" (server v29), 2026-09-30 ~12:10, on the user's request. Rollback: snapshots/v13.

## 2026-09-30 (morning) — v13b submitted (server v27), then back to v13 (v28)
The user found v13b worse on the ladder (manual testing), so v13 (snapshots/v13) was resubmitted as v28 and mybot's
defaults went back to v13 (CONSOLIDATE_FROM 360, MERGE_FROM 400, CARRIER_FOOD_RADIUS_END 3 = no change).
snapshots/v13i as "sworm v13b" on the user's request (for manual ladder testing): v13 + CONSOLIDATE_FROM 300,
MERGE_FROM 350, CARRIER_FOOD_RADIUS_END 6 (mybot defaults now). Offline v v11 (real maps, library off): 0.542 +3.27
v v13's 0.583 +3.03 (6 better, 11 worse), but the longest carrier of all variants (41.1 v 36.4, share of our mass 0.45
v 0.41) - a bet that carrier length matters more against devtest-class opponents than against v11.
Also tried: CARRIER_FOOD_RADIUS_END 6 alone 0.550 +2.59; CONSOLIDATE_FROM 300 + MERGE_FROM 350 alone 0.542 +2.17.

## 2026-09-30 (early) — v13 (v26) ladder test games prevBattles/prev24-53
v13 11-19 (v12: 6-18 in prev1-23). Three opponents by sonar, 10 maps each: prev24-33 mixed 1-2 direction pingers
6-4; prev34-43 devtest (same fingerprints as prev1-10, incl. map-specific ones: schooltime 0xd17d, slithery 3-direction)
2-8, the same as v12; prev44-53 a 4-direction 0xe921 team 3-7. Portals 3/3, autarky / dilemma / stronghold 0/3.
- r500 length losses despite more dragons: schooltime v devtest (prev40) 62 v 33 dragons and 231 v 95 mass at r200,
  then our mass 153 (r350) -> 63 (r499, 2 dragons, longest 36) while theirs grew to 280 (longest 43). Slithery 43 v 66,
  stronghold 26 v 42 (devtest consolidated from r250-300: 35 -> 12 dragons, longest 15 -> 33 by r400). We end with
  2-4 dragons marked carrier.
- Our r300+ sacrifices (728 in 19 games) dropped pearls eaten 37% by a carrier, 57% by other dragons of ours, 4% by
  the enemy: feeders' bodies reach past CARRIER_FOOD_RADIUS 3 and a small dragon that re-feeds passes on a quarter.
  -> CARRIER_FOOD_RADIUS_END (radius from CONSOLIDATE_FROM) and earlier consolidation, being tested (results/ab-fr6*).
- Early eliminations: trophy (0-1 eats in r0-20 while the rush walks to the centre), dilemma (13 of our dragons dead
  in r0-20 to head-ons and body hits; one survivor wandered 30 rounds without eating).
- DISPOSE_STRICT only in crowds of 3+ ally heads: 0.492. Off.

## 2026-09-30 (night) — v12 (v25) ladder test games prev1-20, survival and scout tests
Submitted: snapshots/v13 as "sworm v13" (server v26), on the user's request, 2026-09-30 ~01:00. v13 = v12m +
OPENING_LIB_ONLY 1, MERGE_FROM 400, SEE_CARRIER 1, FEED_SEEN 1. v11 = 0.50 on each: real maps library off v13
0.583 / v12 0.442; generated 0.575 / 0.575; real maps library on 0.558 / 0.533. Roll back to v12 (snapshots/v12m)
if its ladder games do worse.
v12 v devtest and others (prevBattles/prev1-20): 5-15 (devtest 2-8). The opening works on library maps (contested eats
by r50 27 v 11, 31 v 17, 17 v 0, 50 v 0; devil eliminated them r110). Losses: out-bred from r100-250 after winning
the middle (9 games) or out-lasted on length at r500 with equal or better eating (6: portals 39 v 44 and 18 v 25,
schooltime 33 v 47, stronghold 23 v 26, dilemma 10 v 21 and 18 v 25) - the second is what v13's merge targets.
We ate more per move than the opponents in every window (r100-300: 10.2-10.8 v 7.2-8.3 per 100 moves) but died far
more: 3.31 v 2.13 per 100 moves outside head-ons (self-hits 1.26 v 0.44, body 0.48 v 0.25, U-turn stubs 1.36 v 0.97),
so they had 30-60% more moves from r100. 1438 of 1553 self/body deaths were 2-3 cell dragons; 640 died one turn after
a move rated safe (a crowd closed the exit), 826 after a TIGHT turn. Self-play can't show this: both sides share it.
- DISPOSE_STRICT (small dragons prefer moves passing the strict all-heads-race check), real maps, library off, v v11u:
  1e6 0.508 (deaths down: self-hits 0.78 -> 0.69, body 0.28 -> 0.21, but eats 9.27 -> 8.28 per 100 moves; small maps
  0-1 of 8), 400 0.458, v the package's 0.583. Off.
- Dedicated scouts on library maps (applyMap never marks tiles seen, so 1 in 8 small dragons scouts all game; prev10's
  dragon 5): SCOUT_LIB 0 (none) library on v the live v11 0.492 v 0.558 with them (autarky 8/8, schooltime 1/8 from
  7/8). Kept (SCOUT_LIB 1).

## 2026-09-29 (evening) — cutlery batch (FINDINGS.md "Cutlery batch"), v12 real-map regression, v13 endgame merge
Submitted: snapshots/v12m as "sworm v12" (server v25), on the user's request, 2026-09-29 ~23:15.
Late-afternoon runs (logged after a crash lost the session; numbers from results/):
- v12h (blend opening, OPENING_ROUNDS 40): tune30 0.628 +2.55 PASS; library on v the live v11, real maps 0.567 +4.05
  PASS; library off, 60 val maps v v11u 0.560 but real maps 0.450 (gen 0.597); real maps v v11u + agg 0.492 FAIL.
- v12k (+ FARM_MIN 5): val 0.439, real maps 0.283. FARM_MIN back to 3. v12l (+ THREAT_CAP 1): val 0.528 FAIL,
  real 0.389, generated 0.569. v12m = v12l + OPEN_LINE_MIN/MAX (mybot until v13). v12sub: a submission cut of
  v12h without the game-41 fixes (not known to be submitted).
- Schooltime, v12k variants v v11u (4 seeds, 7-8 games): every v12 threat/carrier setting 1/8; v11 threat values
  3/8, v11 carrier 3/8, all v11 values 3/7. Game-41 fixes each alone v the blend (kb_*): interrupted, kb_farm5 46
  games all draws/unfinished.
Every v12 variant is below v11 on the 15 real maps with the library off (0.28-0.49) while above it on generated
maps (0.51-0.60). Same maps, v12l v v11 (54 games): eats and deaths per move are equal, but v11 has more dragons
(moves per game on autarky 9341 v 4303, default 6997 v 5183, trophy 4431 v 2610): the gap opens early and compounds.
Dragons spend the same share of turns at length >= 4 (8.3% v 10.2%; top teams 13%), so SPLIT_ROOM is not holding
back splits. Ablation of v12m (each change group back to its v11 value, 120 games each v v11u): results/ab-*.
Endgame (bench, 24 v12l games to r500): our longest was 30 of 143 cells at r499 (top winners 51 of 133), with 2-4
dragons marked carrier and many mid-size dragons that never fed: feeders need a carrier twice their length.
Ablation (real maps, MAPLIB=0, v v11u, 4 seeds x 2 colours = 120 games; tools/abcmp.py pairs same map/seed/colour):
- v12m as is 0.442 (-1.11 longest). Opening off (OPENING_ROUNDS=0) 0.533 (+1.65): 26 games better, 15 worse; every
  map but devil (0.75 -> 0.50) and default gains or holds. The rush aims at guessed contested ground off the library.
- Carrier back to v11 (CLAIM_ANY_FROM 330, CARRIER_FOOD_FROM 0) 0.475 (+0.78): stronghold 0.25 -> 0.75 (longest -20 ->
  +14), trauma 0.38 -> 0.62, but portals 0.50 -> 0.25, schooltime 0.50 -> 0.38. On stronghold v12m's carriers U-turn
  out of dead ends again and again at 8-14 cells (121 big U-turns v v11's 82; slithery 652 v 437).
- Threat values back to v11 (BIG/BEHIND_THREAT, THREAT_CAP 0): 0.442, only 8 of 120 games differ. Not the cause.
- All v11 values (opening, carrier, threat, SPLIT_ROOM, CONTEST_WALK): 0.508, 13 of 15 maps exactly 0.50 +0.0, i.e.
  the same games as v11 (pocketPearls' respawn counting, the one unparameterised change, barely matters).
On top of opening off (0.533 +1.65), 120 games each: carrier back to v11 0.475 (-1.30; 7 better, 14 worse); claims
from r330 only 0.483; no carrier food only 0.550 +0.04; AHEAD_HUNT 80 0.542 +1.03 (25 better, 24 worse: slithery
0/8, schooltime 7/8); MERGE_FROM 380 alone 0.517 +1.59; MERGE_FROM 400 + SEE_CARRIER + FEED_SEEN 0.583 +3.03 (17/11),
the same with the v11 carrier 0.592 +2.82 (16/9). End state (games reaching r500): longest / our mass 0.35 -> 0.42,
our longest 33 -> 37 (v11 30-33), still 2.7 dragons marked carrier.
More (real maps): SEE_CARRIER + FEED_SEEN without merging 0.517 +2.24; carrier v11 + merge + SEE + BEACON_ALL_FROM 380
0.558 +2.30 (marked carriers at r499 2.8 -> 2.4; not kept).
Generated validation maps (45 maps_val x 2 seeds x 2 colours = 180 games, v v11u): v12m 0.575 +1.95; opening off
0.528 +1.00 (28 better, 37 worse: the rush pays on generated maps, contested motif 0.675 v 0.575, portal pockets 0.700
v 0.575); opening off + MERGE_FROM 400 + SEE_CARRIER + FEED_SEEN 0.575 +2.54 (portal_field 0.388 -> 0.500, sealed
0.528 -> 0.681, small maps 0.625 -> 0.475). The package is the only setting that is >= 0.575 on both sets.
Library on (real maps v the live v11, library on, 120 games): v12m 0.533 +0.07; package + rush on library maps
(OPENING_LIB_ONLY 1) 0.558 -0.24 (13 better, 10 worse v v12m; devil 8/8, schooltime 7/8); package, no rush 0.517 +1.07
(devil 4/8, Colosseum 4/8, dilemma 2/8; autarky, portals, queen better). The rush pays when the library knows the
map (contested line and sources real) and loses when they are guessed: keep it for library maps only.
Trauma as the west team: first crossing r56 whatever the bot (east r19), but then we ate 16 by r100 v v11's 37 (same
seat, same seed): SPLIT_ROOM / CONTEST_WALK suspected (the only v12 changes acting outside the opening), queued.
  Controlled replay (runlog, seed 3, west v v11u east): v13 and v11 both lose from the west seat (r500, longest); the
  first difference is one split a round later (r76 v r75). The seat/seed, not v13, explains most of it.
SPLIT_ROOM 0 + CONTEST_WALK 0 on the package: real 0.550 +2.54 (13 better, 17 worse; trauma unchanged 0.38): kept.
Soft rush (OPENING_BLEND 2) on the package: real 0.500, generated 0.589 +4.26. Generated maps reward any rush, the
hand-made maps punish it off the library; the Sprint's maps are hand-made, so no rush unless the library knows the map.
**v13 (snapshots/v13, mybot): v12m + OPENING_LIB_ONLY 1, MERGE_FROM 400, SEE_CARRIER 1, FEED_SEEN 1.** Smoke: default
params beat v11 on devil (library on, r431) and on val4_07 in both colours (r228, r217). Roster gate v v12m: results/roster-*.
v13 params (defaults as above; AHEAD_HUNT, BEACON_ALL_FROM stay off): MERGE_FROM/MERGE_MARGIN (feed any carrier > L + 3 from MERGE_FROM if reachable in
time), SEE_CARRIER (an ally head in view longer than us and the known carrier is the carrier), FEED_SEEN (sacrifice
only with the carrier's head in view).

## 2026-09-29 (afternoon) — v12: 14 v11 ladder games + 20 top-team games (prevBattles/game21-40)
Top games (FINDINGS.md "Top ladder games 21-40"): winners rush contested ground in the opening, out-breed by r50,
ram when locally ahead (4.2:1, ours 2:1) and trade down 24% of the time. Changes (all behind param()):
- v12a: pocketPearls counts due respawns (dilemma's farm columns were skipped); CARRIER_FOOD_FROM 150,
  CLAIM_ANY_FROM 200; BIG_THREAT (2.5e7 for the carrier or L >= 12 stepping into enemy reach: carriers died to
  adjacent 2-cell enemies). v12b: BEHIND_THREAT 8e6 (was 3e5) when outnumbered. v12c: OPENING_ROUNDS (off): value
  / sqrt(steps + 1) and a contest boost on seen tiles, the top teams' centre rush.
- v12d SPLIT_ROOM 3: a normal split needs the child (born on our tail) to reach 3 free tiles. In 2x2 farm loops
  (slithery, portals' pockets) children were born boxed in and died at once. Most such deaths turned out to be
  U-turns inside the loop, which top teams do as often (slithery winner 294/1684), so the gain is small.
- Tried and dropped: a lower-id right-of-way rule against friendly head-ons. On portals all 75-88 per game were
  between dragons 7-21 tiles apart (blind portal crossings), and the rule changed nothing.
- v12f TRADE_DOWN (off): well ahead (heads +2), a dragon of <= 5 rams an enemy up to 2 shorter.
- v12g CONTEST_WALK: contested = walking distance from a tile to its own image through known kelp (one Dijkstra
  from the tiles within 2 of their image, refreshed every 16 rounds). Wrap-around manhattan rated devil's spawn
  0.75 (its image is 3 tiles away round the sealed edge) against 1.0 at the centre; 7 of 15 library maps seal
  their wrap edges.
Tuning (tune30, v v11u / early; +-0.065 per opponent): v12a 0.533 / 0.533, without BIG_THREAT 0.517 / 0.533,
without carrier food 0.533 / 0.500: all noise, v12a defaults kept.
Ladder games 41-43 (v11, all losses):
- autarky: 138 v 66 eats by r100 and 20 v 22 dragons, then 12 v 33 at r200. 64 of our 75 "hit itself" deaths were
  2-cell parents stranded after a SPLIT in the four 1-wide dead-end edge pockets ((34,17), (34,0), (19,17),
  (19,0); gap-1 tiles): 232 pocket eats for 73 deaths (theirs: 8 eats per death). A visit that eats 3 nets 0
  cells and 0 dragons. 7 of 10 outnumbered deaths in r80-260 were lone 2-cell dragons on plain MOVE deep on
  their side (e.g. 1 v 6 heads), rammed for their farms; drops 13:114. -> FARM_MIN 5 (v12a made pocketPearls
  count due respawns, which made the treadmill easier), DANGER_TTL/DANGER_MULT (targets within 2 of an enemy
  head seen in the last 12 rounds worth 35% unless we are ahead there), OUTNUM_FLEE (outnumbered by 2+ heads:
  +200 per tile from their nearest head).
- default: they entered the portal-only centre at r11 (we r17) and ate 393 there to our 11; 83 v 684 eats.
- trophy (out at r21, 0 eats): a 3-cell enemy showing 2 cells (third just outside our window) sprinted 2 steps
  into one of our 2 dragons at r9; we had treated its reach as 1. RIM_LEN=4 for everyone was timid before;
  RIM_LEN_LOW 3 applies only while we have <= LOW_UNITS dragons. (Verified: a dragon sees moves made earlier
  in the same round, so the threat map is right apart from the length.)
Opening results: pure sqrt (OPENING_ROUNDS 40) on real maps, library off, v v11u/agg: 0.533 v 0.550 without,
Colosseum 1/8 v 6/8 (16x16, spawns touch across the wrap: nothing to race to, dragons scatter and are picked
off). Blend (OPENING_BLEND 1 + CONTEST_WALK + SPLIT_ROOM) on tune30: 0.628 (v v11u 0.700, early 0.650,
agg 0.533) v v12b 0.506; on real maps 0.492. A queen loss as team B: the field 7 steps from spawn was found at
r60 because the contested pull led into the barren centre first (v11 has the same blind spot on that side).

## 2026-09-29 — v11 (snapshots/v11 = v11c, submitted as "sworm v11", server version v24) on the user's request
Ladder, v10: 9 losses in 3 matches (autarky x3, schooltime x3, slithery_fight x2, default). Out-bred from r50
(r200: 5-32 dragons v 29-64) with equal eats per move; 25-53 of our dragons died "hit itself" in r0-150, on autarky
49 of 53 were 2-cell parents dying right after a SPLIT in a dead-end pocket (the L-shaped (34-35,16-17) pocket):
enter at 2, eat 2, split 2+2, stub dies = 0 cells gained, one dragon lost. FARM_MIN (3): enter a pocket only if it
holds >= 3 pearls. Tuning (30 maps, v v10 / early / v9): FARM_MIN 3 0.489 +1.04 longest (v v9 0.533, +3.07) v
FARM_MIN 1 0.464 -0.16 (v v9 0.475). Slithery losses: their longest 28 / 15 at r200 v our 5; CARRIER_FOOD_FROM
(teammates leave pearls within 3 of the carrier) and earlier claims are in, off by default, still being tested.
Head to head v v10 every v11 variant scored 0.417-0.483 (60 games each, same maps): not shown better than v10.
Harness: max_tasks_per_child hung the gate's pool after 60 games (the whole night lost); gate.py now runs each
batch of 60 games in a fresh pool.
CPU (--sandbox, MAPLIB=0, big_empty): max 24.4M points/turn.

## 2026-09-28 (night) — v10 v devtest1 (10 ladder games) and v11 tuning
v10 v devtest1: W dilemma, schooltime (elim them r418; v9 lost it 12v30), stronghold; L autarky (elim r433),
default (v9 won it), devil (elim r363), portals (out-ate 1711 v 1242), queen (to r500; v9 elim r116), slithery_fight
(v9 won 43v39; out-ate 3361 v 2184), trophy (elim r75, 3 eats). Not a controlled comparison: devtest1 entered
default's centre at r10 v v10 (r24 v v9) and played schooltime differently, and each map is one game.
- Trophy: v10's cheap early crossing (PORTAL_EARLY 60) took the (12,21)->(12,6) shortcut at r16/r22, but devtest1
  had crossed at r10; two of ours arrived outnumbered and died head-on r29, r34, r47. Default: they entered the
  portal-only centre at r10, we at r31, and out-ate us there 231 v 101.
- Slithery: devtest1's carrier 13 at r200, 19 at r300, 65 at r500 (ours 7 at r300); our 22 and 20 cell carriers
  died head-on on plain MOVE C at r415 and r432 (escorts on). Likely an enemy just out of view sprinting in after
  we step toward it: the carrier's threat map only used heads in view.
v11 (snapshots/v11a, v11b): PORTAL_EARLY_SIDE (early cheap crossing only if the exit is >= 2 closer to our birth
tile than to its mirror; symFromLibrary gives library maps their symmetry for it), CARRIER_MEMTHREAT (enemy heads
seen in the last 2 rounds, out of view now, make cells within 3 + age a 2e5 threat for the carrier).
Tuning (30 maps_tune maps, v v10 and consol, 120 games per variant; +-0.045 overall, +-0.065 per opponent):
early carrier (grow 200 / claim 250 / feed 300) 0.508 -0.12 v v10 timing 0.508 +0.25; PORTAL_EARLY 60 unconditional
0.533, +2.21 longest on portal maps (off: +0.61); memthreat off 0.492 v on 0.508. v11b v portal rusher 0.567
(+1.78) v v10's 0.533 (+1.53). All within noise; small maps weak for both (0.35-0.37). Overnight gate (840 games
each, v10 baseline then v11b) in results/.
Overnight gate: 477 of 840 games per run failed with "[Errno 23] Too many open files in system" (system file table,
30720) from ~360 games on: pool workers leak descriptors over many games. tools/gate.py and bench.py now recycle
workers every 10 games (max_tasks_per_child); the 4 lost opponents are re-run (results/overnight2.sh).
Valid part (v8, v9, agg; 60 maps: 15 real + maps_val, MAPLIB=0): v10 0.537 overall, v8 0.675, agg 0.504, and v9
0.433 (-2.0 longest) — v9 beats v10 on the validation maps. v10 v v9 by category: long starter 0.278 (-4.6
longest, 9 of 18 pairs lost both), corridors 0.278 (-6.4), sealed 0.278 (-2.7), small 0.357, real maps 0.400.
v11b v v9: 0.467 (-1.3); long starter 0.417, sealed 0.444, corridors still 0.278 (-8.3). Suspect: BIG_UTURN_LEN /
carrier U-turn only with no survivable move — a long dragon in a 1-wide lane can only turn round by U-turn, and on
long-starter maps the long king is carrier from r60. Checked on the tuning set's long-starter/corridor/sealed maps
(results/morning.sh), not the validation set.

## 2026-09-28 (evening) — v10 (snapshots/v10, submitted as "sworm v10", server version v22): portals, gathering, sealed prizes, carrier protection
Ladder, v9 (server v21), 10 games, no headers (maps matched by start positions / kelp consistency): W default,
dilemma (elim r152), slithery_fight 43v39, trauma/stronghold 37v22; L autarky 15v25, portals 20v21, queen (elim
r116), schooltime 12v30, trophy (elim r123); devil log truncated at r37.
- Portals in 5 of 6 losses. Schooltime: 542 portal-only tiles carry 2.59 of ~7.6 pearls/round; they crossed first
  at r11, held the 204-tile field from r163 and ate 564 pearls there; we crossed 28 times (they 200), 6 crossers
  died head-on within 2 rounds, 0 head-turns inside all game. Locally (v v8) v9 does enter (3189 head-turns): we
  were shut out, not lost. Trophy: v9 targets the centre field from round 0 but walks round it; they took the
  (12,5/6)<->(12,21/22) shortcut and sat on it by r10 (we ate 30 v 181 in 150 rounds). Trauma: we got behind the
  portals at r10 and won 1051 v 198 there.
- Autarky: even to r150 (186 v 188, both on home pockets), then they ate OUR pockets (127) and theirs (145) in
  r150-300 while we ate ~10 per block elsewhere: an undefended raid.
- Portals map: out-ate them 1399 v 1162, lost by 1. 94 of our 100 deaths within 2 rounds of a crossing are U-turn
  stubs after pocket farming (deliberate). 39 friendly head-ons, attackers on plain MOVE/SCOUT: stepping through a
  portal onto a teammate at an exit out of view (a dragon's fatal move isn't logged, so the attacker's crossing
  doesn't show).
- Schooltime endgame: carrier U-turned 18 -> 16 -> 14 ..., and at r454 a 16-cell dragon's newborns U-turned 4 times
  in one round (16 -> 8); carriers died head-on on plain MOVE C at L12 (r445) and L20 (r460).
- Centre bias checked and rejected: share of pearls/round in the central 25% of the area is 0.01-0.47 across the
  15 maps (uniform = 0.25), so "go to the middle" is map-specific.
Toolkit upgraded to 1.2.2 (maps/ gains portals, slithery_fight; maplib regenerated, 15 maps).
v10 changes (snapshots/v10a): PORTAL_RISK (known crossing costs 1 early / exit seen empty within 6 rounds, 4 unknown,
8 enemy seen within 2 of the exit in 10 rounds); RALLY (rich spot in view with >= 2 enemy heads within 3 while not
ahead -> MSG_ENEMY id 1; small dragons within 20 gather, the trade rule holds them out of reach until ahead);
PRIZE (off-library: rich tiles seen or known but unreachable on foot make unmapped portal mouths within 10 worth up
to 6 for every small dragon, scoutMode on); BORN_UTURN=0, BIG_UTURN_LEN (carriers / L>=10 U-turn only with no
survivable move); ESCORT_* (from r400, 1 in 4 small dragons within 8 of a carrier >= 8 stand 2-3 from it on the
enemy side instead of feeding); DOOMED dies into kelp before a teammate's head.
tools/genmap.py: a portal pair whose image overlapped one of its own edges left an id with one edge; rejected now.
maps_gen/ gains gen2_00-07.
Results (MAPLIB=0, 15 real + 20 generated maps; v9 = snapshots/v9 with MAPLIB=0):
- v10a v v9, 2 seeds: 0.521 (+0.9 longest); wins every portal map 3-1, loses arena/dilemma/gen1_03 0-4.
- Ablations v v9 (1 seed): PORTAL_RISK=0 0.486 (-1.0 longest), PRIZE=0 0.486 (total -19), RALLY=0 0.514,
  carrier changes off 0.500. Portal risk and prizes pay.
- Two bugs behind the small-map losses: (1) BORN_UTURN also blocked the starting dragons (every dragon is "born"
  on round 0; dilemma's 11-cell dragon has to U-turn on round 1), and BIG_UTURN_LEN held 11-cell starters back:
  now only split children after round 1, and big non-carriers only from round 100; (2) the rally fired on round 2
  (opening contact near spawns looks like "enemy on rich ground"): weak-map set v v9 0.143 from round 0, 0.429
  from RALLY_FROM=60, 0.500 without rally.
- v10 (RALLY_FROM=60) v v9, 2 seeds: 0.507 (-0.3 longest, +1 total); RALLY=0 0.514. Head to head within noise.
  Same opponent (agg, 35 maps, 1 seed): v10 0.657 (+4.2 longest, +38 total) v v9 0.671 (+3.6, +21).
  MAPLIB=1, 15 maps, v10 v v9: 0.517 (+0.0 longest, +13 total). CPU (--sandbox, MAPLIB=0): big_empty max 23.4M,
  slithery_fight 12.8M.
- Shipped as a ladder test of the portal fixes the local opponents can't judge: locally level with v9, a little
  ahead on length.

## 2026-09-28 — v9 (snapshots/v9, submitted as "sworm v9", server version v21) for the Sprint (1 Oct, bo7, unseen maps only)
Plan (agreed 2026-09-28): generalise; every change behind a param(); judge with MAPLIB=0 on the 13 maps plus
generated ones (tools/genmap.py -> maps_gen/) against v8 and an aggressive swarm (snapshots/var/agg = v8 +
TRADE_EVEN=1 SMALL_LEN=3), plus the user's ladder games. Library stays on for the ladder.
Tools: tools/dumbstats.py (per-log table: loops, stubs, self-hits, friendly head-ons, trades by who started them and
local heads, drops eaten after trades); bench.py --logdir (local games as ladder-format logs); tools/genmap.py
(random symmetric maps; the engine refuses a map whose mirror doesn't match: "pearl bed at .. doesn't match its
mirror", so every real map is exactly symmetric); tools/snap.sh NAME (freeze mybot as snapshots/NAME).
Evidence pooled over FINDINGS (17 games) + every ladder batch + Pizza (3) + devtest (10), not one team:
- Losses since 26 Sep: ~13 eliminations, 8-9 on longest (devtest 22v23, 34v40, 15v53; Pizza 32v56; computers
  26v34, 19v30; BitShift 14v20; abasdasga trophy). In devtest 10/11 we out-ate them (1978v1044, 3328v2081) and lost.
- Carrier fragmentation: from CLAIM_ANY_FROM (330) 13-28 of our 2-4 cell dragons were "carrier" at once (devtest
  10: 23-26 in r325-424), fed by 1-2 pearl sacrifices; our longest at r350 was 4-10 v their 10-20.
- Trades: 267 of 391 devtest head-ons (and 67 of 82 Pizza) were started by them, 149 (46) while they had more heads
  within 3 tiles; drops within 3 tiles in the next 10 rounds went to the side with more heads (devtest outnumbered
  336 v 901, ahead 1072 v 249).
- Contested sources: devil centre field / centre-line farms, default's portal-only centre. Correction: autarky's
  (18-19, 0-1/16-17) pockets are Pizza's HOME pockets (spawn x=18; centre is x=26.5): that loss was home farming
  (theirs ~21 per tile, ours 5-8) plus a raid on our pocket (15 eats), not the centre.
- Self-hits: about half our "hit itself" deaths are U-turn stubs (split <= 2 rounds before), the rest not
  (devtest 7/10/11: 132/154/305); 13 ladder games: loops 5.9% of our moves v 4.6%.
v9a (snapshots/v9a1): symmetry inference (candidates rot180/left-right/top-bottom, dropped on any mismatch of
walls, portals or barren tiles between a seen tile and its seen image; resolved when one is left or all three
match 20+ walls; shared in 2 sonar bits, so every dragon knows it by r50 on devil) and mirroring of walls, portals,
portal pairs, barren tiles and countdown maxima; trade rule (TRADE_MODE: heads in view + enemies seen <= 2 rounds
ago within 5; ahead by 2: dragons <= 3 trade evenly; behind: dodge, no lunges, back off); LOOP_FAST.
First bench (MAPLIB=0, 13+12 maps, 1 seed): v9a1 v v8 0.470 (-4.0 longest); TRADE_MODE=0 0.550 (+1.2);
SYMMETRY=0 0.400 (total -24 v -12); LOOP_FAST=0 0.530. The trade rule as built costs length vs v8.
First symmetry attempt resolved at round 0 on 21 matched barren tiles near the wrap seam (right by luck); barren
tiles now must match but don't confirm.
Trade rule breakdown (v9a1 v v8, 1 seed): no enemy memory 0.427 (41 games); BEHIND_FLEE=0 0.560 / -0.4 longest;
AHEAD_MARGIN=99 (no even trades) 0.410 / total -27. The retreat was what hurt; even trades when ahead help.
v8's own weakness is aggression: agg (v8 + TRADE_EVEN=1 SMALL_LEN=3) beats v9a1 without the trade rule 0.230 (total
-58), and v9a1 with it scores 0.330 (total -31). Shipped: TRADE_MODE=1, BEHIND_FLEE=0.
v9b (snapshots/v9b1): contested boost (CONTEST_W: never-seen tile value x(1 + 2 x closeness to the symmetry line,
where a tile is 2k from its own image), RATE_VALUE (never-seen tiles whose countdown max came by mirror or sonar:
their farms are raid targets), SCOUT_MOD (1 in 8 small dragons go for vision, cross unmapped portals at once),
SOURCE_SHARE (MSG_SOURCE = type 0: two (x, y, gap) per frame, N/S every third round, E/W on even rounds),
LOOP_TABU (last 12 head tiles cost 60, 400 for 10 rounds after a stall: the loops were 2x2 orbits beside a
walled-off target, and giving the target up left the next-best one right beside it).
Devil r0-100, team A head-turns in x 13-18: v9b 845, v8 45. v9b (trade off) v v8, 2 seeds: 0.550, +2.6 longest,
+12 total; lost gen1_08 and gen1_09 0-4 (open, mirror): on gen1_08 we ate 10 v 263 and never reached the centre
fields. Suspect: under a mirror the whole wrap seam is "contested" and our spawns sit on it.
v9c (snapshots/v9c2): GROWER_MOD/GROW_FROM (1 in 5 stop splitting from r280), CLAIM_LATE_LEN (6 after r330),
QUIET_SMALL (no self-beacon below CARRIER_MIN_LEN), LATE_EVEN_MAXLEN (6), CLAIM_GATE: v8's "no carrier heard, any
5+ claims" path bypassed every claim rule; harmless while everyone split to 2+2, it made growers claim (18-39
carrier ids per 50 rounds on gen1_10). Simultaneous carriers from r300: v8 rules 3.97, v9b 5.2, v9c2 2.32.
v9c2 (all of the above, trade rule without retreat) v v8, 2 seeds: 0.570 (won both 17, lost both 10), +0.9 longest,
+26 total.
Contested boost hurt open mirror maps (weak-map set, v9b: 0.179; CONTEST_W=0 0.429); CONTEST_HOME=6 (no boost near
our birth tile) didn't help (0.393 v 0.357). walkableUnseen counted mirrored tiles as frontier, so the v8 gate that
lets every small dragon scout unknown portals once the frontier is exhausted never opened (default: v8 crossed
first in 3 of 4 games); fixed.
Aggressive settings, each on v9c2 (1 seed): SMALL_LEN=3 (small dragons: food before safety) v v8 0.660 / v agg
0.560; AHEAD_MARGIN=1 0.700 / 0.580. Baseline v8 v agg: 0.380 (total -16).
Contested boost with SMALL_LEN=3 (v9c4, 2 seeds v v8): 200 0.650, 100 0.720 (won both 24, lost both 2), 0 0.670.
Final (snapshots/v9 = v9c4 + SMALL_LEN=3, AHEAD_MARGIN=1, CONTEST_W=100; MAPLIB=1 for the ladder):
  MAPLIB=0, 13+12 maps, v v8: 0.760 over 100 (won both 26, lost both 0), +4.6 longest, +46 total;
  v agg: 0.680 over 50 (won both 11, lost both 2), +4.8 longest;
  MAPLIB=1, 13 maps, v v8 (MAPLIB=1): 0.635 over 52 (won both 11, lost both 4), +4.4 longest; schooltime 1-3.
  CPU (--sandbox, MAPLIB=0): slithery_fight max 11.6M points/turn, big_empty max 21.6M (v8 21.1M; limit 100M).
Toolkit: 1.2.2 is out (we run 1.1.0). Same helper.hpp; new maps portals, slithery_fight (maps_extra/: the 63x27
devtest map); engine binary differs, no changelog. tools checked with PYTHONPATH=<unpacked 1.2.2 wheel>.
Bench hygiene: each dragon is a process and the per-user limit is 1333 (kern.maxprocperuid); 8 parallel games of
swarms near the 64-unit cap overflow it ("[Errno 35] Resource temporarily unavailable"), as does a second bench
alongside, or pool workers orphaned by killing bench.py. Use -j 6 and one bench at a time.

## 2026-09-28 — v8 (snapshots/v8, submitted as "sworm v8", server version v19): loops, dead-end farms, regional value, endgame reserve
Ladder (v7 vs "abasdasga", prevBattles game1-4, we were EVEN): autarky lost (10 v 10 longest, 4 v 32 dragons, ate
239 v 803; r0-60 we never went west of x=15, 17 v 64 pearls), default eliminated r298 (they farmed the portal-only
centre, 155 crossings to our 34), queen eliminated r263, trophy eliminated r486 after leading 28 v 13 dragons (fed
everything into one 17-long carrier by r420, it ate 4 pearls in 80 rounds while their 20 dragons ate 150, then died).
Bugs, all general:
- Orbiting: 6-14% of our moves were 2-3 cell dragons circling a 2x2 for 8+ turns (queen 12.5% locally). The target
  was a pearl in a dead-end notch/pocket (queen (6/8/11,33), autarky farm pocket (18-19,0-1), dilemma's farm
  corridors): the safety check won't let a dragon too short to U-turn enter a dead end, but the pearl kept the safe
  moves next to it best. STALL_WIN (8): circling <= max(4, L+1) tiles without growing gives up the chased targets
  (+neighbours) for GIVEUP_ROUNDS (40); being next to the target and refusing the step gives it up at once (GIVEUP_ADJ
  40; 10 brought the loops back: queen 1-5). Loops: queen 12.5% -> 3.3%, autarky 5.6% -> 1.9%.
- Dead-end farms: giving up alone throws away the income the winners live on (the ~1850 team on dilemma farmed its
  corridor at a pearl a round). FARM_ENTRY: a step into a small pocket (<= POCKET_MAX 16 tiles behind the mouth, no
  other dragon seen in it) is survivable when its pearls (seen, or out-of-sight tiles due to have respawned) make us
  >= 4 long, so the existing split/U-turn logic takes us out. Stall+farm vs v7: 0.548 (+2.2 longest, +19 total).
- Target choice ignored regions: a move scored only its best single tile / distance, so one pearl two steps away beat
  a whole untouched region (autarky's west). REGION_W/REGION_AVG/REGION_PEARLS: value = best + 0.5 x the
  discount-weighted (0.85/step of route cost) average expected pearls over the tiles reached. Iterations: a sum with
  the curiosity bonus lost default 1-5 (hundreds of barren tiles); pearls-only sum 0.643 but collapsed trophy/default
  (the sum scales with the amount of ground: trophy growth 483 v 1385); discounting by real steps was worse (0.464);
  the average was balanced. Autarky r0-60 moves west of x=15: 93/28 -> 263/208 (sum version).
- Endgame: KEEP_UNITS (3) never feeds the last dragons, so a lone carrier can't be eliminated. Costs ~0.4-0.8 longest
  in self-play (which never punishes over-feeding); kept as insurance for the trophy loss.
Final (v8 = v7 + all of the above) vs v7, 5 seeds: 0.600 over 140 (won both colours 25, lost both 11, +2.0 longest),
no map below 4-6. CPU max 21.4M points/turn (big_empty, --sandbox; v7 19.3M). Without the reserve: 0.614 over 140
(won both colours 27, lost both 11, +3.5 longest), no map below 4-6. Scenario check vs v7 (v8 as A, 3 seeds):
autarky growth 1515 v 730 (v7 self 835 v 864), loops 1.0%; default 1168 v 475 (392 v 1094), wins 2/3 (0/3);
queen loops 4.5% (12.5%); trophy 3/3 with dragons kept to the end.
Tried and rejected: later feeding (420/440: +50 total length, -0.7 to -1.2 longest), distance-based feeding start
(-1.1 / -2.6 longest), reserve of 5 (-1.6), even trades.
Still open: same-round paired head-on deaths rose on default/trophy in the scenario runs (~100 v 26-46; more of our
dragons alive and moving); queen's portal 1 lands on the south-field notches, where 8 of our 38 ladder crossings
killed two of ours.

## 2026-09-27 — ranked ladder vs "computers" (v16: 1 W, 4 L) -> portal cost + carrier-claim gate
Ladder (prevBattles game1-5, we were EVEN): lost devil (elim r311), queen (elim r289), trophy (26 v 34), default
(19 v 30); won dilemma (elim r130). Every loss: level for 40-80 rounds, then out-bred (r100 14 v 29, 4 v 10, 19 v 30,
8 v 11) and out-eaten (280 v 870, 56 v 302, 442 v 541, 261 v 374), with them flooding our half (devil: 3977 of their
head-turns in our half v our 131 in theirs; they ate 378 pearls there to our 230; centre field 141 v 3).
Fixes (snapshots/v7, submitted 2026-09-27 as "sworm v7", server version v18), each A/B'd vs v17 on 13 maps + portals fixture:
- Carrier claims: a dragon that hears no carrier beacons itself, and hearing that echoed back made it carrier, so each
  sonar-cut-off group elected one: 34-43% of our dragon-turns on default (26-52% queen) were non-splitting carriers
  from round 50. (CARRIER_MIN_LEN never mattered: it only covers silence, and CARRIER_MIN_LEN 8/99 replayed v17 exactly.)
  Now the echo makes it a candidate; before CLAIM_ANY_FROM (330) only the king, an existing carrier or a dragon of
  CLAIM_MIN_LEN (12) claims. Local carrier share on default 23-40% -> 4-9%. vs v17 0.548 at 8/300, 8/300 no king,
  12/330 (+18 to +25 total length per game).
- Blind portal crossings: all 24 friendly head-ons on the ladder involved a crossing (default 10, trophy 12, queen 2),
  plus 19 of ours v theirs; we crossed 260 times on default, they 8. Default's portal-only pockets had our dragons
  hopping pocket to pocket (164 of 268 crossings). PORTAL_COST (8): route choice counts a crossing as 8 extra steps
  (survival checks still use real steps). vs v17 4: 0.595, 8: 0.593 over 140 (won both colours 18, lost both 5,
  +1.8 longest), 12: 0.536, 20: 0.488. All the gain on portal maps (default 8-2, queen 8-2; portal-free maps 5-5).
- Combined (shipped defaults): vs v17 0.607 over 140 (won both 23, lost both 8, +2.9 longest, +18 total; default
  10-0, queen 9-1, stronghold 3-7 and trauma 5-5 noisy across claim settings). vs PORTAL_COST alone 0.521.
  CPU max 15.1M points/turn (schooltime, --sandbox; v17 13.2M).
Tried and rejected:
- fewer lunges (only at enemies >= 8 / >= 6 long): 0.381 / 0.393, -3.1 / -2.7 longest. The trophy reading (36 lunges
  while outnumbered, never at their carrier) was wrong: lunges pay. Even trades from 10 / 20 dragons: 0.500 / 0.488.
- carriers only from round 200 / 300: 0.500 / 0.536; feeding from 300 / 330: 0.500 / 0.452.
- rich-ground attraction (tile value per pearl/round within 2 steps, before round 250): 0.452 / 0.500.
Benchmarking: each seed plays both colours, and when the side decides the game the pair splits 1-1 whatever the bots
do (three identical-to-v17 variants scored exactly 42-42). bench.py now also prints pairs won/lost on both colours and
mean longest/total margins per game; read those, not the raw win rate.

## 2026-09-27 — trapped dragons (DOOMED): diagnosed, not fixed; toolkit 1.1.0
Toolkit: upgraded to unswbc 1.1.0. Pearl respawns are now seeded (random per `unswbc run` unless --seed), so seed 0
does not reproduce 1.0.2 games and single games vary a lot (queen v17 vs v16 went r232 -> r59). `Pool(team=)` became
`Pool(key=)`; bench.py and runlog.py are patched and pass their seed to the engine too. Use several seeds.
Diagnosis (tools/trapstats.py; v17 vs v17, 7 maps x 2 seeds, 14 games): DOOMED is our biggest killer, 756 deaths
(54/game, 2003 cells), 745 of them length 2-3 (too short to U-turn). Boxed in by our own dragons 432 (302 with no
food on the way in), dead ends 284 (272 ate on the way in: dead-end farming), by enemies 38. 546 had already had no
safe move for a turn (TIGHT); 111 ally-boxed ones went straight from a safe move. Concentrated on maze maps
(schooltime 244, devil 161, dilemma 140, autarky 123; big_empty 10). Typical case: devil x=5 is a 1-wide wrap
corridor; two of ours enter from opposite ends, each outside the other's 7x7 vision.
Tried, none shipped:
- small dragons avoid corridors whose way out is out of sight: traps 756 -> 985 / 1157 (dead ends 284 -> 45, but
  ally-boxed 432 -> 892): the swarm packs into the open areas instead.
- AREA_PER_DRAGON 10, SPLIT_CROWD 16: no effect (756 / 764); neither cap binds.
- survival look-ahead >= 8 steps: traps 756 -> 721 (dead ends 284 -> 42, ally-boxed 432 -> 632); vs v17 0.487 (37-39-2).
Reading: most traps are population overflow; close one way of dying and the surplus dies another way. Self-play
can't value fewer self-deaths (both sides overflow alike); game 3's analysis (33 of ours lost in rounds 40-80 on equal
split counts vs a ~1500-elo team) says it matters against real opponents. Needs a non-self opponent to test against.
Debug logs now record who blocks each direction ("who") and each safe move's tier ("TIER"); play is unchanged.

## 2026-09-27 — carrier killed crossing portals; "boxed newborns" (ladder games 3-4, we were ODD)
Carrier: on queen_of_spades (game 4) 5 of 10 head-ons were friendly, all at portal 0's mouths ((16,1)<->(20,31),
(15,1)<->(21,31), inside the pearl fields). A crosser can't see its exit; twice (r152 L9, r181) our carrier crossed
from (20,31) onto a teammate's head at (16,1), both dying. Fix (snapshots/v6-portal-fix-2, submitted 2026-09-27 as server version v17): the carrier treats portals
as walls (`avoidPortals` in `enterable()` and the survival DFS; CARRIER_PORTALS=1 restores v16). Other dragons still
cross. Local vs v16 on the 10 portal maps (incl. the portals fixture), 4 seeds: 45-35; queen 6-2, schooltime 7-1.
Traced queen game: carrier crossings 15 -> 0, carrier head-on deaths 5 -> 1. Portal-free maps replay identically.
Newborns: 14-24% of our newborns die on their first move on the ladder. Locally on devil 177 of 715 did, but 170 of
those were U-turn children (a trapped 4-long dragon splits 2+2 as a last resort; 163 of 292 such children were born
into the same trap), which is break-even (2+2 dead cells drop the same pearls as 4). Ordinary splits lost 7 of 291.
A stricter split gate (count only tiles seen empty that no other head can reach first) was neutral (40-38) and was
dropped. The real leak is dragons getting trapped at all (DOOMED): open.
Still open: small dragons still collide at busy portal mouths (traced queen game: 12 head-on deaths vs 19 before).

## 2026-09-27 — portals on unknown maps: ladder map "portals" vs BitShift (prevBattles/game1.txt)
Symptom: a 32x16 ladder map outside our library (two sealed halves plus 24 portal-only 2x2 pearl pockets, 20 portal
pairs). v6-portal-fix treats portals with no library exit as walls, so our only 26 crossings were DOOMED fallbacks;
BitShift crossed ~244 times, farmed the pockets in our half and out-ate us 587 to 447.
Fix (snapshots/v6-portal-learn, submitted 2026-09-27 as server version v16): portal exits are learned in play. Every portal edge seen is noted by id; the second
edge of an id links the pair (same exit rule as the library, `linkPortal`); a crossing into an unknown exit shows the
partner edge on arrival, and the move is then replayed so the body trail stays known. Pairs are relayed on E/W sonar
on even rounds (MSG_PORTAL, off-library only). Small non-carriers (<= SCOUT_LEN 4) scout unknown portals, but only
once fewer than SCOUT_FRONTIER (8) unseen tiles are walkable: portals are the frontier in a sealed pocket, a detour on
a connected map. Library maps are unchanged (traces byte-identical to v6-portal-fix on trauma, default, schooltime).
Local: `tools/reconstruct_map.py` rebuilt the map from the log as a test fixture (not in the library). There, vs
v6-portal-fix: 9-3; 251 crossings vs 9, 647 pocket head-turns vs 36. All 13 maps with MAPLIB=0 (every map unknown)
vs portals-as-walls: 21-15; sealed maps 13-3, connected maps 8-12 (same as learning without scouting).
Tried and rejected: scouting without the frontier gate (trauma 4-0, but default 0-4, autarky 0-4: 11 of 52 blind
crossings on default died within a round); a "blind corner" penalty keeping cautious dragons off portal mouths
(0.43; default 0-6). Still open: on the fixture all 52 of our head-on deaths were friendly (a crosser can't see its
exit), one of them our carrier; and 3 fixture losses were on longest with more total length (the carrier problem).

## 2026-09-27 — portals: v6 never left its spawn strip on trauma (ladder game vs BitShift, prevBattles/game1.txt)
Symptom: on trauma our 10 dragons spent all 3774 moves in x=0-10 and finished with total length 26 (BitShift: 34 dragons,
total 128, longest 14). Trauma's spawn strips are sealed by kelp (264 tiles, 10 pearl tiles at 1/200); the maze, with the
1-per-round farm (11,10-14),(12,14) right behind our wall, is reachable only through portals, and v6 treated every portal
as a wall (applyMap/readVision folded portal bits into `blocked`).
Fix (snapshots/v6-portal-fix, submitted 2026-09-27 as server version v15): `mem.exitTo` holds each library portal's exit, and every movement lookup goes through
`go(i, d)` (flood fill, survival DFS, threat map, head race, lunges, U-turn room, body chain, head history). Exit rule:
a crossing keeps its direction and leaves the partner edge on its far side; it matches all 22 kinds of crossing
BitShift made, and 0 of 89 of our local crossings landed anywhere else. Portals with an unknown partner (maps outside the
library, or a pair with crossed orientation, which no library map has) stay walls. `PORTALS=0` restores v6.
Local, trauma vs v6: in the maze 5176 of 7777 head-turns (seed 0), total 88-111 vs 19-26, only 1 of 146 deaths
within a round of a crossing. All maps vs v6: 25-27 (noise); portal-free maps replay identically.
**Still losing on longest with far more mass:** trauma 14 vs 20 and 19 vs 23 at 4-5x the total length. In the kelp
maze the carrier beacon doesn't carry (9015 of 15128 ladder pings stopped at kelp), so 5-11 dragons claim carrier at
once (trauma seed 1, rounds 400-450), stop splitting, and split the feeders between them. Not fixed yet.

## 2026-09-26 — v6 (snapshots/v6): back to v4 + top-team splitting; v5 reverted
Ladder: v5 ("sworm v3") did clearly worse than v4 ("sworm v2"); 3-17 vs 龙虎豹 (#1), STAR, NUSW, Vibing++.
What the top teams do (prevBattles, 2026-09-26 batch; we are EVEN, "Average Individuals (AI)"):
- They out-eat us 3-14x on every map (STAR on devil 1636 vs 116; 龙虎豹 on autarky 287 vs 28).
- STAR on devil: split (4 -> 2+2) 613 times, never grows past 4-5, never sprints, loses 657 dragons (walls,
  bodies) with a median lifetime of 11 rounds; breeds faster than it dies. Eats spread over 294 tiles.
- 龙虎豹 on autarky: farms the dead-end farm pockets (both sides') with 2-4 cell dragons; at 4 it splits, the
  tail-half child faces out and leaves, the 2-cell rest eats on and dies at the wall (36 "hit a wall" at len 3).
- 龙虎豹 carrier on big_empty: 27 (r300) -> 68 (r400) -> 108 (end); ours reached 82.
- v5 regressions: the U-turn "child room" gate left our king ping-ponging in the start column all game
  (games 14 and 16: longest stuck at 14 / 17 from r50 to r450); SURVIVE_MIN=12 made small dragons more
  cautious, the opposite of what wins.
- Early devil deaths (game 10): 6 of our dragons DOOMED in the 1-wide x=5 / x=8 loop corridors by round 20,
  two of ours entering from opposite ends. Not fixed yet.
v6 = v4 plus: 2+2 split at 4 even with no safe move (the child is the escape), split crowd limit 16 -> 40,
AREA_PER_DRAGON 10 -> 7 (top teams' density), and the narrow carrier-cascade fix from v5 (big newborn never
halves; handover beacon the child can beat; newer news wins for the same carrier).
Tried, shipped off: "disposable" small dragons (SMALL_LEN, DISPOSE_MODE) that chase food over safety:
0.25 vs v4 on devil/schooltime/trauma (corridor head-to-heads), 0.52 overall.
Local: v6 vs v4 0.519 (27-25) — noise. Self-play can't predict the ladder (v5 was neutral locally too).

## 2026-09-26 — v5 (snapshots/v5): fixes from 19 ladder games with v4 (8 wins, up from 1 in 7)
v4 now matches SSS/Vibing++ on dragon count on open maps. Remaining losses: carriers lost late (games 2, 5, 8,
16), and early eliminations on devil / 16x16 (games 13, 14, 20).
- U-turn cascade: a carrier's U-turn child was not recognised as carrier (the handover beacon said parent-3, but
  takeover needs +3), so it split in half on its first turn, and so did its child: 43 -> 20 -> 10 -> 5 in one round
  (game 8 r207; game 5 r427 did ten splits). Handover now beacons child-3; an older relay of the same carrier no
  longer overrides a newer beacon; a long (>= 8) newborn never does a regular split.
- Ping-pong U-turns: with both ends of a coiled carrier boxed in, each child U-turned again at once (-2 per hop).
  U-turn now needs room at the tail (the child's birthplace) unless no legal move exists; otherwise squeeze on.
  Body-chain walk falls back to the unique adjacent own segment when facings don't link up.
- Carrier lunged from 4 steps (game 2 r436, a 64-long carrier; our model assumed 3). Carrier threat reach is now
  min(visible length - 1, 5), and its penalty (1.2e7) outweighs the safe/unsafe tier.
- Our own lunges may sprint up to min(L-1, 6) steps: we die in the head-on anyway, so any enemy head reachable
  this turn is a sure kill.
- Small dragons dying in kelp dead ends (games 13, 14, 20: most early deaths were 2-3 cell dragons DOOMED): moves
  also need a 12-step path against walls and our own body. Counting other dragons as walls for 12 steps rejected
  nearly every move in a swarm (0.20 vs v4 on the affected maps), so they're ignored in that check.
- Endgame: feeders need a carrier >= 4 longer (was 2x), so four 25-long dragons now consolidate.
Local: 0.558 vs v4 (29-23; 32 games to r500, mean longest 35.0 vs 33.5). Self-play can't show the lunge/cascade
fixes well: v4 never lunges 4+ steps and rarely cascades. Directly checked on schooltime: 0 first-turn U-turns by
big newborns (v4: 19). CPU max 22.3M points/turn (--sandbox, schooltime).
Tried and rejected: blocking every first-turn split (delayed normal breeding; devil/trauma 2-10).

## 2026-09-26 — v4 (snapshots/v4): fixes from 7 ladder losses (prevBattles/game1-7)
Ladder diagnosis (we are always EVEN, sonar tag 0xB7D): out-bred from round 1 (round 50: 2-15x fewer dragons,
1.2-12x fewer pearls eaten than SSS / Vibing++), eliminated in 4 of 7 games.
Local results (tools/bench.py, 13 maps x both colours x 2 seeds): **v4 beats v3 0.827 (43-9)**; beats the
intermediate build 0.712. CPU under --sandbox on big_empty, 62 v 62: max 16.8M points/turn (limit 100M).
Changes, each confirmed in a replay or a bench:
- Breeding: split at 4 into 2+2 (was 6 into 3+3), pop cap from the portal-free reachable area / 10 (sealed maps
  like stronghold hold ~26, not 62), split-crowding limit 8 -> 16 ally segments, split until round 380 (was 330;
  0.654 vs 330 on its own, gains all on long games).
- Round-2 suicide lunges (ladder game 7): every dragon beacons itself as carrier until one exists, so teammates
  "guarded" it with +100 even trades. Guarding now needs a real carrier (round >= 60, length >= 8).
- Carrier churn: at round 90 of big_empty 25 dragons thought they were the carrier; the longest dragon stayed ~7
  until round 350. Now the team's first dragon (id 0/1) is the default carrier ("king"); others take over only when
  3+ longer, or when no carrier is heard and they are >= 5 long.
- Carrier U-turn child ate itself: the 54-long child heard the relayed 56-long beacon, became a feeder and
  sacrificed on its first turn (stronghold, round 470). Feeders must now be <= half the carrier's length and not
  newborn; a U-turning carrier hands over by beaconing a length just under its child's.
- Dead-end farms: a trapped dragon whose next step eats takes the pearl before U-turning, but only if the U-turn
  child (born on our tail) has room; otherwise a dragon spanning a corridor between two dead ends (dilemma x=14,
  autarky x=18) ping-pongs forever at constant length instead of shrinking until it fits out.
- DOOMED fallback rams an enemy head, else tries a portal, never an ally head (devil: 232 of our head-ons were
  friendly).
- SSS beacon decoded (tag 0xB3C0: id, length, x, y, ~2x round; verified against ladder game 6); 1 in 6 small
  dragons hunt SSS's carrier from round 150, and the sighting is relayed on E/W sonar every odd round. Vibing++ pings
  are [0xB74A][round][own id][own length], no position, so they are not used.
Tried and rejected (bench vs same code, 52 games, noise ~ +/-0.07): CONSOLIDATE_FROM 300 (0.45) / 400 (0.52),
FEED_FROM 250 (0.44), AREA_PER_DRAGON 6 (0.50) / 15 (0.48). Assuming enemies leaving our window are 4 long
(RIM_LEN=4) made dragons timid and lost small maps (0.22-0.44), so it ships off.
Still weak: stronghold/trauma (sealed halves, decided by the round-500 carrier; portals are treated as walls, so the
farms behind them are never used), queen_of_spades.
Tools: tools/runlog.py (per-turn trace: position, length, action, indicator, deaths), tools/variant.sh (env-var
parameter variants for bench.py).

## 2026-09-26 — mybot rebuilt from scratch (iteration 3, sent for unranked testing)
The old `mybot/` had no source files (only helper.hpp and a stale binary), so the framework described in
STRATEGY.md was not on disk. The new bot is `mybot/main.cpp` plus the generated `mybot/maplib.hpp`.
Fixed along the way:
- Own segments not reachable along the visible facing chain were not treated as obstacles.
- The flood-fill safety check counted dead-end corridors as room (snakes can't turn round). Now also requires a
  time-aware survival path (DFS), plus a strict tier where other heads race us for tiles.
- Split children know nothing about walls: fixed with a built-in map library (tools/gen_maplib.py). A dragon
  identifies the map from its first view, verified to match the engine on all 13 bundled maps.
- Sonar cross-talk: a copy of this bot on the other team accepted our beacons. Checksum is now keyed per team.
- Lunges could trade the team into elimination: lunges need >= 4 living dragons (guards near our carrier excepted).
Local tuning (tools/bench.py, all maps, both colours, 3 seeds, vs the iteration-2 snapshot) was still noise-level
(~0.44-0.5 win rate). "Hive feeding" (continuous feeding of the carrier from round 120) raised carrier growth
but cost population and lost 0.36 vs v2, so it ships disabled (FEED_FROM=1000).
Open: carriers die to enemies that appear at vision range 3 with a 3-step sprint lunge; small maps are decided
by head-on attrition (dragon count), not length.

## FIXED — 77% self-inflicted deaths in mybot-vs-mybot on a small map
Symptom: `unswbc run <smallmap> mybot mybot -v` produced 284 deaths, 218 of them (77%) "no valid action" —
the engine's label when our own SACRIFICE fallback fires (see nav.hpp / brain.hpp step 4, label STUCK/UTURN).
Team A self-destructed at more than double team B's rate (153 vs 65) despite identical code, and still won
by elimination — likely turn-order compounding (ascending dragon id, so A always acts first each round;
matches FINDINGS.md's "more dragons at round 50 predicts the winner" snowball).

Root causes, both in `bot/brain.hpp`:
1. The U-turn escape (`decide()`, step 4) required `L >= 5`, but `canSplit(L-2)` is legal at L=4 too
   (child 2, parent 2). Length 2-4 is the bulk of the swarm right after any split, so the single most
   common trapped size had no escape. Fixed: gate changed to `L >= 4`.
2. Splitting (`decide()`, step 3) checked that the *move* was momentarily safe but never checked local
   density. With `splitAt=4`, `noSplitAfter=420`, `growMod=8`, nearly the whole team splits constantly with
   only a flat `reserveSlots=3` as a population brake, regardless of how packed the area already is. On a
   small map the population outruns the space and dragons box each other in. Fixed: added
   `maxLocalAllies` (config.hpp) — don't split if more than that many allies already sit within 2 tiles.

Status: both patched and unit-tested (bot_test/), NOT yet re-run against the engine. **Next step: re-run
the same map with `-v`, recount the "no valid action" share, confirm it dropped substantially.** If it's
still high, the population cap itself (not just the crowding check) may need to scale with map area
(W*H), not a flat `reserveSlots`.

## Open: turn-order asymmetry
Even with the crowding bug fixed, team A acting first every round on a symmetric map (self-play) may still
produce a persistent edge (space/resource priority). Worth a dedicated test: run mybot vs mybot on a large
open map (where crowding shouldn't bind) and check if team A still wins more often than chance across
several maps/seeds. If so, it's a structural artifact of ascending-id turn order, not a strategy bug -
relevant to interpreting local test results in general, not just this bot.
