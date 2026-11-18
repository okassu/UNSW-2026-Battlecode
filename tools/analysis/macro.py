import sys, os, glob, collections, pathlib, statistics as st
sys.path.insert(0, "tools")
import ladder, topgames, zones, dumbstats
REAL = "portals default dilemma slithery_fight autarky queen_of_spades trophy devil schooltime stronghold".split()
maps = [ladder.load_map(str(p)) for p in sorted(pathlib.Path("maps").glob("*.map"))]
byname = {pathlib.Path(m["path"]).stem: m for m in maps}
rows = collections.defaultdict(list)   # (group, map) -> list of dict
def stats(log, mp, t, a, z, fc, ds, us_label):
    o = {}
    o["win"] = 1.0 if a["winner"] == t else 0.0 if a["winner"] == 1 - t else 0.5
    for c in (25, 50, 100, 200):
        o[f"d{c}"] = a["alive_at"][t].get(c); o[f"e{c}"] = a["eats_at"][t].get(c)
    for c in (200, 300, 500):
        o[f"L{c}"] = a["long_at"][t].get(c)
    o["split1"] = a["first"][t].get("split"); o["eat1"] = a["first"][t].get("eat")
    ss = a["split_sizes"][t]; n = sum(ss.values()) or 1
    o["split2%"] = 100 * ss.get(2, 0) / n
    pl = a["split_parent_len"][t]; o["parentL"] = sum(k * v for k, v in pl.items()) / (sum(pl.values()) or 1)
    sac = a["sac"][t]
    o["sac<50"] = sum(1 for r, L in sac if r < 50); o["sac50-250"] = sum(1 for r, L in sac if 50 <= r < 250); o["sac250+"] = sum(1 for r, L in sac if r >= 250)
    o["sacL"] = st.mean([L for r, L in sac]) if sac else None
    o["splits"] = a["S"][t]["splits"]; o["cross"] = a["cross"][t]; o["sprint"] = a["S"][t]["sprint"]
    car = a["carrier"].get(t); o["carrierBorn"] = car[1] if car else None
    tot = lambda lo: sum(z[t][(lo, k)] for k in ("home", "cont", "enemy"))
    for lo in (0, 25, 50, 100):
        o[f"cont%{lo}"] = 100 * z[t][(lo, "cont")] / tot(lo) if tot(lo) else None
        o[f"enemy%{lo}"] = 100 * z[t][(lo, "enemy")] / tot(lo) if tot(lo) else None
    o["1stC"] = fc.get(t)
    s = us_label
    tr = collections.Counter((st_, loc) for _, st_, loc, _ in ds["trades"])
    me, oth = ("us", "them") if s == "us" else ("them", "us")
    for loc in ("behind", "even", "ahead"):
        o[f"ram_{loc}"] = tr[(me, loc)]
    o["loop%"] = 100 * ds["loops"][s] / max(1, ds["moves"][s])
    return o
def add(group, name, log, mp, t, dsside):
    a = topgames.analyse(log, mp); z, fc = zones.zones(log, mp)
    ds = dumbstats.analyse(log, mp["path"], side_override=dsside)
    rows[(group, name)].append(stats(log, mp, t, a, z, fc, ds, "us"))
    rows[(group + "-opp", name)].append(stats(log, mp, 1 - t, a, z, fc, ds, "them"))
for i in range(1, 41):
    log = f"topEloBattles/game{i}.txt"
    if os.path.getsize(log) == 0: continue
    mp = ladder.identify(log, maps); b = topgames.analyse(log, mp)
    sh = lambda t: b["ping_dirs"][t].get(2, 0) / max(1, sum(b["ping_dirs"][t].values()))
    c = 0 if sh(0) > sh(1) else 1
    add("cutlery", pathlib.Path(mp["path"]).stem, log, mp, c, c)
src = sys.argv[1] if len(sys.argv) > 1 else "results/logs-v12l"
for log in sorted(glob.glob(src + "/*.txt")):
    name = next((m for m in REAL if f"-{m}-" in log), None)
    if not name: continue
    mp = byname[name]; ours = None
    for line in open(log):
        x = dumbstats.IND.match(line.split("\t", 1)[-1])
        if x: ours = int(x[1]) % 2; break
    add("ours", name, log, mp, ours, ours)
import json
json.dump({f"{g}|{m}": v for (g, m), v in rows.items()}, open(sys.argv[2] if len(sys.argv) > 2 else "/dev/null", "w"))
keys = list(next(iter(rows.values()))[0].keys())
def mean(vals):
    v = [x for x in vals if x is not None]
    return f"{st.mean(v):.1f}" if v else "-"
groups = ["cutlery", "cutlery-opp", "ours", "ours-opp"]
print(f"{'metric':12s} " + " ".join(f"{g:>12s}" for g in groups))
for k in keys:
    print(f"{k:12s} " + " ".join(f"{mean([r[k] for (g, m), rs in rows.items() if g == G for r in rs]):>12s}" for G in groups))
