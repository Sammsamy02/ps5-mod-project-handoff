# Agent CP — FindFirstOf/FindAllOf Find-first name construction (layer-1 fix)

Date: 2026-10-08. Staged in `outputs/2026-10-08-l26-runtime/typescan-r4/`
(consumption law: pin-identical original + edited candidate + CHANGES.json row).
Inputs honored: CO bug #1 + the Hooks.cpp:511-518 precedent + the predicted
signature (agent-CO-walk-audit.md §5), CB Add-vs-Find semantics
(agent-CB-fname-machinery.md §1(a)/§5). Port root untouched. No console contact.

## 1. The bug and the fix

Every FindXOf string overload in UObjectGlobals.cpp constructed
`FName(ClassName)` with the default **FNAME_Add**, which through the bound ctor
creates a NEW numbered pool entry instead of retrieving the existing one (the
documented duplicate-name bug, Hooks.cpp:518 comment). `Equals` compares
ComparisonIndex only, so the duplicated index never matches any stored class
name and the mechanically-perfect GUObjectArray walk returns nullptr for every
type. The fix mirrors the port's own Hooks.cpp `AddRequiredObject` workaround:
construct **FNAME_Find first**; only on `IsNone()` fall back to FNAME_Add.

New TU-local helper (Prospero-guarded), inserted after `FindRootClassesByName`
before `FindFirstOf(FName)`:

```cpp
static auto MakeFindXOfClassName(StringViewType ClassName) -> FName
{
    auto Name = FName(ClassName, FNAME_Find);
    if (Name.IsNone())
    {
        Name = FName(ClassName, FNAME_Add);
        LogFindXOfAddFallback(Name);
    }
    return Name;
}
```

## 2. Overloads changed (11 name-construction sites; line numbers = l22-diag original)

| # | Overload | Original line | Decision |
|---|----------|---------------|----------|
| 1-5 | `FindFirstOf(const CharType*)`, `(StringViewType)`, `(const StringType&)`, `(std::string_view)`, `(const std::string&)` | :528/:533/:538/:543/:548 | Find-first + Add-fallback |
| 6 | `FindAllOf(const StringType&, …, bool bUseCache)` — the `FName(InClassName)` construction | :555 | Find-first + Add-fallback |
| 7-11 | `FindAllOf(const CharType*)`, `(StringViewType)`, `(const StringType&)`, `(std::string_view)`, `(const std::string&)` no-cache | :639/:644/:649/:654/:659 | Find-first + Add-fallback |

Find-vs-Add decision, per overload — **uniform: Find-first with Add-fallback**
(pure lookup first; Add only on NAME_None so a not-yet-interned name keeps the
upstream add-new-name semantics, and the fallback still interns it). Call-site
survey (whole port tree) found only pure lookups; none relies on Add's
interning side effect for its own sake: LuaMod.cpp:1651 (`FindFirstOf(ensure_str(...))`
— the failing console path) and :1693 (`FindAllOf(lua.get_string(), …)`),
UE4SSProgram.cpp:97 (`STR("PlayerController")`), GUI Dumpers.cpp:111 /
DataDumpers.cpp:74 (`STR("GameModeBase")`), UnrealInitializer.cpp:1185
(`GetInstanceFromClass(const TCHAR*)` — lookup with lookup fallback). A class
that exists is already in the pool, so Find-first cannot regress a legitimate
lookup (CO §5); CO/Hooks treat these as Find-only lookups and the fallback
covers the only thing Add did that Find cannot.

NOT touched (minimal-fix rule): the two FindXOf FName overloads (the walk
bodies :481-526/:593-637 — hash-table arms, ForEachUObject, TSuperStructRange,
IsValidObjectForFindXOf all byte-identical), `FindAllOf(std::string_view, …,
bool bUseCache)` :588 (pure delegator via ensure_str into the fixed #6),
**FindObject's own FNAME_Add constructions (:309, :330-364)** — FindObject is
not FindXOf, is not on the FindFirstOf failure chain (the init type-scan uses
the FNAME_Find name-parts path via AddRequiredObject, which works on console),
and its long-name parsing semantics are a separate change for the parent to
authorize if ever wanted. Seeds, exclusion rules, GATE-1: untouched.

## 3. Edition determination (CN's parent-obligation pattern)

The l25-A BASE (the identity every l26 row is re-keyed on, per CM's
`l26_row_sync` rule) compiled unit `unreal-uobjectglobals` from
**`outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp`** —
verified in l25-A/pass1+pass2 AND l25-B/pass1+pass2
`unreal-uobjectglobals.command.json` argv and `.d` receipts, and pinned as
`source_sha256 9b335a51…` in the l24-build-r2 COMPOSITION-RECEIPT
(`target_compile.py` matches rows by this exact source path). The port-root
edition (`.superpowers/.../port/unreal/src/UObjectGlobals.cpp`,
sha 49951ac8…) has never been compiled by any l25/l26 build and differs by the
L22Diagnostics logging wrappers. Row keyed on the consumed identity:

- original pin: `l22-runtime/diagnostics/src/UObjectGlobals.cpp`
  sha256 `9b335a5195f91feca4b062fcf6f04131f3cec16652069bbe389532906f109490`
- candidate: `l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp`
  sha256 `12470d77b3a3ebcf39290aee366b828ec8fa99a16d0499bf6dba08aa52ed2228`
- CHANGES.json row 30 in BOTH `changes` and `files`; scope text in new key
  `scope_l26_layer1_findxof` (documents the edition decision). Full-manifest
  audit after the append: all 30 rows x 3 hashes match disk.

Note for readers of CO/CB citations: their port-root line numbers (:481-504)
shift in the l22-diag edition (L22Diagnostics wrappers inflate earlier
functions); the string overloads sit at :528-551/:553-555/:639-662 there.

## 4. Diagnostic marker

`[FFO] add-fallback [n:{:X}]` — `Output::send<LogLevel::Error>` via `STR()`
(wide-char), line ≤ 31 chars. Logged ONLY when the Find-first construction
missed and the Add fallback ran; a Find hit logs nothing. Deduped per name
(key = the Add-result ComparisonIndex — engine Store interns where shard-Find
searches, so index equality is name equality), 128-site cap, try/catch,
CN house pattern. Semantics of the field: nonzero = the (dup or newly
interned) Add index; `[n:0]` cannot occur for real stored names ("None"
interns at index 0 and its Add-duplicate index is nonzero — fixture-proven).

## 5. Verification

Compile (receipts in `results/`):

- **Real prospero-clang++ fidelity**: l25-A/pass1 `unreal-uobjectglobals`
  receipt argv verbatim; deviations only -c → staged file, -o/-MF →
  /tmp/l26-findxof-smoke, -ivfsoverlay → `/tmp/l26-harden-smoke/headers-l26.json`
  (CN's fidelity overlay = l25-A overlay + l26 repoints; the
  UObjectGlobals.hpp mapping is identical in both). **Candidate rc 0,
  original rc 0** (baseline). `L26-FINDXOF-PROSPERO-COMPILE.json`.
- **Z13 host stub compile** (CN's unreal-uobject host command, TU swapped,
  `-D__PROSPERO__ -D__SCE__ -fshort-wchar`): **rc 0**.
  `L26-FINDXOF-HOST-COMPILE.json`.
- Object inspection: UTF-16 `[FFO] add-fallback` present in candidate .o
  (prospero AND host), absent in original .o;
  `MakeFindXOfClassName`/`LogFindXOfAddFallback` local symbols present.
- **Host-arm equivalence**: a no-`__PROSPERO__` compile config does not exist
  in this tree (pre-existing Platform.hpp/Utf16Utf8.hpp static_asserts and the
  ProsperoPlatformCompilerPreSetup #error reject it — same for the unmodified
  original). Equivalence proven mechanically instead: the original TU contains
  zero conditionals of its own, and stripping the Prospero arms from the
  candidate (keeping each `#else` side, dropping the helper block) reproduces
  the original line-for-line (modulo blank lines).

Behavioral fixture `tests/actual_findxof_fname_fix.cpp` (actual edited bodies;
mock pool implements CO's predicted engine behavior: Find = stored entry or
silent {0,0}; Add of an interned name = numbered duplicate {fresh,1}, Add of a
new name = intern {idx,0}). Plain + fatal ASan/UBSan both exit 0
(`results/HOST-TESTS-L26-FINDXOF.json`):

- **RED (CO's predicted failure)**: original default-Add construction on
  "PlayerController" (stored index 1) yields {dup,1}, `Equals` (index-only)
  misses despite two live instances → `FindFirstOf` returns nullptr and
  `FindAllOf` collects nothing.
- **GREEN (fixed)**: all five FindFirstOf string shapes return the first live
  instance; all five no-cache FindAllOf shapes collect both instances; the
  bUseCache overload's :553 construction Equals the stored class name.
- **Marker**: zero lines across all Find-hit lookups; unknown name → exactly
  one `[FFO] add-fallback [n:X]`, deduped on repeat, second unknown logs its
  own line; the fallback interned the new name so the SECOND lookup resolves
  via Find with no further logging (add-new-name semantics preserved);
  literal "None" query: one dup-index line, no crash, still deduped;
  marker line < 60 chars asserted.

## 6. Residuals for the parent

1. Full-tree rebuild, packaging, console proof remain the parent's. Console
   readout for the v5 ladder: P1 now exercises Find-first; absence of
   `[FFO] add-fallback` lines for real class names = lookups resolving via
   Find (bug #1 confirmed/fixed); a fallback line for a known-existing class
   would instead indicate the name genuinely absent from the pool (CO #2/#3
   territory).
2. One extra engine call per string lookup happens ONLY on the miss path
   (Find miss + Add) — the hit path is one call where the old code made one;
   no walk/threading semantics changed.
3. FindObject's FNAME_Add constructions remain upstream-identical (see §2) —
   flagged if the parent wants the same treatment there later.

— agent CP (ZCode), 2026-10-08. Only the staged tree, receipts and this report written.
