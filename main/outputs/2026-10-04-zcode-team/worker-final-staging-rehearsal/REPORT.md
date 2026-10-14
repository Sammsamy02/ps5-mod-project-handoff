# REPORT.md — worker-final-staging-rehearsal, 2026-10-04

Role: complete host-side rehearsal of the final console staging
(`worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md`, 9-entry manifest)
before real execution. Host-only; no network/console; no nested subagents;
writes confined to this directory. CLAIM.json was written first; this REPORT
is the last artifact.

## Result: READY TO EXECUTE — all checks PASS, no corrections required

Full per-check table, evidence, gaps, and the verified command list:
`REHEARSAL-RESULT.md` (same directory). Raw rehearsal run log:
`mfa-rehearsal-output.txt`.

### What was verified
1. **All 9 manifest sha256 + byte sizes re-measured on disk — every one
   matches §A exactly** (PRX `b8bfae77…` 30756791 B; loadlist `49ab7a3a…` 446 B;
   settings `9f3fa127…` 779; MemberVariableLayout `f291d552…` 588;
   VTableLayout `9c20f7c9…` 186; PS5Pad `1d919fbf…` 2960; main.lua `f586043a…`
   21415; enabled.txt `e3b0c442…` 0; UEHelpers `5a8f6609…` 9962). Zero
   mismatches, zero missing files.
2. **Loadlist structure**: 446 B, TAB-separated, LF endings; header
   `modforge-loadlist 1`; `title PPSA33286` / `engine unreal` /
   `framework ue4ss-ps5` / `mod TownfallItemSpawner 1.0.0`; exactly the 7 file
   rows §A quotes; row-size sum 35890 (manifest's corrected figure confirmed).
3. **mfa_activate rehearsal, manifest's exact file set** (root assembled from
   §A source paths; harness `test_activation_final.c` compiled 0 warnings
   against `worker-7-installer/vendored/modforge_activation.c`): **46 PASS /
   0 FAIL, exit=0** — parse, activation, 7-file preload (35897 B), per-file
   size/NUL facts, NativeModSource enumeration oracle, layout pair present
   588/186, and negative controls (MFA_E_TITLE −4, MFA_E_SIZE −8,
   MFA_E_NO_LOADLIST −2).
4. **Consistency checks**: `rehearsal-root` ≡ `worker-loadlist-final/final-package`
   (diff -r IDENTICAL); bundle loadlist ≡ final-package loadlist (same sha);
   PRX first bytes `4f 15 3d 1d` (FSELF → Gate A1 expects `7f 45 4c 46`
   readback); build-receipt `gaps: []`; parsepair `ok:true` with identity
   exactly as claimed (5.6.1-0+++UE5 / BuildId 04bb4330… / prospero-x86_64-sysv
   / FUObjectArray 10 members 0xB8 / TUObjectArray 7 members 0x20); ploader
   line matches pinned STAGING-CHECKLIST §2; §B command paths/hashes match §A.
5. **Launch simulation** documented: Gate A0 first (hard STOP without
   roundtrip) → arm + mode-2 re-send → launch → ~1 min injection report →
   Tier A only (`/data/townfall_supplier_composition.log` fresh: module_start,
   alignment, constructor, supplier/facts events) with the placeholder PRX →
   Tier B (runtime-status.txt, UE4SS.log, Lua lines) NOT expected this launch;
   all-or-nothing unmodded fallback is evidence, never a blind retry.

### Gaps (none blocking)
- Gate A0 FTP STOR write route remains the single host-unverifiable unknown;
  manifest already gates it as first command with hard STOP.
- Manifest sentence "worker-loadlist-final was NOT delivered" is stale — that
  package has since landed and is byte-identical to the manifest's set
  (annotated, existing files untouched).
- worker-composition-4 still has no PRX ⇒ §C contingency not triggered;
  placeholder stays file 1; Tier B not expected.
- Layout-pair code-anchor confirmation still PENDING (declared non-blocking).

### Final answer to the controller
Execute MASTER-STAGING-PACKAGE.md §0 → §B Steps 0–5 → §D as written (verbatim
list reproduced in REHEARSAL-RESULT.md §6, verified line-by-line). No hash,
file, ordering, or command corrections are needed.
