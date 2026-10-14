if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjFirstGCIndex")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjFirstGCIndex"), 0x0);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjLastNonGCIndex")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjLastNonGCIndex"), 0x4);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("OpenForDisregardForGC")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("OpenForDisregardForGC"), 0x8);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjObjects")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjObjects"), 0x10);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("ObjAvailable")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("ObjAvailable"), 0x20);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("UObjectCreateListeners")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("UObjectCreateListeners"), 0x30);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("UObjectDeleteListeners")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("UObjectDeleteListeners"), 0x40);
}

if (auto it = FUObjectArray::MemberOffsets.find(STR("UEP_TotalSize")); it == FUObjectArray::MemberOffsets.end())
{
    FUObjectArray::MemberOffsets.emplace(STR("UEP_TotalSize"), 0x50);
}
