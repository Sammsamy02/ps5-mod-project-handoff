if (auto it = FArchiveState::VTableLayoutMap.find(STR("__vecDelDtor")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetInnermostState")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetInnermostState"), 0x8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("CountBytes")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("CountBytes"), 0x10);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("CountBytes__uint64__uint64")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("CountBytes__uint64__uint64"), 0x10);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetArchiveName")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetArchiveName"), 0x18);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetArchiveName_C")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetArchiveName_C"), 0x18);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetLinker")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetLinker"), 0x20);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("Tell")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("Tell"), 0x28);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("TotalSize")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("TotalSize"), 0x30);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("AtEnd")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("AtEnd"), 0x38);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetArchetypeFromLoader")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetArchetypeFromLoader"), 0x40);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetArchetypeFromLoader__Ptr_C_UObject")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetArchetypeFromLoader__Ptr_C_UObject"), 0x40);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("EngineNetVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("EngineNetVer"), 0x48);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("EngineNetVer_C")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("EngineNetVer_C"), 0x48);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GameNetVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GameNetVer"), 0x50);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GameNetVer_C")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GameNetVer_C"), 0x50);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetMigrationContext")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetMigrationContext"), 0x58);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetMigrationContext_C")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetMigrationContext_C"), 0x58);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetCustomVersions")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetCustomVersions"), 0x60);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("GetCustomVersions_C")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("GetCustomVersions_C"), 0x60);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetCustomVersions")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetCustomVersions"), 0x68);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetCustomVersions__Ref_C_FCustomVersionContainer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetCustomVersions__Ref_C_FCustomVersionContainer"), 0x68);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("ResetCustomVersions")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("ResetCustomVersions"), 0x70);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetDebugSerializationFlags")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetDebugSerializationFlags"), 0x78);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetDebugSerializationFlags__uint32")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetDebugSerializationFlags__uint32"), 0x78);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetFilterEditorOnly")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetFilterEditorOnly"), 0x80);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetFilterEditorOnly__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetFilterEditorOnly__bool"), 0x80);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetSavingOptionalObject")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetSavingOptionalObject"), 0x88);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetSavingOptionalObject__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetSavingOptionalObject__bool"), 0x88);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("UseToResolveEnumerators")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("UseToResolveEnumerators"), 0x90);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("UseToResolveEnumerators_C")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("UseToResolveEnumerators_C"), 0x90);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("ShouldSkipProperty")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("ShouldSkipProperty"), 0x98);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("ShouldSkipProperty_C__Ptr_C_FProperty")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("ShouldSkipProperty_C__Ptr_C_FProperty"), 0x98);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetSerializedProperty")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetSerializedProperty"), 0xA0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetSerializedProperty__Ptr_FProperty")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetSerializedProperty__Ptr_FProperty"), 0xA0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetSerializedPropertyChain")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetSerializedPropertyChain"), 0xA8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetSerializedPropertyChain__Ptr_C_FArchiveSerializedPropertyChain__Ptr_FProperty")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetSerializedPropertyChain__Ptr_C_FArchiveSerializedPropertyChain__Ptr_FProperty"), 0xA8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("Reset")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("Reset"), 0xB0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsLoading")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsLoading"), 0xB8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsLoading__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsLoading__bool"), 0xB8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsLoadingFromCookedPackage")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsLoadingFromCookedPackage"), 0xC0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsLoadingFromCookedPackage__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsLoadingFromCookedPackage__bool"), 0xC0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsSaving")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsSaving"), 0xC8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsSaving__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsSaving__bool"), 0xC8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsTransacting")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsTransacting"), 0xD0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsTransacting__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsTransacting__bool"), 0xD0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsTextFormat")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsTextFormat"), 0xD8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsTextFormat__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsTextFormat__bool"), 0xD8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetWantBinaryPropertySerialization")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetWantBinaryPropertySerialization"), 0xE0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetWantBinaryPropertySerialization__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetWantBinaryPropertySerialization__bool"), 0xE0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetUseUnversionedPropertySerialization")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetUseUnversionedPropertySerialization"), 0xE8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetUseUnversionedPropertySerialization__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetUseUnversionedPropertySerialization__bool"), 0xE8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetForceUnicode")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetForceUnicode"), 0xF0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetForceUnicode__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetForceUnicode__bool"), 0xF0);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsPersistent")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsPersistent"), 0xF8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetIsPersistent__bool")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetIsPersistent__bool"), 0xF8);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetUEVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetUEVer"), 0x100);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetUEVer__FPackageFileVersion")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetUEVer__FPackageFileVersion"), 0x100);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetLicenseeUEVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetLicenseeUEVer"), 0x108);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetLicenseeUEVer__int32")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetLicenseeUEVer__int32"), 0x108);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetEngineVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetEngineVer"), 0x110);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetEngineVer__Ref_C_FEngineVersionBase")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetEngineVer__Ref_C_FEngineVersionBase"), 0x110);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetEngineNetVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetEngineNetVer"), 0x118);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetEngineNetVer__C_uint32")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetEngineNetVer__C_uint32"), 0x118);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetGameNetVer")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetGameNetVer"), 0x120);
}

if (auto it = FArchiveState::VTableLayoutMap.find(STR("SetGameNetVer__C_uint32")); it == FArchiveState::VTableLayoutMap.end())
{
    FArchiveState::VTableLayoutMap.emplace(STR("SetGameNetVer__C_uint32"), 0x120);
}

