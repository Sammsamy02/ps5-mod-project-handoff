# profilegen: per-game runtime profile generator

`profilegen` reads a game's **decrypted PS5 executable** (ELF, `e_type` 0xFE10, scrambled dynamic symbol names, no sections) and its
`param.json`, and writes a **runtime profile**: everything UE4SS normally gets from signature scans, `VTableLayout.ini`,
`MemberVariableLayout.ini` and `UE4SS-settings.ini`, plus the PS5 facts (load base, file mapping, calling conventions).
Every value carries a confidence, the method that found it and the concrete facts that support it.

* Status of everything it writes is `"generated"`. It never writes `console-verified`, never writes a `verification` block, and refuses to
  overwrite a profile that already carries one (exit 3; `--force` replaces it and discards the verification).
* No console contact, no network, nothing copied out of the inputs (they are hashed, not stored).
* **No per-game value lives in the code.** Every detector looks for engine-generic strings, string cross-references, instruction shapes,
  structural invariants and vtable shapes. `HardRuleTests` scans this directory for the Townfall values and fails if one appears.
* Schema name: `modbridge.runtime-profile/1` (**proposed**; the contract is drafted in `docs/profiles/PROFILE-CONTRACT-PROPOSAL.md`,
  and the section and key names below follow that draft and the keys of `docs/profiles/TOWNFALL-ORACLE.md`).

## Usage

```sh
# from the project root (outputs/ps5-mod-bridge)
python3 -m tools.profilegen --eboot PATH/decrypted-eboot.bin --param PATH/param.json --out profiles/PPSA33286/02.003.000
```

| option | meaning |
|---|---|
| `--eboot`, `--param`, `--out` | decrypted executable, `param.json` (title id / content version binding; optional but recommended), output folder |
| `--load-base 0x...` | runtime load base if you have observed it (confidence becomes `high`, method `load.cli_override`) |
| `--game-profiles DIR` | folder of `modbridge.game-profile/1` files (`examples/game-profiles`) for the `version_map` row |
| `--only NAME` | run only that detector (repeatable): `platform build_axes engine_version fname_ctor fname_pool gmalloc uobject_array static_construct static_find_object process_event class_registration uobject_base process_event_members class_default_object field_classes property_subtypes engine_tick fname_tostring gnatives layout_merge` (`layout_merge` is added automatically when a layout detector is selected; the layout detectors read facts the earlier ones leave, so select those too) |
| `--strict` | let a crashing detector raise instead of recording `detector.<name>` in `unresolved` |
| `--utc ISO` | fix `generated.utc` so two runs are byte-identical |
| `--no-evidence`, `--quiet`, `--force` | skip `evidence/`, silence the summary, overwrite a verified profile |
| `--emit-seedtable PATH` | also write `SeedTable.h` (SEED-TABLE-FORMAT.md v2) to PATH; listed in `emit[]` when PATH is inside `--out`; never written unless asked for (see "SeedTable.h") |
| `--seed-gmalloc dispatch\|base_slot` | which GMalloc variable slot 3 seeds: `dispatch` (default, the generator's value: what `FMemory::Malloc`/`Free` load) or `base_slot` (the raw allocator pointer); the header names both |
| `--seed-heap-span unbounded\|omit\|LO:HI` | `kHeapLo`/`kHeapHi` (retired envelope; the profile has none): unbounded (default), dropped, or a range you supply |

Output folder:

```
runtime-profile.json            the document (UTF-8/ASCII, LF, 2-space indent)
ue4ss/VTableLayout.ini          [NativeLayout] header + [UObject] ProcessEvent=0x... + [UEngine] Tick + [FProperty] ... (medium or better)
ue4ss/MemberVariableLayout.ini  header + [FUObjectArray] + [TUObjectArray] + the reflection classes (see "Layout detectors") + a comment block
                                naming every member that was left out and why
ue4ss/UE4SS-settings.ini        fragment: [EngineVersionOverride] MajorVersion/MinorVersion (+ DebugBuild/Stats when high)
evidence/*.json                 per-detector detail (call sites, candidates, access histograms) and timing.json
```

Exit codes: 0 done (unresolved values are listed, not fatal), 2 unreadable/invalid input (nothing is written), 3 refused to overwrite a verified profile,
4 `--emit-seedtable` could not be honoured (the profile itself was written), 5 the executable is an encrypted SELF (refused before any analysis).
Compare a profile with an oracle: `python3 -m tools.profilegen.validate --profile DIR/runtime-profile.json --oracle oracle.json [--notes notes.json] [--markdown]`
(the oracle is run-time data; keys are dotted paths into the profile).
Python: `tools.profilegen.cli.generate(eboot, param, out)` returns a `Result` with `.doc`, `.ctx`, `.timings`.

Requirements: Python 3.9+. **capstone** (`import capstone`) is needed by every detector that decodes instructions; without it those
detectors report `detector.<name>: unavailable` in `unresolved` and the rest still run (platform, build axes, build id, load base, and an
engine version from the marker fingerprint at `medium`). `lief` is not needed. numpy is not needed.

## The document

Top level: `schema`, `profile_id` (`<title id>/<content version>/<sha8>`), `status`, `profile_digest` (sha256 of the canonical JSON of
`binding elf load build symbols structs vtables ue4ss`; excludes timestamps, `emit`, `unresolved`, `status`), `verification` (always `null`),
`generated`, then the data sections, then `emit` (the written files: path, bytes, sha256) and `unresolved` (`id`, `why`, `needs`).

**Every leaf of a data section is an envelope** `{"value", "confidence", "method", "evidence": ["...", ...]}`:
`value` is a string/bool/list/dict (numbers are lowercase `0x` strings, negative `-0x20`), `method` a stable id (`fname_pool.init_ids`),
`evidence` the facts the generator observed (addresses, counts, matched patterns). A test walks the whole document and fails on a bare value.
Addresses: `label` = ELF vaddr; `runtime` = `label + load.base` (always derived by the assembler, with the weaker of the two confidences).
The file offset is **never** the vaddr: `load.file_offset_rule` states the per-segment delta (a PT_LOAD can sit before its own vaddr in the file).

| section | leaves (path suffix) |
|---|---|
| `binding` | `title_id content_version master_version content_id title_name sdk_version param_json.{sha256,bytes} executable.{sha256,bytes}` |
| `elf` | `e_type e_machine e_entry e_phoff e_phentsize e_phnum osabi pointer_size section_table_present dt_rela.label rela.{entries,relative_entries,size_bytes} dynamic.label note.gnu_build_id.label procparam.* segments.ph<N>` (one envelope per program header whose value is `{type, p_flags, flags, p_offset, p_vaddr, p_filesz, p_memsz, file_delta, ...}`) |
| `load` | `base file_offset_rule image_extent.{start,end,vaddr_end}` |
| `build` | `configuration non_shipping stats configuration_margin stats_margin axes_markers_present/total` `engine.{major,minor,patch,changelist,branch,revision_string,store_site.label,branch_string.label}` `build_id allocator_family` |
| `symbols.<name>` | `label` (vaddr) and `runtime`, plus per-symbol extras (`abi`, `prologue_*`, `callsites`, `anchor_*`). Names: `object_array name_pool name_pool_init_flag fname_ctor_wchar fname_intern fnamepool_init fnamepool_reserve fname_default_name_store gmalloc gmalloc_effective_ptr gmalloc_base_slot gmalloc_instance gmalloc_instance_ctor gmalloc_factory gmalloc_bootstrap_store_site fmemory_malloc fmemory_free static_construct_object_internal sco_params_ctor assert_if_in_constructor static_find_object process_event class_registration_helper uclass_vtable allocate_uobject_index create_default_object ffield_class_ctor engine_tick (+ prologue_23, anchor_xref_label) fname_to_string fname_to_string_value gnatives fframe_step` |
| `structs` | `FName.size`, `FNamePool.*` (offsets, bucket table, ids, block geometry), `FNameEntry.header`, `FUObjectArray.*`, `TUObjectArray.*` (+ `elements_per_chunk`), `FUObjectItem.stride`, `UObject.sizeof`, and one group per reflection class: `UObjectBase FUObjectItem UField UStruct UClass UFunction FField FFieldClass FProperty FBoolProperty FStructProperty FArrayProperty FEnumProperty` (members as named by the port's `NativeLayoutSchema.inc`, `UEP_TotalSize`), `UScriptStruct UEnum UWorld AActor AGameModeBase AGameMode UPlayer ULocalPlayer UGameViewportClient UDataTable` and more property classes (`UEP_TotalSize` only) |
| `vtables` | `UObject.{ProcessEvent,ProcessEvent_slot_index,GetFunctionCallspace,CallRemoteFunction,dtor_prefix_slots}`, `FMalloc.{Malloc,Free}`, `UEngine.{Tick,Tick_slot_index}`, `FProperty.{DestroyValueInternal,InitializeValueInternal,GetMinAlignment}`, `UClass.CreateDefaultObject` |
| `version_map` | `pc_build` copied from the title's `modbridge.game-profile/1` row (null plus an `unresolved` entry when absent) |

### Confidence

* **high**: two independent channels agree (for example a string-anchored function cluster *and* a behavioural idiom on the same global), or one
  structural invariant that cannot be met by accident (for example an ELF note read byte for byte), with a unique winner.
* **medium**: one deterministic method with a self-check (unique match, byte re-verification) or a convention that is corroborated but cannot be
  observed from the file; also anything the schema/declaration order fixes from observed neighbours.
* **low**: a heuristic, population statistic, ordering assumption or convention only. A value that could not be found is **not** invented: it is
  absent and listed in `unresolved`.

The runtime address of every symbol is therefore at most **medium** while `load.base` is a platform prediction.

## Detectors (method per value)

| detector | produces | method |
|---|---|---|
| `platform` | `binding.*`, `elf.*`, `load.*`, `build.build_id` | Program-header table (`vaddr <-> offset` per PT_LOAD), RELA table counts, PT_NOTE `NT_GNU_BUILD_ID` (Sony's 16-byte id is padded with 4 zero bytes to a 20-byte note; only that pad is dropped), SCE process-param block. **Load base**: nothing in the file encodes it (linked at 0, only image-relative RELATIVE relocations), so it is the PS5 platform convention (the main executable is mapped at a fixed 4 MiB base; the constant is written as a size in `platform.py`) at **medium**; `--load-base` overrides it at high. A non-SCE_DYNEXEC image gets no base. |
| `build_axes` | `build.configuration/non_shipping/stats` + margins | Port of patternsleuth's `BuildConfiguration`/`Stats` resolvers: 244 UTF-16 marker strings with `#if` gates and engine-version windows (`data/engine_markers.json`, regenerated by `data/gen_engine_markers.py` from patternsleuth's `engine_version.rs`, MIT OR Apache-2.0). Cost model: contradiction 5, omission 1; an axis is reported only if the best candidate beats every candidate differing on it by 4 and >= 10 markers are present. Also yields a version estimate. |
| `engine_version` | `build.engine.*` | `mov dword [m], (minor<<16)\|major` + `mov word [m+4], patch` + a write to `[m+8]` (patternsleuth's `scan_version_stores` shape, decoded with capstone); fallbacks: a NUL-delimited `M.m.p-...+...` string, a `++UE5+Release-M.m` branch string, the marker fingerprint (medium only). Changelist and branch: a tiny interpreter replays the function from its start to the `[m+8]` store, resolving calls to leaf functions (`xor eax,eax; ret`, `lea rax,[rip+str]; ret`). Revision tag `<major>.<minor>.<patch>-<changelist>+++<branch>` is a documented join (UE builds its own string at run time). |
| `fname_ctor` | `symbols.fname_ctor_wchar.*`, `fname_intern`, `structs.FName.size` | UE module names (`Engine Renderer AnimGraphRuntime Landscape RenderCore`, widened with `Core Slate ...` when needed) are registered as `lea rsi,[L"Name"]; lea rdi,[stack]; mov edx,EFindName; call ctor`. Every rip-relative reference to each name is decoded; the call target the *different* names agree on is the constructor (high needs >= 4 anchors, a unique winner, a function start and every call site re-verified as `E8 rel32`). ABI evidence: stack out-pointer in rdi, literal in rsi, `edx` immediates at call sites, whether the body reads `edx`. |
| `fname_pool` | `symbols.name_pool*`, `fnamepool_*`, `fmemory_malloc`, `structs.FNamePool.*` | The function referencing the most distinct default-name literals (`None ByteProperty IntProperty ...`) is `FNamePool::Init`; its `memset(pool+A, 0, S)`, `Malloc(block, align)` + `mov [pool+D], rax`, the id stores after the `"None"`/`"ByteProperty"` stores and the hash-bucket loop give the layout; the pool is the `lea rdi,[rip+X]` at Init's call sites, the init flag the `cmp byte [rip+F],0` guarding them. Cross-checked with the (smallest) block-growth function (reads `CurrentBlock`/`CurrentByteCursor`, zeroes the cursor, stores `Blocks[CurrentBlock]`) and with the readers (`id >> 16`, `movzx word [block+off*2]`). `Malloc` is the callee of the block allocation. |
| `gmalloc` | `symbols.gmalloc*`, `fmemory_free`, `vtables.FMalloc.*`, `build.allocator_family` | **GMalloc is what `FMemory::Malloc` and `FMemory::Free` actually load**: both are `global -> vtable -> call [vt+d]` dispatchers over the same variable (direct `mov` or `lea`+deref); `Free` is the most-called one-argument dispatcher with another vtable offset. The lazy-create function that stores into that variable gets its value from a one-instruction accessor returning a second global (`gmalloc_base_slot`, the raw allocator pointer ZCode seeded), its single bootstrap store, factory, instance and constructor. Allocator family: most frequent `FMalloc<Name>` identifier among UTF-16 strings. |
| `uobject_array` | `symbols.object_array*`, `structs.FUObjectArray/TUObjectArray/FUObjectItem` | Functions referencing `UObjectArray.cpp` strings (source path and fatal messages) are members of `FUObjectArray`; the writable global most of them take the address of is the singleton, confirmed by the `IndexToObject` idiom at its other reference sites. Layout from code: chunk-table load + `shr 16` + `[t+i*8]` gives `Objects` and the chunk size, a u32 compare 0x14 above gives `NumElements`, member accesses (`[this+d]` through `rdi`) give the prefix and `TUObjectArray` fields, TArray-shaped members (pointer at d, Num at d+8, elements indexed by 8) give the two listener arrays, the last byte member is `bShouldRecycleObjectIndices`, `PrimarySerialNumber` the int32 before it, and the structure ends at the next referenced global. No template is used. |
| `static_construct` | `symbols.static_construct_object_internal`, `sco_params_ctor` | `NewObject<T>` carries the literal "NewObject with empty name ..."; after each reference two consecutive calls receive the same stack struct in `rdi` (params ctor, then the factory). The pair seen in the most windows names both. |
| `static_find_object` | `symbols.static_find_object` | Functions referencing `Illegal call to StaticFindObject...` fatal messages; the one that walks `rdx` as UTF-16, compares `rsi` with -1 (`ANY_PACKAGE`) and carries every message. Capped at medium (one structural channel). |
| `process_event` | `vtables.UObject.*`, `symbols.process_event`, `structs.UFunction.FunctionFlags` | Vtables are read from `R_X86_64_RELATIVE` relocations (their file bytes are zero). Among functions shared by many vtables at the same slot, ProcessEvent tests `FUNC_Native` (0x400) on its second argument and makes the two adjacent virtual calls (`GetFunctionCallspace`, `CallRemoteFunction`). The slot byte offset is `slot * 8`; the Itanium two-slot destructor prefix is why it differs from Win64 templates. |
| `class_registration` | `symbols.class_registration_helper`, `structs.UStruct.{PropertiesSize,MinAlignment}`, `structs.UClass.{ClassFlags,ClassCastFlags,ClassWithin,ClassConfigName,ClassConstructor,ClassVTableHelperCtorCaller}`, `UEP_TotalSize` of the core and engine classes | Every native `UCLASS` registers through one helper (`GetPrivateStaticClassBody`): the UTF-16 package literal `/Script/<Module>` is loaded into `rdi`, the class-name literal into `rsi`, `sizeof`/`alignof` into `r8d`/`r9d`. The call target the *different modules agree on* is the helper (5,644 `(package, class)` rows on Townfall; reading them gives `sizeof` of every native class). A linear data-flow walk (`_flow.py`) of the helper says where each argument lands in the `UClass` it allocates (size dword -> `PropertiesSize`, alignment word -> `MinAlignment`, or-ed flags -> `ClassFlags`, 8th argument -> `ClassCastFlags`, result of the within-class function -> `ClassWithin`, `FName` constructor with `rdi = &member` -> `ClassConfigName`, the stack pair -> `ClassConstructor`/`ClassVTableHelperCtorCaller`); open-coded copies of the registration (`mov dword [x+PropertiesSize], size; mov word [x+MinAlignment], align` next to the name literal) are a second channel and give `UFunction`/`UClass`/`UScriptStruct` sizes. Only read when the engine is not older than UE 5 (the argument order differs before). |
| `uobject_base` | `structs.UObjectBase.*`, `structs.FUObjectItem.{Object,Flags,SerialNumber,UEP_TotalSize}`, `symbols.allocate_uobject_index` | `FUObjectArray::AllocateUObjectIndex` is the one `UObjectArray.cpp` member that carries all three fatal messages (disregard pool, maximum number, another object); every function that calls it with its own `this` as `rsi` is a `UObjectBase` constructor: the stores of its arguments give `ClassPrivate` (arg 1), `ObjectFlags` (arg 2), `OuterPrivate` (arg 4), `NamePrivate` (arg 5) and the constant -1 `InternalIndex`; a second constructor variant (name built by the `FName` constructor), the allocator's own index store and `ProcessEvent`'s reads corroborate. The allocator also fills the `FUObjectItem` (`Object`, atomic `Flags`, `SerialNumber`). `UObjectBase`'s size is the end of `OuterPrivate` when `sizeof(UObject)` (registration of the class named `Object`) is exactly that. |
| `process_event_members` | `structs.UFunction.*`, `structs.UStruct.{Script,PropertiesSize,MinAlignment,ChildProperties,DestructorLink}`, `structs.FField.Next`, `structs.FProperty.*`, `vtables.FProperty.{DestroyValueInternal,InitializeValueInternal}` | `ProcessEvent` touches every layer in a source-defined way, so each member is found by the *role* of its access (data-flow walk with `this`/`Function` tagged): `Script.Num` is the dword compared with 0 after the `FUNC_Native` test, `ParmsSize` the word that is the byte count of the parameter `memcpy`, `PropertiesSize` the dword that with it forms the byte count of the `memset`, `MinAlignment` the word compared with 16, `ReturnValueOffset` the word compared with `0xFFFF`, `EventGraphFunction` the qword whose target's flags are tested, `ChildProperties` the chain head stored into the frame, `DestructorLink` the chain whose loop reads `ParmsSize`, `FirstPropertyToInit` the other one, the links the loops follow give `FField::Next`/`DestructorLinkNext`/`PostConstructLinkNext`, `Offset_Internal` the dword every loop reads, `ArrayDim`/`ElementSize` the two it multiplies, `Func` the one indirect call `UFunction::Invoke` makes through its argument. |
| `class_default_object` | `structs.UClass.ClassDefaultObject`, `structs.UStruct.SuperStruct`, second channel for `ChildProperties`, `FField.{Next,ClassPrivate}`, `FFieldClass.CastFlags`; `vtables.UClass.CreateDefaultObject` | The `UClass` vtable (installed by the registration helper) holds one function that passes `RF_Public\|RF_ClassDefaultObject\|RF_ArchetypeObject` (0x31) to a call and stores the result back into `this` after comparing that field with 0: `ClassDefaultObject`. The pointer loaded from `this` whose own field is then read at the same offset is the parent class (`SuperStruct`). The field loop gives the chain head and link, and the two-level load whose result a 64-bit cast mask is applied to is `field->ClassPrivate->CastFlags`. |
| `field_classes` | `structs.FFieldClass.*`, `structs.FField.{Owner,NamePrivate,FlagsPrivate,Next,UEP_TotalSize}`, `structs.FProperty.{ArrayDim,ElementSize,UEP_TotalSize}`, subtype sizes | The names `FProperty` `FBoolProperty` `FIntProperty` ... are loaded into `rsi` before `FFieldClass::FFieldClass(this, Name, Id, CastFlags, Super, ConstructFn)`: the call target they agree on is the constructor, whose argument stores give the layout; `sizeof(FFieldClass)` is its extent rounded to 8, checked against the spacing of the static descriptors (0x48 apart = object + guard). `ConstructFn` is `new T(...)`: the constant it passes to `FMemory::Malloc` is `sizeof(T)`. The `FField(Owner, Name, Flags)` constructor is recognised by its stores; the inline `FProperty` constructor's single 8-byte immediate `1` fixes `ArrayDim` and the order of `ElementSize`; `sizeof(FField)` follows from the Itanium rule that a derived class's first member (`ArrayDim`) sits at the base's data size. |
| `property_subtypes` | `structs.{FStructProperty.Struct,FArrayProperty.{Inner,ArrayFlags},FEnumProperty.{UnderlyingProp,Enum},FBoolProperty.*}`, `vtables.FProperty.GetMinAlignment` | A subtype's vtable is the one its `ConstructFn` installs; the loads through `this` at or beyond `sizeof(FProperty)` in the virtuals it overrides (slots that differ from `FProperty`'s own) are its members. `GetMinAlignment` is the slot whose overrides return `1,4,4,8,8` for byte/int/float/int64/double; its struct/enum overrides forward to `Struct`/`UnderlyingProp` (second channel); the bool `InitializeValue` override (`*(Dest + ByteOffset) &= ~FieldMask`) confirms those two. |
| `engine_tick` | `symbols.engine_tick.*` (label, prologue, anchor), `vtables.UEngine.Tick` | `PURE_VIRTUAL(UEngine::Tick,)` leaves a stub that references the literal `UEngine::Tick`; the function that references `causeevent=` (patternsleuth's `gameengine_tick` anchor) is `UGameEngine::Tick`. Both sit at the same slot of two different RELA-read vtables; the byte offset is `slot * 8`. |
| `fname_tostring` | `symbols.fname_to_string` (+ `_value`) | Among the functions that take the address of the `FNamePool` and index its `Blocks` array: the one that reads `Number` at `[rsi+4]`, returns its own `rdi` (hidden return slot) and has the most callers is `FString FName::ToString() const`; the callee it forwards to with `rdi`/`rsi` swapped is `void FName::ToString(FString&) const` (the form UE4SS binds). The fatal-message path of `AllocateUObjectIndex` calling the first one is a second channel. |
| `gnatives` | `symbols.gnatives`, `symbols.fframe_step` | The zero-initialised global that is the base of many `call [base + index*8]` sites whose index is a freshly loaded byte (`FFrame::Step` inlined everywhere), and the target of many absolute registration stores `mov [G + 8*k], reg` within 256 entries. |
| `layout_merge` | the envelopes of all of the above | `_book.py`: each detector records *observations*; one value from two independent channels is `high`, from one channel `medium` (or `high` for a structural invariant such as an allocation size), disagreeing channels publish nothing and say so. Then declared-order fits (a member that fills a measured gap exactly: `Children`, `PropertyLink`/`RefLink`, `NumParms`, `RPCId`/`RPCResponseId`, `UField::Next`, `RepIndex`, `PropertyLinkNext`/`NextRef`, `RepNotifyFunc`, `ClusterRootIndex`/`RefCount`) at `medium`, and every accepted key of the scoped classes that stayed empty is listed in `unresolved` with the reason. |

The UE4SS files (`emit_ue4ss.py`) are rendered from the JSON, never from detector internals: a file is written only when the identity header
(`EngineMajor/Minor`, `NonShipping`, `Stats`, `EngineRevision`, `BuildId`) is complete at `medium` or better; a class only when *all* its members are
`medium` or better; hex is `0x` + upper-case digits; whole-line `;` comments only. `DebugBuild`/`Stats` are pinned in the settings fragment only at `high`.
PS5 policy keys (`[Hooks]`, `bUseUObjectArrayCache`, ...) are not a property of the image and are left to the fixed policy template.
The **extended** layout (what `emit` writes; `render_*_layout(doc)` without `extended=True` renders only the original classes and is what the staged-pair
comparison uses) appends the reflection classes after `[TUObjectArray]`: a class only with a `UEP_TotalSize` at `medium` or better (the port refuses a class
without a positive size and any member at or beyond it), only members at `medium` or better, only keys the port's binding accepts and only their canonical
names (`data/native_layout_schema.json`, generated by `data/gen_native_layout_schema.py` from the port's `NativeLayoutSchema.inc`; names only), and a
whole-line `;` comment block at the end naming every member left out (`confidence low`, `unresolved`, class not written) with the reason. The vtable file
gets `[UEngine] Tick` and `[FProperty] ...` only for classes the port has a vtable section for.

## SeedTable.h (for a runtime that keeps compiling its seeds)

Whether the runtime reads `runtime-profile.json` or keeps a generated `SeedTable.h` is the runtime owner's decision; both are supported.
`emit_seedtable.py` renders the header in the layout `gen_seed_table.py` writes (guard `RC_SEED_TABLE_H_GENERATED_<base ^ pool label>_`,
`kGenerated`, `kBaseObserved`, `kFNamePoolLabel`, `kHeapLo`/`kHeapHi`, the GMalloc validator constants, `struct SlotSeed`, the four slots in the
order `kGUObjectArray` (0, data), `kFNameCtorWchar` (2, code), `kGMalloc` (3, data), `kStaticConstructObject` (4, code), `kRequiredSetComplete`,
`kImageSha256`) from the profile only:

```sh
python3 -m tools.profilegen --eboot ... --param ... --out DIR --emit-seedtable DIR/runtime/SeedTable.h [--seed-gmalloc base_slot] [--seed-heap-span omit]
python3 -m tools.profilegen.emit_seedtable --profile DIR/runtime-profile.json --out SeedTable.h   # from an existing profile
python3 -m tools.profilegen.diff_seedtable --profile DIR/runtime-profile.json --current <the runtime's SeedTable.h> --markdown docs/profiles/SEEDTABLE-DIFF.md
```

* A slot is `present` only with a label at `medium` or better; otherwise it is written absent (`false`, label 0, empty derivation, a comment with
  the reason) and `kRequiredSetComplete` is `false`, which the v2 policy treats as a loud refusal. The header is refused (exit 4) only without the load
  base, the FNamePool label or the executable hash.
* The derivation string is the compact JSON `{anchor_bytes, rip_arithmetic, uniqueness, method, confidence, source}` built from the profile's
  method and evidence (prologue bytes for code slots), escaped as a C++ literal with the backslash first, so the compiled string is exactly that JSON.
* **GMalloc is never picked silently.** The default is the generator's value (`symbols.gmalloc`); `--seed-gmalloc base_slot` seeds
  `symbols.gmalloc_base_slot` instead. The header carries a comment with the choice and the value that was not seeded; if the chosen variable is missing the
  slot is written absent rather than falling back to the other.
* Constants the profile cannot supply are handled explicitly, not invented: `kHeapLo`/`kHeapHi` (retired envelope) are emitted as an unbounded range with a
  comment unless `--seed-heap-span` says otherwise; `kGMallocVtableLabel` is left out with a comment while no `symbols.gmalloc_vtable` exists
  (`kGMallocInstanceLabel` comes from `symbols.gmalloc_instance`). A runtime build whose V2 validator reads `kGMallocVtableLabel` does not compile
  against such a header, which is the loud outcome; nothing is invented to make it compile.
* No status or verification of the profile is copied into the header, and profile text that would put a console-verification marker into it makes the
  emitter refuse. `docs/profiles/SEEDTABLE-DIFF.md` lists every difference from the runtime's current header on the Townfall image.

## Townfall validation (PPSA33286, content 02.003.000)

Run on the pinned executable (sha256 `3b6cfad4...622fc`, 194,241,424 bytes) and `param.json`: **~20-30 s** end to end on this Mac (8 cores, 506 MB peak RSS)
(`python3 -m tools.profilegen` prints the breakdown; typical: build_axes 4-8 s, fname_ctor 1-7 s, engine_version 2-4 s, index build 7-9 s,
the rest about 1 s each; index times are included in the first detector that needs them).
Of the 169 oracle keys in `docs/profiles/TOWNFALL-ORACLE.md` section 13: **150 match, 9 differ (documented below), 10 are not produced** (the four `engine_tick` keys, not produced by the first version, now match).
Full comparison: `python3 -m tools.profilegen.validate --profile ... --oracle tests/fixtures/profilegen/townfall-oracle.json --notes tests/fixtures/profilegen/townfall-oracle-notes.json --markdown`.

Addresses are `label / runtime` (runtime = label + load base).

| value | generated | oracle | match | confidence | method |
|---|---|---|---|---|---|
| load base | 0x400000 | 0x400000 | yes | medium | `load.platform_convention` (not observable from the file; ZCode observed it in game) |
| engine major.minor.patch | 5.6.1 | 5.6.1 | yes | high / high / medium | `engine.store_site_scan` (+ marker fingerprint 5.6, margin 20) |
| changelist, branch | 0, `UE5` | 0, `UE5` | yes | medium | `engine.changelist_replay`, `engine.branch_accessor` |
| revision string | `5.6.1-0+++UE5` | same | yes | medium | `engine.revision_join` (a join; not present in the image) |
| build id | 04bb4330a7d71dcefe1cc2c4d19769d5 | same | yes | high | `elf.gnu_build_id` |
| shipping / STATS | shipping (margin 60) / off (margin 12), 54 of 244 markers | same | yes | high | `axes.marker_fingerprint` |
| FNamePool | 0x9365ec0 / 0x9765ec0 | same | yes | high | `fname_pool.init_callers` |
| pool offsets CurrentBlock / CurrentByteCursor / Blocks | +0x8 / +0xc / +0x10 | same | yes | high | `fname_pool.init_memset`, `growth_function`, `init_block_store` |
| pool NoneId / BytePropertyId / buckets (count x stride) | +0x14040 / +0x14044 / +0x10040 (256 x 0x40) | same | yes | high / high / medium | `fname_pool.init_ids`, `init_hash_loop` |
| pool init flag | 0x9365ea0 / 0x9765ea0 (pool-0x20) | same | yes | medium | `fname_pool.init_guard` |
| GUObjectArray | 0x93ba110 / 0x97ba110 | same | yes | high | `uobject_array.member_function_cluster` (+ IndexToObject idiom at 58 sites) |
| FName ctor (wchar_t*) | 0x23d590 / 0x63d590 | same | yes | high | `fname_ctor.module_name_anchors` (5 of 5 anchors, 9 call sites) |
| FName ctor ABI | rdi = out FName\*, rsi = const char16_t\*, edx = EFindName | same | yes | medium | callers load `edx = 1` at 9/9 sites; the body and the delegate it calls never read `edx` (the overload always adds) |
| StaticConstructObjectInternal | 0x490aa0 / 0x890aa0 | same | yes | high | `static_construct.new_object_pair` (1720 of 1918 windows) |
| FMemory::Malloc / Free | 0xb7a20 / 0xb7a90 (0x4b7a20 / 0x4b7a90) | same | yes | high | `gmalloc.dispatch_shape` (vtable +0x30 / +0x50; Free has 166,429 callers) |
| ProcessEvent vtable offset / slot | 0x268 / 77 | same | yes | high | `process_event.shared_vtable_function` (5,226 of 7,134 vtables) |
| ProcessEvent function | 0x459830 / 0x859830 | same | yes | high | same (tests `FUNC_Native` at `[ufunction+0xb1]`, calls vtable +0x270/+0x278) |
| **GMalloc** | **0x9350950 / 0x9750950** | ZCode seed 0x930add8 / 0x970add8 | **NO (disagrees with ZCode seed)** | high | `gmalloc.dispatch_shape`: both `FMemory::Malloc` and `FMemory::Free` load the allocator pointer from this variable (417 references). The seed value is also derived, as `symbols.gmalloc_base_slot` (0x930add8 / 0x970add8, medium, `gmalloc.lazy_create_accessor`): the raw allocator pointer the lazy-create function fetches and then stores (possibly wrapped) into GMalloc. Neither was tuned; the console test decides. |
| **StaticFindObject** | 0x48a680 / 0x88a680 | none (oracle: "no address needed") | n/a (found) | **medium** | `static_find_object.fatal_message_family`: carries both `Illegal call to StaticFindObjectFast()` fatal messages, reads `rdx` as UTF-16, compares `rsi` with -1, 122 callers; capped at medium (single structural channel) |
| TUObjectArray (7 members, chunk 0x10000) | 0x0 0x8 0x10 0x14 0x18 0x1c, size 0x20 | same | yes | high (size medium) | `uobject_array.index_to_object_idiom`, `member_access` |
| FUObjectArray prefix (0x0 0x4 0x8 0xc) and ObjObjects | 0x0 0x4 0x8 0xc, 0x10 | same | yes | high | `uobject_array.member_access` |
| **FUObjectArray UObjectCreateListeners / DeleteListeners** | **0x58 / 0x68** | 0x68 / 0x78 | **NO** | medium | `uobject_array.tarray_members` |
| **FUObjectArray PrimarySerialNumber / bShouldRecycleObjectIndices** | **0x90 / 0x94** | 0xb0 / 0xb4 | **NO** | medium | `schema_order` / `member_access` (`cmp byte [this+0x94], 1` before pushing an index on the free list) |
| **FUObjectArray UEP_TotalSize** | **0x98** | 0xb8 | **NO** | high | `uobject_array.end_of_struct`: the next global in BSS (a different object) starts at +0x98 |
| FNameEntry.header | `u16 LE: bit0 wide; length = header >> 6` | adds the 5-bit probe hash | partial | low | only the length shift is measured |
| VTableLayout.ini | byte-identical to ZCode's `[UObject] ProcessEvent=0x268` file | sha256 37a7d860...a8a5 (216 bytes) | yes | - | `emit_ue4ss` |
| MemberVariableLayout.ini | same 588 bytes and layout; **five values differ** (above) | sha256 f291d552...3f32 | NO | - | with those five set to the oracle's values the file is byte-identical to the staged one |

The five `FUObjectArray` tail values are the one place where the generator contradicts the staged pair: the staged ini takes them from the
UE4SS 5.06 template (Windows-derived, larger mutexes), and the oracle itself records that they were never measured (only the first 96 bytes of the
live array were captured). In this image the object-allocation member loops over the create-listener `TArray` at `[this+0x58]`/`[this+0x60]`, the
delete-listener notifier over `[this+0x68]`/`[this+0x70]` under a mutex at `0x78`, the free-index member tests the byte at `0x94`, and the next BSS
object is referenced from `+0x98` (it is a different object with its own accessors, not a member). The generated `MemberVariableLayout.ini` therefore
differs from the staged one; the oracle's ZCode-staged values are template defaults, not measurements.

Not produced (support constants or out of scope, none needed by a seed slot): `gmalloc_vtable`, `gmalloc_malloc_impl`, `gmalloc_slow_path` (need the
concrete allocator vtable), `fname_core`, `process_event_empty_stub_slot76` (a Win64-template observation), `fname_to_string.resolved` /
`static_find_object.address_applicable` (absence claims), and the PS5 `[Hooks]` policy keys.
`unresolved` rows: `version_map.pc_build` (the game profile has no PC build for 02.003.000) and the layout members listed under "Layout detectors".

### What is not derivable generically, and why

* **Load base**: no value in the executable fixes it; it is a platform convention (medium) until a console observation is passed with `--load-base`.
* **That the generated GMalloc is the right variable for UE4SS**: the generator reports what the engine's own `FMemory::Malloc/Free` dereference, and
  separately the raw allocator slot; which one the PS5 runtime should seed is a console question.
* **PrimarySerialNumber** is inferred from declaration order (an `int32` directly before the observed bool); no atomic access to it was found
  in this image. **Listener array roles** (create vs delete) come from declaration order of the two TArrays.
* **StaticFindObject** has no independent second channel, so it stays at medium.
* **FNameEntry bit layout beyond the length shift** is not attempted. The reflection member layouts, `engine_tick`, `FName::ToString` and `GNatives` are
  derived by the layout detectors (below); what they could not derive is listed there.
* Anything that needs the *running* game (UE `BUILD_ID`, the FEngineVersion singleton address, whether the allocator thread-forwarder is installed).

### Assumptions that may not hold on another title

Frame-pointer clang code with `55 48 89 e5` prologues or `int3` padding between functions (function map), Itanium vtables (two destructor slots),
fatal log messages and `__FILE__` strings kept in shipping builds (UObjectArray and StaticFindObject anchors), default-name literals registered in one
function (FNamePool), UE 5.x `FChunkedFixedUObjectArray` (2 pointers + 4 int32). When an assumption fails the value is left unresolved or capped at
`low`/`medium` with the failing fact in `evidence`/`unresolved`; it is not guessed. Each one has a synthetic image that violates it in
`tests/profilegen_robustness_tests.py`:

| assumption | when it fails |
|---|---|
| function map: prologue or padding | either half alone is enough; with neither, FNamePool/GMalloc/GUObjectArray/StaticFindObject are unresolved, the FName constructor and StaticConstructObject are capped at `low` (correct address, no function-start check), the profile lists `analysis.function_map` |
| Itanium destructor pair | `vtables.UObject.dtor_prefix_slots` is only written (medium) when at least half of the long vtable runs start with two distinct nearby functions; otherwise it is `unresolved`. The ProcessEvent slot is measured from the run start and is right either way |
| fatal strings / `__FILE__` kept | stripped strings leave `object_array`, `static_find_object`, `static_construct_object_internal` and the FName constructor (module-name anchors) unresolved; one surviving anchor function caps the confidence below `high` |
| default names in one function | the cluster only makes a function `FNamePool::Init` if the memset + `Malloc(block, align)` layout is in it too; otherwise `fnamepool_init`, `fname_default_name_store`, `default_name_count` and `name_pool` are `low`, the layout and `GMalloc` (reached through `FMemory::Malloc`) are unresolved; fewer than 8 names in any function: nothing |
| `FChunkedFixedUObjectArray` | the member order is only trusted when the bounds-check idiom measures `NumElements` at +0x14. Not measured: schema-derived members are capped at `low`. Measured elsewhere: `NumElements` is reported at the measured distance (medium) and `PreAllocatedObjects`, `MaxElements`, `MaxChunks`, `NumChunks`, `UEP_TotalSize` and the `FUObjectArray` tail are `unresolved`. A flat array (no `index >> 16` step) has no layout at all |

Known limits that leave values unresolved rather than wrong: the engine store scan needs a non-zero minor version (a `x.0` build gets its version from the
marker fingerprint at `medium`, without patch/changelist/revision); a version string that is the only evidence is `medium` (a branch name inside the same
literal does not count as a second channel).

Inputs are refused before anything is written: an encrypted SELF (magic `4f 15 3d 1d` or `54 14 f5 ee`, exit 5), a missing/unreadable/non-JSON/non-object `param.json`, an
executable that is missing, empty or not an ELF64 x86-64 little-endian image, an image with no program headers or no loadable segment, overlapping or
inconsistent `PT_LOAD`s (`p_memsz < p_filesz`, addresses beyond 1 TiB), or any program header that runs past the end of the file (a truncated file).
Mutating header bytes, the segment table, the dynamic table, the notes or the process parameter block never crashes a detector.

## Layout detectors: member layouts, vtable slots, engine tick, FName::ToString, GNatives

The first version derived the global singletons and the `FUObjectArray` layout. The layout detectors add what the port's `MemberVariableLayout.ini`
and `VTableLayout.ini` still need, each value with confidence, method and evidence, and the three runtime symbols that had no address.

### What the port's runtime binds (read from its source, read-only)

`NativeLayoutBinding.cpp` stages the two files through `ParsePair` (`NativeLayoutFiles.hpp`: `[NativeLayout]` identity with `Format=1`,
`Target=prospero-x86_64-sysv`, `PointerSize=8`, `EngineMajor/Minor`, `NonShipping`, `Stats`, `EngineRevision`, `BuildId`; then `Section` / `Key=0xOFFSET`;
a bitfield member carries a `(position, length, storage_bytes)` tuple) and binds them into the engine classes' `MemberOffsets` / `BitfieldInfos` /
`VTableLayoutMap`:

* **Members**: the keys of `NativeLayoutSchema.inc` only (41 classes, 588 member keys plus `UEP_TotalSize`; an unknown key, a duplicate or an alias next to its canonical
  key is refused). A class that appears must carry a positive `UEP_TotalSize` and every member must lie inside it together with the width of the port's compiled
  accessor. The generator checks the offset against the size and leaves the width to the port.
* **Vtables**: 24 sections (`UObject UEngine FProperty FField UField UStruct AActor AGameModeBase AGameMode UGameViewportClient FMalloc FArchive ...`), free-form
  keys (the UE function name), byte offsets that are multiples of 8. `UClass` has no section (so `CreateDefaultObject` stays in the JSON).
* **Seeds**: `UEngine::TickInternal` is assigned from the scan (`gameengine_tick`, here `symbols.engine_tick`) or from `UEngine::VTableLayoutMap["Tick"]` read
  through the live `UGameEngine` object (`OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, Tick, GameEngine)`); `FName::ToStringInternal` is a
  `void(const FName*, FStringOut&)` (`symbols.fname_to_string`); `GNatives_Internal` is the table base that `FFrame::Step` indexes (`symbols.gnatives`).
* The set the Item Spawner path needs (from `outputs/2026-10-07-l19-local-profile-r4`, read-only): 69 members and 7 slots. Of its 52 reflection members the layout
  detectors derive **51** (all but `UClass::Interfaces`); of its 7 slots **4** (`ProcessEvent` from the first version, `UEngine::Tick`,
  `FProperty::DestroyValueInternal`, `FProperty::GetMinAlignment`), not `FProperty::CopyValuesInternal`, `UEngine::LoadMap`, `AGameModeBase::InitGameState`.

Coverage of the 41 member classes: 13 reflection classes have measured members (`UObjectBase` 5 of 5 keys, `FUObjectItem` 5 of 10, `UField` 1 of 1, `UStruct` 9 of 14,
`UClass` 7 of 31, `UFunction` 10 of 11, `FField` 5 of 5, `FFieldClass` 5 of 8, `FProperty` 10 of 10, `FBoolProperty` 4 of 4, `FStructProperty`, `FArrayProperty`,
`FEnumProperty` all), 15 more have only their `UEP_TotalSize` (`UScriptStruct UEnum UWorld AActor AGameModeBase AGameMode UPlayer ULocalPlayer UGameViewportClient UDataTable`
and five more property classes) in the JSON, and 11 are not touched (`FArchive FArchiveState FOutputDevice FWorldContext FUObjectHashTables ...`). No bitfield member is written.
**No template value is substituted anywhere**: a member nobody reads and no measured neighbours bound is `unresolved` (the method name `template_default` is reserved and never emitted).

### Confidence for layout members (`_book.py`)

Every layout detector records *observations* `(path, value, channel, method, evidence, cap)` in a book; `layout_merge` resolves it: one value from two or more independent
channels is `high`; from one channel `medium` (a channel marked as a structural invariant, such as the constant an allocation passes to `FMemory::Malloc`, may be `high` alone);
two values for one path publish nothing and `unresolved` names both. An envelope another detector wrote for the same path joins as one more channel. Declared-order fits
(a member that fills a measured gap exactly) are `medium`; a member with one measured neighbour only (`UStruct.PostConstructLink`) and the zero-initialised members of
`FFieldClass` named by position are `low` and stay in the JSON.

### Validation on Townfall (PPSA33286)

Run on the pinned executable, `python3 -m tools.profilegen` takes **~20 s** (`class_registration` 1.7 s, `field_classes` 0.9 s, the others below 1 s each, on top of the shared
indexes). "known" is a value measured on the same image by other offline analyses (read-only evidence under `/WORKSPACE/.codex/worktrees/translation-core/what/`, listed
below; **not console-verified**) or, where none exists, the UE4SS 5.06 Windows default layout (which is **not a measurement**: it is shown only to say where the PS5 image
differs). `tests/fixtures/profilegen/townfall-layout-known.json` holds the same data; `TownfallValidation` fails if a generated value stops matching a measured one.
Sources: `L19-static` `outputs/2026-10-07-l19-static-layout-check` (REPORT, REPORT-SUPPLEMENT, STATIC/REFLECTION-FACTS), `L19-size` `...-l19-class-size-evidence`,
`L19-subtype` `...-l19-property-subtype-evidence`, `L19-unres` `...-l19-unresolved-evidence`, `L19-total` `...-l19-r4-total-evidence`, `L19-slots` `...-l19-r4-engine-slots`,
`L19-tick` `...-l19-worker-engine-slots`, `L19-iface` `...-l19-remaining-interface-evidence`, `L19-r4` `...-l19-local-profile-r4/profiles/townfall-build-facts.json`, `frame-audit`
`outputs/2026-10-08-native-frame-audit`, `ORACLE` `docs/profiles/TOWNFALL-ORACLE.md`. Method names are abbreviated (`ub` = `uobject_base`, `cr` = `class_registration`, `pem` = `process_event_members`,
`cdo` = `class_default_object`, `fc` = `field_classes`, `ps` = `property_subtypes`, `lm` = `layout_merge`).

**What the table shows:** all 72 values that another analysis measured on this image are reproduced exactly (0 mismatches), and the PS5 image differs from
the Windows template only where Itanium layout and the extra destructor slot say it must: `FProperty.ArrayDim/ElementSize` (0x2c/0x30, not 0x30/0x34: the derived class reuses the
base's tail padding), `UGameViewportClient` (0x3b8, not 0x3c0) and the vtable offsets (`UEngine.Tick` 0x310, `FProperty` slots, `UClass.CreateDefaultObject` +8 each, except Tick, which sits 0x20 further).

| value | generated | known (source) | match | confidence | method |
|---|---|---|---|---|---|
| UObjectBase.ObjectFlags | 0x8 | 0x8 (L19-static) | yes | high | `process_event.member_flow+ub.constructor_stores` |
| UObjectBase.InternalIndex_Private | 0xc | 0xc (L19-static) | yes | high | `ub.allocator_store+ub.constructor_stores` |
| UObjectBase.ClassPrivate | 0x10 | 0x10 (L19-static) | yes | high | `ub.constructor_stores+ub.constructor_variants` |
| UObjectBase.NamePrivate | 0x18 | 0x18 (L19-static) | yes | high | `ub.constructor_stores+ub.constructor_variants` |
| UObjectBase.OuterPrivate | 0x20 | 0x20 (L19-static) | yes | high | `process_event.member_flow+ub.constructor_stores` |
| UObjectBase.UEP_TotalSize | 0x28 | 0x28 (L19-size) | yes | high | `ub.extent` |
| FUObjectItem.Object | 0x0 | 0x0 (L19-static) | yes | medium | `ub.allocator_stores` |
| FUObjectItem.SerialNumber | 0x10 | 0x10 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `ub.allocator_stores` |
| FUObjectItem.Flags | 0x8 | 0x8 (L19-static) | yes | medium | `ub.allocator_stores` |
| FUObjectItem.ClusterRootIndex | 0xc | 0xc (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| FUObjectItem.RefCount | 0x14 | 0x14 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| FUObjectItem.UEP_TotalSize | 0x18 | 0x18 (L19-static) | yes | medium | `uobject_array.index_to_object_idiom` |
| UField.Next | 0x28 | 0x28 (L19-static) | yes | medium | `lm.declared_order_fit` |
| UField.UEP_TotalSize | 0x30 | 0x30 (L19-size) | yes | high | `cr.sizeof` |
| UStruct.SuperStruct | 0x40 | 0x40 (L19-static) | yes | medium | `cdo.create_default_object` |
| UStruct.Children | 0x48 | 0x48 (L19-static) | yes | medium | `lm.declared_order_fit` |
| UStruct.PropertiesSize | 0x58 | 0x58 (L19-static) | yes | high | `cr.argument_flow+cr.inline_immediates+process_event.member_flow` |
| UStruct.Script | 0x60 | 0x60 (L19-static) | yes | medium | `process_event.member_flow` |
| UStruct.MinAlignment | 0x5c | 0x5c (L19-static) | yes | high | `cr.argument_flow+cr.inline_immediates+process_event.member_flow` |
| UStruct.PropertyLink | 0x70 | 0x70 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| UStruct.RefLink | 0x78 | 0x78 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| UStruct.DestructorLink | 0x80 | 0x80 (L19-static) | yes | medium | `process_event.member_flow` |
| UStruct.PostConstructLink | 0x88 | 0x88 (UE4SS 5.06 Windows default, not a measurement) | same | low | `lm.declared_order_fit` |
| UStruct.ChildProperties | 0x50 | 0x50 (L19-static) | yes | high | `cdo.create_default_object+process_event.member_flow` |
| UStruct.UEP_TotalSize | 0xb0 | 0xb0 (L19-size) | yes | high | `cr.sizeof` |
| UClass.ClassConstructor | 0xb0 | 0xb0 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `cr.argument_flow` |
| UClass.ClassFlags | 0xd4 | 0xd4 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `cr.argument_flow` |
| UClass.ClassCastFlags | 0xd8 | 0xd8 (L19-unres) | yes | medium | `cr.argument_flow` |
| UClass.ClassWithin | 0xe0 | 0xe0 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `cr.argument_flow` |
| UClass.ClassConfigName | 0xe8 | 0xe8 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `cr.argument_flow` |
| UClass.ClassDefaultObject | 0x110 | 0x110 (L19-unres) | yes | medium | `cdo.create_default_object` |
| UClass.ClassVTableHelperCtorCaller | 0xb8 | 0xb8 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `cr.argument_flow` |
| UClass.UEP_TotalSize | 0x200 | 0x200 (L19-size) | yes | high | `cr.inline_immediates+cr.alloc_size` |
| UFunction.FunctionFlags | 0xb0 | 0xb0 (L19-static) | yes | medium | `process_event.native_test` |
| UFunction.NumParms | 0xb4 | 0xb4 (L19-static) | yes | medium | `lm.declared_order_fit` |
| UFunction.ParmsSize | 0xb6 | 0xb6 (L19-static) | yes | medium | `process_event.member_flow` |
| UFunction.ReturnValueOffset | 0xb8 | 0xb8 (L19-static) | yes | medium | `process_event.member_flow` |
| UFunction.RPCId | 0xba | 0xba (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| UFunction.RPCResponseId | 0xbc | 0xbc (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| UFunction.FirstPropertyToInit | 0xc0 | 0xc0 (L19-static) | yes | medium | `process_event.member_flow` |
| UFunction.Func | 0xd8 | 0xd8 (L19-static) | yes | medium | `process_event.member_flow` |
| UFunction.EventGraphFunction | 0xc8 | 0xc8 (L19-static) | yes | medium | `process_event.member_flow` |
| UFunction.EventGraphCallOffset | 0xd0 | 0xd0 (L19-static) | yes | medium | `process_event.member_flow` |
| UFunction.UEP_TotalSize | 0xe0 | 0xe0 (L19-size) | yes | high | `cr.inline_immediates` |
| FField.ClassPrivate | 0x8 | 0x8 (L19-static) | yes | medium | `cdo.create_default_object` |
| FField.Owner | 0x10 | 0x10 (L19-static) | yes | medium | `fc.field_constructor_stores` |
| FField.Next | 0x18 | 0x18 (L19-static) | yes | high | `cdo.create_default_object+fc.field_constructor_stores+process_event.member_flow` |
| FField.NamePrivate | 0x20 | 0x20 (L19-static) | yes | medium | `fc.field_constructor_stores` |
| FField.FlagsPrivate | 0x28 | 0x28 (L19-static) | yes | medium | `fc.field_constructor_stores` |
| FField.UEP_TotalSize | 0x30 | 0x30 (L19-total) | yes | medium | `fc.itanium_tail_padding` |
| FFieldClass.FName | 0x0 | 0x0 (L19-unres) | yes | medium | `fc.constructor_stores` |
| FFieldClass.Id | 0x8 | 0x8 (L19-size) | yes | medium | `fc.constructor_stores` |
| FFieldClass.CastFlags | 0x10 | 0x10 (L19-unres) | yes | high | `cdo.create_default_object+fc.constructor_stores` |
| FFieldClass.ClassFlags | 0x18 | 0x18 (UE4SS 5.06 Windows default, not a measurement) | same | low | `fc.zero_initialiser` |
| FFieldClass.SuperClass | 0x20 | 0x20 (L19-unres) | yes | medium | `fc.constructor_stores` |
| FFieldClass.DefaultObject | 0x28 | 0x28 (UE4SS 5.06 Windows default, not a measurement) | same | low | `fc.zero_initialiser` |
| FFieldClass.ConstructFn | 0x30 | 0x30 (L19-size) | yes | medium | `fc.constructor_stores` |
| FFieldClass.UniqueNameIndexCounter | 0x38 | none | - | low | `fc.zero_initialiser` |
| FFieldClass.UEP_TotalSize | 0x40 | 0x40 (L19-total) | yes | high | `fc.extent+fc.spacing` |
| FProperty.ArrayDim | 0x2c | 0x2c (L19-static) | yes | high | `fc.inline_constructor+process_event.member_flow` |
| FProperty.ElementSize | 0x30 | 0x30 (L19-static) | yes | high | `fc.inline_constructor+process_event.member_flow` |
| FProperty.PropertyFlags | 0x38 | 0x38 (L19-static) | yes | medium | `process_event.member_flow` |
| FProperty.RepIndex | 0x40 | 0x40 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| FProperty.RepNotifyFunc | 0x68 | 0x68 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| FProperty.Offset_Internal | 0x44 | 0x44 (L19-static) | yes | medium | `process_event.member_flow` |
| FProperty.PropertyLinkNext | 0x48 | 0x48 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| FProperty.NextRef | 0x50 | 0x50 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `lm.declared_order_fit` |
| FProperty.DestructorLinkNext | 0x58 | 0x58 (L19-static) | yes | medium | `process_event.member_flow` |
| FProperty.PostConstructLinkNext | 0x60 | 0x60 (L19-static) | yes | medium | `process_event.member_flow` |
| FProperty.UEP_TotalSize | 0x70 | 0x70 (L19-size) | yes | high | `fc.allocation_size` |
| FBoolProperty.FieldSize | 0x70 | 0x70 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `ps.overridden_virtuals` |
| FBoolProperty.ByteOffset | 0x71 | 0x71 (L19-subtype) | yes | high | `ps.bool_initialise+ps.overridden_virtuals` |
| FBoolProperty.ByteMask | 0x72 | 0x72 (L19-subtype) | yes | medium | `ps.overridden_virtuals` |
| FBoolProperty.FieldMask | 0x73 | 0x73 (L19-subtype) | yes | high | `ps.bool_initialise+ps.overridden_virtuals` |
| FBoolProperty.UEP_TotalSize | 0x78 | 0x78 (L19-subtype) | yes | high | `fc.allocation_size` |
| FStructProperty.Struct | 0x70 | 0x70 (L19-subtype) | yes | high | `ps.get_min_alignment+ps.overridden_virtuals` |
| FStructProperty.UEP_TotalSize | 0x78 | 0x78 (L19-subtype) | yes | high | `fc.allocation_size` |
| FArrayProperty.Inner | 0x78 | 0x78 (L19-subtype) | yes | medium | `ps.overridden_virtuals` |
| FArrayProperty.ArrayFlags | 0x70 | 0x70 (UE4SS 5.06 Windows default, not a measurement) | same | medium | `ps.overridden_virtuals` |
| FArrayProperty.UEP_TotalSize | 0x80 | 0x80 (L19-subtype) | yes | high | `fc.allocation_size` |
| FEnumProperty.UnderlyingProp | 0x70 | 0x70 (L19-subtype) | yes | high | `ps.get_min_alignment+ps.overridden_virtuals` |
| FEnumProperty.Enum | 0x78 | 0x78 (L19-subtype) | yes | medium | `ps.overridden_virtuals` |
| FEnumProperty.UEP_TotalSize | 0x80 | 0x80 (L19-subtype) | yes | high | `fc.allocation_size` |
| UEngine.Tick | 0x310 | 0x310 (L19-tick) | yes | high | `engine_tick.vtable_slot` |
| FProperty.DestroyValueInternal | 0x140 | 0x140 (L19-size) | yes | medium | `process_event.member_flow` |
| FProperty.InitializeValueInternal | 0x158 | 0x150 (UE4SS 5.06 Windows default, not a measurement) | **differs** | medium | `process_event.member_flow` |
| FProperty.GetMinAlignment | 0x170 | 0x170 (L19-size) | yes | high | `ps.alignment_constants` |
| UClass.CreateDefaultObject | 0x400 | 0x400 (L19-unres) | yes | medium | `cdo.create_default_object` |
| engine_tick.label | 0x2fac8c0 | 0x2fac8c0 (ORACLE) | yes | high | `engine_tick.anchor_string` |
| engine_tick.prologue_23 | 554889e54157415641554154534881ec880000004c8b2d | 554889e54157415641554154534881ec880000004c8b2d (ORACLE) | yes | high | `bytes` |
| engine_tick.anchor_xref_label | 0x2facbae | 0x2facbae (ORACLE) | yes | high | `strings` |
| fname_to_string.label | 0x23f150 | none (oracle: SURV status 3, not found by patternsleuth) | - | high | `fname_tostring.pool_reader_out_param` |
| fname_to_string_value.label | 0x23f0c0 | none (oracle: SURV status 3, not found by patternsleuth) | - | high | `fname_tostring.pool_reader_by_value` |
| gnatives.label | 0x93b8900 | 0x93b8900 (frame-audit) | yes | high | `gnatives.dispatch_table` |
| fframe_step.label | 0x4577a0 | 0x4577a0 (frame-audit) | yes | medium | `gnatives.dispatch_table` |
| create_default_object.label | 0x3222a0 | 0x3222a0 (L19-unres) | yes | medium | `cdo.create_default_object` |
| allocate_uobject_index.label | 0x483b40 | 0x483b40 (L19-static) | yes | medium | `ub.fatal_message_family` |
| ffield_class_ctor.label | 0x363990 | 0x363990 (L19-size) | yes | high | `fc.name_registrations` |
| class_registration_helper.label | 0x30d860 | 0x30d860 (L19-size) | yes | high | `cr.package_literal_calls` |

Other sizes derived from the same registration calls, no known value to compare with: `UScriptStruct` 0xc0, `UEnum` 0x68, `UWorld` 0x9e8, `AActor` 0x2a8, `AGameModeBase` 0x340, `AGameMode` 0x388,
`UPlayer` 0x48, `ULocalPlayer` 0x2b0, `UGameViewportClient` 0x3b8 (template 0x3c0), `UDataTable` 0xb0, `FByteProperty` 0x78, `FClassProperty` 0x80, `FDelegateProperty` 0x78,
`FMapProperty` 0xa0, `FSetProperty` 0x90 (all `high`: the constant the registration or `ConstructFn` passes), and `UObject` `sizeof` 0x28.

### Left unresolved (each with its reason in `unresolved`)

| value | known elsewhere | reason |
|---|---|---|
| `UClass.Interfaces` | 0x1d8 (`L19-iface`) | no generic anchor implemented: it is only read by `ImplementsInterface` / `GetInterfaceAddress`, which are not virtuals and carry no string; not guessed |
| `vtables.FProperty.CopyValuesInternal` | 0xf8 (`L19-size`); template 0xf0 | its callers are thin wrappers around a plain-`memcpy` path; no analysed code identifies the slot |
| `vtables.UEngine.LoadMap` | 0x4f8 (`L19-slots`); template 0x4d8 | `UEngine::LoadMap` has no pure-virtual stub string to anchor it (`UEngine::Tick` does); the template value is not used |
| `vtables.AGameModeBase.InitGameState` | 0x7b8 (`L19-slots`); template 0x7b0 | no string or call-sequence anchor is analysed |
| `UClass.{AllFunctionsCache, AllFunctionsCacheLock, ClassAddReferencedObjects, ClassGeneratedBy, ClassUnique, FirstOwnedClassRep, FuncMap, FuncMapLock, InterfaceFuncMap, NativeFunctionLookupTable, NetFields, ParentFuncMap, PropertiesStartOffset, ReferenceSchema, ReferenceTokenStream, SparseClassData, SparseClassDataStruct, SuperFuncMap, UberGraphFramePointerProperty, bCooked, bLayoutChanging, bNeedsDynamicSubobjectInstancing, bNeedsPostLoadSubobjectInstancing}` | template only | not read by the registration helper, `ProcessEvent` or `CreateDefaultObject`, and not bounded by two measured neighbours (the helper zero-initialises `+0xc8..+0xd2` but naming those bytes would be declaration-order guessing) |
| `UStruct.{ScriptObjectReferences, ScriptAndPropertyObjectReferences, UnresolvedScriptProperties, StructStateFlags}` | template only | same; `StructStateFlags` is only a 2-byte gap whose content (flags or padding) the image does not say |
| `UStruct.PostConstructLink` | template 0x88 | `low` (0x88): declared right after `DestructorLink`, but nothing on its far side is measured to bound it |
| `UFunction.RepOffset`, `FUObjectItem.{ClusterAndFlags, ClusterIndex, FlagsAndRefCount, ObjectPtrLow, RemoteId}` | template only | variants of build options this image does not show (`Flags` is a plain dword at +0x8, so the packed variants do not apply); nothing reads them |
| `FFieldClass.{ClassFlags, DefaultObject, UniqueNameIndexCounter}` | template 0x18 / 0x28 / - | `low`: the constructor zero-initialises those bytes; the names come from declaration order only |
| `version_map.pc_build` | - | the game profile has no PC build for 02.003.000 |

### Assumptions of the layout detectors (and what happens when they fail)

Each row has a synthetic image that violates it in `tests/profilegen_tests.py`; the value becomes `unresolved` or drops a confidence step, it is not guessed.

| assumption | when it fails |
|---|---|
| native classes register through one helper called after a `/Script/<Module>` literal, and the literals survive | no helper: `class_registration_helper`, every size and the `UStruct`/`UClass` offsets it gives are unresolved; two equally popular targets are ambiguous; a helper that stores none of its arguments leaves the offsets unresolved but keeps the call-site sizes; the engine is older than UE 5: the body is not read (argument order differs) |
| the helper's argument order (`InSize`, `InAlignment`, flags, cast flags, config name, constructor pair, `InWithinClassFn`) | a different order assigns members to the wrong roles only if the stores happen to match the shapes; a second channel (open-coded copies, `ProcessEvent`, `CreateDefaultObject`) then disagrees and the path publishes nothing |
| `UObjectArray.cpp` fatal messages kept (find `AllocateUObjectIndex`) | no allocator: the constructor roles are not found and `UObjectBase`/`FUObjectItem` are unresolved; a primary constructor needs 4 of the 5 argument stores |
| `ProcessEvent` has UE's source shape | each role is separate: a missing `memcpy`/`memset` pair loses `ParmsSize`/`PropertiesSize`; fewer than two further chains lose `DestructorLink` and `FirstPropertyToInit`; no `0xFFFF` compare loses `ReturnValueOffset` |
| `UClass` vtable holds one `RF_Public|RF_ClassDefaultObject|RF_ArchetypeObject` function | none or several: `ClassDefaultObject` and its followers unresolved (`need exactly 1`) |
| `FFieldClass` constructor and `ConstructFn` shapes | fewer than 4 agreeing names: no constructor; `sizeof(FFieldClass)` is `high` only if the extent and the static spacing agree; `sizeof(FField)` only when `ArrayDim` is exactly where `FlagsPrivate` ends |
| property subtypes override the base virtuals that read their members | an ambiguous set (two pointer members for `FStructProperty`, not four bytes for `FBoolProperty`) loses that channel |
| `UEngine::Tick` stub literal and the `causeevent=` anchor | one anchor alone: `medium`; the two disagree on the slot: no slot; two functions with the anchor: no label |
| `FName::ToString`: the pool is known and its readers decode `Blocks[id >> 16]` | no pool: unresolved; the by-value form must forward to exactly one out-parameter reader with swapped arguments; a close second candidate by caller count lowers it |
| `GNatives`: a BSS table indexed by a byte at >= 8 sites | fewer sites or two equally popular tables: unresolved; without >= 32 registration stores it is `medium` |
| declared-order fits: UE's member order and an exact gap | any other gap size: no fit |

## Tests

`python3 -m unittest tests.profilegen_tests tests.profilegen_robustness_tests tests.profilegen_seedtable_tests -v` (or `make test-python`).

* `tests/profilegen_tests.py`: ELF loader and segment mapping with a negative-delta segment, string/xref/function/call indexes, each detector's pattern logic
  on hand-assembled images whose numbers are deliberately not any game's, emission rules, assembly, CLI, and the Townfall validation tests that read
  `MODBRIDGE_TOWNFALL_EBOOT` / `MODBRIDGE_TOWNFALL_PARAM` (defaults point at the retained copies) and **skip cleanly when the files are absent**.
  `HardRuleTests` fails if a Townfall value appears in this directory (the list now also holds the addresses and the distinctive offsets the layout detectors validate).
  205 tests: 189 synthetic and 16 Townfall (10 of them from the first version). The 95 layout tests are in 14 classes: `FlowTests` / `BookTests` (the data-flow walker and
  the observation book), `DetectorSelectionTests` (registration order, `--only`, a missing decoder), one class per layout detector (`ClassRegistrationTests`, `UObjectBaseTests`, `ProcessEventMembersTests`, `ClassDefaultObjectTests`,
  `FieldClassesTests`, `PropertySubtypeTests`, `EngineTickTests`, `GNativesTests`, `FNameToStringTests`: hand-assembled shapes with arbitrary numbers, plus negative
  images that must stay unresolved), `LayoutMergeTests` (declared-order fits and their non-fits) and `ExtendedEmitTests` (class/size/confidence/schema rules of the
  extended ini files, checked with a parser of the port's file contract). Six Townfall tests compare every value another analysis measured
  (`tests/fixtures/profilegen/townfall-layout-known.json`), the template differences, confidences, evidence, the contract of the generated ini files and the pinned
  list of unresolved ids; the whole Townfall generation is asserted to stay under 60 s (about 20 s). Fixtures: `tests/fixtures/profilegen/synth.py` and `synth_layout.py`.
* `tests/profilegen_robustness_tests.py` (synthetic, always run when capstone is installed): refusals (encrypted SELF, param.json, odd ELFs, truncation),
  header-mutation fuzzing, other engine versions, and one violating image per assumption above.
* `tests/profilegen_seedtable_tests.py`: header format/order/escaping, `c++ -std=c++20 -fsyntax-only` and a compile-and-run check of the derivation strings
  (skipped without a C++ compiler), the GMalloc choice and heap-span options, absent slots, refusals, the CLI flag, "never a verification marker", the comparison
  tool, and Townfall tests (skipped without the executable; the last one also needs `MODBRIDGE_CURRENT_SEEDTABLE`, default the runtime's retained header).
