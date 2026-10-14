# Agent X — l25 review-fix verification (real compile + run, Mac host)

Independent verification of the fixes reported in `agent-N-review-fixes.md` (tree:
`outputs/2026-10-08-l25-runtime/typescan-r4/`). Nothing in the tree was modified; all
harness files live under `/tmp/l25-verify-fixes/`.

**VERDICT: all three fixes verified by real compile AND execution. 85/85 checks passed
in both plain and ASan+UBSan builds, exit 0. No discrepancy with the fix report's
functional claims (one trivial line-number drift, noted below).**

## Input identity (tree is read-only for this agent; hashes prove what was tested)

| File | sha256 on disk | matches fix report? |
|---|---|---|
| `include/NativePropertyPointerGuard.hpp` | `a249ec41c0dcd8efd030a964d083dbd75806ffadc653e5a82f53fe0bcbadc696` | yes |
| `include/NativePropertyPointerGuard.inl` | `57bed8fc08329bd533f35fc215d04d353e56d74049f7ec8e6f74444e86ccc081` | yes |
| `candidate/src/TypeChecker.cpp` | `c05b06a7db9a50976f7b2d783c9f196b6b74698b872605e978c8af6f1f20ac7d` | yes |

## Harness

`/tmp/l25-verify-fixes/fix_verify_harness.cpp` (build script: `build.sh`):

- Compiles the **real** `NativePropertyPointerGuard.hpp` + `.inl` directly from the tree
  (`-I<TREE>/include`), using the exact include recipe of the tree's own
  `tests/test_l24_guard.cpp`: engine mocks (`CharType`, `FName`, `FField`/`FProperty`/
  `FFieldClass` with `inline static std::unordered_map<std::string,int> MemberOffsets`,
  `EClassCastFlags`, `FNAME_Find`), `#define STR(x) x`, `#define __PROSPERO__ 1`, then
  `#include "NativePropertyPointerGuard.inl"` inside `namespace RC::Unreal`. `<atomic>`
  included explicitly, as `TypeChecker.cpp` itself does before the .inl.
- The trailer parser is included **verbatim** from the real
  `candidate/src/TypeChecker.cpp` lines 40–89 via `parser_region.inc`, generated with
  `sed -n '40,89p'` (region sha256
  `9492b7f328ad16d5c9f595357da98e216c3c4c870f44faa2306786cce1e5cb0b`) — the same
  extraction pattern as `/tmp/l25-sanity/scanbody_harness.cpp`.
- The per-object catch's record fallback (`TypeChecker.cpp:776-778`) is replicated
  character-for-character in `record_like_scan_catch()`.
- Compile flags match the tree's `tests/run.py`: `clang++ -std=c++20 -include utility`
  (Apple clang 17.0.0); plus a second build with
  `-fsanitize=address,undefined -fno-sanitize-recover=all -g`, matching run.py's
  asan-ubsan mode. (`run.py`'s extra `-I …/2026-10-07-l19-local-profile-r4/units` is
  unused by `test_l24_guard.cpp` and by this harness.)

## Compile result

- plain and asan-ubsan builds: **0 errors**. With `-Wall -Wextra -Wswitch` the only
  warnings are 3 × `-Wmismatched-tags` from the harness's mock structs (inherited from
  the tree's own `test_l24_guard.cpp` recipe, mock-only, harmless on this ABI).
  **No `-Wswitch` warning** — `reason_text`'s switch over all six enum values is
  exhaustive, confirming the report's claim.

## Run result — `l25 fix verification harness: 85 checks, 0 failures` (exit 0, both modes)

Non-vacuity controls first: with full layouts the fixture passes
`ReadClassChecked`→class, `ReadNextChecked`→next, `ReadPointerChecked`→target, zero
records — so the failure tests below are testing real refusal paths. A separate
**negative control** (`negative_control.cpp`) proves the same fixtures genuinely make
`layout_offset` throw (the unchanged throwing `ReadClass` throws
`Tried getting member variable 'FField::ClassPrivate'…`), i.e. the Checked wrappers
really had something to catch.

### FIX 1 (Checked wrappers cannot terminate on layout miss) — VERIFIED
- C1 `FField::ClassPrivate` removed: `ReadClassChecked`/`ReadPointerChecked` return
  `nullptr`, record exactly one `LayoutMissing` with field==field/owner, class==0,
  name empty / == key; process alive (an escape from `noexcept` would have
  `std::terminate`d before any later check).
- C1b nested no-double-record: `Next` present + `ClassPrivate` missing →
  `ReadNextChecked` records **exactly one** LayoutMissing (inner `ReadClassChecked`),
  matching the report's note.
- C2 `Next` removed: `ReadNextChecked` catches its own first-statement throw → one
  LayoutMissing record.
- C3 transitive (review's key point): `FProperty::UEP_TotalSize` missing →
  `property_readable` throws inside `classify_class_refusal` → caught → LayoutMissing.
- C4 negative member (`UEP_TotalSize = -1`): throws "[PropertyPointer] negative layout
  offset refused" → caught → LayoutMissing.
- C5/C6 transitive through identity check: `FFieldClass::UEP_TotalSize` missing throws
  inside `KnownPropertyClassFull` (full mode) and `KnownPropertyClassBootstrap`
  (scan mode) → caught → LayoutMissing, both modes.
- Post-restore control still passes (fixture intact after mutations).

### Enum append-only + reason_text — VERIFIED
- `static_assert`s: `SlotRead=0, ClassNull=1, ClassMisaligned=2, ClassIdentity=3,
  LayoutMissing=4, Unknown=5` — compiles, so the underlying values are exact.
- `reason_text`: "slot read"/"class null"/"class misaligned"/"class identity" unchanged;
  **"layout missing"** and **"unknown"** returned for the new values.

### FIX 2 (parser bounds check + Unknown fallback) — VERIFIED
- Well-formed guard trailers parse: reason (`class misaligned`, `slot read`, and all
  four legacy reasons), field/class hex (upper, lower, mixed case) exact.
- `[PropertyPointer] negative layout offset refused` (real legacy message, no trailer)
  and an unannotated prefixed refusal → `parsed=false`, no throw; the replicated
  scan-catch record expression stores **`Unknown`** (harvest text "unknown") with
  field=0, class=0, object name stamped — not the pre-fix fabricated `SlotRead`.
- Out-of-order markers (object before reason; class before field; `field=0x` at exact
  end-of-text) → `parsed=false`, **no exception**, no underflowed `substr` count
  (UBSan build confirms no UB).
- `reason=layout missing` in a trailer (never thrown by the guard) → unparsed →
  recorded `Unknown`; consistent with the design that LayoutMissing is recorded
  directly, never thrown.

### FIX 3 (ANSI-suffix doc-only note) — VERIFIED
Comment-only at `.inl:159-164` above the unchanged bound (`offset + 2u + 8u >
current_byte_offset`, .inl:165); compiles; no code change (diffed against the claim
by inspection; hash `57bed8fc…` matches the report).

## Discrepancies vs the fix report
- **Functional: none.** Every behavior claim in `agent-N-review-fixes.md` reproduced.
- Trivial citation drift: the task brief cites the parser check at "`:63-68`" and the
  report says "now :59-68"; in the on-disk file the comment+check occupy lines 59–65
  (comment 59–63, guard 64–65). Content is exactly as described. All other line
  citations (catch 771–778, .inl 282–350, comment 158–167) match the on-disk file.

## Provenance
- Harness + build script + binaries: `/tmp/l25-verify-fixes/`
  (`fix_verify_harness.cpp`, `negative_control.cpp`, `parser_region.inc`, `build.sh`,
  `harness-plain`, `harness-asan-ubsan`).
- Tree untouched (read-only use; hashes above re-checked after the runs).
