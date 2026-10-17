# Agent FN — freeze + original-mod-identity prep (2026-10-10, console in rest mode)

Task: freeze record for PRX efbfc656 + acceptance-proof prep for the
unmodified TownfallItemSpawner rerun. Read-only + 3 new .md files only
(no tree source edits, no console contact, no builds). Deliverables written:

1. `FROZEN-KNOWN-GOOD-20261010.md` — the freeze record.
2. `ORIGINAL-MOD-IDENTITY.md` — the acceptance-proof prep.
3. This report.

## Findings (all verified directly against the artifacts tonight)

- **PRX identity re-verified**: pass-1 AND pass-2 of
  `2026-10-08-l26-package-A/…/modbridge_supplier_composition.prx` =
  sha256 efbfc656432fcf349afa2fdc70c115773c1e21c92cddfeb57abbb36cf90949f8,
  31,419,959 B, magic 4f153d1d. Matches FINAL-RECEIPT-VARIANT-A.json
  (prx+twin blocks) and pass-1 STATIC-CHECKS.json (20/20,
  fself_magic_wrapped=4f153d1d, 207 imports) / BUILD-RECEIPT.json
  (MAC_PACKAGE_CHECKS_PASS, 0 gaps). Variant B twin aa29c765…e852 both
  passes (unsafe diagnostic sibling, not frozen).
- **Snapshot verified**: snapshot-20261010T042420Z.tar.gz sha
  240259295b813319898e0b2d186a219b80fd8388b19c3bf2a6a1b23306113000 —
  `shasum -a 256 -c` OK from the sidecar (bare filename; must run in the
  snapshots dir). Manifest: snapshot_id 24025929, 50 files, consumer =
  BRIDGE_SNAPSHOT_ID.
- **BUILD_ID reconciliation**: stamp header GeneratedBuildStamp.hpp
  (fe9a21bf…, 1487 B) pins l26-20261010T043709Z in BUILD-INPUTS.json,
  both INPUT-SELECTION.json passes, and EXECUTION-RECEIPT.json
  (generated_at_utc 2026-10-10T04:37:09Z). Console log line 23 shows the
  same stamp. HONESTY: the failed-attempt dir
  `2026-10-08-l26-build-variants/l26-A.stampfix-failed-0432/` is real
  (partial pass1/objects + headers.vfs.json, mtime window 04:26–04:32Z,
  no stamp receipt), but NO artifact anywhere records the literal
  `l26-20261010T042544Z` — that BUILD_ID is narrative, not receipted; the
  "3rd attempt" count likewise. Stated as such in the freeze record.
- **Rebuild-reproducibility verdict: NO.** The stamp string sits in
  .rodata (bare ELF section 3, offset 0x1244000 size 0x16b732; stamp at
  bare 0x126591f / wrapped 0x126f7af; snapshot id at 0x126f988) and
  build_variants.py regenerates the header with a fresh timestamp before
  any unit compiles on each --execute. The efbfc656==efbfc656 and
  aa29c765==aa29c765 twin pairs prove PACKAGING determinism over the SAME
  runtime objects (runtime-l26-A.o dbf6e6ae…ce3c, TWIN-RECEIPT:
  both build passes identical under the one stamp) — not that a fresh
  build reproduces the bytes. Freeze = FILE + receipts + snapshot.
- **Session evidence pinned with line numbers** (claude2-menu-session-20261010.log,
  1,111 lines, grep -a): L23 [BUILD] stamp; L24–25 [ST] probes PASS; L605
  [FFO2] GameEngine oracle; L814 mod Loaded v1.0; L829/831 [LUA-FN]
  0x39B47/0x31AAD; L952 24 pickups Pages=3; L970 seam-refused len=4; L1049–1056
  DRY-1..6 PASS (00:55:14, reruns 00:54:02/00:54:20); L1110 ItemId
  configure failure (00:55:47, main.lua:317).
- **Independent 13/13 marker recheck**: read-only grep of the frozen PRX
  against harvest/markers-declared.json (UTF-8 + UTF-16LE, either_encoding
  honored) — all 13 declared markers composed in the bytes. Re-proves the
  pre-upload manifest gate claim.
- **Rollforward procedure** documented two ways: (a) redeploy the frozen
  file (verify_build.sh --manifest then prep_modded_launch.py --variant A
  --prx-path …); (b) rebuild from the snapshot (snapshot_tree/BRIDGE_SNAPSHOT_ID
  → build_variants.py --execute → l26_packaging_driver.py --prepare --execute
  → verify_build.sh --manifest → prep) with the new-BUILD_ID caveat and the
  archive-first rule.
- **Console state** (LAUNCH-READY-A-20261010T051653Z.md, 31/31 PASS):
  upload with decrypt-off readback (magic + full sha match), settings
  original cache-off 1947 B (b980cea7…, bUseUObjectArrayCache=false),
  ploader.ini restore + one loadpayload arm + fresh pid, klog tap + eboot
  monitor. zzdiag v10 (79a4cc85…, 4,092 B, loadlist row 4092) rides
  lua_fast_deploy.sh, NOT prep_modded_launch.py — flagged in the doc.
- **Original mod FOUND locally** (no user zip needed):
  `.superpowers/native-observations/item-spawner-real-mod-intake-1/original.zip`
  (a9f7ed00…7fb, 5,939 B; Nexus silenthilltownfall mods/95 file_id=218,
  intake.json mod_edits=false). Member main.lua (21,415 B,
  f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658) is
  BYTE-IDENTICAL (cmp + diff empty) to
  `evidence/townfall-main.lua.console-copy`. enabled.txt empty member also
  pinned. Corroborated by the whole 2026-10-04→10-08 staging/provenance/
  audit lineage (all f586043a) and prior l16/l22 review records. Four
  false fixture candidates documented so they can't be mistaken for the
  original. Wake-time fetch+shasum+cmp template written; never edit the mod.

## Bottom line for the wake

The menu-up build efbfc656 and the unmodified original mod are both frozen
and byte-accounted. The acceptance rerun needs: redeploy/verify the frozen
PRX per FROZEN-KNOWN-GOOD §6(a), re-verify the console mod per
ORIGINAL-MOD-IDENTITY §4, run, write the UNMODIFIED PROOF verdict.

— agent FN (ZCode), 2026-10-10
