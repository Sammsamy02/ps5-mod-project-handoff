if (auto it = UObjectBase::VTableLayoutMap.find(STR("__vecDelDtor")); it == UObjectBase::VTableLayoutMap.end())
{
    UObjectBase::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UObjectBase::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UObjectBase::VTableLayoutMap.end())
{
    UObjectBase::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UObjectBase::VTableLayoutMap.find(STR("RegisterDependencies")); it == UObjectBase::VTableLayoutMap.end())
{
    UObjectBase::VTableLayoutMap.emplace(STR("RegisterDependencies"), 0x8);
}

if (auto it = UObjectBase::VTableLayoutMap.find(STR("DeferredRegister")); it == UObjectBase::VTableLayoutMap.end())
{
    UObjectBase::VTableLayoutMap.emplace(STR("DeferredRegister"), 0x10);
}

if (auto it = UObjectBase::VTableLayoutMap.find(STR("DeferredRegister__Ptr_UClass__Ptr_C_wchar_t__Ptr_C_wchar_t")); it == UObjectBase::VTableLayoutMap.end())
{
    UObjectBase::VTableLayoutMap.emplace(STR("DeferredRegister__Ptr_UClass__Ptr_C_wchar_t__Ptr_C_wchar_t"), 0x10);
}

