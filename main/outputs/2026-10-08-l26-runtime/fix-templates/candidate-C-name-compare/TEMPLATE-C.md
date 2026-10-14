# TEMPLATE C — class-name Equals compare reads wrong NamePrivate slot / number-form

Candidate root cause C: the walk's class-name compare
`Class->GetNamePrivate().Equals(ClassName)` fails for every object because
(a) `NamePrivate` is read at a wrong offset (profile), or (b) `FName::Equals`
compares the wrong index slot for this engine's FName form (case-preserving
pool: display id vs comparison id), or (c) a number-form mismatch (CO's #1
FNAME_Add duplicate mechanism — already mitigated Find-first by CP's
`MakeFindXOfClassName`, residual: stored-side or lookup-side Number).

---

## 1. Where the compare gets its value NOW (exact files + lines)

Compare sites — l26 candidate `typescan-r4/candidate/src/UObjectGlobals.cpp`:

| Site | Lines | Compare |
|---|---|---|
| `FindFirstOf(FName)` direct-class match | `:552` | `Class->GetNamePrivate().Equals(ClassName)` |
| `FindFirstOf(FName)` super-chain match | `:561-568` (`:563`) | `super_struct->GetNamePrivate().Equals(ClassName)` via `TSuperStructRange` |
| `FindAllOf(StringType,...)` cache-scan | `:640`, `:648` | same two forms |
| `FindAllOf(FName,...)` walk | `:690`, `:700` | same two forms |
| (adjacent, init path) `StaticFindObject_InternalNoToStringFromNames` | `:231` | `NamePart.Equals(PathName)` |

Name construction feeding the compare (l26, `__PROSPERO__` only):
`MakeFindXOfClassName` `:517-526` — `FName(ClassName, FNAME_Find)`,
Add-fallback logged `[FFO] add-fallback [n:X]` `:498-516`.

The read chain:

| Step | File:line | Source of the value |
|---|---|---|
| `GetNamePrivate()` | generated wrapper PORT `unreal/generated_include/MemberVariableLayout_SrcWrapper_UObjectBase.hpp` (`FName& GetNamePrivate()`) | `UObjectBase::MemberOffsets["NamePrivate"]` — **PROFILE-driven** |
| Map publication | PORT `ue4ss/src/NativeLayoutBinding.cpp:175-199` | from `profile/units/CompiledNativeLayoutDeclarations.hpp` string[0] INI `[UObjectBase] NamePrivate=0x18` (schema row `{"UObjectBase","NamePrivate","UObjectBase","NamePrivate",false}` in `ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc`) |
| Boot pin | `profile/units/RuntimeBuildProfile.hpp` row `{"UObjectBase::NamePrivate",true,24,8,...}` + `JoinedLayoutPreflight.hpp:20` (Required, 24) | refuse-on-mismatch at CAND `src/UnrealInitializer.cpp:1384-1415` |
| `Equals` | CAND `typescan-r4/candidate/include/Unreal/NameTypes.hpp:354-360` | **CODE**: `return ComparisonIndex == Other.ComparisonIndex;` (index-only) |
| FName layout | same file `:227-231` | `ComparisonIndex` at +0; `DisplayIndex` at +4 ONLY under `WITH_CASE_PRESERVING_NAME`; `Number` last. (`GetDisplayIndexFast` `:383-389` returns DisplayIndex if defined else ComparisonIndex.) |
| number-form siblings | same file `:334-337` `operator==` compares `ToUnstableInt()` (index+number); `:408-410` pack index+number | Equals IGNORES Number; `==` and hashing USE it |

Console-proven comparator for contrast: the init-time successes
(`Already constructed: /Script/Engine.PlayerController`, `[TypeScan]
scan-complete ... core-ready=true`) went through `GetFullName()` STRING
compares (`StaticFindObject_InternalSlow` :184-206) — string mode is the
form with console evidence behind it (CO §4).

---

## 2. Fill-in patch templates

### C1 — NamePrivate offset correction (profile; AS/AM five-layer)

Only if the audit proves the stored slot is NOT +0x18 (e.g. engine stores
FName at a different UObjectBase slot in this build). Same five-layer edit
as TEMPLATE-A variant A-profile with
`member_dotted=UObjectBase::NamePrivate`, `section=UObjectBase`,
`key=NamePrivate`, `old_offset=24 (0x18)`, `true_offset=<<NAMEPRIVATE_TRUE_OFFSET>>`,
`width=8`; digest recomputed; JoinedLayoutPreflight.hpp:20 row updated.
NOTE the width guard: the INI parser validates every member offset+width
inside `UEP_TotalSize` (0x28) — a slot beyond the class size refuses at boot,
which is the desired fail-closed behavior for a bad value.

### C2 — Compare correction (code; slot-insensitive match)

If the audit shows the engine pool is case-preserving (lookup ctor returns
the DISPLAY id in the ComparisonIndex slot vs stored COMPARISON id, or vice
versa), make the walk compare match either index — CAND
`typescan-r4/candidate/src/UObjectGlobals.cpp`, add a local helper above
`FindFirstOf(FName)` and use it at the six sites:

```diff
+    // l26 walkfix-C: class-name comparator tolerant of case-preserving FName
+    // slot placement (comparison id vs display id). <<PROVENANCE_SLOT_C>>
+    static auto FindXOfNameMatches(const FName& Stored, const FName& Lookup) -> bool
+    {
+        if (Stored.Equals(Lookup)) { return true; }               // index-only, fast path
+        return Stored.GetDisplayIndexFast() == Lookup.GetDisplayIndexFast()
+            || Stored.GetComparisonIndex() == Lookup.GetDisplayIndexFast()
+            || Stored.GetDisplayIndexFast() == Lookup.GetComparisonIndex();
+    }
     ...
-            if (Class->GetNamePrivate().Equals(ClassName) && IsValidObjectForFindXOf(Object))
+            if (FindXOfNameMatches(Class->GetNamePrivate(), ClassName) && IsValidObjectForFindXOf(Object))
```

(repeat the substitution at `:563`, `:640`, `:648`, `:690`, `:700`; leave the
init-path `:231` `Equals` untouched — that path is console-proven.)

String-mode alternative (the console-proven comparator; heavier — only if the
audit cannot exonerate string rendering):

```diff
-            if (Class->GetNamePrivate().Equals(ClassName) && IsValidObjectForFindXOf(Object))
+            if (String::iequal(Class->GetNamePrivate().ToString(), ClassName.ToString()) && IsValidObjectForFindXOf(Object))
```

(This reintroduces ToString into the hot walk — 35K objects per call;
acceptable for a diagnostic-gated fix, prefer C2-index if sufficient.)

### C3 — Number-form correction

Only if the audit shows stored class names with Number != 0 (numbered
duplicates) while lookups carry 0 (or vice versa). Equals already ignores
Number, so a C3 verdict means the mechanism is inside `MakeFindXOfClassName`
or the ctor: extend `MakeFindXOfClassName` (:517-526) to retry a numbered
probe when the unnumbered Find misses:

```diff
         auto Name = FName(ClassName, FNAME_Find);
         if (Name.IsNone())
         {
             Name = FName(ClassName, FNAME_Add);
             LogFindXOfAddFallback(Name);
+            // l26 walkfix-C3: if the Add created/returned a NUMBERED entry,
+            // the stored names may be numbered forms; retry the numbered Find
+            // before giving the walk a name that cannot match. <<PROVENANCE_SLOT_C>>
+            if (Name.GetNumber() != 0)
+            {
+                Output::send<LogLevel::Warning>(STR("[FFO] numbered-entry [n:{:X} number:{}]\n"),
+                    Name.GetComparisonIndex().ToUnstableInt(), Name.GetNumber());
+            }
         }
```

(log-only first, mirroring the existing `[FFO]` diagnostic family — a
behavioral retry needs the audit's exact number evidence; do not blind-fix).

---

## 3. Verification command set

```bash
# 1. Walk harness D2 discriminator (CO section 5 / harness header): comparison
#    semantics index+number vs index-only vs display-id; case sensitivity. Add
#    fixtures: stored comparison-id vs lookup display-id (C2 catches), stored
#    numbered name (C3 surfaces via [FFO] numbered-entry), wrong NamePrivate
#    slot (C1 catches via profile fixture offsets).
cd /tmp/l26-ufunc-harness && python3 extract_and_build.py && cat results/run.stdout.txt

# 2. Profile suite ONLY if C1 landed (NamePrivate row + digest moved).
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/profile/tests/run_tests.py

# 3. Scan-refusal suite.
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/tests/run_tests.py

# 4. Compile + link proof with updated CHANGES rows.
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/build_runtime.py
```

Console acceptance signal (authorized launch only): P1 PlayerController
FindFirstOf valid, P2 ItemDatabaseSubsystem valid, and the `[FFO]
add-fallback` family stays SILENT for known classes (a fallback firing for
"PlayerController" after C-landed = the ctor path is still wrong — do not
ship).

## 4. CHANGES.json row shapes

C2/C3 edit `typescan-r4/candidate/src/UObjectGlobals.cpp` — row exists;
recompute `candidate_sha256`:

```json
{
  "original": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp",
  "candidate": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp",
  "original_sha256": "9b335a5195f91fec...",
  "candidate_sha256": "<<RECOMPUTED_AFTER_EDIT>>"
}
```

(copy the full `original_sha256` from the live `typescan-r4/CHANGES.json`
row; the template deliberately abbreviates it.)

C1 moves the four profile-part rows exactly as TEMPLATE-A §4 A-profile
(originals frozen l25 pins, `candidate_sha256` recomputed, new
`profile_digest`, `walkfix_c_corrected_facts: 1`, scope sentence citing the
audit receipt).
