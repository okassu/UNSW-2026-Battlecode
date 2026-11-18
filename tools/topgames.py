"""Two-sided report on full ladder logs (both teams' pings visible, no indicators).

    python3 tools/topgames.py prevBattles/game21.txt ... [--detail]

Teams: EVEN = descendants of even root ids, ODD = odd roots (children inherit through splits).
Per game and team: result, dragons and eats at checkpoints, first eat / first split, split sizes and
the parent's length when splitting, deaths by reason, sacrifices (no valid action) and when they start,
head-ons, sprints, portal crossings, the longest dragon over time and when the final carrier was born,
and the sonar fingerprint (pings per dragon-round, directions per ping round, value classes, echo mix).
"""
from __future__ import annotations

import collections
import os
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ladder  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
RX = {
    "move": re.compile(r"dragon (\d+) moves ([NESW]+) to \((\d+),(\d+)\)(?: and grows to (\d+))?"),
    "ask": re.compile(r"dragon (\d+) asks to move ([NESW]+)"),
    "split": re.compile(r"dragon (\d+) splits off dragon (\d+), (\d+) cells"),
    "ping": re.compile(r"dragon (\d+) pings (\d+) ([NESW]) and (?:reaches dragon (\d+) \(echo: ([a-z ]+)\)|stops at kelp|(.*))"),
    "die": re.compile(r"dragon (\d+) dies\. Reason: (.*)"),
    "noact": re.compile(r"dragon (\d+) offers nothing"),
    "taken": re.compile(r"\((\d+),(\d+)\) pearl taken"),
}
CHECK = (10, 25, 50, 100, 150, 200, 300, 400, 500)


def analyse(path, mp):
    W, H = (mp["W"], mp["H"]) if mp else (0, 0)
    team, length, alive, born = {}, {}, set(), {}
    if mp:
        for i, body in enumerate(mp["dragons"]):
            team[i], length[i], born[i] = i % 2, len(body), 0
            alive.add(i)
    pos = {}
    S = {t: collections.Counter() for t in (0, 1)}
    split_sizes = {0: collections.Counter(), 1: collections.Counter()}
    split_parent_len = {0: collections.Counter(), 1: collections.Counter()}
    deaths = {0: collections.Counter(), 1: collections.Counter()}
    sac_rounds = {0: [], 1: []}
    alive_at = {0: {}, 1: {}}
    eats_at = {0: {}, 1: {}}
    long_at = {0: {}, 1: {}}
    first = {0: {}, 1: {}}
    pend = set()
    ping_dirs = {0: collections.Counter(), 1: collections.Counter()}  # dirs used per (dragon, round)
    cur_ping = collections.defaultdict(set)
    ping_vals = {0: collections.Counter(), 1: collections.Counter()}
    echo = {0: collections.Counter(), 1: collections.Counter()}
    dragon_rounds = {0: 0, 1: 0}
    headon = {0: 0, 1: 0}
    cross = {0: 0, 1: 0}
    last_round = 0
    moved_this_round = set()
    cur = 0
    eaten_tiles = {0: collections.Counter(), 1: collections.Counter()}
    pending_die = []

    def close_round(r):
        for t in (0, 1):
            n = sum(1 for d in alive if team.get(d, d % 2) == t)
            dragon_rounds[t] += n
            for c in CHECK:
                if r == c:
                    alive_at[t][c] = n
                    eats_at[t][c] = S[t]["eats"]
                    long_at[t][c] = max([length.get(d, 0) for d in alive if team.get(d, d % 2) == t] or [0])
        for (d, rr), dirs in cur_ping.items():
            ping_dirs[team.get(d, d % 2)][len(dirs)] += 1
        cur_ping.clear()

    for line in open(path, errors="replace"):
        p = line.rstrip("\n").split("\t")
        if len(p) < 2 or not p[0].isdigit():
            continue
        r, ev = int(p[0]), p[1]
        if r != cur:
            for rr in range(cur, r):
                close_round(rr)
            cur = r
            # resolve head-ons (victim then attacker, same round)
        last_round = r
        m = RX["move"].match(ev)
        if m:
            d, dirs, xy = int(m[1]), m[2], (int(m[3]), int(m[4]))
            t = team.setdefault(d, d % 2)
            alive.add(d)
            if len(dirs) > 1:
                S[t]["sprint"] += 1
                length[d] = length.get(d, 2) - (len(dirs) - 1)
            if m[5]:
                length[d] = int(m[5])
                if xy in pend:
                    S[t]["eats"] += 1
                    eaten_tiles[t][xy] += 1
                    first[t].setdefault("eat", r)
                    pend.discard(xy)
            if d in pos and W and len(dirs) == 1:
                px, py = pos[d]
                dx, dy = ladder.D[dirs]
                if ((px + dx) % W, (py + dy) % H) != xy:
                    cross[t] += 1
            pos[d] = xy
            continue
        m = RX["taken"].match(ev)
        if m:
            pend.add((int(m[1]), int(m[2])))
            continue
        m = RX["ping"].match(ev)
        if m:
            d, v, dr = int(m[1]), int(m[2]), m[3]
            t = team.get(d, d % 2)
            S[t]["pings"] += 1
            cur_ping[(d, r)].add(dr)
            ping_vals[t][v] += 1
            echo[t][m[5] if m[5] else ("kelp" if "kelp" in ev else "other:" + (m[6] or ""))] += 1
            continue
        m = RX["split"].match(ev)
        if m:
            a, c, k = int(m[1]), int(m[2]), int(m[3])
            t = team.get(a, a % 2)
            team[c] = t
            split_sizes[t][k] += 1
            split_parent_len[t][length.get(a, 0)] += 1
            length[a] = length.get(a, 0) - k
            length[c] = k
            born[c] = r
            alive.add(c)
            S[t]["splits"] += 1
            first[t].setdefault("split", r)
            continue
        m = RX["die"].match(ev)
        if m:
            d, why = int(m[1]), m[2]
            t = team.get(d, d % 2)
            deaths[t][why] += 1
            if why.startswith("gave no valid"):
                sac_rounds[t].append((r, length.get(d, 0)))
            if why.startswith("head-on"):
                headon[t] += 1
            alive.discard(d)
            continue
    close_round(cur)
    # winner
    res = {}
    for t in (0, 1):
        n = sum(1 for d in alive if team.get(d, d % 2) == t)
        res[t] = (n, max([length.get(d, 0) for d in alive if team.get(d, d % 2) == t] or [0]))
    if res[0][0] == 0 and res[1][0] > 0:
        winner = 1
    elif res[1][0] == 0 and res[0][0] > 0:
        winner = 0
    else:
        winner = 0 if res[0][1] > res[1][1] else 1 if res[1][1] > res[0][1] else None
    carrier = {}
    for t in (0, 1):
        live = [d for d in alive if team.get(d, d % 2) == t]
        if live:
            d = max(live, key=lambda x: length.get(x, 0))
            carrier[t] = (d, born.get(d, 0), length.get(d, 0))
    return dict(path=path, end=last_round, winner=winner, res=res, S=S, split_sizes=split_sizes,
                split_parent_len=split_parent_len, deaths=deaths, sac=sac_rounds, alive_at=alive_at,
                eats_at=eats_at, long_at=long_at, first=first, ping_dirs=ping_dirs, ping_vals=ping_vals,
                echo=echo, dragon_rounds=dragon_rounds, headon=headon, cross=cross, carrier=carrier,
                eaten_tiles=eaten_tiles)


def fingerprint(a, t):
    pv = a["ping_vals"][t]
    n = sum(pv.values())
    if not n:
        return "silent"
    zero = pv.get(0, 0) / n
    top = pv.most_common(3)
    distinct = len(pv)
    dirs = a["ping_dirs"][t]
    nd = sum(dirs.values())
    dmix = " ".join(f"{k}d:{100 * v / nd:.0f}%" for k, v in sorted(dirs.items()))
    rate = n / max(1, a["dragon_rounds"][t])
    ec = a["echo"][t]
    ne = sum(ec.values())
    emix = " ".join(f"{k}:{100 * v / ne:.0f}%" for k, v in ec.most_common(6))
    return (f"pings/dragon-round {rate:.2f}; {dmix}; zero {100 * zero:.0f}%, distinct {distinct}, top "
            f"{[(hex(v), c) for v, c in top]}; echo {emix}")


def main():
    args = [x for x in sys.argv[1:] if not x.startswith("--")]
    maps = [ladder.load_map(p) for p in sorted(pathlib.Path(ROOT / "maps").glob("*.map"))]
    rows = []
    for log in args:
        if os.path.getsize(log) == 0:
            continue
        mp = ladder.identify(log, maps)
        name = pathlib.Path(mp["path"]).stem if mp else "unknown"
        a = analyse(log, mp)
        w = {0: "EVEN", 1: "ODD", None: "draw"}[a["winner"]]
        print(f"\n### {os.path.basename(log)} {name} ({mp['W']}x{mp['H']})  end r{a['end']}  winner {w}  "
              f"final (dragons, longest) EVEN {a['res'][0]} ODD {a['res'][1]}")
        for t in (0, 1):
            nm = "EVEN" if t == 0 else "ODD "
            S = a["S"][t]
            al = " ".join(f"{a['alive_at'][t].get(c, '-')}" for c in CHECK)
            ea = " ".join(f"{a['eats_at'][t].get(c, '-')}" for c in CHECK)
            lo = " ".join(f"{a['long_at'][t].get(c, '-')}" for c in CHECK)
            ss = dict(a["split_sizes"][t].most_common(4))
            sp = dict(a["split_parent_len"][t].most_common(5))
            sac = a["sac"][t]
            sac_s = f"{len(sac)} from r{sac[0][0]}" if sac else "0"
            car = a["carrier"].get(t)
            print(f"  {nm} dragons@{CHECK}: {al}")
            print(f"       eats: {ea}   longest: {lo}")
            print(f"       first eat r{a['first'][t].get('eat', '-')} split r{a['first'][t].get('split', '-')}; "
                  f"splits {S['splits']} sizes {ss} parentL {sp}; sprints {S['sprint']}; crossings {a['cross'][t]}")
            print(f"       deaths {dict(a['deaths'][t])}; sacrifices {sac_s}; carrier id,born,L {car}")
            print(f"       sonar: {fingerprint(a, t)}")
        rows.append((os.path.basename(log), name, w, a))
    print("\n### summary (winner first)")
    for f, name, w, a in rows:
        wt = a["winner"] if a["winner"] is not None else 0
        lt = 1 - wt
        print(f"{f:11s} {name:16s} r{a['end']:<3d} win {w:4s} d@50 {a['alive_at'][wt].get(50, '-')}v{a['alive_at'][lt].get(50, '-')}"
              f" eats@50 {a['eats_at'][wt].get(50, '-')}v{a['eats_at'][lt].get(50, '-')}"
              f" eats@100 {a['eats_at'][wt].get(100, '-')}v{a['eats_at'][lt].get(100, '-')}"
              f" long@300 {a['long_at'][wt].get(300, '-')}v{a['long_at'][lt].get(300, '-')}"
              f" final {a['res'][wt]}v{a['res'][lt]}")


if __name__ == "__main__":
    main()
