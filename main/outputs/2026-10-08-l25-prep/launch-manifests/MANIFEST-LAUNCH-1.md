# MANIFEST-LAUNCH-1 — round-4 spawn test + zzdiag-v11 probe + ueselftest (L1+R1)

Agent GQ (ZCode), 2026-10-10. Mac-only prep, battery-safe: every artifact
below is precomputed offline. No console contact, no builds, no mod edits.
Sources: WAKE-RUNBOOK-20261010.md §10 (launch law) + §2 (zzdiag deploy),
ueselftest/CONSOLE-TEST-PLAN.md (GB), zzdiag-v11/ (FJ), keymap/MAPPING.md
(FV), mod-manager/DESIGN.md (GG), l23 harvest loadlist receipt (base
composition), prep_modded_launch receipts (who-uploads-what).

Constants (WAKE-RUNBOOK copy-paste block):

```bash
CONSOLE=ps5-host-not-configured.invalid
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
PREP=$WHAT/outputs/2026-10-08-l25-prep
LM=$PREP/launch-manifests
FTPR="ftp://$CONSOLE:2121"
ROOT=/data/modbridge/roots/PPSA33286
```

## 0. What this launch is

ONE user launch that carries, in one session and this order of user
actions: (1) the round-4 SPAWN acceptance on the unmodified spawner
original bytes (L3+R3 menu -> select item -> SPAWN), (2) the zzdiag-v11
pickup-read probe riding the same session ([PRB-*] markers, free
corroboration), (3) the UE4SS parity self-test armed on L1+R1 (F12) after
the spawn settles. PRX = round-4 build (placeholder below; NOT round-3
56a9b1a0 — WAKE-RUNBOOK §10.3). Everything Lua/ini-side is precomputed
here so the wake window is turnkey.

## 1. PRX (owned by prep_modded_launch, NOT this manifest)

| item | value |
|---|---|
| PRX path | `<round4-prx>` — TBD at AC time (build receipt lands in outputs/2026-10-08-l26-*/ when AC power returns; WAKE-RUNBOOK §10.3 contents LOCKED) |
| Deploy tool | `python3 $PREP/harvest/prep_modded_launch.py --variant A --prx-path <round4-prx>` |
| Hard gates | `verify_build.sh <prx> --manifest $PREP/harvest/markers-declared.json` = PASS BEFORE deploy; decrypt-off readback (curl -Q "SITE SELF"): magic 4f153d1d + full sha256 match; round-3 stays FALLBACK |
| FY equivalence | during the launch window, rebuild the same snapshot with the pre-FY serial driver (/tmp/build_variants.py.pre-FY.bak); PRX shas must match, else stop using concurrent builds |

prep_modded_launch ALSO owns (never this manifest): UE4SS-settings.ini
(1947 B, b980cea7...), ploader.ini (440 B), the ONE loadpayload arm, the
klog tap, the eboot monitor. It NEVER touches mod files, PS5Pad.ini, or
loadlist.txt (FN freeze doc flags zzdiag as riding lua_fast_deploy, not
prep — same split applies here).

## 2. zzdiag-v11 (owned by lua_fast_deploy, WAKE-RUNBOOK §2 — BEFORE this staging)

```bash
bash $PREP/harvest/lua_fast_deploy.sh $PREP/zzdiag-v11/main.lua probe-v11-1
```

Uploads `$PREP/zzdiag-v11/main.lua` to `$ROOT/ue4ss/Mods/zzdiag/Scripts/main.lua`,
rewrites the zzdiag loadlist row size-exact to the ACTUAL `wc -c` of the
deployed bytes, verifies both (`SCRIPT VERIFIED`, `LOADLIST OK`), arms
ploader if not resident, starts the klog tap.

**REVISION NOTE (observed during this prep):** zzdiag-v11/main.lua was
revised by a concurrent agent at 16:28 local (adds `[MAP]` milestone
lines per "Claude #4"): FJ's reported bytes were 4084 B / sha256
84fb6a939d72f1d073cc7220731f61e396e5676b08877f3752e007d51b9e631d; the
CURRENT file this manifest pins is **4065 B / sha256
779f4c16d599661ea2c8bc11b3d9024c563ed04aa1addab6efe507b11265ce8d**.
`lua_fast_deploy.sh` self-corrects the row from the actual bytes, but
BEFORE upload re-measure (`wc -c` + `shasum -a 256`) and use THAT value
in every loadlist reference below (all rows in this manifest's files
assume 4065). If the file moved again, re-pin the one row — never
hand-shrink anything else. The live zzdiag row currently reads 4092
(v10, FN freeze); after this step it must read the measured v11 size.
Its enabled.txt row (0) is untouched.

## 3. ueselftest — ALL 12 files admitted (the loadlist-row law, exact rule)

**RULE (documented, from modforge_activation.h + GG DESIGN + GB plan +
zzdiag precedent): a mod file is readable by the game process ONLY if it
is listed in loadlist.txt with a size-exact row.** MFA activation
(module_start) preloads every listed file of a consumed role (lua_mod,
config, script) into owned memory; after module_start a fresh open of
/data fails with ENOENT, so UE4SS serves Lua/require/dofile reads from the
activation, never from disk. An uploaded-but-unlisted file can never be
opened at runtime. GB's LP() loader resolves parts via `require("partN")`
with dofile fallbacks, executed at L1+R1 PRESS TIME (long after
module_start) — so ALL 12 lua files (main + part2..part12) NEED loadlist
rows, not just FTP upload. GB verified `require` works through the
composed runtime's native module searcher (custom_module_searcher ->
native_lua_module, scripts_path-relative, LuaMod.cpp:1073-1112) "as long
as each part has a loadlist row". zzdiag multi-file precedent: the
sidecar-row law (zzdiag-v7 LOADLIST-ROW.txt; ConsoleCommandsMod ships 3
required sidecars as rows in FO's MANIFEST). Answer to the task question:
**FTP upload alone is INSUFFICIENT; every part gets a row; enabled.txt
(0 B) is a row too.**

## 4. FTP upload list (game CLOSED; order law: mod files FIRST, pad, loadlist LAST)

Remote base `$ROOT` (all text files — plain-text law, no SITE SELF
decrypt hazard; readback = fetch + `cmp`):

| # | remote path | local file | bytes | sha256 |
|---|---|---|---|---|
| 1 | `ue4ss/Mods/ueselftest/enabled.txt` | (create empty at deploy) | 0 | — (empty) |
| 2 | `ue4ss/Mods/ueselftest/Scripts/main.lua` | `$PREP/ueselftest/main.lua` | 3961 | 1eb63f7d6db90bb798c0642037ab4001c94290f745a8188ee36645402af1709b |
| 3 | `ue4ss/Mods/ueselftest/Scripts/part2.lua` | `$PREP/ueselftest/part2.lua` | 3735 | 7ba044fb38e25ab1969824d6433bb5f96ec40cfb807c0398f58fb7af168cca93 |
| 4 | `ue4ss/Mods/ueselftest/Scripts/part3.lua` | `$PREP/ueselftest/part3.lua` | 3724 | fc1de5fdba0bf1c8a2ad3998985a0ad317a0ae7d42f219cdb2d649f365d873a |
| 5 | `ue4ss/Mods/ueselftest/Scripts/part4.lua` | `$PREP/ueselftest/part4.lua` | 3731 | 2c303fd51a36a6d78556f70a92dd30eabead59d0cdbbfbaf2331b11692207cd9 |
| 6 | `ue4ss/Mods/ueselftest/Scripts/part5.lua` | `$PREP/ueselftest/part5.lua` | 3869 | 9ba4300f391e7e005d99f46ecfb8329e51dfadec25b970363fafe3e616901ff0 |
| 7 | `ue4ss/Mods/ueselftest/Scripts/part6.lua` | `$PREP/ueselftest/part6.lua` | 3732 | ad4c3c918231e3ee6c8d7b36553ed0081ff43da261a6a916eb881eb83e3b4022 |
| 8 | `ue4ss/Mods/ueselftest/Scripts/part7.lua` | `$PREP/ueselftest/part7.lua` | 3646 | 052ffc3bbfb52bb7cd38d245f4b04d4d5b9ac9a54c49d21c8d03e58833bde1ce |
| 9 | `ue4ss/Mods/ueselftest/Scripts/part8.lua` | `$PREP/ueselftest/part8.lua` | 3662 | efcb0eedc744c6ecdaed54537cb78284df8a3f8f6d6cd580c8f937a7b900d318 |
| 10 | `ue4ss/Mods/ueselftest/Scripts/part9.lua` | `$PREP/ueselftest/part9.lua` | 3797 | 29274afe6ecafb3749344edbc354469fa9c7b24cf856a17620b96b162d1bd40e |
| 11 | `ue4ss/Mods/ueselftest/Scripts/part10.lua` | `$PREP/ueselftest/part10.lua` | 3681 | ebf628174d8306fab64c4d445b7668e79db663c7cf3b4db2b835f3245045825d |
| 12 | `ue4ss/Mods/ueselftest/Scripts/part11.lua` | `$PREP/ueselftest/part11.lua` | 2251 | 7de0c0db64c2986c4f809d817db2c9ae1770aeb6c0b90b36a8e193c1f20bd29c |
| 13 | `ue4ss/Mods/ueselftest/Scripts/part12.lua` | `$PREP/ueselftest/part12.lua` | 3180 | d1247ac80c7a22627d6f5b54d264bdf4502fccd05643f439e4f34adb56f860d1 |
| 14 | `ue4ss/PS5Pad.ini` | `$LM/launch1-PS5Pad.ini` (byte-copy of GB's `$PREP/ueselftest/PS5Pad-round4-proposed.ini`) | 1736 | 957d3e25e3c4802af94b1f561d247cc4e7a383193520bb94b4fab1b98f7ed453 |
| 15 | `loadlist.txt` | `$LM/launch1-loadlist.txt` (see §5 — pull LIVE first, reconcile, then this shape) | 1292 | 17b65436f5d1a12e4bc22b297c4c00159d268fcd155bef6057c157bb85a17c53 |

PS5Pad.ini verification (performed this session): `keymap-gen.py --check
.../PS5Pad-round4-proposed.ini --pin-pad <l23 3718 B profile> --ladder
pairs10 --mod TownfallItemSpawner=<console copy> --mod zzdiag=<v11> --mod
ueselftest=<main.lua> --expect-size 1736` = **0 violations, 0 warnings**
(7 bindings: 6 pinned verbatim + L1+R1 = F12). Diff vs the console-proven
3718 B baseline: bindings identical except the single added row
`L1+R1 = F12`; the baseline's 6 pinned rows are carried verbatim.

## 5. loadlist.txt — MFA row re-pin

Sidecar law (GG DESIGN §3 / EF TEST-PLAN): PULL THE LIVE loadlist FIRST;
reconcile — keep every live value except: (a) the PS5Pad.ini config row
re-pinned 3718 -> **1736** (REPLACED IN PLACE, position preserved), (b)
the 13 ueselftest rows APPENDED at the end (GB's plan order:
enabled.txt, main.lua, part2..part12). The reference file
`launch1-loadlist.txt` (this directory) is the expected result from the
known live state (l23 base + zzdiag rows after §2):

- 27 lines total; **22 file rows** (base 7 + zzdiag 2 + ueselftest 13).
- Per-row byte counts are the wc -c values in §4 and 21415/9962/etc for
  the resident rows (unchanged).
- The loadlist file itself is NOT size-pinned by MFA (it IS the
  manifest); its own size is recorded for the receipt only.

```bash
# fetch live, verify assumptions, then upload the reconciled file LAST
curl -s --max-time 12 "$FTPR$ROOT/loadlist.txt" -o /tmp/l1-live.txt
grep -c "ueselftest" /tmp/l1-live.txt        # must be 0 (no stale rows)
grep "ue4ss/Mods/zzdiag/Scripts/main.lua" /tmp/l1-live.txt   # row must read the measured v11 size (4065 as of this manifest)
# ...reconcile per the law above (or verify live == launch1-loadlist.txt shape)...
curl -s --max-time 15 -T $LM/launch1-loadlist.txt "$FTPR$ROOT/loadlist.txt"
# verify readback
curl -s --max-time 12 "$FTPR$ROOT/loadlist.txt" | cmp $LM/launch1-loadlist.txt - && echo LOADLIST VERIFIED
curl -s --max-time 12 "$FTPR$ROOT/loadlist.txt" | grep -c "ueselftest"   # must be 13
curl -s --max-time 12 "$FTPR$ROOT/loadlist.txt" | grep -c $'\t'          # 27 tab-rows
curl -s --max-time 12 "$FTPR$ROOT/ue4ss/PS5Pad.ini" | cmp $LM/launch1-PS5Pad.ini - && echo PAD VERIFIED
for f in main part2 part3 part4 part5 part6 part7 part8 part9 part10 part11 part12; do
  curl -s --max-time 12 "$FTPR$ROOT/ue4ss/Mods/ueselftest/Scripts/$f.lua" | cmp "$PREP/ueselftest/$f.lua" - && echo "$f VERIFIED"
done
```

The full GB deploy script (ueselftest/CONSOLE-TEST-PLAN.md §1-2) is the
operator's copy-paste source; this manifest pins the same 13 rows, the
same pad, and adds the reconciliation checks above.

## 6. Rollback (precomputed: `launch1-loadlist-rollback.txt`, 545 B, sha256 8f86dfb7ca1554270fcf15975389ac7d37bab913980daffda25107f5d111d2f1)

1. Delete the 13 ueselftest rows (and ONLY those); restore the PS5Pad.ini
   config row to `3718` IN PLACE. Reference file = base + zzdiag@4065
   (14 lines / 9 file rows). zzdiag-v11, the spawner, UEHelpers, the PRX
   and settings are NOT rolled back by this launch.
2. Restore the previous PS5Pad.ini bytes: either re-upload the l23-pinned
   3718 B profile (MAPPING.md rollback law: deterministic regeneration
   reproduces old bytes; the l23 harvest copy is the exact byte source)
   or keep the round-4 pad if the next launch (launch 2) is imminent —
   DOCUMENTED CHOICE: launch 2 keeps it (see MANIFEST-LAUNCH-2 §1).
3. Remote DELE of the 13 ueselftest files optional (unlisted lua files
   are never admitted — MFA allowlist law; note in the receipt).
4. Next-launch confirmation: `[ST2] boot` line absent; 5 `[l22] Lua mod
   started` lines unchanged (spawner, zzdiag only); spawner control
   unaffected.

## 7. User sequence (one launch; GB §3 order — spawn FIRST, self-test additive)

1. LAUNCH Townfall (round-4 PRX armed per §1). 2. Load the SAME save. 3.
   Get IN-WORLD. 4. Watch boot markers (below). 5. SPAWN TEST: L3+R3
   once -> menu opens -> select item -> SPAWN (the round-4 acceptance;
   also watch `[zz]`/`[PRB-1..6]` riding free). 6. Wait ~10 s after the
   spawn settles, then press **L1+R1 ONCE** (hold L1, tap R1): K1/K2 +
   the 167-call run + `[ST2] SUMMARY`. 7. If `[ST2] ... SKIP
   no-valid-pc` lines appear (pressed too early), press L1+R1 once more
   in-world. 8. CLEAN EXIT via in-game quit to dashboard (never a power
   event). Crash: note code + WHEN, harvest immediately, REBOOT.

## 8. Watch list (harvest BEFORE any relaunch; grep -a ALWAYS)

```bash
python3 $PREP/harvest/harvest.py --label l26-round4-spawn-st2-r1
grep -a "\[BUILD\]" <UE4SS.log>            # round-4 stamp; snapshot != e8c4bb62
grep -a "Spawned pickup\|inventory ID\|Failed at" <UE4SS.log>   # spawn acceptance
grep -a "\[PRB-\|\[zz\]" <UE4SS.log>       # zzdiag-v11 probe ladder
grep -a "\[ST2\]" <UE4SS.log> | tee /tmp/st2-r1.log
grep -a "\[ST2\] SUMMARY\|\[ST2\] done" /tmp/st2-r1.log         # expect 167 of 167 PASS
grep -a "io-roundtrip" /tmp/st2-r1.log      # must read PASS rw roundtrip on console
grep -a "mfa_activate_failed\|code=-8" <UE4SS.log>             # must be ABSENT
grep -a "Lua mod started" <UE4SS.log>       # spawner + zzdiag + ueselftest admitted
```

klog (/tmp/l25-variant-A/klog-*.log, NEW file per launch, tap started
BEFORE launch): no copyin bursts, no nonsleeping, no fatal backtrace.
Three-passes law (GB §4): the ST2 summary must be IDENTICAL across 3
clean-exit launches (r1/r2/r3) with ZERO file changes between passes;
`diff` the three `[ST2]` extracts — only value-bearing detail noise may
differ, verdicts must match. The spawn re-test is optional after r1.

— agent GQ. GG/GC/GB/FV/FJ files read-only; hashes above computed this
session from the exact staged bytes.
