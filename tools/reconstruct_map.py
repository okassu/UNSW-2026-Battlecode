"""Rebuild an unknown ladder map from a game log, as a local test fixture.

    python3 tools/reconstruct_map.py LOG W H NAME OUT.map

Evidence, mirrored through rot180 symmetry (maps are "SYMMETRY xy"):
  - a one-step move to the neighbour opens that edge; to anywhere else it is a
    portal whose partner edge lies behind the landing tile (a crossing keeps its direction);
  - "hit a wall" after a one-step ask marks that edge kelp;
  - "(x,y) next pearl due in N" gives spawn tiles and a gap range.
Edges never crossed in the whole game (or touching a tile no head ever visited)
become kelp: an approximation, so treat results on the fixture as indicative.
Starting dragons are not in the log; pass them with --dragon "team len x y x y ..."
"""
import argparse, collections, re

ap = argparse.ArgumentParser()
ap.add_argument("log"); ap.add_argument("W", type=int); ap.add_argument("H", type=int)
ap.add_argument("name"); ap.add_argument("out")
ap.add_argument("--dragon", action="append", default=[], help='"team len x0 y0 x1 y1 ..." head first')
a = ap.parse_args()
W, H = a.W, a.H
DXY = {'N': (0, -1), 'E': (1, 0), 'S': (0, 1), 'W': (-1, 0)}
OPP = {'N': 'S', 'S': 'N', 'E': 'W', 'W': 'E'}
st = lambda p, d: ((p[0] + DXY[d][0]) % W, (p[1] + DXY[d][1]) % H)
rot = lambda p: (W - 1 - p[0], H - 1 - p[1])
def canon(p, d):  # edge as (tile, 'N'|'W')
    if d == 'S': return (st(p, 'S'), 'N')
    if d == 'E': return (st(p, 'E'), 'W')
    return (p, d)
def rot_edge(e):
    p, d = e
    return canon(rot(p), OPP[d])

mv = re.compile(r'^(\d+)\tdragon (\d+) moves ([NESW]+) to \((\d+),(\d+)\)')
ask = re.compile(r'^(\d+)\tdragon (\d+) asks to move ([NESW]+)')
die = re.compile(r'^(\d+)\tdragon (\d+) dies\. Reason: (.*)')
due = re.compile(r'^(\d+)\t\((\d+),(\d+)\) next pearl due in (-?\d+)')
head, asked = {}, {}
visited, opened, kelp = set(), set(), set()
portal = {}  # canon edge -> partner canon edge
gaps = collections.defaultdict(set)
for line in open(a.log):
    m = due.match(line)
    if m:
        gaps[(int(m[2]), int(m[3]))].add(int(m[4])); continue
    m = ask.match(line)
    if m:
        asked[int(m[2])] = m[3]; continue
    m = mv.match(line)
    if m:
        d, dirs, to = int(m[2]), m[3], (int(m[4]), int(m[5]))
        visited.add(to)
        prev = head.get(d)
        if prev is not None and len(dirs) == 1:
            if st(prev, dirs) == to:
                opened.add(canon(prev, dirs))
            else:
                e1, e2 = canon(prev, dirs), canon(to, OPP[dirs])
                portal[e1], portal[e2] = e2, e1
        elif prev is not None:
            cur = prev
            for c in dirs: cur = st(cur, c)
            if cur == to:
                cur = prev
                for c in dirs:
                    opened.add(canon(cur, c)); cur = st(cur, c)
        head[d] = to
        continue
    m = die.match(line)
    if m:
        d = int(m[2])
        if m[3] == 'hit a wall' and d in head and len(asked.get(d, '')) == 1:
            kelp.add(canon(head[d], asked[d]))
        head.pop(d, None)
# the first move of each dragon has no known start: seed starts from --dragon heads
for spec in a.dragon:
    t = list(map(int, spec.split()))
    for i in range(t[1]): visited.add((t[2 + 2 * i], t[3 + 2 * i]))
# symmetry
visited |= {rot(p) for p in visited}
opened |= {rot_edge(e) for e in opened}
kelp |= {rot_edge(e) for e in kelp}
for e1, e2 in list(portal.items()):
    portal.setdefault(rot_edge(e1), rot_edge(e2))
for p, s in list(gaps.items()):
    gaps[rot(p)] |= s
clash = (opened & kelp) | (opened & set(portal)) | (kelp & set(portal))
# pair ids
pid, ids = {}, 0
for e1, e2 in sorted(portal.items()):
    if e1 in pid or portal.get(e2) != e1: continue
    pid[e1] = pid[e2] = ids; ids += 1
lines = [f"MAP {W} {H}", "SYMMETRY xy", f"MAP_NAME {a.name}", f"TILE_COUNT {W * H}"]
for y in range(H):
    for x in range(W):
        s = {v for v in gaps.get((x, y), set()) if v > 0}
        lines.append(f"TILE {x} {y} {min(s)} {max(s)}" if s else f"TILE {x} {y} 0 0")
edges, stats = [], collections.Counter()
for y in range(H):
    for x in range(W):
        for d, idx in (('N', 2 * y * (W + 1) + x), ('W', (2 * y + 1) * (W + 1) + x)):
            e = ((x, y), d)
            other = st((x, y), d)
            if e in pid: kind, p = 2, pid[e]; stats['portal'] += 1
            elif e in kelp: kind, p = 1, -1; stats['kelp (hit a wall)'] += 1
            elif e in opened: kind, p = 0, -1; stats['open (crossed)'] += 1
            elif (x, y) not in visited or other not in visited: kind, p = 1, -1; stats['kelp (unvisited tile)'] += 1
            else: kind, p = 1, -1; stats['kelp (never crossed, guessed)'] += 1
            edges.append(f"EDGE {idx} {kind} {p}")
lines.append(f"EDGE_COUNT {len(edges)}"); lines += edges
lines.append(f"DRAGON_COUNT {len(a.dragon)}")
for spec in a.dragon: lines.append(f"DRAGON {spec}")
lines.append("END")
open(a.out, "w").write("\n".join(lines) + "\n")
print(f"wrote {a.out}: {dict(stats)}, {ids} portal pairs, {len(visited)} visited tiles, "
      f"{sum(1 for p in gaps if any(v > 0 for v in gaps[p]))} spawn tiles, clashes {len(clash)}")
