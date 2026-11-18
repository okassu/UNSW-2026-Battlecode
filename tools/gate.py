"""Test gate: a candidate against every opponent style on a map set, reported by category.

    ~/.local/share/uv/tools/unswbc/bin/python tools/gate.py CANDIDATE [options]

    --maps DIR_OR_GLOB ...   default: maps/*.map maps_val/*.map (validation set: never tune on it)
    --opps NAME ...          default: every opponent in tools/roster.txt
    --seeds N                games per map, opponent and colour (seeds 0..N-1), default 1
    --out FILE.jsonl         one line per game (default gate-<candidate>.jsonl); --report FILE re-reads one
    --baseline FILE.jsonl    compare row by row and apply the ship rule
    --logdir DIR             keep ladder-format logs and add behaviour metrics (tools/dumbstats.py)

Games run in batches of 10 per worker, each batch in a fresh process pool: one long-lived pool leaked
file descriptors until the system table filled ("[Errno 23] Too many open files in system" after ~360
games), and recycling workers with max_tasks_per_child hung the pool.
Every game is played with both colours, as tools/bench.py does. Rows: overall, per opponent,
and per map category (size, symmetry, portals, style, resource motif, starter; real maps are
classified from their files, generated ones from their .json). Columns: games, win rate, seed
pairs won on both colours / lost on both, mean longest and total length margins per game.
Ship rule (fixed before testing): overall win rate >= 0.55 or mean longest margin >= +1.0, and
no opponent or category row with 8+ games more than 0.08 below the baseline.
"""
from __future__ import annotations

import argparse
import collections
import concurrent.futures as cf
import glob
import json
import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bench  # noqa: E402  (play, play_seeded)
from unswbc.run import _resolve  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent


def roster(path=ROOT / "tools" / "roster.txt"):
    out = {}
    for line in open(path):
        line = line.split("#")[0].strip()
        if line:
            name, bot = line.split()[:2]
            out[name] = bot
    return out


def categories(map_path: str) -> dict:
    """Attributes of a map: from the generator's .json, else read from the .map itself."""
    side = map_path[:-4] + ".json"
    W = H = 0
    sym = "?"
    portals = 0
    lengths = []
    for line in open(map_path):
        t = line.split()
        if not t:
            continue
        if t[0] == "MAP":
            W, H = int(t[1]), int(t[2])
        elif t[0] == "SYMMETRY":
            sym = t[1]
        elif t[0] == "EDGE" and t[2] == "2":
            portals += 1
        elif t[0] == "DRAGON" and t[1] == "0":
            lengths.append(int(t[2]))
    if os.path.exists(side):
        j = json.load(open(side))
        c = {k: j[k] for k in ("size", "sym", "style", "motif", "starter")}
        c["source"] = "generated"
    else:
        size = "small" if W * H <= 24 * 24 else "large" if W >= 48 else "medium"
        c = dict(size=size, sym=sym, style="real", motif="real",
                 starter="long" if lengths and max(lengths) >= 10 else "short", source="real")
    c["portals"] = "yes" if portals else "no"
    return c


def run_game(args):
    m, opp, bot_c, bot_o, seed, swap, logdir = args
    argv_c, dir_c, _ = _resolve(bot_c)
    argv_o, dir_o, _ = _resolve(bot_o)
    kw = {}
    stem = pathlib.Path(m).stem
    if logdir:
        kw = {"log_path": os.path.join(logdir, f"{pathlib.Path(opp).name}-{stem}-s{seed}-{'B' if swap else 'A'}.txt"),
              "log_team": "B" if swap else "A", "header": f"us vs {opp} ({stem}) seed {seed}"}
    if swap:
        r = bench.play_seeded(seed, m, argv_o, str(dir_o), argv_c, str(dir_c), **kw)
    else:
        r = bench.play_seeded(seed, m, argv_c, str(dir_c), argv_o, str(dir_o), **kw)
    me, them = ("B", "A") if swap else ("A", "B")
    w = r["winner"]
    return dict(map=m, opp=opp, seed=seed, swap=swap, rounds=r["rounds"], reason=r["reason"],
                result="draw" if w is None else "win" if w == me else "loss",
                long_me=r["longest"][me], long_them=r["longest"][them],
                total_me=r["total"][me], total_them=r["total"][them], log=kw.get("log_path"))


def summarise(rows):
    n = len(rows)
    if not n:
        return None
    wr = sum(1 if r["result"] == "win" else 0.5 if r["result"] == "draw" else 0 for r in rows) / n
    pairs = collections.defaultdict(list)
    for r in rows:
        pairs[(r["map"], r["opp"], r["seed"])].append(r["result"])
    both_w = sum(1 for v in pairs.values() if len(v) == 2 and v == ["win", "win"])
    both_l = sum(1 for v in pairs.values() if len(v) == 2 and v == ["loss", "loss"])
    lm = sum(r["long_me"] - r["long_them"] for r in rows) / n
    tm = sum(r["total_me"] - r["total_them"] for r in rows) / n
    return dict(n=n, wr=wr, both_w=both_w, both_l=both_l, long=lm, total=tm)


def behaviour(rows):
    """Mean dumbstats metrics over the games' logs (our side)."""
    import dumbstats
    acc = collections.Counter()
    k = 0
    for r in rows:
        if not r.get("log") or not os.path.exists(r["log"]):
            continue
        try:
            a = dumbstats.analyse(r["log"], r["map"])
        except Exception:  # noqa: BLE001
            continue
        k += 1
        mv = max(1, a["moves"]["us"])
        d = a["deaths"]["us"]
        acc["loop%"] += 100.0 * a["loops"]["us"] / mv
        acc["deaths/100mv"] += 100.0 * (sum(d.values()) - d["stub"]) / mv
        acc["friendly/g"] += a["friendly"]["us"]
        acc["eat share"] += a["eats"]["us"] / max(1, a["eats"]["us"] + a["eats"]["them"])
        acc["outnumbered trades/g"] += sum(1 for _, st, loc, _ in a["trades"] if st == "them" and loc == "behind")
    return {key: v / k for key, v in acc.items()} if k else {}


def table(rows, base=None, metrics=False):
    keys = ["size", "sym", "portals", "style", "motif", "starter", "source"]
    groups = collections.OrderedDict()
    groups["ALL"] = rows
    for opp in sorted({r["opp"] for r in rows}):
        groups[f"opp={opp}"] = [r for r in rows if r["opp"] == opp]
    for k in keys:
        for v in sorted({r["cat"][k] for r in rows}):
            groups[f"{k}={v}"] = [r for r in rows if r["cat"][k] == v]
    bgroups = None
    if base:
        bgroups = collections.OrderedDict()
        bgroups["ALL"] = base
        for name in groups:
            if name == "ALL":
                continue
            k, v = name.split("=", 1)
            bgroups[name] = [r for r in base if (r["opp"] if k == "opp" else r["cat"][k]) == v]
    flags = []
    hdr = f"{'row':24s} {'games':>5s} {'win':>6s} {'W2':>4s} {'L2':>4s} {'long':>6s} {'total':>7s}"
    if base:
        hdr += f"  {'base':>6s} {'d.win':>6s} {'d.long':>6s}"
    print(hdr)
    for name, g in groups.items():
        s = summarise(g)
        if not s:
            continue
        line = f"{name:24s} {s['n']:5d} {s['wr']:6.3f} {s['both_w']:4d} {s['both_l']:4d} {s['long']:+6.2f} {s['total']:+7.1f}"
        if bgroups is not None:
            b = summarise(bgroups.get(name, []))
            if b:
                dw, dl = s["wr"] - b["wr"], s["long"] - b["long"]
                line += f"  {b['wr']:6.3f} {dw:+6.3f} {dl:+6.2f}"
                if name != "ALL" and s["n"] >= 8 and dw < -0.08:
                    flags.append(name)
                    line += "  <-- down"
        if metrics:
            bm = behaviour(g)
            if bm:
                line += "  | " + " ".join(f"{k} {v:.2f}" for k, v in bm.items())
        print(line)
    s = summarise(rows)
    ok = (s["wr"] >= 0.55 or s["long"] >= 1.0) and not flags
    print(f"\nship rule: overall {s['wr']:.3f} (>= 0.55) or longest {s['long']:+.2f} (>= +1.0); "
          f"rows down > 0.08 vs baseline: {flags or 'none'}  ->  {'PASS' if ok else 'FAIL'}")
    return ok


def load(path):
    rows = [json.loads(l) for l in open(path) if l.strip()]
    for r in rows:
        r["cat"] = categories(r["map"])
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("candidate", nargs="?")
    ap.add_argument("--maps", nargs="*", default=["maps/*.map", "maps_val/*.map"])
    ap.add_argument("--opps", nargs="*")
    ap.add_argument("--seeds", type=int, default=1)
    ap.add_argument("-j", type=int, default=6)
    ap.add_argument("--out")
    ap.add_argument("--report", help="only print the tables for an existing results file")
    ap.add_argument("--baseline")
    ap.add_argument("--logdir")
    ap.add_argument("--metrics", action="store_true", help="add behaviour metrics (needs logs)")
    ap.add_argument("--resume", action="store_true", help="keep the games already in --out and play only the rest")
    a = ap.parse_args()
    base = load(a.baseline) if a.baseline else None
    if a.report:
        table(load(a.report), base, a.metrics)
        return
    ros = roster()
    opps = a.opps or list(ros)
    maps = []
    for pat in a.maps:
        maps += sorted(glob.glob(os.path.join(pat, "*.map")) if os.path.isdir(pat) else glob.glob(pat))
    out = a.out or f"gate-{pathlib.Path(a.candidate).name}.jsonl"
    if a.logdir:
        os.makedirs(a.logdir, exist_ok=True)
    jobs = [(m, o, a.candidate, ros.get(o, o), s, sw, a.logdir)
            for o in opps for m in maps for s in range(a.seeds) for sw in (False, True)]
    print(f"{len(jobs)} games: {len(maps)} maps x {len(opps)} opponents x {a.seeds} seeds x 2 colours -> {out}",
          flush=True)
    rows = []
    done = 0
    if a.resume and os.path.exists(out):
        rows = [json.loads(l) for l in open(out) if l.strip()]
        have = {(r["map"], r["opp"], r["seed"], r["swap"]) for r in rows}
        jobs = [j for j in jobs if (j[0], j[1], j[4], j[5]) not in have]
        done = len(rows)
        print(f"resuming: {len(rows)} games kept, {len(jobs)} to play", flush=True)
    total = done + len(jobs)
    with open(out, "a" if a.resume else "w") as f:
        # A fresh pool per batch: workers leak descriptors over hundreds of games, and
        # max_tasks_per_child hangs the pool once the first workers retire (Python 3.13).
        batch = a.j * 10
        for start in range(0, len(jobs), batch):
            with cf.ProcessPoolExecutor(a.j) as ex:
                futs = {ex.submit(run_game, j): j for j in jobs[start:start + batch]}
                for fu in cf.as_completed(futs):
                    done += 1
                    try:
                        r = fu.result()
                    except Exception as e:  # noqa: BLE001
                        print(f"ERROR {futs[fu][:2]}: {e}", file=sys.stderr)
                        continue
                    f.write(json.dumps(r) + "\n")
                    f.flush()
                    rows.append(r)
            print(f"  {done}/{total}", flush=True)
    for r in rows:
        r["cat"] = categories(r["map"])
    table(rows, base, a.metrics)


if __name__ == "__main__":
    main()
