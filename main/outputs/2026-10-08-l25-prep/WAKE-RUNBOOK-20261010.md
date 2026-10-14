# WAKE RUNBOOK — 2026-10-10 (post MENU-UP, ItemId-blocker round)

> EVIDENCE LAW UPDATE (agent-HA, 2026-10-10, appended — history below is
> unchanged): every /tmp path in this runbook is LEGACY. Klog taps,
> readbacks and receipts now land durable under
> `outputs/2026-10-08-l25-prep/evidence/runtime/...` by tool default; each
> legacy path below is annotated "legacy, now durable at ...". See
> `EVIDENCE-RETENTION.md` (same directory).

Written by agent FP (ZCode), 2026-10-10 early UTC, while the console is in
REST MODE and agents FJ–FO finish offline. Context: STATE-L26-WAVE.md §9,
Claude round-3 directives in `CLAUDE-ROUND3-RECEIPT.md`.

Position at sleep: PRX **efbfc656** resident (round-2 bridge build — the mod
MENU opened on it, 24 pickups, DRY 6/6); the ONLY failure is
`str(pickup.PickupInventoryItem.ItemId)` (reverse read, unrouted). Tonight,
offline: FJ zzdiag-v11 probe, FK refusal logging, FL/FM reverse-site routing
(root rebuilt/repackaged a routed PRX — sha lands in build receipts), FN
known-good freeze + original-mod identity doc, FO 12-mod ladder schema.

**Master order (Claude's instrument-first): probe names the hop on the
CURRENT build BEFORE any new PRX goes on the console.**

Constants (copy-paste block):

```bash
CONSOLE=ps5-host-not-configured.invalid
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
PREP=$WHAT/outputs/2026-10-08-l25-prep
FTPR="ftp://$CONSOLE:2121"
# mod root on console: /data/modbridge/roots/PPSA33286/
# PRX on console:      /data/plugins/modbridge_supplier_composition.prx
```

## 1. USER wakes the console → ROOT restores daemons

Expect daemon deaths like tonight (klogsrv died in a sleep cycle;
`harvest/wakeup_prep.sh` CHECKS daemons but does NOT start them).

```bash
# 1a. console state: awake? no game running?
curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c eboot        # must be 0
curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c payload.elf  # likely 0 (see 1c)

# 1b. daemon census (kstuff.elf / pldmgr.elf / ftpsrv.elf / klogsrv.elf)
for d in kstuff.elf pldmgr.elf ftpsrv.elf klogsrv.elf; do
  echo "$d: $(curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c "\"$d\"" || true)"
done

# 1c. IF klogsrv is ABSENT (expected) — restart it ONCE (never double-dispatch):
curl -s --max-time 20 "http://$CONSOLE:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fklogsrv-ps5.elf"
sleep 5; curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c klogsrv.elf   # must be >= 1
```

**ploader-resident wait (Z2 law).** If a `payload.elf` IS resident, its window
is bounded at ~120 s from arm: DO NOT dispatch another loadpayload, DO NOT
launch — WAIT for the "Timeout, shutting down." toast / the pid to disappear
(≤120 s), then proceed. Never double-dispatch a loadpayload.

## 2. ROOT deploys zzdiag-v11 (game closed) — ~30 s, NO PRX change

Claude Q1: instrument zzdiag, NOT the mod. The probe launch rides the
RESIDENT efbfc656, so the failing hop is named on the CURRENT build first.

```bash
# game must be closed (script enforces this too)
bash $PREP/harvest/lua_fast_deploy.sh $PREP/zzdiag-v11/main.lua probe-v11-1
# (if agent FJ staged it elsewhere, use the exact path from the FJ report — do NOT improvise a probe)
```

`lua_fast_deploy.sh` uploads the script, rewrites the loadlist zzdiag row
SIZE-EXACT, verifies both (`SCRIPT VERIFIED`, `LOADLIST OK`), arms ploader if
not resident, and starts the klog tap into /tmp/l25-variant-A/ (legacy,
now durable at outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-A/ --
agent-HA 2026-10-10 evidence law; the tool writes durable by default since
that date). It prints
`=== LAUNCH NOW ===` when ready. If the v11 file is not on disk yet, FJ has
not landed — STOP and read FJ's report first.

## 3. USER launch #1 — PROBE (names the failing hop)

User steps (one launch, user-launched, no rapid-fire):

1. LAUNCH Townfall. 2. Load the SAME save. 3. Get IN-WORLD (controlling the
   character). 4. Wait ~10 s. 5. Press **L3+R3 ONCE** (OPTIONS = game pause;
   F7 also works). 6. Optionally attempt a spawn from the menu to exercise
   the ItemId path. 7. CLEAN EXIT via in-game quit to dashboard — never a
   power event. If it crashes: note code + WHEN, harvest immediately, REBOOT
   before any next launch.

ROOT harvests (read-only, BEFORE any relaunch):

```bash
python3 $PREP/harvest/harvest.py --label l26-probe-v11-1
grep -a "\[PRB-" $PREP/evidence/runtime/l25-variant-A/klog-*.log /tmp/l25-variant-A/klog-*.log $PREP/evidence/*.log 2>/dev/null | tail -40
# (legacy /tmp/l25-variant-A/ kept for pre-2026-10-10 taps; durable first)
# (also grep -a '[PRB-' over the fresh harvest's UE4SS.log copy)
grep -a "\[FN\] seam refused" <fresh UE4SS.log>   # FK's call-site tag + hex units, if FK landed in time
```

**→ the hop is NAMED.** Decision feeds step 4. (Reading rules at the bottom —
especially grep -a.)

## 4. ROOT decision — routed build or fix-first

```bash
# 4a. read the overnight build receipts; the new routed PRX sha is THERE:
ls -lt $WHAT/outputs/2026-10-08-l26-package-A/ | head
cat $WHAT/outputs/2026-10-08-l26-package-A/FINAL-RECEIPT-VARIANT-A.json | grep -a "sha256\|manifest"
# 4b. manifest verdict must be recorded PASS, e.g. re-run the frozen check:
bash $PREP/harvest/verify_build.sh <new-prx-path> --manifest $PREP/harvest/markers-declared.json
```

- **IF manifest-PASS** (all declared markers present in the PRX): deploy the
  routed build (full prep from step 1 — never partial-patch):

  ```bash
  python3 $PREP/harvest/prep_modded_launch.py --variant A --prx-path <new-prx-path>
  # expects: decrypt-off readback byte-exact (magic 4f153d1d, sha+size match),
  # ploader armed, klog tap running -> prints LAUNCH-READY receipt
  ```

- **ELSE (or the probe names a hop the routed build does NOT cover):**
  fix the named hop FIRST (per the [PRB-*] evidence + FK refusal lines),
  rebuild, re-verify manifest, then deploy. Do not ship an unverified PRX.

- Deploy of the routed build is step-4 work; the ACCEPTANCE launch (step 5)
  still runs the byte-identical ORIGINAL mod.

## 5. USER launch #2 — ACCEPTANCE: byte-identical ORIGINAL mod (MENU+SPAWN)

Restore the mod's ORIGINAL bytes per agent FN's identity/freeze doc (the
mod's console main.lua is archived at
`$PREP/evidence/townfall-main.lua.console-copy`; verify sha/size per the FN
doc before and after upload). Then ONE user launch: load save, in-world,
L3+R3 → menu opens → select an item → SPAWN.

**Acceptance = MENU up AND item spawned** (per ACCEPTANCE-LADDER.md), on the
unmodified original mod bytes, with a clean exit. Harvest:

```bash
python3 $PREP/harvest/harvest.py --label l26-accept-orig-1
grep -a "Spawned pickup\|inventory ID\|Failed at" <fresh UE4SS.log>
```

## 6. The 12-mod ladder (agent FO's schema)

After MENU+SPAWN acceptance, run the 12 staged candidate mods per FO's
ladder schema (candidates staged in `$PREP/multimod-prep/`:
CANDIDATE-INVENTORY.md, CANDIDATE-SHEETS.md, TEST-PLAN.md). Rules:

- ONE launch per mod; user-launched; CLEAN EXIT between mods (game closed
  before the next deploy; ploader re-arm per the Z2 wait above).
- Per-mod deploy via `lua_fast_deploy.sh` where Lua-only; mods needing new
  roots/loadlist rows follow the SIZE-EXACT loadlist law.
- `harvest.py --label <mod-id>-<n>` verdict for each mod, per FO's schema;
  zzdiag retirement per `multimod-prep/ZZDIAG-RETIREMENT.md` timing (keep it
  resident until the ladder schema says to drop it).
- Any mod crash: harvest, note code + WHEN, REBOOT before the next launch.

## READING RULES that bit us (do not relearn)

- **Mixed-encoding logs: `grep -a` ALWAYS** (`evidence/claude2-menu-session-20261010.log`
  interleaves narrow/wide lines; plain grep silently drops binary-looking lines).
- **UE4SS.log on console: `/data/plugins/UE4SS.log`** (harvest.py pulls it;
  `[FN] seam refused` / `[LFB-M]` / `[PRB-*]` markers live there and in klog).
- **Klog taps land under `outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-A/klog-*.log`**
  (legacy: `/tmp/l25-variant-A/klog-*.log`, now durable at the evidence
  runtime dir -- agent-HA 2026-10-10) (klogsrv = live TCP
  3232 stream, no history replay — tap must be running BEFORE the event).
- **FindAllOf real-nil contract:** FindFirstOf/StaticFindObject wrappers are
  NEVER Lua nil (a miss is a non-nil null-wrapper — only `pcall o:IsValid()`
  tests resolution); **FindAllOf returns real nil** on a miss. Do not "fix" a
  nil FindAllOf by nil-checking FindFirstOf.
- **Size-exact loadlist:** every loadlist.txt row's byte-count must match the
  staged file exactly (modforge MFA admission). `lua_fast_deploy.sh` rewrites
  the zzdiag row correctly — never hand-edit the counts.
- Decrypt-off (`-Q "SITE SELF"`) for EVERY executable readback; never upload
  unsigned/modified executables; one launch per test; klog "CRASH KILL_END" =
  the NORMAL quit path; no FindFunction/class-children probes from Lua.

— agent FP (ZCode), 2026-10-10. Commands verified against harvest/ scripts
(lua_fast_deploy.sh, wakeup_prep.sh, prep_modded_launch.py --variant/--prx-path,
verify_build.sh --manifest, harvest.py --label). No console contact was made
writing this file.


## 10. POWER-LOSS variant (console was OFF, not rest) — 2026-10-10 user sequence

The console lost power (fully off). The wake sequence changes:
1. Jailbreak startup: boot the console through the normal jailbreak entry
   (user-driven; hen/kstuff per the usual boot flow). THEN the daemon
   ladder comes up via autoload.txt (kstuff-lite, nanoDNS, ftpsrv, elfldr,
   ps5upload, OnionHEN, klogsrv, ploader — ploader ARMS but exits per its
   cycle; never double-dispatch, wait the Z2 <=120s if resident).
2. Verify daemons (processes_list) — restart klogsrv via
   loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fklogsrv-ps5.elf if dead
   (it died in a sleep cycle before; cold boot usually brings it up).
3. [CLAUDE ROUND-7 NAMING FIX] SPAWN LAUNCH USES ROUND-4. Labels:
   - [ROUND-4 UPDATE] 59b0528b = THE ROUND-4 SPAWN-PROVEN BUILD — the new
     KNOWN-GOOD FALLBACK (deployed, spawn receipted, FR parity proven).
     efbfc656 = the menu-up build (prior fallback, superseded) (better than any unbuilt sha: proven).
     It carries FM's plain-string pushers (a pass there proves a shape we
     do not ship — it is fallback ONLY if round-4 fails its gates).
   - 56a9b1a0 = ROUND-3 (FR revert + FW cleanup), built + 15/15 gated but
     NEVER UPLOADED; superseded by round-4 (same contents, fresh stamp).
   - ROUND-4 = the FR+FW contents lock (see the CONTENTS declaration,
     §12b) — rebuilt fresh, gated, uploaded with decrypt-off readback.
   READBACK LAW: an old file surviving one power event does NOT prove
   uploads can't roll back — decrypt-off readback after EVERY power event,
   no exceptions. The round-4 PRX is built+gated when AC power returns
   (contents LOCKED: round-3 tree + FR parity revert + FW routed
   sites/literal markers; MODBRIDGE_RELEASE_DIAG_OFF stays OFF; FX
   patches stay OUT). DECRYPT-OFF READBACK of the ROUND-4 sha before
   arming (curl -Q "SITE SELF", magic 4f153d1d + sha256 match).
4. ONE launch: round-4 PRX + zzdiag-v11 probe (deploy via lua_fast_deploy
   after prep). Watch: [BUILD] stamp (new build id, snapshot != e8c4bb62),
   [LFB-M] fname-rev ... ok=1, [PRB-1..6], and the mod's "configure
   pickup" step. THEN if the UE4SS self-test (agent GB) made it into the
   build window, press its combo to run the parity self-test in the same
   session. FY equivalence proof: build the same snapshot with the old
   serial driver (/tmp/build_variants.py.pre-FY.bak -- legacy, now durable at
evidence/tmp-preserved-20261010/tmp/build_variants.py.pre-FY.bak, sha
815127c2...) DURING the console
   launch window; PRX shas must match, else stop using concurrent builds
   until explained.
5. Parity note: FM's plain-string property reads are being REVERTED to
   upstream userdata returns (agent FR, offline round-4) — the deployed
   round-3 PRX 56a9b1a0 still has plain-string reads; if a ladder mod
   needs userdata parity, wait for the round-4 build before that mod.
6. Deploy gate is AUTOMATIC now (Claude round-8 item 6, the -6 lesson):
   prep_modded_launch.py runs harvest/prelaunch_rowcheck.py after uploads
   (STEP 5.5, before arming and LAUNCH-READY) — every loadlist `file` row
   is verified present on the console at its EXACT declared size with the
   game closed; any missing/mismatched row aborts prep with the exact row
   and the fix (upload the missing rows or fix the loadlist), so the -6
   whole-module-refusal class can no longer cost a relaunch.


## 11. ROUND-6 changes (Claude review of the battery round)

- FX patches: build them BACK-TO-BACK on the Mac as separate labelled builds
  (P1 fault-ledger first, then P2 variant-B removal), each fully gated
  (snapshot/build/package/manifest per build). Console-test the FINAL build
  in the ladder v2 launch — no separate launches. If ladder v2 regresses,
  bisect with the intermediate builds. Build order on AC power:
  round-4 -> equivalence-serial -> P1-build -> P2-build.
- Stellar Blade dump: FIRST console session after the Townfall launch
  (user's own copy). Decrypted copy stays on the Mac, NEVER uploaded.
  Once on the Mac: profile-generator dry run (GJ's run-blade-profile.py)
  + container format survey. Fixes stay engine/version-level only.
- While power is out: pak track (capabilities #7/#8) Mac-only work is live
  (agents GK/GL/GM/GN): generalized overlay builder, no-game-file-change
  runtime mount design, Leon glasses cause + generic PC-shader fallback,
  LogicMods/BPModLoader through the harness + pak-side plan.
  Everything reported Mac-only until console-proven.


## 12. AC-power build staging order (round-4 scope lock + round-5/6 content)

Appended by agent GR, 2026-10-10 (docs only; ROOT owns execution). Law:
Claude locked **round-4 = FR + FW only** (§10.3). GA's path shim is ROUND-5
content and must be EXCLUDED from every build through the round-4 gate and
the serial-equivalence proof, then restored. Each build below is a fully
separate, fully gated cycle: snapshot -> build -> package -> manifest
(`verify_build.sh --manifest harvest/markers-declared.json`) -> archive the
labelled outputs (bisect depends on every intermediate surviving).

Absolute paths (constants from §0 of this runbook):

```
RT=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime
SHIM_SRC=$RT/typescan-r4/candidate/src/LuaPathShim.cpp        # 851 lines
SHIM_HPP=$RT/typescan-r4/candidate/include/LuaPathShim.hpp    # 91 lines
DRIVER=$RT/build_variants.py
MARKERS=$PREP/harvest/markers-declared.json                   # 18 markers incl. 3 [SHIM]
CHANGES=$RT/typescan-r4/CHANGES.json
```

### Step 0 — SHIM SET-ASIDE (before anything builds; root-owned)

Move aside / revert the following FIVE surfaces (the four mandated by the
round-4 lock + the CHANGES.json reconciliation GR verified on disk; keep all
set-aside copies in one dated staging dir so step 3 is an exact inverse):

1. `SHIM_SRC` — `typescan-r4/candidate/src/LuaPathShim.cpp` (whole file).
2. `SHIM_HPP` — `typescan-r4/candidate/include/LuaPathShim.hpp` (whole file;
   also named in CHANGES.json `added_files` + `added_header_hashes`).
3. The **LuaMod.cpp insertion block**: `typescan-r4/candidate/src/LuaMod.cpp`
   lines **7842-7862** — the single `#if defined(__PROSPERO__)` … `#endif`
   `[SHIM]` block inside `prepare_mod` (post Rule-10-fix shape). Remove the
   whole block; nothing else in LuaMod.cpp is shim content.
   Post-check: `wc -l LuaMod.cpp` must return **9509** (the pre-GA FR/FW-era
   shape).
4. The **`lua-pathshim` NEW_UNITS row**: `$DRIVER` line ~112,
   `'lua-pathshim':{'source':…,'template':'luamod','note':'agent GA io/path
   virtual-cwd shim (whole-TU __PROSPERO__)'},` — delete the entry, leave
   `lua-lfb` / `buildstamp` / `tcselftest` (round-3/4 content) in place.
   The census self-adjusts (`391+len(NEW_UNITS)`).
5. The **3 [SHIM] manifest markers**: `$MARKERS` entries with ids
   `shim-installed` (`[SHIM] path-resolver installed`), `shim-io-open`
   (`[SHIM] io-open resolved=`), `shim-write-rejected`
   (`[SHIM] write-rejected`) — remove; 15 markers remain (FW's literal-strict
   set).
6. CHANGES.json reconciliation (same motion): remove the `round5_row_sync`
   block and the `LuaPathShim.hpp` entries in `added_files` /
   `added_header_hashes`; refresh the `candidate_sha256` row for LuaMod.cpp
   to the 9509-line set-aside shape. Without this the G2 candidate-hash gates
   fail the round-4 build. Valid-JSON check after every edit.

Pre-build checks: `python3 -m py_compile $DRIVER`; `python3 -c "import
json;json.load(open('$MARKERS'));json.load(open('$CHANGES'))"`;
`grep -rn "LuaPathShim\|lua-pathshim" $RT/typescan-r4/candidate/src
$RT/typescan-r4/candidate/include $DRIVER` must return ZERO hits (no shim
reference anywhere outside the staging dir).

### Steps 1-2 — round-4 build + serial-equivalence proof (§10/§11 laws)

1. **Round-4 snapshot/build/package/gate** on the set-aside tree (contents
   LOCKED: round-3 tree + FR + FW; `MODBRIDGE_RELEASE_DIAG_OFF` stays OFF;
   FX patches stay OUT). Archive PRX + all receipts; record the sha.
   Deploy law: decrypt-off readback (magic `4f153d1d` + sha match) before
   arming; one launch; spawn test on round-4 (NOT round-3 `56a9b1a0`;
   round-3 stays the fallback).
2. **Serial-equivalence build**: DURING the console launch window, build the
   same snapshot with the pre-FY serial driver
   (`/tmp/build_variants.py.pre-FY.bak` -- legacy, now durable at
evidence/tmp-preserved-20261010/tmp/build_variants.py.pre-FY.bak). PRX shas must MATCH the step-1
   build; any mismatch => stop using concurrent builds until explained
   (FY law).

### Steps 3-6 — restore, then the round-5/6 labelled builds

3. **Restore the shim** (exact inverse of step 0, from the staging dir):
   LuaPathShim.cpp/.hpp back; LuaMod.cpp block back (post-check: 9530
   lines); `lua-pathshim` NEW_UNITS row back; 3 [SHIM] markers back (18
   total); CHANGES.json `round5_row_sync` + hpp rows back with refreshed
   hashes. Re-run the step-0 pre-build checks in their positive form.
4. **P1 fault-ledger labelled build**: apply `parked/P1-fault-ledger/
   01-fault-ledger-desaturation.patch` (+ its MANIFEST/driver wiring per
   P1 NOTES), full gate cycle, archive as P1. Never apply without the NOTES'
   flags honored; `--plan` before `--execute`.
5. **P2 variant-B labelled build**: apply the P2 patch set (01 driver, 02
   tree, 03 packaging driver; P1 stays in), full gate cycle, archive as P2.
6. **Round-5 build** (shim + FR/FW + P1/P2 + any further landed round-5
   content root admits by then) — full gate cycle. THIS build (the final
   gated one) is what the ladder-v2 window console-tests: GB self-test combo
   (L1+R1, needs the 1736 B `ueselftest/PS5Pad-round4-proposed.ini` + MFA
   row re-pin) + the four ladder-v2 mods + resident spawner; one launch +
   up to 2 restarts per LADDER-V2. If ladder v2 regresses, bisect with the
   archived step-1/2/4/5 builds (no separate launches).

### Step 6a — symbols sidecar in every package pass (agent HH, 2026-10-10)

`$RT/l26_packaging_driver.py` now emits the PRX symbols sidecar at
final-receipt time (PHASEA-PLANS #5 BUILD GAP closure; consumer contract =
`phasea-tools/symbolize_crash.py --symbols-sidecar`). Applies to EVERY
build above — steps 1, 2, 4, 5 and 6 — automatically, because the emission
is inside `finalize_variant`:

- Writes `<pkg>/<prx-name>.symbols.json` beside the packaged PRX:
  `prx_sha256` = the packaged pass-1 PRX's actual sha256 (the pairing key
  symbolize_crash hard-refuses on mismatch), `build_id` from the bare PRX's
  `.note.gnu.build-id`, exact `.dynsym` anchors in `symbols[]`
  (DYNSYM-EXACT), and the `-r` runtime.map reconstruction embedded under
  `map_reconstructed[]` (tier MAP-RECONSTRUCTED — kept OUT of `symbols[]`
  so the consumer never labels order-assumed rows SIDECAR-EXACT).
- New gates fail the build like any other: **P12** sidecar present + valid
  JSON + required keys + `prx_sha256` == packaged pass-1 PRX; **P13**
  `prx_sha256` matches BOTH packaging twins.
- Post-package check (Mac-only, seconds): import the driver and run
  `validate_symbols_sidecar(<sidecar>, [<pass-1 prx>, <pass-2 prx>])`, or
  round-trip `phasea-tools/symbolize_crash.py`'s loader
  `load_prx_symbols_sidecar(sidecar, prx)` — must accept, and must
  hard-refuse a tampered copy. Reference receipts:
  `outputs/2026-10-08-l25-prep/agent-HH-packer-tests/SIDECAR-EMIT-TEST-RECEIPT.json`
  (20/20). No console contact; crash-symbolization use stays per PHASEA #5
  (opportunistic, never crash-inducing).

### Step 6b — layout-table provenance whitelist before ship (agent HH, 2026-10-10)

Rule-10 gate for anything that ships the blade-prep per-version layout
table (GI's audit found the blind-test game name in `generated_by`):

- Run `python3 outputs/2026-10-08-l25-prep/harvest/pack_layout_tables.py`
  (read-only on the authoring table). It writes
  `blade-prep/shippable/per-version-table.shippable.json` +
  `.shippable.cells.json` + `per-version-table.provenance-diff.txt` +
  `PACK-LAYOUT-TABLES-RECEIPT.json`, replacing every known game name
  (Townfall family / Stellar Blade / Astro Bot / Silent Hill) with role
  tokens (MEASURED-TITLE / BLIND-TEST-TITLE / NEGATIVE-CONTROL-TITLE), and
  fail-closes (exit 1) if any known name survives in the outputs.
- Already run on the real table 2026-10-10: PASS, 9 occurrences in 7
  fields neutralized, independent grep of the shippable outputs = zero
  hits. Ship ONLY the `shippable/` files, never the authoring original.
  Reference receipts:
  `outputs/2026-10-08-l25-prep/agent-HH-packer-tests/PACK-LAYOUT-WHITELIST-TEST-RECEIPT.json`
  (17/17) and `blade-prep/shippable/PACK-LAYOUT-TABLES-RECEIPT.json`.

Standing laws unchanged: FX patches never enter round-4; every executable
upload gets decrypt-off readback; one launch per test; never replay failed/
reserved jobs; `MODBRIDGE_RELEASE_DIAG_OFF` OFF unless a release build is
explicitly ordered.
