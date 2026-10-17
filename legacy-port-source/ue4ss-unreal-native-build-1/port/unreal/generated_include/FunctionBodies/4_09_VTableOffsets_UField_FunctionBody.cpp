if (auto it = UField::VTableLayoutMap.find(STR("__vecDelDtor")); it == UField::VTableLayoutMap.end())
{
    UField::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UField::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UField::VTableLayoutMap.end())
{
    UField::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UField::VTableLayoutMap.find(STR("AddCppProperty")); it == UField::VTableLayoutMap.end())
{
    UField::VTableLayoutMap.emplace(STR("AddCppProperty"), 0x1C8);
}

if (auto it = UField::VTableLayoutMap.find(STR("AddCppProperty__Ptr_UProperty")); it == UField::VTableLayoutMap.end())
{
    UField::VTableLayoutMap.emplace(STR("AddCppProperty__Ptr_UProperty"), 0x1C8);
}

if (auto it = UField::VTableLayoutMap.find(STR("Bind")); it == UField::VTableLayoutMap.end())
{
    UField::VTableLayoutMap.emplace(STR("Bind"), 0x1D0);
}

