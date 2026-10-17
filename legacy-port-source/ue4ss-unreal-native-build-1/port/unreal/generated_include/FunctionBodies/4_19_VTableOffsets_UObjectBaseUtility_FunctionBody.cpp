if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("__vecDelDtor")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("CanBeClusterRoot")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("CanBeClusterRoot"), 0x18);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("CanBeClusterRoot_C")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("CanBeClusterRoot_C"), 0x18);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("CanBeInCluster")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("CanBeInCluster"), 0x20);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("CanBeInCluster_C")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("CanBeInCluster_C"), 0x20);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("CreateCluster")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("CreateCluster"), 0x28);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("OnClusterMarkedAsPendingKill")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("OnClusterMarkedAsPendingKill"), 0x30);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("AddToCluster")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("AddToCluster"), 0x38);
}

if (auto it = UObjectBaseUtility::VTableLayoutMap.find(STR("AddToCluster__Ptr_UObjectBaseUtility__bool")); it == UObjectBaseUtility::VTableLayoutMap.end())
{
    UObjectBaseUtility::VTableLayoutMap.emplace(STR("AddToCluster__Ptr_UObjectBaseUtility__bool"), 0x38);
}

