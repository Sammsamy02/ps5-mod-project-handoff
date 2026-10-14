if (auto it = UDataTable::VTableLayoutMap.find(STR("__vecDelDtor")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("_getUObject")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("_getUObject"), 0x1B0);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("_getUObject_C")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("_getUObject_C"), 0x1B0);
}

