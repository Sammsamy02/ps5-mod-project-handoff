if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjObjects")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjObjects"), 0x0);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjFirstGCIndex")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjFirstGCIndex"), 0x20);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjLastNonGCIndex")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjLastNonGCIndex"), 0x24);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("MaxObjectsNotConsideredByGC")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("MaxObjectsNotConsideredByGC"), 0x28);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("OpenForDisregardForGC")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("OpenForDisregardForGC"), 0x2C);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjAvailableList")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjAvailableList"), 0x58);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjAvailableListEstimateCount")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjAvailableListEstimateCount"), 0x68);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("UObjectCreateListeners")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("UObjectCreateListeners"), 0x70);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("UObjectDeleteListeners")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("UObjectDeleteListeners"), 0xA8);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("PrimarySerialNumber")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("PrimarySerialNumber"), 0xE0);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("bShouldRecycleObjectIndices")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("bShouldRecycleObjectIndices"), 0xE4);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("UEP_TotalSize")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("UEP_TotalSize"), 0xE8);
}
