"""Whose body do our dragons run into? Rebuilds every body from the log (moves, growth, splits, deaths) and, for each
`hit a body` / `hit itself` death, finds the dragon occupying the tile the victim asked to move into.

    python3 tools/analysis/bodyhits.py prevBattles/prev34.txt ...   (logs with our indicator lines)
"""
import collections, pathlib, re, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import ladder, topgames, dumbstats
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
ASK = re.compile(r"dragon (\d+) asks to move ([NESW]+)")
D = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
maps = [ladder.load_map(str(p)) for p in sorted((pathlib.Path(__file__).resolve().parent.parent.parent / "maps").glob("*.map"))]
tot = collections.Counter()
for log in sys.argv[1:]:
    mp = ladder.identify(log, maps); W, H = mp["W"], mp["H"]
    body = {i: collections.deque(b) for i, b in enumerate(mp["dragons"])}
    team = {i: i % 2 for i in body}; ours = None; ask = {}; young = {}
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        x = dumbstats.IND.match(ev)
        if x:
            if ours is None: ours = int(x[1]) % 2
            continue
        m = ASK.match(ev)
        if m: ask[int(m[1])] = m[2]; continue
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); xy = (int(m[3]), int(m[4])); b = body.setdefault(d, collections.deque())
            b.appendleft(xy)
            L = int(m[5]) if m[5] else len(b) - 1
            while len(b) > L: b.pop()
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); b = body[a]
            tail = [b.pop() for _ in range(min(k, len(b)))]   # tail cells, old tail first = child's head first
            body[c] = collections.deque(tail); team[c] = team.get(a, a % 2); young[a] = young[c] = r
            continue
        m = DIE.match(ev)
        if m:
            d = int(m[1]); why = m[2]
            if why in ("hit a body", "hit itself") and ours is not None and d in body and body[d]:
                side = "us" if team.get(d, d % 2) == ours else "them"
                stub = r - young.get(d, -99) <= 2
                h = body[d][0]; dirs = ask.get(d, "")
                owner = "?"
                if dirs:
                    dx, dy = D[dirs[0]]; tgt = ((h[0] + dx) % W, (h[1] + dy) % H)
                    occ = [o for o, bb in body.items() if o != d and tgt in bb]
                    if tgt in list(body[d])[1:]: owner = "own body"
                    elif occ: owner = "teammate" if team.get(occ[0], occ[0] % 2) == team.get(d, d % 2) else "enemy"
                    else: owner = "empty/unknown"
                tot[(side, why, "stub" if stub else "non-stub", owner)] += 1
            body.pop(d, None)
for k in sorted(tot, key=lambda k: (k[0], k[1], k[2], -tot[k])): print(f"{k[0]:4s} {k[1]:11s} {k[2]:8s} into {k[3]:14s} {tot[k]:5d}")
