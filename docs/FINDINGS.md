# Findings so far (10 games: two ranked battles, 𓎼𓃭𓅱𓂋𓇌 𓏏𓅱 𓂋𓄿 "GtR" vs SSS)

Sample caveat: everything under "Team profiles" and most of "Strategy" comes from ONE matchup.
Only the "Engine and log facts" section is rules-driven and holds against any opponent.

## Engine and log facts (verified)
- Log line format: `<round>\t<event>`. Events: `dragon N asks to move X`, `moves D to (x,y) [and grows to L]`,
  `asks to split off k cells` / `splits off dragon C, k cells`, `pings V D and reaches dragon M (echo: ...)` or `stops at kelp`,
  `dies. Reason: ...`, `offers nothing the rules accept` (= no valid action), `(x,y) pearl appears|pearl taken|next pearl due in N`.
- Teams are not named in the log. Dragons descending from even root ids are one team (EVEN = owner of dragon 0,
  the team listed first on the site); odd roots are the other. Verified with 100% of ally/enemy echoes in every game.
- **The engine is deterministic.** Same map + same two bots = byte-identical log. `tools/difflogs.py` finds the first
  divergence between two replays of a map, which shows exactly where a team changed its code.
- Pearls dropped on death are logged as `pearl appears` lines immediately after the `dies` line (every second segment from the head).
- A newborn child's first move is logged `grows to k` without eating; only count an eat when a `pearl taken` line precedes it.
- Sprint cost is paid per extra step as the step happens (step 2 onward removes one segment each).
- Head-on log order: victim `dies` first, attacker second.
- Portals exist on several maps (32x32, 48x24, 25x35, 60x40). Vision does not cross them; blind crossings kill.
- A tile holding an uneaten pearl keeps resetting its countdown, so walled-off tiles look like farms. Only count tiles that are actually harvested.

## Map library (maps repeat: 7 distinct maps in 10 games)
| Map | Symmetry | Start | Features | Seen |
|---|---|---|---|---|
| 48x24 | rot180 | 2 x len 4 | kelp maze, portals; 6-tile farm by each spawn: (11,10-14),(12,14) and (35-36,9-13) | battle1 g1, battle2 g3 |
| 64x64 | rot180 | 3 x len 6 | open, no kelp, no portals, pearls everywhere; constant fighting | battle1 g2, battle2 g4 |
| 25x35 | rot180 | 2 x len 3 | maze, portals; all pearls come from two 16-tile fields at (13-18,1-3) and (6-11,31-33) | battle1 g3, battle2 g2 |
| 32x32 | rot180 | 4 x len 4 | 24 portal pairs, heavily used | battle1 g4 |
| 16x16 | rot180 | 2 small | tiny, early contact | battle1 g5 |
| 32x16 | mirror L-R | 3 x len 4 | narrow kelp corridors; real farms on the centre line (14-17,7-8),(15-16,0/15) | battle2 g1 |
| 60x40 | mirror L-R | 3 x len 4 | portals (500+ crossings), pearl field bottom-centre | battle2 g5 |

## Team profiles
**GtR (won 7 of 10)**
- Keeps bigger dragons; selective sprint lunges (length-3 attackers into length 5-9 heads, 3.2:1 length trade on open maps).
- U-turn trick: splits off everything but a 2-cell stub, whose death is the price (145 stubs in one game).
- New version (battle 2): contests farms/fields, hunts the enemy carrier late (shadowed it at 3-4 tiles for 12 rounds,
  struck at round 452), consolidates from ~round 400 by sacrificing large dragons.
- Weaknesses: crowding (dragons boxed in by teammates' bodies), carrier sat idle for its last 29 rounds in one loss,
  sacrificed a length-23 dragon on round 499 (wasted).
- Sonar: round timestamp in high bits + random-looking 18-bit payload, flooded. Not decoded.

**SSS**
- Always splits at length 4 into 2+2; runs near the 64-dragon cap.
- Starts sacrificing on a timer: first sacrifice at round 301-333 in every long game, regardless of map or threat.
  On open maps this collapses its swarm and it lost all three such games.
- Beacon over sonar, plain text: bits 48-63 tag 0xB3C0, 38-47 dragon id, 31-37 length, 25-30 x, 19-24 y,
  9-18 timestamp (about 2 x round), 0-8 nonce. Relays the longest dragon it has heard of. No liveness check
  (dead leader's beacon keeps circulating). Readable by us: in most games our side would receive one almost every
  round after round 300, accurate to ~2-3 tiles. Spoofing untested.
- Heavy portal user and dies there a lot (111 portal deaths in one game).
- Lost a won game (59 vs 27 at round 460) when its carrier coiled into a pocket of its own body and kelp and had no legal move.

## Strategy conclusions
Rules-driven (safe to build on now):
- Head-on kills both regardless of length: lunge small into big; only trade evenly when you have more dragons.
- Death drops ceil(L/2) pearls: consolidation costs ~half the mass; length-3 sacrifices return the most (67%).
- Long dragons must check they can still reach their own tail (or that open space >= length) before every move.
- Treat portals as walls unless the exit is known and recently seen empty.
- Tile countdowns are visible: detect farms/fields, remember them, share them, route dragons there.
- Only sacrifice a dragon if the carrier can reach and eat its pearls before round 500.
- Keep 2-3 tiny insurance dragons alive so you can't be eliminated.

Matchup-derived (treat as tunable parameters, not rules):
- Whoever controls the pearl sources wins maze maps.
- More dragons at round 50 predicted the winner in 8 of 10 games.
- The carrier decided 5 of 10 games (killed twice, self-trapped once, idle once, survived unchallenged once).
- Our own beacon must carry a secret tag and a timestamp; assume opponents read sonar.

## Open questions (need other teams' games)
- Does any top team rush for early elimination, wall off areas with bodies, ambush portal exits, or jam sonar?
- Are tiny-dragon swarms actually optimal, or just what these two teams converged on?
- How many maps are in the pool?
