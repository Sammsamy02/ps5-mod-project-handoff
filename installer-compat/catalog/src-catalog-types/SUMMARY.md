# UE4SS Lua API — Object/Type Method Catalog

**Mac-only / host-tested.** Pure static source analysis; no FTP, no payloads, no launches,
no pldmgr/shsrv calls, no console contact. Nothing outside `/tmp/sideproject/catalog-types/` was written.

- **Source root (read-only):** `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/`
- **Output:** `types.json` (array of type objects; `implementation` fields are paths relative to the source root)
- **Catalog:** 42 types, 456 method entries total (168 non-inherited definitions + inherited exposure), 20 metamethods
- `ps5_runtime_status` is `""` on every type/method by design — to be filled later by GPT.
- Metatable-name resolution: port's own `include/LuaType/LuaUObject.hpp` + `LuaUnrealString.hpp` override the two
  upstream headers; remaining name structs resolve from the build dependency tree
  `.../ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/LuaType/` (verified in `compile-recipes.json` include paths).

## Method counts per type

| Type | Metatable / global | Parent | Category | Own methods | Total exposed | Metamethods | needs_live_game |
|---|---|---|---|---|---|---|---|
| AActor | AActor | UObject | other | 2 | 20 | 0 | true |
| FAnsiString | FAnsiString | ObjectBase (LocalObjectBase) | string | 11 | 13 | 0 | false |
| FName | FNameUserdata | ObjectBase (LocalObjectBase) | string | 3 | 5 | 2 | true |
| FOutputDevice | FOutputDevice | ObjectBase (RemoteObjectBase) | other | 1 | 3 | 0 | true |
| FPackageName | FPackageName (global table) | — | other | 2 | 2 | 0 | false |
| FSoftObjectPath | FSoftObjectPathUserdata | ObjectBase (LocalObjectBase) | object-lifecycle | 2 | 4 | 0 | true |
| FString | FString | ObjectBase (LocalObjectBase) | string | 11 | 13 | 0 | false |
| FText | FText | ObjectBase (LocalObjectBase) | string | 1 | 3 | 2 | true |
| FURL | FURLUserdata | ObjectBase (LocalObjectBase) | other | 0 | 2 | 1 | true |
| FUtf8String | FUtf8String | ObjectBase (LocalObjectBase) | string | 11 | 13 | 0 | false |
| FWeakObjectPtr | FWeakObjectPtr | ObjectBase (LocalObjectBase) | object-lifecycle | 2 | 4 | 0 | true |
| LocalUnrealParam | LocalUnrealParam | — | reflection | 5 | 5 | 0 | true |
| ModRef | ModRef | ObjectBase (RemoteObjectBase) | other | 2 | 3 | 1 | true |
| RemoteObjectBase | RemoteObjectBase | — | other | 3 | 3 | 0 | false |
| RemoteUnrealParam | RemoteUnrealParam | — | reflection | 5 | 5 | 0 | true |
| TArray | TArray | ObjectBase (RemoteObjectBase) | container | 7 | 8 | 3 | true |
| ThreadId | ThreadIdUserdata | ObjectBase (LocalObjectBase) | other | 1 | 3 | 1 | false |
| TMap | TMap | ObjectBase (RemoteObjectBase) | container | 7 | 8 | 1 | true |
| TSet | TSet | ObjectBase (RemoteObjectBase) | container | 6 | 7 | 1 | true |
| TSoftObjectPtr | TSoftObjectPtrUserdata | ObjectBase (LocalObjectBase) | object-lifecycle | 3 | 5 | 0 | true |
| UClass | UClass | UStruct | reflection | 2 | 23 | 0 | true |
| UDataTable | UDataTable | UObject | reflection | 9 | 27 | 1 | true |
| UE4SS | UE4SS (global table) | — | other | 1 | 1 | 0 | false |
| UEnum | UEnum | UObject | reflection | 7 | 25 | 0 | true |
| UFunction | UFunction | UStruct | reflection | 2 | 23 | 1 | true |
| UInterface | UInterface | UClass | reflection | 0 | 23 | 0 | true |
| UnrealVersion | UnrealVersion (global table) | — | other | 7 | 7 | 0 | true |
| UObject | UObject | RemoteObjectBase | reflection | 17 | 18 | 4 | true |
| UScriptStruct | UScriptStruct | UObject (manual merge) | reflection | 7 | 28 | 2 | true |
| UStruct | UClass | UObject | reflection | 3 | 21 | 0 | true |
| UWorld | UWorld | UObject | other | 1 | 19 | 0 | true |
| XArrayProperty | ArrayProperty | XProperty | reflection | 1 | 10 | 0 | true |
| XBoolProperty | ObjectProperty | XProperty | reflection | 4 | 13 | 0 | true |
| XDelegateProperty | DelegateProperty | XProperty | reflection | 0 | 9 | 0 | true |
| XEnumProperty | EnumProperty | XProperty | reflection | 1 | 10 | 0 | true |
| XFieldClass | FieldClass | ObjectBase (LocalObjectBase) | reflection | 1 | 3 | 0 | true |
| XInterfaceProperty | InterfaceProperty | XProperty | reflection | 1 | 10 | 0 | true |
| XMulticastDelegateProperty | MulticastDelegateProperty | XProperty | reflection | 5 | 14 | 0 | true |
| XMulticastSparseDelegateProperty | MulticastSparseDelegateProperty | XProperty | reflection | 5 | 14 | 0 | true |
| XObjectProperty | ObjectProperty | XProperty | reflection | 1 | 10 | 0 | true |
| XProperty | Property | ObjectBase (RemoteObjectBase) | reflection | 7 | 9 | 0 | true |
| XStructProperty | StructProperty | XProperty | reflection | 1 | 10 | 0 | true |

## Totals

- Types: **42** — container: 3, object-lifecycle: 3, other: 10, reflection: 21, string: 5
- Method entries (incl. inherited exposure): **456**
- Distinct method definitions (non-inherited): **168**
- Metamethod bindings: **20**
- Types usable without a live game (`needs_live_game: false`): RemoteObjectBase, FString, FUtf8String, FAnsiString, ThreadId, UE4SS, FPackageName

## Alphabetical index (type -> own methods)

- **AActor** (AActor): GetWorld, GetLevel
- **FAnsiString** (FAnsiString): ToString, Empty, Clear, Len, IsEmpty, Append, Find, StartsWith, EndsWith, ToUpper, ToLower
- **FName** (FNameUserdata): ToString, GetComparisonIndex, Equals
- **FOutputDevice** (FOutputDevice): Log
- **FPackageName** (FPackageName (global table)): IsShortPackageName, IsValidLongPackageName
- **FSoftObjectPath** (FSoftObjectPathUserdata): GetAssetPathName, GetSubPathString
- **FString** (FString): ToString, Empty, Clear, Len, IsEmpty, Append, Find, StartsWith, EndsWith, ToUpper, ToLower
- **FText** (FText): ToString
- **FURL** (FURLUserdata): *(no own methods)*
- **FUtf8String** (FUtf8String): ToString, Empty, Clear, Len, IsEmpty, Append, Find, StartsWith, EndsWith, ToUpper, ToLower
- **FWeakObjectPtr** (FWeakObjectPtr): Get, get
- **LocalUnrealParam** (LocalUnrealParam): Get, get, Set, set, type
- **ModRef** (ModRef): SetSharedVariable, GetSharedVariable
- **RemoteObjectBase** (RemoteObjectBase): GetAddress, IsValid, type
- **RemoteUnrealParam** (RemoteUnrealParam): Get, get, set, Set, type
- **TArray** (TArray): GetArrayAddress, GetArrayNum, GetArrayMax, GetArrayDataAddress, Empty, ForEach, IsValid
- **ThreadId** (ThreadIdUserdata): ToString
- **TMap** (TMap): IsValid, Find, Add, Contains, Remove, Empty, ForEach
- **TSet** (TSet): IsValid, Add, Contains, Remove, Empty, ForEach
- **TSoftObjectPtr** (TSoftObjectPtrUserdata): GetWeakPtr, GetTagAtLastTest, GetObjectID
- **UClass** (UClass): GetCDO, IsChildOf
- **UDataTable** (UDataTable): GetRowStruct, GetRowMap, FindRow, AddRow, RemoveRow, EmptyTable, GetRowNames, GetAllRows, ForEachRow
- **UE4SS** (UE4SS (global table)): GetVersion
- **UEnum** (UEnum): GetNameByValue, ForEachName, GetEnumNameByIndex, InsertIntoNames, EditNameAt, EditValueAt, RemoveFromNamesAt
- **UFunction** (UFunction): GetFunctionFlags, SetFunctionFlags
- **UInterface** (UInterface): *(no own methods)*
- **UnrealVersion** (UnrealVersion (global table)): GetMajor, GetMinor, IsEqual, IsAtLeast, IsAtMost, IsBelow, IsAbove
- **UObject** (UObject): GetFullName, GetFName, GetClass, GetOuter, IsAnyClass, Reflection, GetPropertyValue, SetPropertyValue, IsClass, GetWorld, CallFunction, IsA, HasAllFlags, HasAnyFlags, HasAnyInternalFlags, ProcessConsoleExec, IsValid
- **UScriptStruct** (UScriptStruct): GetBaseAddress, GetStructAddress, GetPropertyAddress, IsValid, IsMappedToObject, IsMappedToProperty, GetProperty
- **UStruct** (UClass): GetSuperStruct, ForEachFunction, ForEachProperty
- **UWorld** (UWorld): SpawnActor
- **XArrayProperty** (ArrayProperty): GetInner
- **XBoolProperty** (ObjectProperty): GetByteMask, GetByteOffset, GetFieldMask, GetFieldSize
- **XDelegateProperty** (DelegateProperty): *(no own methods)*
- **XEnumProperty** (EnumProperty): GetEnum
- **XFieldClass** (FieldClass): GetFName
- **XInterfaceProperty** (InterfaceProperty): GetInterfaceClass
- **XMulticastDelegateProperty** (MulticastDelegateProperty): Add, Remove, Broadcast, GetBindings, Clear
- **XMulticastSparseDelegateProperty** (MulticastSparseDelegateProperty): Add, Remove, Broadcast, GetBindings, Clear
- **XObjectProperty** (ObjectProperty): GetPropertyClass
- **XProperty** (Property): GetClass, GetFullName, GetFName, GetOffset_Internal, IsA, ContainerPtrToValuePtr, ImportText
- **XStructProperty** (StructProperty): GetStruct

## Inheritance chains

- `ObjectBase` (template root; GetAddress/IsValid) → RemoteObjectBase / LocalObjectBase → all UE4SS types
- UObject-family: `UObject` → `UStruct` → `UClass` → `UInterface`; `UObject` → `UEnum`, `UWorld`, `AActor`, `UDataTable`;
  `UStruct` → `UFunction`. `UScriptStruct` is a local wrapper that manually merges the `UStruct`+`UObject` method sets.
- XProperty-family: `XProperty` → XArrayProperty, XBoolProperty, XEnumProperty, XInterfaceProperty, XObjectProperty,
  XStructProperty, XDelegateProperty, XMulticastDelegateProperty, XMulticastSparseDelegateProperty.
- String-family: template `TLuaStringBase` → `FString`, `FUtf8String`, `FAnsiString` (identical method sets).

## Quirks worth knowing (found during extraction)

- `UStruct` and `UClass` share the literal metatable name **"UClass"** (`UStructName::ToString()` returns "UClass").
- `XProperty`'s metatable name is **"Property"**; `XBoolProperty` and `XObjectProperty` both register as **"ObjectProperty"**.
- CORRECTION (2026-10-07): the `*Userdata` metatable names (`FName` → **"FNameUserdata"**, plus
  FSoftObjectPath/TSoftObjectPtr/FURL/ThreadId) are **UPSTREAM UE4SS 3.0.1's own convention, NOT a
  port rename**. The vendored upstream tree (`ue4ss-unreal-native-build-1/source-dependencies/ue4ss`,
  version.cache 3.0.1.0.0) uses the identical literal names at the identical lines (e.g.
  `LuaFName.cpp:17`, `LuaFSoftObjectPath.cpp:17/39`, `LuaTSoftObjectPtr.cpp:17/40`, `LuaFURL.cpp:13`,
  `LuaThreadId.cpp:18`). An earlier revision of this summary wrongly claimed the port renamed these;
  the port kept upstream naming unchanged (only unrelated include-path tweaks differ).
- On `__PROSPERO__` (PS5), UFunction invocation runs inside a protected C frame (`FNativeUFunctionCall`,
  src/LuaType/LuaUObject.cpp:97-154) so C++ exceptions cannot cross the C-compiled Lua VM — UObject:CallFunction and
  UFunction __call both route through it.
- `UScriptStruct:GetBaseAddress` is deprecated and always throws (src/LuaType/LuaUScriptStruct.cpp:80).
- `FURL` has a stub `__call` and no member functions (todos in source).

## Out of scope (adjacent but not object/type methods)

- Free global functions in `src/Mod/LuaMod.cpp` (`FindFirstOf`, `FindAllOf`, `StaticFindObject`, `StaticConstructObject`,
  `RegisterHook`, `NotifyOnNewObject`, `ForEachUObject`, `RegisterCustomProperty`, `DumpAllObjects`, `GenerateSDK`, ...)
  — these are library functions, not methods on a userdata type.
- Enum/constant tables registered as globals: `Key` (Win32 VK codes, LuaMod.cpp:448-614), `ModifierKey` (:616-626),
  `EObjectFlags` (:631-672), `EInternalObjectFlags` (:674-692), `EFindName` (:697-702), `PropertyTypes` (:951-1087).
  The `Key`/`ModifierKey` tables are the main PC-keyboard (Win32) surface; no object method in this catalog reads the keyboard.
- `LoadExport` external-data tables (src/LuaLibrary.cpp:321-368).

