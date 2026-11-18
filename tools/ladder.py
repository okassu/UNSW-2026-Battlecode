"""Report on a batch of ladder games pasted into prevBattles/gameN.txt.

    python3 tools/ladder.py [prevBattles/game*.txt ...]

For each non-empty log: which map it was (start positions next to the map's dragon heads, else
the same-size map whose kelp no move ever crosses), our side, the result, the dumbstats table,
and how each side used ground that only portals reach (first entry, eats inside per 100 rounds).
Ends with one summary line per game.
"""
from __future__ import annotations

import collections
import glob
import os
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dumbstats  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
D = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
MOVE = re.compile(r"dragon (\d+) moves ([NESW]+) to \((\d+),(\d+)\)")


def load_map(p):
    W = H = 0
    edges, gap, heads = {}, {}, []
    for l in open(p):
        t = l.split()
        if not t:
            continue
        if t[0] == "MAP":
            W, H = int(t[1]), int(t[2])
        elif t[0] == "EDGE":
            edges[int(t[1])] = int(t[2])
        elif t[0] == "TILE":
            gap[(int(t[1]), int(t[2]))] = (int(t[3]), int(t[4]))
        elif t[0] == "DRAGON":
            n = int(t[2])
            heads.append([(int(t[3 + 2 * i]), int(t[4 + 2 * i])) for i in range(n)])
    return dict(path=p, W=W, H=H, edges=edges, gap=gap, dragons=heads)


def eidx(W, H, x, y, d):
    if d == "N":
        return 2 * y * (W + 1) + x
    if d == "S":
        return 2 * ((y + 1) % H) * (W + 1) + x
    if d == "W":
        return (2 * y + 1) * (W + 1) + x
    return (2 * y + 1) * (W + 1) + (x + 1) % W


def identify(log, maps):
    first, moves, pos = {}, [], {}
    mx = my = 0
    for l in open(log, errors="replace"):
        p = l.rstrip("\n").split("\t")
        if len(p) < 2 or not p[0].isdigit():
            continue
        m = MOVE.match(p[1])
        if not m:
            continue
        d, xy = int(m[1]), (int(m[3]), int(m[4]))
        mx, my = max(mx, xy[0]), max(my, xy[1])
        if p[0] == "1" and d not in first:
            first[d] = xy
        if d in pos and len(m[2]) == 1 and len(moves) < 20000:
            moves.append((pos[d], m[2], xy))
        pos[d] = xy
    cands = [mp for mp in maps if mp["W"] > mx and mp["H"] > my]
    for mp in cands:  # start positions
        W, H = mp["W"], mp["H"]
        ok = n = 0
        for i, body in enumerate(mp["dragons"]):
            if i in first:
                n += 1
                h, (x, y) = body[0], first[i]
                if min(abs(x - h[0]), W - abs(x - h[0])) + min(abs(y - h[1]), H - abs(y - h[1])) == 1:
                    ok += 1
        if n and ok == n:
            return mp
    best, bad_best = None, None
    for mp in cands:  # kelp consistency
        W, H = mp["W"], mp["H"]
        bad = sum(1 for (x, y), d, to in moves if ((x + D[d][0]) % W, (y + D[d][1]) % H) == to
                  and mp["edges"].get(eidx(W, H, x, y, d), 0) == 1)
        if bad_best is None or bad < bad_best or (bad == bad_best and mp["W"] * mp["H"] < best["W"] * best["H"]):
            best, bad_best = mp, bad
    return best if bad_best == 0 else None


def portal_only(mp):
    W, H, edges = mp["W"], mp["H"], mp["edges"]
    start = [b[0] for b in mp["dragons"]]
    seen, q = set(start), list(start)
    for (x, y) in q:
        for d, (dx, dy) in D.items():
            if edges.get(eidx(W, H, x, y, d), 0) != 0:
                continue
            n = ((x + dx) % W, (y + dy) % H)
            if n not in seen:
                seen.add(n)
                q.append(n)
    return {p for p in mp["gap"] if p not in seen}


def portal_usage(log, ponly):
    team, ours, pend = {}, None, set()
    first, eat = {}, collections.Counter()
    for l in open(log, errors="replace"):
        p = l.rstrip("\n").split("\t")
        if len(p) < 2 or not p[0].isdigit():
            continue
        r, ev = int(p[0]), p[1]
        m = re.match(r"dragon (\d+): ", ev)
        if m and ours is None:
            ours = int(m[1]) % 2
        m = re.match(r"dragon (\d+) splits off dragon (\d+)", ev)
        if m:
            team[int(m[2])] = team.get(int(m[1]), int(m[1]) % 2)
            continue
        m = re.match(r"\((\d+),(\d+)\) pearl taken", ev)
        if m:
            pend.add((int(m[1]), int(m[2])))
            continue
        m = re.match(r"dragon (\d+) moves \w+ to \((\d+),(\d+)\)( and grows)?", ev)
        if m:
            d, xy = int(m[1]), (int(m[2]), int(m[3]))
            s = "us" if team.get(d, d % 2) == ours else "them"
            if xy in ponly:
                first.setdefault(s, r)
                if m[4] and xy in pend:
                    eat[s] += 1
            if m[4]:
                pend.discard(xy)
    return first, eat


def main():
    logs = sys.argv[1:] or sorted(glob.glob(str(ROOT / "prevBattles" / "game*.txt")),
                                  key=lambda s: int(re.sub(r"\D", "", os.path.basename(s)) or 0))
    maps = [load_map(p) for p in sorted(glob.glob(str(ROOT / "maps" / "*.map")))]
    summary = []
    for log in logs:
        if os.path.getsize(log) == 0:
            continue
        mp = identify(log, maps)
        name = pathlib.Path(mp["path"]).stem if mp else "unknown"
        a = dumbstats.analyse(log, mp["path"] if mp else None)
        print(f"\n### {os.path.basename(log)}: {name}")
        dumbstats.report(a)
        extra = ""
        if mp:
            po = portal_only(mp)
            if po:
                first, eat = portal_usage(log, po)
                extra = (f"portal-only ground ({len(po)} tiles): first entry us {first.get('us', '-')} them "
                         f"{first.get('them', '-')}, eats inside us {eat['us']} them {eat['them']}")
                print("  " + extra)
        last = max(a["alive_at"])
        summary.append(f"{os.path.basename(log):12s} {name:16s} {a['result']:16s} r{a['end']:<3d} eats {a['eats']['us']:4d} v "
                       f"{a['eats']['them']:<4d} dragons@r50 {a['alive_at'].get(50, a['alive_at'][last])}")
    print("\n### summary")
    for s in summary:
        print(s)


if __name__ == "__main__":
    main()
