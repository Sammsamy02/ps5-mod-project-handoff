# worker-shadow-integrate — admission note

Task: integrate the host-verified shadow-copy code scanner (worker-code-scan,
35/35) into the UE4SS runtime's ScanGame path as staged source files.
Scope held: staged unit integration only; no shared-lineage edits, no
composition, no console contact, no game claims.

## 1. Result

**Host test: 38/38 checks pass; clean under AddressSanitizer + UBSan**
(`host_test/run-output.txt`, `host_test/run-output-asan.txt`; asan exit 0,
zero sanitizer diagnostics). The test runs against the REAL contracts, not
re-implementation shadows:

- the REAL `PlatformModuleSnapshot.hpp` constructor validates the shadow
  ranges (any extent/ordering/overlap violation throws and is refused);
- the REAL UE4SS owned-range scanner — `OwnedRangeScanner.inl`, the exact
  unit the composition's `scanner` runtime unit builds from — executes the
  exact non-Windows `Signatures.cpp` seam
  `SinglePassScanner::start_scan(map, config.ModuleSource->ReadableRanges())`
  over the shadow ranges (compiled in a mocked environment: Profiler stubs +
  static-member definitions + `m_num_threads=1`, Scalar; the scan algorithms
  are verbatim);
- the REAL generated `SeedTable.h` (launch11-combined units) supplies the
  pinned seeds — included, never retyped;
- the REAL retained decrypted eboot (194,241,424 bytes,
  sha256 `3b6cfad4...`) is the fixture.

## 2. Staged files (this directory only; nothing outside it was modified)

| File | What |
|---|---|
| `shadow_module_source.hpp` | NEW unit. Wraps `shadow_copy.{hpp,cpp}` into the `PlatformModuleSnapshot` ModuleSource contract: compact FIPS 180-4 SHA-256 identity gate (verified against FIPS vectors + the pinned image digest), `ModuleInput` assembly per INTEGRATION.md §2 (monolithic MainExe, `base = img->base`, `size = backing_size`, per-PT_LOAD file-backed `{backing, filesz}` ranges, `find_export` -> nullptr), lifetime owner whose deleter runs `shadow_shutdown`, and `ShadowSigAdapter` — the match-address bifurcation (`live(c)` = linked+slide store/call address; `bytes(c)` = readable shadow pointer; `live_via_rip()` = data-class recipe). |
| `shadow_arm.hpp` | NEW unit. The THIRD ScanGame arm (INTEGRATION.md §1.3/§4), dependency-injected (eboot bytes, bridge `observed_base`, seed pins, optional-slot defs, log sink): slide-provenance gate (0 = refuse, never a cached slide; != `kBaseObserved` = refuse naming both), identity gate, image-shape pins (first PT_LOAD linked vaddr 0; whole-image extent `0xbc602c8`), REQUIRED-slot cross-check under a uniqueness discipline (exact audited hit count, group-by-resolved-target, corroborating sig for slot 0), OPTIONAL slots resolvable-with-fallback. Carries the four pinned audited sigs from worker-code-scan REPORT §2. |
| `patch-unrealinitializer-shadow-arm.diff` | Unified diff (120 lines) against `worker-launch11-vtable-prebuild/staging/initializer-vtable-pre/UnrealInitializer.cpp`. Adds `#include "shadow_arm.hpp"` (prospero block), the `SeedArming::shadow_arm_stage()` wrapper (pins from `SeedTable.h`, slide via the file's own `module_view_base()` bridge-geometry accessor, log sink via `Output::send`, every refusal through the existing `seed_refused` loud-throw, eboot read via the NID-routed fopen route), and the one-line call after `consume_required_slot_seeds` in `ScanGame()`. Verified: `patch --dry-run` applies cleanly and round-trips byte-identically. |
| `patched/UnrealInitializer.cpp` (+ `.base`) | The patched staging copy and its pristine base (for review/diffing; no lineage file touched). |
| `ue4ss_headers/` | Verbatim pinned copies the host test compiles against (see §3 pins). |
| `pinned/` | Verbatim pinned `shadow_copy.{hpp,cpp}` (worker-code-scan), `SeedTable.h` (launch11-combined units). |
| `host_test/` | `host_test.cpp`, `mock_owned_range_scanner.cpp`, `Makefile`, `run-output.txt`, `run-output-asan.txt`. |

`Signatures.cpp` itself needs NO change: its non-Windows arm already calls
`start_scan(map, config.ModuleSource->ReadableRanges())`, and the host test
proves the shadow satisfies that exact seam (real scanner code, correct live
addresses, no boundary-crossing matches, absent patterns yield nothing). The
match-address bifurcation is consumed inside `on_match_found` handlers via
`ShadowSigAdapter` (host-tested on both classes: direct-function and
rip-decoded data).

## 3. Pin cascade (what the staged units are pinned to)

```
decrypted eboot  sha256 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc
  == SeedTable.h kImageSha256            (identity gate; refusal names both hashes)
  == worker-game-version/console-decrypted-eboot.bin (test fixture, read-only)

SeedTable.h (launch11-combined units, sha256 d183e0c0...)
  kBaseObserved 0x400000                 (slide pin; bridge observed_base must equal)
  kGUObjectArray 0x93ba110 | kFNameCtorWchar 0x23d590
  kGMalloc      0x930add8 | kStaticConstructObject 0x490aa0   (cross-check labels)
  image shape   extent 0xbc602c8, first PT_LOAD linked vaddr 0 (arm's shape pins)

shadow_copy.hpp/cpp (worker-code-scan, 3c345c45.../c7f41e88...)   (scanner, unchanged)
  sigs pinned in shadow_arm.hpp (uniqueness-audited: 1/1/1/1 hits + 2-site corroboration)

UE4SS contracts (verbatim copies under ue4ss_headers/, compiled by the test):
  Unreal/PlatformModuleSnapshot.hpp     3d05b068...  (lineage-overlay-2-l10r3-b)
  SigScanner/SinglePassSigScanner.hpp   fd4c706e...  (lineage-overlay-2-l10r3-b)
  SigScanner/OwnedRangeScanner.inl      59bd538d...  (the scanner unit's impl)
  Helpers/UETargetModules.hpp           90c41a85...  (lineage-overlay-2-l10r3-b)
  Helpers/Common.hpp / SigScanner/Common.hpp  e5286b1e... / 88e708ad...

Integration patch base:
  UnrealInitializer.cpp (initializer-vtable-pre staging) 39fe33f3... -> patched dd0bd752...
```

Cascade rule: a new game image changes the eboot sha -> the arm's identity
gate refuses loudly, which is the same event that invalidates every seed
(seed table + sigs re-derive together against the new image). A moved image
(observed_base != `kBaseObserved`) refuses naming both values. Nothing in the
cascade is ever silently overridden by the shadow.

## 4. What the host test verified (38 checks)

- SHA-256 engine: FIPS empty/`abc`/64-byte-boundary vectors.
- Identity: fixture sha256 == generated `SeedTable::kImageSha256`.
- Init/contract: `Create` passes the REAL `PlatformModuleSnapshot`
  constructor; MainExe view `base/size` == shadow base/backing (0x9633418);
  `ReadableRanges()` == per-segment `{backing, filesz}`; sorted,
  non-overlapping, inside one extent (independently recomputed);
  `FindExport` configured, returns nullptr (bridge source keeps that duty);
  the exact `SetupUnrealModules()` seam lines consume `Views()/IsMonolithic()`
  unchanged (monolithic publish to every ScanTarget).
- Patterns -> live: `shadow_arm` ARMED with slide == observed_base ==
  `kBaseObserved`; all four REQUIRED cross-checks GREEN with live addresses
  exactly `0x97ba110 / 0x63d590 / 0x970add8 / 0x890aa0` (asserted from the
  generated SeedTable, not literals); GUA sig-A corroboration green.
- REAL `start_scan` seam: fname sig -> `live(c)` == 0x63d590, `bytes(c)`
  reads the pinned prologue from the shadow; GUA xref sig (with `??`
  wildcards) -> rip decode on shadow bytes -> 0x97ba110; absent pattern
  never fires; match pointer inside a supplied range.
- Refusal honesty: no geometry; slide mismatch (both bases named); identity
  mismatch (both hashes named, actual first); malformed pin; seed/shadow
  label mismatch (both labels + both would-be live addresses named); the
  documented NON-unique 16-byte SCO prologue refused on hit count (22879 !=
  1); drifted image extent refused.
- Optional slots: all four real names report "fallback retained" (no pinned
  sigs — degradation paths stay); the resolution machinery itself proven via
  a labeled demo entry -> live 0x890aa0.
- Determinism + lifetime: second arm run identical; releasing the snapshot
  runs `shadow_shutdown` (ASan/LSan clean — no leaked images).

## 5. Expected composition behavior (when the controller admits this)

1. Drop `shadow_copy.{hpp,cpp}`, `shadow_module_source.hpp`, `shadow_arm.hpp`
   side-by-side into the unrealinitializer unit's include path; apply
   `patch-unrealinitializer-shadow-arm.diff` (round-trip verified). One open
   repin: `kDecryptedEbootPath` in the patch must be set from the game
   profile `identity.executable_console_path` (staged placeholder:
   `/data/homebrew/townfall-ps5-ue56-v1/decrypted/eboot.bin`; the
   NID-routed fopen route is proven live by the bridge probes).
2. Runtime shape on console, armed path: SHAPE A gate (unchanged) ->
   `consume_required_slot_seeds` (unchanged, slots 0/2/3/4 seed-bound) ->
   `shadow_arm_stage()`: reads the eboot (~194 MB, once), hashes it
   (~sub-second), builds the ~150 MiB shadow, cross-checks the four seeds,
   logs per-slot GREEN lines; any refusal is the same loud terminal
   `seed_refused` throw as the seed path (never replayed, never re-rated).
   The bridge's live ModuleSource and `SigScannerStaticData` are untouched:
   the shadow is retained separately (`SeedArming::ShadowSource`).
3. Memory/order notes for the composer: the arm runs INSIDE ScanGame, i.e.
   AFTER `SetupUnrealModules()` — correct for the staged direct-scan design
   (`scan_pattern` on the shadow). If a future composition wants the full
   container path (`start_scan` over the shadow ranges in the standard
   DoScan arms), the swap site is `StaticStorage::GlobalConfig.ModuleSource`
   BEFORE `SetupUnrealModules()` (UnrealInitializer.cpp:1387), with handlers
   consuming `ShadowSigAdapter` per the bifurcation contract; that is a
   separate, controller-owned composition decision, not part of this stage.
4. Existing behavior preserved: SHAPE A refusal on partial tables, seed
   binding + validation chain, ps_scan dead arm, both Lua override-scan
   skips, optional-slot fallbacks, lease/span/class rules. A LOAD0 hit still
   maps to an exec-only CALL-ONLY address; a LOAD7 hit to
   data-readable-under-lease. No live exec page is ever read — that is the
   point of the shadow.

## 6. Honest limits

- Host verification only. The PRX-side build, the real `fopen` read of the
  decrypted eboot in-game, and the console-time arming order are designed
  and patched but NOT console-executed (same limit class as worker-code-scan;
  Phase F in-game arming stays future work under the usual evidence rules).
- The patch is verified to apply and round-trip against the
  `initializer-vtable-pre` staging base; it has NOT been compiled inside the
  full PRX composition (composition is out of this worker's scope). The
  wrapper only uses symbols already present in that translation unit
  (`seed_refused`, `module_view_base`, `SeedTableNS`, `Output::send`,
  `ensure_str`, `Platform::ModuleSnapshot`).
- Optional slots remain unbound by design until per-slot sigs are derived
  AND uniqueness-audited against the pinned image (the demo entry proves
  machinery only — it is not a sig claim for any optional slot).
- The cross-check sig set is exactly the four audited sigs from
  worker-code-scan; adding any sig requires the same uniqueness audit
  (the arm enforces audited hit counts mechanically).
