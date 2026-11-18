import sys, glob, collections
sys.path.insert(0, "tools")
import topgames, dumbstats
MOVE, SPLIT, TAKEN = topgames.RX["move"], topgames.RX["split"], topgames.RX["taken"]
WIN = ((0, 50), (50, 100), (100, 200), (200, 300), (300, 501))
def run(log):
    team, ours, pend = {}, None, set()
    c = collections.defaultdict(collections.Counter)
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        x = dumbstats.IND.match(ev)
        if x:
            if ours is None: ours = int(x[1]) % 2
            continue
        m = TAKEN.match(ev)
        if m: pend.add((int(m[1]), int(m[2]))); continue
        m = SPLIT.match(ev)
        if m: team[int(m[2])] = team.get(int(m[1]), int(m[1]) % 2); c[next(w for w in WIN if w[0] <= r < w[1])][(team[int(m[2])], "split")] += 1; continue
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); t = team.setdefault(d, d % 2); w = next(w for w in WIN if w[0] <= r < w[1]); xy = (int(m[3]), int(m[4]))
            c[w][(t, "move")] += 1
            if m[5] and xy in pend: c[w][(t, "eat")] += 1; pend.discard(xy)
    return ours, c
for name, pat in (("v12 ladder (prev1-20)", "prevBattles/prev*.txt"), ("v12m v v11, lib on", "results/logs-lib13/lib_v12m/*.txt"), ("v13 v v11, lib on", "results/logs-lib13/lib_F_rush/*.txt")):
    tot = collections.defaultdict(collections.Counter)
    for log in glob.glob(pat):
        ours, c = run(log)
        if ours is None: continue
        for w, cc in c.items():
            for (t, k), v in cc.items(): tot[w][("us" if t == ours else "them", k)] += v
    print(name)
    for w in WIN:
        cc = tot[w]
        f = lambda s: f"eats/100mv {100*cc[(s,'eat')]/max(1,cc[(s,'move')]):5.1f} moves {cc[(s,'move')]:6d} splits {cc[(s,'split')]:5d}"
        print(f"   r{w[0]:3d}-{w[1]:3d}  us {f('us')}   them {f('them')}")
