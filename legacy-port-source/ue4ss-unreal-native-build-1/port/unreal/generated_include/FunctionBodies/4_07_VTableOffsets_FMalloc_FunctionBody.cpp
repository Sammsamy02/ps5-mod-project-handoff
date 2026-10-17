if (auto it = FMalloc::VTableLayoutMap.find(STR("__vecDelDtor")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("InitializeStatsMetadata")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("InitializeStatsMetadata"), 0x10);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Malloc")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Malloc"), 0x18);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Malloc__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Malloc__uint64__uint32"), 0x18);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Realloc")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Realloc"), 0x20);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Realloc__Ptr_void__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Realloc__Ptr_void__uint64__uint32"), 0x20);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Free")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Free"), 0x28);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Free__Ptr_void")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Free__Ptr_void"), 0x28);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("UpdateStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("UpdateStats"), 0x30);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocatorStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocatorStats"), 0x38);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocatorStats__Ref_FGenericMemoryStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocatorStats__Ref_FGenericMemoryStats"), 0x38);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("DumpAllocatorStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("DumpAllocatorStats"), 0x40);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("DumpAllocatorStats__Ref_FOutputDevice")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("DumpAllocatorStats__Ref_FOutputDevice"), 0x40);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("IsInternallyThreadSafe")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("IsInternallyThreadSafe"), 0x48);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("IsInternallyThreadSafe_C")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("IsInternallyThreadSafe_C"), 0x48);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("ValidateHeap")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("ValidateHeap"), 0x50);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocationSize")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocationSize"), 0x58);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocationSize__Ptr_void__Ref_uint64")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocationSize__Ptr_void__Ref_uint64"), 0x58);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetDescriptiveName")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetDescriptiveName"), 0x60);
}

