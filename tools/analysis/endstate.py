import sys, glob, collections, statistics as st
sys.path.insert(0, "tools")
import topgames, dumbstats, ladder
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
for src in sys.argv[1:]:
    shares, ncs, longs, masses, olong = [], [], [], [], []
    for log in sorted(glob.glob(src + "/*.txt")):
        name = log.split("/")[-1].split("-")[1]
        mp = ladder.load_map(f"maps/{name}.map")
        team = {i: i % 2 for i in range(len(mp["dragons"]))}; length = {i: len(b) for i, b in enumerate(mp["dragons"])}; alive = set(team)
        lab = {}; ours = None; last = 0
        for line in open(log):
            p = line.rstrip("\n").split("\t", 1)
            if len(p) < 2 or not p[0].isdigit(): continue
            r, ev = int(p[0]), p[1]; last = r
            x = dumbstats.IND.match(ev)
            if x:
                ours = int(x[1]) % 2 if ours is None else ours; lab[int(x[1])] = (r, x[2]); continue
            m = MOVE.match(ev)
            if m:
                d = int(m[1]); team.setdefault(d, d % 2); alive.add(d)
                if m[5]: length[d] = int(m[5])
                continue
            m = SPLIT.match(ev)
            if m:
                a, c, k = int(m[1]), int(m[2]), int(m[3]); team[c] = team.get(a, a % 2); length[a] -= k; length[c] = k; alive.add(c); continue
            m = DIE.match(ev)
            if m: alive.discard(int(m[1]))
        if last < 499: continue
        mine = [d for d in alive if team[d] == ours]; theirs = [d for d in alive if team[d] != ours]
        if not mine: continue
        L = max(length[d] for d in mine); M = sum(length[d] for d in mine)
        shares.append(L / M); longs.append(L); masses.append(M); olong.append(max([length[d] for d in theirs] or [0]))
        ncs.append(sum(1 for d in mine if d in lab and lab[d][0] >= 495 and lab[d][1].endswith(" C")))
    print(f"{src}: games to r500 {len(longs)}  our longest {st.mean(longs):.1f} (v11 {st.mean(olong):.1f})  mass {st.mean(masses):.0f}  longest/mass {st.mean(shares):.2f}  carriers marked at end {st.mean(ncs):.1f}")
