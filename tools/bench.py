"""Headless match runner for local testing.

Plays bot A against bot B on one or more maps (in parallel, both colour
orders) and prints winner, reason, longest living dragon per team and death
causes. Run with the toolkit's own interpreter, which has the engine:

    ~/.local/share/uv/tools/unswbc/bin/python tools/bench.py mybot other [maps...]

Bots are project dirs (rebuilt once up front) or prebuilt executables.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures as cf
import os
import pathlib
import re
import sys

from unswbc.bot import Bot, Pool
from unswbc.engine import EngineModule
from unswbc.run import DEATH_REASONS, _resolve

LEN_RE = re.compile(rb"^LENGTH (\d+)$", re.M)


LADDER_REASONS = {"W": "hit a wall", "S": "hit itself", "O": "hit a body", "H": "head-on collision",
                  "A": "gave no valid action"}


def play(map_path: str, argv_a: list[str], dir_a: str, argv_b: list[str], dir_b: str, seed: int = 0,
         log_path: str | None = None, log_team: str = "A", header: str = "") -> dict:
    pools = {"A": Pool(argv_a, cwd=dir_a, key=f"{seed:016x}-a"), "B": Pool(argv_b, cwd=dir_b, key=f"{seed:016x}-b")}
    live: dict[int, Bot] = {}
    teams: dict[int, str] = {}
    lengths: dict[int, int] = {}
    deaths = {"A": collections.Counter(), "B": collections.Counter()}
    peak = {"A": 0, "B": 0}
    count = {"A": 0, "B": 0}
    # Ladder-format trace (the lines the site's game log shows), so tools written for
    # ladder logs (tools/dumbstats.py) read local games too. Indicators only for log_team,
    # as on the ladder, where only our bot's indicator strings are visible to us.
    trace: list[tuple[int, int, str]] = []
    pending: dict[int, tuple[int, str, int]] = {}  # dragon -> (round, dirs, length) of its last move
    split_ask: list = [None]

    def emit(r: int, text: str) -> None:
        trace.append((r, len(trace), text))

    def spawn(dragon_id: int, init: bytes) -> None:
        team = next((l.split()[1] for l in init.decode().splitlines() if l.startswith("TEAM")), "A")
        teams[dragon_id] = team
        live[dragon_id] = Bot(pools[team], init=init, name=str(dragon_id))
        count[team] += 1
        if log_path and split_ask[0] is not None:
            parent, k, r = split_ask[0]
            emit(r, f"dragon {parent} splits off dragon {dragon_id}, {k} cells")
            split_ask[0] = None

    def reply(dragon_id: int, block: bytes) -> bytes:
        m = LEN_RE.search(block)
        if m:
            lengths[dragon_id] = int(m.group(1))
        out = live[dragon_id].ask(block)
        if log_path:
            text = block.decode(errors="replace").split("\n")
            r = int(text[0].split()[1]) + 1 if text[0].startswith("ROUND") else 0
            length = lengths.get(dragon_id, 0)
            head = None
            for l in text:
                t = l.split()
                if len(t) == 6 and t[1] == str(dragon_id) and t[5] == "1":
                    head = (int(t[2]), int(t[3]))
            if dragon_id in pending and head is not None:
                pr, dirs, plen = pending.pop(dragon_id)
                if plen and length > plen:
                    emit(pr, f"({head[0]},{head[1]}) pearl taken")
                    emit(pr, f"dragon {dragon_id} moves {dirs} to ({head[0]},{head[1]}) and grows to {length}")
                else:
                    emit(pr, f"dragon {dragon_id} moves {dirs} to ({head[0]},{head[1]})")
            for l in out.decode(errors="replace").splitlines():
                t = l.split()
                if not t:
                    continue
                if t[0] == "MOVE" and len(t) > 1:
                    pending[dragon_id] = (r, "".join(t[1:]), length)
                elif t[0] == "SPLIT" and len(t) > 1:
                    split_ask[0] = (dragon_id, int(t[1]), r)
                    pending.pop(dragon_id, None)
                elif t[0] == "INDICATOR" and teams.get(dragon_id) == log_team:
                    emit(r, f"dragon {dragon_id}: {l[10:]}")
        return out

    def death(dragon_id: int, round_num: int, reason: str) -> None:
        if log_path:
            pending.pop(dragon_id, None)
            emit(round_num + 1, f"dragon {dragon_id} dies. Reason: {LADDER_REASONS.get(reason, reason)}")
        deaths[teams.get(dragon_id, "?")][DEATH_REASONS.get(reason, reason)] += 1
        lengths.pop(dragon_id, None)
        bot = live.pop(dragon_id, None)
        if bot is not None:
            bot.stop()

    try:
        result = EngineModule().run(pathlib.Path(map_path).read_bytes(), reply, death, spawn,
                                    lambda line: None, 0, seed)
    finally:
        for bot in live.values():
            bot.stop()
        for pool in pools.values():
            pool.close()
    if log_path:
        with open(log_path, "w") as f:
            f.write(header + "\n\n")
            for r, _, text in sorted(trace):
                f.write(f"{r}\t{text}\n")
    longest = {"A": 0, "B": 0}
    for d, n in lengths.items():
        t = teams[d]
        longest[t] = max(longest[t], n)
    return {
        "map": pathlib.Path(map_path).stem, "rounds": result.rounds + 1, "winner": result.winner,
        "reason": result.end_reason, "longest": longest,
        "alive": {"A": result.a_dragons, "B": result.b_dragons},
        "total": {"A": result.a_length, "B": result.b_length},
        "spawned": count, "deaths": deaths,
    }


def play_seeded(seed: int, *args, **kw) -> dict:
    # Runs in a worker process: bots inherit DRAGON_SEED, which varies their tie-breaks,
    # and the engine (unswbc >= 1.1) takes the same seed for pearl respawn timing.
    os.environ["DRAGON_SEED"] = str(seed)
    return play(*args, seed=seed, **kw)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("maps", nargs="*")
    ap.add_argument("-j", type=int, default=8)
    ap.add_argument("--one-way", action="store_true", help="only play a as team A")
    ap.add_argument("-d", "--deaths", action="store_true", help="print death causes")
    ap.add_argument("-s", "--seeds", type=int, default=1, help="games per map and colour (seeds 0..n-1: DRAGON_SEED and engine)")
    ap.add_argument("-q", "--quiet", action="store_true", help="only print the per-map summary")
    ap.add_argument("--logdir", help="write a ladder-format log per game here (a's indicators only)")
    args = ap.parse_args()
    maps = args.maps or sorted(str(p) for p in pathlib.Path("maps").glob("*.map"))
    argv_a, dir_a, _ = _resolve(args.a)
    argv_b, dir_b, _ = _resolve(args.b)
    jobs = []
    for m in maps:
        for seed in range(args.seeds):
            jobs.append((m, False, seed))
            if not args.one_way:
                jobs.append((m, True, seed))
    score = collections.Counter()
    with cf.ProcessPoolExecutor(args.j) as ex:
        futs = {}
        for m, swap, seed in jobs:
            kw = {}
            if args.logdir:
                os.makedirs(args.logdir, exist_ok=True)
                stem = pathlib.Path(m).stem
                kw = {"log_path": os.path.join(args.logdir, f"{stem}-s{seed}-{'B' if swap else 'A'}.txt"),
                      "log_team": "B" if swap else "A", "header": f"us vs {args.b} ({stem}) seed {seed}"}
            if swap:
                f = ex.submit(play_seeded, seed, m, argv_b, str(dir_b), argv_a, str(dir_a), **kw)
            else:
                f = ex.submit(play_seeded, seed, m, argv_a, str(dir_a), argv_b, str(dir_b), **kw)
            futs[f] = (m, swap, seed)
        rows = []
        for f in cf.as_completed(futs):
            m, swap, seed = futs[f]
            try:
                r = f.result()
            except Exception as e:  # noqa: BLE001
                print(f"{m} swap={swap}: ERROR {e}", file=sys.stderr)
                continue
            rows.append((m, swap, r, seed))
    per_map: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for m, swap, r, _ in sorted(rows, key=lambda x: (x[0], x[1], x[3])):
        me, them = ("B", "A") if swap else ("A", "B")
        w = r["winner"]
        outcome = "draw" if w is None else ("WIN " if w == me else "loss")
        score[outcome.strip()] += 1
        per_map[r["map"]][outcome.strip()] += 1
        if args.quiet:
            continue
        how = "elim" if r["reason"] == 0 else "len"
        line = (f"{r['map']:16s} {'B' if swap else 'A'}  {outcome} {how:4s} r{r['rounds']:3d}  "
                f"longest {r['longest'][me]:3d} vs {r['longest'][them]:3d}  "
                f"total {r['total'][me]:4d} vs {r['total'][them]:4d}  "
                f"alive {r['alive'][me]:2d} vs {r['alive'][them]:2d}  spawned {r['spawned'][me]:3d}")
        print(line)
        if args.deaths:
            print("    me:", dict(r["deaths"][me]), " them:", dict(r["deaths"][them]))
    for name, c in sorted(per_map.items()):
        print(f"  {name:16s} W{c['WIN']:3d} L{c['loss']:3d} D{c['draw']:3d}")
    n = sum(score.values())
    print(dict(score), f"win rate {(score['WIN'] + 0.5 * score['draw']) / max(1, n):.3f} over {n}")
    # Each seed is played with both colours. When the side decides the game, a pair splits
    # 1-1 whatever the bots do, so only pairs won or lost on both sides carry signal, and
    # the length margins below catch differences that don't flip the winner.
    pairs = collections.defaultdict(list)
    margin_long, margin_total = [], []
    for m, swap, r, seed in rows:
        me, them = ("B", "A") if swap else ("A", "B")
        pairs[(m, seed)].append(r["winner"] == me if r["winner"] is not None else None)
        margin_long.append(r["longest"][me] - r["longest"][them])
        margin_total.append(r["total"][me] - r["total"][them])
    pc = collections.Counter()
    for v in pairs.values():
        if len(v) == 2 and None not in v:
            pc["won both" if all(v) else "lost both" if not any(v) else "split"] += 1
    mean = lambda xs: sum(xs) / max(1, len(xs))
    print(f"pairs: {dict(pc)} | mean margin per game: longest {mean(margin_long):+.2f}, total length {mean(margin_total):+.2f}")


if __name__ == "__main__":
    main()
