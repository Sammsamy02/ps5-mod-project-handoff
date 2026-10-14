# l24 review packet — PropertyImageClassRange spanpub fix (Mac-only)

Edition: l24, 2026-10-08. Scope of this document: Mac-only review material for
Claude's approval decision on the l24 PRX candidate
(`modbridge_supplier_composition.prx`, sha256
`2e712a8b0e1ffc2b8c12af6e5b7a7a8fe63fe6e6a5c24d113655faebb79c92f4`,
31,331,783 bytes, SELF magic `4f153d1d`, SDK stamp `0x11000000`).
No console contact occurred at any point in this edition; nothing here
authorizes upload — that is a separate, explicit approval.

Inputs to this packet (analysis docs, read-only):

- `/tmp/l24-analysis/ANALYSIS.md` — l23 console-test deep-dive (root cause)
- `/tmp/l24-design/DESIGN.md` — l24 design (items 1-3)
- `/tmp/l24-sources/SOURCE-MAP.md` — l23 typescan source survey
- `/tmp/l24-trackb/OFFSETS.md` — Track B container-offset evidence
- `/tmp/l24-markers/MARKERS.md` — marker/rollback inventory
- `/tmp/l24-audit/L21-AUDIT.md` — l21 context and honesty rules
- `/tmp/l24-profile-thread/PROFILE-THREAD.md` — profile/thread parts study
- `/tmp/l24-packaging/PACKAGING.md` — packaging pipeline reference

Build/package receipts (all under
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/`):

- `2026-10-08-l24-runtime/` — sources (this directory)
- `2026-10-08-l24-build-r2/TWIN-RECEIPT.json` — twin runtime build
- `2026-10-08-l24-package/FINAL-RECEIPT.json`, `BUILD-REPORT.md`,
  `PREFLIGHT-TABLE.md`, `PACKET-RECEIPT.json`,
  `review-l24-20261008T182822Z.zip`
- Console evidence base (l23, read-only):
  `2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/`

---

## 1. ROOT CAUSE — the guard's proof source could never succeed on this console

Citation: `/tmp/l24-analysis/ANALYSIS.md` §1-§4 (evidence: klogs, UE4SS.log,
supplier JSONL, saved decrypted eboot). Summary of the proven chain:

**Observed (l23 launch, PID127, 2026-10-08).** Preflight passed all 50
startup-required keys
(`[LayoutPreflight] checked=66 startup-required=50 conditional-map=14 runtime-deferred=2 missing-required=0`,
UE4SS.log :493). The FName pool was admitted (pin `0x9765EC0`, 2665 ms,
'None' id 0x0 / 'ByteProperty' id 0x3 round-trip). Then the type scan threw
its terminal on the **first FField of the first scanned object**
(`/Script/Engine.Actor`): zero types and zero fields completed
(`type_scan_complete=false`, `type_scan_markers=0`, TYPE-SCAN.txt empty;
UE4SS.log jumps from `Initializing type system` (:597-599) to
`Stopping mod 'TownfallItemSpawner' for uninstall` (:600) within ≤1 s).
Terminal, verbatim, from runtime-status.txt (5 lines):

```
modforge-runtime-status 1
session 127 299445
state 1 activated 0
state 2 failed 1 [PropertyPointer] unreadable field or unknown property FField class refused
```

The game stayed alive (PID127, 8772.7 MiB, no crash in klog); teardown was the
controlled uninstall path. The runtime PRX itself was healthy:
`runtime_start_ok=0`, whole 194 MiB code image authenticated
(`code_image_cache_ok=1`, SHA + 3 function pins), imports 0 missing, pool
copyin ordinals 1-64 all errno 0.

**Proof chain (deterministic wiring bug, not a layout failure).**

1. `ReadClass` (l23 `NativePropertyPointerGuard.inl` :56-63) refuses unless
   `known_property_class(klass)` holds; disjunct 5 fires at its first
   sub-step: `image_class_range(klass, size)` =
   `SeedArming::PropertyImageClassRange`.
2. That callback built its mapping array **only** from
   `image->reported[0..reported_count)` and required
   `rc::imagecoverage::from_profile(...)->readable` →
   `live_coverage(maps, map_count, ...)`. With `map_count == 0`,
   `live_coverage` can only return false ("segment hit; current readable
   mapping coverage missing").
3. `image->reported[]` is filled exclusively inside
   `bridge_code_cache_prepare(..., module_info=true, ...)`
   (`lineage/lifecycle/runtime_start_bridge.cpp:1161-1171`), which requires
   `g_loaded_module_info` — a successful loaded-module-info query.
4. **Every launch section in aligned-entry.log reports
   `image_segment_source=2`** (= `g_loaded_module_info ? 1 : 2`,
   runtime_start_bridge.cpp:803; last five occurrences at lines
   42409/43494/44583/45672/46761, all = 2; 80 sections total). So the cache
   metadata was prepared with `module_info=false`, `reported_count=0`, and
   `PropertyImageClassRange` returns false for **every** address — valid or
   not. The module-info path has never been available in this environment.
5. Meanwhile the *segment* side of the proof was present and correct
   (`image->ready`, `image->base`, all 5 PT_LOADs), and this capture's real
   VM records existed (`image_segment_mapping_count=0x40` = 64 spanpub
   records). Only the guard's copy of the policy omitted them: the correct
   fallback **already existed in the same binary** — `bridge_image_range()`
   (runtime_start_bridge.cpp:807+) uses module-info when obtained, else falls
   back to `from_profile(rcprofile::kLoadSegments, …, g_spanpub_records,
   g_image_mapping_count)`. `PropertyImageClassRange` duplicated a stricter
   module-info-only subset and never consulted the spanpub records.

**Hypotheses refuted by the saved binary** (decrypted eboot, sha256
`3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`):

- Heap-FFieldClass hypothesis — refuted: all 86 constructor call sites of the
  FFieldClass descriptor (ELF vaddr 0x363990) place built-in descriptors as
  static image objects in the RW PT_LOAD vaddr [0x913C000, +memsz 0x4F7418]
  (≈ 0x939E3F0-0x93BA318; FProperty's descriptor at 0x93A7530, getter 0x3E1CD0).
  The design goal "class pointer inside the authenticated data/BSS PT_LOAD" is
  satisfiable.
- CastFlags hypothesis — refuted: derived classes accumulate
  CASTCLASS_FProperty (0x8000) (observed flags 0x8001, 0x208001, 0x4008001,
  0x14008001, 0x24008001, ...), so the strict gate's bit test would pass.
- Name-list hypothesis — unlikely first failure: pool admitted with verified
  ByteProperty round-trip this boot.
- What this is NOT (ANALYSIS §4): not a layout/offset failure (50/50 preflight);
  not the GATE-1 copyin problem (pool admitted, errno 0 ×64); not
  KeyProp/ValueProp/ElementProp absence (conditional/lazy, would surface as
  `[TypeScan] missing-layout-key=` warnings that never ran); not a game crash
  or kstuff issue (kstuff already disabled; module quarantined); the
  `[R3]`/`[WalkRxGuard]`/vtable-fallback "refused"/"failed" lines are
  pre-existing benign paths.

**Why it killed the whole scan**: `MissingKeys::attempt` absorbs only the
exact legacy missing-member diagnostic; the `[PropertyPointer]` throw
propagated through the per-field lambda, the object loop and the top-level
wrappers, hit the initializer's post-arm `catch(...)` (UnrealInitializer.cpp
:2429), revoked the guard, and UE4SS uninstalled the mod.

## 2. THE FIX — route PropertyImageClassRange through bridge_image_range

The minimal, faithful option from ANALYSIS §5.1 was taken: the guard's
callback now delegates to the bridge's existing full policy instead of
duplicating a module-info-only subset. As built
(`typescan-r3/include/NativePropertyReadBackend.inl`, l24 sha256
`d3d088b061daec39d4ac2017f0b2706352db0aa669ba2a43214ffda50b094525`):

```cpp
static bool PropertyImageClassRange(uintptr_t address,size_t length) {
    // Primary: route through the bridge's existing policy (module-info when
    // obtained, else PT_LOAD + spanpub VM records). Same fail-closed
    // semantics — no proof means refuse.
    rc::imagecoverage::Hit hit{};
    if (bridge_image_range(static_cast<uint64_t>(address), length, &hit) != 1) return false;
    if (!hit.readable || bridge_copyin_range_faulted(address, length) != 0) return false;
    // Secondary: the same admission-coverage proof as l23 (budgeted VQ +
    // fault-ledger re-check). This is the post-range proof that the bytes
    // are actually readable, independent of the range source.
    PropertyReadOps ops; admission::Budget budget(ops.now_ms(),1000);
    return admission::coverage(ops,budget,address,length).verdict==admission::Verdict::Readable;
}
```

Removed: the `image->reported[]`-only mapping construction, the
`reported_count<=4` truncated array, and the duplicate
`bridge_code_cache_context` re-derivation. Retained: fail-closed semantics
(no proof ⇒ refuse), the fault-ledger veto, and a fresh budgeted VQ
readability verdict. `bridge_image_range` was already declared
`extern "C"` in UnrealInitializer.cpp:778-785 and is internal to the composed
PRX — hence **no new OS import** (`new_os_imports: []`, raw undefined census
and 207 exact import identities unchanged; section 5).

Why this is sound for the strict (post-scan) identity check: the binary
evidence (§1) shows genuine FFieldClass descriptors are image statics inside
the RW PT_LOAD, and the spanpub records captured this launch (64 mappings)
cover exactly those pages. The check is not weakened — the missing proof
source is supplied.

Watch item retained from ANALYSIS §5.6: the l23 callback's `Mapping[4]`
truncation is gone entirely; `bridge_image_range`/`live_coverage` walk the
full spanpub count.

## 3. DEFENSE-IN-DEPTH — bootstrap-safe class check + refusal collection

As built in `typescan-r3/include/NativePropertyPointerGuard.hpp` (l24 sha256
`4e7dc22f9977f466e718b98bb13b28f3ed8945ffeea57c1f6d86155c3ca66046`) and
`.inl` (l24 sha256
`69075e733bf6b9209beff48f42cafd8db3c9af49a2ebb57c757f241eb051056a`):

**3.1 Refusal taxonomy + bounded collection.** New `RefusalReason` enum
(`SlotRead`, `ClassNull`, `ClassMisaligned`, `ClassIdentity`) with
`reason_text()`; `RefusalRecord{reason, field_address, class_pointer,
object_name[64]}`; `RefusalLog` with capacity 50 + `dropped` counter (no
allocation; static-size records). New scan-mode API:
`BeginScanCollection(RefusalLog*)`, `EndScanCollection()`, `ScanModeActive()`.
New `noexcept` checked entry points that classify the failure via
`classify_class_refusal` (property-readable/slot-read → null → misaligned →
identity) and record instead of throw: `ReadClassChecked`, `ReadNextChecked`,
`ReadPointerChecked`.

**3.2 Phase-dependent class admission.** `known_property_class` now
dispatches on `scan_mode`:

- **Bootstrap check (during scan)** — `KnownPropertyClassBootstrap`: guard
  enabled + `FFieldClass::UEP_TotalSize` bounds + a readability probe of the
  class through `read_bytes` (the VQ-based backend, which legitimately admits
  heap), then the class `FName` is read via the safe reader and accepted iff
  its display text ends in `"Property"` (`fname_ends_in_property`, via
  `FName::ToString` through the already-verified pool, ANSI/wide both
  handled) **or** it exactly matches one of the 39 known FField class names
  (the l23 list, compared via `FNAME_Find` with no interning side effects).
  No CastFlags requirement, no image-range requirement during the scan
  (classes may still be initializing; CastFlags would be premature).
- **Strict check (after the scan)** — `KnownPropertyClassFull`: the l23
  `known_property_class` verbatim (image-range via the *fixed*
  `PropertyImageClassRange` + FName/CastFlags reads +
  `CASTCLASS_FProperty` + exact `FNAME_Find` list match). Post-scan/Lua-time
  consumers therefore keep full-strength admission.

**3.3 TypeChecker integration** (`typescan-r3/candidate/src/TypeChecker.cpp`,
l24 sha256 `dcebc6e607a6effafff9bb5a75bd6395d49b86fe86dfcc1e9c3ea794947996cf`):
a function-scope `RefusalLog` (line 84), `BeginScanCollection` immediately
before the first `find_all_property_types` call (line 667), `EndScanCollection`
after the scan list (line 834), then — before the missing-keys print — new
harvest lines when anything was recorded:

```
[TypeScan] refusals collected=N dropped=M
[TypeScan] refusal i/total reason=<slot read|class null|class misaligned|class identity> object=<name> field=0x… class=0x…
[TypeScan] refusals-truncated stored=50 total=T
```

The core gate (`require_type` list, `[TypeScan] scan-complete missing-keys=N
core-ready=B`, stop before Lua only when `core_ready==false`) is unchanged.

**3.4 Honest as-built notes for review** (facts, not conclusions):

- The scan loop itself (TypeChecker.cpp :200 `ReadNext`, :181
  `ReadClass`/`NativeBootstrapFieldClass`) still calls the **throwing**
  variants. During scan mode those calls succeed via the bootstrap check;
  the `…Checked` collect-and-continue API is present and compiled but not
  called by any candidate in this edition. Consequence: if a class is still
  refused after the fix, the terminal is byte-identical to l23's generic
  refusal (the `RefusalLog` stays empty because nothing records). The
  machinery exists for a follow-up revision to wire in; it is inert as
  shipped.
- `RefusalRecord::object_name` is never populated (no SetScanContext was
  implemented; the Checked API passes `nullptr`), so `object=` in the print
  would be empty if records were produced.
- The Checked entry points are `noexcept` but call `layout_offset`, which
  throws on a missing layout key — a missing `FField::ClassPrivate`/`Next`
  key would terminate rather than throw. Both keys are startup-required and
  were PRESENT in the l23 console preflight, so this is unreachable in the
  observed environment, but it is a real contract change for the Checked API.
- `EndScanCollection` is not exception-safe: a throw between Begin and End
  (e.g. the cyclic-chain refusal) leaves `scan_mode` set with a dangling
  `scan_log`. Mitigated in practice because that path already fails startup,
  revokes the guard (`enabled=false` makes all reads fail) and uninstalls the
  mod; no re-install exists.
- Design deviations from `/tmp/l24-design/DESIGN.md`: the bootstrap check
  drops the design's acceptance conjunct (a) image containment (heap is
  admitted when readable + name matches); the design's no-engine-call pool
  resolver (`PropertyResolveFName`) was not implemented —
  `fname_ends_in_property` uses `FName::ToString()`; the
  `NativeTypeScanPolicy.hpp` recorder-parameter change was not needed and
  that header is byte-identical to l23; the "bootstrap-accepted" warning
  audit line was not implemented.

## 4. CONTAINER OFFSETS — designed, verified, and deliberately NOT promoted in l24

This is the one item where the shipped edition is narrower than the design;
the packet states it plainly so nothing is over-claimed.

- **Evidence in hand (two-source)**: Track B generic-platform compiler
  measurement (`/tmp/l24-trackb/OFFSETS.md`; receipt
  `2026-10-08-l23-layout-generic/CONTAINER-RECEIPT.json`, sha256
  `887062025404cf5b4271890e22a26a5bd7532548ce99fc917d0cd92bfbce4626`):
  `FMapProperty::KeyProp=0x70`(112,w8), `FMapProperty::ValueProp=0x78`(120,w8),
  `FSetProperty::ElementProp=0x70`(112,w8), calibrated by
  `FArrayProperty::Inner=0x78` — each agreeing exactly with the independent
  static image evidence (`image-measurements.json` against eboot
  `3b6cfad4…`). Compiler = genuine prospero-clang++ 21.1.8 wrapper
  (sha256 `8501bb9d…`), pinned Epic `UnrealType.h` at commit
  `6978b63c8951e57d97048d8424a0bebd637dde1d` (sha256 `cc16f8ec…`), ELF
  symbol readback (not text parsing), 165/165 member crosscheck.
  Platform-independence is empirically corroborated: the audit's deliberately
  wrong lock model shifted every lock-bearing record (6 calibration
  mismatches are exactly the two-lock FUObjectArray members + IRIS slot)
  while all four container offsets still matched the PS5 image — FProperty
  containers hold no platform-varying storage. Class sizes corroborate:
  Map 160 / Set 144 / Array 128.
- **The UEP_TotalSize fix requirement** (DESIGN §3.2):
  `NativeLayoutBinding.cpp::Prepare` throws
  `Native member class requires positive UEP_TotalSize` for any non-empty
  member section without a class total, and validates
  `offset + width <= UEP_TotalSize`. Promoting only the three member offsets
  would have crashed startup staging. A correct promotion must also ship
  `FMapProperty::UEP_TotalSize=0xa0`(160) and `FSetProperty::UEP_TotalSize=0x90`
  (144) with their own two-source provenance (image: named-constructor
  allocation sizes; Track B: compiler `sizeof`), update
  `generate_overlay.py`'s join to accept the Track B receipt
  (`GENERIC_PLATFORM_COMPILER_MEASURED` route), emit the totals in the INI
  sections, and flip the three `JoinedLayoutPreflight.hpp` rows to
  pinned 112/120/112 (still `Disposition::Conditional`).
- **What l24 actually shipped: nothing.** `2026-10-08-l24-runtime/profile/CHANGES.json`
  is a placeholder with `files: []` ("profile part unchanged from l23");
  the l24 PREFLIGHT-TABLE.md states Map KeyProp/ValueProp and Set ElementProp
  remain conditional/unpinned and absent from the 93-fact declarations;
  BUILD-REPORT.md: "no new container offset shipped"; profile digest stays
  `53ed7c04591fc1e10a13209527c111abd75eb9f6a478bfb813d9c293fb6ded8f`.
- **Implication**: Map/Set nested inner-property discovery stays lazy
  (missing-key collectable, `[TypeScan] missing-layout-key=`), exactly as in
  l23. This does not block the l24 goal: the l23 console stop was the
  PropertyPointer/class-recognition guard (§1), not the container keys —
  those are conditional and never fired. The Track B promotion remains a
  ready, receipt-backed candidate for a future edition that also lands the
  generator totals fix.

## 5. BUILD RESULTS — twin compile identical, imports unchanged

Pipeline per `/tmp/l24-packaging/PACKAGING.md`, executed Mac-only with
`scope LOCAL_COMBINED_PACKAGE_ONLY`, `console_actions_authorized: false`.
BASE = `2026-10-08-l23-build-r2` (l23 twin runtime, 374 selected units).
Parts: `profile` (empty), `typescan-r3` (12 rows + 4 added headers),
`thread` (empty).

**Runtime twin build** (`2026-10-08-l24-build-r2/TWIN-RECEIPT.json`,
status `MAC_ONLY_IDENTICAL_L24_RUNTIME_PAIR`):

- `runtime-l24.o` sha256
  `c25130952d67f7b519cfa03df2143c2f9676f9ef469d23a1e866402c52152dec`,
  119,827,176 bytes — **byte-identical across pass1/pass2** (l23 was
  119,807,336 B; +19,840 B of guard code).
- 125 fresh units recompiled per pass, 249 retained, 374 selected, exactly
  391 link inputs; every per-unit object hash equal across passes (verified
  over the full 125-unit lists, incl. `unreal-flat-typechecker`
  `4a1008fa…`, `unreal-unrealinitializer` `3eda7d5f…`,
  `unreal-src-property-fmapproperty` `ed955b2c…`, `-fsetproperty`
  `37e55321d…`, `-farrayproperty` `718a1aa5…`, `lua-custom-property`
  `2ab4e34c…`, `luamod` `78198b02…`, `program` `95115165…`).
- The compiled l24 `unreal-flat-typechecker.o` contains the new
  `KnownPropertyClassBootstrap` symbols; the l23 object contains none — the
  built runtime provably carries the l24 changes.

**Imports unchanged** (`pass1/IMPORT-DELTA.json`, mirrored in the zip's
`twin/`): raw undefined census 212 == 212 baseline (added `[]`, removed
`[]`, baseline pinned to l23-build-r2 `undefined.stdout`
`b0fd9a33…`). FINAL-RECEIPT: `imports_unchanged: true`, exactly **207**
exact ELF import identities per pass, row-for-row equal to the l23 package
baseline (`2026-10-08-l23-package/pass-1/EXACT-IMPORT-IDENTITIES.json`),
`require_getter` on. No new OS import (`new_os_imports: []`) — consistent
with the fix reusing the already-declared internal `bridge_image_range`.

**Packaging** (`2026-10-08-l24-package/FINAL-RECEIPT.json`, status
`MAC_ONLY_L24_TWINS_AND_PACKAGE_CHECKS_PASS`):

- Both passes produced byte-identical PRXs: sha256
  `2e712a8b0e1ffc2b8c12af6e5b7a7a8fe63fe6e6a5c24d113655faebb79c92f4`,
  31,331,783 bytes; SELF magic `4f153d1d`; SDK stamp `0x11000000`.
- 20/20 static gates each pass (`pass-{1,2}/STATIC-CHECKS.json`, zero gaps;
  includes elf/fself magic, embedded-header/payload-digest/segment equality,
  single PT_SCE_MODULE, explicit DT_INIT/DT_FINI, loader-owns-no-arrays,
  eh_frame set, imports_equal_extended_contract, entry alignment evidence).
- Constructor/destructor arrays preserved: 238 init / 2 fini slots
  (unchanged vs l23).
- Checker self-tests 11/11 OK (`checker-tests.log`).
- Review zip `review-l24-20261008T182822Z.zip` (9,680,880 B, sha256
  `edf4e814494a3b30d70b2012cee9e0811967e90e9059df9854c89889c628fa56`;
  `PACKET-RECEIPT.json`: `console_contact:false`, 26 manifest files; the
  archive carries the 4 unified diffs, the PRX artifact, both passes'
  receipts and the twin/import-delta evidence).

## 6. CHANGED FACTS — exactly 4 files differ from the l23 sources

The whole source delta vs the l23-r2 candidates the BASE composition
selected (12 tracked rows; the other 11 replacement rows are byte-identical
carries, and the added header `NativeTypeScanPolicy.hpp` is also unchanged —
`diffs/IDENTICAL-CARRIED-FILES.txt`):

| File (under `2026-10-08-l24-runtime/typescan-r3/`) | l23 sha256 | l24 sha256 | Change |
|---|---|---|---|
| `include/NativePropertyReadBackend.inl` | `96179b2dbbb8b8a7fdae8941696d5a757b6d53f3a4b0ea08d31d839b71c4a112` | `d3d088b061daec39d4ac2017f0b2706352db0aa669ba2a43214ffda50b094525` | `PropertyImageClassRange` routed through `bridge_image_range` + spanpub fallback (§2) |
| `include/NativePropertyPointerGuard.hpp` | `f64577fa6f384f0bbabbad795bb653b96981e4a7605d3a6f45ce176a7551db5c` | `4e7dc22f9977f466e718b98bb13b28f3ed8945ffeea57c1f6d86155c3ca66046` | Refusal taxonomy, `RefusalLog`, scan-mode API, Checked declarations |
| `include/NativePropertyPointerGuard.inl` | `ede64b2c697050d7223058e97a66bc36fa001cb386610cfd1ed17cd445fe916c` | `69075e733bf6b9209beff48f42cafd8db3c9af49a2ebb57c757f241eb051056a` | Bootstrap/strict class-check dispatch, refusal classification, Checked bodies |
| `candidate/src/TypeChecker.cpp` | `d7b934a51d0b5788515f0d7575a7832fd433dfc2e0f2690225da8d65b57a434a` | `dcebc6e607a6effafff9bb5a75bd6395d49b86fe86dfcc1e9c3ea794947996cf` | Scan-lifetime `RefusalLog` + `Begin/EndScanCollection` + refusal printout |

Unchanged in the same overlay: `include/NativeTypeScanPolicy.hpp`
(`ac06c50c2fdc68ca1b83060e5942b1a1129c68bf27d572c515956f7c6d1acdc7`),
`candidate/src/UnrealInitializer.cpp`
(`78c16fb7c06a457e0b179e8564cc96baf1c86313fb6007e2fdb8735e76146321` — the
2-arg `Install` call site is untouched), `candidate/src/CustomProperty.cpp`
(`66be0042102f1140d38bb0f305a21a3b6791394c51a31dd0b4e02471e33ef938`), and
the 6 generated-include wrappers + 3 Property cpps (see IDENTICAL list).
Thread/profile parts: `files: []`.

**Known metadata staleness (disclosed, cosmetic-but-reviewable):** in
`typescan-r3/CHANGES.json` only the operative `files` array carries the
recomputed candidate hash for TypeChecker.cpp; the duplicate `changes` array
still holds the placeholder hash, `added_header_hashes` still holds the l23
hashes for the 3 edited headers (live values above), the `scope` string
still says "changes not yet landed", and `typescan-r3/REPORT.md` is still
the l23-r2 text. The build driver gates on the `files` array and the
live-file hashes (BUILD-PLAN §4), so the built artifact is correct — the
symbol evidence in §5 proves the edited sources were compiled — but the
manifest text is inconsistent and should be refreshed before any upload.
`2026-10-08-l24-runtime/BUILD-PLAN.md` §5 items 5 (hash/report refresh)
were therefore only partially completed.

## 7. TEST PLAN

**7.1 Host suites that exist** (`typescan-r3/tests/`, run via `run.py`):
five C++ suites each built and run in plain + fatal ASan/UBSan modes against
`typescan-r3/include` and the l19-profile-r4 units — `actual_scan.cpp`
(real scan-body extraction incl. guard/core gate), `actual_getters.cpp` +
`actual_pointer_getters.inc` (mock-callback getter refusals),
`actual_backend.cpp` (mocked codecache/imagecoverage over
PropertyReadBytes/PropertyImageClassRange), `test_policy.cpp`
(MissingKeys/checked_pointer), `actual_custom.cpp` (owned-array writer) —
plus `test_scan.py`/`test_custom.py` drivers with `--unguarded/--original`
baselines and the RED logs (`RAW-NEXT-RED.log`, `CUSTOM-ASSIGNMENT-RED.log`)
quarantined under `results/inherited-l23-r2/`.

**7.2 Status for l24 — honest statement**: the host suites were **not**
re-run after the l24 edits landed. Test binaries and sources carry mtimes
12:58 (placeholder era); the edits are 13:07-13:17; no fresh
`HOST-TESTS.json`/`TARGET-COMPILE.json` exists under `results/` (only the
quarantined l23 evidence, which must not be presented as l24). The design's
new cases — the production-shape fixture (`module_info=false,
reported_count=0`), refusal-attribution cases, bootstrap-admission cases,
resolver cases — were not added (test sources are byte-identical to l23).
What was freshly machine-verified for l24 is the full target-compile and
link chain: the twin build compiled all 125 affected units (including every
l24-touched TU) with exit-0 assertions and byte-identical results, plus the
packaging gate battery (§5). The design's host-test items (DESIGN §5.1-5.3)
remain open work; the launch evidence in 7.3 is the acceptance path for the
fix itself.

**7.3 Launch test (only after approval + explicit user authorization; PS5
rules apply — decrypt-off, fake-sign already applied by make_fself, single
upload, no rapid-fire, kstuffSharp stays quarantined, ploader remains last
in autoload.txt).**

Pre-upload: fresh decrypt-off (`SITE SELF`) backup/readback of the live PRX
— currently l23's `0da199eb954529bbe8fdc563a8c58d76853d560feba0230ad2fd4374b3f0d4e1`
(31,324,743 B, verified live in the l23 RUNTIME-READBACK.json) — then a
single upload of the l24 PRX `2e712a8b…`, and before launching anything,
decrypt-off readback proving magic `4f153d1d` + full sha256 match. Rollback =
re-push the l23 PRX with the same verification.

Harvest reading order (MARKERS §6): supplier-diag bootstrap tags →
aligned-entry pad/settings tags → UE4SS.log `[BuildProfile]`/66-key
LayoutPreflight table (missing-required=0) → pool admission + FName
round-trip → `Initializing type system`. **The decisive new acceptance
signal for l24** (never seen in any prior launch): `[TypeScan]
missing-layout-key=…` lines (if any) and
`[TypeScan] scan-complete missing-keys=N core-ready=1`, then
`[l22] Lua mod started: TownfallItemSpawner (main script returned)` →
`[l23] game-thread self-test queued via ExecuteInGameThread …` → keybind
registrations → on L3+R3 the pad marker family and
`[l22] keybind callback entry …` → queue drain / SCO / AddToViewport
markers. Interpretation of the new lines: `refusals collected=0` (or absent)
on success; any `refusal … reason=… field=0x… class=0x…` line names the
exact failing check. Honest failure mode: because the scan loop still uses
the throwing variants (§3.4), a residual class refusal aborts with the same
generic l23 terminal and an empty refusal log — if that occurs, the next
step is a bounded revision wiring the Checked variants into the loop, not a
re-upload of this PRX. Terminal health check: runtime-status.txt
`state 1 activated …` vs `state 2 failed 1 <text>`; game-alive check via the
process endpoint; one launch per boot cycle.

## 8. NOT CHANGED — everything else from l23 is carried intact

- **Game-thread self-test** (`L23GameThreadProbe`): unchanged; thread part
  `files: []`. Same state machine, production-API enqueue, genuine-thread
  acceptance, 30 s one-shot timeout, Poll in the event loop, Cancel at
  uninstall; host harness `thread/tests/queue.cpp` untouched. `LuaMod.cpp`/
  `UE4SSProgram.cpp` sources are byte-identical to l23 (they were recompiled
  only because they sit in the aliased header closure).
- **l22 markers / `L22Diagnostics`**: all `[l22]` markers and wrapper
  semantics (observe/error/throw_lua_error, AddToViewport identity reuse,
  best-effort logging that never replaces results) byte-identical.
- **Collect-and-continue layers**: LayoutPreflight collect-all-then-refuse
  (conditional keys continue; FText stays deferred; seed-bank retirement on
  refusal) and TypeScan missing-key collection with the `scan-complete`
  summary — unchanged; the new refusal printout is additive and sits before
  the missing-keys print.
- **Profile**: 93 facts (69 members + 24 vtables) byte-for-byte; digest
  `53ed7c04…`; JoinedLayoutPreflight 66/50/14/2;
  `conditional_legacy_absence_exceptions` unchanged; no new offsets, no
  container promotion (§4).
- **Guard contracts**: single-shot 2-arg `Install` (same call site,
  UnrealInitializer.cpp:1645), `Revoke` on every failure path,
  `parent_shutdown_requirement` (Program revokes after engine consumers
  retire), exact legacy missing-key diagnostic as the only absorbable text,
  no raw dereference, no discovery-map insertion — all unchanged. The
  CustomProperty `SetOwnedArrayInner` owned-writer seam is unchanged.
- **Mod files**: main.lua byte-identical Nexus release (21,415 B, sha256
  `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`,
  never edited); PS5Pad.ini, UE4SS-settings.ini, plugins-list untouched; the
  L3+R3 → F7 rebind (OPTIONS = game pause) unchanged.
- **Diagnostic channels** 1-5 (UE4SS log, `diagnostic_event` aligned-entry
  sink + probe_config.h, supplier JSONL, runtime-status.txt, klog) and the
  pad-tag vocabulary — unchanged.
- **Rollback/change-control infrastructure**: l23's before/-snapshot
  pattern, decrypt-off verification, unchanged-files manifest, checkpoints —
  to be repeated for any l24 upload (§7.3).
- **Scope walls**: no console contact, no upload/arming/launch, no
  mod-manager edits, no edits under l23-* directories, no new OS imports, no
  kstuffSharp/autoload changes.

## 9. Open review items (summary for Claude)

1. The core fix is minimal and matches the proven root cause exactly (§1-§2);
   the binary evidence says the strict check becomes satisfiable once the
   mapping proof exists.
2. Defense-in-depth landed partially: bootstrap check + refusal taxonomy are
   in; the Checked collect-and-continue API is compiled but unwired, so
   residual refusals still abort with the generic terminal (§3.4). Accepting
   this edition means accepting "fix first, wire diagnostics on next failure".
3. Container offsets deliberately not promoted; the UEP_TotalSize generator
   gap is the documented reason members-only promotion must not ship; Track
   B receipt remains ready (§4).
4. Manifest staleness in `typescan-r3/CHANGES.json` (duplicate `changes`
   array, `added_header_hashes`, `scope` text) and the un-rewritten
   `REPORT.md` should be refreshed before upload (§6).
5. Host suites were not re-run post-edit; the fresh machine evidence is the
   twin target compile + packaging gates (§7.2). The design's new host cases
   (production-shape `module_info=false` fixture especially) are open work.
6. No console claim is made by this packet: queue delivery, Lua start,
   menu/spawn, and the actual effect of the fix remain unproved until the
   authorized launch harvest in §7.3.

— End of packet. Mac-only; no console contact; upload not authorized by this
document.
