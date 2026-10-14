# Agent CY — walk-finds-nothing fix templates (pre-staged, l26)

Date: 2026-10-08. READ-ONLY trees; the only files written live under
`outputs/2026-10-08-l26-runtime/fix-templates/` (8 files) plus this report.
CR/CT/CU/CW/CS audit reports had NOT landed in this directory at authorship
time (verified by listing); DECISION.md binds their verdicts to templates
with fill-later slots. CO's `agent-CO-walk-audit.md` is the shared baseline.

## Deliverables

```
outputs/2026-10-08-l26-runtime/fix-templates/
  README.md                              consumption law, sha anchors, shared verification assets
  DECISION.md                            audit verdict -> template matrix (+ arbitration rules)
  candidate-A-objectflags/TEMPLATE-A.md  + values.example.json
  candidate-B-canonical-class/TEMPLATE-B.md + values.example.json
  candidate-C-name-compare/TEMPLATE-C.md   + values.example.json
```

Each template carries the four required elements: (1) exact files+lines
where the read gets its value NOW, (2) fill-in-the-blank diffs with
`<<SLOT>>` VALUE slots (+ values.json as the single typed-numbers authority,
AS/AM law), (3) the verification command set, (4) CHANGES.json row shapes
per the consumption law verified in `build_runtime.py` (parts
profile/typescan-r4/thread; original/candidate/sha pins; profile digest in
three places; boot preflight refuses on layer mismatch).

## Key tracing findings (what decides which template fires)

1. **A — the HasAnyFlags ObjectFlags read is HARDCODED, not profile-driven.**
   Filter at l26 candidate `src/UObjectGlobals.cpp:267-271` →
   `UObject::HasAnyFlags` (PORT `UObject.hpp:267-270`) → VC = `UObject425`
   for 5.6 (`Container.cpp` version dispatch) → `UObject425.cpp:23-26` →
   `Flags425` identity map → **`unreal/include/Unreal/VersionedContainer/Flags.hpp:11-15`
   `ptr_cast(p_this, 0x8)` with the "TODO: Hard-coding this" comment**. The
   profile `[UObjectBase] ObjectFlags=0x8` row feeds only the generated
   `GetObjectFlags()` accessor, which this path never calls. So an
   ObjectFlags-offset verdict requires the A-CODE variant (offset override
   or routing through the generated accessor); the A-PROFILE variant applies
   to the genuinely profile-driven members (FUObjectItem::Flags/Object/
   UEP_TotalSize, TUObjectArray::*, ClassPrivate/NamePrivate/SuperStruct —
   all via MemberOffsets + NativeLayoutBinding publication, schema rows
   already present).
2. **B — the canonical seed is name-walk-resolved with a non-nullity-only
   gate.** `UClass::StaticClassStorage` is seeded at l26 candidate
   `src/TypeChecker.cpp:165-166` via `StaticFindObject_InternalSlow`
   (GetFullName STRING walk, `src/UObjectGlobals.cpp:184-206`); the core
   gate (`TypeChecker.cpp:979-1017`) checks presence only, so a wrong-but-
   nonnull canonical poisons all five `IsA<T>` filters silently. Lazy
   fallback (`UObjectMacros.hpp:149-167`) throws on null (loud), not on
   wrong. B1 identity gate is the default fix; B2 init-order, B3 pinned
   seed slot (schema row + image-SHA gate), B4 FName-parts seeding variants
   included.
3. **C — compare sites + Equals semantics pinned.** Six compare sites in
   candidate `src/UObjectGlobals.cpp` (:552, :563, :640, :648, :690, :700);
   NamePrivate is PROFILE-driven (MemberOffsets["NamePrivate"]=0x18,
   schema + preflight rows confirmed); `FName::Equals`
   (candidate `include/Unreal/NameTypes.hpp:354-360`) compares
   ComparisonIndex ONLY, `operator==` (:334-337) index+number, DisplayIndex
   exists only under WITH_CASE_PRESERVING_NAME (:227-231). CP's
   MakeFindXOfClassName (:517-526) already landed Find-first; C1/C2/C3 cover
   offset, slot-tolerant/string compare, and number-form residuals.

## Verification assets wired into all templates

- `/tmp/l26-ufunc-harness/` (CD's verbatim-region host harness; regions
  already include is_valid_object_for_findxof / findfirstof / fname_equals /
  staticfindobject_slow) — the walk harness each template extends with a
  reproducing fixture + negative control.
- `outputs/2026-10-08-l26-runtime/profile/tests/run_tests.py` (profile
  variants), `outputs/2026-10-08-l26-runtime/tests/run_tests.py`,
  `outputs/2026-10-08-l26-runtime/build_runtime.py` (compile+link proof,
  re-verifies every CHANGES row sha).

## Consumption law summary (verified in build_runtime.py source)

Rows must key on the ORIGINAL the base l25-A build consumed (PORT path for
never-overlaid files, prior-tree candidate for overlaid ones; e.g. candidate
UObjectGlobals.cpp's original is `outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp`),
candidates must live under the l26 part dirs, header shadowing is by
(basename, original-sha) vfs aliasing, and any profile edit must move all
five layers + digest together or startup preflight refuses (fail-closed).

— agent CY (ZCode), 2026-10-08. Read-only; only fix-templates/ + this file
written.
