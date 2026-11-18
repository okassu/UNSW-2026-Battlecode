import re, glob, collections
REAL = "portals default dilemma slithery_fight autarky queen_of_spades trophy devil schooltime stronghold".split()
IND = re.compile(r"dragon (\d+): (\S+)"); DIE = re.compile(r"dragon (\d+) dies\. Reason: (.*)")
SPL = re.compile(r"dragon (\d+) splits off dragon (\d+)")
tally = collections.defaultdict(collections.Counter); nomap = collections.Counter()
for log in glob.glob("results/logs-v12l/*.txt"):
    m = next((m for m in REAL if f"-{m}-" in log), None)
    if not m: continue
    last, split, ours = {}, {}, None
    for line in open(log):
        p = line.split("\t", 1)
        if len(p) < 2 or not p[0].isdigit(): continue
        r, ev = int(p[0]), p[1].strip()
        x = IND.match(ev)
        if x:
            d = int(x[1]); last[d] = (r, x[2])
            if ours is None: ours = d % 2
            continue
        x = SPL.match(ev)
        if x: split[int(x[1])] = r; split[int(x[2])] = r; continue
        x = DIE.match(ev)
        if x and ours is not None and int(x[1]) % 2 == ours:
            d, why = int(x[1]), x[2]
            stub = r - split.get(d, -99) <= 2
            if why in ("hit itself", "hit a body", "gave no valid action") and not stub:
                lab = last.get(d, (0, "none"))[1].rstrip(":")
                tally[(why, "portals" if m == "portals" else "other maps")][lab] += 1
for k in sorted(tally):
    t = tally[k]; n = sum(t.values())
    print(f"{k[0]:22s} {k[1]:10s} n={n:5d}  " + "  ".join(f"{l} {100*c/n:.0f}%" for l, c in t.most_common(7)))
