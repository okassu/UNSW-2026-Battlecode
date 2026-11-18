import sys, os, pathlib, collections
sys.path.insert(0, "tools")
import ladder, topgames
maps = [ladder.load_map(p) for p in sorted(pathlib.Path("maps").glob("*.map"))]
for i in range(1, 41):
    log = f"topEloBattles/game{i}.txt"
    if os.path.getsize(log) == 0: continue
    mp = ladder.identify(log, maps)
    a = topgames.analyse(log, mp)
    name = pathlib.Path(mp["path"]).stem if mp else "?"
    out = []
    for t in (0, 1):
        pv = a["ping_vals"][t]; n = sum(pv.values())
        dirs = a["ping_dirs"][t]; nd = sum(dirs.values()) or 1
        rate = n / max(1, a["dragon_rounds"][t])
        dmix = "/".join(f"{100*dirs.get(k,0)//nd}" for k in (1,2,3,4))
        rep = sum(c for v, c in pv.items() if c > 1) / max(1, n)   # share of pings whose value repeats
        hi = collections.Counter(v >> 52 for v in pv.elements()).most_common(1)
        hish = hi[0][1] / n if hi else 0
        S_ = a["S"][t]; ss = a["split_sizes"][t].most_common(2); sac = a["sac"][t]
        out.append(f"{'W' if a['winner']==t else 'L' if a['winner']==1-t else 'D'} r{rate:.2f} d{dmix} rep{100*rep:.0f} hi3{100*hish:.0f} "
                   f"eat{a['first'][t].get('eat','-')} spl{a['first'][t].get('split','-')} ss{ss} sac{len(sac)}@{sac[0][0] if sac else '-'} "
                   f"d50={a['alive_at'][t].get(50,'-')}")
    print(f"g{i:<2} {name[:10]:10s} r{a['end']:<3} | 0: {out[0]}\n{'':18s}| 1: {out[1]}")
