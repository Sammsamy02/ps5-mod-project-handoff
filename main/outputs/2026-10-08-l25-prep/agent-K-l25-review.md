# Agent K — l25 pre-review (read-only quality pass)

Tree reviewed: `outputs/2026-10-08-l25-runtime/typescan-r4/` (l24 copy + l25 edits).
Parallel worker dirs `../profile` and `../tests` (items 5/6) exist with fresh content
(`generate_overlay.py`, `test_scan_refusals.cpp`, RESULTS.json); not reviewed per
instructions. No file in the reviewed tree was modified; no console/network contact.

**VERDICT: ISSUES FOUND** — all six claimed changes are implemented as described and
hashes verify; no blocker found; one MEDIUM safety-property regression and several
LOW/INFO items below.

## Claim verification (all six claims hold)

1. **Checked-read wiring — VERIFIED.** Scan loop reads the class and next-field through
   `ReadClassChecked`/`ReadNextChecked` with no `missing_keys.attempt` wrapper:
   `candidate/src/TypeChecker.cpp:285-294`. `NativeBootstrapFieldClass` was rewritten to
   `ReadClassChecked` + name stamping (`TypeChecker.cpp:247-252`) and is used for the
   array inner (`:493`), set element (`:532`), map key (`:572`) and map value (`:599`)
   reads. `ReadPointerChecked` (`.inl:309-318`) remains unused in production — nothing in
   the scan still calls a throwing guard wrapper; remaining throwing accessors
   (`GetChildProperties`, `GetSize`, `GetSuperClass`, `GetInner` etc.) throw only
   missing-key diagnostics, which stay inside `missing_keys.attempt` (`:300`, `:488`,
   `:527`, `:567`, `:594`).
2. **Per-object catch — VERIFIED.** `try` opens at `TypeChecker.cpp:268` before
   `GetChildProperties()`; `catch (const std::runtime_error&)` at `:756-768` rethrows
   anything not starting with `[PropertyPointer]` (`:763`, `rfind(...,0)!=0`), so the
   cyclic-chain refusal (`:276`, `[TypeScan]` prefix) and all missing-key diagnostics
   (plain text, `NativeTypeScanPolicy.hpp:12-21`) stay fatal/collectable exactly as l24.
   `Install`'s `std::logic_error` (`.inl:270`) is not a `runtime_error`, so it can never
   be absorbed. Trailer parse `TypeChecker.cpp:50-82`: guard messages always emit the
   four markers in fixed order (`reason → object → field → class`,
   `NativePropertyPointerGuard.inl:92-106`); reason values come from a fixed
   space-containing vocabulary but are delimited by the next marker position, not by
   spaces; `object=` values are layout keys (C identifiers: `Inner`, `KeyProp`, …) or
   `-`; the fixed message prefixes contain none of the marker substrings. Unambiguous
   for every message the guard generates.
3. **ScanCollectionScope RAII — VERIFIED.** Declared
   `include/NativePropertyPointerGuard.hpp:55-61` (Begin in ctor, End in dtor, deleted
   copy/move); used once at `TypeChecker.cpp:779` inside the `IsAtLeast(4,25)` block, so
   it closes on normal exit and on any unwind; the l24 manual `EndScanCollection()` call
   was removed (comment at `:945-948`); refusal printing (`:949-963`) runs after the
   scope has closed, with the log still alive at function scope (`:128`). No double-End
   (single scope; `EndScanCollection` is idempotent anyway). This genuinely fixes an l24
   hazard: an escaping cyclic-chain refusal used to leave `scan_mode` active pointing at
   the destroyed function-scope log.
4. **Bootstrap order + pool bound — VERIFIED.**
   - Exact-match first: `KnownPropertyClassBootstrap` now does the pure index compare
     before any ToString (`NativePropertyPointerGuard.inl:203-204`), with indices
     pre-computed once via 39 `FNAME_Find` lookups in a magic-static
     (`.inl:112-131`); per-call work is 39 `uint32` compares (`.inl:132-138`) —
     allocation-free and lookup-free after warmup. `FNAME_Find` is lookup-only (no pool
     mutation) per the tree's own doctrine, cited at
     `candidate/src/UnrealInitializer.cpp:1108-1109`. Suffix runs only after
     `fname_index_within_verified_pool` passes (`.inl:206-207`).
   - Comparison-index semantics match the tree: the l23 full check already used
     `GetComparisonIndex().ToUnstableInt()` as the non-zero guard before `name==candidate`
     (`.inl:182`), and the initializer logs FName identities the same way
     (`UnrealInitializer.cpp:1915` etc.). (See L2 below for the Number nuance.)
   - Pool layout: the guard's constants (`.inl:23-27`: CurrentBlock u32 @ +0x8,
     CurrentByteOffset u32 @ +0xC, block = id>>16, stride 2, 8192 max) exactly mirror
     `rc_seed::kPoolLayout` in the lineage header
     `outputs/2026-10-08-l24-package/lineage/SeedValidation.hpp:60-73`
     (`{0x8, 0xC, 0x10, 0x14040, 0x14044, 16, 8192}`) and the id decomposition
     `entry_location` (`SeedValidation.hpp:92-97`, `(id & 0xFFFF) * 2`). The bound logic
     mirrors `check_entry_bounds` (`SeedValidation.hpp:555-577`): refuse `block >
     current_block`; within the open block refuse `offset + needed > CBO` with needed =
     2 (entry header) + 8 (suffix chars); unreadable words and `current_block >= 8192`
     fail closed (`.inl:146-161`).
   - Publication: `PublishVerifiedPoolAddress(pool_runtime)` at
     `candidate/src/UnrealInitializer.cpp:1105`, immediately after the pool-admission
     success logs (`:1094-1098`) and before the ctor-binding phase — i.e. on the real
     seeds path, in `consume_required_slot_seeds` (`:969`), which runs from `ScanGame()`
     (`:1365`) before `Install` (`:1652`) and before `TypeChecker::store_all_object_types()`
     (`:2351`). Single-shot first-wins CAS (`.inl:264-268`). Fail-open only while
     unpublished: `if (!pool) return true;` (`.inl:148`) keeps l24 behavior for host
     fixtures / pre-arming / the non-seeds AOB path (disclosed in the scope note).
5. **Throwing wrappers — VERIFIED.** All three now append the trailer:
   `ReadClass` `.inl:330-331`, `ReadNext` `.inl:338-339` and `.inl:342-343`, `ReadPointer`
   `.inl:352-353`. Refusal conditions are unchanged — `ReadClass` reaches
   `known_property_class` exactly on the `ClassIdentity` branch, which
   `classify_class_refusal` (`.inl:220-229`) only returns when the slot was readable,
   non-null and aligned, matching the l24 inline condition sequence.
6. **CHANGES.json / build script — VERIFIED.** Recomputed sha256 (8 files, exceeding the
   requested spot-check of 3): candidate TypeChecker `bb91f45d…`, original/copied TypeChecker
   `dcebc6e6…` (identical to the l24 tree file — provenance intact), candidate
   UnrealInitializer `c4364dd7…`, original UnrealInitializer `78c16fb7…`, guard hpp
   `a0c949bd…`, guard inl `c7217cbd…`, CustomProperty `66be0042…`, FMapProperty
   `68139f5e…` — all match CHANGES.json. `build_runtime.py:6-7`:
   `BASE=outputs/2026-10-08-l24-build-r2` (exists), `OUT=outputs/2026-10-08-l25-build-r2`
   (not yet created — consistent with "no twin build yet"). The scope note honestly
   discloses: host suites not re-run (confirmed: `tests/actual_scan.cpp` still has zero
   references to the Checked API / scope / publish — l24-era extraction), no target
   compile, no packaging, no console proof.

## Findings

**F1 — MEDIUM — noexcept Checked wrappers can still throw → std::terminate where l24
collected a missing key.**
`ReadClassChecked`/`ReadNextChecked`/`ReadPointerChecked` are declared `noexcept`
(`include/NativePropertyPointerGuard.hpp:86-88`) but all call `layout_offset`
(`NativePropertyPointerGuard.inl:29-34`) first, which throws on a missing member
(the plain missing-key diagnostic, `.inl:31`) or a negative offset (`.inl:32`), and
`classify_class_refusal → property_readable` also does an `FProperty::UEP_TotalSize`
lookup (`.inl:44`, used at `.inl:222`). l24 wrapped the scan's guard calls in
`missing_keys.attempt`, which converted the missing-member diagnostic into a collected
key and ended that object's chain (`NativeTypeScanPolicy.hpp:24-29`); l25 removed that
wrapping (`TypeChecker.cpp:285-294`), so a layout-map miss for
`FField::ClassPrivate`/`FField::Next`/`FProperty::UEP_TotalSize` at scan time now
escapes a `noexcept` function and terminates the process. In practice the path should
be unreachable — `PublishStagedNativeLayoutMaps` succeeds (or throws) before
`Install` (`UnrealInitializer.cpp:1626-1652`) and the L19 joined preflight gates
startup-required keys — but nothing in this changeset audits that those exact keys are
in the required preflight set, and the degradation behavior regressed from
"collect + continue" to "terminate". Recommended: catch-and-record inside the Checked
wrappers, or assert the keys at `Install` time.

**F2 — LOW — suffix-bound margin assumes ANSI entries.**
`fname_index_within_verified_pool` budgets `offset + 2 + 8 <= CBO`
(`.inl:159`); a wide-character entry whose display name ends in `L"Property"` needs
2 + 16 bytes. This matches the tree's established necessary-not-sufficient admission
doctrine (`SeedValidation.hpp:51`, parameterized `needed_bytes` in
`check_entry_bounds`), so it is a doctrine-consistent laxness, not a violation — noted
so the parent doesn't mistake the bound for a full safety proof.

**F3 — LOW — unparseable `[PropertyPointer]` messages get a fabricated reason.**
If a prefixed message without the trailer reaches the per-object catch (e.g.
`[PropertyPointer] negative layout offset refused`, `.inl:32`, or CustomProperty's
`[PropertyPointer] custom array Inner exceeds owned allocation`,
`candidate/src/CustomProperty.cpp:22`), `parsed=false` and the record is filed as
`SlotRead` with `field=0 class=0` (`TypeChecker.cpp:765-767`). Contained and
object-named, but the reason/addresses are invented rather than marked unknown.
Related robustness nit: `text.substr(reason_pos + 8, object_pos - (reason_pos + 8))`
(`TypeChecker.cpp:60`) would throw `std::out_of_range` from inside the catch if markers
ever appeared out of order — the guard never emits that, but the parser is not
defensive against it.

**F4 — LOW — index match ignores the FName Number half.**
`known_property_class_index_match` compares only the comparison index
(`.inl:132-137`), while the l23 full check used `name == candidate` (number included,
`.inl:182`). An FName with a known index but nonzero number would pass bootstrap.
Scan-mode bootstrap is explicitly heuristic (no CastFlags either), downstream
population compares full FNames, and FFieldClass singleton names never carry numbers —
theoretical only.

**F5 — LOW — redundant per-node class validation (performance).**
Each node's class is validated up to three times per scan pass: the loop's
`ReadClassChecked(field)` (`TypeChecker.cpp:290`), `ReadNextChecked`'s internal
re-validation of the same node (`.inl:295`), and the next iteration's
`ReadClassChecked` on the node just validated as `next` (`.inl:306`). Each validation
runs readability probes and FName reads through the VQ-proof backend
(`NativePropertyReadBackend.inl:20-27`). No correctness impact.

**F6 — INFO — kPoolLayout constants duplicated, not derived.**
The pool constants live twice: `.inl:16-27` mirrors
`SeedValidation.hpp:60-73` (values verified equal today). The guard header set is
shared with host fixtures that cannot include the initializer's headers, so the
duplication is deliberate (commented), but a future layout revision could silently
diverge — worth a cross-check at the l26 review if SeedValidation ever changes.

**F7 — INFO — ReadPointer message format changed.**
l24: `"[PropertyPointer] unreadable/null/foreign target refused: <key>"`
(l24 `.inl:244`); l25 drops the `": <key>"` tail in favor of the trailer with
`object=<key>` (`.inl:352-353`). No in-tree consumer greps the old suffix (searched
l24/l25 trees); external harvest/log tooling should be re-checked at integration.

**Positive notes.** The RAII conversion fixes a real l24 dangling-`scan_log`/
stuck-scan-mode hazard on escaping exceptions; the per-object catch preserves
cyclic-chain fatality and missing-key collectability; `PublishVerifiedPoolAddress`
single-shot CAS is correct and sequenced publish (1105) → Install (1652) → scan (2351)
on the seeds path; all guard reads fail closed after `Revoke` (the `read()` double
`enabled` check, `.inl:35-38`); no use-after-free, unsafe TOCTOU (pool cursor races
only make the bound conservative; the allocator is append-only), or uninitialized
read was found — all touched locals are initialized, and `probe[64]` is only a
readability sink, never evaluated.

## Checklist cross-reference

- Checked-read wiring: claim 1 verified; gap = F1 (terminate path).
- Catch containment + trailer parseability: verified; nits F3.
- RAII exception safety / no double-End / placement vs printing: verified.
- Bootstrap order, allocation/pool-mutation freeness, index semantics, pool layout vs
  `rc_seed::kPoolLayout`: verified with citations; nits F2, F4, F6.
- Fail-open disclosure + real-path publication: verified (`.inl:148`,
  `UnrealInitializer.cpp:1105`).
- UAF / TOCTOU / uninitialized: none found.
- CHANGES.json hashes: 8/8 recomputed match; build script constants verified.
