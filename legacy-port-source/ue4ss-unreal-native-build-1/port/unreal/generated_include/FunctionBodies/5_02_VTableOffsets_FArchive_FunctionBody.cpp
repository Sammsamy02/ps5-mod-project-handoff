if (auto it = FArchive::VTableLayoutMap.find(STR("__vecDelDtor")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FWeakObjectPtr")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FWeakObjectPtr"), 0x110);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FSoftObjectPath")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FSoftObjectPath"), 0x118);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FSoftObjectPtr")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FSoftObjectPtr"), 0x120);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FObjectPtr")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FObjectPtr"), 0x128);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FLazyObjectPtr")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FLazyObjectPtr"), 0x130);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ptr_Ref_FField")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ptr_Ref_FField"), 0x138);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ptr_Ref_UObject")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ptr_Ref_UObject"), 0x140);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FText")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FText"), 0x148);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("operator<<__Ref_FName")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("operator<<__Ref_FName"), 0x150);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("ForceBlueprintFinalization")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("ForceBlueprintFinalization"), 0x158);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Serialize")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Serialize"), 0x160);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Serialize__Ptr_void__int64")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Serialize__Ptr_void__int64"), 0x160);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeBits")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeBits"), 0x168);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeBits__Ptr_void__int64")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeBits__Ptr_void__int64"), 0x168);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeInt")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeInt"), 0x170);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeInt__Ref_uint32__uint32")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeInt__Ref_uint32__uint32"), 0x170);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeIntPacked")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeIntPacked"), 0x178);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeIntPacked__Ref_uint32")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeIntPacked__Ref_uint32"), 0x178);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Preload")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Preload"), 0x180);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Preload__Ptr_UObject")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Preload__Ptr_UObject"), 0x180);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Seek")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Seek"), 0x188);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Seek__int64")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Seek__int64"), 0x188);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("AttachBulkData__Ptr_UE_Serialization_FEditorBulkData")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("AttachBulkData__Ptr_UE_Serialization_FEditorBulkData"), 0x190);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("AttachBulkData__Ptr_UObject__Ptr_FBulkData")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("AttachBulkData__Ptr_UObject__Ptr_FBulkData"), 0x198);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("DetachBulkData__Ptr_UE_Serialization_FEditorBulkData__bool")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("DetachBulkData__Ptr_UE_Serialization_FEditorBulkData__bool"), 0x1A0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("DetachBulkData__Ptr_FBulkData__bool")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("DetachBulkData__Ptr_FBulkData__bool"), 0x1A8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeBulkData")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeBulkData"), 0x1B0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SerializeBulkData__Ref_FBulkData__Ref_C_FBulkDataSerializationParams")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SerializeBulkData__Ref_FBulkData__Ref_C_FBulkDataSerializationParams"), 0x1B0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("IsProxyOf")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("IsProxyOf"), 0x1B8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("IsProxyOf_C__Ptr_FArchive")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("IsProxyOf_C__Ptr_FArchive"), 0x1B8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Precache")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Precache"), 0x1C0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Precache__int64__int64")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Precache__int64__int64"), 0x1C0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("FlushCache")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("FlushCache"), 0x1C8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SetCompressionMap")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SetCompressionMap"), 0x1D0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("SetCompressionMap__Ptr_TArray_FCompressedChunk_FDefaultAllocator___ECompressionFlags")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("SetCompressionMap__Ptr_TArray_FCompressedChunk_FDefaultAllocator___ECompressionFlags"), 0x1D0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Flush")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Flush"), 0x1D8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("Close")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("Close"), 0x1E0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("MarkScriptSerializationStart")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("MarkScriptSerializationStart"), 0x1E8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("MarkScriptSerializationStart__Ptr_C_UObject")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("MarkScriptSerializationStart__Ptr_C_UObject"), 0x1E8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("MarkScriptSerializationEnd")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("MarkScriptSerializationEnd"), 0x1F0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("MarkScriptSerializationEnd__Ptr_C_UObject")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("MarkScriptSerializationEnd__Ptr_C_UObject"), 0x1F0);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("MarkSearchableName")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("MarkSearchableName"), 0x1F8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("MarkSearchableName_C__Ptr_C_UObject__Ref_C_FName")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("MarkSearchableName_C__Ptr_C_UObject__Ref_C_FName"), 0x1F8);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("UsingCustomVersion")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("UsingCustomVersion"), 0x200);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("UsingCustomVersion__Ref_C_FGuid")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("UsingCustomVersion__Ref_C_FGuid"), 0x200);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("GetCacheableArchive")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("GetCacheableArchive"), 0x208);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PushSerializedProperty")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PushSerializedProperty"), 0x210);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PushSerializedProperty__Ptr_FProperty__C_bool")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PushSerializedProperty__Ptr_FProperty__C_bool"), 0x210);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PopSerializedProperty")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PopSerializedProperty"), 0x218);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PopSerializedProperty__Ptr_FProperty__C_bool")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PopSerializedProperty__Ptr_FProperty__C_bool"), 0x218);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("AttachExternalReadDependency")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("AttachExternalReadDependency"), 0x220);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("AttachExternalReadDependency__Ref_TFunction_bool_cdecl_double_")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("AttachExternalReadDependency__Ref_TFunction_bool_cdecl_double_"), 0x220);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PushFileRegionType")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PushFileRegionType"), 0x228);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PushFileRegionType__EFileRegionType")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PushFileRegionType__EFileRegionType"), 0x228);
}

if (auto it = FArchive::VTableLayoutMap.find(STR("PopFileRegionType")); it == FArchive::VTableLayoutMap.end())
{
    FArchive::VTableLayoutMap.emplace(STR("PopFileRegionType"), 0x230);
}

