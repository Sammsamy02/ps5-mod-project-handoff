# MANIFEST-LAUNCH-2 — ladder v2: four type-mods in ONE launch (FX-final PRX)

> **SUPERSEDED 2026-10-11 by `MANIFEST-LAUNCH-2-FINAL.md` (agent IE)** —
> the plan moved to Claude round-8: the PRX for this launch is the FINAL
> STACK (round-4 base + P1 + P2 + world-fix + round-5 set-aside restore,
> each its own labelled gated build — not the round-6 "FX-final" single
> build), plus the self-test re-arm and the current zzdiag-v11 revision
> (4094 live at launch-1; re-measure at deploy). Everything below keeps
> its data value: §1 hashes, the staging-input shape, the co-fire review
> and the row-count law are carried into the FINAL manifest; its
> loadlist/pad/rollback rows are re-synthesized there (zzdiag@4065 and
> ueselftest@3961 here are pre-revision). — pointer added by agent IE;
> body otherwise untouched.

Agent GQ (ZCode), 2026-10-10. Mac-only prep, battery-safe: the loadlist,
pad and upload plan below were SYNTHESIZED OFFLINE by GG's modmanager.py
(real dry-run in a /tmp store this session; console never contacted).
Sources: mod-ladder/LADDER-V2.md (GC), mod-manager/DESIGN.md + modmanager.py
(GG), WAKE-RUNBOOK §11 (Claude round-6), MANIFEST-LAUNCH-1.md (the
resident state this launch starts from).

Constants: same block as MANIFEST-LAUNCH-1 (`PREP`, `FTPR`, `ROOT`, `LM`).

## 0. What this launch is

LADDER-V2's combined generality launch: the FOUR type-mods enabled
TOGETHER (CheatManagerEnablerMod = event/hook, ConsoleEnablerMod =
value/array-write, TownfallMiniMap = config/io + sustained-tick primary,
StaminaMod = sustained-tick secondary) alongside the resident spawner
(menu/control), zzdiag-v11 (probe) and ueselftest (self-test combo).
PRX per Claude round-6 (WAKE-RUNBOOK §11): the FX-patched FINAL build
(P1 then P2, fully gated, built on AC power) is console-tested IN THIS
launch — no separate FX launch.

| item | value |
|---|---|
| PRX path | `<fx-final-prx>` — TBD at AC time (P2 build receipt; if only P1 exists when the window opens, STOP — the FINAL build is the plan) |
| Deploy tool | `prep_modded_launch.py --variant A --prx-path <fx-final-prx>` (same gates: verify_build --manifest PASS, decrypt-off readback magic 4f153d1d + sha match) |

## 1. Mod set and source provenance (hashes verified on disk this session)

| Mod | source (read-only) | bytes | sha256 |
|---|---|---|---|
| CheatManagerEnablerMod | TREE-B LF `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/CheatManagerEnablerMod/Scripts/main.lua` | 1407 | 0b70465317ef3fced4e342f04592896658e590c3cab472e2993460fba6734c83 |
| ConsoleEnablerMod | TREE-B LF `.../assets/Mods/ConsoleEnablerMod/Scripts/main.lua` | 3173 | b31feb8ffb8adac1aa8204d65665ddd2fc4971e61cba210f18054e9bfbe911a8 |
| TownfallMiniMap v3.1 | TREE-A CRLF `.../townfall-ue4ss-real-script-audit-1/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` (+ shipped `enabled.txt` 0 B) | 196743 | d8f8d64f84d7e0933b828211f6610d950e6244ea1a45f1e9f4f3bd8c353c920f |
| StaminaMod | `.superpowers/native-dependency-spike/real-mod-lua-runner-1/inputs/stamina_main.lua` (identical in all 3 local copies), staged as NEW mod dir `Mods/StaminaMod/Scripts/main.lua` | 2811 | 7667fed7e4d7b4cfa9c9f0ed2a794ac815f2121218f43f41192147f62294e335 |
| zzdiag (kept resident) | `$PREP/zzdiag-v11/main.lua` — **REVISED mid-prep by a concurrent agent** (adds [MAP] milestone lines); FJ's report said 4084/84fb6a93..., current file is below. Re-measure at wake; the row must equal the ACTUAL deployed bytes | 4065 | 779f4c16d599661ea2c8bc11b3d9024c563ed04aa1addab6efe507b11265ce8d |
| ueselftest (kept resident) | `$PREP/ueselftest/*.lua` (12 files) | 42969 total | per-file: see MANIFEST-LAUNCH-1 §4 |
| TownfallItemSpawner | resident, NEVER re-uploaded (rows from base/live; original bytes per FN freeze) | 21415 | f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658 |
| shared/UEHelpers | resident, never re-upload (row 9962) | 9962 | (row-pinned; unmanaged) |

All six store mods passed GG's preflight harness (host-lua-harness
runner.lua, io-shim mode, lua5.5): **LOADS-CLEAN, 6/6** — including
MiniMap's CRLF copy.

## 2. The synthesized admission manifest (`launch2-loadlist.txt`)

Produced by `modmanager.py deploy --allow-cofire` (dry-run) in a /tmp
store seeded with the launch-1 final state as the offline base
(receipts/last/loadlist.txt = launch1-loadlist.txt — under `--apply` the
LIVE loadlist is fetched instead, same law). Result, pinned here:

- **35 lines / 30 file rows / 1766 B**, sha256
  314d1bccdf591f020d9b13193bf6c1a8bfaceaa27ff8babedfd1b5cf99b94977.
- Composition: l23 base 12 lines (pad row stays 1736 — see §4) +
  zzdiag 2 rows (4065 + 0; current revision, see §1) + ueselftest 13
  rows + the 8 NEW rows
  (CheatManager 1407+0, ConsoleEnabler 3173+0, MiniMap 196743+0,
  Stamina 2811+0) appended in ladder order.
- Cross-validation: disabling the four mods and re-synthesizing
  reproduces `launch1-loadlist.txt` BYTE-IDENTICAL (verified this
  session) — the rollback target is exact.
- Store provenance: `launch2-modmanager-manifest.json` (per-file
  size+sha of every staged copy; install-time sha==source-sha law).

## 3. Upload plan (GG's order law: mod files -> PS5Pad.ini -> loadlist.txt LAST)

Captured verbatim from the dry-run in `launch2-deploy-plan.txt`
(24 managed rows + pad + loadlist). Compact form (remote under `$ROOT`;
all text files; readback cmp every file):

| # | remote path | bytes | sha256[:16] |
|---|---|---|---|
| 1-2 | `ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua` + `enabled.txt` | 1407 / 0 | 0b70465317ef3fce / e3b0c44298fc1c14 |
| 3-4 | `ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua` + `enabled.txt` | 3173 / 0 | b31feb8ffb8adac1 / e3b0c44298fc1c14 |
| 5-6 | `ue4ss/Mods/StaminaMod/Scripts/main.lua` + `enabled.txt` | 2811 / 0 | 7667fed7e4d7b4cf / e3b0c44298fc1c14 |
| 7-8 | `ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` + `enabled.txt` | 196743 / 0 | d8f8d64f84d7e093 / e3b0c44298fc1c14 |
| 9-21 | `ue4ss/Mods/ueselftest/Scripts/{main,part2..12}.lua` (byte-identical re-assert) | 42969 total | per MANIFEST-LAUNCH-1 §4 |
| 22-23 | `ue4ss/Mods/zzdiag/Scripts/main.lua` + `enabled.txt` (byte-identical re-assert) | 4065 / 0 | 779f4c16d599661e / e3b0c44298fc1c14 |
| 24 | `ue4ss/PS5Pad.ini` = `launch2-PS5Pad.ini` | 1736 | a6313eec781173df |
| 25 | `loadlist.txt` = `launch2-loadlist.txt` (LAST) | 1766 | 314d1bccdf591f02 |

`--allow-cofire` is REQUIRED (reviewed acceptance, GG DESIGN §4): the
spawner+zzdiag F7 co-fire on L3+R3 is the console-proven pair (every
session since l22); the gate forces the explicit decision, it is not a
new conflict. Resident-mod uploads (spawner main.lua, UEHelpers) are NOT
in the plan — the manager never manages them.

STAGING-INPUT SHAPE (needed to reproduce this synthesis in a fresh
store): GG's `install` stages ONLY `Scripts/main.lua` from a FLAT
(zzdiag-shape) dir. ueselftest and StaminaMod were therefore installed
from Scripts-shaped INPUT trees (layout normalization on /tmp byte
copies — `Scripts/main.lua` + `Scripts/part2..12.lua`; sha-identical to
GB's originals, install-verified): if ops re-install ueselftest from
its flat dir instead, the 11 parts would be silently dropped from the
store/synthesis and the L1+R1 run would report `partN LOAD-FAIL` on
console. zzdiag installs flat correctly (single-file mod).

## 4. PS5Pad.ini — GC drift, reconciled (reported, not silently fixed)

- GC's LADDER-V2 §1.3.1/§4.1: "the live 3,718 B console-proven
  PS5Pad.ini stays byte-identical — no regeneration, no MFA re-pin"
  (zero new binds from the four type-mods — verified: none registers a
  RegisterKeyBind key). TRUE for the four mods in isolation.
- BUT GC's own launch-2 sequence (§4.2 step 5) presses the ueselftest
  combo, and ueselftest stays resident from launch 1 — so the live pad
  at launch 2 is the 1736 B round-4 profile, NOT 3718 B. The manager's
  regenerated pad for the full set {spawner, zzdiag, ueselftest, +4
  bind-less mods} = 7 bindings, 6 pinned + L1+R1=F12, **1736 B** — the
  pad ROW therefore stays pinned at 1736; no arithmetic drift vs launch
  1. File bytes differ from GB's launch-1 pad ONLY in the
  `; inputs-sha256:` provenance header (bindings block byte-identical;
  FV's hash covers name|path|content per mod — see VERIFICATION-SHEET
  note N3; the manager pad's header moved again when zzdiag's bytes were
  revised mid-prep, faf4742b... -> a6313eec..., still 1736 B / identical
  bindings). Upload the manager's `launch2-PS5Pad.ini`; its 0-violation
  `--check --expect-size 1736` receipt is in `launch2-deploy-plan.txt`.
- If instead ops roll launch 1 fully back (3718 B pad restored) and
  skip the combo in launch 2, the manager would still regenerate a
  1736 B-class pad when ueselftest is in the store — the combo stays
  available. Either path pins 1736 while ueselftest is enabled; the
  3718 B pin only returns when ueselftest is disabled/absent.

## 5. Other GC-plan drift found and NOT silently fixed (decisions for ops)

1. **PRX**: GC §4.0 stages round-4 for this launch; Claude round-6
   (WAKE-RUNBOOK §11, later) moves the FX-patched FINAL build INTO this
   launch. This manifest follows round-6 (`<fx-final-prx>`); if the FX
   builds are not ready when the window opens, round-4 is the only
   sanctioned fallback (never round-3).
2. **Row-count law**: GC §4.1 says "base 7 + 8 = 15 file rows"; correct
   only against the bare l23 base. The actual launch-2 composition
   (launch-1 state kept resident) = **30 file rows** (7 base + 2 zzdiag
   + 13 ueselftest + 8 new). The `grep -c $'\t'` check must expect 35
   tab-rows / 30 file rows (or 22/9 after full rollback).
3. **zzdiag/ueselftest residency**: GC §4.0 says "do not add/remove rows
   for zzdiag as part of this plan" — honored; the manager re-asserts
   their rows byte-identically (in-place replace, no new rows).
4. **Staging tree**: GC §4.1's table and GG's upload plan agree on all
   four mods' bytes/hashes (verified this session against the exact
   trees). No numeric drift.

## 6. Rollback (precomputed: `launch2-loadlist-rollback.txt` = launch-1 state)

1. Manager path (preferred): `disable` the four mods (or the four
   `modmanager.py disable <Mod>`), `deploy --apply --allow-cofire`:
   synthesis drops exactly the 8 rows — verified byte-identical to
   `launch1-loadlist.txt` (1292 B, 17b65436...). Pad row stays 1736
   (ueselftest still resident).
2. Receipt-pair path: `rollback --apply` re-uploads the deploy's
   prev-loadlist + prev-PS5Pad (the launch-1 pair) — same result.
3. Remote DELE of the four mod files optional (unlisted lua files are
   never admitted; GC §4.6). MiniMap's shipped enabled.txt goes with
   its dir.
4. Next-launch confirmation: the four `[l22] Lua mod started` lines
   absent; spawner/zzdiag/ueselftest unaffected; klog clean.

## 7. User sequence (LADDER-V2 §4.2) and watch list

Launch -> load save -> in-world (ClientRestart window: CheatManager +
ConsoleEnabler markers) -> ~10-15 s -> SELF-TEST COMBO L1+R1 once ->
L3+R3 spawner control rung (navigate, close) -> play 60-90 s with
minimap HUD captures at t0 and t0+30 s (tick evidence) -> optional
MiniMap settings change (io round-trip attempt) -> CLEAN EXIT. One UI
open at a time (GC §1.3.4). Crash: note code + WHEN, harvest, REBOOT.

```bash
python3 $PREP/harvest/harvest.py --label ladder2-combo-r1
grep -a "Lua mod started" <UE4SS.log>                                   # 6 mods admitted
grep -a "CheatManagerEnabler" <UE4SS.log>                               # hook type
grep -a "ConsoleEnabler\|ConsoleKey\[" <UE4SS.log>                      # write type
grep -a "TownfallMiniMap\|too old" <UE4SS.log>                          # io/tick (absence law)
grep -a "\[Stamina\]" <UE4SS.log>                                       # tick secondary
grep -a "Menu created\|Loaded v1.0" <UE4SS.log>                         # spawner control
grep -a "\[ST2\] SUMMARY" <UE4SS.log>                                   # self-test (kept resident)
grep -a "mfa_activate_failed\|code=-8" <UE4SS.log>                      # must be ABSENT
```

3-passes law (GC §3.4): type ACCEPTED only after the same pass markers
in 3 clean-exit launches (r1 + r2 + r3), byte-identical staging (no
file churn between passes); watch-item: CheatManager's marker in EVERY
launch's ClientRestart window, ConsoleEnabler's only the first per boot
(by design). A PRX/build change resets the count (r-labels per build).

— agent GQ. GG/GC/GB/FV/FJ files read-only; the /tmp store used for the
synthesis is disposable (all its outputs are pinned in this directory).
