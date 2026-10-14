# agent-GQ — wake-time launch manifests (both launches fully staged offline)

Agent GQ (ZCode), 2026-10-10, Mac-only battery-safe prep. No console
contact, no builds, no edits to GB's/GC's/GG's/FV's/FJ's files (hash
re-verified and pinned). All deliverables under
`outputs/2026-10-08-l25-prep/launch-manifests/`:

| artifact | bytes | sha256[:16] | what |
|---|---|---|---|
| MANIFEST-LAUNCH-1.md | 13342 | 96c727fc2d80a64a | round-4 spawn + zzdiag-v11 + ueselftest manifest (upload list w/ per-file sha256+bytes, loadlist law, rollback, user sequence, watch list) |
| launch1-loadlist.txt | 1292 | 17b65436f5d1a12e | synthesized 27-line / 22-file-row manifest (pad row re-pinned 3718->1736; 13 ueselftest rows appended in GB's plan order) |
| launch1-loadlist-rollback.txt | 545 | 8f86dfb7ca155427 | rollback reference (pad row back to 3718, ueselftest rows off, zzdiag kept) |
| launch1-PS5Pad.ini | 1736 | 957d3e25e3c4802a | byte-copy of GB's round-4 pad; keymap --check = 0 violations / 0 warnings (run this session vs the round-4 set, --expect-size 1736) |
| MANIFEST-LAUNCH-2.md | 11916 | f37009f97d30ecb4 | ladder-v2 manifest: modmanager-synthesized plan + GC reconciliation |
| launch2-loadlist.txt | 1766 | 314d1bccdf591f02 | modmanager dry-run output: 35 lines / 30 file rows (launch-1 state + 8 type-mod rows) |
| launch2-PS5Pad.ini | 1736 | a6313eec781173df | manager-regenerated pad (7 bindings = 6 pinned + L1+R1=F12; audit --check 0/0) |
| launch2-loadlist-rollback.txt | 1292 | 17b65436f5d1a12e | = launch-1 state; disable-path synthesis verified BYTE-IDENTICAL to it |
| launch2-deploy-plan.txt | 3835 | 3d40391b4325d87c | verbatim dry-run upload plan (order law) + audit receipt (0 violations / 0 warnings) |
| launch2-modmanager-manifest.json | 8196 | caa0fb0b6c7291bb | /tmp-store provenance (per-file size+sha of all 6 staged mods) |
| VERIFICATION-SHEET.md | 8521 | 5097027ba0ec6829 | per-artifact pin + admission rule + order law + who-uploads-what split + notes N1-N5 |
| GO-NO-GO.md | 7288 | 2b09a902988d56ee | common wake gates + per-launch checklists + hard stops (WAKE-RUNBOOK §1/§10/§11 law) |

## Both manifests are READY

- LAUNCH 1 is fully precomputed: 15-row upload list (13 ueselftest files
  + pad + loadlist, every row sha256+wc-c pinned), the exact loadlist
  content with per-row byte counts, pad row re-pin arithmetic
  (3718->1736), rollback rows, GO/NO-GO. PRX stays a placeholder
  `<round4-prx>` (path lands with the AC-power build receipt); deploy +
  decrypt-off readback + FY-equivalence gates are written as checklists.
- LAUNCH 2 was synthesized FOR REAL by GG's modmanager.py (dry-run,
  /tmp store, zero console contact): six mods installed (zzdiag,
  ueselftest, CheatManagerEnabler 1407, ConsoleEnabler 3173, MiniMap
  196743 CRLF, Stamina 2811 — every staged sha == source sha), all 6
  preflights LOADS-CLEAN on the real lua5.5 harness, deploy refused
  without `--allow-cofire` exactly per GG's gate design, final
  synthesis 1766 B / 30 file rows, audit 0 violations / 0 warnings.
  Rollback exactness proven twice: disabling the four mods regenerates
  launch1-loadlist.txt byte-identical (cmp).

## The multi-file admission rule (task's open question — documented)

A mod file is readable by the game ONLY if listed in loadlist.txt:
MFA activation (module_start) preloads every listed lua_mod/config/
script file into owned memory; after module_start fresh /data opens
fail ENOENT, so UE4SS serves require/dofile from the activation
(modforge_activation.h; GG DESIGN; LADDER-V2 "unlisted lua files are
never admitted"). ueselftest's parts resolve at L1+R1 PRESS TIME via
require -> native module searcher (LuaMod.cpp:1073-1112), so ALL 12 lua
files need size-exact rows — upload alone is dead weight. GB's 13-row
plan (enabled.txt + 12 lua) is the correct reading; the manifests pin
it. zzdiag-v7 sidecar + ConsoleCommandsMod's 3 required sidecar rows
are the precedent.

## Drift found (reported, NOT silently fixed — details in MANIFEST-LAUNCH-2 §4-5)

1. GC's "PS5Pad.ini untouched at 3718 B, no re-pin" is true only for
   the four type-mods; GC's own launch-2 step 5 presses the ueselftest
   combo, so the real pad is the 1736 B round-4 profile and the pad row
   pins 1736. Reconciled: pad row = 1736 in BOTH launches; the two
   pinned pad FILES differ only in the inputs-sha256 provenance header
   (bindings byte-identical) — N3/N4 in the verification sheet; GB's
   header hash is not reproducible from canonical paths (generation
   paths unknown), so its artifact is hash-pinned, never regenerated
   at deploy time.
2. GC's row-count law "base 7 + 8 = 15 file rows" assumes the bare l23
   base; the actual launch-2 composition is 30 file rows (7 base + 2
   zzdiag + 13 ueselftest + 8 new). The GO/NO-GO check pins 30/35.
3. GC staged round-4 for launch 2; Claude round-6 (WAKE-RUNBOOK §11,
   later) puts the FX-patched FINAL build in this launch instead —
   manifest follows round-6 (`<fx-final-prx>`), with an explicit
   reviewed-decision box if the FX build is missing.
4. Row ORDER: GB's deploy script order (enabled-first, numeric parts)
   vs modmanager's canonical order (main-first, lexicographic parts,
   enabled last) — identical row SETS and sizes; MFA sorts by path
   internally (launch-1 synthesis cross-validated against GG's tool:
   same 13 rows, same sizes, order-only diff). Each launch pins exactly
   one order (launch 1 = GB's, launch 2 = the manager's); never mixed.
5. GG's flat-dir install stages ONLY main.lua: ueselftest must be
   installed from a Scripts-shaped input tree or the 11 parts silently
   drop (LOAD-FAIL at press time). Documented in MANIFEST-LAUNCH-2 §3
   with the exact /tmp input shapes used.
6. **zzdiag-v11 was REVISED mid-prep by a concurrent agent** (16:28
   local; adds [MAP] milestone lines per "Claude #4"): 4084 B /
   84fb6a93... (FJ's report) -> 4065 B / 779f4c16... (current). Not my
   edit (my session only read it; the /tmp store preserved the original
   bytes, proving the install-time pin). All references were
   re-synthesized against the current 4065 file; lua_fast_deploy
   self-corrects the row from actual wc -c at deploy time, and both
   manifests carry a re-measure-at-wake step. If it moves again, re-pin
   that one row only.

Verification performed this session: keymap --check 0/0 on the round-4
pad; manager synthesis cross-validated against the hand-built launch-1
reference; disable-path rollback == launch-1 reference byte-identical
(cmp, twice — before and after the zzdiag revision); all four type-mod
staging hashes on disk == LADDER-V2/MANIFEST.md citations; spawner
freeze copy 21415 / f586043a confirmed. Limits: PRX paths are
placeholders by design; the /tmp store is disposable (all outputs
pinned here); nothing was console-tested (that is the wake window's
job, via GO-NO-GO.md).
