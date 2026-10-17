if (auto it = FUObjectHashTables::MemberOffsets.find(STR("Hash")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("Hash"), 0x28);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("HashOuter")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("HashOuter"), 0x78);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("ObjectOuterMap")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("ObjectOuterMap"), 0xC8);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("ClassToObjectListMap")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("ClassToObjectListMap"), 0x118);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("ClassToChildListMap")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("ClassToChildListMap"), 0x168);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("ClassToChildListMapVersion")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("ClassToChildListMapVersion"), 0x1B8);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("PackageToObjectListMap")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("PackageToObjectListMap"), 0x1C0);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("ObjectToPackageMap")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("ObjectToPackageMap"), 0x210);
}

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("UEP_TotalSize")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("UEP_TotalSize"), 0x260);
}
