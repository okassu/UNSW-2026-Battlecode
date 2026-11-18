"""Per-map win rates of gate result files side by side (same maps/seeds/opponent), with the pairwise
difference against the first file: games won by one and lost by the other on the same map, seed and colour.

    python3 tools/abcmp.py results/ab-ab_base.jsonl results/ab-ab_noopen.jsonl ...
"""
import collections, json, sys

files = sys.argv[1:]
data = {}
for f in files:
    d = {}
    for l in open(f):
        r = json.loads(l)
        s = 1.0 if r["result"] == "win" else 0.5 if r["result"] == "draw" else 0.0
        d[(r["map"], r["opp"], r["seed"], r["swap"])] = (s, r["long_me"] - r["long_them"])
    data[f] = d
maps = sorted({k[0] for d in data.values() for k in d})
name = lambda f: f.split("/")[-1].replace(".jsonl", "").replace("ab-", "")[:13]
print(f"{'map':16s}" + "".join(f"{name(f):>14s}" for f in files))
for m in maps + ["ALL"]:
    row = f"{m.split('/')[-1][:-4] if m != 'ALL' else 'ALL':16s}"
    for f in files:
        v = [s for k, (s, _) in data[f].items() if m == "ALL" or k[0] == m]
        L = [x for k, (_, x) in data[f].items() if m == "ALL" or k[0] == m]
        row += f"{(sum(v) / len(v) if v else float('nan')):>8.2f} {(sum(L) / len(L) if L else 0):+5.1f}"
    print(row)
base = data[files[0]]
for f in files[1:]:
    common = [k for k in data[f] if k in base]
    up = sum(1 for k in common if data[f][k][0] > base[k][0]); down = sum(1 for k in common if data[f][k][0] < base[k][0])
    print(f"{name(f)} v {name(files[0])}: {len(common)} common games, better in {up}, worse in {down}")
