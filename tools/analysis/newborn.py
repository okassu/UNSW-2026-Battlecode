"""First-turn deaths of our newborn dragons (split off this round): did the newborn have a free neighbouring tile?
Rebuilds all bodies from the log. Usage: python3 tools/analysis/newborn.py LOG ..."""
import collections, pathlib, re, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import ladder, topgames, dumbstats
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
ASK = re.compile(r"dragon (\d+) asks to move ([NESW]+)")
D = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
maps = [ladder.load_map(str(p)) for p in sorted((pathlib.Path(__file__).resolve().parent.parent.parent / "maps").glob("*.map"))]
tot = collections.Counter(); ex = []
for log in sys.argv[1:]:
    mp = ladder.identify(log, maps); W, H = mp["W"], mp["H"]
    body = {i: collections.deque(b) for i, b in enumerate(mp["dragons"])}
    team = {i: i % 2 for i in body}; ours = None; lab = {}; born = {}; ask = {}
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        x = dumbstats.IND.match(ev)
        if x: d = int(x[1]); ours = d % 2 if ours is None else ours; lab[d] = (r, x[2].split()[0]); continue
        m = ASK.match(ev)
        if m: ask[int(m[1])] = (r, m[2]); continue
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); b = body.setdefault(d, collections.deque()); b.appendleft((int(m[3]), int(m[4])))
            L = int(m[5]) if m[5] else len(b) - 1
            while len(b) > L: b.pop()
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); b = body[a]
            body[c] = collections.deque([b.pop() for _ in range(min(k, len(b)))]); team[c] = team.get(a, a % 2)
            pl = lab.get(a, (None, "?")); born[c] = (r, pl[1] if pl[0] == r else "?", k)
            continue
        m = DIE.match(ev)
        if m:
            d = int(m[1])
            if ours is not None and team.get(d) == ours and d in born and born[d][0] == r and m[2] == "hit itself" and d in body and body[d]:
                h = body[d][0]
                occ = set()
                for o, bb in body.items():
                    occ.update(bb)
                free = []
                for dr, (dx, dy) in D.items():
                    if mp["edges"].get(ladder.eidx(W, H, h[0], h[1], dr), 0) == 1: continue   # kelp
                    n = ((h[0] + dx) % W, (h[1] + dy) % H)
                    if n not in occ: free.append(dr)
                kind = born[d][1]
                tot[(kind, "had a free neighbour" if free else "boxed in")] += 1
                if free and len(ex) < 8: ex.append(f"{log.split('/')[-1]} r{r} d{d} ({kind} child, {born[d][2]} cells) head {h} asked {ask.get(d)} free {free}")
            body.pop(d, None)
for k, v in sorted(tot.items()): print(f"  {k}: {v}")
print("\n".join(ex))
