import sys, glob, collections
sys.path.insert(0, "tools")
import dumbstats
REAL = "portals default dilemma slithery_fight autarky queen_of_spades trophy devil schooltime stronghold Colosseum arena big_empty default_small trauma".split()
K = ("stub", "selfhit", "body", "wall", "headon", "noaction")
per = collections.defaultdict(collections.Counter)
for log in glob.glob(sys.argv[1] + "/*.txt"):
    name = next((m for m in REAL if f"-{m}-" in log), None)
    if not name: continue
    a = dumbstats.analyse(log, f"maps/{name}.map")
    for s in ("us", "them"):
        for key in (name, "ALL"):
            t = per[(key, s)]
            t["games"] += 1; t["moves"] += a["moves"][s]; t["eats"] += a["eats"][s]; t["win"] += (a["result"] in ("WIN", "eliminated THEM")) == (s == "us")
            for k, v in a["deaths"][s].items(): t[k] += v
            t["friendly"] += a["friendly"][s]; t["xdeath"] += a["xdeath"][s]
for (m, s) in sorted(per):
    t = per[(m, s)]; mv = t["moves"] / 100 or 1
    print(f"{m[:12]:12s} {s:4s} g{t['games']:<3d} won {t['win']:3d} moves/g {t['moves']//t['games']:6d} eats/100 {t['eats']/mv:5.2f}  deaths/100: " +
          " ".join(f"{k} {t[k]/mv:.2f}" for k in K) + f"  sum {sum(t[k] for k in K)/mv:.2f}")
