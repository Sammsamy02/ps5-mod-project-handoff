# l13 full build — Mac-only PASS, upload HOLD

The approved l13 runtime changes have completed two fresh l12-derived composition and packaging passes. The two runtime objects, combined objects and final wrapped PRXs are byte-identical. No console contact, upload, payload dispatch, launch or configuration change occurred during this work.

## Final artifact

- File: `prx-candidate-l13-2/modbridge_supplier_composition.prx`
- Size: **30,907,159 bytes**
- SHA-256: **`cd58fa12c1864eb11f3f1eb72a66361c8f7468f9b349629e98261a3aac6a705f`**
- Wrapped SELF magic: `4f153d1d`; embedded module type: `0xfe18`; SDK stamp: `0x11000000`.
- Strong imports: **200**, including exactly two additions over l12: `_openat` and `sceKernelRead`, both declared through the pinned libkernel provider. No removals or changes to the other 198 import identities.
- The new bridge helpers resolve internally; they are not PRX imports.

The aggregate receipt is `receipts/FINAL-GATES.json`. `SOURCE-MANIFEST.json` records the approved runtime/header hashes, the isolated pipeline scripts, final manifest and artifact identity. `deployment/candidate-manifest.json` identifies the exact candidate and expected live l12 preimage.

## Verification and receipts

| Check | Result | Receipt |
|---|---|---|
| Wrapped PRX, composed runtime and combined object determinism | Byte-identical across both fresh passes | `receipts/FINAL-GATES.json` |
| Static packaging checks | 20/20 per candidate | `prx-candidate-l13-{1,2}/verification-evidence/static-checks.json` |
| Full inherited offline gate suite | 11/11 for each candidate | `gate-receipt-l13-1-final.json`, `gate-receipt-l11-prx-candidate-l13-2.json` |
| Bridge definition, combined resolution, final import exclusion | PASS for five checked bridge functions | `receipts/accessor-verification-prx-candidate-l13-{1,2}.json` |
| Source/header ODR and consumer symbol checks | PASS | `receipts/prx-candidate-l13-{1,2}/` |
| New bootstrap-only import provenance and object usage | PASS | Each candidate's `verification-evidence/bootstrap-only-imports.json` and lineage verification |
| Approved l13 source hashes, frozen l12 PRX, original sources and retained pad hashes | Unchanged | `receipts/FINAL-GATES.json` + preceding local gate receipt |
| Transfer/rollback helper | 9 local stub-curl tests pass; preview remains HOLD | `deployment/tests/verification.json` |

Gate IDs exercised: W1, W2, W3, I1, D1, F1, S1, P1, M0, M1, I1+. Optional manifest sections B1/B2/B3/D2 were absent in the inherited l12 manifest and are **not** claimed as passed. These IDs are distinct from the separately run ODR checker's similarly named checks. Its historical map-name/retained-body advisory notes remain visible in the receipts.

The approved runtime sources were not changed during packaging. Their earlier local checks remain applicable: 22 admission helper tests, 9 initializer adapter tests, 60 sink/lifetime assertions, five harvest fixtures and two PS5-target translation-unit compilations. Those are host evidence, not proof that either new SDK import resolves on the console.

First-pass D1 was provisional until the twin existed; that receipt is preserved. A separate final comparison of candidate 1 against candidate 2 now also passes 11/11. Each successful pass has its own manifest and driver-evidence archive. The first overlay was moved aside under `driver/staging/lineage-overlay-2-aside-*` before building the second, preserving its bytes.

## Packaging corrections made locally

The first attempt exposed an exact typed-import rejection for `_openat`. `ld.lld -r` preserved the bridge's earlier untyped declaration because the typed import table came later. The isolated packager now places that existing typed table immediately before the bridge, preserving all frozen bootstrap units' relative order. Both new imports become FUNC references and the unchanged strict harness accepts 200 imports. No import validation was disabled. The failed attempt and red/green receipt are retained.

A manifest gate then caught an adapter receipt pointer referring to replacement-unit evidence instead of its existing extra-unit receipt. Correcting that pointer passed M1 without recompiling or changing the artifact. The second complete build remained independent; the corrected manifest was used for both final gate comparisons. Pipeline logs are in `logs/`; details are in `reports/PACKAGE-PRX-L13-REPORT.md` and `reports/PIPELINE-REVIEW.md`.

## Upload and rollback plan — requires the user's next OK

Only one replacement is proposed: `/data/plugins/modbridge_supplier_composition.prx`. Existing mod-root configurations remain unchanged for this build. Neither game executables nor kernel payloads are part of the operation.

1. User confirms Townfall is fully closed and approves this upload step.
2. Capture a fresh executable backup using `SITE SELF` on that same connection and require the server's decrypt-off acknowledgment. Compare its hash with the last confirmed l12 preimage, `024a4143bb84333e4552814fd68e67f6f72f7b04630dcd2b4a512b44d8bc34b4`. If it differs, stop before writing and reconcile the new state.
3. Freeze a backup receipt before the single upload; read back with decrypt-off and require exact candidate SHA-256/size/magic.
4. Only after successful readback, tell the user the artifact is staged. Payload loading and the one manual game launch remain separate user actions; this helper performs neither.
5. Rollback, if separately approved, restores the exact freshly backed-up signed PRX and verifies it by decrypt-off readback. It does not reuse the malformed historical `/tmp/l12-upload/ROLLBACK.sh`.

The prepared helper defaults to HOLD and has not contacted FTP. Its detailed usage is in `deployment/DELIVERY.md`. Backups will be captured only during the approved transfer; an old local capture is not represented as a fresh live backup. Klog, if later authorized, must go to a new file. No launch may occur before existing logs are harvested.

## Next console test criterion

Correlate PRX readback hash and fresh session/PID in UE4SS.log, runtime-status and supplier diagnostics. The admission milestone passes only if the pool is admitted via a logged source (VirtualQuery is preferred), Unreal startup completes, and TownfallItemSpawner stays installed with no current-session `Stopping mod`/terminal failure. A mere Ploader OK or absence of a stop line in an incomplete log is insufficient. Harvest before another launch overwrites evidence.

Menu appearance is recorded separately and is not required for this GATE-1 test. No l13 input fix was added.

## Input recommendation for the next build

The existing `scePadGetHandle` path already follows ALREADY_OPENED, but only during delayed polling retries. The l12 PID101 admission timeout prevented that loop from starting: no borrow attempt was recorded. First see whether l13 lets the existing path reach `pad_borrow_returned`, acquisition, state reads and F7 edges. `ALREADY_OPENED` alone does not prove ownership or borrow success.

If borrowing fails or supplies no usable input, prefer a small Unreal key-state observer on a proven game-thread callback: validate the local PlayerController and reflected `IsInputKeyDown`/FKey layout, then log L3/R3 state transitions before connecting the menu. This avoids acquiring another raw pad handle. It still requires per-build layout validation, a functioning game-thread carrier and menu navigation support; it is not automatically valid for every Unreal title. Full source-level analysis is in `../2026-10-07-l13-local-admission/reports/INPUT-NEXT-BUILD.md`.

**Status: Mac-only build verified. l13 loader resolution, pool admission, continued mod lifetime and menu/spawn behavior remain untested on the PS5.**
