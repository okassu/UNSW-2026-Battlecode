# Running the bot and the test harness on another computer

Works on Linux, on Windows through WSL (Ubuntu), and on macOS. Everything is plain `sh` + `python3`; the only
machine-specific pieces (compiled snapshot bots and the param-variant launchers) are rebuilt by one script.

## One-time setup
1. Windows only: install WSL (`wsl --install` in an admin PowerShell, reboot, open Ubuntu) and keep the repo inside
   the Linux file system (e.g. `~/battlecode`, not `/mnt/c/...`: games spawn one process per dragon and the Windows
   file bridge is slow).
2. Tools: `sudo apt install build-essential python3 git` (Linux/WSL) or Xcode command-line tools (macOS), then
   uv (`curl -LsSf https://astral.sh/uv/install.sh | sh`) and the toolkit: `uv tool install unswbc==1.2.2`.
   `unswbc` with no arguments lists anything still missing.
3. Copy the repo over. From the MacBook (2026-09-30) it came as `battlecode-pc.tar.gz`: everything, game logs in
   `results/logs-*` included, except the macOS-compiled snapshot builds (`.unswbc-build`) and the Mac-path variant
   launchers (`snapshots/var/`), which step 4 rebuilds. Unpacked it is ~4 GB. In WSL/Linux:
   `mkdir -p ~/battlecode && tar -xzf battlecode-pc.tar.gz -C ~/battlecode` (from Windows the file is under
   `/mnt/c/Users/<you>/Downloads/`). Any other way works too, e.g. `rsync -a --exclude 'results/logs-*' --exclude '.unswbc-build' ...`.
4. `sh tools/setup_pc.sh` rebuilds every snapshot named in `tools/variants.txt` and recreates `snapshots/var/*`.
   `sh tools/setup_pc.sh core` builds only what the opponent roster and the v12/v13 comparisons need.
5. Submitting: `unswbc auth` once (API key), then `unswbc submit snapshots/v13i -n "sworm ..." -d "..."`.
6. Claude Code's notes about this project (how you hand over ladder games, what was tested, the top-team findings)
   are in `docs/claude-memory/`. Claude Code keeps memory per project path, so on the PC copy them into
   `~/.claude/projects/<the new project's folder>/memory/`, or just ask Claude to read docs/claude-memory first.

## Running tests
- A queue of gate runs: write lines `VARIANT SET` into a file and `sh tools/run_queue.sh jobs.txt`
  (sets: `real`, `val`, `lib`, `roster`; see the script's header). Results land in `results/`, runs resume if
  stopped, and the worker count defaults to cores - 1 (`JOBS=N` overrides).
- Compare runs game by game (same map, seed and colour): `python3 tools/abcmp.py results/ab-cand.jsonl results/ab-X.jsonl`.
- Category table and ship rule: `$(sh tools/py.sh) tools/gate.py --report results/ab-X.jsonl --baseline results/ab-cand.jsonl`.
- New variant: `sh tools/variant.sh NAME snapshots/v13/.unswbc-build/bot MAPLIB=0 KEY=VALUE ...`, then
  `python3 tools/mkvariants.py` so the list (and other machines) know about it.
- A single game with a log: `$(sh tools/py.sh) tools/runlog.py maps/devil.map snapshots/v13 snapshots/v11 out.txt 3`.

## State when moved (2026-09-30 morning)
- Live on the ladder: "sworm v13" = `snapshots/v13` = `mybot` defaults (server v28, same code as v26). Tried and
  dropped: v13b = `snapshots/v13i` (v27, longer endgame; did worse on the ladder). Earlier: v12 = `snapshots/v12m` (v25). docs/BUGS.md (newest first) has every test behind these; FINDINGS.md the top-team notes.
- Unfinished: the BEACON_TTL 30 test (87 of 120 games). Resume with `echo "ttl30_u real" | sh tools/run_queue.sh -`
  and compare with `python3 tools/abcmp.py results/ab-e_all400.jsonl results/ab-ttl30_u.jsonl`.
- Baselines for comparisons: `results/ab-e_all400.jsonl` (v13 settings, real maps, library off), `results/lib13-lib_F_rush.jsonl`
  (v13, library on), `results/val13-e_all400.jsonl` (generated maps).

## Differences from the MacBook
- The Mac capped us at `-j 7` (per-user process limit 1333; each game runs one process per dragon). Linux usually
  allows far more; if games fail with "Resource temporarily unavailable", lower `JOBS` or raise `ulimit -u`.
- `results/*.sh` from before 2026-09-30 are MacBook queue scripts with absolute paths; use `tools/run_queue.sh` instead.
