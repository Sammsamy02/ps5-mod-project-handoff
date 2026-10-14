# TEMPLATE A — ObjectFlags (or another walk member) read at a wrong offset

Candidate root cause A: the walk's per-object filter reads garbage because a
member offset is wrong. As traced below, there are TWO structurally different
reads in the filter chain, and they get their offsets from DIFFERENT sources:

- **A1 — UObjectBase::ObjectFlags (via `HasAnyFlags`): HARDCODED `0x8` in
  code. The profile row exists but is NOT consumed by this read.** A profile
  correction alone does NOT fix the walk; the fix is code-level (or routing
  the read through the profile-driven accessor).
- **A2 — any profile-driven member (FUObjectItem::Flags/Object/UEP_TotalSize,
  TUObjectArray::*, UObjectBase::ClassPrivate/NamePrivate/OuterPrivate,
  UStruct::SuperStruct): profile-driven.** The AS/AM five-layer correction
  applies verbatim.

Pick the variant the audit verdict names. Fill slots from `values.json`.

---

## 1. Where each read gets its value NOW (exact files + lines)

Walk filter entry — l26 candidate
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp:267-271`
(`IsValidObjectForFindXOf`):

```cpp
return !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)) && !object->IsA<UClass>() && ...
```

Item-level skip (Unreachable) — same file, `ForEachUObject_Chunked :1021-1052`
(`item->Flags`/`!Object` skip at ~:1039-1040), stride from
`FUObjectItem::UEP_TotalSize()`.

### A1 chain (ObjectFlags read is HARDCODED)

| Step | File:line (PORT unless CAND) | What it does |
|---|---|---|
| 1 | CAND `src/UObjectGlobals.cpp:269` | `object->HasAnyFlags(0x30)` |
| 2 | PORT `unreal/include/Unreal/UObject.hpp:267-270` | `Container::UnrealObjectVC->UObject_has_any_flag(this, FlagsToCheck)` |
| 3 | PORT `unreal/src/VersionedContainer/Container.cpp` (5.x branches) | engine 5.6 → `UnrealObjectVC = make_unique<UObject425>()` |
| 4 | PORT `unreal/src/VersionedContainer/UObject425.cpp:23-26` | `Flags::has_any_flag(p_this, Flags425::to_impl_flags(...))` |
| 5 | PORT `unreal/include/Unreal/VersionedContainer/Flags425.hpp:20-21` | `to_impl_flags` identity for `RF_ClassDefaultObject=0x10`/`RF_ArchetypeObject=0x20` |
| 6 | **PORT `unreal/include/Unreal/VersionedContainer/Flags.hpp:11-15`** | `get_flags`: `ptr_cast<FlagsType*>(p_this, 0x8)` — **HARDCODED offset 0x8**, comment line 13: *"TODO: Hard-coding this for now."* |

**The profile `[UObjectBase] ObjectFlags=0x8` row is published into
`UObjectBase::MemberOffsets` (consumed by the generated `GetObjectFlags()`,
PORT `unreal/generated_include/MemberVariableLayout_SrcWrapper_UObjectBase.hpp`)
but `HasAnyFlags` never calls that accessor.** If the true engine offset for
`ObjectFlags` differs from 0x8, the filter reads some other dword; if that
garbage dword happens to intersect bits 0x10/0x20, EVERY object is excluded
→ walk finds nothing. (CO §3.5 rated this "ruled out" for the CURRENT 0x8
value on binary evidence `[obj+0xC] InternalIndex / [obj+0x18] NamePrivate`
— a verdict that ObjectFlags is wrong must present fresh engine-binary
evidence contradicting that, e.g. a disassembled flag read at another
UObjectBase offset.)

### A2 chain (profile-driven members)

Generated accessor pattern (example `FUObjectItem::Flags`):
CAND `src/UObjectArray.cpp:169-186` (`GetFlagsInternal` → `GetFlags()` for
`Version::IsAtLeast(4,13)` = our 5.6) → generated
`MemberVariableLayout_SrcWrapper_FUObjectItem.hpp` →
`FUObjectItem::MemberOffsets["Flags"]` → published at startup by
`ue4ss/src/NativeLayoutBinding.cpp:175-199` (`PublishStagedNativeLayoutMaps`)
from the profile INI text in
`profile/units/CompiledNativeLayoutDeclarations.hpp` (string[0],
`[FUObjectItem] Flags=0x8`, `[UObjectBase] ClassPrivate=0x10 NamePrivate=0x18
ObjectFlags=0x8 ...`, `[UStruct] SuperStruct=0x40`), keyed through the schema
at `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc`
(rows `{"UObjectBase","ObjectFlags","UObjectBase","ObjectFlags",false}` etc. —
all A2 members already HAVE schema rows, so no schema edit is needed).

Boot enforcement: `profile/units/RuntimeBuildProfile.hpp` `kRequiredMembers[]`
(row `{"UObjectBase::ObjectFlags",true,8,4,"<source>"}` = offset 8, width 4)
+ `profile/units/JoinedLayoutPreflight.hpp:19-23` (joined rows for
ClassPrivate/NamePrivate/OuterPrivate/ObjectFlags/InternalIndex_Private) are
evaluated at startup by CAND `src/UnrealInitializer.cpp:1384-1415` and REFUSE
on mismatch.

---

## 2. Fill-in patch templates

### Variant A-code — HasAnyFlags offset override (A1)

New candidate header (mirrors PORT include spelling):

`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include/Unreal/VersionedContainer/Flags.hpp`
(copy of PORT Flags.hpp, sha `ecfed956...`, with `get_flags` changed):

```diff
     template<ValidEnumFlagType FlagsType>
     auto get_flags(void* p_this) -> FlagsType*
     {
-        // TODO: Hard-coding this for now. Consider not hard-coding this.
-        return Helper::Casting::ptr_cast<FlagsType*>(p_this, 0x8);
+        // l26 walkfix-A: ObjectFlags offset was hardcoded 0x8; <<PROVENANCE_SLOT_A>>
+        // Engine-binary evidence: <<EVIDENCE_RECEIPT_SLOT_A>>
+        return Helper::Casting::ptr_cast<FlagsType*>(p_this, <<OBJECTFLAGS_TRUE_OFFSET>>);
     }
```

Slots:
- `<<OBJECTFLAGS_TRUE_OFFSET>>` — decimal, from values.json `objectflags_true_offset` (e.g. `0x10`).
- `<<PROVENANCE_SLOT_A>>` — one-line source citation (audit report path + sha).
- `<<EVIDENCE_RECEIPT_SLOT_A>>` — engine-binary disassembly receipt path + sha (AA-style).

ALTERNATIVE (safer, keeps a single source of truth): route the VC read
through the profile-driven accessor instead of a new constant —
new candidate `typescan-r4/candidate/src/VersionedContainer/UObject425.cpp`
(copy of PORT, sha `daa27fcc...`):

```diff
     auto UObject425::UObject_has_any_flag(void* p_this, const EObjectFlags flags_to_check) -> bool
     {
-        return Flags::has_any_flag(p_this, Flags425::to_impl_flags(flags_to_check));
+        // l26 walkfix-A: read through the profile-published UObjectBase::ObjectFlags
+        // accessor so the walk filter and the [UObjectBase] profile row can never
+        // disagree. Falls back to the legacy hardcoded read only pre-publication.
+        auto* obj = static_cast<Unreal::UObjectBase*>(p_this);
+        const auto impl = Flags425::to_impl_flags(flags_to_check);
+        using Underlying = std::underlying_type_t<decltype(impl)>;
+        return (static_cast<Underlying>(obj->GetObjectFlags()) & static_cast<Underlying>(impl)) != 0;
     }
```

(`GetObjectFlags()` throws if `MemberOffsets["ObjectFlags"]` is missing —
acceptable: startup publishes the profile before any walk; the preflight at
UnrealInitializer.cpp:1384-1415 already refuses otherwise.) If the audit
instead convicts the ITEM-level read, apply the same routing to
`FUObjectItem::GetFlagsInternal` (CAND `src/UObjectArray.cpp:169-186`)
— but that one already goes through the generated accessor; for it use
variant A-profile below.

### Variant A-profile — profile value correction (A2, AS/AM five-layer)

Applies when the audit convicts a profile-driven member M
(`FUObjectItem::Flags`, `UObjectBase::ClassPrivate/NamePrivate`, etc.).
Five files move TOGETHER (digest is recomputed last):

**(1) `profile/profiles/townfall-build-facts.json`** — member row
(currently e.g. `{"name": "UObjectBase::ObjectFlags", "offset": 8, "width": 4, "source": "outputs/2026-10-07-l19-static-layout-check/STATIC-FACTS.json:UObjectBase.ObjectFlags"}`):

```diff
-    {"name": "<<MEMBER_DOTTED>>", "offset": <<OLD_OFFSET>>, "width": <<WIDTH>>, "source": "<<OLD_SOURCE>>"},
+    {"name": "<<MEMBER_DOTTED>>", "offset": <<TRUE_OFFSET>>, "width": <<WIDTH>>, "source": "<<NEW_SOURCE_SLOT>>"},
```

**(2) `profile/units/CompiledNativeLayoutDeclarations.hpp`** — string[0] INI
text, section `<<SECTION>>` (keep keys sorted; hex, DERIVED from decimal):

```diff
 [<<SECTION>>]
-<<KEY>>=0x<<OLD_OFFSET_HEX>>
+<<KEY>>=0x<<TRUE_OFFSET_HEX>>
```

**(3) `profile/units/RuntimeBuildProfile.hpp`** — `kRequiredMembers[]` row
`{"<<MEMBER_DOTTED>>",true,<<OLD_OFFSET>>,<<WIDTH>>,"<<OLD_SOURCE>>"}` →
same substitution with `<<TRUE_OFFSET>>` + `<<NEW_SOURCE_SLOT>>`.

**(4) `profile/units/JoinedLayoutPreflight.hpp`** — if the member has a
joined row (`{"UObjectBase::ObjectFlags",Kind::Member,Disposition::Required,true,8,...}`),
update the offset literal identically. (A joined row whose value changes
without this edit = boot refusal at [LayoutPreflight].)

**(5) digest** — recompute
`sha256(json.dumps(patched_json, sort_keys=True, separators=(',',':')))`
and write it to: `profile/CHANGES.json.profile_digest`,
`CompiledNativeLayoutDeclarations.hpp` string[3], `RuntimeBuildProfile.hpp`
`kDigest`. (Reuse `profile-fix-template/apply_engine_binary_fix.py` /
`instantiate_profile_fix.py` mechanics — they already implement exactly this
drop path; see `outputs/2026-10-08-l26-runtime/profile-fix-template/PROCEDURE.md`.)

NOTE (A-profile does NOT fix the HasAnyFlags read): correcting the
`[UObjectBase] ObjectFlags` profile value only changes
`UObjectBase::GetObjectFlags()` consumers. If the convicted read is the
walk's `HasAnyFlags`, you MUST also land variant A-code. Landing both is
the safe default when the audit names ObjectFlags specifically.

---

## 3. Verification command set

```bash
# 1. Profile suite (if A-profile landed): preflight10, INI total-size guard,
#    digest pair+negative compile, strict join controls, retained-facts immutability.
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/profile/tests/run_tests.py

# 2. Scan-refusal suite (unchanged behavior of guarded scans).
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/tests/run_tests.py

# 3. Walk harness (CD's verbatim-region harness): add/extend a fixture where the
#    OLD offset yields garbage intersecting 0x30 (all excluded) and the NEW
#    offset yields sane flags (PlayerController found). The harness already
#    extracts is_valid_object_for_findxof.inc + the walk; edit template.cpp.in
#    + regions manifest, then:
cd /tmp/l26-ufunc-harness && python3 extract_and_build.py && cat results/run.stdout.txt

# 4. Compile + link proof with the new CHANGES rows (two passes, full import graph).
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/build_runtime.py
```

Harness pass criteria for A: with `<<TRUE_OFFSET>>` the fixture object with
clean flags passes `IsValidObjectForFindXOf`; a CDO-flagged fixture (0x10)
is excluded; with the OLD offset the mock must demonstrably exclude
everything (reproduces walk-finds-nothing) — that negative control is the
proof the fix addresses the convicted mechanism.

## 4. CHANGES.json row shapes

### A-code (typescan-r4 part; NEW header — original is the PORT file the base consumed)

Append to `files[]` in `outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json`:

```json
{
  "original": "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/VersionedContainer/Flags.hpp",
  "copied_original": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/original/include/Unreal/VersionedContainer/Flags.hpp",
  "candidate": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include/Unreal/VersionedContainer/Flags.hpp",
  "original_sha256": "ecfed9560854e0658b77c20e05f2f4dbd4cb2023a704d72ce6436987875e0104",
  "candidate_sha256": "<<RECOMPUTED_AFTER_EDIT>>",
  "copied_original_sha256": "ecfed9560854e0658b77c20e05f2f4dbd4cb2023a704d72ce6436987875e0104"
}
```

(The UObject425.cpp alternative uses original = PORT
`unreal/src/VersionedContainer/UObject425.cpp`, sha `daa27fcc...`, same shape.)

### A-profile (profile part; originals stay the frozen l25 pins — Z9 law)

Rows already exist for all four profile files; ONLY `candidate_sha256` and
the digest-family fields change:

```json
{
  "original": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/profile/profiles/townfall-build-facts.json",
  "candidate": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/profile/profiles/townfall-build-facts.json",
  "original_sha256": "95d22dbe494a225b53e1df78864d66e34a4345adf962eee5e413909b9e49b6dd",
  "candidate_sha256": "<<RECOMPUTED_AFTER_EDIT>>"
}
```

…plus the same substitution for the three `units/*.hpp` rows, plus part-level
`profile_digest: <<NEW_DIGEST>>`, plus a `walkfix_a_corrected_facts` count and
a scope sentence naming the audit receipt. If a member row's source citation
changes, `prior_facts_retained` drops by the corrected count exactly as the
AS/AM listener fix did (93−5=88 precedent in the live CHANGES).
