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

if (auto it = FUObjectHashTables::MemberOffsets.find(STR("UEP_TotalSize")); it == FUObjectHashTables::MemberOffsets.end())
{
    FUObjectHashTables::MemberOffsets.emplace(STR("UEP_TotalSize"), 0x1B8);
}
