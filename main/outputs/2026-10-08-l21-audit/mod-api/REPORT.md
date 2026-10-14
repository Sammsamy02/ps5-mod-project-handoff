# l21 local mod/API audit

**Official 5.6.1 header followup:** all 13 field identities/order are now corroborated. The real inline `TFunction<void()>` requires alignment16, while the selected 48-byte array placeholder retains only alignment8. Offsets agree with the observed native geometry; actual selected callsite alignment remains unmeasured. See [OFFICIAL-SCO-COMPARISON.md](OFFICIAL-SCO-COMPARISON.md). The preceding report is preserved as `REPORT.prior-official-header.md`.

Scope: local report and saved-image inspection only. No console contact, runtime build, or mod/runtime/profile edit. This report does not propose a patch or claim readiness.

The frozen mod uses **31 distinct reflected method names directly, plus 2 in UEHelpers = 33**, all with **zero console-proven mod execution**. PID184 stopped before Lua began. The saved `runtime-status.txt:4` reports `Virtual FMalloc::QuantizeSize is unavailable, possibly unsupported in engine version`; the same harvest's `UE4SS.log:438` stops TownfallItemSpawner. The native FName ByteProperty startup probe did execute (`UE4SS.log:397–403`), and the Kismet string class/function/CDO were found (`410–415`). Neither proves arbitrary mod FName construction, name conversion, menu construction, text rendering, input dispatch, or spawning.

## Provenance and counts

Authoritative composition: `outputs/2026-10-08-menu-complete-build/pass1/COMPOSITION-RECEIPT.json`. Source selection is `outputs/2026-10-08-menu-complete-build/SOURCE-SELECTION.json` (there is no `pass1/SOURCE-SELECTION.json`). `evidence-pins.json` contains absolute paths, byte sizes, SHA256, selected object paths/hashes, and source-hash comparisons. All inspected sources that have an exact selected-unit match agree with that unit's source hash.

* main.lua: `outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/main.lua`, 21,415 bytes, SHA256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`.
* Actual selected initializer: `outputs/2026-10-08-r5-thread-final-integration/thread/UnrealInitializer.cpp`, SHA256 `f32e06f5a7782de3cb4271f4013ec7bb5bdbf2f292495dd568eb059d86474faf`. The corresponding `-v2` copy is identical.
* Actual selected LuaMod: `outputs/2026-10-08-r5-thread-source-integration/thread/LuaMod.cpp`, SHA256 `3f9960fe8a822693d441fd981cc01a3f2ad5a53116f372fe4b0a0107bec34c50`. The `r5-thread-final-integration-v2` copy inspected initially is identical.
* Actual selected LuaUObject: `outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp`, SHA256 `48aa5605354a649462f090615546d043dc268c0b06ec0a7dd226964e5a1a0638`. The coherent source copy is **not** this selected unit.
* Other engine sources are under `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/`; other Lua types under the sibling `ue4ss/` tree.

| Inventory | Count | Meaning |
|---|---:|---|
| Direct reflected method names | 31 | `reflected-methods.tsv` has exact main.lua lines |
| UEHelpers additional reflected names | 2 | IsPlayerController, IsLocalPlayerController |
| Total reflected names / unknown full API status | 33 / 33 | Implemented source; concrete native signatures/behavior not proved |
| API/dependency surfaces | 16 | 14 unknown; 2 narrowly safe missing-key refusal mechanisms |
| Named value shapes | 14 | Struct/container/string layouts need native reflection and ownership evidence |
| Direct object/struct property paths | 9 | Specific game property offsets/types unmeasured |
| Conditional initializer virtual lookups | 10 | 4 enabled; 6 disabled in the saved settings |
| Console-proven menu/mod APIs | 0 | Lua did not begin |
| Menu rows / TextBlocks | 12 / 18 | 3 options + 8 pickups + 1 status; 3 fixed labels + 3 section headings + 12 row labels |
| Mandatory menu SCO calls | 23 | 5 tree/layout objects + 18 TextBlocks; fallback root adds 1 |
| FText attempts on a complete first build/open | 42 | 18 creation texts + 12 build refresh + 12 show refresh |
| Legacy database entries | 25 | Each enum lookup; path lookup conditional on valid result |
| Binary-join dependency rows | 71 | Includes repeated keys across surfaces; not a complete runtime binary inventory |

A source function existing is not a measured game layout. A profile binding carrying measured provenance is not a live call. A `pcall` does not contain arbitrary native faults. The two “unbound-but-safely-failing” rows only describe absence of a map key and the immediate return/throw before the protected access; they do not certify whole APIs or present-but-incorrect offsets.

## Actual call order

`call-order.tsv` supplies a joinable sequence. Lua evaluation only builds local row definitions, registers six keys, attempts LoadMap callback registration, and logs Loaded. First F7 queues toggleMenu. Then scanItems finds the database and enumerates entries; buildUI discovers a controller; it finds WidgetBlueprintLibrary/UserWidget; tries reflected Create; falls back to SCO for the root; constructs tree/canvas/borders/box and 18 TextBlocks; refreshes text; adds to viewport; hides initially; returns success; refreshes again; shows and locks input. Navigation refreshes labels. Selection can enter spawn. Closing hides and unlocks; the future LoadMap callback also drops all references and resets state.

This order matters: `LoadAsset` is only the spawn fallback after a cooked class misses StaticFindObject (`main.lua:283–287`). It is not a prerequisite for showing the menu. `scanItems` precedes widget creation but its outer `pcall` allows the UI attempt after database failure.

## Input, thread dispatch, and LoadMap

Six `RegisterKeyBind` calls at `main.lua:694,698,703,708,713,722` register callback references and program keydown events (`LuaMod.cpp:1946–1999`). The callback invokes ExecuteInGameThread at six locations. The saved settings choose EngineTick; selected source (`5322–5455`) can redirect to the FuncPtr marshal when usable, then uses availability/fallback logic. The hook's address, registration success, installed pump, correct current thread, queue progress, and actual key event are separate facts. None of this mod's callbacks ran in PID184.

`RegisterLoadMapPreHook` at `main.lua:731` is wrapped in `pcall`. Its implementation merely stores a callback (`LuaMod.cpp:2742–2766`). The native LoadMap delivery is armed separately, including the one-shot deferred path at `4700–4717`. A failed deferred arm explicitly leaves callbacks undelivered for the process lifetime. Thus accepted registration is not evidence of map-reset delivery. A missed reset can retain stale wrappers; Lua's normal UObject `IsValid` implementation is just a non-null pointer check (`ue4ss/include/LuaType/LuaUObject.hpp:185–202`), not a serial/GC validation.

`startup-hook-lookups.tsv` records enabled settings and exact initializer lines. Saved `UE4SS-settings.ini:63–75` enables UEngine LoadMap/Tick, AGameModeBase InitGameState, and UObject ProcessEvent. The compiled declarations carry offsets `0x4f8`, `0x310`, `0x7b8`, and `0x268`, respectively. Native provenance belongs to the parent's binary/member census. These lookups are after the observed stop and were not reached.

The six disabled branches are AActor BeginPlay/EndPlay/Tick, UGameViewportClient Tick, UObject ProcessConsoleExec, and UStruct Link. Their exact initializer branches are bypassed for the frozen settings; this is not a whole-binary unreachable proof. Required CDO/instance searches at initializer `2041–2088` occur independently of several hook flags, so disabling a hook does not eliminate those object-search dependencies. The required lookup macro throws on an absent key; the optional macro returns null before reading an object vtable (`UnrealVirtualBaseVC.hpp:104–133`). Both still need correct present offsets, valid instances, vtable data, and selected jump-resolution behavior.

## Object discovery and the UEHelpers path

`main.lua:15–23` requires UEHelpers under pcall, calls its controller helper under pcall, and then tries FindFirstOf outside that helper pcall. The saved UEHelpers initializes two invalid-object caches at lines35/45; GetPlayerController uses FindAllOf(PlayerController) or FindAllOf(Controller), then IsPlayerController / IsLocalPlayerController (`69–81`). The mod's outer operations provide different error boundaries; there is no universal safe fallback for invalid native metadata.

FindFirstOf and FindAllOf use FName comparisons and object/class/superclass traversal (UObjectGlobals.cpp `434–478`, `546–589`), with alternative hash/cache routes. StaticFindObject may use the hash route when available or scan by object name/outer chain. The bound base metadata does not prove concrete game object discovery, class compatibility, current lifetime, or the Lua wrappers. `binary-join-requirements.tsv` includes the object array/item/base fields, FField class/name/next, superclass chains, and property metadata consumed by these paths. No absence from a previously chosen graph is treated as a reachability proof.

## SCO and the UE5.6 parameter structure

The selected initializer binds slot4 at label `0x890aa0` after address classification, but explicitly states “identity and ABI unverified” (`1151–1162`). The source uses the UE5.6 `Above55` structure (`UObjectGlobals.hpp:313–342`) and passes its address to a function pointer (`UObjectGlobals.cpp:82–92,122–136`). An unready function or null class returns null in that wrapper. A non-null incorrect function remains an ABI risk.

Local saved-image evidence adds useful specificity without changing that runtime admission:

* Image SHA256 rechecked as `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, 194,241,424 bytes. Runtime labels in the saved disassembly are ELF virtual addresses plus `0x400000`; the actual PT_LOAD file mapping was parsed.
* All **114** saved SCO instructions and **86** saved ObjectInitializer instructions exactly match image bytes. Fresh Capstone output is saved beside this report.
* Entry `0x890aa0` consumes a parameter pointer in RDI, saves it in RBX, loads Class/Outer/Name/flags, invokes the allocation path, calls a class constructor via `[Class+0xb0]`, and returns the result in RAX. This is strong saved-image evidence consistent with SCO identity/ABI, not proof of a live invocation.
* A native parameter initializer at `0x893530`, reached by a visible callsite preceding a SCO call, writes Class at0, Outer at8, clears fields including SerialNumber at0x70 and SubobjectOverrides at0x78. Output is `sco-parameter-constructor-disassembly.tsv`.

| Source field | Offset | Width | Native read/write evidence |
|---|---:|---:|---|
| Class | 0x00 | 8 | SCO0x890ac5; ctor0x893539 |
| Outer | 0x08 | 8 | SCO0x890ac8; ctor0x893541 |
| Name | 0x10 | 8 | SCO0x890aed/0x890bad |
| SetFlags | 0x18 | 4 | SCO0x890acc |
| InternalSetFlags | 0x1c | 4 | SCO0x890bb1 |
| bCopyTransientsFromClassDefaults | 0x20 | 1 | initializer0x891ddd |
| bAssumeTemplateIsArchetype | 0x21 | 1 | SCO0x890afa |
| Template | 0x28 | 8 | SCO0x890ae4; initializer0x891dd5 |
| InstanceGraph | 0x30 | 8 | initializer0x891deb |
| ExternalPackage | 0x38 | 8 | SCO0x890bb5 |
| PropertyInitCallback | 0x40 | 0x30 source opaque region | initializer reads0x40/0x68 and chooses inline object0x50 |
| SerialNumber | 0x70 | 4 | SCO0x890acf; ctor0x89354d |
| SubobjectOverrides | 0x78 | 8 | initializer0x891e64; ctor0x893554 |

The source defaults `UE_WITH_REMOTE_OBJECT_HANDLE=0` at header18–20. Source shape with the selected 8-byte FName is 0x80 bytes / alignment8; native reads establish an active extent through byte0x7f and agree with those field placements. **The disassembly alone does not independently prove total sizeof/alignment, padding semantics, or possible unobserved branches.** The subsequent official-header comparison supplies the real callback type and establishes its inline-storage alignment requirement. The native accesses strongly match a 48-byte inline callback / remote-disabled model; the selected placeholder does not preserve the official alignment. Exact target macros and selected callsite alignment remain missing. `sco-call-candidate-windows.txt` is diagnostic only: its arbitrary window starts and raw rel32 candidates are not function-boundary or full-callgraph proof.

## Reflection, values, and ownership

The selected LuaUObject added frame-bound checks at `199–234`: top-level parameter offset, ElementSize, ArrayDim and extent must fit declared ParmsSize and fixed local capacity. Parameter enumeration uses UFunction NumParms/ReturnValueOffset and CPF flags, then generic pushers; ProcessEvent occurs at365. Out tables are copied back at367–415. Return values dispatch through pushers at417–431. Bounds validation relies on correctly measured metadata readers; it does not prove that the function's real signature matches those descriptors.

The **33 names** and exact callsites are in `reflected-methods.tsv`; every one still needs actual function resolution, receiver/class compatibility, parameter count/order/offset/type/flags/extent, return/out behavior, and game-thread execution evidence. Generic descriptors from a different UFunction are insufficient. Six additional internal reflected operations appear in dependencies: Conv_NameToString, Conv_StringToText, Conv_TextToString, AssetRegistryHelpers GetAssetRegistry/GetAsset, and AssetRegistry GetAssetByObjectPath. They are not included in the 33 mod/helper method-name count.

`value-shapes.tsv` covers FMargin, FSlateColor, FLinearColor, FVector2D, FAnchors, FSlateFontInfo, FTransform, FHitResult, TArray actor pointers, FVector, FRotator, FText, FString, and AssetData/ScriptInterface. Struct-table conversion iterates real reflected fields and writes only matching Lua keys (`675–752`); fields without handlers are skipped. It does not independently check every nested field against the containing struct's bounds. The mod passes a `SpecifiedColor/ColorUseRule` shape to SetShadowColorAndOpacity; without that function's real parameter descriptor, compatibility is unproved and a mismatched shape may quietly leave fields zero.

FHitResult out conversion iterates the **whole** reflected struct, not just the ImpactPoint/Location/ImpactNormal keys later used in Lua. Its otherwise-unused fields and their registered pushers belong in the dependency audit. Likewise GetControlRotation and location returns can materialize all reflected fields. TArray ignore-list conversion reads Inner, element size and alignment, placement-constructs FScriptArray, and AddZeroed allocates (`992–1029`); the actor-pointer inner is written by the object pusher. Native TArray layout, element alignment, correct reflected inner type, allocator ABI and cleanup all remain necessary.

Object/int/bool/nested property paths are in `direct-properties.tsv`. ObjectProperty writes assume a direct UObject pointer; integer writes use the corresponding numeric pusher; bitfield writes use FBoolProperty FieldMask/ByteOffset/ByteMask (`1607–1632`). The GetNonTrivialLocal bool branch reads a plain bool at the base (`1625–1626`), unlike the masked normal Get: packed return/out/nested bool behavior must be checked against real descriptors. `w.Font.Size` traverses a remote ScriptStruct wrapper and reflected Size field (`LuaUScriptStruct.cpp:168–196`), so finding Font is not evidence of a correct write to Size.

### FText can fail while “Menu created” succeeds

FText(string) is implemented through reflected KismetTextLibrary Conv_StringToText (`FText.cpp:147–155`), not a directly bound constructor function. The Lua SetText input pusher itself converts the Lua FText to FString and reconstructs a new FText (`LuaUObject.cpp:1831–1848`), adding Conv_TextToString and another Conv_StringToText route. TypeChecker discovers TextProperty, active FText size and ValueOpsProperty (`TypeChecker.cpp:533–538`). Copy and destruction can call FProperty CopyValuesInternal and DestroyValueInternal; the source's maximum buffer is0x30, not evidence of the game's active FText size. Missing conversion functions/properties can throw, but malformed present metadata or incorrect ownership is not certified safe by pcall.

All mod SetText attempts are inside ignored pcalls (`main.lua:433–435,469`). Most style changes are likewise ignored. Thus Menu created / menuOpen / even a visible border can coexist with blank or wrong labels. Complete text evidence needs string conversion, text value construction, SetText reflected invocation, correct lifetime/reference release and actual visible labels. None is console-proved.

### FName/FString allocation and release

The native FName constructor ByteProperty probe succeeded independently of the generic Kismet `FName::ToString` fallback. The fallback reads real function metadata, obtains InName and return offsets, calls ProcessEvent, copies an FString from the return slot, then frees the raw parameter buffer (`NameTypes.cpp:76–107`). There is no independent bounds validation for those two offsets in this wrapper. The input name property is dereferenced without an explicit null check in that path.

FString's TArray copy constructor performs `CopyToEmpty` (`Array.hpp:830–832`); allocator storage destruction frees through the engine allocator (`ContainerAllocationPolicies.hpp:683–692`). Therefore the copied return can trigger QuantizeSize/Realloc and later Free. The raw original returned FString in the parameter frame is not visibly destructed before `ParamOwner.reset()`; its ownership/release is an unresolved source-visible gap, not a console-measured leak. Similar raw parameter-frame cleanup questions apply to input FString, constructed FText, and TArray storage in generic Lua dispatch and BPMacros. Success and error paths both need coverage; Lua out-table-reference cleanup alone is not native value destruction.

## Priority for the review packet

**Startup/menu prerequisites:** first join the allocator failure and all subsequent enabled startup dependencies against the complete selected binary inventory; then establish Lua loading, six-key delivery and executor progress, object/controller discovery, root and widget construction, reflected menu calls, visible text, and close/map-reset behavior. The four enabled initializer hooks and generic metadata readers are listed separately from the six disabled source branches. This report is a review packet only; it does not authorize or imply a new build.

**Spawn-only dependencies:** LineTraceSingle with its out hit and ignore array, transform/deferred actor spawn, pickup component/property writes, asset-registry LoadAsset fallback and failed-spawn cleanup. These cannot be used to infer a menu blocker merely because they exist in the mod.

**Semantic/ownership audit gaps:** raw reflected frames containing engine-owned strings/text/arrays, FString return copying, FText copy/destruction and suppressed text errors, whole-output struct conversion, bitfield-return behavior, stale wrappers after missed map cleanup and widget GC/lifetime. These remain distinct from missing numerical layout facts.

Generic reflected lookup is a legitimate design. The 33 methods do **not** each require hardcoded per-game descriptors. They need trustworthy generic metadata readers plus evidence that their discovered descriptors and generic conversion/dispatch path are valid for the concrete call, with appropriate bounds, ownership and error behavior. Such evidence has not yet been demonstrated or gated for these calls; no off-console attempt can supply the missing execution observations.

`REPRODUCE.md` gives the exact local-only recipe for the saved-image instruction checks and fresh SCO disassembly.

## Missing facts retained for the next audit join

1. Complete enabled startup call chain after the QuantizeSize stop, including all generic allocator virtuals reached by FString/TArray and all mandatory object/CDO searches. Do not infer that one allocator slot closes startup.
2. Present-key field/virtual provenance and native width for every descriptor reader in object discovery, TypeChecker and reflection; the parent owns the complete binary inventory. This report's 71 join rows are additive, not a closed census.
3. Concrete game UFunction descriptors and invocation behavior for 33 direct/helper names and the six hidden reflected helpers above; all are unproved as mod APIs.
4. Lua start and six callback registrations; pad event mapping, closure retention, selected executor, live queue/drain, game-thread identity, reentrancy, and shutdown cleanup.
5. LoadMap native hook arming and actual pre-callback delivery. Registration pcall, stored callback, and cleanup source are insufficient; current wrappers are non-null-only validity checks.
6. Runtime SCO identity/admission and live ABI; official/native field agreement and the real callback type are now documented, while exact target macro configuration, whole native sizeof/alignof, selected caller alignment and first safe object construction remain missing. Widget ownership, engine GC reachability, proper root initialization, viewport insertion and map cleanup are separate facts.
7. Exact game struct numeric widths/offsets/alignment, nested bounds, enum underlying types, bool packing, FHitResult whole-output shape, and TArray element/container/allocator ABI.
8. Actual FText active size/value-ops property, Kismet text functions/CDO/signatures, copy/destroy correctness, SetText success, and visible text. Font.Size write-through, shadow shape, padding/anchors/colors/layout success are not implied by Menu created.
9. Engine-owned FString/FText/TArray lifetime and cleanup on both success and failure; raw-frame reclamation is not contained-value destruction. Native allocator calling conventions and ownership must remain joined to the parent's allocator inventory.
10. Exact direct property descriptors for WidgetTree, RootWidget, Font.Size, ShadowOffset, Pawn, PickupInteractable, PickupInventoryItem.ItemId, PickupAmount and bAutoDestroyOnPickUp. Reflection search success does not prove these writes.
11. Spawn-only LoadAsset registry helpers, FScriptInterface, FAssetData version/shape, asset resolution and load behavior; deferred spawn/trace/transform/component mutation and cleanup remain unproved.
12. Optional/indirect dependencies that a simple graph omits. FOptionalProperty's apparent unchecked lookup at header27 is commented out; the two compiled getters check missing RepNotifyFunc and throw. TypeChecker615–618 registers OptionalProperty via generic GetSize without calling GetValueProperty, while SDK/USMap generator sources have real getter calls. This establishes a narrow missing-key refusal and a narrower TypeChecker observation, not whole-binary unreachability or a measured native optional layout.

Evidence tables are `*.json` and matching `*.tsv`. `SCO-IMAGE-VALIDATION.json` pins the image, PT_LOAD mapping and exact instruction checks. `COUNTS.json` is the machine-readable count receipt. No observed post-Lua outcome exists in this run.
