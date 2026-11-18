# Per window: dragons, heads on contested ground (mean), contested eats, head-on deaths by local count and zone.
import sys, re, collections, pathlib
sys.path.insert(0, "tools")
import ladder, topgames, zones, dumbstats
maps = [ladder.load_map(str(p)) for p in sorted(pathlib.Path("maps").glob("*.map"))]
MOVE, SPLIT, DIE, TAKEN = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"], topgames.RX["taken"]
for log in sys.argv[1:]:
    mp = ladder.identify(log, maps); W, H = mp["W"], mp["H"]
    heads = [b[0] for b in mp["dragons"]]
    d0 = zones.bfs(mp, [h for i, h in enumerate(heads) if i % 2 == 0]); d1 = zones.bfs(mp, [h for i, h in enumerate(heads) if i % 2 == 1])
    def zone(t, xy):
        a, b = (d0, d1) if t == 0 else (d1, d0)
        if xy not in a or xy not in b: return "far"
        diff = a[xy] - b[xy]; return "home" if diff < -3 else "enemy" if diff > 3 else "cont"
    ours = None
    team = {i: i % 2 for i in range(len(heads))}; pos = {i: tuple(h) for i, h in enumerate(heads)}; alive = set(team); pend = set()
    WIN = 25; st = collections.defaultdict(collections.Counter); cur = 0; lastdie = None
    def close(r):
        w = r // WIN * WIN
        for d in alive:
            t = team[d]; z = zone(t, pos[d]) if d in pos else "?"
            st[w][(t, "n")] += 1; st[w][(t, z)] += 1
        st[w]["rounds"] += 1
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        if r != cur:
            for rr in range(cur, r): close(rr)
            cur = r
        x = dumbstats.IND.match(ev)
        if x:
            if ours is None: ours = int(x[1]) % 2
            continue
        m = TAKEN.match(ev)
        if m: pend.add((int(m[1]), int(m[2]))); continue
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); t = team.setdefault(d, d % 2); xy = (int(m[3]), int(m[4])); pos[d] = xy; alive.add(d)
            if m[5] and xy in pend:
                pend.discard(xy); st[r // WIN * WIN][(t, "eat_" + zone(t, xy))] += 1
            continue
        m = SPLIT.match(ev)
        if m:
            a, c = int(m[1]), int(m[2]); team[c] = team.get(a, a % 2); alive.add(c); continue
        m = DIE.match(ev)
        if m:
            d = int(m[1]); t = team.get(d, d % 2); alive.discard(d)
            if m[2] == "head-on collision" and d in pos:
                # local heads within 3 of the victim at that moment
                near = collections.Counter(team[e] for e in alive if e in pos and min(abs(pos[e][0]-pos[d][0]), W-abs(pos[e][0]-pos[d][0])) + min(abs(pos[e][1]-pos[d][1]), H-abs(pos[e][1]-pos[d][1])) <= 3)
                near[t] += 1
                loc = "ahead" if near[ours] > near[1 - ours] else "behind" if near[ours] < near[1 - ours] else "even"
                st[r // WIN * WIN][(t, "ho_" + loc)] += 1
                st[r // WIN * WIN][(t, "hoZ_" + zone(ours, pos[d]))] += 1
    close(cur)
    print(f"== {log} ({pathlib.Path(mp['path']).stem}), we are {'EVEN' if ours == 0 else 'ODD'}; zones from OUR side")
    print("win   dragons  cont-heads(avg) contEats  enemyEats(on their side)  our head-on deaths when we were behind/even/ahead  (their deaths)")
    for w in sorted(st):
        s = st[w]; n = max(1, s["rounds"])
        us, th = ours, 1 - ours
        print(f"r{w:<3}  {s[(us,'n')]/n:4.1f} v {s[(th,'n')]/n:4.1f}   {s[(us,'cont')]/n:4.1f} v {s[(th,'cont')]/n:4.1f}      {s[(us,'eat_cont')]:3d} v {s[(th,'eat_cont')]:3d}    us-on-enemy {s[(us,'eat_enemy')]:3d} them-on-ours {s[(th,'eat_enemy')]:3d}    "
              f"{s[(us,'ho_behind')]}/{s[(us,'ho_even')]}/{s[(us,'ho_ahead')]} ({s[(th,'ho_behind')]}/{s[(th,'ho_even')]}/{s[(th,'ho_ahead')]})  where(our side view) home {s[(us,'hoZ_home')]} cont {s[(us,'hoZ_cont')]} enemy {s[(us,'hoZ_enemy')]}")
