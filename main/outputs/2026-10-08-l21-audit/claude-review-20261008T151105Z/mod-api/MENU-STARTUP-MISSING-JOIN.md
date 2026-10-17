# Startup/menu missing-facts join

The bounded dependency join identifies **four startup/menu layout keys absent from the existing profile**: `FMalloc::QuantizeSize`, `Malloc`, `Realloc`, and `Free`. All four now have official-source/default-configuration and saved-image semantic evidence. They are **candidates, not existing profile values**. PID184 stopped on QuantizeSize before Lua began; none of this mod’s menu APIs executed.

This joins 71 dependency rows / **63 unique keys** against the 711-key inventory. Of those 63, **47 are already profiled, 14 are absent, and two are runtime discoveries rather than profile offsets**. The inventory’s 635 unresolved linked keys must not all be presented as menu blockers. This join is not a complete dynamic closure.

| Startup/menu missing key | Current profile | New candidate byte offset | Alternate exec-disabled branch | Evidence / remaining limit |
|---|---|---:|---:|---|
| FMalloc::QuantizeSize | Absent | 0x68 | 0x70 | Observed startup stop; default source layout and independent image behavior agree |
| FMalloc::Malloc | Absent | 0x30 | 0x38 | Source allocator dependency; default source layout and independent image behavior agree |
| FMalloc::Realloc | Absent | 0x40 | 0x48 | FString/container allocation path; default source layout and independent image behavior agree |
| FMalloc::Free | Absent | 0x50 | 0x58 | FString/container release path; default source layout and independent image behavior agree |

The source-supported default is `UE_ALLOW_EXEC_COMMANDS=1`; actual game target-rule/command-line overrides are not captured. All four candidate rows retain `approved_for_profile=false`, and the allocator packet retains `ready_for_runtime_build=false`. The offsets above are not evidence of live calls or a completed gate. Full source declarations, relocations, target addresses and configuration alternatives are copied into the [JSON join](MENU-STARTUP-MISSING-JOIN.json) from [the allocator candidate table](../allocator/postfetch/candidate-table.json).

**Already-profiled dependencies:** 45 unique startup/menu keys cover object discovery, generic reflection metadata, nested structs, bool/enum access, value operations and four enabled hooks. The hooks are UEngine LoadMap `0x4f8`, UEngine Tick `0x310`, AGameModeBase InitGameState `0x7b8`, and UObject ProcessEvent `0x268`. “Profiled” records current bindings and inventory provenance; it does not prove every access width, present-value correctness or live execution. The JSON preserves each profile value and unknown widths.

**The other ten missing numerical keys are conditional, not established menu blockers:**

- Six initializer branches are disabled in the frozen settings: `AActor::BeginPlay`, `AActor::EndPlay`, `AActor::Tick`, `UGameViewportClient::Tick`, `UObject::ProcessConsoleExec`, `UStruct::Link`. This proves only bypass of those particular initializer branches. Other linked callers remain unclassified.
- Four optional-property keys remain unresolved: `FProperty::RepNotifyFunc`, `HasIntrusiveUnsetOptionalState`, `IsIntrusiveOptionalValueSet`, `InitializeValueInternal`. Their use on this menu path is unestablished. The selected TypeChecker observation and guarded missing-key getters do not establish whole-binary unreachability. `DestroyValueInternal` is already profiled and also belongs to menu value cleanup.

**Spawn-only separation:** explicit trace-ignore-array conversion adds two already-profiled keys, `FArrayProperty::Inner` and `FProperty::GetMinAlignment`; it adds no missing profile key in this bounded join. FTransform/FVector/FRotator, whole FHitResult out conversion, TArray actor pointers, deferred spawning, pickup writes and the LoadAsset FAssetData/FScriptInterface fallback retain descriptor, ABI and ownership gaps. Their explicit mod callsites are spawn paths. Generic nested/output conversions can still introduce indirect dependencies; absent calls do not prove unreachability.

**Remaining menu facts outside numerical profile gaps:**

- `FText::StaticSize_Private` and `FText::ValueOpsProperty` are runtime discoveries, not missing profile offsets. Their validity, text conversion signatures, copy/destruction and visible labels remain unproved; ignored SetText pcalls can hide blank labels.
- Official SCO callback storage declares alignment 16; the selected placeholder permits alignment 8. Field offsets agree with native observations. Actual caller alignment and target macro overrides remain unmeasured: this is not proof of a misaligned native call. See [the official-header comparison](OFFICIAL-SCO-COMPARISON.md).
- Raw reflected frames need contained FString/FText/TArray ownership and cleanup evidence. The FName fallback’s copied return does not establish destruction of the frame-owned original. Nested bounds, packed bool results, wrapper/GC lifetime and map reset remain semantic audit gaps.
- The 33 reflected method names need valid discovered descriptors and generic conversion/dispatch evidence. They do **not** require per-method hardcoded game descriptors. Menu struct/property layouts, Lua start, key delivery, queue draining, widget construction and LoadMap delivery remain unproved.

The [full audit](REPORT.md), source tables and [join JSON](MENU-STARTUP-MISSING-JOIN.json) preserve evidence and scope. No runtime/profile/mod edits, build or console contact occurred for this join.
