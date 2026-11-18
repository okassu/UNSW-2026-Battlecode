"""Head-on deaths of long dragons (10+) on both sides, with the killing move's step count, from logs with our
indicators. Usage: python3 tools/analysis/carrierkills.py LOG ..."""
import sys, re, collections, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import dumbstats, topgames
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
ASK = re.compile(r"dragon (\d+) asks to move ([NESW]+)")
tot = collections.Counter(); games = 0
for log in sys.argv[1:]:
    games += 1
    lab, length, ask, team = {}, {}, {}, {}
    ours = None; dead = collections.defaultdict(list)
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        x = dumbstats.IND.match(ev)
        if x:
            d = int(x[1]); ours = d % 2 if ours is None else ours; lab[d] = x[2]; continue
        m = ASK.match(ev)
        if m: ask[(int(m[1]), r)] = m[2]; continue
        m = MOVE.match(ev)
        if m:
            team.setdefault(int(m[1]), int(m[1]) % 2)
            if m[5]: length[int(m[1])] = int(m[5])
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); team[c] = team.get(a, a % 2); length[a] = length.get(a, 0) - k; length[c] = k; continue
        m = DIE.match(ev)
        if m and m[2] == "head-on collision": dead[r].append(int(m[1]))
    for r, ds in dead.items():
        for d in ds:
            if length.get(d, 0) < 10: continue
            mine = team.get(d, d % 2) == ours
            others = [e for e in ds if e != d and team.get(e, e % 2) != team.get(d, d % 2)]
            steps = max((len(ask.get((e, r), "")) for e in others), default=0)
            tot[("our" if mine else "their", "long dead")] += 1
            tot[("our" if mine else "their", f"killer steps {min(steps, 6)}")] += 1
print(f"{games} games")
for side in ("our", "their"):
    n = tot[(side, "long dead")]
    print(f"  {side} 10+ dragons killed head-on: {n} ({n / max(1, games):.2f} per game); killer steps: " +
          " ".join(f"{s}:{tot[(side, f'killer steps {s}')]}" for s in range(0, 7) if tot[(side, f'killer steps {s}')]))
