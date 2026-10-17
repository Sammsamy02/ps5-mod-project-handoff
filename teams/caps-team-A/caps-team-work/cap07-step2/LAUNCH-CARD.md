# LAUNCH-CARD — capability #7 step 2: runtime pak mount, no file change

Short and plain. One launch. Rollback = **R11 bfd95db6** (last clean build).

## What this one launch proves

That a PC pak mod (the pak + utoc + ucas trio) can be loaded **while the game's
own files are never changed** — by having our runtime ask the engine's own
"mount a pak" button to load the mod from our mod folder. If it works, the mod
shows up in game exactly like the old disk method, but nothing in the game is
touched, so a game update can't undo it.

This is the data-only version of what the Leon trial did by swapping the game's
index file on disk. Here we touch no game file at all.

## Preconditions (integrator, before the launch)

1. Build carries the new mount unit wired per `PATCH/APPLY.md`, on top of R11.
2. The mod trio (one `*_P.pak` + its `.utoc` + `.ucas`) is present, unchanged,
   under the mod root (`/data/modbridge/roots/<title>/`), NOT in the game's
   paks folder. (Read-only FTP check is fine; no game-file writes.)
3. All gates green: `tests/run.sh`, repo `no_detour_gate.py`, `fs_throw_gate.py`,
   `verify_build.sh --manifest` for the exact PRX, loadlist row-check.
4. A full before/after dump hash of the game's paks dir (to prove no game file
   changed across the launch).
5. One launch per boot; one agent on the console; ask "game closed?" before any
   log read.

## What the user does in game

Launch Townfall once, reach the playable 3D scene, look for the modded content
(the same thing the Leon mod changed). Leave running if it appears; report if
the game fails to launch or the mod is absent.

## PASS / FAIL log lines (harvest `ue4ss.decoded.log`, `[PAKMOUNT]` prefix)

- PASS: `[PAKMOUNT] mounted via engine MountPak delegate (data-only)` AND the
  user sees the modded content, AND the before/after paks-dir hash is identical
  (no game file changed).
- Soft FAIL (clean, vanilla — NOT a crash): any `[PAKMOUNT] refuse: ...` line
  (`image sha mismatch`, `P4 signature mismatch`, `MountPak delegate unbound`,
  `pak layer not found`, `engine mount did not take`). The game still runs
  vanilla; this is the fail-closed path, the build is safe, and the refuse
  reason says exactly what to fix.
- Hard FAIL: a crash, OR the paks-dir hash changed (would mean something wrote a
  game file — must not happen; investigate before any re-launch).

## If it refuses

Each refuse reason maps to one fix: sha/signature → re-pin for this image
(per-version table); delegate unbound → call later in init; pak layer not found
→ chain-head resolution wrong (check P3/P4 wiring). No re-launch is needed to
read the reason — it is in the log.
