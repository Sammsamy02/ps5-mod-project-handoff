if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("__vecDelDtor")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

