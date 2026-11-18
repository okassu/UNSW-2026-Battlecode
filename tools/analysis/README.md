# One-off analysis scripts (2026-09-29/30 night)

Run from the repo root with `python3 tools/analysis/<script>.py`; their reports are in results/analysis-2026-09-30/.
Most import tools/ladder.py, topgames.py, zones.py and dumbstats.py; paths inside point at prevBattles/ and results/.

- fp.py, fp2.py: sonar and behaviour fingerprints per side of each full ladder log (who is cutlery / devtest).
- macro.py: two-sided macro stats (dragons, eats, splits, sacrifices, zones, trades) for top-team logs v bench logs.
- rates.py, rates2.py: deaths per 100 moves by cause (cutlery v our bench; per map, us v v11).
- sacs.py: who eats the pearls a sacrifice drops (top teams recycle small dragons; own swarm eats 87-97%).
- conv.py: mass and longest dragon at r300-500; how mass turns into the carrier; our dragons' lengths at r499.
- endstate.py DIR ...: per bench log directory, our longest / mass share / dragons marked carrier at r499.
- lens.py: share of dragon-turns at length >= 4 (splittable), us v v11 v top teams.
- labels.py: our last indicator before self/body deaths (DOOMED etc.).
- uturns.py DIR: big-dragon U-turns per map and side.
- eatwin.py: eats per 100 moves by round window, ladder v self-play.
- retake.py: the prev1-20 (v12 ladder) mid-control / retake analysis.
