"""Play one game and write a per-turn trace: every dragon's head position,
length, action and indicator each round, plus spawns and deaths with reasons.

    ~/.local/share/uv/tools/unswbc/bin/python tools/runlog.py MAP BOT_A BOT_B OUT.txt [SEED]

SEED (default 0) seeds pearl respawns (unswbc >= 1.1) and is passed to the bots as DRAGON_SEED.
"""
from __future__ import annotations

import os
import pathlib
import sys

from unswbc.bot import Bot, Pool
from unswbc.engine import EngineModule
from unswbc.run import DEATH_REASONS, _resolve


def main() -> None:
    map_path, a, b, out = sys.argv[1:5]
    seed = int(sys.argv[5]) if len(sys.argv) > 5 else 0
    os.environ["DRAGON_SEED"] = str(seed)
    argv_a, dir_a, _ = _resolve(a)
    argv_b, dir_b, _ = _resolve(b)
    pools = {"A": Pool(argv_a, cwd=str(dir_a), key=f"{seed:016x}-a"), "B": Pool(argv_b, cwd=str(dir_b), key=f"{seed:016x}-b")}
    live: dict[int, Bot] = {}
    lines: list[str] = []
    teams: dict[int, str] = {}
    state = {"round": "0"}

    def spawn(dragon_id: int, init: bytes) -> None:
        team = next((l.split()[1] for l in init.decode().splitlines() if l.startswith("TEAM")), "A")
        live[dragon_id] = Bot(pools[team], init=init, name=str(dragon_id))
        teams[dragon_id] = team
        lines.append(f"{state['round']}\tdragon {dragon_id} spawns team {team}")

    def reply(dragon_id: int, block: bytes) -> bytes:
        out = live[dragon_id].ask(block)
        text = block.decode(errors="replace").split("\n")
        rnd = length = "?"
        head = "?"
        for i, l in enumerate(text):
            t = l.split()
            if not t:
                continue
            if t[0] == "ROUND" or (t[0] == "CONTINUE" and len(t) > 1):
                rnd = t[1]
            elif t[0] == "LENGTH":
                length = t[1]
            elif len(t) == 6 and t[1] == str(dragon_id) and t[5] == "1":
                head = f"({t[2]},{t[3]})"
        acts = [l for l in out.decode(errors="replace").splitlines() if l.split()[:1] in (["MOVE"], ["SPLIT"])]
        ind = [l[10:] for l in out.decode(errors="replace").splitlines() if l.startswith("INDICATOR ")]
        logs = [l[4:] for l in out.decode(errors="replace").splitlines() if l.startswith("LOG ")]
        state["round"] = rnd
        lines.append(f"{rnd}\tdragon {dragon_id} at {head} len {length} team {teams.get(dragon_id)} "
                     f"-> {acts[-1] if acts else 'NOTHING'} [{ind[-1] if ind else ''}]")
        for l in logs:
            lines.append(f"{rnd}\tdragon {dragon_id} log:{l}")
        return out

    def death(dragon_id: int, round_num: int, reason: str) -> None:
        lines.append(f"{round_num}\tdragon {dragon_id} dies team {teams.get(dragon_id)} reason {DEATH_REASONS.get(reason, reason)}")
        bot = live.pop(dragon_id, None)
        if bot is not None:
            bot.stop()

    try:
        result = EngineModule().run(pathlib.Path(map_path).read_bytes(), reply, death, spawn, lambda line: None, 0, seed)
    finally:
        for bot in live.values():
            bot.stop()
        for pool in pools.values():
            pool.close()
    with open(out, "w") as f:
        f.write(f"{a} vs {b} on {pathlib.Path(map_path).stem}: winner {result.winner} reason {result.end_reason}\n")
        for line in lines:
            f.write(line.rstrip("\n") + "\n")
    print(f"winner {result.winner} reason {result.end_reason} rounds {result.rounds + 1} -> {out}")


if __name__ == "__main__":
    main()
