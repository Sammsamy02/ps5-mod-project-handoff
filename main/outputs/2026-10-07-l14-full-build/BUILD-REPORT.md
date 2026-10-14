# l14 full build: Mac-only PASS, upload HOLD

The approved image-admission change is implemented and has completed two fresh launch11-derived composition and packaging passes. No console contact, upload, payload dispatch, launch or configuration edit occurred. Pad/input code is unchanged.

## Candidate

- File: `prx-candidate-l14-2/modbridge_supplier_composition.prx`
- Bytes: **30,913,975**
- SHA-256: **`769284e3a713f741b51ca2c5da64e654db0fb93e6b725511488600bc35ac8444`**
- Wrapped SELF magic: `4f153d1d`; embedded module type `0xfe18`; SDK `0x11000000`.
- Strong imports: **200**, identical by full identity to l13: **zero additions, removals or changes**.
- Runtime object, packaging combined object and wrapped PRX are byte-identical across both independent builds.

The candidate is held locally. It has not run on the PS5. It is not evidence of a menu, item spawn or universal UE4SS compatibility.

## Source change

Six files differ from l13: three new headers and three modified runtime files. `reports/DIFF-SUMMARY.md` explains the change; `reports/SOURCE-DIFF.patch` contains the complete diff. Image metadata now uses exact readable data/BSS segment coverage; heap reads remain VirtualQuery-first. Five independent metadata fields and their safely evaluable anchor/entry checks are collected before refusal. Unreadable pointer dependencies are logged as blocked. Each record includes source/outcome/range, VQ or segment evidence, oracle/shadow status and elapsed/budget. A final line logs total admission elapsed even on a propagated exception where stack unwinding occurs.

The original non-image publication is unchanged. Actual loaded module-info segments are preferred. Where module-info is unavailable, exact PT_LOAD facts from the retained, hash-verified per-build image are combined with the same launch's retained live VM records and observed base. That fallback is labeled `profile-PT_LOAD+live-mappings`; a saved profile alone never grants a read. This remains a runtime evidence path to test, not a claimed successful module-info query.

## Tests and gates

| Check | Result | Receipt |
|---|---|---|
| Exact console layout against unmodified l13 | Expected FAIL: image metadata absent from heap publication | `../2026-10-07-l14-image-admission/receipts/l13-exact-case-red.log` |
| Actual production initializer with image base 0x400000, pool 0x9765EC0, heap 0x22C0000000 | Exact case passes; 8 integration scenarios pass | `../2026-10-07-l14-image-admission/receipts/initializer-integration.json` |
| Image/pool proof policy | 14 scenarios pass | Local gate archive, `image-pool.json` |
| Actual bridge publication/segment getter | PASS; image stays excluded from heap table | `../2026-10-07-l14-image-admission/receipts/bridge-segments.json` |
| Existing heap admission and shared budget | 22 tests pass | Local gate archive, `range-engine.json` |
| Sink errors, partial writes, fault/lifetime handling | 60 assertions pass | Local gate archive, `sink-lifetime.json` |
| Changed initializer and bridge PS5 target compilation | Both compile | Local gate archive, `ps5-compile.json` |
| Static PRX packaging checks | 20/20 each | `prx-candidate-l14-{1,2}/verification-evidence/static-checks.json` |
| Applicable inherited offline gate suite | 11/11 each | `gate-receipt-l14-1-final.json`, `gate-receipt-l11-prx-candidate-l14-2.json` |
| Internal bridge definitions, combined resolution, exclusion from imports | All 6 accessors PASS | `receipts/accessor-verification-prx-candidate-l14-{1,2}.json` |
| Double-build determinism and exact import census | PASS | `receipts/FINAL-GATES.json` |
| Original l13 sources, frozen artifacts and retained pad sources | Hashes preserved | Local and final gate receipts |

Host integration tests use ASan/UBSan and replace only OS, clock and direct-memory boundaries. They execute production policy and orchestration; they do not execute Townfall. The worst scheduled retry path consumes the shared 60 seconds, below the 300-second startup deadline. A modeled 61-second OS call is rejected after returning and starts no later read. A synchronous platform call cannot be forcibly preempted by this budget; actual console time must be measured.

Gate IDs: W1, W2, W3, I1, D1, F1, S1, P1, M0, M1, I1+. Optional B1/B2/B3/D2 sections were absent in the inherited manifest and are **not** claimed passed. The separate ODR checker passes with its inherited historical map-name/body advisories visible in archived receipts. Candidate 1's provisional D1 is retained; its final comparison after the twin existed passes all 11 gates.

`receipts/FINAL-GATES.json` is the aggregate build receipt. `SOURCE-MANIFEST.json` pins runtime/header sources, pipeline scripts, editions, final manifest and artifact. Each candidate has its own manifest/driver evidence archive under `receipts/prx-candidate-l14-{1,2}/`. Full local logs are in `logs/`. Builds/tests were executed under network denial.

## Next step requiring separate approval

Approve replacement of only `/data/plugins/modbridge_supplier_composition.prx`, after Townfall is fully closed and FTP is on. The expected live preimage is the deployed l13 SHA `cd58fa12c1864eb11f3f1eb72a66361c8f7468f9b349629e98261a3aac6a705f`.

Before any upload, take a fresh decrypt-off backup using `-Q "SITE SELF"` on that same executable-download connection, confirm the server acknowledgment, exact preimage hash and SELF magic, and retain the backup as rollback. If the preimage differs, stop before writing. After a separately approved upload, use a decrypt-off readback and require exact candidate hash, size and wrapper. No INI, load-list, autoload or mod-root/config changes are part of l14.

The next klog must go to a new file. The user performs one launch. Harvest before any subsequent launch. Rollback, if requested/approved, restores that fresh signed l13 backup and verifies it with decrypt-off readback. No game executable or kernel component is involved.

## Console interpretation

The next admission milestone needs a fresh l14/session-matched log showing segment coverage for image fields, heap admission via a logged readable source, total admission elapsed, completion of Unreal initialization and the mod still active. An absent stop line in an incomplete log or Ploader OK alone is insufficient.

Record all per-address checks, VQ start/end/protection/flags, segment source/bounds, oracle/shadow status, total elapsed vs 60 seconds, any current-session `Stopping mod`/failure, pad borrow/state lines, L3+R3/F7 input edges and widget/viewport calls. Pad code remains unchanged, so successful admission does not promise menu input. Read current autotune.ini. Compare with the existing smoke checklist, including row N for startup-deadline expiration, before proposing one next fix.

**Status: Mac-only build verified. l14 console admission, continued mod lifetime, input/menu and item spawning remain unproved. Upload is on HOLD.**
