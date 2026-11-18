import sys, os, pathlib, collections
sys.path.insert(0, "tools")
import ladder, topgames
maps = [ladder.load_map(p) for p in sorted(pathlib.Path("maps").glob("*.map"))]
for i in range(1, 41):
    log = f"topEloBattles/game{i}.txt"
    if os.path.getsize(log) == 0: continue
    mp = ladder.identify(log, maps)
    a = topgames.analyse(log, mp)
    name = pathlib.Path(mp["path"]).stem if mp else "unknown"
    print(f"game{i} {name} r{a['end']} winner {a['winner']} res {a['res']}")
    for t in (0, 1):
        pv = a["ping_vals"][t]; n = sum(pv.values())
        top = [(hex(v), c) for v, c in pv.most_common(2)]
        print(f"   {t}: {topgames.fingerprint(a, t)[:170]}")
