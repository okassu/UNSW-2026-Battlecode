"""Where each team eats: home / contested / enemy side, by round window (full or bench logs).

    python3 tools/zones.py LOG [LOG ...]

Tiles are classed by walking distance (kelp respected, portals ignored) from each team's start heads:
contested when the two distances differ by <= 3. Also the first round each team eats on contested ground.
"""
import collections, pathlib, re, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ladder, topgames

WIN = ((0, 25), (25, 50), (50, 100), (100, 200), (200, 500))


def bfs(mp, starts):
    W, H, edges = mp["W"], mp["H"], mp["edges"]
    dist = {s: 0 for s in starts}
    q = list(starts)
    for c in q:
        x, y = c
        for d, (dx, dy) in ladder.D.items():
            if edges.get(ladder.eidx(W, H, x, y, d), 0) != 0:
                continue
            n = ((x + dx) % W, (y + dy) % H)
            if n not in dist:
                dist[n] = dist[c] + 1
                q.append(n)
    return dist


def zones(log, mp):
    heads = [b[0] for b in mp["dragons"]]
    d0 = bfs(mp, [h for i, h in enumerate(heads) if i % 2 == 0])
    d1 = bfs(mp, [h for i, h in enumerate(heads) if i % 2 == 1])
    team, pend = {}, set()
    cnt = {t: collections.Counter() for t in (0, 1)}
    first_c = {}
    for line in open(log, errors="replace"):
        p = line.rstrip("\n").split("\t")
        if len(p) < 2 or not p[0].isdigit():
            continue
        r, ev = int(p[0]), p[1]
        m = topgames.RX["split"].match(ev)
        if m:
            team[int(m[2])] = team.get(int(m[1]), int(m[1]) % 2)
            continue
        m = topgames.RX["taken"].match(ev)
        if m:
            pend.add((int(m[1]), int(m[2])))
            continue
        m = topgames.RX["move"].match(ev)
        if m and m[5]:
            xy = (int(m[3]), int(m[4]))
            if xy not in pend:
                continue
            pend.discard(xy)
            t = team.get(int(m[1]), int(m[1]) % 2)
            a, b = (d0, d1) if t == 0 else (d1, d0)
            if xy not in a or xy not in b:
                z = "far"
            else:
                diff = a[xy] - b[xy]
                z = "home" if diff < -3 else "enemy" if diff > 3 else "cont"
            if z == "cont":
                first_c.setdefault(t, r)
            for lo, hi in WIN:
                if lo <= r < hi:
                    cnt[t][(lo, z)] += 1
    return cnt, first_c


def main():
    maps = [ladder.load_map(str(p)) for p in sorted(pathlib.Path("maps").glob("*.map"))]
    for log in sys.argv[1:]:
        mp = ladder.identify(log, maps)
        if not mp:
            continue
        cnt, fc = zones(log, mp)
        print(f"{pathlib.Path(log).name:14s} {pathlib.Path(mp['path']).stem[:10]:10s}", end="")
        for t in (0, 1):
            s = " ".join(f"{cnt[t][(lo,'home')]}/{cnt[t][(lo,'cont')]}/{cnt[t][(lo,'enemy')]}" for lo, _ in WIN)
            print(f" | {'E' if t == 0 else 'O'} 1stC r{fc.get(t, '-')} {s}", end="")
        print()


if __name__ == "__main__":
    main()
