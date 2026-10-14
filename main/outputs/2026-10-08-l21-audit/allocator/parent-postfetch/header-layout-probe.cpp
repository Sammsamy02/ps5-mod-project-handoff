#include "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h"
static_assert(sizeof(void*) == 8);
FExec::~FExec() {}
bool FExec::Exec(UWorld*, const TCHAR*, FOutputDevice&) { return false; }
#if !UE_ALLOW_EXEC_COMMANDS
bool FExec::Exec(UWorld*, const TCHAR*) { return false; }
#endif
void* FMalloc::TryMalloc(SIZE_T c, uint32 a) { return Malloc(c,a); }
void* FMalloc::TryRealloc(void* p, SIZE_T c, uint32 a) { return Realloc(p,c,a); }
void* FMalloc::MallocZeroed(SIZE_T c, uint32 a) { return Malloc(c,a); }
void* FMalloc::TryMallocZeroed(SIZE_T c, uint32 a) { return TryMalloc(c,a); }
void FMalloc::InitializeStatsMetadata() {}
void FMalloc::UpdateStats() {}
void FMalloc::GetAllocatorStats(FGenericMemoryStats&) {}
struct AllocatorLayoutProbe final : FMalloc {
 void* Malloc(SIZE_T, uint32) override { return nullptr; }
 void* Realloc(void*, SIZE_T, uint32) override { return nullptr; }
 void Free(void*) override {}
};
AllocatorLayoutProbe force_vtable;
