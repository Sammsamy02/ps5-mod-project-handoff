# MANIFEST-LAUNCH-2-FINAL — final stack + ladder v2 + self-test re-arm (Claude round-8)

Agent IE (ZCode), 2026-10-11. Mac-only prep, battery-safe: every artifact
below was synthesized OFFLINE this session (keymap-gen.py regen + --check,
modmanager-row-compatible loadlist synthesis, prelaunch_rowcheck.py fixture
run — zero console contact, zero builds, zero mod edits). This file
SUPERSEDES `MANIFEST-LAUNCH-2.md` (GQ, staged for the OLD round-6
"FX-final PRX" plan; that file keeps its data value and a superseded note —
it is updated, not rewritten). Governing plan: Claude round-8 (final stack
of stacked labelled builds + LADDER V2 + self-test re-arm + zzdiag v11).

Constants (WAKE-RUNBOOK copy-paste block):

```bash
CONSOLE=ps5-host-not-configured.invalid
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
PREP=$WHAT/outputs/2026-10-08-l25-prep
LM=$PREP/launch-manifests
FTPR="ftp://$CONSOLE:2121"
ROOT=/data/modbridge/roots/PPSA33286
```

## 0. What this launch is (launch-2)

ONE user launch window that console-tests, in this order of content:

1. **THE FINAL STACK PRX** — the AC-power build chain of Claude round-8:
   round-4 base + P1 fault-ledger + P2 variant-B + world-fix + round-5
   set-aside restore, each its own labelled gated build, stacked in order
   (§1). Placeholder `<final-stack-prx>` until built.
2. **LADDER V2** (GC): the four type-mods TOGETHER — CheatManagerEnablerMod
   (event/hook), ConsoleEnablerMod (value/array-write), TownfallMiniMap
   (config/io + sustained-tick primary), StaminaMod (sustained-tick
   secondary) — alongside the resident spawner (menu/control).
3. **Self-test re-arm** (L1+R1 → F12): launch-1 ended `armed=true` with 0
   idx lines (combo never fired); launch-2 presses it explicitly. The
   round-5 shim upgrades the io rows from launch-1's SKIP-class to
   expected-PASS (§6).
4. **zzdiag v11 (current revision)** riding free: [MAP] milestone lines +
   the new PRB-7 fname-eq row + cb= driver-rate counter.

Starting console state (verified by launch-1, CLAUDE-ROUND4-SPAWN-PROVEN):
round-4 PRX 59b0528b resident (spawn proven), zzdiag@4094 + ueselftest 13
rows + pad 1736 deployed, all 22 file rows readback-verified. Live lesson
from launch-1 in force: **FTP reads return EMPTY under game load — every
verify below runs game-closed.**

## 1. THE BUILD STACK CHECKLIST (ordered; each step = its own labelled, fully gated build at AC power)

Gate cycle per step (WAKE-RUNBOOK §12 law): snapshot → build → package →
`verify_build.sh <prx> --manifest $PREP/harvest/markers-declared.json` =
PASS → archive the labelled outputs (bisect depends on every intermediate
surviving) → P12/P13 symbols sidecar (HH, automatic in
`l26_packaging_driver.py` `finalize_variant`). Markers-declared.json is
**15** today (round-4 shape); it returns to **18** at step S4 (3 [SHIM]
markers back). If S3 (world-fix) declares its own new markers, the
manifest for S3+ must declare them — re-check `markers-declared.json` at
each step's gate, never assume.

| # | labelled build | content | patches / sources (pinned this session) |
|---|---|---|---|
| S0 | round-4 base (DONE, console-resident) | r3 tree + FR parity revert + FW literal markers; spawn-proven | PRX 59b0528b77ca…4af61 (snapshot a79b3655, stamp l26-20261011T030618Z). NOT rebuilt. |
| S1 | **P1 fault-ledger** | FaultLedger desaturation (RecordResult, no saturation poisoning, `[ST]` fault-ledger self-test line) | `parked/P1-fault-ledger/01-fault-ledger-desaturation.patch` (4 hunks, `patch -p1 --fuzz=0`) + wiring per `parked/P1-fault-ledger/NOTES.md`. NEVER-APPLY-WITHOUT law satisfied: launch-2 watches the `[ST]` `copyin_fault_ledger_st_pass=1` line. |
| S2 | **P2 variant-B removal** (P1 stays in) | variant B dropped from default build+package (env-opt-in only); dead B arms removed from tree | `parked/P2-variant-b-removal/` 01-build-variants-drop-b.patch → 02-tree-dead-code.patch → 03-packaging-driver.patch, apply IN ORDER (03 is authored ON TOP OF P1's packaging hunk — P1 FIRST). Post-P2 shas pinned in its NOTES (build_variants.py 53739fcf…, UnrealInitializer.cpp 6a9087b5…, LuaMod.cpp 5a6a06e6…, CHANGES.json 8fb0d3ef…, l26_packaging_driver.py dabe31ed…). |
| S3 | **world-fix** | IA's World-lookup fix + the `[FL-MAP]` runtime logger (ordered map log) — answers launch-1 finding 5 ([MAP] absent: World lookup returned nil/"?" with db/pc fine) | patches staged under `round5-staging/world-fix/` (IA's staging dir; RE-CHECK contents when IA lands). Markers: re-check + declare any new [FL-MAP] markers. |
| S4 | **round-5 = THE FINAL STACK** | (a) FULL set-aside restore per ROUND4-CONTENTS-DECLARATION: `LuaPathShim.{cpp,hpp}` back + LuaMod.cpp insertion block back (post-check 9530 lines) + `lua-pathshim` NEW_UNITS row back + GT first-launch TU (`FirstLaunchProfile.{cpp,hpp}` + firstlaunch NEW_UNITS row) + HM KB TU (`KernelBridgeProbes.{cpp,hpp}`, compiles to nothing ungated) + **3 [SHIM] markers back (15→18)** + CHANGES.json `round5_row_sync`/hpp rows back with refreshed hashes. (b) **IB's writable-shim** (round-5 content; RE-CHECK IB's report/patch when it lands). Exact inverse = WAKE-RUNBOOK §12 step 3. | set-aside files pinned in `round4-setaside/` (LuaPathShim.cpp/hpp, LuaMod-insertion-block.txt, FirstLaunchProfile.cpp/hpp, KernelBridgeProbes.cpp/hpp). |
| — | **`<final-stack-prx>`** | THE PRX launch-2 deploys = the S4 output. Placeholder sha until built. If any upstream step is missing when the window opens: STOP — the FINAL stack is the plan; bisect exceptions below are for REGRESSIONS, not for skipping steps. | — |

Standing laws for the stack: FX-patch laws from round-6 carry over (P1
then P2 back-to-back, fully gated; never apply piecemeal behind an in-flight
build); FY serial-equivalence proof runs DURING the launch window (rebuild
the same snapshot with the pre-FY serial driver, shas must match — else
stop concurrent builds); every executable upload gets decrypt-off readback
(magic 4f153d1d + full sha256); if ladder v2 regresses, bisect with the
archived S1/S2/S3 builds — no separate launches.

Deploy (ROOT-owned, before any Lua staging, game closed):
`python3 $PREP/harvest/prep_modded_launch.py --variant A --prx-path
<final-stack-prx>` (same gates as ever: verify_build --manifest PASS first;
prep also owns settings 1947 B / ploader.ini 440 B / the ONE loadpayload
arm / klog tap / eboot monitor — never this manifest).

## 2. THE LOADLIST — `launch2-final-loadlist.txt` (this directory)

35 lines / **30 file rows** / 1766 B / sha256
b860dee10e36a3548551013f1351e1ba8ac487a36b3740f77fdee26880e6eee2
(re-measure-affected; see the two RE-MEASURE rows). Composition = launch-1
live state (base 12 lines incl. pad row 1736 + zzdiag 2 rows + ueselftest
13 rows) + the 8 NEW rows (4 ladder mods × main.lua + enabled.txt) appended
in ladder order. Synthesis law unchanged (GG manager / sidecar law: pull
LIVE first, keep live values, append only; MFA sorts by path internally).

| row(s) | path(s) | bytes | sha256 (verified on disk this session) | pin class |
|---|---|---|---|---|
| 4 config | settings / MVL / VTL / **PS5Pad.ini** | 1947 / 588 / 216 / **1736** | pad: see §3 (header-varies law N3) | resident/row-pinned; pad row = 1736 |
| 2 | spawner main + enabled | 21415 / 0 | f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658 / (empty) | FN freeze; NEVER re-upload |
| 1 | UEHelpers | 9962 | (row-pinned; unmanaged) | resident |
| 2 | zzdiag main + enabled | **4086** / 0 | 12a1e55600566b2d361e5ea775a2fef99e552bdcb8a4f376feb8cde96e72e7e7 / (empty) | **RE-MEASURE-AT-DEPLOY** (IC edited mid-prep: 4065 GQ-era → 4094 live launch-1 → 4086 current w/ PRB-7 fname-eq) |
| 13 | ueselftest enabled + main + part2..12 | 0 / **3987** / 3735,3724,3731,3869,3732,3646,3662,3797,3681,2251,3180 | main 2c7e69cbf99280f7f15d2e4d65aefecc2a23bc3078431e7b27b821ec3c2ac599 (**RE-MEASURE**: IC's RUN-start line landed mid-prep, 3961→3987); parts per MANIFEST-LAUNCH-1 §4 (unchanged Oct 10 13:46) | main RE-MEASURE; parts pinned |
| 8 | CheatManager main+enabled | 1407 / 0 | 0b70465317ef3fced4e342f04592896658e590c3cab472e2993460fba6734c83 | hash-pinned (TREE-B LF) |
| 8 | ConsoleEnabler main+enabled | 3173 / 0 | b31feb8ffb8adac1aa8204d65665ddd2fc4971e61cba210f18054e9bfbe911a8 | hash-pinned (TREE-B LF) |
| 8 | MiniMap main+enabled | 196743 / 0 | d8f8d64f84d7e0933b828211f6610d950e6244ea1a45f1e9f4f3bd8c353c920f | hash-pinned (TREE-A CRLF — preserve CRLF end-to-end) |
| 8 | StaminaMod main+enabled | 2811 / 0 | 7667fed7e4d7b4cfa9c9f0ed2a794ac815f2121218f43f41192147f62294e335 | hash-pinned |

Count check at deploy: `grep -c $'\t'` = **35** tab-lines, `grep -c "^file"`
= **30** file rows (27 / 22 after rollback). Every row size-exact per the
MFA law (mismatch = `mfa_activate_failed code=-8`, game continues
unmodded). Loadlist itself is NOT size-pinned by MFA (it IS the manifest);
1766 B is the receipt number — it moves with the two RE-MEASURE rows (same
digit count today, so 1766 holds while those stay 4-digit).

## 3. PS5Pad.ini — FV-regenerated for ALL resident mods (`launch2-final-PS5Pad.ini`)

Regenerated this session with keymap-gen.py over the FULL launch-2 set
{spawner, zzdiag, CheatManagerEnabler, ConsoleEnabler, MiniMap, Stamina,
ueselftest} against the console-proven l23 3718 B pin (`--ladder pairs10`):

- **1736 B, 7 bindings = 6 pinned verbatim (L3+R3=F7, UP, DOWN, CROSS,
  LEFT, RIGHT) + 1 auto (L1+R1 = F12 ← ueselftest)**. sha256 of this
  regen 6f54a6acab59d9cf2caf4f4a0eaf008608d9907e5f0d9ceafad8f4188b42f5fe.
- **`--check … --expect-size 1736` = 0 violations, 0 warnings**
  (`launch2-final-audit-receipt.txt`).
- **The four ladder mods add ZERO pad rows** — verified by the
  generator's own extractor this run ("0 unmapped, 0 extraction errors";
  the only new-key consumers would have shown as auto rows).
  ConsoleEnabler's Tilde/F10 are ENGINE `InputSettings.ConsoleKeys`
  additions, not UE4SS RegisterKeyBind keys — invisible to the pad by
  design (FV divergence note, MAPPING.md §2). The launch-2 pad therefore
  still includes ALL wanted binds: the spawner's F7+nav (pinned), zzdiag's
  F7 (shared with the spawner), and the self-test L1+R1.
- **Combo budget**: 1 of 10 pairs10 auto slots consumed; 9 free
  (TOUCH_PAD+L1, TOUCH_PAD+R1, TOUCH_PAD+L2, TOUCH_PAD+R2, L1+R2, L1+L2,
  R1+L2, R1+R2, L2+R2); ceiling 16 total rows (6 pinned + 10 auto);
  parser ceiling 128. No chord-contains-chord breach possible (antichain
  law enforced by the generator).
- **Only co-fire**: F7 fires spawner+zzdiag via L3+R3 — the reviewed,
  console-proven pair (every session since l22) → `--allow-cofire` at
  manager deploy time.
- **N3 boundary**: pad HEADER bytes vary by generating path (inputs-sha256
  hashes name|path|content per mod; this regen ≠ GB's 957d3e25 ≠ the
  manager's a6313eec) while size+bindings are invariant. OPERATIONS RULE:
  at deploy, upload the pad the MANAGER regenerates (or this exact file if
  ops stage by hand from this directory) — never cross-regenerate or
  hand-edit; `wc -c` the actual bytes and pin THAT in the row.

## 4. UPLOAD LIST + ORDER LAW + THE AUTOMATIC ROW-CHECK GATE

**Order law (GG): mod files FIRST → PS5Pad.ini → loadlist.txt LAST.** MFA
reads the loadlist at module_start and preloads what it names; the console
must never hold a manifest referencing files that are not there yet. All
text files — no SITE SELF decrypt hazard; readback = fetch + `cmp` every
file, GAME CLOSED (launch-1 live lesson: FTP reads return empty under
game load).

| # | remote path (under `$ROOT`) | bytes | sha256[:16] |
|---|---|---|---|
| 1-2 | `ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua` + `enabled.txt` | 1407 / 0 | 0b70465317ef3fce / e3b0c44298fc1c14 |
| 3-4 | `ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua` + `enabled.txt` | 3173 / 0 | b31feb8ffb8adac1 / e3b0c44298fc1c14 |
| 5-6 | `ue4ss/Mods/StaminaMod/Scripts/main.lua` + `enabled.txt` | 2811 / 0 | 7667fed7e4d7b4cf / e3b0c44298fc1c14 |
| 7-8 | `ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` + `enabled.txt` | 196743 / 0 | d8f8d64f84d7e093 / e3b0c44298fc1c14 |
| 9-21 | `ue4ss/Mods/ueselftest/enabled.txt` + `Scripts/{main,part2..12}.lua` | 0 / 3987* / parts | enabled 0 B **must be uploaded** (−6 lesson); main 2c7e69cbf99280f7* |
| 22-23 | `ue4ss/Mods/zzdiag/Scripts/main.lua` + `enabled.txt` | 4086* / 0 | 12a1e55600566b2d* |
| 24 | `ue4ss/PS5Pad.ini` | 1736 | (manager's regen; see §3 N3) |
| 25 | `loadlist.txt` = `launch2-final-loadlist.txt` | 1766* | b860dee10e36a354* |

\* = RE-MEASURE-AT-DEPLOY (IC-dependent; §2). Not in the plan: spawner
main.lua, UEHelpers, settings/MVL/VTL, PRX/ploader (prep-owned; the
manager refuses non-scope paths by code).

**THE −6 LESSON, enforced by the automatic gate (Claude round-8 item 6).**
Launch-1's only real incident: the loadlist declared
`ueselftest/enabled.txt` which was never uploaded → MFA refused the WHOLE
module (`mfa_activate_failed -6`), launch wasted; all-or-nothing admission
worked as designed. Rule: EVERY loadlist row — enabled.txt-class included —
must have its file actually uploaded and readback-cmp'd. The gate is
**ID's `harvest/prelaunch_rowcheck.py`** (23,284 B, sha256
7fe528961d99d4afdf2d790c3cff80db222abb165e42cb25ea0152d148749167): it
fetches the live loadlist, parses every file row, and verifies each file
EXISTS at its EXACT declared size (plus sha via `--expect`). Run it as the
LAST pre-launch step (game closed):

```bash
python3 $PREP/harvest/prelaunch_rowcheck.py \
  --expect $LM/launch2-final-rowcheck-pins.json
# exit 0 = GO; the receipt lands under $PREP/evidence/rowcheck/<ts>/
```

`launch2-final-rowcheck-pins.json` (this directory; 30 path rows) pins
every managed/asserted row with size+sha256 and marks the two IC-dependent
rows `remeasure`. The pins were VALIDATED this session against a /tmp
fixture built from the exact staged bytes: **VERDICT PASS, 30/30 rows** —
and the first run caught the mid-prep IC revisions as SIZE-MISMATCH before
re-pinning, which is exactly the drift class the gate exists to catch
(`launch2-final-audit-receipt.txt`).

## 5. GO/NO-GO + ROLLBACK (reconciled with launch-1's)

GO requires every box PASS (or documented N/A) — common wake gates first
(GO-NO-GO.md §0: jailbreak boot, daemon census, klogsrv restart-once, eboot
count 0, Z2 ≤120 s ploader wait, klog tap NEW file BEFORE launch, no
unharvested prior crash).

PRX / stack gates:
- [ ] S1..S4 each built, gated (verify_build --manifest PASS at its own
      marker count), archived, sidecar'd; **`<final-stack-prx>` = the S4
      output resolved to a real path + sha.** No skipped steps (STOP law).
- [ ] markers-declared.json back to **18** at S4 (3 [SHIM] markers
      present); S3's [FL-MAP] markers (if any) declared.
- [ ] Deployed via prep_modded_launch + **decrypt-off readback** (curl -Q
      "SITE SELF"): magic 4f153d1d + full sha + size match. Round-4
      59b0528b stays the FALLBACK (return to it only by explicit reviewed
      rollback — re-upload the frozen PRX bytes, never rebuild).
- [ ] FY equivalence scheduled during the launch window.

Staging gates (game closed; this manifest):
- [ ] LIVE loadlist pulled + reconciled (sidecar law); launch-1 state
      confirmed (zzdiag + ueselftest rows + pad 1736) or re-staged per §5
      rollback note.
- [ ] The two RE-MEASURE rows re-pinned from actual `wc -c`/sha of the
      bytes being uploaded (zzdiag main, ueselftest main); loadlist +
      pins re-hashed after re-pin.
- [ ] Uploads in the §4 order; EVERY file readback `cmp` VERIFIED —
      **enabled.txt-class rows included** (−6 lesson).
- [ ] Pad uploaded (manager's regen or this dir's file); readback cmp;
      row 1736 (re-pin if the regen's `wc -c` differs).
- [ ] `prelaunch_rowcheck.py --expect launch2-final-rowcheck-pins.json`
      = **exit 0 / VERDICT PASS** — THE automatic pre-launch gate.
- [ ] Manager gates recorded: preflight 6/6 LOADS-CLEAN; keymap --check
      0 violations; conflicts = only the reviewed F7 co-fire
      (`--allow-cofire`); MiniMap CRLF bytes preserved end-to-end.

Handoff: user sequence (§7) issued; GO only when all above PASS.

**Rollback (both manifests' rollbacks reconcile):**
1. **launch-2 rollback = the launch-1 final state**: drop exactly the 8
   ladder rows → `launch2-final-loadlist-rollback.txt` (this directory,
   27 lines / 22 file rows / 1292 B / sha256
   9883ed1c0a4b4d03a437a14d8f518f53a4e342d669c531c891335756046332d6 =
   byte-identical to the live launch-1 `launch1-loadlist.txt` at
   zzdiag@4094 / ueselftest@3961). Manager path: `disable` the four mods +
   `deploy --apply --allow-cofire` (or `rollback --apply` = the prev
   pair). Pad row stays 1736 (ueselftest still resident). NOTE: if the
   zzdiag/ueselftest rows were RE-MEASURED at launch-2 deploy, the
   rollback file must carry the SAME re-measured values (never the stale
   4094/3961) — regenerate it from the live loadlist at rollback time if
   in doubt.
2. **launch-1 rollback = the round-4 resident state** (spawner + zzdiag
   only, pad back to 3718 in place): GQ's `launch1-loadlist-rollback.txt`
   (545 B, 14 lines / 9 file rows) — **its zzdiag row reads 4065, STALE
   twice over** (live is 4094; launch-2 may re-measure again). At any
   rollback execution, re-pin that row to the bytes actually on console
   (or redeploy zzdiag via lua_fast_deploy.sh which self-pins) before
   upload. Remote DELE of mod files optional (unlisted lua files are
   never admitted).
3. Next-launch confirmation after either rollback: the dropped mods'
   `[l22] Lua mod started` lines absent; spawner/zzdiag/ueselftest
   markers unaffected; klog clean.

## 6. SELF-TEST + [MAP] + FNAME-EQ EXPECTATIONS (what launch-2 proves)

**ST2 (ueselftest v1 + IC's RUN-start line)** — `grep -a "\[ST2\]"`:
- Boot: `[ST2] boot v1 M=167 armed=true pf=true|false` — with the round-5
  path shim + IB's writable-shim in the FINAL stack, **pf=true is now the
  expectation** (launch-1 was the no-shim degrade: progress-io
  unavailable → resume disabled, SKIP-class by design).
- Combo press (L1+R1 → F12): `[ST2] K1 f12`, `[ST2] K2 q` — **launch-1
  never fired these (0 idx lines)**; launch-2's re-arm is proven BY THIS
  PAIR. If K1 absent again: pad/keybind owner (verify the L1+R1 row +
  `--check`), not the build.
- Run start: **`[ST2] RUN start 1..167`** (IC's new line) then
  `[ST2] part2 loaded 21` + per-part lines (LOAD-FAIL = deploy gap: a
  part missing from the loadlist/activation).
- 167 numbered rows `idx/167 name PASS|FAIL|SKIP`; end block:
  `167/167 skip-class-list PASS 27-skips` + 27 unnumbered SKIP lines +
  `[ST2] SUMMARY <N> of 167 PASS` + `[ST2] done`.
- **io rows upgrade with the shim**: `io-roundtrip` must read
  **`PASS rw roundtrip`** (launch-1: SKIP no-io-path; 'w-only open-r-miss'
  would be a P0 finding per FS). Full pass = SUMMARY 167 of 167 PASS,
  zero FAIL, done present, clean exit. `SKIP no-valid-pc` → re-press the
  combo ONCE in-world, not a failure.
- Acceptance bar (LADDER-V2 §3.2): ≥95% rows PASS AND 0 FAIL in the four
  type-critical families (hook / property-array-write / io / delay-loop).
  FAIL inside a critical family = STOP; the ladder verdicts do not
  override it.

**[MAP] + [FL-MAP] (the world-fix build's proof)** — launch-1 finding 5:
zzdiag's `[MAP]` lines ABSENT (World lookup nil/"?", db/pc fine). With S3
in the stack:
- zzdiag v11: `[zz] [MAP] <name> @<n>s` first-LoadMap/change lines
  (reflected `World:GetName`) — expected PRESENT this launch.
- **IA's runtime logger: `[FL-MAP]` ordered map log** (the first-launch
  map sequence, per IA's build notes — grep -a "\[FL-MAP\]").
- Still absent → owner = the S3 world-fix (engine-side World resolution),
  NOT the Lua side; do not touch zzdiag.

**PRB-7 fname-eq (zzdiag v11's new row)** — after the proven P1..P6 chain
(`[PRB-1] inv ok=1 type=userdata`, `id ok=1`, `tos ok=1`, `TS ok=1`):
`[PRB-7] fname-eq ok=1 w=1` — the FName ctor round-trip from the ItemId
string form: `f == ii` (the `__eq` metamethod firing on the userdata) and
`str(f) == s3` (string round-trip). ok=0 w=1 ⇒ the `__eq` metamethod did
not fire (a REAL parity finding — CALL-SHEET's FName-eq note); e ⇒ the
ctor threw (read the indented error tail). This row rides every probe
pass (auto at +18 s and per F7).

**Ladder v2 per-type markers** (LADDER-V2 §2, unchanged): CheatManager
Constructed/Enabled (or the already-exist skip-shape) in EVERY ClientRestart
window; ConsoleEnabler `ConsoleKey[n]:` read-back lines incl. Tilde/F10
(first window per boot only — WasConsoleCreated design); MiniMap started +
NO `:4270` too-old line + HUD captures t0/t0+30 s (USER-VISIBLE primary) —
**io verdict now tiered FULL-capable** (shim in stack): map-menu setting
change → clean exit → relaunch → persisted = FULL io PASS; Stamina init
line + zero `[Stamina]` async errors; spawner control (`Loaded v1.0` /
`Menu created`) green WHILE the four run. P1's `[ST]` fault-ledger line
(`copyin_fault_ledger_st` =15, `..._st_pass` =1) at startup. 3-passes law:
type ACCEPTED only after the same pass markers in r1+r2+r3 (byte-identical
staging, no file churn; a build change resets the count).

**Must-be-absent**: `mfa_activate_failed`, `code=-8`, `[zz] T` gaps (pump),
MiniMap `:4270`, `[FN] seam refused`, klog copyin bursts / nonsleeping /
fatal backtrace.

## 7. User sequence (one launch; GB §3 order + self-test re-arm)

1. LAUNCH Townfall (final-stack PRX armed per §1). 2. Load the SAME save.
3. Get IN-WORLD (ClientRestart window: CheatManager + ConsoleEnabler
markers). 4. Wait ~10-15 s (MiniMap tick spool-up). 5. **SELF-TEST COMBO:
hold L1, tap R1 ONCE** → K1/K2 + RUN start + the 167-call run + SUMMARY
(re-press once in-world if `SKIP no-valid-pc`). 6. L3+R3 once: spawner
menu opens — navigate, close (control rung). 7. Play 60-90 s; minimap HUD
captures at t0 and t0+30 s. 8. Optional (now FULL-capable): open MiniMap's
map menu, change one setting (io round-trip). 9. One UI open at a time.
10. CLEAN EXIT via in-game quit to dashboard. Crash: note code + WHEN,
harvest, REBOOT.

```bash
python3 $PREP/harvest/harvest.py --label ladder2-final-r1
grep -a "\[BUILD\]" <UE4SS.log>              # the FINAL STACK stamp (new id; != l26-20261011T030618Z)
grep -a "Lua mod started" <UE4SS.log>        # 7 mods admitted: spawner, zzdiag, ueselftest, CheatManager, ConsoleEnabler, MiniMap, Stamina
grep -a "\[ST2\]" <UE4SS.log> | tee /tmp/st2-l2f.log
grep -a "RUN start\|SUMMARY\|io-roundtrip\|done" /tmp/st2-l2f.log
grep -a "\[MAP\]\|\[FL-MAP\]" <UE4SS.log>
grep -a "fname-eq" <UE4SS.log>               # [PRB-7]
grep -a "CheatManagerEnabler\|ConsoleKey\[\|TownfallMiniMap\|too old\|\[Stamina\]" <UE4SS.log>
grep -a "Menu created\|Loaded v1.0" <UE4SS.log>
grep -a "copyin_fault_ledger" <UE4SS.log>    # P1's [ST] line
grep -a "mfa_activate_failed\|code=-8\|seam refused" <UE4SS.log>   # must be ABSENT
```

## 8. Artifacts in this directory (all pinned this session)

| file | bytes | sha256 | role |
|---|---|---|---|
| `MANIFEST-LAUNCH-2-FINAL.md` | this | — | the launch-2 final manifest |
| `launch2-final-loadlist.txt` | 1766 | b860dee10e36a3548551013f1351e1ba8ac487a36b3740f77fdee26880e6eee2 | 35 lines / 30 file rows; zzdiag@4086 + ueselftest@3987 (both RE-MEASURE) |
| `launch2-final-loadlist-rollback.txt` | 1292 | 9883ed1c0a4b4d03a437a14d8f518f53a4e342d669c531c891335756046332d6 | = live launch-1 state (zzdiag@4094/ueselftest@3961); re-measure if launch-2 re-pinned |
| `launch2-final-PS5Pad.ini` | 1736 | 6f54a6acab59d9cf2caf4f4a0eaf008608d9907e5f0d9ceafad8f4188b42f5fe | FV regen over the full 7-mod set; --check 0/0 |
| `launch2-final-rowcheck-pins.json` | 5586 | 8441c8622de97622571290c66c8ff5bb9e606f69070c2b3af44294e1c60f2e70 (re-hash after any re-pin) | 30-row --expect pins for prelaunch_rowcheck.py |
| `launch2-final-audit-receipt.txt` | 1190 | — | offline audit: keymap --check 0/0 + rowcheck fixture 30/30 PASS + the mid-prep drift catch |

Open items at deploy (all IC/IA/IB-dependent): re-measure zzdiag main +
ueselftest main (and re-hash loadlist/pins/rollback rows); re-check
`round5-staging/world-fix/` contents + any new declared markers (S3) and
IB's writable-shim patch (S4); `<final-stack-prx>` sha when S4 builds.

— agent IE (ZCode). GG/GC/GB/FV/FJ/GQ/ID files read-only; the /tmp fixture
(`/tmp/ie-l2f-fixture`) is disposable; no mods, no builds, no console
contact. MANIFEST-LAUNCH-2.md updated with a superseded pointer only.
