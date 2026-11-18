# Findings so far (17 games: battles 1-2 GtR vs SSS, plus matches 1-7 of 2026-09-25)

Sample caveat: battles 1-2 (10 games) were one matchup, 𓎼𓃭𓅱𓂋𓇌 𓏏𓅱 𓂋𓄿 "GtR" vs SSS. Matches 1-7 (`replays/game1-7.txt`,
no headers) add at least four more teams, which are named here only by their sonar fingerprint; team identities in
that batch are inferred, not confirmed. Only the "Engine and log facts" section is rules-driven and holds against any opponent.

## Engine and log facts (verified)
- Log line format: `<round>\t<event>`. Events: `dragon N asks to move X`, `moves D to (x,y) [and grows to L]`,
  `asks to split off k cells` / `splits off dragon C, k cells`, `pings V D and reaches dragon M (echo: ...)` or `stops at kelp`,
  `dies. Reason: ...`, `offers nothing the rules accept` (= no valid action), `(x,y) pearl appears|pearl taken|next pearl due in N`.
- Death reasons seen: `head-on collision`, `hit itself`, `hit a body`, `hit a wall`, `gave no valid action`
  (the last one is how sacrifices show up: the dragon `offers nothing the rules accept`).
- Teams are not named in the log. Dragons descending from even root ids are one team (EVEN = owner of dragon 0,
  the team listed first on the site); odd roots are the other. Verified with 100% of ally/enemy echoes in every game.
- **The engine is deterministic for a given seed.** Since unswbc 1.1.0 (2026-09-27) pearl respawn timing is seeded;
  `unswbc run` picks a random seed unless given `--seed`, so ladder games likely differ in pearl timing, and seed 0 on
  1.1.0 does not reproduce 1.0.2 games. Same map + bots + seed = byte-identical log. `tools/bench.py -s N` and
  `tools/runlog.py ... SEED` pass the seed to the engine. `tools/difflogs.py` finds the first divergence between two
  replays of a map (only meaningful at the same seed).
- **Elimination ends the game at once** (matches 3 and 7 ended at rounds 147 and 144). Otherwise the longest dragon at
  round 500 wins, not total length: the losers of matches 1, 2 and 5 had MORE total length (59 vs 45, 120 vs 113, 54 vs 48).
- Pearls dropped on death are logged as `pearl appears` lines immediately after the `dies` line (every second segment from the head).
- A newborn child's first move is logged `grows to k` without eating; only count an eat when a `pearl taken` line precedes it.
- Sprint cost is paid per extra step as the step happens (step 2 onward removes one segment each).
- Head-on log order: victim `dies` first, attacker second.
- Portals exist on most maps (32x32, 48x24, 25x35, 60x40, 25x25, 54x18, 32x16 rot180). Vision does not cross them; blind crossings kill.
- Portal exit rule: a crossing keeps its direction and comes out of the partner edge (same id) on its far side, e.g. trauma
  (10,0) E -> (16,7) and (16,7) W -> (10,0). Verified on all 22 kinds of crossing in a trauma ladder game (2026-09-27);
  every library map pairs edges two by two with matching orientation.
- Trauma and stronghold spawn strips (x 0-10 / 37-47) are sealed by kelp: the maze and its farms are reachable only through portals.
- A portal crossing is blind (the exit is never in vision), so crossings into busy areas kill in pairs: every friendly
  head-on in the 2026-09-27 ladder batch was a crossing. Crossing only when it saves many steps (PORTAL_COST) paid.
- The ladder uses maps outside the bundled 13 (toolkit 1.1.0 ships the same 13): "portals", 32x16 rot180, 3 dragons of
  length 3 per side, two sealed halves plus 24 portal-only 2x2 pearl pockets, 20 portal pairs. The exit rule held
  for every crossing there too. A rebuild from the log: `tools/reconstruct_map.py`.
- A tile holding an uneaten pearl keeps resetting its countdown, so walled-off tiles look like farms. Only count tiles that are actually harvested.
- Judge map symmetry from start positions (dragon 0 vs dragon 1 on round 1), not from pearl tiles: dropped pearls
  aren't symmetric and skew any tile-set test.

## Map library (maps repeat: 10 distinct maps in 17 games)
| Map | Symmetry | Start | Features | Seen |
|---|---|---|---|---|
| 48x24 | rot180 | 2 x len 4 | kelp maze, portals; 6-tile farm by each spawn: (11,10-14),(12,14) and (35-36,9-13). In match 5 the east farm was never entered in 500 rounds although dragons passed next to it 30+ times; it may only be reachable through a portal | battle1 g1, battle2 g3, match 5 |
| 64x64 | rot180 | 3 x len 6 | open, no kelp, no portals, pearls everywhere; constant fighting | battle1 g2, battle2 g4 |
| 25x35 | rot180 | 2 x len 3 | maze, portals; all pearls come from two 16-tile fields at (13-18,1-3) and (6-11,31-33) | battle1 g3, battle2 g2, match 6 |
| 32x32 | rot180 | 4 x len 4 | 24 portal pairs, heavily used (593 crossings in match 4) | battle1 g4, match 4 |
| 16x16 | rot180 | 2 small | tiny, early contact | battle1 g5 |
| 32x16 A | mirror L-R | 3 x len 4 | narrow kelp corridors; farms (15-16,0/15),(17,7), centre-line field (14-17,5-10), outlying field tiles (8,0),(8,15),(11,0),(20,0),(23,0),(23,15),(26,0),(26,15) | battle2 g1, match 3 |
| 60x40 | mirror L-R | 3 x len 4 | portals (500+ crossings), pearl field bottom-centre | battle2 g5 |
| 32x16 B | rot180 | 3, uneven (longest 12) | kelp maze, few portal crossings (7); farms at (14,0-2),(17,0-2) and mirrors (14/17,13-15); dragons 0 and 1 spawn on them at (14,2),(17,13) | match 1 |
| 54x18 | rot180 | 6, uneven (longest 15) | kelp maze, 10 portal crossings; farms (18,0-1),(18,16-17),(35,0),(35,17), fields (19,1),(34,16) | match 2 |
| 25x25 | mirror L-R | 2 x len 3 | fairly open (87% of edges crossed); one 11-tile centre field (10-14,7-10); two portal pairs on x=12: (12,5)<->(12,22), (12,6)<->(12,21) | match 7 |

## Team profiles
**GtR (battles 1-2, won 7 of 10 vs SSS; not identified with certainty in matches 1-7, see RND)**
- Keeps bigger dragons; selective sprint lunges (length-3 attackers into length 5-9 heads, 3.2:1 length trade on open maps).
- U-turn trick: splits off everything but a 2-cell stub, whose death is the price (145 stubs in one game).
- New version (battle 2): contests farms/fields, hunts the enemy carrier late (shadowed it at 3-4 tiles for 12 rounds,
  struck at round 452), consolidates from ~round 400 by sacrificing large dragons.
- Weaknesses: crowding (dragons boxed in by teammates' bodies), carrier sat idle for its last 29 rounds in one loss,
  sacrificed a length-23 dragon on round 499 (wasted).
- Sonar: round timestamp in high bits + random-looking 18-bit payload, flooded. Not decoded.

**SSS (lost 7 of 10 to GtR; won matches 4, 5, 6 and very likely 7)**
- Splits at length 4 into 2+2 and runs near the 64-dragon cap (this opening turns out to be common, see Strategy).
- Starts sacrificing on a timer: first sacrifice at round 301-333 in battles 1-2, and at 309/302/306 in matches 4-6.
  On open maps this collapses its swarm and it lost all three such games; none of matches 4-6 was an open map, and it won all three.
- Beacon over sonar, plain text: bits 48-63 tag 0xB3C0, 38-47 dragon id, 31-37 length, 25-30 x, 19-24 y,
  9-18 timestamp (about 2 x round), 0-8 nonce. Relays the longest dragon it has heard of. No liveness check
  (dead leader's beacon keeps circulating; seen again in match 6). **The beacon starts at exactly round 151** (matches 4, 5, 6),
  accurate to ~2-3 tiles. Spoofing untested.
- Second signal: a constant ping value 1403067405 (0x53A11C0D), sent only E/W as a terrain probe, from round 8-22 onward
  (1-2% of its pings). **SSS can be identified in-game by round ~22.**
- Match 6: invaded ODD's home field by ~round 150 and held it for 350 rounds (0 ODD field eats), killing 11 ODD dragons
  head-on in rounds 100-200 (ODD fell from 10 dragons to 2).
- Match 7 (inferred SSS: same constant ping, 2+2 splits, game ended before round 151): took the centre field 63 pearls to 3,
  started head-ons only when ahead on dragon count (19 of 19), and eliminated ODD at round 144.
- Heavy portal user and dies there a lot (111 portal deaths in one game; 48 in match 4, 22 in match 6).
- Weakness: its own swarm crowds itself. 45 and 35 `hit a body` deaths in matches 4 and 6, including a length-15 dragon
  killed by an ally's body; 3 friendly head-ons once it passed 40 dragons in match 7.
- Lost a won game (59 vs 27 at round 460) when its carrier coiled into a pocket of its own body and kelp and had no legal move.

**RND, random-sonar team (EVEN in matches 1-2: won both; ODD in matches 4-6: lost all three to SSS)**
- Sonar is silent until round 333-369, then uniform 64-bit random values (every bit ~50% set, no correlation with round,
  a different value on each direction). Can't be decoded; probably encrypted. **Identifiable by its silence until ~round 335.**
- U-turn stubs disposed of against walls: 93, 57 and 30 of its splits (matches 1, 2, 5) are followed within 2 rounds by the
  2-cell parent stub dying with `hit a wall`. These are deliberate, not a pathing bug.
- Consolidates at exactly round 401: first sacrifice on round 401 in matches 1, 2 and 4 (426 in match 5, none in match 6).
  Swarms to ~63 dragons on 54x18 and 36 on 32x16, then collapses into one carrier (41, 33).
- Against SSS it lost the carrier race: in match 5 its eventual carrier was born on round 427 and reached 17 (SSS's was born
  on round 248 and reached 27); longest 14 in matches 4 and 6. Lost field control in match 6, and never entered its own home farm in match 5.
- Could be GtR's current version (U-turn stubs and ~round 400 consolidation match), but its sonar doesn't match GtR's documented format.

**B60, bit-60 sonar team (ODD in matches 1-2, lost both)**
- Caps every dragon with U-turn splits at a map-dependent length: at 13-14 on 32x16 B (splits off 12), at 18 on 54x18
  (splits off 16). The 2-cell stub dies `hit itself` (248 and 258 stubs). Longest never exceeded 14 / 18 all game.
- Never consolidates: finished with 22-33 dragons and more total length than the winner, and lost on longest dragon both times
  (13 vs 33, 18 vs 41). Barely fights (0 head-ons in match 1, 10 in match 2); 0 portal crossings in match 2.
- Sonar from round 1, same value on all 4 directions; bits 48-63 are 0; the value is ~round x 2^25 plus ~20 bits of jitter
  (a per-dragon round counter). In rounds 401-500 only, bit 60 is set on pings from dragons that just reached the cap length
  (exactly 600 such pings in both games). **Identifiable on round 1.**

**Silent teams (match 3 both sides, match 7 ODD): no sonar at all**
- Match 3 (32x16 A): both sides split 4 into 2+2 and never grew a dragon past length 4. The winner reached the centre farm one
  round earlier (round 19 vs 20), ate 193 farm/field pearls to 25 by round 100, split 149 times to 33, and eliminated the other side on round 147.
  Head-on trades were even (1.3 length ratio both ways); it won by out-breeding.
- The match-3 winner took 71% of its deaths from its own mistakes (61 boxed in by its own bodies; 18 walked into the same walls
  from the same farm tiles again and again) and it barely mattered: its swarm re-ate 147 of its own dropped pearls.
- Match 7 ODD: half the splits of SSS, avoided portals, lost the field 3 pearls to 63.

## Strategy conclusions
Rules-driven (safe to build on now):
- Head-on kills both regardless of length: lunge small into big; only trade evenly when you have more dragons
  (match 7's winner started 19 head-ons, all while ahead on dragon count).
- Death drops ceil(L/2) pearls: consolidation costs ~half the mass; length-3 sacrifices return the most (67%).
- Long dragons must check they can still reach their own tail (or that open space >= length) before every move.
- Treat portals as walls unless the exit is known and recently seen empty.
- Tile countdowns are visible: detect farms/fields, remember them, share them, route dragons there.
- Only sacrifice a dragon if the carrier can reach and eat its pearls before round 500.
- A carrier is mandatory: the longest dragon wins at round 500, and three losers in matches 1-7 had more total length.
- Keep 2-3 tiny insurance dragons alive so you can't be eliminated. Elimination happened on rounds 144 and 147 on small maps
  with 2-3 starting dragons, so this applies from the opening, not only in the endgame.
- U-turn by split: when a head is stuck, split the rest off as a new dragon and let the 2-cell stub die
  (costs 2 cells, returns 1 pearl). Three teams do this routinely.
- Sonar fingerprints identify opponents early: SSS by round ~22 (constant 0x53A11C0D), B60 on round 1 (bit-60/counter format),
  RND by silence until ~round 335. That is enough to pick matchup parameters in-game.

Matchup-derived (treat as tunable parameters, not rules):
- Whoever controls the pearl sources wins: true on the maze maps of battles 1-2 and in every game of matches 1-7 where one side
  clearly held them (matches 3, 5, 6, 7). In match 3 a one-round lead to the farm snowballed into elimination.
- More dragons at round 50 predicted the winner in 14 of 17 games (match 6 was the exception: SSS came back by taking ODD's field).
- The carrier decided 5 of 10 games in battles 1-2 (killed twice, self-trapped once, idle once, survived unchallenged once).
  In matches 1, 2, 4, 5 and 6 the loser's longest dragon was only 13-18.
- Sacrifice timing that has won: SSS from ~round 305, RND from round 401. Starting at 426 (RND, match 5) was too late.
- Splitting at length 4 into 2+2 is the common opening (SSS, RND and both match-3 teams), not an SSS signature.
- Self-inflicted deaths inside your own swarm are cheap if the drops get re-eaten (match 3: 147 own-drop eats).
- Our own beacon must carry a secret tag and a timestamp; assume opponents read sonar.

## Top ladder games 21-40 (2026-09-29, full logs, no team names)
Tools: `tools/topgames.py` (two-sided macro table + sonar fingerprint), `tools/zones.py` (eats by home / contested /
enemy ground and window), `tools/dumbstats.py --` with `side_override` for logs without our indicators.
Teams by sonar fingerprint:
- **R1** (games 21-30, one side each): pings 0.3-1.5 per dragon-round, one direction, random-looking 64-bit values
  (encrypted). Went 7-3 against Z. Disposes of U-turn stubs by walking them into walls.
- **Z** (games 21-30, the other side): `SONAR 0` broadcast on all 4 directions every round (a pure echo scanner); later
  build mixes in coded values. Consolidates by sacrifice from exactly round 251; won 3 of the 5 games that reached r500.
- Games 31-35: an `0xF5E...` team (2-3) against teams with `0x....000000000003/6` and coded values. Games 36-40: an
  `0x1`-constant team against a random 1-3 direction team (3-2).
What winners do (20 games, every team):
- **Rush contested ground in the opening.** Devil: R1 had 5 of 6 heads on contested ground at r25 (Z: 0) and ate
  28 then 92 pearls there in r25-50 / r50-100, while Z ate only its home (7/14/7). Trophy winners ate 17-18 contested
  pearls in r25-50 against 1-4. R1 skipped its home pearls entirely on devil (1 home eat in r0-25).
  Our v12 in self-play: 7 home / 0 contested at r25 on devil. With OPENING_ROUNDS=40: centre eats 42 v 0 in r25-50 and
  4 of 4 wins vs v11 on devil + trophy (library on).
- **Out-breed**: more dragons at r50 in 16 of 20 games, more eats at r50 in 15 of 20. Everybody splits 4 -> 2+2 and runs
  near the 64 cap; longest dragon at r200 was <= 7 in 19 of 20 games (the exception won on portals with a carrier born r16).
- **Trade only when locally ahead**: winners started 265 head-ons ahead, 114 even, 63 behind (4.2:1); losers 169/116/80
  (2.1:1). Ours in self-play: 47/30/24 (2:1), i.e. loser-like.
- Small maps end by elimination (r56-366 in 10 of 20 games); on big maps the carrier is built late (final carriers born
  r300-470, 24-62 long at r500) by sacrifices from r251 (Z), r401 (0xF5E) or by eating.
- Nobody sprints (0 multi-step moves in 20 games).
Sonar: the pure scanner (Z) lost 7 of 10 to R1, which pings little. No evidence yet that echo scanning itself wins.

## Cutlery batch: 39 games of the rank-1 team (topEloBattles/game1-40, 2026-09-29 evening; game3 empty)
These logs replaced the games 21-40 above. Full logs, no names. Cutlery = the side whose pings most often go in 2
directions in one round (2.0-2.9 pings per dragon-round, 1d/2d/3d mix, rarely 4, random 64-bit values) and that gives
"no valid action" from r1-30; it is the one team present in every game. Cutlery went 22-17; by blocks of five: 1-5 v a
one-direction pinger (repeated values) 2-2; 6-10 v a 4-direction pinger with tagged low bits (...03/...06/...02) 3-2;
11-15 v a 4-direction random pinger 0-5 (cutlery's values had repeated top bits there: likely an older build);
16-20 v a 3-direction pinger 4-1; 21-25 v the 4-direction random team 4-1; 26-30 v the `0xF5E...` team 2-3; 31-35
v an `0xA...` low-rate team 4-1; 36-40 v a ~1 ping/dragon-round 3-4 direction team 3-2. Silent opponents (games 17,
33, 37) lost every game. Tools: the side rule and the tables below came from scratch scripts on top of
`tools/topgames.py`, `tools/zones.py` and `dumbstats.analyse(..., side_override=)`.
What winners do (39 games, both sides, every team):
- **Out-breed and out-eat early**: the winner split more in 35 of 39 games, ate more by r50 in 33, had more dragons at
  r100 in 32 and at r50 in 29. Big-map (r500) winners: 34 dragons at r100, 43 at r200, 558 eats by r200, 470 splits;
  losers 27 / 33 / 397 / 325. Elimination winners (small maps) had 23 dragons at r100 against the loser's 7.
- **Trade a lot, not only when ahead** (this batch): winners started 423 head-ons locally ahead, 296 even, 345 behind;
  losers 242 / 229 / 311. The earlier batch's 4.2:1 ratio does not hold here; what holds is that winners fight more.
- **Recycle small dragons**: r500 winners had ~270 "no valid action" deaths per game (131 in r50-250, 122 after r250);
  losers ~115. 65% were 2-cell dragons and 23% 3-cell; 62% had a smaller ally head within 3 tiles and only 10% a
  dragon of 8+. Own small dragons ate 87-97% of the dropped pearls, 5-7 rounds later; the enemy got 0-1%.
  So this is not carrier feeding: they die inside their own swarm and the swarm re-eats them (see match 3 above).
- **Carriers**: r500 winners' longest was 8.6 at r200, 13 at r300, 51 at r500 (losers 6 / 9 / 33); final carriers
  born ~r347 for both. Across every map the r500 winner's longest was 38-64.
- Contested share of eats in r0-25 did not predict the winner here (higher for the winner in 11, lower in 9); the r500
  winners reached contested ground first (first contested eat r10 v r19).
- Nobody sprints (0 multi-step moves in 39 games). First split: r1-2 on most maps, r8 on portals, r13-30 on
  trophy/queen_of_spades (starting length).
Survival, per 100 moves (cutlery on the ladder v our v12l in self-play on the same 10 maps): hit itself (not a U-turn
stub) 0.00 v 1.19, no valid action 1.25 v 0.20 (both drop ceil(L/2), so partly bookkeeping: cutlery chooses no action
where we step into ourselves), hit a body 0.04 v 0.37, friendly head-on 0.02 v 0.18, death within 2 rounds of a portal
crossing 0.06 v 0.45. Trapped deaths in all: <= 1.29 v 1.76. Portals is our worst map (3.59 self-hits, 2.85 crossing
deaths). Our self-hits and body hits with an indicator line were all DOOMED (no legal move): walking into dead ends.
Our economy on the same real maps (self-play, library off, v12l v v11): v12l split less than v11 on most maps (autarky 99
v 201, default 76 v 124, trophy 59 v 102) and had fewer dragons at r200 (autarky 12 v 26, default 15 v 25, trophy 16 v 27);
top winners on those maps: 243 / 169 / 124 splits.

## Open questions
- Early elimination: answered, yes twice (rounds 144 and 147), but by out-breeding after winning the pearl source,
  not by a planned rush. Body walls, portal ambushes and sonar jamming: still not seen.
- Is RND the current GtR? A battle with a `#` header naming the teams would settle it.
- Is the 48x24 east farm reachable except through a portal?
- Are tiny-dragon swarms optimal? Every team seen so far swarms; B60 caps its dragons at 14-18 and lost both games.
- How many maps are in the pool? 10 distinct maps in 17 games; 4 of the 7 new games were on library maps.
