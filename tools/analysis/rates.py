import sys, os, glob, collections, pathlib
sys.path.insert(0, "tools")
import dumbstats, ladder, topgames
REAL = ("portals default dilemma slithery_fight autarky queen_of_spades trophy devil schooltime stronghold").split()
maps = [ladder.load_map(p) for p in sorted(pathlib.Path("maps").glob("*.map"))]
K = ("stub", "selfhit", "body", "wall", "headon", "noaction")
def acc(tot, a, s):
    tot["games"] += 1; tot["moves"] += a["moves"][s]; tot["loops"] += a["loops"][s]; tot["eats"] += a["eats"][s]
    tot["friendly"] += a["friendly"][s]; tot["xdeath"] += a["xdeath"][s]
    for k, v in a["deaths"][s].items(): tot[k] += v
def show(name, t):
    m = t["moves"] / 100
    print(f"{name:26s} games {t['games']:3d}  per 100 moves: " + "  ".join(f"{k} {t[k]/m:.2f}" for k in K + ("friendly", "xdeath"))
          + f"  loop {100*t['loops']/max(1,t['moves']):.1f}%  eats {t['eats']/m:.1f}")
cut, opp = collections.Counter(), collections.Counter()
permap = collections.defaultdict(collections.Counter)
for i in range(1, 41):
    log = f"topEloBattles/game{i}.txt"
    if os.path.getsize(log) == 0: continue
    mp = ladder.identify(log, maps); b = topgames.analyse(log, mp)
    sh = lambda t: b["ping_dirs"][t].get(2, 0) / max(1, sum(b["ping_dirs"][t].values()))
    c = 0 if sh(0) > sh(1) else 1
    a = dumbstats.analyse(log, mp["path"] if mp else None, side_override=c)
    acc(cut, a, "us"); acc(opp, a, "them"); acc(permap["cut:" + pathlib.Path(mp["path"]).stem], a, "us")
ours, v11 = collections.Counter(), collections.Counter()
for log in glob.glob("results/logs-v12l/*.txt"):
    mname = next((m for m in REAL if f"-{m}-" in log), None)
    if not mname: continue
    a = dumbstats.analyse(log, f"maps/{mname}.map")
    acc(ours, a, "us"); acc(v11, a, "them"); acc(permap["v12l:" + mname], a, "us")
show("cutlery (ladder)", cut); show("cutlery's opponents", opp); show("v12l (bench, same maps)", ours); show("v11 (bench, same maps)", v11)
print()
for k in sorted(permap, key=lambda x: (x.split(":")[1], x)): show(k, permap[k])
