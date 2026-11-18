import sys, glob, collections
sys.path.insert(0, "tools")
import topgames, dumbstats, ladder
MOVE, SPLIT = topgames.RX["move"], topgames.RX["split"]
tot = collections.defaultdict(collections.Counter)
for log in sorted(glob.glob(sys.argv[1] + "/*.txt")):
    name = log.split("-")[-3] if False else log.split("/")[-1].split("-")[1]
    mp = ladder.load_map(f"maps/{name}.map")
    length = {i: len(b) for i, b in enumerate(mp["dragons"])}; team = {i: i % 2 for i in length}
    ours = None
    for line in open(log):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        x = dumbstats.IND.match(ev)
        if x:
            if ours is None: ours = int(x[1]) % 2
            continue
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); team.setdefault(d, d % 2)
            if m[5]: length[d] = int(m[5])
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); team[c] = team.get(a, a % 2); L = length.get(a, 0)
            if L >= 8 and k == L - 2:
                side = "us" if team[a] == ours else "them"
                tot[name][side] += 1; tot["ALL"][side] += 1; tot[name][side + "_cells"] += 2
            length[a] = L - k; length[c] = k
for n, c in sorted(tot.items()): print(f"{n:16s} big U-turns us {c['us']:4d}  v11 {c['them']:4d}")
