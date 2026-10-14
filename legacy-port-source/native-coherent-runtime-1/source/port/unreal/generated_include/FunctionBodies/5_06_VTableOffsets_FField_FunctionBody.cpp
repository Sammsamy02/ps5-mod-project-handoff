if (auto it = FField::VTableLayoutMap.find(STR("GetFieldSize")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetFieldSize"), 0x0);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetFieldSize_C")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetFieldSize_C"), 0x0);
}

if (auto it = FField::VTableLayoutMap.find(STR("__vecDelDtor")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x8);
}

if (auto it = FField::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x8);
}

if (auto it = FField::VTableLayoutMap.find(STR("Serialize")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("Serialize"), 0x10);
}

if (auto it = FField::VTableLayoutMap.find(STR("Serialize__Ref_FArchive")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("Serialize__Ref_FArchive"), 0x10);
}

if (auto it = FField::VTableLayoutMap.find(STR("PostLoad")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("PostLoad"), 0x18);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetPreloadDependencies")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetPreloadDependencies"), 0x20);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetPreloadDependencies__Ref_TArray_UObject_*_FDefaultAllocator_")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetPreloadDependencies__Ref_TArray_UObject_*_FDefaultAllocator_"), 0x20);
}

if (auto it = FField::VTableLayoutMap.find(STR("BeginDestroy")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("BeginDestroy"), 0x28);
}

if (auto it = FField::VTableLayoutMap.find(STR("AddReferencedObjects")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("AddReferencedObjects"), 0x30);
}

if (auto it = FField::VTableLayoutMap.find(STR("AddReferencedObjects__Ref_FReferenceCollector")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("AddReferencedObjects__Ref_FReferenceCollector"), 0x30);
}

if (auto it = FField::VTableLayoutMap.find(STR("AddCppProperty")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("AddCppProperty"), 0x38);
}

if (auto it = FField::VTableLayoutMap.find(STR("AddCppProperty__Ptr_FProperty")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("AddCppProperty__Ptr_FProperty"), 0x38);
}

if (auto it = FField::VTableLayoutMap.find(STR("Bind")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("Bind"), 0x40);
}

if (auto it = FField::VTableLayoutMap.find(STR("PostDuplicate")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("PostDuplicate"), 0x48);
}

if (auto it = FField::VTableLayoutMap.find(STR("PostDuplicate__Ref_C_FField")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("PostDuplicate__Ref_C_FField"), 0x48);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetInnerFieldByName")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetInnerFieldByName"), 0x50);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetInnerFieldByName__Ref_C_FName")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetInnerFieldByName__Ref_C_FName"), 0x50);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetInnerFields")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetInnerFields"), 0x58);
}

if (auto it = FField::VTableLayoutMap.find(STR("GetInnerFields__Ref_TArray_FField_*_FDefaultAllocator_")); it == FField::VTableLayoutMap.end())
{
    FField::VTableLayoutMap.emplace(STR("GetInnerFields__Ref_TArray_FField_*_FDefaultAllocator_"), 0x58);
}

