"""Random symmetric maps for generalisation tests (the Sprint uses only unseen maps).

    python3 tools/genmap.py OUTDIR [--count 12] [--seed 1]            # random mix (maps_gen/)
    python3 tools/genmap.py OUTDIR --grid --count 60 --seed 3 --prefix tune   # stratified grid

--grid cycles size (small/medium/large) x symmetry x style (open, rooms, maze, sealed halves,
1-wide wrap corridors) x resource motif (contested field, home dead-end pockets, portal-only
field, 2x2 portal pockets, scattered fields) x starter (short dragons / one long one); each
map gets a .json with its attributes, which tools/gate.py uses for its category table.

Each map mixes a layout style (open / rooms / maze / sealed halves joined by portals),
a symmetry (rot180 "xy", left-right mirror "y", top-bottom mirror "x", as in maps/*.map),
and pearl sources placed the way the ladder maps place them: a background rate, fields
on the contested line (tiles as far from one spawn as from the other), sometimes behind
portals only, and home pockets that are rich on some maps and barren on others.
Only one half is generated; the other half is its image, so the map is exactly symmetric.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import random

DIRS = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
OPP = {"N": "S", "S": "N", "E": "W", "W": "E"}


class Map:
    def __init__(self, W, H, sym, rng):
        self.W, self.H, self.sym, self.rng = W, H, sym, rng
        self.kelp = set()      # canonical edges (tile, 'N'|'W') that are kelp
        self.portal = {}       # canonical edge -> id
        self.gap = {}          # tile -> (lo, hi)
        self.dragons = []      # (team, [tiles head first])

    def st(self, p, d):
        return ((p[0] + DIRS[d][0]) % self.W, (p[1] + DIRS[d][1]) % self.H)

    def canon(self, p, d):
        if d == "S":
            return (self.st(p, "S"), "N")
        if d == "E":
            return (self.st(p, "E"), "W")
        return (p, d)

    def image(self, p):
        x, y = p
        if self.sym == "xy":
            return (self.W - 1 - x, self.H - 1 - y)
        if self.sym == "y":
            return (self.W - 1 - x, y)
        return (x, self.H - 1 - y)

    def image_dir(self, d):
        if self.sym == "xy":
            return OPP[d]
        if self.sym == "y":
            return {"E": "W", "W": "E"}.get(d, d)
        return {"N": "S", "S": "N"}.get(d, d)

    def image_edge(self, e):
        p, d = e
        return self.canon(self.image(p), self.image_dir(d))

    def in_half(self, p):
        """The generated half (the rest is its image); the middle line counts once."""
        x, y = p
        if self.sym == "y":
            return x < (self.W + 1) // 2
        if self.sym == "x":
            return y < (self.H + 1) // 2
        return y < self.H // 2 or (self.H % 2 == 1 and y == self.H // 2 and x < (self.W + 1) // 2)

    def set_kelp(self, p, d, on=True):
        e = self.canon(p, d)
        for f in (e, self.image_edge(e)):
            if on:
                self.kelp.add(f)
            else:
                self.kelp.discard(f)

    def set_gap(self, p, g):
        self.gap[p] = g
        self.gap[self.image(p)] = g

    def blocked(self, p, d):
        return self.canon(p, d) in self.kelp

    def portal_exit(self, p, d):
        """Tile a step across edge d of p lands on through a portal, or None."""
        e = self.canon(p, d)
        pid = self.portal.get(e)
        if pid is None:
            return None
        other = next(f for f, i in self.portal.items() if i == pid and f != e)
        # keep the direction: out of the partner edge on its far side
        q, od = other  # od in N/W: the edge between q and st(q, od)
        return self.st(q, OPP[od]) if d == od else q if d == OPP[od] else None

    def neighbours(self, p):
        for d in DIRS:
            e = self.canon(p, d)
            if e in self.portal:
                q = self.portal_exit(p, d)
                if q is not None:
                    yield q
            elif e not in self.kelp:
                yield self.st(p, d)

    def reach(self, start):
        seen = {start}
        q = [start]
        for p in q:
            for n in self.neighbours(p):
                if n not in seen:
                    seen.add(n)
                    q.append(n)
        return seen

    def write(self, path, name):
        W, H = self.W, self.H
        lines = [f"MAP {W} {H}", f"SYMMETRY {self.sym}", f"MAP_NAME {name}", f"TILE_COUNT {W * H}"]
        for y in range(H):
            for x in range(W):
                lo, hi = self.gap.get((x, y), (0, 0))
                lines.append(f"TILE {x} {y} {lo} {hi}")
        edges = []
        for y in range(H):
            for x in range(W):
                for d, i in (("N", 2 * y * (W + 1) + x), ("W", (2 * y + 1) * (W + 1) + x)):
                    e = ((x, y), d)
                    if e in self.portal:
                        edges.append(f"EDGE {i} 2 {self.portal[e]}")
                    elif e in self.kelp:
                        edges.append(f"EDGE {i} 1 -1")
                    else:
                        edges.append(f"EDGE {i} 0 -1")
        lines.append(f"EDGE_COUNT {len(edges)}")
        lines += edges
        lines.append(f"DRAGON_COUNT {len(self.dragons)}")
        for team, body in self.dragons:
            lines.append(f"DRAGON {team} {len(body)} " + " ".join(f"{x} {y}" for x, y in body))
        lines.append("END")
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")


def wall_line(m, x0, y0, length, horizontal):
    """A kelp wall `length` tiles long (edges between rows or columns)."""
    for k in range(length):
        p = ((x0 + k) % m.W, y0 % m.H) if horizontal else (x0 % m.W, (y0 + k) % m.H)
        if m.in_half(p):
            m.set_kelp(p, "N" if horizontal else "W")


def style_open(m):
    r = m.rng
    for _ in range(m.W * m.H // 60):
        wall_line(m, r.randrange(m.W), r.randrange(m.H), r.randint(2, 6), r.random() < 0.5)


def style_rooms(m):
    r = m.rng
    size = r.randint(5, 9)
    for y in range(0, m.H, size):
        for x in range(m.W):
            if (x % size) not in (size // 2, size // 2 + 1) or r.random() < 0.3:
                p = (x, y)
                if m.in_half(p):
                    m.set_kelp(p, "N")
    for x in range(0, m.W, size):
        for y in range(m.H):
            if (y % size) not in (size // 2,) or r.random() < 0.3:
                p = (x, y)
                if m.in_half(p):
                    m.set_kelp(p, "W")
    for _ in range(m.W * m.H // 80):  # extra doors
        p = (r.randrange(m.W), r.randrange(m.H))
        if m.in_half(p):
            m.set_kelp(p, r.choice("NW"), False)


def style_maze(m):
    """Randomised DFS maze on 2x2 cells (1-wide corridors between 1-wide walls of kelp edges),
    with extra openings so it has loops, like devil and schooltime."""
    r = m.rng
    for y in range(m.H):
        for x in range(m.W):
            p = (x, y)
            if m.in_half(p):
                m.set_kelp(p, "N")
                m.set_kelp(p, "W")
    cw, ch = m.W, m.H
    seen = set()
    stack = [(r.randrange(cw), r.randrange(ch))]
    seen.add(stack[0])
    while stack:
        p = stack[-1]
        opts = [d for d in DIRS if m.st(p, d) not in seen]
        if not opts:
            stack.pop()
            continue
        d = r.choice(opts)
        n = m.st(p, d)
        if m.in_half(p) or m.in_half(n):
            e = m.canon(p, d)
            m.set_kelp(e[0], e[1], False)
        seen.add(n)
        stack.append(n)
    for _ in range(m.W * m.H // 6):  # loops and small open areas
        p = (r.randrange(m.W), r.randrange(m.H))
        if m.in_half(p):
            m.set_kelp(p, r.choice("NW"), False)


def contested_points(m):
    """Fixed points of the symmetry on the torus: equally far from both sides' spawns."""
    W, H = m.W, m.H
    if m.sym == "xy":
        return [(W // 2, H // 2), (0, 0), (W // 2, 0), (0, H // 2)]
    if m.sym == "y":
        return [(W // 2, H // 2), (W // 2, 0), (0, H // 2), (0, 0)]
    return [(W // 2, H // 2), (0, H // 2), (W // 2, 0), (0, 0)]


def block(m, cx, cy, w, h):
    return [((cx + dx) % m.W, (cy + dy) % m.H) for dy in range(-(h // 2), h - h // 2) for dx in range(-(w // 2), w - w // 2)]


def enclose(m, tiles):
    """Kelp around a tile set (both halves, by symmetry)."""
    s = set(tiles)
    for p in tiles:
        for d in DIRS:
            if m.st(p, d) not in s:
                m.set_kelp(p, d)
    return s


def add_portal_pair(m, e1, e2, next_id):
    imgs = {m.image_edge(e1), m.image_edge(e2)}
    if (imgs & {e1, e2} and imgs != {e1, e2}) or any(e in m.portal for e in (e1, e2, *imgs)):
        return next_id  # the image would overwrite one of its own edges, or an edge is taken
    for e in (e1, e2, m.image_edge(e1), m.image_edge(e2)):
        m.kelp.discard(e)
    if {m.image_edge(e1), m.image_edge(e2)} == {e1, e2}:
        m.portal[e1] = m.portal[e2] = next_id
        return next_id + 1
    m.portal[e1] = m.portal[e2] = next_id
    m.portal[m.image_edge(e1)] = m.portal[m.image_edge(e2)] = next_id + 1
    return next_id + 2


def place_dragons(m, rng, per_side, lengths):
    taken = set()
    home = [p for p in ((x, y) for y in range(m.H) for x in range(m.W)) if m.in_half(p) and not m.in_half(m.image(p))]
    rng.shuffle(home)
    for L in lengths[:per_side]:
        for head in home:
            if head in taken or m.image(head) in taken:
                continue
            body = [head]
            ok = True
            while len(body) < L and ok:
                opts = [d for d in DIRS if not m.blocked(body[-1], d) and m.canon(body[-1], d) not in m.portal
                        and m.st(body[-1], d) not in taken and m.st(body[-1], d) not in body
                        and m.in_half(m.st(body[-1], d)) and not m.in_half(m.image(m.st(body[-1], d)))]
                if not opts:
                    ok = False
                    break
                body.append(m.st(body[-1], rng.choice(opts)))
            if not ok:
                continue
            img = [m.image(p) for p in body]
            if set(img) & (taken | set(body)):
                continue
            taken |= set(body) | set(img)
            m.dragons.append((0, body))
            m.dragons.append((1, img))
            break
    # engine order: dragon 0 is team 0's first, dragon 1 team 1's first, ...
    return len(m.dragons) == 2 * per_side


def generate(seed):
    rng = random.Random(seed)
    for attempt in range(200):
        W = rng.choice([16, 20, 24, 25, 30, 32, 32, 40, 48, 54, 60, 64])
        H = rng.choice([16, 18, 24, 25, 30, 32, 35, 40]) if W > 20 else rng.choice([16, 20])
        sym = rng.choice(["xy", "xy", "y", "x"])
        style = rng.choice(["open", "rooms", "maze", "sealed", "maze", "rooms"])
        m = Map(W, H, sym, rng)
        {"open": style_open, "rooms": style_rooms, "maze": style_maze, "sealed": style_rooms}[style](m)
        tiles = [(x, y) for y in range(H) for x in range(W)]
        bg = rng.choice([(1, 300), (1, 800), (1, 2000), (1, 4000), (0, 0)])
        for p in tiles:
            if m.in_half(p):
                m.set_gap(p, bg)
        next_id = 0
        # contested fields on the symmetry's fixed points
        pts = contested_points(m)
        rng.shuffle(pts)
        behind_portals = style == "sealed" or rng.random() < 0.3
        for i, (cx, cy) in enumerate(pts[: rng.randint(1, 2)]):
            w, h = rng.randint(2, 6), rng.randint(2, 6)
            field = block(m, cx, cy, w, h)
            field = sorted(set(field) | {m.image(p) for p in field})
            g = rng.choice([(1, 10), (1, 30), (5, 50), (1, 100)])
            for p in field:
                m.set_gap(p, g)
            if behind_portals and i == 0:
                enclose(m, field)
                # two portal pairs: from an edge inside the field to an edge out in the open
                inner = [e for p in field for e in (m.canon(p, "N"), m.canon(p, "W")) if e in m.kelp]
                rng.shuffle(inner)
                for e_in in inner[:2]:
                    for _ in range(50):
                        q = (rng.randrange(W), rng.randrange(H))
                        e_out = m.canon(q, e_in[1])
                        if (q not in field and m.st(q, OPP[e_in[1]]) not in field and m.in_half(q)
                                and e_out not in m.portal and m.image_edge(e_out) not in m.portal
                                and e_out != m.image_edge(e_out)):
                            next_id = add_portal_pair(m, e_in, e_out, next_id)
                            break
        # home sources: rich pocket, small field, or nothing
        home_kind = rng.choice(["rich", "field", "none"])
        if style == "sealed":
            # wall between the halves; only the portals above connect them
            for k in range(max(W, H)):
                if sym == "y":
                    for x in (W // 2, 0):
                        m.set_kelp((x, k % H), "W")
                else:
                    for y in (H // 2, 0):
                        m.set_kelp((k % W, y), "N")
            for _ in range(rng.randint(2, 4)):
                e1 = m.canon((rng.randrange(W), rng.randrange(H)), rng.choice("NW"))
                e2 = m.canon((rng.randrange(W), rng.randrange(H)), e1[1])
                if e1 != e2 and e1 not in m.portal and e2 not in m.portal and m.image_edge(e1) != e1:
                    next_id = add_portal_pair(m, e1, e2, next_id)
        per_side = rng.randint(2, 5)
        lengths = [rng.randint(3, 5) for _ in range(per_side)]
        if rng.random() < 0.2:
            lengths[0] = rng.randint(8, 14)
        if not place_dragons(m, rng, per_side, lengths):
            continue
        if home_kind != "none":
            head = m.dragons[0][1][0]
            cx, cy = (head[0] + rng.randint(-5, 5)) % W, (head[1] + rng.randint(-5, 5)) % H
            pocket = [p for p in block(m, cx, cy, rng.randint(2, 3), rng.randint(2, 3)) if m.in_half(p) and not m.in_half(m.image(p))]
            g = (1, 2) if home_kind == "rich" else (5, 40)
            for p in pocket:
                m.set_gap(p, g)
        # every dragon of both sides must reach the others, and most of the map
        r0 = m.reach(m.dragons[0][1][0])
        if not all(b[0] in r0 for _, b in m.dragons):
            continue
        if len(r0) < 0.5 * W * H:
            continue
        spawn_tiles = [p for p in tiles if m.gap.get(p, (0, 0))[1] > 0]
        if not any(p in r0 for p in spawn_tiles):
            continue
        return m, dict(style=style, sym=sym, W=W, H=H, bg=bg, home=home_kind, portals=len(m.portal) // 2,
                       dragons=per_side, lengths=lengths, behind_portals=behind_portals, reach=len(r0))
    raise RuntimeError(f"seed {seed}: no valid map in 200 attempts")


def style_corridors(m):
    """1-wide lanes that wrap round the map (devil): a wall between every pair of rows (or
    columns), broken every 8-14 tiles, so the swarm lines up in lanes and meets head-on."""
    r = m.rng
    horizontal = m.sym != "x" if r.random() < 0.8 else m.sym == "x"
    n_across, n_along = (m.H, m.W) if horizontal else (m.W, m.H)
    for a in range(0, n_across, 2):
        gap_at = r.randrange(n_along)
        k = 0
        while k < n_along:
            if (k - gap_at) % r.randint(8, 14) == 0:
                k += 1
                continue
            p = (k, a) if horizontal else (a, k)
            if m.in_half(p):
                m.set_kelp(p, "N" if horizontal else "W")
            k += 1
    for _ in range(m.W * m.H // 120):  # a few dead ends
        p = (r.randrange(m.W), r.randrange(m.H))
        if m.in_half(p):
            m.set_kelp(p, "W" if horizontal else "N")


STYLES = {"open": style_open, "rooms": style_rooms, "maze": style_maze, "sealed": style_rooms,
          "corridors": style_corridors}


def seal_halves(m, rng, next_id):
    """Kelp wall between the halves; only portal pairs cross it (trauma, stronghold)."""
    W, H = m.W, m.H
    for k in range(max(W, H)):
        if m.sym == "y":
            for x in (W // 2, 0):
                m.set_kelp((x, k % H), "W")
        else:
            for y in (H // 2, 0):
                m.set_kelp((k % W, y), "N")
    for _ in range(rng.randint(2, 4)):
        e1 = m.canon((rng.randrange(W), rng.randrange(H)), rng.choice("NW"))
        e2 = m.canon((rng.randrange(W), rng.randrange(H)), e1[1])
        if e1 != e2 and m.image_edge(e1) != e1:
            next_id = add_portal_pair(m, e1, e2, next_id)
    return next_id


def portal_link(m, rng, inside, next_id, tries=60):
    """One portal pair from an edge on the rim of `inside` to an edge out in the open."""
    rim = [e for p in inside for e in (m.canon(p, "N"), m.canon(p, "W")) if e in m.kelp]
    rng.shuffle(rim)
    for e_in in rim[:4]:
        for _ in range(tries):
            q = (rng.randrange(m.W), rng.randrange(m.H))
            e_out = m.canon(q, e_in[1])
            far = m.st(q, OPP[e_in[1]])
            if q in inside or far in inside or not m.in_half(q) or e_out == m.image_edge(e_out):
                continue
            before = next_id
            next_id = add_portal_pair(m, e_in, e_out, next_id)
            if next_id != before:
                return next_id
    return next_id


def home_heads(m):
    return [b[0] for t, b in m.dragons if t == 0]


def motif_contested(m, rng, next_id, enclosed=False):
    pts = contested_points(m)
    rng.shuffle(pts)
    for i, (cx, cy) in enumerate(pts[: rng.randint(1, 2)]):
        field = block(m, cx, cy, rng.randint(2, 6), rng.randint(2, 6))
        field = sorted(set(field) | {m.image(p) for p in field})
        g = rng.choice([(1, 10), (1, 30), (5, 50), (1, 100)])
        for p in field:
            m.set_gap(p, g)
        if enclosed and i == 0:
            enclose(m, field)
            for _ in range(2):
                next_id = portal_link(m, rng, set(field), next_id)
    return next_id


def motif_home_pockets(m, rng, next_id):
    """Dead-end farm pockets beside the spawns (autarky): 2x2-2x3 of fast tiles, kelp all
    round but one opening, so a short dragon can't turn round inside."""
    heads = home_heads(m)
    taken = {p for _, b in m.dragons for p in b}
    for _ in range(rng.randint(1, 3)):
        hx, hy = rng.choice(heads)
        for _try in range(30):
            cx, cy = (hx + rng.randint(-6, 6)) % m.W, (hy + rng.randint(-6, 6)) % m.H
            pocket = block(m, cx, cy, rng.randint(2, 3), 2)
            if any(not m.in_half(p) or m.in_half(m.image(p)) or p in taken for p in pocket):
                continue
            ps = enclose(m, pocket)
            rim = [(p, d) for p in pocket for d in DIRS if m.st(p, d) not in ps]
            p, d = rng.choice(rim)
            m.set_kelp(p, d, False)
            for q in pocket:
                m.set_gap(q, rng.choice([(1, 2), (1, 3)]))
            break
    return next_id


def motif_portal_pockets(m, rng, next_id):
    """Several 2x2 pockets reachable only by portal, each holding fast tiles (the portals map)."""
    taken = {p for _, b in m.dragons for p in b}
    for _ in range(rng.randint(3, 6)):
        for _try in range(30):
            cx, cy = rng.randrange(m.W), rng.randrange(m.H)
            pocket = block(m, cx, cy, 2, 2)
            if any(not m.in_half(p) or p in taken or m.image(p) in pocket for p in pocket):
                continue
            enclose(m, pocket)
            for q in pocket:
                m.set_gap(q, (1, 5))
            next_id = portal_link(m, rng, set(pocket), next_id)
            break
    return next_id


def motif_scattered(m, rng, next_id):
    for _ in range(rng.randint(3, 6)):
        cx, cy = rng.randrange(m.W), rng.randrange(m.H)
        g = rng.choice([(5, 30), (5, 40), (10, 60)])
        for p in block(m, cx, cy, 2, 2):
            m.set_gap(p, g)
    return next_id


MOTIFS = ["contested", "home_pockets", "portal_field", "portal_pockets", "scattered"]
SIZES = {"small": ([16, 18, 20, 24], [16, 18, 20]),
         "medium": ([25, 30, 32, 40], [16, 20, 24, 25, 30, 32]),
         "large": ([48, 54, 60, 63, 64], [24, 27, 30, 35, 40])}


def grid_spec(k):
    """Attribute values for the k-th grid map: every value of every attribute appears equally
    often, and each pair of attributes cycles through all combinations (coprime periods)."""
    return dict(size=["small", "medium", "large"][k % 3], sym=["xy", "y", "x"][(k // 3) % 3],
                style=["open", "rooms", "maze", "sealed", "corridors"][k % 5],
                motif=MOTIFS[(k // 5) % 5], starter=["short", "long"][(k // 15) % 2])


def generate_spec(seed, spec):
    rng = random.Random(seed)
    for attempt in range(300):
        Ws, Hs = SIZES[spec["size"]]
        W, H = rng.choice(Ws), rng.choice(Hs)
        m = Map(W, H, spec["sym"], rng)
        STYLES[spec["style"]](m)
        tiles = [(x, y) for y in range(H) for x in range(W)]
        bg = rng.choice([(1, 300), (1, 800), (1, 2000), (1, 4000), (0, 0)])
        for p in tiles:
            if m.in_half(p):
                m.set_gap(p, bg)
        next_id = 0
        if spec["style"] == "sealed":
            next_id = seal_halves(m, rng, next_id)
        per_side = rng.randint(2, 5) if spec["starter"] == "short" else rng.randint(2, 4)
        lengths = [rng.randint(3, 5) for _ in range(per_side)]
        if spec["starter"] == "long":
            lengths[0] = rng.randint(10, 14)
        if not place_dragons(m, rng, per_side, lengths):
            continue
        motif = spec["motif"]
        if motif == "contested":
            next_id = motif_contested(m, rng, next_id)
        elif motif == "portal_field":
            next_id = motif_contested(m, rng, next_id, enclosed=True)
        elif motif == "home_pockets":
            next_id = motif_home_pockets(m, rng, next_id)
            next_id = motif_contested(m, rng, next_id)
        elif motif == "portal_pockets":
            next_id = motif_portal_pockets(m, rng, next_id)
        else:
            next_id = motif_scattered(m, rng, next_id)
        if any(m.canon(b[i], d) in m.kelp or m.canon(b[i], d) in m.portal for _, b in m.dragons
               for i in range(len(b) - 1) for d in DIRS if m.st(b[i], d) == b[i + 1]):
            continue  # a pocket wall or a portal cut through a starting body
        r0 = m.reach(m.dragons[0][1][0])
        if not all(b[0] in r0 for _, b in m.dragons) or len(r0) < 0.5 * W * H:
            continue
        spawn_tiles = [p for p in tiles if m.gap.get(p, (0, 0))[1] > 0]
        if not any(p in r0 for p in spawn_tiles):
            continue
        ids = collections.Counter(m.portal.values())
        if any(v != 2 for v in ids.values()):
            continue
        return m, dict(spec, W=W, H=H, bg=list(bg), portals=len(ids), dragons=per_side, lengths=lengths, reach=len(r0))
    raise RuntimeError(f"seed {seed} {spec}: no valid map in 300 attempts")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("--count", type=int, default=12)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--grid", action="store_true", help="stratified grid of sizes, symmetries, styles, motifs, starters")
    ap.add_argument("--prefix", default="gen")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    for k in range(a.count):
        if a.grid:
            m, info = generate_spec(a.seed * 1000 + k, grid_spec(k))
        else:
            m, info = generate(a.seed * 1000 + k)
        name = f"{a.prefix}{a.seed}_{k:02d}"
        m.write(os.path.join(a.outdir, name + ".map"), name)
        with open(os.path.join(a.outdir, name + ".json"), "w") as f:
            json.dump(info, f)
        print(name, info)


if __name__ == "__main__":
    main()
