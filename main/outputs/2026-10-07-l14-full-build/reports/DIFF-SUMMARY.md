# l14 local changes

Image-resident FName fields now use exact readable data/BSS segment bounds separately from the intentionally non-image heap span publication. Heap reads retain VirtualQuery-first admission, copyin fallback, the shared 60-second scheduling deadline, prior-fault exclusions and no shadow permission override.

- `ImageCoverage.hpp`: generic complete-range segment proof. Prefer real loaded module-info segments. Otherwise combine the captured per-build PT_LOAD profile with the same launch's retained VM mappings and matching observed base. Reject executable-only segments, profile-only evidence, unmapped gaps, straddles, overflow and rounded segment slack.
- `ImageLoadProfile.hpp`: Townfall's five exact PT_LOAD records from the retained executable, including data/BSS. The file hash and segment rows were independently rechecked. Game-specific values remain in this profile header.
- `PoolAdmissionEngine.hpp`: image reads require segment coverage with VirtualQuery cross-check. Hard negatives veto admission; an unavailable VirtualQuery API can be tolerated only with complete segment/live evidence. Heap ordering and fault policy remain unchanged.
- `FNamePoolChecks.hpp`: production collect-all evaluator. Independently checks five pool fields, the None anchor, indexed block pointers and None/ByteProperty entries. Unknown pointer dependencies produce explicit blocked lines, never speculative reads.
- `UnrealInitializer.cpp`: replaces the impossible image-in-heap precheck with the production evaluator; logs segment/VQ bounds, every check's outcome/source, oracle/shadow status and total admission elapsed time. Refuses before subsequent engine calls if any prerequisite fails.
- `runtime_start_bridge_r14_diag_spanpub.cpp`: preserves the existing heap publication; retains the complete existing VM snapshot and exposes internal `bridge_image_range`. Real module-info and profile/live fallback are labeled distinctly.

Pad and input code are unchanged. No timing-only fix, game executable changes, new imports or console contact. The complete source diff is `SOURCE-DIFF.patch`.

## Evidence and limits

The unmodified l13 initializer refuses the exact image-metadata/non-image-span case. The new production initializer passes the modeled image base 0x400000, pool 0x9765EC0 and heap block 0x22C0000000. The integration harness substitutes only OS, clock and direct-memory boundaries; it executes the production admission orchestration and policies.

Host checks: 14 image/pool scenarios, 8 actual-initializer scenarios, actual bridge/segment integration, 22 retained heap/admission tests and 60 sink/lifetime assertions. ASan/UBSan pass. Both changed translation units compile for PS5. The latest aggregate receipt is `../2026-10-07-l14-image-admission/receipts/LATEST-GATES.json` relative to the full-build directory.

The 60-second budget includes retry sleeps and is shared across all checks. A modeled 61-second OS call is rejected after it returns and starts no subsequent read. The code cannot preempt an indefinitely blocked platform call; measured host policy timing is not proof of console wall time.

Current retained supplier evidence lacks a usable main module-info handle on this setup. The fallback uses saved exact PT_LOAD bounds plus current retained VM records, not invented runtime module-info. No profile alone grants readability. Real console behavior, full initializer progress, mod lifetime and the menu/spawn remain unproved for l14.
