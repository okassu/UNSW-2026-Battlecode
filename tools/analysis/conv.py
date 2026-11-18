# Mass (total length) and longest per side at r300/350/400/450/500; conversion = longest@500 / mass@350.
import sys, os, glob, collections, pathlib, statistics as st
sys.path.insert(0, "tools")
import ladder, topgames, dumbstats
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
maps = [ladder.load_map(str(p)) for p in sorted(pathlib.Path("maps").glob("*.map"))]
byname = {pathlib.Path(m["path"]).stem: m for m in maps}
CK = (300, 350, 400, 450, 499)
def run(log, mp):
    team, length, alive = {}, {}, set()
    for i, b in enumerate(mp["dragons"]): team[i] = i % 2; length[i] = len(b); alive.add(i)
    snap = {}; cur = 0; last = 0
    def close(r):
        if r in CK:
            snap[r] = {t: (sum(length[d] for d in alive if team[d] == t), max([length[d] for d in alive if team[d] == t] or [0]),
                           sum(1 for d in alive if team[d] == t)) for t in (0, 1)}
    for line in open(log, errors="replace"):
        p = line.split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        if r != cur:
            for rr in range(cur, r): close(rr)
            cur = r
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); team.setdefault(d, d % 2); alive.add(d)
            if len(m[2]) > 1: length[d] = length.get(d, 2) - (len(m[2]) - 1)
            if m[5]: length[d] = int(m[5])
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); team[c] = team.get(a, a % 2); length[a] -= k; length[c] = k; alive.add(c); continue
        m = DIE.match(ev)
        if m: alive.discard(int(m[1]))
    close(cur)
    return snap
rows = collections.defaultdict(list)
REAL = "portals default dilemma slithery_fight autarky queen_of_spades trophy devil schooltime stronghold Colosseum arena big_empty default_small trauma".split()
for log in glob.glob(sys.argv[1] + "/*.txt"):
    name = next((m for m in REAL if f"-{m}-" in log), None)
    if not name: continue
    ours = None
    for line in open(log):
        x = dumbstats.IND.match(line.split("\t", 1)[-1])
        if x: ours = int(x[1]) % 2; break
    s = run(log, byname[name])
    if 499 not in s: continue
    rows["ours"].append((name, {r: s[r][ours] for r in s})); rows["opp"].append((name, {r: s[r][1 - ours] for r in s}))
for i in range(1, 41):
    log = f"topEloBattles/game{i}.txt"
    if os.path.getsize(log) == 0: continue
    mp = ladder.identify(log, maps); a = topgames.analyse(log, mp)
    s = run(log, mp)
    if 499 not in s: continue
    for t in (0, 1): rows["topW" if a["winner"] == t else "topL"].append((pathlib.Path(mp["path"]).stem, {r: s[r][t] for r in s}))
for g, rs in rows.items():
    f = lambda r, i: st.mean([x[r][i] for _, x in rs if r in x])
    conv = st.mean([x[499][1] / max(1, x[350][0]) for _, x in rs if 350 in x])
    print(f"{g:5s} n={len(rs):3d} mass " + " ".join(f"r{r}:{f(r,0):.0f}" for r in CK) + " | longest " + " ".join(f"{f(r,1):.0f}" for r in CK)
          + " | dragons " + " ".join(f"{f(r,2):.0f}" for r in CK) + f" | L500/mass350 {conv:.2f}")

print("--- lengths of our dragons at r499 (sorted), bench games reaching r500")
import re
for log in sorted(glob.glob(sys.argv[1] + "/*.txt"))[:400]:
    name = next((m for m in REAL if f"-{m}-" in log), None)
    if not name: continue
    ours = None
    for line in open(log):
        x = dumbstats.IND.match(line.split("\t", 1)[-1])
        if x: ours = int(x[1]) % 2; break
    mp = byname[name]
    team, length, alive = {}, {}, set()
    for i, b in enumerate(mp["dragons"]): team[i] = i % 2; length[i] = len(b); alive.add(i)
    lab = {}; last = 0
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]; last = r
        x = dumbstats.IND.match(ev)
        if x: lab[int(x[1])] = x[2]; continue
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
    ls = sorted(((length[d], lab.get(d, "?")) for d in alive if team[d] == ours), reverse=True)
    print(f"{name[:12]:12s} " + " ".join(f"{L}{'C' if l.endswith(' C') else ''}" for L, l in ls[:14]) + ("..." if len(ls) > 14 else ""))
