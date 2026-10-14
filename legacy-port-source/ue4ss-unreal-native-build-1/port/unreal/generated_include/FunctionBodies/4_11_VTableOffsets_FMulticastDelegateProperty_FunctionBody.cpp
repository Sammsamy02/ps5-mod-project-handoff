if (auto it = FMulticastDelegateProperty::VTableLayoutMap.find(STR("__vecDelDtor")); it == FMulticastDelegateProperty::VTableLayoutMap.end())
{
    FMulticastDelegateProperty::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = FMulticastDelegateProperty::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == FMulticastDelegateProperty::VTableLayoutMap.end())
{
    FMulticastDelegateProperty::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

