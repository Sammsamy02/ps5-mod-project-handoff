# IP-shim-root — apply notes (for ROOT at the next compose/AC window)

Target: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp`
(sha256 of the UNPATCHED file at staging time — verify before applying:

```
shasum -a 256 outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp
```

see `unpatched-sha256.txt` in this dir; if it diverged, re-anchor the patch on
the new bytes — the hunk context is the `setup_mod_directory_path` Prospero
native branch).

Apply (from the candidate root):

```
cd outputs/2026-10-08-l26-runtime/typescan-r4/candidate
patch -p1 < ../../../2026-10-08-l25-prep/round5-staging/IP-shim-root/01-ue4ssprogram-shim-root.patch
```

Post-apply duties (standard l26 candidate workflow):
1. Refresh the unit's CHANGES.json row (changed-file hash) the way the l26
   packaging driver expects (`l26_packaging_driver.py` re-derives digests;
   typescan-r4 CHANGES.json `changes[]`/`files[]` rows for UE4SSProgram.cpp
   must carry the new sha).
2. The next `--plan` gate re-proves the tree; no new unit, no header cascade
   (the change is function-local, no ABI/header change).
3. Integration with II's round-5 restore: independent file — II's
   restore/additions scripts touch LuaMod.cpp/LuaPathShim/UnrealInitializer,
   NOT UE4SSProgram.cpp's setup_mod_directory_path. No ordering constraint
   beyond both being applied before the next build.

Markers: no NEW marker literals are introduced by this patch (the
`[SHIM] path-resolver installed …` and `[SHIM] write-area …` lines already
exist in LuaPathShim.cpp and are already in the round-5 marker set — II's
markers-add-round5.json). The patch only makes them REACHABLE.

Verification receipts in this dir:
- `syntax-receipt-UE4SSProgram-shim-root.log` — EXIT=0, 0 errors (prospero)
- `syntax-argv.txt` — exact argv (program unit from
  l24-build-r2 COMPOSITION-RECEIPT.json, source swapped, l26 -I prepends,
  l26-A headers.vfs.json, LLVM_CONFIG env per IA/IK note)
