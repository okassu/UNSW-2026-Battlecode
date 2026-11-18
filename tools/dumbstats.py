"""Dumb-behaviour report for game logs in the ladder format (prevBattles/gameN.txt, or the
logs `tools/bench.py --logdir` writes). One table per log, us vs them, so a ladder game is
diagnosed in seconds and two versions can be compared on the same opponent.

    python3 tools/dumbstats.py LOG [LOG ...] [--map maps/x.map] [--sum]

Our side is the parity of the first indicator line (only our bot's indicators are in a log).
Rates are per 100 dragon-moves unless noted. Definitions:
  loop      move inside a stall: the last 8 moves covered <= max(4, L+1) tiles without growing
  stub      death within 2 rounds of that dragon splitting (U-turn stubs; deliberate)
  selfhit   "hit itself" that is not a stub
  friendly  head-on between two dragons of the same side
  trade     head-on vs the enemy, by who started it and local heads (within 3 tiles): behind/even/ahead
  drops     pearls eaten within 3 tiles of a trade in the next 10 rounds, us:them
  xdeath    deaths within 2 rounds of a portal crossing (a move to a non-adjacent tile)
"""
from __future__ import annotations

import argparse
import collections
import re

MOVE = re.compile(r"dragon (\d+) moves ([NESW]+) to \((\d+),(\d+)\)(?: and grows to (\d+))?")
SPLIT = re.compile(r"dragon (\d+) splits off dragon (\d+), (\d+) cells")
DIES = re.compile(r"dragon (\d+) dies\. Reason: (.*)")
IND = re.compile(r"dragon (\d+): (.*)")
TAKEN = re.compile(r"\((\d+),(\d+)\) pearl taken")
D = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}


def load_map(path):
    W = H = 0
    init = []
    for l in open(path):
        t = l.split()
        if t and t[0] == "MAP":
            W, H = int(t[1]), int(t[2])
        elif t and t[0] == "DRAGON":
            init.append(int(t[2]))
    return W, H, init


def rows(path):
    for line in open(path, errors="replace"):
        p = line.rstrip("\n").split("\t")
        if len(p) >= 2 and p[0].isdigit():
            yield int(p[0]), p[1]


def analyse(path, mapf=None, side_override=None):
    """side_override: parity to call 'us' in logs without our indicators (full ladder logs)."""
    W = H = 0
    init = []
    if mapf:
        W, H, init = load_map(mapf)
    if not W:  # infer from coordinates seen
        for _, ev in rows(path):
            m = MOVE.match(ev)
            if m:
                W, H = max(W, int(m[3]) + 1), max(H, int(m[4]) + 1)

    def cheb(a, b):
        dx = abs(a[0] - b[0]); dy = abs(a[1] - b[1])
        return max(min(dx, W - dx), min(dy, H - dy))

    team, ln, pos, alive = {}, {i: L for i, L in enumerate(init)}, {}, set()
    ours = side_override
    moved_round, split_round, crossed = {}, {}, {}
    trail = collections.defaultdict(collections.deque)  # dragon -> deque of (pos, len)
    moves = collections.Counter(); loops = collections.Counter()
    eats = collections.Counter(); eat_log = []
    deaths = collections.defaultdict(collections.Counter)
    alive_at, longest_at = {}, {}
    last_die = None
    trades, friendly, xdeath = [], collections.Counter(), collections.Counter()
    pend_taken = set()
    last_round = 0

    def side(d):
        return "us" if team.get(d, d % 2) == ours else "them"

    def snapshot(r):
        us = [ln.get(d) or 0 for d in alive if side(d) == "us"]
        th = [ln.get(d) or 0 for d in alive if side(d) == "them"]
        alive_at[r] = (len(us), len(th))
        longest_at[r] = (max(us, default=0), max(th, default=0))

    for r, ev in rows(path):
        if r != last_round:
            for k in range(last_round + 1, r + 1):
                if k in (50, 100, 200, 300, 400) and ours is not None:
                    snapshot(k)
            last_round = r
        m = IND.match(ev)
        if m:
            d = int(m[1])
            if ours is None:
                ours = d % 2
            continue
        m = SPLIT.match(ev)
        if m:
            a, b, k = int(m[1]), int(m[2]), int(m[3])
            team[b] = team.get(a, a % 2)
            ln[b] = k
            if ln.get(a) is not None:
                ln[a] -= k
            split_round[a] = r
            alive.add(b)
            continue
        m = TAKEN.match(ev)
        if m:
            pend_taken.add((int(m[1]), int(m[2])))
            continue
        m = MOVE.match(ev)
        if m:
            d, dirs, xy = int(m[1]), m[2], (int(m[3]), int(m[4]))
            team.setdefault(d, d % 2)
            alive.add(d)
            if m[5]:
                if xy in pend_taken:
                    eats[side(d)] += 1
                    eat_log.append((r, xy, side(d)))
                    pend_taken.discard(xy)
                ln[d] = int(m[5])
            elif moved_round.get(d) == r and ln.get(d) is not None:
                ln[d] -= 1  # sprint step
            if d in pos and len(dirs) == 1:
                dx, dy = D[dirs]
                if ((pos[d][0] + dx) % W, (pos[d][1] + dy) % H) != xy:
                    crossed[d] = r
            moved_round[d] = r
            pos[d] = xy
            s = side(d)
            moves[s] += 1
            t = trail[d]
            t.append((xy, ln.get(d) or 0))
            if len(t) > 8:
                t.popleft()
            if len(t) == 8 and t[-1][1] <= t[0][1]:
                if len({p for p, _ in t}) <= max(4, (ln.get(d) or 0) + 1):
                    loops[s] += 1
            continue
        m = DIES.match(ev)
        if m:
            d, why = int(m[1]), m[2]
            s = side(d)
            stub = d in split_round and r - split_round[d] <= 2
            key = "stub" if stub else {"hit itself": "selfhit", "hit a body": "body", "hit a wall": "wall",
                                       "head-on collision": "headon", "gave no valid action": "noaction"}.get(why, why)
            deaths[s][key] += 1
            if d in crossed and r - crossed[d] <= 2:
                xdeath[s] += 1
            if why == "head-on collision":
                if last_die and last_die[0] == r:
                    v, xy = last_die[1], last_die[2]
                    a = d
                    if side(v) == side(a):
                        friendly[side(v)] += 1
                    else:
                        near = collections.Counter()
                        for o in alive:
                            if o not in (a, v) and o in pos and xy and cheb(pos[o], xy) <= 3:
                                near[side(o)] += 1
                        local = ("ahead" if near["us"] > near["them"] else
                                 "behind" if near["us"] < near["them"] else "even")
                        trades.append((r, side(a), local, xy))
                    last_die = None
                else:
                    last_die = (r, d, pos.get(d))
            alive.discard(d)
            trail.pop(d, None)
            continue
    snapshot(last_round)
    drops = collections.defaultdict(collections.Counter)
    for tr, starter, local, xy in trades:
        for er, exy, es in eat_log:
            if xy and tr <= er <= tr + 10 and cheb(exy, xy) <= 3:
                drops[local][es] += 1
    us_n, th_n = alive_at[last_round]
    result = ("eliminated US" if not us_n else "eliminated THEM" if not th_n else
              "WIN" if longest_at[last_round][0] > longest_at[last_round][1] else
              "LOSS" if longest_at[last_round][0] < longest_at[last_round][1] else "TIE?")
    return dict(path=path, W=W, H=H, ours=ours, end=last_round, result=result, moves=moves, loops=loops,
                eats=eats, deaths=deaths, alive_at=alive_at, longest_at=longest_at, trades=trades,
                friendly=friendly, xdeath=xdeath, drops=drops)


def report(a):
    per = lambda n, s: 100.0 * n / max(1, a["moves"][s])
    side = "EVEN" if a["ours"] == 0 else "ODD"
    print(f"== {a['path']}  ({a['W']}x{a['H']}, we are {side})  end r{a['end']}: {a['result']}")
    al = "  ".join(f"r{r} {u}v{t}" for r, (u, t) in sorted(a["alive_at"].items()))
    lo = "  ".join(f"r{r} {u}v{t}" for r, (u, t) in sorted(a["longest_at"].items()) if r >= 300)
    print(f"  dragons  {al}")
    print(f"  longest  {lo}")
    print(f"  eats {a['eats']['us']} v {a['eats']['them']}   moves {a['moves']['us']} v {a['moves']['them']}")
    for s in ("us", "them"):
        d = a["deaths"][s]
        print(f"  {s:4s} loop {per(a['loops'][s], s):4.1f}%  deaths: " +
              " ".join(f"{k} {d[k]}" for k in ("stub", "selfhit", "body", "wall", "headon", "noaction") if d[k]) +
              f"  friendly {a['friendly'][s]}  xdeath {a['xdeath'][s]}")
    tc = collections.Counter((st, loc) for _, st, loc, _ in a["trades"])
    for st in ("them", "us"):
        print(f"  trades started by {st:4s}: " + "  ".join(f"{loc} {tc[(st, loc)]}" for loc in ("behind", "even", "ahead")))
    print("  drops us:them  " + "  ".join(f"{loc} {a['drops'][loc]['us']}:{a['drops'][loc]['them']}"
                                          for loc in ("behind", "even", "ahead")))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--map", help="map file (exact size and starting lengths); else inferred")
    ap.add_argument("--sum", action="store_true", help="also print totals over all logs")
    args = ap.parse_args()
    tot = collections.Counter()
    for p in args.logs:
        a = analyse(p, args.map)
        report(a)
        tot["games"] += 1
        tot["win"] += a["result"] in ("WIN", "eliminated THEM")
        for s in ("us", "them"):
            tot[f"moves_{s}"] += a["moves"][s]
            tot[f"loops_{s}"] += a["loops"][s]
            tot[f"eats_{s}"] += a["eats"][s]
            tot[f"friendly_{s}"] += a["friendly"][s]
            tot[f"xdeath_{s}"] += a["xdeath"][s]
            for k, v in a["deaths"][s].items():
                tot[f"{k}_{s}"] += v
        for _, st, loc, _ in a["trades"]:
            tot[f"trade_{st}_{loc}"] += 1
    if args.sum and tot["games"] > 1:
        per = lambda s: 100.0 * tot[f"loops_{s}"] / max(1, tot[f"moves_{s}"])
        print(f"== TOTAL {tot['games']} games, won {tot['win']}")
        print(f"  eats {tot['eats_us']} v {tot['eats_them']}   loop {per('us'):.1f}% v {per('them'):.1f}%")
        for s in ("us", "them"):
            print(f"  {s:4s} " + " ".join(f"{k} {tot[k + '_' + s]}" for k in
                                          ("stub", "selfhit", "body", "wall", "headon", "noaction", "friendly", "xdeath")))
        for st in ("them", "us"):
            print(f"  trades started by {st:4s}: " + "  ".join(f"{loc} {tot[f'trade_{st}_{loc}']}" for loc in ("behind", "even", "ahead")))


if __name__ == "__main__":
    main()
