// [l25 agent-AU 2026-10-08] Corrected compiled-in 5_06 FMalloc vtable DEFAULT map.
// Binary-verified truth (agent-AB gmalloc-audit §4/§7; agent-AH §1.1 reconstructed
// vtable + lineage staged table, independently): Malloc=+0x30 TryMalloc=+0x38
// Realloc=+0x40 TryRealloc=+0x48 Free=+0x50 ... GetDescriptiveName=+0xd0. The
// generator's original table was exactly one slot (+8) low on every entry (it
// assumed a 3-slot prefix; this Konami/Sony shipping build carries a dtor pair
// plus four Exec-family prefix slots). Latent hazard only while the staged native
// layout table publishes first (insert-if-absent), but a staging failure would
// silently mis-dispatch every FMalloc virtual one slot low (Realloc -> TryMalloc
// wrapper -> Malloc(ptr-as-size)), per AB §5. __vecDelDtor* slots unchanged.
// Staged candidate of port-root original; port root untouched.

if (auto it = FMalloc::VTableLayoutMap.find(STR("__vecDelDtor")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Malloc")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Malloc"), 0x30);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Malloc__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Malloc__uint64__uint32"), 0x30);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("TryMalloc")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("TryMalloc"), 0x38);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("TryMalloc__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("TryMalloc__uint64__uint32"), 0x38);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Realloc")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Realloc"), 0x40);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Realloc__Ptr_void__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Realloc__Ptr_void__uint64__uint32"), 0x40);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("TryRealloc")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("TryRealloc"), 0x48);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("TryRealloc__Ptr_void__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("TryRealloc__Ptr_void__uint64__uint32"), 0x48);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Free")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Free"), 0x50);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Free__Ptr_void")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Free__Ptr_void"), 0x50);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("MallocZeroed")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("MallocZeroed"), 0x58);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("MallocZeroed__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("MallocZeroed__uint64__uint32"), 0x58);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("TryMallocZeroed")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("TryMallocZeroed"), 0x60);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("TryMallocZeroed__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("TryMallocZeroed__uint64__uint32"), 0x60);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("QuantizeSize")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("QuantizeSize"), 0x68);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("QuantizeSize__uint64__uint32")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("QuantizeSize__uint64__uint32"), 0x68);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocationSize")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocationSize"), 0x70);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocationSize__Ptr_void__Ref_uint64")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocationSize__Ptr_void__Ref_uint64"), 0x70);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Trim")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Trim"), 0x78);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("Trim__bool")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("Trim__bool"), 0x78);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("SetupTLSCachesOnCurrentThread")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("SetupTLSCachesOnCurrentThread"), 0x80);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("MarkTLSCachesAsUsedOnCurrentThread")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("MarkTLSCachesAsUsedOnCurrentThread"), 0x88);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("MarkTLSCachesAsUnusedOnCurrentThread")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("MarkTLSCachesAsUnusedOnCurrentThread"), 0x90);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("ClearAndDisableTLSCachesOnCurrentThread")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("ClearAndDisableTLSCachesOnCurrentThread"), 0x98);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("InitializeStatsMetadata")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("InitializeStatsMetadata"), 0xA0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("UpdateStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("UpdateStats"), 0xA8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocatorStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocatorStats"), 0xB0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetAllocatorStats__Ref_FGenericMemoryStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetAllocatorStats__Ref_FGenericMemoryStats"), 0xB0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("DumpAllocatorStats")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("DumpAllocatorStats"), 0xB8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("DumpAllocatorStats__Ref_FOutputDevice")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("DumpAllocatorStats__Ref_FOutputDevice"), 0xB8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("IsInternallyThreadSafe")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("IsInternallyThreadSafe"), 0xC0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("IsInternallyThreadSafe_C")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("IsInternallyThreadSafe_C"), 0xC0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("ValidateHeap")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("ValidateHeap"), 0xC8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetDescriptiveName")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetDescriptiveName"), 0xD0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("OnMallocInitialized")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("OnMallocInitialized"), 0xD8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("OnPreFork")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("OnPreFork"), 0xE0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("OnPostFork")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("OnPostFork"), 0xE8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetImmediatelyFreeableCachedMemorySize")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetImmediatelyFreeableCachedMemorySize"), 0xF0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetImmediatelyFreeableCachedMemorySize_C")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetImmediatelyFreeableCachedMemorySize_C"), 0xF0);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetTotalFreeCachedMemorySize")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetTotalFreeCachedMemorySize"), 0xF8);
}

if (auto it = FMalloc::VTableLayoutMap.find(STR("GetTotalFreeCachedMemorySize_C")); it == FMalloc::VTableLayoutMap.end())
{
    FMalloc::VTableLayoutMap.emplace(STR("GetTotalFreeCachedMemorySize_C"), 0xF8);
}

