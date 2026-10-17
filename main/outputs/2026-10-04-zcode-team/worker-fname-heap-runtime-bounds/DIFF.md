# DIFF.md — worker-fname-heap-runtime-bounds (launch10-c)

Three staged deltas + one REQUIRED companion hunk for the controller's fold.
Full unified diffs live in `staging/`; this file summarizes hunks and re-application
anchors. Bases verified by sha256 before editing.

## 1. Initializer unit (take my staged files as the unit)

- `staging/UnrealInitializer.cpp` — base
  `worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` (v2, sha256
  `b943aed5966270fb7e817ae6df7932637cd5437b526f4512036134907669a6a3`); staged sha
  `0852ec167e3ab97c55498af4b78c83454f138e307ac70b62bf9e6f6ceb3ffa0b`; diff
  `staging/unrealinitializer.diff` (4 hunks):
  1. `@@ -612` — extern "C" declarations of the bridge accessors
     (`bridge_readable_span_count/at`, `bridge_capture_envelope`) with provenance.
  2. `@@ -828` — Phase 3 rewrite: spanpub snapshot (bounded kMaxPublishedSpans
     copy) + empty-table loud refusals + envelope fetch (accessor value, floored
     by max published hi — under-stating the envelope only ever shifts refusals
     toward the honest truncation class) + `check_pool_shape` v2 call.
  3. `@@ -843` — refusal tail (value-printing preserved, coordinator-approved):
     `"FName pool at 0x{:X}: {failure} (observed CurrentBlock {}, Blocks[0] 0x{:X};
     capture envelope end 0x{:X})"` — the frozen pair is gone by design; when
     membership matched the refusal cannot reach this tail (that branch only
     fires on shape failure).
  4. `@@ -866` — success evidence line names the CONTAINING published span:
     `"…Blocks[0] 0x{:X} inside published non-image readable span [0x{:X},0x{:X})…"`.
- `staging/SeedValidation.hpp` — same base (`ca03e754…`); staged sha
  `6e8f8c4a08a8d53e43169efe2e71350c38d3bffa29b5af887cc029440b899633`; diff
  `staging/seedvalidation.diff`: replaces the 4-arg `check_pool_shape` with the
  v2 published-span version (+ `PoolShapeCheck.bounds_source`), adds
  `AddressSpan`, `kImageFileExtentBytes=0xbc602c8`, `kFNameBlockCapacityBytes=
  0x20000`, `kMaxPublishedSpans=16`, `kVmProtRead=1`, `VmSpanRecord`,
  `PublicationResult{published,total,cap_saturated,envelope_end}`,
  `publish_nonimage_readable_spans` (pure; top-16 by size, ascending, extent-
  clipped remainders, envelope over ALL records). `check_entry_bounds`,
  `entry_matches_ascii`, `classify_seed_address`, GMalloc V1/V2 untouched.
- `staging/SeedTable.h` — byte-identical to the launch-10 generated table
  (`d183e0c0…`, == gmalloc staging base; `SeedTable.launch10.h` copy).
  `kHeapLo/kHeapHi` stay in the generated table for regeneration contract
  provenance; NO code references remain (retired from admission).
- BREAKING header change: the old 4-arg `check_pool_shape` no longer exists —
  any stale fixture/worker driving the old signature must rebase to this staging.

## 2. Bridge side (spanpub) — hunks for the consolidated r14+diag fold

- Full staged file (geometrydiag base, sha `c3d6c4de…` before edits; staged sha
  `24f41e76…`): `staging/runtime_start_bridge_r13.geometrydiag-spanpub.cpp`.
- Trimmed hunks: `staging/spanpub-hunks-vs-geometrydiag.diff` (3 hunks).
  Re-application onto consolidated bridge fc06fe52 (r14+diag) is expected to be
  trivial BECAUSE the additions are new self-contained functions. The two hunks
  that touch `bridge_register_readable_ranges` internals (FLAG per controller
  instruction):
  - ONE line on the walk-failure early return:
    `::diagnostic_event("bridge_spanpub_count", 0);` — loud absence; the table
    stays empty and the initializer refuses naming it. In r14+diag, add the same
    line wherever the capture/convert failure path emits `bridge_lease_reg`.
  - ONE call after the frozen snapshot is filled/registered:
    `bridge_spanpub_publish(mappings, count, extent_begin, kImageExtentVaddr);`
    — r14 anchors: `mappings`/`count` are the frozen capture array + count
    (r14:403-408 in the r14-only file; wherever r14+diag keeps them after the
    lease fix), `extent_begin` = `kCandidateBase`, extent bytes =
    `kImageExtentVaddr` (0xbc602c8). If r14 renamed the locals, only this call
    line needs mapping.
  - Plus the new block after the registration function: `#include
    "SeedValidation.hpp"` (co-locate the header with the bridge TU or add -I to
    the initializer staging dir), the `bridge_spanpub_publish` definition +
    forward declaration, bounded storage (`g_spanpub_records[kBridgeVmmapEntries]`
    bss ≈ 384 KiB, `g_published_readable_spans[16]`, `g_spanpub_envelope_end`),
    diagnostics `bridge_spanpub_count/total/cap/env_low/env_high`, and the three
    extern "C" accessors. Zero new imports beyond <cstdint> (already included);
    SeedValidation.hpp is freestanding (<cstddef>/<cstdint>/<string_view>).
- Naming per the coordinator's example: `bridge_readable_span_count` /
  `bridge_readable_span_at(uint32_t i, uint64_t* b, uint64_t* e)` exactly;
  `bridge_capture_envelope(uint64_t*)` added for truncation honesty.

## 3. REQUIRED companion hunk — core capture capacity (coordinator option (a))

The user's bar: the collection must reliably cover the ranges the pool validator
reads; the heap sits at record ~3.4k in the launch-9-class enumeration, far past
the first 1,024. The BRIDGE capture already satisfies this (16,384 static bss).
The CORE-side capture does not; controller folds (not my TU, proposal-only):

- `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/
  live_module_facts.hpp:85`: `kMaxVmMappings = 1024` → `16384`
  (`static constexpr size_t kMaxVmMappings = 16384;`).
- `.../src/live_module_facts.cpp:203`: `OsVmMapping mappings[kMaxVmMappings];`
  → `static OsVmMapping mappings[kMaxVmMappings];` (process-lifetime bss;
  16,384×24B = 384 KiB — stack would be unsafe on a game-hijacked thread).
  `vm_mappings_truncated` semantics at :210 are unchanged and now false in the
  observed shape. Consumers in launch-10 are fresh units (controller-confirmed),
  same class as the anchor-cap bump.

## 4. Compile gates (formal, receipts/)

- Initializer: exact 190-arg unit argv
  (`worker-launch10-gmalloc-integration/receipts/compile-gate.argv.json`,
  input/output/-MF swapped; argv preserved at
  `receipts/compile-gate.argv.json`; LLVM_CONFIG pinned
  llvm/LLVM-21.1.8-macOS-ARM64) → **exit 0, 6 warnings in exactly the 4
  pre-existing baseline classes** (1 pragma-message asmjit, 2
  deprecated-enum-enum-conversion, 2 -Wswitch, 1 -Wundefined-inline — identical
  multiset to the gmalloc baseline stderr). Object:
  `receipts/unreal-unrealinitializer-spanpub.o` sha `6f830293…`.
- Bridge: pinned prospero SDK, `-Wall -Wextra -Werror -std=c++20 -O1 -fPIC
  -fno-stack-protector -fvisibility=hidden -ffreestanding -fno-builtin`,
  overlay includes (l10r3-a: generated/probe/lifecycle/base + LuaRaw):
  baseline geometrydiag → exit 0, **0 bytes stderr**
  (`receipts/bridge-baseline-compile.stderr`); my spanpub → exit 0, **0 bytes
  stderr** (`receipts/bridge-spanpub-compile.stderr`), object sha `3ab04702…`.
  `llvm-nm` confirms defined `bridge_readable_span_count`,
  `bridge_readable_span_at`, `bridge_capture_envelope` (T) + bss storage.
