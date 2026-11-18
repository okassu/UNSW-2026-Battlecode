# Share of dragon-turns (r10-380) spent at length >= 4 (could split) and the length histogram, per side.
import sys, os, glob, collections, pathlib
sys.path.insert(0, "tools")
import ladder, topgames, dumbstats
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
maps = [ladder.load_map(str(p)) for p in sorted(pathlib.Path("maps").glob("*.map"))]
byname = {pathlib.Path(m["path"]).stem: m for m in maps}
def run(log, mp):
    team, length = {}, {}
    for i, b in enumerate(mp["dragons"]): team[i] = i % 2; length[i] = len(b)
    h = {0: collections.Counter(), 1: collections.Counter()}
    for line in open(log, errors="replace"):
        p = line.split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); t = team.setdefault(d, d % 2)
            if len(m[2]) > 1: length[d] = length.get(d, 2) - (len(m[2]) - 1)
            if m[5]: length[d] = int(m[5])
            if 10 <= r < 380: h[t][min(length.get(d, 2), 8)] += 1
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); team[c] = team.get(a, a % 2); length[a] -= k; length[c] = k
    return h
def show(name, H):
    n = sum(H.values()) or 1
    print(f"{name:28s} moves {n:7d}  " + " ".join(f"L{k}{'+' if k == 8 else ''} {100*H[k]/n:4.1f}%" for k in range(2, 9)) + f"  | L>=4 {100*sum(H[k] for k in range(4, 9))/n:.1f}%")
REAL = "portals default dilemma slithery_fight autarky queen_of_spades trophy devil schooltime stronghold".split()
per = collections.defaultdict(collections.Counter)
for log in sorted(glob.glob(sys.argv[1] + "/*.txt")):
    name = next((m for m in REAL if f"-{m}-" in log), None)
    if not name: continue
    ours = None
    for line in open(log):
        x = dumbstats.IND.match(line.split("\t", 1)[-1])
        if x: ours = int(x[1]) % 2; break
    h = run(log, byname[name])
    per[("ours", name)].update(h[ours]); per[("opp", name)].update(h[1 - ours])
    per[("ours", "ALL")].update(h[ours]); per[("opp", "ALL")].update(h[1 - ours])
if len(sys.argv) > 2:
    for i in range(1, 41):
        log = f"topEloBattles/game{i}.txt"
        if os.path.getsize(log) == 0: continue
        mp = ladder.identify(log, maps); a = topgames.analyse(log, mp); h = run(log, mp)
        nm = pathlib.Path(mp["path"]).stem
        for t in (0, 1):
            g = "topW" if a["winner"] == t else "topL"
            per[(g, nm)].update(h[t]); per[(g, "ALL")].update(h[t])
for (g, m) in sorted(per, key=lambda x: (x[1], x[0])): show(f"{m[:14]} {g}", per[(g, m)])
