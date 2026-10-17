# TEMPLATE B — IsA\<UClass\> canonical class pointer resolves wrong

Candidate root cause B: one of the five meta-class canonical pointers used by
the walk filter (`UClass`, `UFunction`, `UPackage`, `UEnum`, `UScriptStruct`)
resolves to the WRONG class object (non-null, so it passes the core gate).
If it lands on a class in every object's super-chain (e.g. resolves to
`/Script/CoreUObject.Object`'s class object), every `IsA<T>()` in the filter
returns true for every object → `IsValidObjectForFindXOf` false everywhere →
walk finds nothing, mechanically perfect iteration (CO §5 #3 pattern).

---

## 1. Where the canonical pointer gets its value NOW (exact files + lines)

Filter — l26 candidate `typescan-r4/candidate/src/UObjectGlobals.cpp:267-271`:

```cpp
!object->IsA<UClass>() && !object->IsA<UFunction>() && !object->IsA<UPackage>()
                       && !object->IsA<UEnum>() && !object->IsA<UScriptStruct>()
```

| Step | File:line | What it does |
|---|---|---|
| 1 | PORT `unreal/include/Unreal/UObject.hpp:83-87` | `IsA<T>() { return IsA(T::StaticClass()); }` |
| 2 | CAND `typescan-r4/candidate/src/UObject.cpp:103-110` (PORT `unreal/src/UObject.cpp:103-110`) | `IsA(UClass*) { return GetClassPrivate()->IsChildOf(Class); }` |
| 3 | PORT `unreal/include/Unreal/CoreUObject/UObject/Class.hpp:177-190` | `IsChildOfInternal`: pointer-equality walk up `GetSuperStruct()` (profile `[UStruct] SuperStruct=0x40`) until match/null |
| 4 | Canonical value | `UClass::StaticClass()` → `UClass::StaticClassStorage` |

`StaticClassStorage` is seeded in TWO places, in this order:

- **Primary (init type-scan)** — CAND `typescan-r4/candidate/src/TypeChecker.cpp:165-166`
  (PORT `unreal/src/TypeChecker.cpp:86-87`):
  ```cpp
  UClass* class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Class")));
  UClass::StaticClassStorage = class_ptr;
  ```
  (Same pattern at :152-205 for Object/Field/Struct/Enum/Package/Function/
  ScriptStruct/Interface — the five filter canonicals are `UClass`:165-166,
  `UFunction`:196, `UPackage`:189, `UEnum`:159-160, `UScriptStruct`:204-205.)
  `StaticFindObject_InternalSlow` (CAND `src/UObjectGlobals.cpp:184-206`) is
  the SAME chunked walk with a `GetFullName()` STRING compare — its
  correctness depends on `GetNamePrivate().ToString()` + the OuterPrivate
  chain rendering exact paths.
- **Lazy fallback** — PORT `unreal/include/Unreal/UObjectMacros.hpp:149-167`
  (`IMPLEMENT_EXTERNAL_OBJECT_CLASS`): if storage is null/`STATIC_CLASS_INVALID`
  at first use, re-resolve the same FullName, THROW if still null (a MISSING
  canonical crashes loudly — CO §3.5 — it cannot produce quiet find-nothing).

**The gap:** the core-ready gate at CAND `src/TypeChecker.cpp:979-1017`
(`require_type("UClass", UClass::StaticClassStorage && != STATIC_CLASS_INVALID)`
at :988; `scan-complete` log at :1016) checks NON-NULLITY ONLY. A wrong-but-
nonnull canonical (string-walk matched the wrong object: duplicate path from
a numbered/duplicated FName rendering, an OuterPrivate-chain anomaly, or a
ToString glitch) passes silently and poisons every later `IsA` filter.

Init order (CAND `src/UnrealInitializer.cpp`): `:2403` `store_all_object_names()`
(FNAME_Find names) → `:2406-2415` required-objects construction check →
`:2418` `store_all_object_types()` (throws on `!core_ready`). The walk filter
runs only later (Lua FindFirstOf at runtime), long after seeding.

---

## 2. Fill-in patch templates (pick by audit verdict; B1 is the safe default)

### B1 — Canonical identity gate (fail-closed; touches TypeChecker.cpp only)

CAND `typescan-r4/candidate/src/TypeChecker.cpp`. IMPORTANT placement: the
five filter canonicals seed at :159-160 (UEnum), :165-166 (UClass), :189
(UPackage), :196 (UFunction), :204-205 (UScriptStruct) — the verification
must EXECUTE only after the LAST of them (:205). Declare the helper at
function scope near `missing_keys` (:131) and compute `canonicals_verified`
just before the gate block at :975 (the missing-keys print):

```diff
+        // l26 walkfix-B: non-null is not identity. The core gate only checked
+        // presence; a wrong-but-nonnull canonical silently excludes every
+        // object in IsA<T> filters. <<PROVENANCE_SLOT_B>>
+        auto verify_canonical = [](const char* storage_name, UClass* canonical,
+                                   const CharType* expected_short_name) -> bool {
+            if (!canonical) { return true; } // absence is the existing gate's job
+            try {
+                const auto rendered = canonical->GetNamePrivate().ToString();
+                if (!String::iequal(rendered, expected_short_name)) {
+                    Output::send<LogLevel::Error>(
+                        STR("[TypeScan] canonical-mismatch storage={} expected={} got={} ptr=0x{:X}\n"),
+                        StringType(storage_name, storage_name + std::char_traits<char>::length(storage_name)),
+                        expected_short_name, rendered, reinterpret_cast<uintptr_t>(canonical));
+                    return false;
+                }
+            } catch (...) {
+                Output::send<LogLevel::Error>(STR("[TypeScan] canonical-verify-threw storage={}\n"),
+                    StringType(storage_name, storage_name + std::char_traits<char>::length(storage_name)));
+                return false;
+            }
+            return true;
+        };
+        const bool canonicals_verified =
+            verify_canonical("UClass",        class_ptr,         STR("Class")) &&
+            verify_canonical("UFunction",     UFunction::StaticClassStorage,     STR("Function")) &&
+            verify_canonical("UPackage",      UPackage::StaticClassStorage,      STR("Package")) &&
+            verify_canonical("UEnum",         UEnum::StaticClassStorage,         STR("Enum")) &&
+            verify_canonical("UScriptStruct", UScriptStruct::StaticClassStorage, STR("ScriptStruct"));
```

and join it into the existing gate (before `:1016`):

```diff
         Output::send(STR("[TypeScan] scan-complete missing-keys={} core-ready={}\n"), missing_keys.values.size(),core_ready);
-        if (!core_ready) return false;
+        if (!core_ready || !canonicals_verified) return false;
```

(Declaration of `verify_canonical` goes at function scope near
`missing_keys` at :131; the `canonicals_verified` computation goes after the
last seed — anywhere between :205 and the :975 gate block — so all five
storages are populated when read.) This converts the silent wrong-canonical
into the EXISTING loud refusal path (`UnrealInitializer.cpp:2418-2421`
throws with the `[TypeScan] canonical-mismatch` evidence line above it).

### B2 — Init-order fix (late re-verification)

If the audit shows the type-scan runs before `/Script` classes reach their
final state (timing/order), add a one-shot reseed at FIRST FindXOf use —
CAND `src/UObjectGlobals.cpp`, inside `MakeFindXOfClassName` (:517-526):

```diff
     static auto MakeFindXOfClassName(StringViewType ClassName) -> FName
     {
+        // l26 walkfix-B2: first-use canonical re-verification. <<PROVENANCE_SLOT_B>>
+        static const bool canonicals_reseeded = [] {
+            if (TypeChecker::filter_canonicals_verified_once()) { return true; } // B1 helper
+            Output::send<LogLevel::Warning>(STR("[FFO] canonical reseed before first lookup\n"));
+            (void)TypeChecker::store_all_object_types(); // idempotent reseed; throws on refusal
+            return true;
+        }();
+        (void)canonicals_reseeded;
         auto Name = FName(ClassName, FNAME_Find);
```

Use ONLY if the audit demonstrates an ordering window; B1 without B2 already
refuses loudly, and an unnecessary reseed walks 35K objects a second time.

### B3 — Seed slot (pinned canonical address, engine-binary provenance)

If the audit supplies a measured address of `/Script/CoreUObject.Class` (etc.)
from the engine binary (AA-style receipt), pin it in the profile so the seed
is identity-checked against ground truth rather than name-walk-only. Landing
shape (three layers + schema row, because it is a NEW native fact):

- `profile/profiles/townfall-build-facts.json`: append to `layout` facts
  `{"name": "CanonicalClass::UClass", "offset": <<PINNED_ADDR>>, "width": 8, "source": "<<SEED_EVIDENCE_SLOT>>"}`.
- `profile/units/CompiledNativeLayoutDeclarations.hpp` + `RuntimeBuildProfile.hpp`
  + `JoinedLayoutPreflight.hpp` rows likewise; digest recomputed (AS/AM law).
- NEW schema row in
  `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc`
  (`{"CanonicalClass","UClass","CanonicalClass","UClass",false}`) —
  REQUIRED: schema-less INI keys throw `Unknown native member` at
  `ue4ss/src/NativeLayoutBinding.cpp:72-74` (the UObjectDeleteListenersCritical
  0x78 lesson, profile-fix-template/PROCEDURE.md header).
- Consumer: TypeChecker.cpp seed site compares the walk-resolved pointer to
  the pin; mismatch → refuse (reuse B1's printout + return false).

NOTE: a canonical-address pin is image-specific (slide 0x400000 fixed for
this eboot); it must be gated on the existing image-SHA identity check, same
as the shadow-corroboration arm. Do NOT pin without that gate.

### B4 — FName-form seeding variant

If the audit shows `StaticFindObject_InternalSlow`'s `GetFullName()` render
mis-matches (ToString/number-form), switch the FIVE filter canonicals to the
name-parts walk that the init required-objects path already proved on console
(`StaticFindObject_InternalNoToStringFromNames`, CAND
`src/UObjectGlobals.cpp:208-255`, FName-parts compare, `FNAME_Find`):

```diff
-        UClass* class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Class")));
+        // l26 walkfix-B4: FName-parts resolution (FNAME_Find), no ToString in the
+        // loop - the mode the init required-objects walk proved on console. <<PROVENANCE_SLOT_B>>
+        const std::vector<FName> parts{ FName(STR("/Script/CoreUObject"), FNAME_Find), FName(STR("Class"), FNAME_Find) };
+        UClass* class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalNoToStringFromNames(parts));
```

(Verify the exact outer-part spelling against how `StaticFindObject_InternalNoToStringFromStrings`
is called today — CO §4 documents the init successes with this comparator.)

---

## 3. Verification command set

```bash
# 1. Walk harness: extend fixtures with (i) correct canonicals -> PlayerController
#    found; (ii) WRONG canonical (UClass::StaticClassStorage -> mock Object class)
#    -> old code: nothing found (reproduces bug); B1 code: [TypeScan]
#    canonical-mismatch + refuse; D1 discriminator from CO section 5.
cd /tmp/l26-ufunc-harness && python3 extract_and_build.py && cat results/run.stdout.txt

# 2. Scan-refusal suite (TypeChecker edits must not disturb guarded scans).
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/tests/run_tests.py

# 3. Profile suite ONLY if B3 landed (new fact rows + digest).
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/profile/tests/run_tests.py

# 4. Compile + link proof with updated CHANGES rows.
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/build_runtime.py
```

Console acceptance signal (only under the authorized launch ladder): the
`[TypeScan] scan-complete ... core-ready=true` line STILL appears, no
`canonical-mismatch` line, and the FindFirstOf probes P1/P2 return valid.

## 4. CHANGES.json row shapes (typescan-r4 part)

B1/B2/B4 edit existing candidate files — rows already exist; recompute
`candidate_sha256` only:

```json
{
  "original": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/TypeChecker.cpp",
  "candidate": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/TypeChecker.cpp",
  "original_sha256": "c05b06a7db9a50976f7b2d783c9f196b6b74698b872605e978c8af6f1f20ac7d",
  "candidate_sha256": "<<RECOMPUTED_AFTER_EDIT>>"
}
```

(same shape for `src/UObjectGlobals.cpp`, original
`outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp`, sha
`9b335a5195f91fec...` — copy the full value from the live CHANGES row.)

B3 additionally appends the profile-part rows (four files, originals = the
frozen l25 pins in the live `profile/CHANGES.json`) with recomputed
`candidate_sha256`, a NEW `<<PROFILE_DIGEST_NEW>>`, `required_members`
incremented by the number of pinned canonicals, and a
`walkfix_b_pinned_canonicals` provenance key naming the audit receipt.
