# l22 63-key preflight table (Mac-only)

Extracted from the actual shipped candidate header. The profile values below are static declarations, not new console measurements. `Deferred` rows are runtime discoveries, not fabricated layout offsets.

| Key | Kind | Disposition | Current profile | Evidence classification |
|---|---|---|---|---|
| `FUObjectArray::ObjObjects` | Member | Required | 0x10 | startup_menu_source_dependency |
| `TUObjectArray::Objects` | Member | Required | 0x0 | startup_menu_source_dependency |
| `TUObjectArray::NumElements` | Member | Required | 0x14 | startup_menu_source_dependency |
| `TUObjectArray::NumChunks` | Member | Required | 0x1c | startup_menu_source_dependency |
| `TUObjectArray::MaxChunks` | Member | Required | 0x18 | startup_menu_source_dependency |
| `FUObjectItem::Object` | Member | Required | 0x0 | startup_menu_source_dependency |
| `FUObjectItem::Flags` | Member | Required | 0x8 | startup_menu_source_dependency |
| `UObjectBase::ClassPrivate` | Member | Required | 0x10 | startup_menu_source_dependency |
| `UObjectBase::NamePrivate` | Member | Required | 0x18 | startup_menu_source_dependency |
| `UObjectBase::OuterPrivate` | Member | Required | 0x20 | startup_menu_source_dependency |
| `UObjectBase::ObjectFlags` | Member | Required | 0x8 | startup_menu_source_dependency |
| `UObjectBase::InternalIndex_Private` | Member | Required | 0xc | startup_menu_source_dependency |
| `UStruct::SuperStruct` | Member | Required | 0x40 | startup_menu_source_dependency |
| `UStruct::Children` | Member | Required | 0x48 | startup_menu_source_dependency |
| `UStruct::ChildProperties` | Member | Required | 0x50 | startup_menu_source_dependency |
| `UField::Next` | Member | Required | 0x28 | startup_menu_source_dependency |
| `FField::ClassPrivate` | Member | Required | 0x8 | startup_menu_source_dependency |
| `FField::NamePrivate` | Member | Required | 0x20 | startup_menu_source_dependency |
| `FField::Next` | Member | Required | 0x18 | startup_menu_source_dependency |
| `FFieldClass::FName` | Member | Required | 0x0 | startup_menu_source_dependency |
| `FFieldClass::CastFlags` | Member | Required | 0x10 | startup_menu_source_dependency |
| `FFieldClass::SuperClass` | Member | Required | 0x20 | startup_menu_source_dependency |
| `UFunction::NumParms` | Member | Required | 0xb4 | startup_menu_source_dependency |
| `UFunction::ParmsSize` | Member | Required | 0xb6 | startup_menu_source_dependency |
| `UFunction::ReturnValueOffset` | Member | Required | 0xb8 | startup_menu_source_dependency |
| `UFunction::FunctionFlags` | Member | Required | 0xb0 | startup_menu_source_dependency |
| `UFunction::Func` | Member | Required | 0xd8 | startup_menu_source_dependency |
| `FProperty::PropertyFlags` | Member | Required | 0x38 | startup_menu_source_dependency |
| `FProperty::ArrayDim` | Member | Required | 0x2c | startup_menu_source_dependency |
| `FProperty::ElementSize` | Member | Required | 0x30 | startup_menu_source_dependency |
| `FProperty::Offset_Internal` | Member | Required | 0x44 | startup_menu_source_dependency |
| `UObject::ProcessEvent` | Vtable | Required | 0x268 | startup_menu_source_dependency |
| `FStructProperty::Struct` | Member | Required | 0x70 | startup_menu_source_dependency |
| `UStruct::PropertiesSize` | Member | Required | 0x58 | startup_menu_source_dependency |
| `UStruct::MinAlignment` | Member | Required | 0x5c | startup_menu_source_dependency |
| `FProperty::CopyValuesInternal` | Vtable | Required | 0xf8 | startup_menu_source_dependency |
| `FProperty::DestroyValueInternal` | Vtable | Required | 0x140 | startup_menu_source_dependency |
| `FBoolProperty::ByteOffset` | Member | Required | 0x71 | startup_menu_source_dependency |
| `FBoolProperty::ByteMask` | Member | Required | 0x72 | startup_menu_source_dependency |
| `FBoolProperty::FieldMask` | Member | Required | 0x73 | startup_menu_source_dependency |
| `FEnumProperty::UnderlyingProp` | Member | Required | 0x70 | startup_menu_source_dependency |
| `FEnumProperty::Enum` | Member | Required | 0x78 | startup_menu_source_dependency |
| `FArrayProperty::Inner` | Member | Conditional | 0x78 | spawn_explicit_array_conversion |
| `FProperty::GetMinAlignment` | Vtable | Conditional | 0x170 | spawn_explicit_array_conversion |
| `FMalloc::QuantizeSize` | Vtable | Required | 0x68 | startup_menu_source_dependency |
| `FMalloc::Realloc` | Vtable | Required | 0x40 | startup_menu_source_dependency |
| `FMalloc::Free` | Vtable | Required | 0x50 | startup_menu_source_dependency |
| `FText::StaticSize_Private` | Runtime | Deferred | runtime-discovered | startup_menu_runtime_discovery_not_profile_offset |
| `FText::ValueOpsProperty` | Runtime | Deferred | runtime-discovered | startup_menu_runtime_discovery_not_profile_offset |
| `FMalloc::Malloc` | Vtable | Required | 0x30 | startup_menu_source_dependency |
| `UEngine::LoadMap` | Vtable | Required | 0x4f8 | startup_menu_source_dependency |
| `UEngine::Tick` | Vtable | Required | 0x310 | startup_menu_source_dependency |
| `AGameModeBase::InitGameState` | Vtable | Required | 0x7b8 | startup_menu_source_dependency |
| `AActor::BeginPlay` | Vtable | Conditional | missing; lazy lookup retained | disabled_initializer_branch_only |
| `AActor::EndPlay` | Vtable | Conditional | missing; lazy lookup retained | disabled_initializer_branch_only |
| `AActor::Tick` | Vtable | Conditional | missing; lazy lookup retained | disabled_initializer_branch_only |
| `UGameViewportClient::Tick` | Vtable | Conditional | missing; lazy lookup retained | disabled_initializer_branch_only |
| `UObject::ProcessConsoleExec` | Vtable | Conditional | missing; lazy lookup retained | disabled_initializer_branch_only |
| `UStruct::Link` | Vtable | Conditional | missing; lazy lookup retained | disabled_initializer_branch_only |
| `FProperty::RepNotifyFunc` | Member | Conditional | missing; lazy lookup retained | conditional_optional_property_dependency |
| `FProperty::HasIntrusiveUnsetOptionalState` | Vtable | Conditional | missing; lazy lookup retained | conditional_optional_property_dependency |
| `FProperty::IsIntrusiveOptionalValueSet` | Vtable | Conditional | missing; lazy lookup retained | conditional_optional_property_dependency |
| `FProperty::InitializeValueInternal` | Vtable | Conditional | missing; lazy lookup retained | conditional_optional_property_dependency |

Counts: 49 startup-required; 12 conditional map keys; 2 deferred runtime discoveries. All 49 startup-required declarations are present and match their pins. Missing conditional map keys are logged without stopping; matching known pins remain mandatory if supplied.

FText discovery, engine calls, menu rendering and spawning still require target evidence.
