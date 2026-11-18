# Dragon bot framework (C++)

A strategy framework built from the replay findings (10 games, GtR vs SSS). It's a complete, compiling bot,
but a starting point: every matchup-dependent choice is a named parameter in `config.hpp`, meant to be tuned
with local test matches.

## Files
| File | Role |
|---|---|
| `main.cpp` | The turn loop. Replaces the `main.cpp` from `unswbc init`. |
| `world.hpp` | The **only** file that calls the unswbc helper. Converts it into plain structs. |
| `geom.hpp` | Wrapping grid, directions, hashing. |
| `config.hpp` | All tunable parameters, with the finding behind each default. |
| `memory.hpp` | What one dragon remembers: tiles seen, pearl timers, spawn rates, kelp, portal pairs. |
| `nav.hpp` | Safety and pathing: time-aware flood fill, BFS, enemy threat map, lunge search. |
| `comms.hpp` | Our 64-bit sonar format with a secret tag, plus the SSS beacon decoder. |
| `maplib.hpp` | Farm/field tiles on maps already seen on the ladder. |
| `brain.hpp` | Roles and the per-turn decision. |

## Setup
1. `unswbc init` → choose C++. Copy everything in `bot/` into the project folder next to its `helper.hpp`,
   replacing `main.cpp`. Keep the `test/` folder **outside** the project (it has a stub `helper.hpp` and its own `main`).
2. Change `SECRET` in `comms.hpp` and never publish it.
3. `unswbc maps`, then `unswbc run maps/<map>.map mybot mybot -v`. Add `--sandbox` to check CPU cost.
4. If the first real build fails, the fix is almost certainly a helper name in `world.hpp`. It was compile-checked
   against a stub written from the Helper reference page, not your actual `helper.hpp`
   (most likely suspect: the field names of `SonarEchoes`).

Offline checks (outside the project): `g++ -std=c++20 -Itest -Ibot test/test_main.cpp -o t && ./t`

## Turn priority (brain.hpp)
Each turn a dragon picks one action, first match wins:
1. **Lunge.** Sprint into a visible enemy head that is at least `myLen + lungeMargin` long, or is the known
   enemy carrier. A head-on kills both regardless of length, so small-into-big is always a good trade.
   Carriers, insurance dragons and anything longer than `maxLungeLen` never lunge.
2. **Sacrifice** (feeders only). Within `feedRadius` of the carrier with enough rounds left for the carrier to
   eat the drops: output no action, which the engine treats as death, dropping ceil(L/2) pearls.
3. **Split** (growers, before `noSplitAfter`). At `splitAt` (4) into 2+2, as both top teams do.
   One dragon in `growMod` is a "big grower" that keeps its length, so carrier candidates exist before the endgame.
4. **Escape when trapped.** No safe move and length ≥ 5: U-turn split (GtR's trick). The old tail becomes a new
   dragon facing out; the 2-cell stub is lost.
5. **Move** to the best target, using only safe first steps.

## Safety rules (nav.hpp) — rules-driven, not tunable guesses
- A move is **safe** if the flood fill from the destination reaches at least `min(len + spaceMargin, spaceCap)`
  cells, or reaches our own tail. The flood fill is time-aware: our own segment `i` frees up on step `L - i + 1`
  (the tail blocks step 1, as the rules say). This is the check that would have saved SSS's 59-long carrier.
- Our body beyond vision comes from remembered head positions (segment i = where the head was i steps ago).
- **Portals are walls** unless the exit is known (learned from our own crossing or from two edges sharing an id)
  and that exit tile is in vision this turn and empty. Blind crossings killed 100+ dragons a game in replays.
- **Threat map:** an enemy of length Le can put its head anywhere within Le−1 steps next action. Dragons of length
  ≥ `avoidThreatLen`, carriers and insurance stay out of those cells when they have any alternative.
- Turn order is handled for free: we observe after lower ids have moved, so teammates never pick the same cell.

## Economy (brain.hpp targeting)
Scores every BFS-reachable cell, divided by distance + 1:
- pearl present or seen within `pearlMemory` rounds: `wPearl`
- tile whose countdown says it spawns by the time we arrive: `wSpawn`, x`farmBonus` for tiles resetting every
  ≤ 2 rounds, x`fieldBonus` for ≤ 40 (rates are measured from observed countdown resets)
- unvisited map-library tile: `wPreset` (the hint is dropped once the tile is seen never to spawn)
- if a teammate's head is closer to the target: x`sharedPenalty`, so the swarm spreads
- nothing known: explore stale/unseen tiles, biased by a per-dragon heading

## Roles
| Role | Who | Behaviour |
|---|---|---|
| grower | default | farm, split, lunge when worth it |
| CARRIER | longest known dragon, length ≥ `minCarrierLen` | never lunges or splits, avoids threats, always eating |
| feeder | endgame (`consolidateRounds` before the end), if it can reach the carrier in time | walk to the carrier, sacrifice beside it |
| insure | endgame, ~1 in `insuranceMod` | avoid fights, never sacrifice, so we can't be eliminated |
| hunt | endgame, ~1 in `hunterMod`, when enemy carrier intel exists | walk to the enemy carrier, lunge on sight |

## Sonar (comms.hpp)
Four rays a turn: N and S carry the carrier beacon (id, length, x, y, round); E and W rotate facts (enemy-carrier
position, productive tiles, portal edges). A 16-bit tag keyed on `SECRET` rejects enemy and forged messages.
**Liveness:** only the carrier refreshes its beacon's round, so a dead carrier's beacon expires after `beaconTTL`.
That's the flaw that broke SSS in battle 1 game 4. Readable SSS beacons (tag 0xB3C0) are decoded into enemy-carrier
intel when `decodeSSS` is on.

## Which parts are solid and which are guesses
Solid (rules-driven): flood-fill safety, tail reachability, portal caution, head-on arithmetic, sacrifice
arithmetic, beacon liveness.

Guesses from one matchup, tune them: `consolidateRounds`, `lungeMargin`, `maxLungeLen`, `growMod`, `splitAt`,
`noSplitAfter`, `avoidThreatLen`, all target weights.

## Known gaps (next things to build)
- Enemy length is a lower bound (visible segments only), so lunges are conservative.
- No even trades when we clearly outnumber the enemy (GtR's battle-2 behaviour): needs an enemy-count estimate.
- Sprint safety uses single-step body timing (conservative); flood fill ignores portals.
- Feeders sacrifice at whatever length they are; length 3 returns the most per dragon.
- The carrier's beacon position can be a few rounds stale; feeders may sacrifice slightly off target.
- Map recognition is by size only; confirm against kelp at round 1 before trusting presets.

## Testing plan
Make opponent variants by copying the project and editing `config.hpp`, then `unswbc run -v` them against each
other on every bundled map:
- **SSS-like:** `consolidateRounds = 200` (sacrifices from round 300), `growMod = 1000` (no big growers).
- **Passive:** `lunges = false`, `hunt = false`.
The engine is deterministic, so any difference between two runs comes from the code change you made.
