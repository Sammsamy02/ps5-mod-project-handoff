# l25 prep — Agent B: bootstrap class-check order fix

Base tree: `outputs/2026-10-08-l24-runtime/typescan-r3/`
Files edited (only these two, per ownership rules):
- `include/NativePropertyPointerGuard.inl`
- `include/NativePropertyPointerGuard.hpp` (comment-only)

Task: Claude l25 item 4 — in the bootstrap scan-phase class check, run the
exact match against the 39 known FField class names FIRST and the
ends-with-"Property" suffix fallback SECOND. Both rules retained (either
accepts); only evaluation order changes. Design reference:
`/tmp/l24-design/DESIGN.md` section 2 — accept = (a) AND ((b) OR (c));
(b) = suffix rule, (c) = known-name rule; l25 requires (c) before (b).

## Actual function names found (in .inl)

- `KnownPropertyClassBootstrap(uintptr_t klass)` — scan-phase check; this is
  where the reorder applies.
- `KnownPropertyClassFull(uintptr_t klass)` — strict post-scan l23 check
  (lines 60–82 of the .inl); **unchanged**.
- `known_property_class(uintptr_t klass)` — scan-mode dispatch (lines 121–126);
  **unchanged**.
- `bootstrap_property_class` / `accept_property_class` from DESIGN.md do not
  exist under those names in this tree; `KnownPropertyClassBootstrap` /
  `known_property_class` are the corresponding implementations.

## The 39-name list

- Bootstrap list: `.inl` lines 104–113 (39 `STR("...")` entries, "Property"
  through "OptionalProperty"); counted programmatically: 39.
- Strict list: `.inl` lines 70–79 (same 39 names).
- Verified the two arrays are byte-identical (modulo whitespace) via
  whitespace-stripped md5 of both ranges: both
  `9abcdeff715aa47633816041937a5d8c`. The reorder reuses the exact same
  bootstrap array and the same comparison mechanism — `FName(known, FNAME_Find)`
  lookup plus `name == candidate` FName equality, which is the case-insensitive
  exact match in this codebase (FName comparison-index semantics; `FNAME_Find`
  is lookup-only, no interning side effects). No string-compare mechanism was
  introduced; reordering only, as specified.

## Diffs

The tree is not git-tracked (files show as untracked), so diffs are
reconstructed from the pre-edit content read before modification.

### include/NativePropertyPointerGuard.inl

Hunk 1 — function header comment (now lines 83–87):

```diff
@@ -83,4 +83,5 @@
     // l24: bootstrap class check (during scan). Accepts readable heap FFieldClass
-    // objects whose FName ends in "Property" or matches a known name. No
-    // CastFlags requirement (classes may still be initializing). Uses read_bytes
-    // (which admits heap regions), not image_class_range (image-only).
+    // objects whose FName matches a known name (l25: checked first) or ends in
+    // "Property" (l25: fallback, was first in l24). No CastFlags requirement
+    // (classes may still be initializing). Uses read_bytes
+    // (which admits heap regions), not image_class_range (image-only).
```

Hunk 2 — the reorder itself (now lines 97–104):

```diff
@@ -96,8 +97,11 @@
         FName name{};
         if (!read_class_fname(klass, name)) return false;
-        // Accept if the name ends in "Property" (suffix heuristic) or is a
-        // known FField class name (exact match, no interning side effects).
-        if (fname_ends_in_property(name)) return true;
+        // l25: evaluation-order fix — the exact match against the 39 known
+        // FField class names (case-insensitive FName comparison; FNAME_Find
+        // lookup only, no interning side effects) runs FIRST; the
+        // ends-with-"Property" suffix heuristic runs SECOND as a fallback.
+        // Either accepts; only the evaluation order changed (DESIGN.md
+        // section 2: rule (c) tried before rule (b)), so an exact known
+        // name never pays the ToString-based suffix path.
         const CharType* names[] = {STR("Property"),STR("NumericProperty"),STR("ObjectPropertyBase"),
```

Hunk 3 — suffix demoted to fallback (now lines 114–119):

```diff
@@ -109,7 +113,8 @@
         for (const auto* known : names) {
             const FName candidate(known, FNAME_Find);
             if (candidate.GetComparisonIndex().ToUnstableInt() != 0 && name == candidate) return true;
         }
+        if (fname_ends_in_property(name)) return true; // l25: suffix rule (b) demoted to fallback
         return false;
     }
```

Net effect in `KnownPropertyClassBootstrap` after the FName read at line 96:
1. lines 104–117: exact 39-name known-name match (rule (c)) — returns true on hit;
2. line 118: `fname_ends_in_property(name)` suffix test (rule (b)) — fallback;
3. line 119: `return false` (identity refusal).

### include/NativePropertyPointerGuard.hpp (comment-only)

```diff
@@ -43,3 +43,4 @@
 // During the type scan, the bootstrap class check is used (readable heap
-// + FName suffix "Property" or known FField class name). After the scan,
-// the full check applies (image range + CastFlags + exact name match).
+// + known FField class name (l25: checked first) or FName suffix
+// "Property" (l25: fallback, was first in l24)). After the scan,
+// the full check applies (image range + CastFlags + exact name match).
```

Now at `.hpp` lines 43–46. No code change in the header.

## Invariants preserved

- **Strict post-scan phase unchanged**: `KnownPropertyClassFull` (`.inl`
  lines 60–82) was not edited — still image range + CastFlags
  (`CASTCLASS_FProperty`) + exact `FName(known, FNAME_Find)` match against
  the l23 list, same order, same formatting. The dispatch
  `known_property_class` (lines 121–126) is also untouched.
- **Refusal attribution intact**: both accept rules are pure predicates on
  the already-read `FName`; their OR is commutative, so the accepted set is
  identical to l24 — only the path taken for names matching both rules
  changes (all 39 known names end in "Property", so those now accept via the
  known-name path instead of the suffix path). `KnownPropertyClassBootstrap`
  still returns a single bool; `classify_class_refusal` (lines ~127–135) and
  `ReadClassChecked` (lines ~175–190) still record
  `RefusalReason::ClassIdentity` on false, and the throwing `ReadClass`
  still throws the same error string. No refusal codes or taxonomy changed.
- **No side-effect delta**: `fname_ends_in_property` resolves via
  `FName::ToString` (read-only pool resolution) and the known-name loop uses
  `FNAME_Find` (lookup-only, no interning) — neither mutates the FName pool,
  so reordering cannot alter pool state. A suffix-only name (ends in
  "Property" but not one of the 39) now pays up to 39 failed `FNAME_Find`
  lookups before accepting — the accepted l25 cost of the fix.
- **Scope**: only the two owned files were edited; no builds, no
  console/network. Brace balance verified (52/52). `FNAME_Find` code sites
  remain exactly 2 (a third grep hit is the new comment text).

## Change citation index

- `.inl` lines 83–87 — bootstrap header comment (order documented).
- `.inl` lines 97–103 — l25 reorder rationale comment (replaces old
  comment + suffix-first call).
- `.inl` lines 104–117 — known-name exact match, now evaluated first
  (array at 104–113, loop at 114–117; unchanged content, moved position).
- `.inl` line 118 — suffix fallback (moved from former line 98).
- `.hpp` lines 43–46 — scan-mode control comment (order documented).
