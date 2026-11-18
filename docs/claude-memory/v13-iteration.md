---
name: v13-iteration
description: "2026-09-29/30 night: v12 submitted (v25); v13 candidate = snapshots/v13; what was tested and rejected; PC port files"
metadata:
  node_type: memory
  type: project
  originSessionId: 805eeb45-73eb-4636-af82-3b5febcf20c6
  modified: 2026-09-30T04:10:15.447Z
---

User direction (2026-09-29 evening): cutlery's play is the same archetype as v12, so **keep iterating v12 until it is
reliable on real AND unseen maps**, keeping every quirk and finding from the top games (FINDINGS.md "Cutlery batch").

- **Submitted**: v12m as "sworm v12" (server v25), 2026-09-29 ~23:15, on "push v12". Its test games (prevBattles/
  prev1-20): 5-15, devtest 2-8; analysis in docs/BUGS.md 2026-09-30 (we out-eat per move but die 3.31 v 2.13 per 100
  moves; losses = out-bred from r100-250, or longer carrier at r500).
- **v13 candidate = snapshots/v13** (mybot defaults): v12m + OPENING_LIB_ONLY 1, MERGE_FROM 400, SEE_CARRIER 1,
  FEED_SEEN 1. Real maps library off v v11u 0.583 (v12m 0.442), generated 0.575 (v12m 0.575), library on v live v11
  0.558 (v12m 0.533). **Submitted as "sworm v13" (server v26) ~01:00 2026-09-30** on the user's "submit now";
  plan: read its ladder test games, roll back to v12m before the Sprint (1 Oct) if it does worse.
- Tested and rejected (params stay at v12 behaviour): carrier back to v11, threat values, SPLIT_ROOM/CONTEST_WALK off,
  soft rush off-library, AHEAD_HUNT, BEACON_ALL_FROM, DISPOSE_STRICT (1e6 and 400), SCOUT_LIB 0 (scouts help).
  DISPOSE_CROWD (strict bonus only in crowds of 3+) was still running at 00:45 on 2026-09-30.
- Pending: roster gate (cand then ab_base = v12m, 720 games each v agg consol early portal raider v9) via
  results/queue6.sh -> results/roster-*.
- **PC port** (user asked: "after everything has concluded, format everything so i can run this on my pc"): drafted
  docs/PC.md, tools/setup_pc.sh (rebuilds from tools/variants.txt), tools/py.sh, tools/run_queue.sh,
  tools/mkvariants.py. Re-run mkvariants at the end; results/*.sh are MacBook-only.

- **2026-09-30 morning:** "sworm v13b" = snapshots/v13i = mybot submitted (server v27) for the user's manual testing
  (v13 + CONSOLIDATE_FROM 300, MERGE_FROM 350, CARRIER_FOOD_RADIUS_END 6). The user then moved to their PC: bundle
  ~/Desktop/battlecode-pc.tar.gz (no results/logs-*, builds or snapshots/var), verified by unpacking + setup_pc.sh
  core + a 2-game gate. Instructions in docs/PC.md; memory copied to docs/claude-memory/. BEACON_TTL 30 test stopped
  at 87/120 (resume on the PC).

- **Then:** the user found v13b worse on the ladder ("lets keep using v13"): v13 resubmitted as v28, mybot defaults
  back to v13. v13 is the Sprint candidate.

- **v14 work (2026-09-30):** user said don't move to the PC, keep iterating v13; they add more ladder games to
  prevBattles/prev54+ for reference only (don't let them block work). Found: 27% of our split parents die within 2
  rounds (devtest ~15%), mostly DOOMED. SPLIT_PARENT_SAFE (snapshots/v14a, param off by default in mybot): real maps
  library off 0.617 +3.40 v v13's 0.583 (19 better, 15 worse); library-on and generated runs in results/lib13-sp_lib,
  val13-sp_u. Plan: if they hold, bake it in and ask the user for ladder games of it.

- **v14 candidate (2026-09-30 ~12:00) = snapshots/v14 = mybot**: v13 + SPLIT_PARENT_SAFE 3 (mode 1 with the library,
  mode 2 without). prev54-83 were 30 more v13 games (v13 total 23-36-1; autarky 0-6, dilemma 1-4). Waiting on the
  user for ladder games of v14 (they submit or tell me to).

- **v14 submitted (server v29) ~12:10 2026-09-30; its ladder games prev84-147: 30-33-1 (v13 23-36-1).** User: "analyse
  and start on v15". v15 so far (mybot, params off unless noted): CARRIER_DEPTH (no effect), CLAIM_LATE_LEN 10 (worse),
  CARRIER_REACH 6 (neutral offline, in for v15: 4/17 carrier kills were 4-5 step sprints), LUNGE_MAX, HOME_MOD
  (home guards v trophy raids) - tests in results/*-ln5*, *-hg3*.

- **v15 = snapshots/v15 = mybot (v14 + CARRIER_REACH 6 + LUNGE_MAX 5) submitted as server v30 ~13:55 2026-09-30.**
  HOME_MOD rejected (library off 0.483). Combined offline check running (results/ab-v15u, lib13-v15lib).

**How to apply:** compare with tools/abcmp.py; don't kill gate runs to reorder.
Related: [[cutlery-games]], [[generalise-strategy]], [[test-harness-pending]].
