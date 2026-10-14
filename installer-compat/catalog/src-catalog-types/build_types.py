#!/usr/bin/env python3
"""Build /tmp/sideproject/catalog-types/types.json — machine-readable catalog of
UE4SS Lua API object/type methods for the PS5 port tree at
.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/

Mac-only, host-tested: pure static source analysis, no engine/console contact.
Implementation paths are relative to the port ue4ss root:
  .../native-coherent-runtime-1/source/port/ue4ss/
"""
import json

H = "include/LuaType/LuaUObject.hpp"
HS = "include/LuaType/LuaUnrealString.hpp"

def m(name, sig, impl, cat=None, inh=None, notes=None):
    d = {"name": name, "lua_signature": sig, "implementation": impl}
    if inh:
        d["inherited_from"] = inh
    if notes:
        d["notes"] = notes
    return d

# ---- shared method sets -------------------------------------------------
OBJ_BASE = [
    m("GetAddress", "obj:GetAddress() -> integer", f"{H}:172",
      inh="ObjectBase (RemoteObjectBase/LocalObjectBase template)",
      notes="Remote types return the remote C++ pointer; local types return address of the owned copy."),
    m("IsValid", "obj:IsValid() -> boolean", f"{H}:185",
      inh="ObjectBase (RemoteObjectBase/LocalObjectBase template)",
      notes="Remote: true iff pointer non-null. Local: always true. Overridden by UObject-family (hpp:761) and UScriptStruct/TArray/TMap/TSet."),
]

def uobj_methods():
    """Own methods of UObject (UObjectBase), exact lines in LuaUObject.hpp."""
    return [
        m("GetFullName", "obj:GetFullName() -> string|nil", f"{H}:532"),
        m("GetFName", "obj:GetFName() -> FName", f"{H}:552"),
        m("GetClass", "obj:GetClass() -> UClass", f"{H}:557"),
        m("GetOuter", "obj:GetOuter() -> UObject", f"{H}:562"),
        m("IsAnyClass", "obj:IsAnyClass() -> boolean", f"{H}:570"),
        m("Reflection", "obj:Reflection() -> table (ReflectionTable, has :GetProperty)", f"{H}:578",
          notes="Returns table with ReflectedObject + GetProperty(string PropertyName) -> XProperty (sub-function at hpp:587)."),
        m("GetPropertyValue", "obj:GetPropertyValue(string PropertyName) -> any", f"{H}:620"),
        m("SetPropertyValue", "obj:SetPropertyValue(string PropertyName, any Value)", f"{H}:625"),
        m("IsClass", "obj:IsClass() -> boolean", f"{H}:630"),
        m("GetWorld", "obj:GetWorld() -> UWorld|nil", f"{H}:640"),
        m("CallFunction", "obj:CallFunction(... ) -> function return values", f"{H}:647",
          notes="Delegates to call_ufunction_from_lua (src/LuaType/LuaUObject.cpp:153; Prospero protected-call wrapper at :97)."),
        m("IsA", "obj:IsA(UClass Class | string ObjectFullName) -> boolean", f"{H}:651",
          notes="Implementation: src/LuaType/LuaUObject.cpp:2247 (overloads: UClass userdata or full class name string)."),
        m("HasAllFlags", "obj:HasAllFlags(EObjectFlags ObjectFlags) -> boolean", f"{H}:655",
          notes="Integer EObjectFlags from global EObjectFlags table."),
        m("HasAnyFlags", "obj:HasAnyFlags(EObjectFlags ObjectFlags) -> boolean", f"{H}:679"),
        m("HasAnyInternalFlags", "obj:HasAnyInternalFlags(EInternalObjectFlags InternalObjectFlags) -> boolean", f"{H}:703"),
        m("ProcessConsoleExec", "obj:ProcessConsoleExec(string Cmd, nil Reserved, UObject Executor) -> boolean", f"{H}:727"),
        m("IsValid", "obj:IsValid() -> boolean", f"{H}:761",
          notes="UObject-family override: checks pointer, special_invalid_ptr, global-unreal-objects map, and IsUnreachable."),
    ]

def uobj_inherited(tag_uobject=False):
    base = [m("GetAddress", "obj:GetAddress() -> integer", f"{H}:172", inh="ObjectBase")]
    own = uobj_methods()
    if not tag_uobject:
        return base + own
    tagged = []
    for mm in own:
        mm = dict(mm)
        mm.setdefault("inherited_from", "UObject")
        tagged.append(mm)
    return base + tagged

UOBJ_METAMETHODS = [
    {"name": "__index", "behavior": "property/function lookup via custom property list then reflected FProperty/UFunction; returns invalid UObject for safe chaining", "implementation": f"{H}:502"},
    {"name": "__newindex", "behavior": "property write via property pushers", "implementation": f"{H}:507"},
    {"name": "__call", "behavior": "error if instance is nullptr", "implementation": f"{H}:512"},
    {"name": "__eq", "behavior": "pointer equality (uobject_equal_implementation, src/LuaType/LuaUObject.cpp:2273)", "implementation": f"{H}:522"},
]

def T(name, metatable, cpp_class, parent, source, category, methods, metamethods,
      pc_notes="", needs_live_game=True, notes=None, global_ctor=None):
    # (pc_notes -> pc_only_behavior_notes; see dict below)
    d = {
        "type": name,
        "metatable_name": metatable,
        "cpp_class": cpp_class,
        "parent_type": parent,
        "category": category,
        "source_file": source,
        "methods": methods,
        "metamethods": metamethods,
        "pc_only_behavior_notes": pc_notes,
        "ps5_runtime_status": "",
        "needs_live_game": needs_live_game,
    }
    if notes:
        d["notes"] = notes
    if global_ctor:
        d["global_constructor"] = global_ctor
    return d

types = []

# ---- root base ----------------------------------------------------------
types.append(T(
    "RemoteObjectBase", "RemoteObjectBase", "RC::LuaType::ObjectBase (template root)", None,
    H, "other",
    [m("GetAddress", "obj:GetAddress() -> integer", f"{H}:172"),
     m("IsValid", "obj:IsValid() -> boolean", f"{H}:185"),
     m("type", "obj:type() -> string", f"{H}:209", notes='Returns "RemoteObjectBase" for a bare final ObjectBase; overridden by every concrete type.')],
    [],
    notes="Template base of ALL Lua-exposed UE4SS types (RemoteObjectBase/LocalObjectBase aliases). Not directly constructible from Lua; method table is inherited by every type below.",
    needs_live_game=False))

# ---- UObject family -----------------------------------------------------
types.append(T(
    "UObject", "UObject", "RC::LuaType::UObject (UObjectBase<Unreal::UObject>)", "RemoteObjectBase",
    H, "reflection",
    uobj_inherited(), UOBJ_METAMETHODS,
    notes="Root of all engine-object wrappers. Also exposes dynamic members (properties and UFunctions) via __index on live instances."))

types.append(T(
    "UStruct", "UClass", "RC::LuaType::UStruct", "UObject",
    "src/LuaType/LuaUStruct.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetSuperStruct", "s:GetSuperStruct() -> UStruct", "src/LuaType/LuaUStruct.cpp:57"),
        m("ForEachFunction", "s:ForEachFunction(function(UFunction Function) -> boolean|nil) -> nil", "src/LuaType/LuaUStruct.cpp:65", notes="Return true from callback to break."),
        m("ForEachProperty", "s:ForEachProperty(function(XProperty Property) -> boolean|nil) -> nil", "src/LuaType/LuaUStruct.cpp:96", notes="Return true from callback to break."),
    ], [],
    notes='metatable_name is literally "UClass" (UStructName::ToString returns "UClass" — historical upstream quirk, shared with UClass).'))

types.append(T(
    "UClass", "UClass", "RC::LuaType::UClass", "UStruct",
    "src/LuaType/LuaUClass.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetSuperStruct", "c:GetSuperStruct() -> UStruct", "src/LuaType/LuaUStruct.cpp:57", inh="UStruct"),
        m("ForEachFunction", "c:ForEachFunction(function(UFunction) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:65", inh="UStruct"),
        m("ForEachProperty", "c:ForEachProperty(function(XProperty) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:96", inh="UStruct"),
        m("GetCDO", "c:GetCDO() -> UObject", "src/LuaType/LuaUClass.cpp:56", notes="ClassDefaultObject; invalid UObject if instance is nullptr."),
        m("IsChildOf", "c:IsChildOf(UClass Other) -> boolean", "src/LuaType/LuaUClass.cpp:72"),
    ], [],
    notes='Shares metatable name "UClass" with UStruct (both register into the same metatable registry key).'))

types.append(T(
    "UInterface", "UInterface", "RC::LuaType::UInterface", "UClass",
    "src/LuaType/LuaUInterface.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetSuperStruct", "i:GetSuperStruct() -> UStruct", "src/LuaType/LuaUStruct.cpp:57", inh="UStruct"),
        m("ForEachFunction", "i:ForEachFunction(function(UFunction) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:65", inh="UStruct"),
        m("ForEachProperty", "i:ForEachProperty(function(XProperty) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:96", inh="UStruct"),
        m("GetCDO", "i:GetCDO() -> UObject", "src/LuaType/LuaUClass.cpp:56", inh="UClass"),
        m("IsChildOf", "i:IsChildOf(UClass Other) -> boolean", "src/LuaType/LuaUClass.cpp:72", inh="UClass"),
    ], [],
    notes="Adds no new methods; only the type() string differs."))

types.append(T(
    "UFunction", "UFunction", "RC::LuaType::UFunction", "UStruct",
    "src/LuaType/LuaUFunction.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetSuperStruct", "f:GetSuperStruct() -> UStruct", "src/LuaType/LuaUStruct.cpp:57", inh="UStruct"),
        m("ForEachFunction", "f:ForEachFunction(function(UFunction) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:65", inh="UStruct"),
        m("ForEachProperty", "f:ForEachProperty(function(XProperty) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:96", inh="UStruct"),
        m("GetFunctionFlags", "f:GetFunctionFlags() -> integer (EFunctionFlags)", "src/LuaType/LuaUFunction.cpp:78"),
        m("SetFunctionFlags", "f:SetFunctionFlags(integer NewFlags) -> nil", "src/LuaType/LuaUFunction.cpp:84"),
    ],
    [{"name": "__call", "behavior": "invoke the UFunction with a params table; out-params written back, return value(s) returned (call_ufunction_from_lua, src/LuaType/LuaUObject.cpp:153; Prospero protected C frame wrapper at :97-154)", "implementation": "src/LuaType/LuaUFunction.cpp:70"}],
    notes="Callable: obj:FuncName{ParamA=..., ...} resolves to a bound UFunction userdata whose __call executes it."))

types.append(T(
    "UEnum", "UEnum", "RC::LuaType::UEnum", "UObject",
    "src/LuaType/LuaUEnum.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetNameByValue", "e:GetNameByValue(integer Value) -> FName", "src/LuaType/LuaUEnum.cpp:54"),
        m("ForEachName", "e:ForEachName(function(FName Name, integer Value) -> boolean|nil) -> nil", "src/LuaType/LuaUEnum.cpp:72"),
        m("GetEnumNameByIndex", "e:GetEnumNameByIndex(integer Index) -> FName, integer", "src/LuaType/LuaUEnum.cpp:111", notes="Two return values."),
        m("InsertIntoNames", "e:InsertIntoNames(string Name, integer Value, integer Index [, boolean ShiftValues]) -> nil", "src/LuaType/LuaUEnum.cpp:133"),
        m("EditNameAt", "e:EditNameAt(integer Index, string NewName) -> nil", "src/LuaType/LuaUEnum.cpp:202"),
        m("EditValueAt", "e:EditValueAt(integer Index, integer NewValue) -> nil", "src/LuaType/LuaUEnum.cpp:249"),
        m("RemoveFromNamesAt", "e:RemoveFromNamesAt(integer Index [, integer Count [, boolean AllowShrinking]]) -> nil", "src/LuaType/LuaUEnum.cpp:294"),
    ], []))

types.append(T(
    "UScriptStruct", "UScriptStruct", "RC::LuaType::UScriptStruct (LocalObjectBase)", "UObject (manual merge)",
    "src/LuaType/LuaUScriptStruct.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetSuperStruct", "ss:GetSuperStruct() -> UStruct", "src/LuaType/LuaUStruct.cpp:57", inh="UStruct"),
        m("ForEachFunction", "ss:ForEachFunction(function(UFunction) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:65", inh="UStruct"),
        m("ForEachProperty", "ss:ForEachProperty(function(XProperty) -> boolean|nil)", "src/LuaType/LuaUStruct.cpp:96", inh="UStruct"),
        m("GetBaseAddress", "ss:GetBaseAddress() -> error", "src/LuaType/LuaUScriptStruct.cpp:80", notes="Deprecated & removed: always throws."),
        m("GetStructAddress", "ss:GetStructAddress() -> integer", "src/LuaType/LuaUScriptStruct.cpp:86"),
        m("GetPropertyAddress", "ss:GetPropertyAddress() -> integer", "src/LuaType/LuaUScriptStruct.cpp:95"),
        m("IsValid", "ss:IsValid() -> boolean", "src/LuaType/LuaUScriptStruct.cpp:103", notes="Override: struct mapped, in global map, not unreachable."),
        m("IsMappedToObject", "ss:IsMappedToObject() -> boolean", "src/LuaType/LuaUScriptStruct.cpp:119"),
        m("IsMappedToProperty", "ss:IsMappedToProperty() -> boolean", "src/LuaType/LuaUScriptStruct.cpp:134"),
        m("GetProperty", "ss:GetProperty() -> XStructProperty", "src/LuaType/LuaUScriptStruct.cpp:149"),
    ],
    [{"name": "__index", "behavior": "struct data member read (custom properties then reflected FProperty)", "implementation": "src/LuaType/LuaUScriptStruct.cpp:58"},
     {"name": "__newindex", "behavior": "struct data member write", "implementation": "src/LuaType/LuaUScriptStruct.cpp:63"}],
    notes="Local object wrapper (ScriptStructWrapper); cannot use the normal UObject construct chain, so inheritance is merged manually (UStruct + UObject setup_member_functions at :77-78)."))

types.append(T(
    "UWorld", "UWorld", "RC::LuaType::UWorld", "UObject",
    "src/LuaType/LuaUWorld.cpp", "other",
    uobj_inherited(True) + [
        m("SpawnActor", "w:SpawnActor(UClass Class, table Location {X,Y,Z}, table Rotation {Pitch,Yaw,Roll}) -> AActor", "src/LuaType/LuaUWorld.cpp:62",
          notes="Location/Rotation must be plain tables; userdata FVector/FRotator args are rejected (throws overload error)."),
    ], []))

types.append(T(
    "AActor", "AActor", "RC::LuaType::AActor", "UObject",
    "src/LuaType/LuaAActor.cpp", "other",
    uobj_inherited(True) + [
        m("GetWorld", "a:GetWorld() -> UWorld|nil", "src/LuaType/LuaAActor.cpp:54", notes="Overrides UObject:GetWorld with actor-specific path."),
        m("GetLevel", "a:GetLevel() -> UObject", "src/LuaType/LuaAActor.cpp:61"),
    ], []))

types.append(T(
    "UDataTable", "UDataTable", "RC::LuaType::UDataTable", "UObject",
    "src/LuaType/LuaUDataTable.cpp", "reflection",
    uobj_inherited(True) + [
        m("GetRowStruct", "dt:GetRowStruct() -> UObject|nil", "src/LuaType/LuaUDataTable.cpp:62"),
        m("GetRowMap", "dt:GetRowMap() -> table[rowName] = UScriptStruct(row reference)", "src/LuaType/LuaUDataTable.cpp:84"),
        m("FindRow", "dt:FindRow(string RowName) -> UScriptStruct|nil", "src/LuaType/LuaUDataTable.cpp:128", notes="Reference-based row access (FindRowUnchecked)."),
        m("AddRow", "dt:AddRow(string RowName, UScriptStruct|table RowData)", "src/LuaType/LuaUDataTable.cpp:134", notes="Table input converted via pushers/convert_lua_table_to_struct."),
        m("RemoveRow", "dt:RemoveRow(string RowName)", "src/LuaType/LuaUDataTable.cpp:140"),
        m("EmptyTable", "dt:EmptyTable()", "src/LuaType/LuaUDataTable.cpp:146"),
        m("GetRowNames", "dt:GetRowNames() -> table[string]", "src/LuaType/LuaUDataTable.cpp:152"),
        m("GetAllRows", "dt:GetAllRows() -> table[{Name=string, Data=UScriptStruct}]", "src/LuaType/LuaUDataTable.cpp:158"),
        m("ForEachRow", "dt:ForEachRow(function(string RowName, UScriptStruct RowData) -> boolean|nil)", "src/LuaType/LuaUDataTable.cpp:164"),
    ],
    [{"name": "__len", "behavior": "#dt returns row map Num()", "implementation": "src/LuaType/LuaUDataTable.cpp:50"}]))

# ---- FName / strings / text ---------------------------------------------
types.append(T(
    "FName", "FNameUserdata", "RC::LuaType::FName (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaFName.cpp", "string",
    OBJ_BASE + [
        m("ToString", "n:ToString() -> string", "src/LuaType/LuaFName.cpp:119"),
        m("GetComparisonIndex", "n:GetComparisonIndex() -> integer", "src/LuaType/LuaFName.cpp:127"),
        m("Equals", "n:Equals(FName Other) -> boolean", "src/LuaType/LuaFName.cpp:135", notes="Must be called with ':' not '.'."),
    ],
    [{"name": "__call", "behavior": "constructor: FName(string Name [, EFindName FindType]) or FName(integer ComparisonIndex [, EFindName])", "implementation": "src/LuaType/LuaFName.cpp:48"},
     {"name": "__eq", "behavior": "FName equality", "implementation": "src/LuaType/LuaFName.cpp:103"}],
    global_ctor="Pre-loaded global FName table + NAME_None (src/Mod/LuaMod.cpp:6190-6193)."))

types.append(T(
    "FString", "FString", "RC::LuaType::FString (TLuaStringBase<Unreal::FString>)", "ObjectBase (LocalObjectBase)",
    HS, "string",
    OBJ_BASE + [
        m("ToString", "s:ToString() -> string", f"{HS}:68"),
        m("Empty", "s:Empty()", f"{HS}:74"),
        m("Clear", "s:Clear()", f"{HS}:80", notes="Alias of Empty."),
        m("Len", "s:Len() -> integer", f"{HS}:86"),
        m("IsEmpty", "s:IsEmpty() -> boolean", f"{HS}:92"),
        m("Append", "s:Append(string|FString Other)", f"{HS}:98"),
        m("Find", "s:Find(string SubStr) -> integer|nil", f"{HS}:119", notes="1-based index; nil when not found."),
        m("StartsWith", "s:StartsWith(string Prefix) -> boolean", f"{HS}:142"),
        m("EndsWith", "s:EndsWith(string Suffix) -> boolean", f"{HS}:157"),
        m("ToUpper", "s:ToUpper() -> FString", f"{HS}:172"),
        m("ToLower", "s:ToLower() -> FString", f"{HS}:179"),
    ], [],
    global_ctor="Global constructor function FString(string) (src/Mod/LuaMod.cpp:6205).",
    needs_live_game=False,
    notes="Template-shared method set with FUtf8String/FAnsiString (TLuaStringBase). Local container; host-testable without a game."))

types.append(T(
    "FUtf8String", "FUtf8String", "RC::LuaType::FUtf8String (TLuaStringBase<Unreal::FUtf8String>)", "ObjectBase (LocalObjectBase)",
    HS, "string",
    OBJ_BASE + [
        m("ToString", "s:ToString() -> string", f"{HS}:68"),
        m("Empty", "s:Empty()", f"{HS}:74"),
        m("Clear", "s:Clear()", f"{HS}:80"),
        m("Len", "s:Len() -> integer", f"{HS}:86"),
        m("IsEmpty", "s:IsEmpty() -> boolean", f"{HS}:92"),
        m("Append", "s:Append(string|FUtf8String Other)", f"{HS}:98"),
        m("Find", "s:Find(string SubStr) -> integer|nil", f"{HS}:119"),
        m("StartsWith", "s:StartsWith(string Prefix) -> boolean", f"{HS}:142"),
        m("EndsWith", "s:EndsWith(string Suffix) -> boolean", f"{HS}:157"),
        m("ToUpper", "s:ToUpper() -> FUtf8String", f"{HS}:172"),
        m("ToLower", "s:ToLower() -> FUtf8String", f"{HS}:179"),
    ], [],
    global_ctor="Global constructor function FUtf8String(string) (src/Mod/LuaMod.cpp:6220).",
    needs_live_game=False))

types.append(T(
    "FAnsiString", "FAnsiString", "RC::LuaType::FAnsiString (TLuaStringBase<Unreal::FAnsiString>)", "ObjectBase (LocalObjectBase)",
    HS, "string",
    OBJ_BASE + [
        m("ToString", "s:ToString() -> string", f"{HS}:68"),
        m("Empty", "s:Empty()", f"{HS}:74"),
        m("Clear", "s:Clear()", f"{HS}:80"),
        m("Len", "s:Len() -> integer", f"{HS}:86"),
        m("IsEmpty", "s:IsEmpty() -> boolean", f"{HS}:92"),
        m("Append", "s:Append(string|FAnsiString Other)", f"{HS}:98"),
        m("Find", "s:Find(string SubStr) -> integer|nil", f"{HS}:119"),
        m("StartsWith", "s:StartsWith(string Prefix) -> boolean", f"{HS}:142"),
        m("EndsWith", "s:EndsWith(string Suffix) -> boolean", f"{HS}:157"),
        m("ToUpper", "s:ToUpper() -> FAnsiString", f"{HS}:172"),
        m("ToLower", "s:ToLower() -> FAnsiString", f"{HS}:179"),
    ], [],
    global_ctor="Global constructor function FAnsiString(string) (src/Mod/LuaMod.cpp:6235).",
    needs_live_game=False))

types.append(T(
    "FText", "FText", "RC::LuaType::FText (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaFText.cpp", "string",
    OBJ_BASE + [
        m("ToString", "t:ToString() -> string", "src/LuaType/LuaFText.cpp:92"),
    ],
    [{"name": "__call", "behavior": "constructor: FText(string Text)", "implementation": "src/LuaType/LuaFText.cpp:47"},
     {"name": "__eq", "behavior": "equality of string representations", "implementation": "src/LuaType/LuaFText.cpp:76"}],
    global_ctor="Pre-loaded global FText table (src/Mod/LuaMod.cpp:6199)."))

# ---- containers ----------------------------------------------------------
types.append(T(
    "TArray", "TArray", "RC::LuaType::TArray (RemoteObjectBase<FScriptArray>)", "ObjectBase (RemoteObjectBase)",
    "src/LuaType/LuaTArray.cpp", "container",
    [OBJ_BASE[0]] + [
        m("GetArrayAddress", "a:GetArrayAddress() -> integer", "src/LuaType/LuaTArray.cpp:79"),
        m("GetArrayNum", "a:GetArrayNum() -> integer", "src/LuaType/LuaTArray.cpp:87"),
        m("GetArrayMax", "a:GetArrayMax() -> integer", "src/LuaType/LuaTArray.cpp:95"),
        m("GetArrayDataAddress", "a:GetArrayDataAddress() -> integer", "src/LuaType/LuaTArray.cpp:103"),
        m("Empty", "a:Empty()", "src/LuaType/LuaTArray.cpp:111"),
        m("ForEach", "a:ForEach(function(integer Index, any Element) -> boolean|nil)", "src/LuaType/LuaTArray.cpp:121",
          notes="Elements read-only for trivial inner types, writable for UObject. Known large-array stack-corruption TODO in source."),
        m("IsValid", "a:IsValid() -> boolean", "src/LuaType/LuaTArray.cpp:182", notes="Override: remote pointer truthiness."),
    ],
    [{"name": "__index", "behavior": "a[i] element read (1-based; auto-grows with AddZeroed on OOB)", "implementation": "src/LuaType/LuaTArray.cpp:59"},
     {"name": "__newindex", "behavior": "a[i] = v element write (1-based; auto-grows)", "implementation": "src/LuaType/LuaTArray.cpp:64"},
     {"name": "__len", "behavior": "#a returns Num()", "implementation": "src/LuaType/LuaTArray.cpp:69"}]))

types.append(T(
    "TMap", "TMap", "RC::LuaType::TMap (RemoteObjectBase<FScriptMap>)", "ObjectBase (RemoteObjectBase)",
    "src/LuaType/LuaTMap.cpp", "container",
    [OBJ_BASE[0]] + [
        m("IsValid", "mp:IsValid() -> boolean", "src/LuaType/LuaTMap.cpp:74"),
        m("Find", "mp:Find(key) -> value", "src/LuaType/LuaTMap.cpp:83", notes="Throws 'Map key not found.' when absent."),
        m("Add", "mp:Add(key, value)", "src/LuaType/LuaTMap.cpp:89"),
        m("Contains", "mp:Contains(key) -> boolean", "src/LuaType/LuaTMap.cpp:95"),
        m("Remove", "mp:Remove(key)", "src/LuaType/LuaTMap.cpp:101"),
        m("Empty", "mp:Empty()", "src/LuaType/LuaTMap.cpp:107"),
        m("ForEach", "mp:ForEach(function(key Key, value Value) -> boolean|nil)", "src/LuaType/LuaTMap.cpp:113"),
    ],
    [{"name": "__len", "behavior": "#mp returns Num()", "implementation": "src/LuaType/LuaTMap.cpp:62"}]))

types.append(T(
    "TSet", "TSet", "RC::LuaType::TSet (RemoteObjectBase<FScriptSet>)", "ObjectBase (RemoteObjectBase)",
    "src/LuaType/LuaTSet.cpp", "container",
    [OBJ_BASE[0]] + [
        m("IsValid", "st:IsValid() -> boolean", "src/LuaType/LuaTSet.cpp:68"),
        m("Add", "st:Add(element)", "src/LuaType/LuaTSet.cpp:77"),
        m("Contains", "st:Contains(element) -> boolean", "src/LuaType/LuaTSet.cpp:83"),
        m("Remove", "st:Remove(element)", "src/LuaType/LuaTSet.cpp:89"),
        m("Empty", "st:Empty()", "src/LuaType/LuaTSet.cpp:95"),
        m("ForEach", "st:ForEach(function(element) -> boolean|nil)", "src/LuaType/LuaTSet.cpp:101"),
    ],
    [{"name": "__len", "behavior": "#st returns Num()", "implementation": "src/LuaType/LuaTSet.cpp:56"}]))

# ---- object references / misc engine types ------------------------------
types.append(T(
    "FWeakObjectPtr", "FWeakObjectPtr", "RC::LuaType::FWeakObjectPtr (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaFWeakObjectPtr.cpp", "object-lifecycle",
    OBJ_BASE + [
        m("Get", "w:Get() -> UObject", "src/LuaType/LuaFWeakObjectPtr.cpp:52", notes="auto_construct_object of resolved pointer."),
        m("get", "w:get() -> UObject", "src/LuaType/LuaFWeakObjectPtr.cpp:58", notes="lowercase alias"),
    ], []))

types.append(T(
    "FSoftObjectPath", "FSoftObjectPathUserdata", "RC::LuaType::FSoftObjectPath (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaFSoftObjectPath.cpp", "object-lifecycle",
    OBJ_BASE + [
        m("GetAssetPathName", "p:GetAssetPathName() -> FName", "src/LuaType/LuaFSoftObjectPath.cpp:54"),
        m("GetSubPathString", "p:GetSubPathString() -> FString", "src/LuaType/LuaFSoftObjectPath.cpp:60"),
    ], []))

types.append(T(
    "TSoftObjectPtr", "TSoftObjectPtrUserdata", "RC::LuaType::TSoftObjectPtr (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaTSoftObjectPtr.cpp", "object-lifecycle",
    OBJ_BASE + [
        m("GetWeakPtr", "sp:GetWeakPtr() -> FWeakObjectPtr", "src/LuaType/LuaTSoftObjectPtr.cpp:55"),
        m("GetTagAtLastTest", "sp:GetTagAtLastTest() -> integer", "src/LuaType/LuaTSoftObjectPtr.cpp:61"),
        m("GetObjectID", "sp:GetObjectID() -> FSoftObjectPath", "src/LuaType/LuaTSoftObjectPtr.cpp:67"),
    ], []))

types.append(T(
    "FOutputDevice", "FOutputDevice", "RC::LuaType::FOutputDevice (RemoteObjectBase)", "ObjectBase (RemoteObjectBase)",
    "src/LuaType/LuaFOutputDevice.cpp", "other",
    OBJ_BASE + [
        m("Log", "d:Log(string Message)", "src/LuaType/LuaFOutputDevice.cpp:54"),
    ], []))

types.append(T(
    "FURL", "FURLUserdata", "RC::LuaType::FURL (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaFURL.cpp", "other",
    list(OBJ_BASE),
    [{"name": "__call", "behavior": "stub (todo in source)", "implementation": "src/LuaType/LuaFURL.cpp:44"}],
    notes="No member functions bound yet ('todo: setup member functions' in source)."))

types.append(T(
    "ThreadId", "ThreadIdUserdata", "RC::LuaType::ThreadId (LocalObjectBase)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaThreadId.cpp", "other",
    OBJ_BASE + [
        m("ToString", "tid:ToString() -> string", "src/LuaType/LuaThreadId.cpp:65"),
    ],
    [{"name": "__eq", "behavior": "std::thread::id equality", "implementation": "src/LuaType/LuaThreadId.cpp:49"}],
    pc_notes="std::thread::id formatted via fmt; host threading, no Windows dependency.",
    needs_live_game=False))

# ---- XProperty family ----------------------------------------------------
def xinh():
    return [
        m("GetAddress", "obj:GetAddress() -> integer", f"{H}:172", inh="ObjectBase"),
        m("IsValid", "obj:IsValid() -> boolean", f"{H}:185", inh="ObjectBase"),
    ]

def xprop_inherited():
    tagged = []
    for mm in XPROP_OWN:
        mm = dict(mm)
        mm.setdefault("inherited_from", "XProperty")
        tagged.append(mm)
    return xinh() + tagged

XPROP_OWN = [
    m("GetClass", "p:GetClass() -> XFieldClass", "src/LuaType/LuaXProperty.cpp:97"),
    m("GetFullName", "p:GetFullName() -> string|nil", "src/LuaType/LuaXProperty.cpp:104"),
    m("GetFName", "p:GetFName() -> FName", "src/LuaType/LuaXProperty.cpp:124"),
    m("GetOffset_Internal", "p:GetOffset_Internal() -> integer", "src/LuaType/LuaXProperty.cpp:130"),
    m("IsA", "p:IsA(PropertyTypes PropertyType) -> boolean", "src/LuaType/LuaXProperty.cpp:136",
      notes="Takes the global PropertyTypes entry table; reads FFieldClassPointer member."),
    m("ContainerPtrToValuePtr", "p:ContainerPtrToValuePtr(UObjectDerivative Container [, integer ArrayIndex]) -> lightuserdata", "src/LuaType/LuaXProperty.cpp:186"),
    m("ImportText", "p:ImportText(string Buffer, lightuserdata Data, integer PortFlags, UObject OwnerObject)", "src/LuaType/LuaXProperty.cpp:215"),
]

types.append(T(
    "XProperty", "Property", "RC::LuaType::XProperty (RemoteObjectBase<FProperty>)", "ObjectBase (RemoteObjectBase)",
    "src/LuaType/LuaXProperty.cpp", "reflection",
    xinh() + XPROP_OWN, [],
    notes='metatable_name is literally "Property" (FPropertyName::ToString, historical quirk). Auto-construct dispatcher auto_construct_property at src/LuaType/LuaXProperty.cpp:17.'))

types.append(T(
    "XArrayProperty", "ArrayProperty", "RC::LuaType::XArrayProperty", "XProperty",
    "src/LuaType/LuaXArrayProperty.cpp", "reflection",
    xprop_inherited() + [
        m("GetInner", "p:GetInner() -> XProperty (inner element property)", "src/LuaType/LuaXArrayProperty.cpp:55"),
    ], []))

types.append(T(
    "XBoolProperty", "ObjectProperty", "RC::LuaType::XBoolProperty", "XProperty",
    "src/LuaType/LuaXBoolProperty.cpp", "reflection",
    xprop_inherited() + [
        m("GetByteMask", "p:GetByteMask() -> integer", "src/LuaType/LuaXBoolProperty.cpp:57"),
        m("GetByteOffset", "p:GetByteOffset() -> integer", "src/LuaType/LuaXBoolProperty.cpp:63"),
        m("GetFieldMask", "p:GetFieldMask() -> integer", "src/LuaType/LuaXBoolProperty.cpp:69"),
        m("GetFieldSize", "p:GetFieldSize() -> integer", "src/LuaType/LuaXBoolProperty.cpp:75"),
    ], [],
    notes='metatable_name is literally "ObjectProperty" (FBoolPropertyName::ToString quirk — collides by name with XObjectProperty).'))

types.append(T(
    "XEnumProperty", "EnumProperty", "RC::LuaType::XEnumProperty", "XProperty",
    "src/LuaType/LuaXEnumProperty.cpp", "reflection",
    xprop_inherited() + [
        m("GetEnum", "p:GetEnum() -> UEnum", "src/LuaType/LuaXEnumProperty.cpp:55"),
    ], []))

types.append(T(
    "XInterfaceProperty", "InterfaceProperty", "RC::LuaType::XInterfaceProperty", "XProperty",
    "src/LuaType/LuaXInterfaceProperty.cpp", "reflection",
    xprop_inherited() + [
        m("GetInterfaceClass", "p:GetInterfaceClass() -> UClass", "src/LuaType/LuaXInterfaceProperty.cpp:57"),
    ], []))

types.append(T(
    "XObjectProperty", "ObjectProperty", "RC::LuaType::XObjectProperty", "XProperty",
    "src/LuaType/LuaXObjectProperty.cpp", "reflection",
    xprop_inherited() + [
        m("GetPropertyClass", "p:GetPropertyClass() -> UClass", "src/LuaType/LuaXObjectProperty.cpp:57"),
    ], [],
    notes='metatable_name is literally "ObjectProperty" (FObjectPropertyName::ToString quirk — same string as XBoolProperty).'))

types.append(T(
    "XStructProperty", "StructProperty", "RC::LuaType::XStructProperty", "XProperty",
    "src/LuaType/LuaXStructProperty.cpp", "reflection",
    xprop_inherited() + [
        m("GetStruct", "p:GetStruct() -> UScriptStruct", "src/LuaType/LuaXStructProperty.cpp:57", notes="Wraps struct in ScriptStructWrapper with property mapping."),
    ], []))

types.append(T(
    "XFieldClass", "FieldClass", "RC::LuaType::XFieldClass (LocalObjectBase<FFieldClassVariant>)", "ObjectBase (LocalObjectBase)",
    "src/LuaType/LuaXFieldClass.cpp", "reflection",
    OBJ_BASE + [
        m("GetFName", "fc:GetFName() -> FName", "src/LuaType/LuaXFieldClass.cpp:51"),
    ], [],
    notes="Wraps FFieldClassVariant (UE>=4.25 FFieldClass / older UClass)."))

def delegate_methods(line_add, line_remove, line_broadcast, line_bindings, line_clear):
    return [
        m("Add", "d:Add(UObject TargetObject, string|FName FunctionName)", f"src/LuaType/LuaXDelegateProperty.cpp:{line_add}"),
        m("Remove", "d:Remove(UObject TargetObject, string|FName FunctionName)", f"src/LuaType/LuaXDelegateProperty.cpp:{line_remove}"),
        m("Broadcast", "d:Broadcast(... signature params ...)", f"src/LuaType/LuaXDelegateProperty.cpp:{line_broadcast}", notes="Params marshalled per signature function pushers; ProcessMulticastDelegate."),
        m("GetBindings", "d:GetBindings() -> table[{Object=UObject, FunctionName=FName}] | nil", f"src/LuaType/LuaXDelegateProperty.cpp:{line_bindings}"),
        m("Clear", "d:Clear()", f"src/LuaType/LuaXDelegateProperty.cpp:{line_clear}"),
    ]

types.append(T(
    "XDelegateProperty", "DelegateProperty", "RC::LuaType::XDelegateProperty", "XProperty",
    "src/LuaType/LuaXDelegateProperty.cpp", "reflection",
    xprop_inherited(), [],
    notes="Single-cast delegate property; inherits XProperty methods, adds none of its own (single-cast bind/call not yet exposed)."))

types.append(T(
    "XMulticastDelegateProperty", "MulticastDelegateProperty", "RC::LuaType::XMulticastDelegateProperty", "XProperty",
    "src/LuaType/LuaXDelegateProperty.cpp", "reflection",
    xprop_inherited() + delegate_methods(155, 186, 217, 279, 320), [],
    notes="Multicast inline delegate; constructed bound to base+property (PusherParams ctor at :80)."))

types.append(T(
    "XMulticastSparseDelegateProperty", "MulticastSparseDelegateProperty", "RC::LuaType::XMulticastSparseDelegateProperty", "XProperty",
    "src/LuaType/LuaXDelegateProperty.cpp", "reflection",
    xprop_inherited() + delegate_methods(430, 461, 492, 554, 595), [],
    notes="Multicast sparse delegate; same method set as XMulticastDelegateProperty."))

# ---- param wrappers ------------------------------------------------------
types.append(T(
    "RemoteUnrealParam", "RemoteUnrealParam", "RC::LuaType::RemoteUnrealParam (LuaMadeSimple::RemoteObject<void>)", None,
    "src/LuaType/LuaUObject.cpp", "reflection",
    [
        m("Get", "param:Get() -> any", "src/LuaType/LuaUObject.cpp:2432"),
        m("get", "param:get() -> any", "src/LuaType/LuaUObject.cpp:2437"),
        m("set", "param:set(any Value)", "src/LuaType/LuaUObject.cpp:2442"),
        m("Set", "param:Set(any Value)", "src/LuaType/LuaUObject.cpp:2447"),
        m("type", "param:type() -> string", "src/LuaType/LuaUObject.cpp:2453"),
    ], [],
    notes="Wraps raw out-param data for UFunction calls; routed through m_property_value_pushers."))

types.append(T(
    "LocalUnrealParam", "LocalUnrealParam", "RC::LuaType::LocalUnrealParam<ParamType> (LuaMadeSimple::LocalObject)", None,
    H, "reflection",
    [
        m("Get", "param:Get() -> any", f"{H}:886"),
        m("get", "param:get() -> any", f"{H}:891"),
        m("Set", "param:Set(any Value)", f"{H}:896"),
        m("set", "param:set(any Value)", f"{H}:901"),
        m("type", "param:type() -> string", f"{H}:907"),
    ], [],
    notes="Local by-value param wrapper used by trivial-type pushers."))

# ---- mod / singleton global types ---------------------------------------
types.append(T(
    "ModRef", "ModRef", "RC::LuaType::LuaModRef (RemoteObjectBase<RC::LuaMod>)", "ObjectBase (RemoteObjectBase)",
    "src/LuaType/LuaModRef.cpp", "other",
    [OBJ_BASE[0]] + [
        m("SetSharedVariable", "mod:SetSharedVariable(string VariableName, any Value)", "src/LuaType/LuaModRef.cpp:61",
          notes="nil/bool/lightuserdata/number/string/UObject-family userdata accepted; table/function/thread rejected."),
        m("GetSharedVariable", "mod:GetSharedVariable(string VariableName) -> any|nil", "src/LuaType/LuaModRef.cpp:170"),
    ],
    [{"name": "__newindex", "behavior": "mod.OnUnload = function() ... end registers unload callback (string field, function or nil)", "implementation": "src/LuaType/LuaModRef.cpp:36"}],
    notes="Instance per Lua mod (RegisterMod returns it); global name in scripts is chosen by the mod."))

types.append(T(
    "UE4SS", "UE4SS (global table)", "LuaMod::setup_lua_classes_internal", None,
    "src/Mod/LuaMod.cpp", "other",
    [
        m("GetVersion", "UE4SS.GetVersion() -> integer Major, integer Minor, integer Hotfix", "src/Mod/LuaMod.cpp:6107"),
    ], [],
    pc_notes="Compile-time library version constants; platform-neutral.",
    needs_live_game=False))

types.append(T(
    "UnrealVersion", "UnrealVersion (global table)", "LuaMod::setup_lua_classes_internal", None,
    "src/Mod/LuaMod.cpp", "other",
    [
        m("GetMajor", "UnrealVersion.GetMajor() -> integer", "src/Mod/LuaMod.cpp:6120"),
        m("GetMinor", "UnrealVersion.GetMinor() -> integer", "src/Mod/LuaMod.cpp:6125"),
        m("IsEqual", "UnrealVersion.IsEqual(number Major, number Minor) -> boolean", "src/Mod/LuaMod.cpp:6130"),
        m("IsAtLeast", "UnrealVersion.IsAtLeast(number Major, number Minor) -> boolean", "src/Mod/LuaMod.cpp:6141"),
        m("IsAtMost", "UnrealVersion.IsAtMost(number Major, number Minor) -> boolean", "src/Mod/LuaMod.cpp:6152"),
        m("IsBelow", "UnrealVersion.IsBelow(number Major, number Minor) -> boolean", "src/Mod/LuaMod.cpp:6163"),
        m("IsAbove", "UnrealVersion.IsAbove(number Major, number Minor) -> boolean", "src/Mod/LuaMod.cpp:6174"),
    ], [],
    needs_live_game=True,
    notes="Reads the engine version detected from the running game."))

types.append(T(
    "FPackageName", "FPackageName (global table)", "LuaMod::setup_lua_classes_internal", None,
    "src/Mod/LuaMod.cpp", "other",
    [
        m("IsShortPackageName", "FPackageName.IsShortPackageName(string PossiblyLongName) -> boolean", "src/Mod/LuaMod.cpp:6252"),
        m("IsValidLongPackageName", "FPackageName.IsValidLongPackageName(string InLongPackageName) -> boolean", "src/Mod/LuaMod.cpp:6269"),
    ], [],
    pc_notes="Pure string validation; no Win32 paths involved.",
    needs_live_game=False))

with open("/tmp/sideproject/catalog-types/types.json", "w") as f:
    json.dump(types, f, indent=2)
    f.write("\n")

print(f"wrote {len(types)} types")
for t in types:
    own = sum(1 for mm in t["methods"] if not mm.get("inherited_from"))
    print(f"  {t['type']:35s} metatable={t['metatable_name']:28s} methods={len(t['methods'])} (own {own}) metamethods={len(t['metamethods'])}")
