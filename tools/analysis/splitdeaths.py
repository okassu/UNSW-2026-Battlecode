"""How often our split parents / children die within 2 rounds, by our SPLIT / UTURN labels.

    python3 tools/analysis/splitdeaths.py LOG_OR_DIR [...]     (logs with our indicator lines: ladder or bench)
"""
import collections, glob, os, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import topgames, dumbstats

MOVE, SPLIT, DIE = topgames.RX["move"], topgames.RX["split"], topgames.RX["die"]
logs = []
for a in sys.argv[1:]:
    logs += sorted(glob.glob(os.path.join(a, "*.txt"))) if os.path.isdir(a) else [a]
tot = collections.Counter()
for log in logs:
    lab, pend = {}, {}
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t", 1)
        if len(p) < 2 or not p[0].isdigit():
            continue
        r, ev = int(p[0]), p[1]
        x = dumbstats.IND.match(ev)
        if x:
            lab[int(x[1])] = (r, x[2].split()[0])
            continue
        m = SPLIT.match(ev)
        if m:
            a, c = int(m[1]), int(m[2])
            if a in lab and lab[a][0] == r and lab[a][1] in ("SPLIT", "UTURN"):
                k = lab[a][1]
                tot[(k, "n")] += 1
                pend[a], pend[c] = (r, k, "parent"), (r, k, "child")
            continue
        m = DIE.match(ev)
        if m:
            d = int(m[1])
            if d in pend and r - pend[d][0] <= 2:
                tot[(pend[d][1], pend[d][2])] += 1
            pend.pop(d, None)
print(f"{len(logs)} logs")
for k in ("SPLIT", "UTURN"):
    n = tot[(k, "n")]
    print(f"  {k}: {n} ({n / max(1, len(logs)):.0f} per game); parent dead within 2 rounds {100 * tot[(k, 'parent')] / max(1, n):.0f}%,"
          f" child {100 * tot[(k, 'child')] / max(1, n):.0f}%")
