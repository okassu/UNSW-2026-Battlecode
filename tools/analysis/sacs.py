# Who eats the pearls a sacrifice drops, and how soon? Also the sacrificed dragon's length and the length of the
# longest ally head within 3 tiles at that moment (feeding a big dragon vs disposing of a trapped one).
import sys, os, re, collections, pathlib, json
sys.path.insert(0, "tools")
import ladder, topgames
maps = [ladder.load_map(str(p)) for p in sorted(pathlib.Path("maps").glob("*.map"))]
MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
APPEAR = re.compile(r"\((\d+),(\d+)\) pearl appears"); TAKEN = topgames.RX["taken"]
def run(log, mp):
    team, length, pos = {}, {}, {}
    for i, body in enumerate(mp["dragons"]): team[i] = i % 2; length[i] = len(body)
    W, H = mp["W"], mp["H"]
    drop_of = {}           # tile -> (team, round, kind)
    last_die = None
    out = collections.defaultdict(collections.Counter)
    pend = None
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t")
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1]
        m = APPEAR.match(ev)
        if m:
            if last_die and last_die[1] == r:
                drop_of[(int(m[1]), int(m[2]))] = (last_die[0], r, last_die[2])
            continue
        m = MOVE.match(ev)
        if m:
            d = int(m[1]); team.setdefault(d, d % 2); pos[d] = (int(m[3]), int(m[4]))
            if m[5]:
                length[d] = int(m[5])
                if pend and pend[0] == pos[d]:
                    xy = pend[0]; t, r0, kind = pend[1]
                    who = "own" if team[d] == t else "enemy"
                    # is the eater the longest of its team right now?
                    mx = max((length.get(x, 0) for x in pos if team.get(x) == team[d]), default=0)
                    big = "big" if length[d] >= mx and length[d] >= 8 else "small"
                    out[(t, kind)][f"{who}-{big}"] += 1
                    out[(t, kind)]["delay_sum"] += r - r0
                    out[(t, kind)]["eaten"] += 1
                    pend = None
            continue
        m = TAKEN.match(ev)
        if m:
            xy = (int(m[1]), int(m[2]))
            pend = (xy, drop_of.pop(xy)) if xy in drop_of else None
            continue
        m = SPLIT.match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3]); team[c] = team.get(a, a % 2)
            length[a] = length.get(a, 0) - k; length[c] = k
            continue
        m = DIE.match(ev)
        if m:
            d, why = int(m[1]), m[2]; t = team.get(d, d % 2)
            kind = "sac" if why.startswith("gave no valid") else "other"
            if kind == "sac":
                L = length.get(d, 0); hp = pos.get(d)
                near = [length.get(x, 0) for x in pos if x != d and team.get(x) == t and hp and x in pos and
                        min(abs(pos[x][0]-hp[0]), W-abs(pos[x][0]-hp[0])) + min(abs(pos[x][1]-hp[1]), H-abs(pos[x][1]-hp[1])) <= 3]
                nb = max(near, default=0)
                out[(t, kind)]["n"] += 1; out[(t, kind)][f"L{min(L, 5)}"] += 1
                out[(t, kind)]["nearBig8" if nb >= 8 else "nearAlly" if nb > 0 else "alone"] += 1
                out[(t, kind)]["r<50" if r < 50 else "r50-250" if r < 250 else "r250+"] += 1
            last_die = (t, r, kind)
            pos.pop(d, None)
            continue
    for k in out.values(): k["drops_total"] = 0
    total_drops = collections.Counter(v[0:1] + (v[2],) for v in drop_of.values())  # uneaten remain
    return out
agg = collections.defaultdict(collections.Counter)
res = json.load(open(sys.argv[1])) if len(sys.argv) > 1 else None
for i in range(1, 41):
    log = f"topEloBattles/game{i}.txt"
    if os.path.getsize(log) == 0: continue
    mp = ladder.identify(log, maps); a = topgames.analyse(log, mp)
    elim = a["end"] < 500
    o = run(log, mp)
    for t in (0, 1):
        g = ("W" if a["winner"] == t else "L") + (" elim" if elim else " r500")
        for kind in ("sac",):
            agg[g].update(o[(t, kind)])
for g in ("W elim", "L elim", "W r500", "L r500"):
    c = agg[g]; n = c["n"] or 1; e = c["eaten"] or 1
    print(f"{g}: sacrifices {c['n']}  L2 {100*c['L2']//n}% L3 {100*c['L3']//n}% L4 {100*c['L4']//n}% L5+ {100*c['L5']//n}%  "
          f"| ally within 3: >=8 long {100*c['nearBig8']//n}%, smaller {100*c['nearAlly']//n}%, none {100*c['alone']//n}%  "
          f"| r<50 {c['r<50']} r50-250 {c['r50-250']} r250+ {c['r250+']}")
    print(f"        drops eaten {c['eaten']}: own-big {100*c['own-big']//e}% own-small {100*c['own-small']//e}% "
          f"enemy {100*(c['enemy-big']+c['enemy-small'])//e}%  mean delay {c['delay_sum']/e:.1f} rounds")
