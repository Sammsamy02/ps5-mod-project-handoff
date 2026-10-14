# FROZEN KNOWN-GOOD — PRX efbfc656 (menu-up build)

Agent FN freeze record, 2026-10-10 (written while the console is in REST MODE —
document/verification prep only, no console contact, no builds, no tree edits).
This is the freeze record for the FIRST build on which the unmodified
TownfallItemSpawner mod MENU OPENED on PS5. Companion acceptance-proof prep:
`ORIGINAL-MOD-IDENTITY.md` (same directory). Claude's directives this freeze
serves: freeze a known-good build; rerun with the byte-identical original mod
as the acceptance proof; then start the 12-mod generality ladder.

All paths are relative to `/WORKSPACE/.codex/worktrees/translation-core/what/`
unless absolute.

---

## 1. Frozen identity (verified by agent FN against the files themselves)

| Field | Value |
|---|---|
| Artifact | `outputs/2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx` |
| sha256 | `efbfc656432fcf349afa2fdc70c115773c1e21c92cddfeb57abbb36cf90949f8` |
| Size | 31,419,959 B |
| Magic (first 4 B) | `4f153d1d` — fake-signed SELF (unencrypted make_fself wrap; the embedded ELF stays plain `7f454c46`) |
| Variant | l26-A production (engine-side delete-listener registration ON, `-DMODBRIDGE_L25_ENGINE_ADD_LISTENER`; B-skip OFF) |
| BUILD_ID | `l26-20261010T043709Z` |
| Snapshot id | `24025929` (frozen tree tar, §3) |

Receipt citations (fields read directly, not trusted from memory):

- `outputs/2026-10-08-l26-package-A/FINAL-RECEIPT-VARIANT-A.json`:
  `prx.sha256` = efbfc656…49f8, `prx.bytes` = 31419959;
  `twin.sha256` = efbfc656…49f8 (pass-2 identical, same size);
  `status` = `MAC_ONLY_L26_VARIANT_A_TWINS_AND_PACKAGE_CHECKS_PASS`;
  `static_checks_each_pass` = 20; `exact_import_count` = 207;
  `constructor_arrays` = { .init_array: 238, .fini_array: 2 };
  `runtime_object_sha256` = dbf6e6aec5e52d551e9fc46aedb2f192bb80acf5cbf9961bf03a24a7e472ce3c
  (the l26-A runtime object both packaging passes consumed).
- `outputs/2026-10-08-l26-package-A/pass-1/STATIC-CHECKS.json`:
  `status` = `STATIC_CHECKS_PASSED` (all 20 checks), including
  `elf_magic_bare` = 7f454c46, `fself_magic_wrapped` = 4f153d1d,
  `wrapped_embedded_elf_header_equal` (12 segments),
  `fself_payload_digest_matches_bare`,
  `imports_equal_extended_contract` (count 207 == expected 207),
  `init_fini_slots` = { init_slots: 239, fini_slots: 2 }.
- `outputs/2026-10-08-l26-package-A/pass-1/BUILD-RECEIPT.json`:
  `status` = `MAC_PACKAGE_CHECKS_PASS`, `gaps` = [] (0), 21 commands, 41 log rows,
  `inputs_sha256` = 3f02e7161e0af56ca1c16b07f9742729fd99c0b047c1df30d29db8273fe7ba0d,
  `source_provenance_verified_by_new_manifest` = true,
  `console_contact` = false / `native_execution` = false (Mac-only build).
- Agent FN re-verified tonight, read-only: `shasum -a 256` of pass-1 AND pass-2
  PRX = efbfc656…49f8, both 31,419,959 B, first 8 bytes `4f15 3d1d 0001 0112`.

Independent marker recheck (agent FN, read-only grep of the frozen PRX against
`outputs/2026-10-08-l25-prep/harvest/markers-declared.json`, both UTF-8 and
UTF-16LE encodings, `either_encoding` entries honored): **13/13 PASS** —
`[LUA-FN] in='` (utf-8 x1), `[LFB] lua-fname-bridge composed` (utf-16le x1),
`[FFO] query-fallback` (utf-16le x1), `[FFO2]` (utf-16le x2), `[LFB-M]`
(utf-8 x2), `[BUILD]` (utf-8 x1 + utf-16le x1), `[ST] wcslen-16units`
(utf-8 x2), `[ST] wstring-rt` (utf-8 x2), `[ST] lua-` (utf-16le x3) +
`ffo PlayerController` (utf-8 x1), `[FN] tostring` (utf-16le x1),
`[FN] pool-decoder verified` (utf-16le x1), `[Seeds][L26] engine delete-listener
ops admitted` (utf-16le x1 + utf-8 x1), `slot 2 FName ctor bound` (utf-16le x1).
This independently re-proves the "13/13 manifest markers verified in the PRX
before upload" claim recorded in CLAUDE-MENU-UP-SPAWN-BLOCKED.md §intro/§7.2.

Twin-variant sibling (context, NOT part of this freeze): variant B
`outputs/2026-10-08-l26-package-B/pass-1/modbridge_supplier_composition.prx`
sha256 `aa29c765dbd9de60a98391850c47d549dd3502b1124e8af33d834e39d44ee852`
(31,395,175 B, both packaging passes identical) — marked UNSAFE FOR LONG-TERM
USE (delete-listener registration skipped) in VARIANT-PAIR-RECEIPT.json /
VARIANT-PAIR-PACKAGING-RECEIPT.json. Diagnostic A/B only; never promote.

## 2. Build provenance and stamp reconciliation

### 2.1 Frozen snapshot (the tree the build consumed)

- Tar: `outputs/2026-10-08-l26-runtime/snapshots/snapshot-20261010T042420Z.tar.gz`
  — sha256 `240259295b813319898e0b2d186a219b80fd8388b19c3bf2a6a1b23306113000`,
  364,425 B.
- Sidecar `snapshot-20261010T042420Z.sha256` contains a bare-filename
  `shasum -a 256 -c` line — **run it from the snapshots directory**:
  ```
  cd outputs/2026-10-08-l26-runtime/snapshots
  shasum -a 256 -c snapshot-20261010T042420Z.sha256
  # -> snapshot-20261010T042420Z.tar.gz: OK   (verified by agent FN tonight)
  ```
- Manifest `snapshot-20261010T042420Z.manifest.json`: schema `tree-snapshot/1`,
  `created_utc` 2026-10-10T04:24:21Z, `snapshot_id` **24025929** ("first 8 hex
  of tar sha256"), `file_count` 50 with per-file sha256s, `console_contact`
  false, `consumer`: "BRIDGE_SNAPSHOT_ID env var for build_variants.py
  --execute (stamped into candidate/include/GeneratedBuildStamp.hpp)".
  Frozen components: the typescan-r4 candidate tree, the three CHANGES.json
  part-files (profile, typescan-r4, thread), the three drivers
  (build_variants.py, build_runtime.py, l26_packaging_driver.py) and the
  verifier stack (verify_build.sh, grep_markers.py, markers-declared.json,
  snapshot_tree.sh).

### 2.2 The BUILD_ID and the failed-attempt reconciliation

- The stamped header:
  `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include/GeneratedBuildStamp.hpp`
  — sha256 `fe9a21bf0b32eedde3e4bddbf7366d0d56d7ab6731046f2137f1eabc52a706e2`,
  1,487 B — defines `BRIDGE_BUILD_ID "l26-20261010T043709Z"`,
  `BRIDGE_SNAPSHOT_ID "24025929"`, 12 change ids. It is pinned with exactly
  this build_id in `outputs/2026-10-08-l26-package-A/BUILD-INPUTS.json` and in
  BOTH `pass-1/INPUT-SELECTION.json` and `pass-2/INPUT-SELECTION.json`, and in
  `outputs/2026-10-08-l26-build-variants/EXECUTION-RECEIPT.json`
  (`build_stamp.generated_at_utc` = 2026-10-10T04:37:09Z, driver sha
  815127c2e24eb3893d8d8c87eb59db0caf131187e2d3d3c8cc037bd3521c5ab9).
- **Which stamp the console log shows**: the live session opened with
  `[BUILD] id=l26-20261010T043709Z … snapshot=24025929` (session log line 23,
  §4 below). The console ran the 043709Z build — the executed stamp.
- **The failed attempt**: `outputs/2026-10-08-l26-build-variants/
  l26-A.stampfix-failed-0432/` exists and holds only `headers.vfs.json` plus a
  partial `pass1/objects/` compile set (no stamp receipt, no BUILD_ID record).
  Its mtimes place it 2026-10-10 04:26Z–04:32Z (local 23:26 CDT), i.e. between
  the PREFLIGHT-WAIVER-20261010T0253Z direct-driver invocation (build started
  02:56Z) and the successful 04:37:09Z stamp — a build attempt that died
  mid-compile and was archived under the "stampfix-failed" label.
- **Honesty note (do not paper over this)**: NO local artifact records the
  literal BUILD_ID `l26-20261010T042544Z`. A repo-wide grep (all *.md/*.json/
  *.log under outputs/) finds zero hits for `042544`. The "failed 042544Z
  attempt" figure in the wave narrative is consistent with the timeline (a
  stamp generated ~04:25:44Z, failure ~04:26Z, archive label 0432) but is NOT
  receipt-documented; only the failed DIRECTORY is real. Likewise the exact
  count "3rd build attempt" is narrative, not receipted: the receipts prove
  one waive+direct-execution (02:53Z waiver), one archived failed partial
  build (stampfix-failed-0432), and one successful executed build (stamp
  04:37:09Z). If the failed BUILD_ID ever matters, recover it from the console
  klog/launch history at wake time — do not assert it from this record.
- **Which stamp a REBUILD from this snapshot would produce**: a NEW one. See
  §5 — the freeze is the file, not rebuild-reproducibility.

### 2.3 Why the two packaging runs tonight produced identical PRX shas

The twin identity chain, each link receipted:

1. BUILD twin: `outputs/2026-10-08-l26-build-variants/l26-A/TWIN-RECEIPT.json`
   — status `MAC_ONLY_IDENTICAL_L26_VARIANT_A_RUNTIME_PAIR_ENGINE_LISTENER`;
   build passes 1 and 2 each produced `runtime-l26-A.o` sha
   dbf6e6ae…ce3c under the SAME GeneratedBuildStamp.hpp (fe9a21bf…,
   build_id l26-20261010T043709Z). Both build passes of one `--execute` share
   the single stamp generated before any unit compiles.
2. PACKAGING twins: `FINAL-RECEIPT-VARIANT-A.json` — packaging pass-1 and
   pass-2 both emit PRX efbfc656…49f8 (31,419,959 B); `FINAL-RECEIPT-
   VARIANT-B.json` / `VARIANT-PAIR-PACKAGING-RECEIPT.json` — variant B both
   passes emit aa29c765…e852 (31,395,175 B). `PACKAGING-EXECUTION-RECEIPT.json`
   records both passes as `package_combined.py --pass-id 1|2` consuming the
   SAME frozen runtime object (variant artifact `runtime-l26-A.o`,
   dbf6e6ae…ce3c per PREPARATION-RECEIPT.json `runtime_inputs`).

So "identical PRX shas across the two packaging runs" (efbfc656 for A,
aa29c765 for B) is PACKAGING determinism over identical build objects with an
identical embedded stamp — it is NOT evidence that a fresh `--execute` from
the snapshot reproduces the bytes (§5).

## 3. Where the freeze lives (the frozen set)

The freeze is this quadruple — all under `outputs/`:

1. The PRX itself: `2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx`
   (efbfc656…49f8; the pass-2 twin is byte-identical and `2026-10-08-l26-package-B/*`
   is the diagnostic sibling, not part of the freeze).
2. The receipts: `2026-10-08-l26-package-A/FINAL-RECEIPT-VARIANT-A.json` +
   `pass-1|pass-2/{BUILD-RECEIPT,STATIC-CHECKS,EXACT-IMPORT-IDENTITIES,
   CONSTRUCTOR-ARRAY-CENSUS,PACKAGE-RESULT}.json` +
   `2026-10-08-l26-build-variants/{EXECUTION-RECEIPT.json, VARIANT-PAIR-RECEIPT.json,
   VARIANT-PAIR-PACKAGING-RECEIPT.json, PACKAGING-EXECUTION-RECEIPT.json}` +
   `l26-A/TWIN-RECEIPT.json` + `harvest/LAUNCH-READY-A-20261010T051653Z.md`
   (the 31/31 upload/arming ledger for the exact session).
3. The snapshot: `2026-10-08-l26-runtime/snapshots/snapshot-20261010T042420Z.tar.gz`
   (+ .manifest.json + .sha256 sidecar; snapshot id 24025929).
4. The session evidence: `2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log`
   (§4) and `evidence/townfall-main.lua.console-copy` (see ORIGINAL-MOD-IDENTITY.md).

Optional hardening at owner's discretion (NOT done — read-only tonight):
copy the PRX + snapshot to a distinctly named frozen dir so no future wave
archives/renames the package dir out from under the freeze.

## 4. Session evidence (menu-up, the reason this build is the freeze)

Log: `outputs/2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log`
— 1,111 lines, mixed narrow/wide encoding, ALWAYS `grep -a`. Verbatim key
lines with their line numbers:

```
L23   [2026-10-10 00:53:05] [BUILD] id=l26-20261010T043709Z changes=profile:scope;typescan-r4:scope;...;typescan-r4:scope_ey_lua_fname_bridge snapshot=24025929
L24   [2026-10-10 00:53:05] [ST] wcslen-16units expect=16 got=16 stride=2
L25   [2026-10-10 00:53:05] [ST] wstring-rt size=19 content-match=yes raw-first8=50006c0061007900
L605  [2026-10-10 00:53:06] [FFO2] q='GameEngine' id=0x47036 num=0 scanned=35479 exact=2 viaSuper=0 first=0x19000390A0 arm=w a
L814  [2026-10-10 00:53:08] [Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
L829  [2026-10-10 00:53:14] [LUA-FN] in='ItemDatabaseSubsystem' len=21 id=0x39B47 decoded='ItemDatabaseSubsystem' OK
L831  [2026-10-10 00:53:14] [LUA-FN] in='PlayerController' len=16 id=0x31AAD decoded='PlayerController' OK
L952  [2026-10-10 00:54:35] [Lua] [TownfallItemSpawner] 24 spawnable pickups found. Pages=3
L970  [2026-10-10 00:54:35] [FN] seam refused: reason='e m b e d d' len=4 at=1 (NAME_None produced; embedded NUL in a wide buffer is the stride-corruption signature)
L1049 [2026-10-10 00:55:14] [Lua] [zz] [DRY-1] pcs PASS n=1 iv=1 ipc=1
L1050 [2026-10-10 00:55:14] [Lua] [zz] [DRY-2] cdo PASS y
L1051 [2026-10-10 00:55:14] [Lua] [zz] [DRY-3] uwcls PASS y
L1053 [2026-10-10 00:55:14] [Lua] [zz] [DRY-4] create PASS y
L1055 [2026-10-10 00:55:14] [Lua] [zz] [DRY-5] sco PASS y
L1056 [2026-10-10 00:55:14] [Lua] [zz] [DRY-6] stext PASS ftext
L1110 [2026-10-10 00:55:47] [Lua] [TownfallItemSpawner] Failed at configure pickup. See UE4SS.log. ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua:317: Spawned pickup has no valid inventory ID.
```

Reading of the evidence (unchanged from CLAUDE-MENU-UP-SPAWN-BLOCKED.md and
STATE-L26-WAVE.md §9): the full menu path is console-proven end-to-end
(engine init → slot-2 ctor → decoder → bridge → FindFirstOf → UMG Create →
StaticConstructObject(TextBlock) → SetText(FText) → mod scan 24 pickups →
F7 menu VISIBLE IN GAME; DRY ladder 6/6 with reruns at 00:54:02/00:54:20).
The ONE remaining blocker is the reverse ItemId read
(`str(pickup.PickupInventoryItem.ItemId)`, main.lua:311–317) failing at
00:55:47, with the once-per-reason `seam refused … len=4` refusal (L970) as
the smoking gun and zero reverse-family [LFB-M] fires — the next work item,
NOT a defect of this build. The freeze covers the menu-up milestone exactly.

## 5. Rebuild-reproducibility: verified NON-reproducible — freeze the FILE

Facts verified by agent FN directly in the binary and the driver:

1. The BUILD_ID stamp string IS in the artifact, in `.rodata`:
   - Wrapped PRX: `l26-20261010T043709Z` occurs exactly ONCE, at file offset
     `0x126f7af`; `24025929` (snapshot id) at `0x126f988`.
   - Bare ELF (`pass-1/bare-original/modbridge_supplier_composition.prx`,
     magic 7f454c46): section table parsed — section index 3 is `.rodata`
     (type SHT_PROGBITS, file offset 0x1244000, size 0x16b732); the stamp
     sits at bare-ELF offset `0x126591f`, inside .rodata.
2. The stamp is REGENERATED on every build: `build_variants.py`
   (outputs/2026-10-08-l26-runtime/build_variants.py) deletes
   GeneratedBuildStamp.hpp at --plan time and regenerates it fresh BEFORE any
   unit compiles on every --execute (its own header comment + code: the
   stamp is written from `generated_at_utc` = the moment of --execute, and
   the execute REFUSES to run without a valid BRIDGE_SNAPSHOT_ID env).
3. Therefore a full rebuild from the frozen snapshot produces a PRX whose
   .rodata differs in the stamp string (new `l26-<new-UTC-ts>Z`) → different
   bytes → a DIFFERENT sha256. **A rebuild will NOT reproduce efbfc656
   byte-for-byte.** The identical efbfc656/aa29c765 twins across tonight's
   two packaging runs prove only that PACKAGING is deterministic over the
   same build objects (§2.3).

Bottom line, stated plainly: **the freeze is the PRX file + receipts +
snapshot — NOT rebuild-reproducibility.** To redeploy the known-good build,
upload the frozen `pass-1/modbridge_supplier_composition.prx` itself
(its sha is the acceptance key: efbfc656…49f8, decrypt-off readback magic
4f153d1d). A rebuilt PRX is a NEW build that must re-run the whole
verify_build.sh --manifest gate and earns a new freeze record of its own.

## 6. Rollforward procedure (reproduce FROM the frozen snapshot)

Two distinct goals — do not conflate them:

**(a) Redeploy the EXACT frozen bytes (the acceptance-proof path — preferred):**
skip build entirely; the frozen PRX is the artifact.

```
cd /WORKSPACE/.codex/worktrees/translation-core/what/outputs
shasum -a 256 2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx
# MUST print efbfc656432fcf349afa2fdc70c115773c1e21c92cddfeb57abbb36cf90949f8
xxd -l 4 2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx
# MUST show 4f153d1d (fake-signed SELF; 7f454c46 here = decrypted view — refetch/redo, see PS5 rules)
python3 2026-10-08-l25-prep/harvest/verify_build.sh \
    2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx \
    --manifest 2026-10-08-l25-prep/harvest/markers-declared.json \
    2026-10-08-l26-package-A
# expect 13/13 markers + receipt sha match (agent FN's read-only grep already re-proved 13/13 in the bytes)
python3 2026-10-08-l25-prep/harvest/prep_modded_launch.py \
    --variant A \
    --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx
# (add --dry-run first to plan; it uploads + verifies decrypt-off readback sha/magic, restores settings/ploader, arms ploader, starts klog tap)
```

**(b) Rebuild from the frozen snapshot (new BUILD_ID — a NEW build, new sha):**

```
cd /WORKSPACE/.codex/worktrees/translation-core/what/outputs
# 0) verify the frozen tree tar (sidecar is bare-filename — run in its dir)
cd 2026-10-08-l26-runtime/snapshots && shasum -a 256 -c snapshot-20261010T042420Z.sha256
# 1) extract the snapshot tar over a FRESH tree location (never in place over live dirs), then:
export BRIDGE_SNAPSHOT_ID=24025929        # from the manifest: snapshot_id = first 8 hex of tar sha
python3 2026-10-08-l26-runtime/build_variants.py --execute
#    (stamp generated fresh BEFORE compiling -> NEW BUILD_ID -> new bytes; refuses without valid env)
python3 2026-10-08-l26-runtime/l26_packaging_driver.py --prepare --execute
#    (two packaging passes per variant; twins must match per variant)
python3 2026-10-08-l25-prep/harvest/verify_build.sh <built.prx> --manifest
#    (13/13 declared markers REQUIRED in the PRX, UTF-8 + UTF-16LE; nonzero exit = do not upload)
python3 2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path <built.prx>
```

Caveats, per the receipts: `--execute` refuses to overwrite existing variant
dirs (archive/rename first — the 02:53Z waiver's archive-20261010T025518 is
the pattern); if the tree has changed since the snapshot, run
`harvest/snapshot_tree.sh` FIRST and use ITS new SNAPSHOT-ID (do not reuse
24025929 for a changed tree — the stamp exists to bind build↔tree); the
default full build is the only upload-grade mode (`--incremental` is a local
accelerator only, per build_variants.py's own help).

## 7. Console-side state that made it work (re-established by prep at wake)

From LAUNCH-READY-A-20261010T051653Z.md (the 31/31 PASS ledger for the exact
session) and CLAUDE-MENU-UP-SPAWN-BLOCKED.md §9:

- **PRX upload with PS5-law verification**: curl -T upload; readback with
  decrypt-OFF (`SITE SELF` discipline) — magic 4f153d1d AND full sha256
  byte-exact == efbfc656…49f8, size 31,419,959 B (ledger rows 12–15). The
  prior resident PRX was backed up first (31,369,495 B,
  c13244083d5212dc3447a0be88133b7f4eb0bf2df5e78a865e7d89cb7b2fc2c7).
- **UE4SS settings: original cache-off, exactly 1947 B** — restored from
  `/tmp/l24-settings-backup.ini` (sha b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2,
  `bUseUObjectArrayCache=false`) to
  `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`; readback cmp
  identical (ledger rows 16–20). Size-exact law (MFA admission): 1947 B.
- **ploader armed fresh**: ploader.ini enabled-state restored (440 B, sha
  3eab951051082de57de3dc3708b3c7210ae8ba4d99ad44b87e0d8c92e0cbb3d5),
  ONE loadpayload dispatch, fresh payload.elf pid confirmed, inject row for
  the variant PRX = 2 (ledger rows 21–28).
- **zzdiag v10 resident** (auto-probe + M1–M6 + F7 rerun; the DRY-1..6 ladder
  lines are zzdiag's): local `2026-10-08-l25-prep/zzdiag-v10/main.lua`,
  sha 79a4cc85072746e3094c641a7845108ba67c8fd5b87842786073863254cd50fd,
  4,092 B (loadlist zzdiag row size-exact 4092); deployed via
  `harvest/lua_fast_deploy.sh` (upload + readback cmp + loadlist row check).
  prep_modded_launch.py does NOT upload zzdiag — if a wake finds zzdiag
  stale, redeploy it with lua_fast_deploy.sh before the run.
- **klogsrv + klog taps**: klogsrv had died in a sleep cycle and was
  restarted that night; wakeup restores (CLAUDE doc §9). prep starts the klog
  tap + eboot monitor (ledger rows 29–31).

prep_modded_launch.py re-establishes all of the upload/settings/ploader/klog
items itself (its steps 1–7); zzdiag is the one item with a separate deploy
path. ploader remains last line of autoload.txt and exits per game launch
cycle — between launches in one boot, re-arm via
`pldmgr /loadpayload` (standing mission rule).

## 8. What this freeze does NOT claim

- No claim that spawn works: configure-pickup ItemId read still fails
  (L1110). Menu-up is the frozen milestone; the reverse-read seam is open
  work on top of this base.
- No claim of rebuild byte-reproducibility (§5 — stamp regeneration makes it
  impossible by design).
- No claim the failed 042544Z BUILD_ID is receipted (§2.2 honesty note).
- Variant B (aa29c765) is NOT frozen as known-good — it is the unsafe
  diagnostic sibling.
- Mac-only receipts: the build/package evidence is all `console_contact:
  false`; the console-proven part is the session log + LAUNCH-READY ledger.

— agent FN (ZCode), 2026-10-10. Read-only pass; the only files created are
this record, ORIGINAL-MOD-IDENTITY.md, and agent-FN-freeze.md.

## ROUND-4 FREEZE (2026-10-11) — supersedes efbfc656 as fallback

59b0528b77ca00c8f717a2fd9a3b8d0e838961aadf111933de31d27060a4af61
= round-4 spawn-proven build (snapshot a79b3655, stamp
l26-20261011T030618Z, 15/15 manifest PASS, P12/P13 sidecar paired,
decrypt-off readback verified on console). Session receipts:
evidence/round4-spawn-session-20261011{.log,.decoded.log} — items
spawn (Brick ID=5e, Shotgun ID=54), FR parity revert proven by the
PRB ladder twice, driver cb=23/24 sustained. Claude round-8: accepted
as console-proven; user eye-confirm: items worked in use.

## ACCEPTANCE PASS 1 FREEZE (2026-10-12) — supersedes 59b0528b as fallback

1e49188b3824d261 (snapshot 40831653, stamp l26-20261012T011649Z, 19/19
manifest, build exit-0, fs-throw gate pass). Contents: P1 fault-ledger +
P2 variant-B removal + world-fix (GetName/GetWorld) + lexical paths +
fs-throw gate + build lock + expected-status exits. NO event map logger,
NO shim root (both crashed and deferred).
Session: evidence/acceptance-pass1-20261012{.log,.decoded.log} —
spawner+CM+CE+Stamina all working, zero stutter, zero exceptions,
94 seconds. Claude round-12: accepted as smoke pass, not full acceptance.
