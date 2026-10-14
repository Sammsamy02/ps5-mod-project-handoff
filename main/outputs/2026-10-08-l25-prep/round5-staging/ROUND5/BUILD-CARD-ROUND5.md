# ROUND-5 BUILD CARD — the AC-power labelled build (agent II staging, 2026-10-11)

ROUND-5 = the full restore + additions on top of the **P1+P2+world-fix
stack** (WAKE-RUNBOOK-20261010.md section 12 order law: serial-equivalence
proof -> P1 fault-ledger -> P2 variant-B -> ROUND-5; every step a separate
fully-gated cycle, every intermediate archived for bisect).

All artifacts referenced live under
`outputs/2026-10-08-l25-prep/round5-staging/ROUND5/` (absolute prefix
`/WORKSPACE/.codex/worktrees/translation-core/what/`).

---

## 0. PREREQUISITES — what must land BEFORE this card executes

| # | Prerequisite | Artifact path (absolute prefix `.../what/outputs/2026-10-08-l25-prep/`) | Status at staging (2026-10-11 ~09:30Z) |
|---|---|---|---|
| P0 | AC-queue predecessors done: serial-equivalence proof, P1 build, P2 build (order law; round-4 59b0528b already built+spawn-proven) | receipts under `../2026-10-08-l26-build-variants/` + `../2026-10-08-l26-package-A/` per WAKE-RUNBOOK section 12 steps 1-2, 4-5 | queued at AC power |
| P1 | **agent IA/IK world-fix — LANDED 2026-10-11**: `round5-staging/world-fix/` = `01-diagnosis.md` + `02-core-fix.patch` (native GetName binding) + `02b-getworld-fallback.patch` (IK: __PROSPERO__ GetWorld outer-chain fallback, AActor semantics) + `03-map-logger.patch` ([FL-MAP] logger) + `APPLY-ORDER-AND-PROBE-EXPECTATIONS.md` (apply order 02 -> 02b -> 03, syntax-receipted) | `round5-staging/world-fix/` | LANDED |
| P2 | **agent IB writable-area shim**: the stash edit itself LANDED 2026-10-11 09:14 (`round4-setaside/LuaPathShim.cpp` sha `2f6d2e6d6c31e74093231367b12a7d54a01926e2fa2ea5ff87380fec255e3896`, 1655 ln; `.hpp` sha `0f5a043d741d828355220eb8eaa5bae3304c701f1a2b27fa36c6e35f15cea5be`, 121 ln; `[SHIM] write-area ` marker at :1547) — REQUIRED: IB's report (marker budget + host-test results) so the write-area entry's budget note is confirmed | `round4-setaside/LuaPathShim.{cpp,hpp}` + IB's report (pending) | STASH EDIT LANDED; REPORT PENDING |
| P3 | **agent IC zzdiag/ueselftest updates**: Lua-side only, NOT build content — no PRX dependency; must be deployed (lua_fast_deploy + rowcheck gate) for the launch-2 window AFTER the build | `zzdiag-v11/`, `ueselftest/`, `agent-IC-st2-zzdiag.md` (LANDED 09:09) | LANDED (deploy-side) |
| P4 | The frozen fallback is intact: round-4 PRX `59b0528b77ca00c8f717a2fd9a3b8d0e838961aadf111933de31d27060a4af61` file + receipts (FROZEN-KNOWN-GOOD-20261010.md section "ROUND-4 FREEZE") | `../2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx` | VERIFIED |

Card is EXECUTABLE with P1 missing ONLY via `apply-additions-round5.sh
--allow-pending` (the [FL-MAP] sockets stay open and the manifest verify
runs at 27 markers instead of 27+N) — ROOT must explicitly acknowledge; the
preferred state is all prerequisites landed.

## 1. THE RESTORE (idempotent, --verify-capable)

```bash
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
R5=$WHAT/outputs/2026-10-08-l25-prep/round5-staging/ROUND5
bash $R5/restore-round5.sh            # S1-S5, skips completed steps
bash $R5/restore-round5.sh --verify   # must exit 0: every sha/hole checked
```

Restores (per ROUND4-CONTENTS-DECLARATION.md):
- S1 files (COPY from `round4-setaside/`, stash preserved):
  `LuaPathShim.{cpp,hpp}` -> candidate `{src,include}/`; `FirstLaunchProfile.{cpp,hpp}` -> candidate `{src,include}/`.
  **KernelBridgeProbes.{cpp,hpp} STAYS SET ASIDE** (Claude-reviewed gated build only; the restore refuses if it finds them in the tree).
- S2 the 21-line LuaMod.cpp [SHIM] block, byte-exact from
  `round4-setaside/LuaMod-insertion-block.txt`, after the unique anchor
  `            setup_lua_require_paths(lua);` in prepare_mod.
  Post-shape: **9530 lines**; restored sha `9445f77bb132...` (round-4 set-aside
  shape: 9509 lines, `aac03fbc...`). Conceptual-compile check: the block's
  3-arg extern matches `RC::LuaPathShim_install(lua_State*, const char*, const char*)`
  (verified against the POST-IB stash, definition now at :1637).
- S3 NEW_UNITS rows (exact text, inserted after the tcselftest row):
  `'lua-pathshim':{...template:'luamod'...}` (verbatim per WAKE-RUNBOOK section 12 step 0.4)
  + `'firstlaunch':{...template:'unreal-unrealinitializer'...}` (reconstructed to GT section 3's registration spec — the original row text was not preserved on disk; noted honestly).
  Census becomes **391+5 = 396**.
- S4 the 3 [SHIM] manifest entries (shim-installed / shim-io-open /
  shim-write-rejected, utf-16le, literals re-verified against the post-IB
  stash at :1623/:753/:604) -> markers-declared.json = **18**.
- S5 CHANGES.json: LuaMod.cpp candidate_sha256 refreshed in BOTH `changes[]`
  and `files[]` rows; `LuaPathShim.hpp` + `FirstLaunchProfile.hpp` re-added
  to `added_headers` + `added_header_hashes` (post-IB shas);
  `round5_row_sync` updated; `round4_setaside: false` + `round5_restored: true`.
  Valid-JSON gate after the edit.

## 2. THE ADDITIONS

```bash
bash $R5/additions/apply-additions-round5.sh            # refuses if restore not verified
bash $R5/additions/apply-additions-round5.sh --verify   # must exit 0
```

- (a) **GT first-launch wiring** — `additions/fl-wiring-unrealinitializer.patch`
  applies to `typescan-r4/candidate/src/UnrealInitializer.cpp`: include
  `<UE4SSProgram.hpp>` + `<filesystem>`; the [FL] block inserted BEFORE
  `SeedArming::consume_required_slot_seeds(...)` (line ~1682 round-4 shape)
  calling `RC_first_launch_try_profile(derived-root, SeedArming::module_view_base())`
  with the GA Rule-10 root derivation. Grounding verified: get_program() is a
  live inline static at that point (program constructed + mods dirs pinned
  before ScanGame, UE4SSProgram.cpp:429-431 vs :1346; throws, never UB).
  CHANGES UnrealInitializer.cpp rows auto-refreshed. **Proof it applies:
  dry-run + apply + ordering (try@1703 < consume@1706) exercised 2026-10-11.**
- (b) **Marker additions** — `additions/markers-add-round5.json`:
  `shim-write-area` (IB, `[SHIM] write-area `, utf-16le, :1547) + 8 [FL]
  entries (narrow utf-8 literals verified against FirstLaunchProfile.cpp:
  profile slot=, profile loaded=, detect verdict=, table row=,
  verify request, pin, commit outcome=, FAIL-CLOSED reason=).
  Manifest = **27** here. Every literal is grepped in the tree sources before
  the manifest is touched (FW phantom-build law).
- (c) **IA/IK world-fix** — applies, in the syntax-receipted order
  (world-fix/APPLY-ORDER-AND-PROBE-EXPECTATIONS.md): `02-core-fix.patch` ->
  `02b-getworld-fallback.patch` -> `03-map-logger.patch` (all three are
  CANDIDATE-relative patches, applied with cwd = the candidate tree);
  refreshes ONLY the CHANGES rows of files whose sha actually changed.
  **Proven in the /tmp validation: all three applied cleanly on top of the
  restored tree; rows refreshed for LuaMod.cpp + LuaUObject.hpp; the driver's
  G12b header-consumer cascade then verified LuaUObject.hpp -> 33 consumers
  recompiled, gaps=[]; G2t kept all four listener units fresh.**
- (d) **[FL-MAP] markers — RESOLVED from the landed 03-map-logger.patch**
  (three contiguous wide literals, utf-16le): `fl-map-detour-pre`
  (`[FL-MAP] order={} map='{}' phase=pre src=detour`), `fl-map-detour-loaded`
  (`... phase=loaded src=detour url='{}'`), `fl-map-marshal-watch`
  (`... phase=loaded src=marshal-watch` — the R3-world delivery source).

**Expected final marker count: 30** (15 round-4 + 3 [SHIM] restore + 1 IB
write-area + 8 [FL] + 3 [FL-MAP]). **Proven: the /tmp validation's manifest
reached exactly 30 with every literal grepped in-tree.**

## 3. PLAN-MODE VALIDATION — ALREADY RUN, GREEN (2026-10-11, /tmp mirror)

`bash $R5/plan-validate-round5.sh` rebuilds the whole restored+additions
state under `/tmp/r5-plan` (mirror of the drivers + candidate tree + CHANGES
parts with candidate-side paths repointed; BASE receipts verbatim so the
18k-pin sweep re-proves the LIVE tree read-only), applies ALL FOUR patches
(wiring + 02 + 02b + 03), and runs the REAL driver `--plan`. Latest run
(exit 0, /tmp/r5-final4.log):

- **G6-link-input-census planned_inputs = 396** (391 + 5 NEW_UNITS) x4 passes
- **G2-part-change-pins: 42 rows, 0 not-ok** (restored CHANGES rows consistent
  with the fully-patched tree: LuaMod.cpp 9719 lines = 9509 + 21 restore
  block + 189 map-logger; wired UnrealInitializer.cpp; patched LuaUObject.hpp)
- **G1-base-selection-pins ok=True, 19044 pins, drift=0** (live BASE intact)
- **G2q second-hop: Q's copyin-probe patch REBASED cleanly onto the
  wired UnrealInitializer.cpp** (`patch-rebased-onto-revised-base`,
  composed_has_token=true, G2q-second-hop-override ok=True)
- **G12b: LuaUObject.hpp -> 33 consumers recompiled, gaps=[]** (the world-fix
  header cascade is covered); G2t: all four listener units fresh
- G2r/G2s/G4/G4-ctoken/G5/G12 all True; G-stamp plan-clean; G7/G9/G10
  PLANNED (execute-enforced); G-base-dep x3 SKIP (documented .d drift skips)
- **Manifest = 30 markers**, every required literal grepped in-tree
- 1 accepted ok=false: `G2q-copyin-probe-pin-unrealinitializer.base` — an
  EXPECTATION pin by the driver's own comment; accepted IFF the rebase
  succeeded with the token present (it did)
- Idempotency proven: re-running restore on a standing mirror = 8 SKIPs,
  verify exit 0

Live-tree `--plan` at AC power should reproduce exactly this journal (same
gates; paths live instead of /tmp).

## 4. THE AC COMMAND SEQUENCE (ROOT, at AC power)

```bash
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
PREP=$WHAT/outputs/2026-10-08-l25-prep
RT=$WHAT/outputs/2026-10-08-l26-runtime
R5=$PREP/round5-staging/ROUND5

# 4.1 RESTORE + ADDITIONS (sections 1-2 above), then:
bash $R5/restore-round5.sh --verify                 # MUST exit 0
bash $R5/additions/apply-additions-round5.sh        # wiring + 02/02b/03 + markers
bash $R5/additions/apply-additions-round5.sh --verify  # MUST exit 0

# 4.2 pre-build checks (section 12 step 0 law, positive form):
python3 -m py_compile $RT/build_variants.py
python3 -c "import json;json.load(open('$PREP/harvest/markers-declared.json'));json.load(open('$RT/typescan-r4/CHANGES.json'))"
grep -rn "LuaPathShim\|lua-pathshim" $RT/typescan-r4/candidate/src $RT/typescan-r4/candidate/include $RT/build_variants.py | wc -l   # MUST be > 0 (restored); zero = restore silently absent
# (one -fsyntax-only of the WIRED UnrealInitializer.cpp is recommended before --execute:
#  GT's TU itself passed prospero -fsyntax-only 2026-10-10; the wiring block is
#  the only new code in a proven TU — see residuals)

# 4.3 SNAPSHOT (fresh; never reuse a prior SNAPSHOT-ID for a changed tree):
bash $PREP/harvest/snapshot_tree.sh     # -> SNAPSHOT-ID; export it:
export BRIDGE_SNAPSHOT_ID=<new-SNAPSHOT-ID>

# 4.4 PLAN then EXECUTE:
python3 $RT/build_variants.py --plan    # journal must match section 3's greens
python3 $RT/build_variants.py --execute

# 4.5 PACKAGE (twins per variant; P12/P13 sidecar gates are inside finalize):
python3 $RT/l26_packaging_driver.py --prepare --execute

# 4.6 MANIFEST VERIFY at the FINAL count (30 expected):
python3 $PREP/harvest/verify_build.sh <built.prx> --manifest $PREP/harvest/markers-declared.json
#    expect "30/30" style all-present — any MISS = do NOT upload (FG phantom-build law)

# 4.7 P12/P13 SIDECAR CHECK (HH law, seconds, Mac-only):
python3 - <<'PY'
import importlib.util
spec = importlib.util.spec_from_file_location('pk', '<RT>/l26_packaging_driver.py')
pk = importlib.util.module_from_spec(spec); spec.loader.exec_module(pk)
ok = pk.validate_symbols_sidecar('<pkg>/<prx>.symbols.json', ['<pass1.prx>', '<pass2.prx>'])
print('P12/P13', 'PASS' if ok else 'FAIL')
PY

# 4.8 ARCHIVE + LABEL the round-5 outputs (bisect depends on it);
#     deploy is NOT part of this card — it rides the ladder-v2 launch window
#     (WAKE-RUNBOOK section 12 step 6): prep_modded_launch.py + decrypt-off
#     readback (magic 4f153d1d + sha match) + ID's rowcheck gate + the
#     self-test/ladder rung per LADDER-V2.
```

## 5. ROLLBACK (re-set-aside — exact inverse; stash always survived)

```bash
# tree side (order matters for the patch stack):
#   1) reverse the world-fix + wiring patches (cwd = candidate tree):
#      for p in 03-map-logger 02b-getworld-fallback 02-core-fix; do
#        patch -R -p1 --silent < $PREP/round5-staging/world-fix/$p.patch; done
#      patch -R -p1 --silent -d $WHAT < $R5/additions/fl-wiring-unrealinitializer.patch
#   2) rm $RT/typescan-r4/candidate/src/LuaPathShim.cpp $RT/typescan-r4/candidate/include/LuaPathShim.hpp \
#        $RT/typescan-r4/candidate/src/FirstLaunchProfile.cpp $RT/typescan-r4/candidate/include/FirstLaunchProfile.hpp
#      (KernelBridgeProbes was never restored — nothing to remove)
#   3) LuaMod.cpp: delete lines 7842-7862 (the 21-line block); post-check 9509 lines, sha aac03fbc5897...
#   4) driver: delete the lua-pathshim + firstlaunch NEW_UNITS rows (census back to 394)
#   5) manifest: remove the added entries -> back to 15 (round-4) — or keep the
#      additions for the NEXT round-5 attempt and only revert the build dirs
#   6) CHANGES: restore LuaMod.cpp rows to aac03fbc..., UnrealInitializer.cpp rows
#      to 6ed7a208... (pre-wiring), LuaUObject.hpp row to 560eaf6d... (pre-02),
#      remove the added_headers/hashes entries, round4_setaside: true
#   then: bash $R5/restore-round5.sh --verify  # must FAIL (holes = set-aside state restored)

# console side (if the round-5 PRX was already uploaded and regressed):
# redeploy the FROZEN round-4 FILE byte-exact (FROZEN-KNOWN-GOOD section 6a):
#   59b0528b77ca00c8f717a2fd9a3b8d0e838961aadf111933de31d27060a4af61
#   decrypt-off readback law; one launch; never an unbuilt sha.
```

## 6. HONEST RESIDUALS (do not paper over)

1. **No compile of the wired/restored code happened** (battery law). Evidence
   chain: GT's TU passed prospero `-fsyntax-only` (2026-10-10); GA's
   LuaPathShim + LuaMod insertion passed the same (pre-IB); IB's upgraded
   stash has NO syntax check on record yet (report pending); the wiring block
   is the only new code in a TU that compiles today. The driver's own compile
   happens at `--execute`; a pre-execute `-fsyntax-only` of the wired
   UnrealInitializer.cpp + IB's upgraded LuaPathShim.cpp is recommended and
   lawful (single-file, no build).
2. **The firstlaunch NEW_UNITS row text is reconstructed** to GT's spec (the
   original row was deleted at set-aside and is not preserved anywhere on
   disk — no snapshot or receipt captured it). Functionally equivalent;
   noted for provenance honesty.
3. **[FL-MAP] sockets RESOLVED** (2026-10-11): IA/IK's 02/02b/03 patches +
   the apply-order doc landed; the three utf-16le entries are pinned from the
   landed diff text and grep-verified in the /tmp-validated tree.
4. **The FL wiring is the call-site only**: the PendingVerify probe/report/
   commit loop and the pin-registry retarget of consume_required_slot_seeds
   are follow-on rungs (GT section 3) — round-5 ships the ladder + markers,
   not the full retarget.
5. Plan-mode G7/G9/G10 are PLANNED (execute-enforced) — the twins/import-
   delta/define-confinement proofs only exist after the real build.
6. The `/tmp` plan validation re-proved the LIVE BASE pins read-only
   (G1: 19044 pins, 0 drift) — the live round-4 tree is intact at staging
   time; the live tree itself was NOT modified by anything in this staging
   (battery law; `--verify` runs only).
