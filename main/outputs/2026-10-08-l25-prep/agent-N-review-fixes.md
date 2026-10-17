# Agent N — l25 review fixes (bounded, review-driven)

Tree edited: `outputs/2026-10-08-l25-runtime/typescan-r4/`
Input review: `outputs/2026-10-08-l25-prep/agent-K-l25-review.md` (read first, per instructions).
Files edited (only these three): `include/NativePropertyPointerGuard.hpp`,
`include/NativePropertyPointerGuard.inl`, `candidate/src/TypeChecker.cpp`.
No builds, no console/network contact, no other files touched except CHANGES.json hash
updates + the dated entry (as instructed).

Fixes applied: F1 (MEDIUM, required), F2 (LOW), F3 (LOW, documentation only) — exactly the
three findings from the task; nothing else.

## FIX 1 (MEDIUM) — noexcept Checked wrappers can no longer terminate on a layout miss

Design note: each Checked wrapper's ENTIRE body is wrapped in one
`catch (const std::runtime_error&)`. A whole-body catch is the form of "wrap each
layout_offset call" that also covers the TRANSITIVE lookups review F1 flagged —
`classify_class_refusal → property_readable` (`FProperty::UEP_TotalSize`) and
`known_property_class → KnownPropertyClassBootstrap / read_class_fname`
(`FFieldClass::UEP_TotalSize`, `FFieldClass::FName`) — which a per-call rewrite of only
the top-level `layout_offset` statements would have missed. On a catch the wrapper records
`RefusalReason::LayoutMissing` and returns the failure sentinel (`nullptr`), preserving
`noexcept` and restoring the l24 collect-and-continue degradation. Throwing wrappers
(`ReadClass`/`ReadNext`/`ReadPointer`) are byte-for-byte unchanged.

### include/NativePropertyPointerGuard.hpp — enum appended, NOT renumbered

```diff
 enum class RefusalReason : int {
     SlotRead = 0,       // field slot or property span unreadable
     ClassNull = 1,      // class pointer is null
     ClassMisaligned = 2,// class pointer not pointer-aligned
     ClassIdentity = 3,  // class readable but not a recognized property class
+    // l25-fix (review F1/F3): appended WITHOUT renumbering the values above.
+    LayoutMissing = 4,  // layout member missing/negative at scan time (caught inside Checked wrappers)
+    Unknown = 5,        // [PropertyPointer] message without a parseable detail trailer (legacy marker)
 };
 inline constexpr const char* reason_text(RefusalReason r) noexcept {
     switch (r) {
     case RefusalReason::SlotRead:        return "slot read";
     case RefusalReason::ClassNull:       return "class null";
     case RefusalReason::ClassMisaligned: return "class misaligned";
     case RefusalReason::ClassIdentity:   return "class identity";
+    case RefusalReason::LayoutMissing:   return "layout missing";
+    case RefusalReason::Unknown:         return "unknown";
     }
     return "?";
 }
```

Existing values 0–3 and their harvest strings are unchanged; `reason_text` keeps an
exhaustive switch (no `-Wswitch` gap). Declaration comment above the Checked API extended
to document the catch-and-record behavior (hpp, checked-API comment block).

### include/NativePropertyPointerGuard.inl — Checked wrappers (now .inl:282-350)

A comment block documents the containment, then each wrapper gains one try/catch.
Representative diff (`ReadClassChecked`; `ReadNextChecked` and `ReadPointerChecked` get
the identical treatment with their own address/name in the record):

```diff
 FFieldClass* ReadClassChecked(const FField* field, RefusalLog* log) noexcept {
+    try {
         const int offset = layout_offset(FField::MemberOffsets, STR("ClassPrivate"), "FField::ClassPrivate");
         ...existing body unchanged...
+    } catch (const std::runtime_error&) {
+        if (log) record_refusal(log, RefusalReason::LayoutMissing,
+                                reinterpret_cast<uintptr_t>(field), 0, nullptr);
+        return nullptr;
+    }
 }
```

Catch-handler records per wrapper:
- `ReadClassChecked` / `ReadNextChecked`: field = `reinterpret_cast<uintptr_t>(field)`, name = nullptr.
- `ReadPointerChecked`: field = `reinterpret_cast<uintptr_t>(owner)`, name = `key`.

Note: `ReadNextChecked`'s nested `ReadClassChecked` calls record LayoutMissing themselves
and return nullptr, so the outer wrapper returns without double-recording.

## FIX 2 (LOW) — unparseable [PropertyPointer] messages + substr guard

### candidate/src/TypeChecker.cpp — trailer parser (now :59-68)

```diff
         if (reason_pos == std::string::npos || object_pos == std::string::npos ||
             field_pos == std::string::npos || class_pos == std::string::npos)
             return out;
+        // l25-fix (review F3): the guard always emits the four markers in
+        // fixed order with room for each; bounds-check that ordering before
+        // extracting so out-of-order/overlapping markers in a hostile or
+        // legacy text return unparsed instead of feeding the substr
+        // arithmetic a negative (underflowing) count.
+        if (!(reason_pos + 8 <= object_pos && object_pos + 8 <= field_pos && field_pos + 9 <= class_pos))
+            return out;
```

The emitted order is reason(8) → object(8) → field(9) → class(9) marker lengths; with the
check in place every subsequent `substr` position/count is provably in-range (no
`std::out_of_range`, no underflowing count), so the parser is defensive against any text,
not just guard-generated messages.

### candidate/src/TypeChecker.cpp — per-object catch (now :771-778)

```diff
                     const auto detail = parse_property_pointer_refusal(refusal_text);
+                    // l25-fix (review F3): a prefixed message without the
+                    // parseable trailer (e.g. a legacy or unannotated guard
+                    // refusal) is recorded with the explicit Unknown marker,
+                    // not a fabricated SlotRead with invented addresses.
                     store_all_object_types_scan_refusals.record(
-                        detail.parsed ? detail.reason : NativePropertyGuard::RefusalReason::SlotRead,
+                        detail.parsed ? detail.reason : NativePropertyGuard::RefusalReason::Unknown,
                         detail.field_address, detail.class_pointer, object_name_ascii.c_str());
```

`Unknown` is the appended enum value 5; a legacy/unparseable prefixed refusal (e.g.
`[PropertyPointer] negative layout offset refused`, CustomProperty's messages) now reads
`reason=unknown` in the harvest line instead of a fabricated `slot read` with field=0
class=0.

## FIX 3 (LOW, documentation only) — ANSI margin note

### include/NativePropertyPointerGuard.inl — fname_index_within_verified_pool (now :158-167)

```diff
         // Suffix test needs the entry header (2 bytes) + 8 suffix characters.
+        // l25-fix note (review F2): this margin assumes an ANSI suffix —
+        // 2 (entry header) + 8 ("Property" ANSI bytes). A wide-character
+        // entry whose display name ends in L"Property" needs 2 + 16 bytes
+        // and is NOT covered by this bound; the bound is therefore
+        // necessary-not-sufficient, consistent with the admission doctrine
+        // in SeedValidation's check_entry_bounds.
         if (block == current_block && offset + 2u + 8u > current_byte_offset) return false;
```

Comment only; the bound itself is unchanged.

## Compile-plausibility verification (inspection only, per rules)

- **noexcept discipline**: every return path of all three Checked wrappers is inside the
  try block or its catch; `catch (const std::runtime_error&)` matches everything
  `layout_offset` (and the transitive lookups) throw; nothing can escape the noexcept
  boundary on the reviewed failure paths.
- **enum append-only**: `SlotRead=0, ClassNull=1, ClassMisaligned=2, ClassIdentity=3`
  untouched; `LayoutMissing=4`, `Unknown=5` appended. `reason_text` switch extended to all
  six values (stays exhaustive; `constexpr`/`noexcept` unchanged).
- **includes**: no new headers needed — `std::runtime_error`, `std::string`,
  `std::string::npos/substr` were already in use in the same TUs
  (TypeChecker.cpp includes `<string>`; the .inl already throws `std::runtime_error`).
- **other in-tree users**: `tests/test_l24_guard.cpp`, `tests/actual_getters.cpp` and the
  Property sources only use equality comparisons against the original four enum values
  (no exhaustive switches) and the unchanged wrapper signatures, so the append and the
  internal try/catch are source-compatible with them. Prebuilt test binaries are l24-era
  artifacts and were neither rebuilt nor run (no-build rule).
- **brace balance** of the edited .inl region re-checked (68/68); throwing wrappers and
  all other functions byte-identical to the reviewed state.

## CHANGES.json updates (tree root)

Only the hashes of the three touched files were updated; a dated `review_fixes` entry was
added (existing format preserved; `changes`/`files` arrays keep 12 entries each):

| File | old sha256 | new sha256 |
|---|---|---|
| `candidate/src/TypeChecker.cpp` (candidate_sha256, updated in BOTH `changes` and `files` arrays) | `bb91f45ddfe581934e00900f60af008a639e2c6eae14f822fc4526059936862e` | `c05b06a7db9a50976f7b2d783c9f196b6b74698b872605e978c8af6f1f20ac7d` |
| `include/NativePropertyPointerGuard.hpp` (added_header_hashes) | `a0c949bd447544a63dd4f153dad334ab5b330139b16c05dee639de5c8636d801` | `a249ec41c0dcd8efd030a964d083dbd75806ffadc653e5a82f53fe0bcbadc696` |
| `include/NativePropertyPointerGuard.inl` (added_header_hashes) | `c7217cbd07433f7e63960d1532eba128fc664c2a9a64435f42b4e14da89fd85c` | `57bed8fc08329bd533f35fc215d04d353e56d74049f7ec8e6f74444e86ccc081` |

Post-edit verification run: every `candidate_sha256` (12/12 in both arrays), every
`copied_original_sha256` (provenance intact, e.g. TypeChecker original still
`dcebc6e6…`), and all four `added_header_hashes` match the on-disk files. Untouched
candidates (`UnrealInitializer.cpp` `c4364dd7…`, `CustomProperty.cpp` `66be0042…`)
re-verified unchanged.

NOT DONE, never claim otherwise: no host suite re-run, no target compile, no build, no
packaging, no console proof — the review's own scope note still applies to this tree.
