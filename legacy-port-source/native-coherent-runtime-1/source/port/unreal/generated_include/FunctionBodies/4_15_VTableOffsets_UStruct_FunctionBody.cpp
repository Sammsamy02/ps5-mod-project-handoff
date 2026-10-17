if (auto it = UStruct::VTableLayoutMap.find(STR("__vecDelDtor")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("GetInheritanceSuper")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("GetInheritanceSuper"), 0x218);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("GetInheritanceSuper_C")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("GetInheritanceSuper_C"), 0x218);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("Link")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("Link"), 0x220);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("Link__Ref_FArchive__bool")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("Link__Ref_FArchive__bool"), 0x220);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeBin")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeBin"), 0x228);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeBin_C__Ref_FArchive__Ptr_void")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeBin_C__Ref_FArchive__Ptr_void"), 0x228);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeTaggedProperties")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeTaggedProperties"), 0x230);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeTaggedProperties_C__Ref_FArchive__Ptr_uint8__Ptr_UStruct__Ptr_uint8__Ptr_C_UObject")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeTaggedProperties_C__Ref_FArchive__Ptr_uint8__Ptr_UStruct__Ptr_uint8__Ptr_C_UObject"), 0x230);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("InitializeStruct")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("InitializeStruct"), 0x238);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("InitializeStruct_C__Ptr_void__int32")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("InitializeStruct_C__Ptr_void__int32"), 0x238);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("DestroyStruct")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("DestroyStruct"), 0x240);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("DestroyStruct_C__Ptr_void__int32")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("DestroyStruct_C__Ptr_void__int32"), 0x240);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeExpr")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeExpr"), 0x248);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeExpr__Ref_int32__Ref_FArchive")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeExpr__Ref_int32__Ref_FArchive"), 0x248);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("GetPrefixCPP")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("GetPrefixCPP"), 0x250);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("GetPrefixCPP_C")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("GetPrefixCPP_C"), 0x250);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SetSuperStruct")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SetSuperStruct"), 0x258);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SetSuperStruct__Ptr_UStruct")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SetSuperStruct__Ptr_UStruct"), 0x258);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeSuperStruct")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeSuperStruct"), 0x260);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("SerializeSuperStruct__Ref_FArchive")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("SerializeSuperStruct__Ref_FArchive"), 0x260);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("PropertyNameToDisplayName")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("PropertyNameToDisplayName"), 0x268);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("PropertyNameToDisplayName_C__FName")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("PropertyNameToDisplayName_C__FName"), 0x268);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("FindPropertyNameFromGuid")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("FindPropertyNameFromGuid"), 0x270);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("FindPropertyNameFromGuid_C__Ref_C_FGuid")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("FindPropertyNameFromGuid_C__Ref_C_FGuid"), 0x270);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("FindPropertyGuidFromName")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("FindPropertyGuidFromName"), 0x278);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("FindPropertyGuidFromName_C__C_FName")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("FindPropertyGuidFromName_C__C_FName"), 0x278);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("ArePropertyGuidsAvailable")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("ArePropertyGuidsAvailable"), 0x280);
}

if (auto it = UStruct::VTableLayoutMap.find(STR("ArePropertyGuidsAvailable_C")); it == UStruct::VTableLayoutMap.end())
{
    UStruct::VTableLayoutMap.emplace(STR("ArePropertyGuidsAvailable_C"), 0x280);
}

