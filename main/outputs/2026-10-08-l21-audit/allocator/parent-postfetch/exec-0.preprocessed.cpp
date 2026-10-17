# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/header-layout-probe.cpp"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 471 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/header-layout-probe.cpp" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 1




# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/CoreTypes.h" 1

using SIZE_T=long unsigned int;
using size_t=long unsigned int;
using uint32=unsigned int;
using uint64=unsigned long long;
using TCHAR=char16_t;
# 6 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/HAL/PlatformAtomics.h" 1
# 7 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/HAL/PlatformCrt.h" 1
# 8 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/Misc/Exec.h" 1
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/Exec.h" 1





# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/Misc/AssertionMacros.h" 1
# 7 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/Exec.h" 2

class FOutputDevice;
class UWorld;
# 28 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/Exec.h"
class FExec
{
public:
          virtual ~FExec();
# 50 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/Exec.h"
          virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) final;




          virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd ) final;



protected:

 virtual bool Exec_Runtime( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) { return false; }


 virtual bool Exec_Dev( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) { return false; }


 virtual bool Exec_Editor( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) { return false; }

};
# 2 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/Misc/Exec.h" 2
# 9 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/Misc/OutputDevice.h" 1

class FOutputDevice { public: void Logf(const TCHAR*, ...); };
# 10 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 2
# 1 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/stubs/Templates/Atomic.h" 1
# 11 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h" 2

class UWorld;
template <typename T> class TAtomic;





enum
{



 DEFAULT_ALIGNMENT = 0,


 MIN_ALIGNMENT = 8,
};







namespace UE::Private
{
          extern class FMalloc* GMalloc;
}







         extern class FMalloc* const& GMalloc;


         extern class FMalloc** GFixedMallocLocationPtr;


struct FGenericMemoryStats;







class FUseSystemMallocForNew
{
public:






          void* operator new(size_t Size);






          void operator delete(void* Ptr);







 void* operator new[](size_t Size);






 void operator delete[](void* Ptr);
};


class FMalloc :
 public FUseSystemMallocForNew,
 public FExec
{
public:



 virtual void* Malloc( SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT ) = 0;





          virtual void* TryMalloc( SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT );




 virtual void* Realloc( void* Original, SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT ) = 0;






          virtual void* TryRealloc(void* Original, SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT);




 virtual void Free( void* Original ) = 0;




          virtual void* MallocZeroed(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);





          virtual void* TryMallocZeroed(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);







 virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment)
 {
  return Count;
 }
# 158 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h"
 virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut)
 {
  return false;
 }




 virtual void Trim(bool bTrimThreadCaches)
 {
 }




 virtual void SetupTLSCachesOnCurrentThread()
 {
 }




 virtual void MarkTLSCachesAsUsedOnCurrentThread()
 {
 }




 virtual void MarkTLSCachesAsUnusedOnCurrentThread()
 {
 }




 virtual void ClearAndDisableTLSCachesOnCurrentThread()
 {
 }





          virtual void InitializeStatsMetadata();
# 215 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/MemoryBase.h"
          virtual void UpdateStats();


          virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats );


 virtual void DumpAllocatorStats( class FOutputDevice& Ar )
 {
  Ar.Logf( u"Allocator Stats for %s: (not implemented)", GetDescriptiveName() );
 }





 virtual bool IsInternallyThreadSafe() const
 {
  return false;
 }




 virtual bool ValidateHeap()
 {
  return( true );
 }






 virtual const TCHAR* GetDescriptiveName()
 {
  return u"Unspecified allocator";
 }




 virtual void OnMallocInitialized() {}




 virtual void OnPreFork() {}




 virtual void OnPostFork() {}




 virtual uint64 GetImmediatelyFreeableCachedMemorySize() const
 {
  return 0;
 }






 virtual uint64 GetTotalFreeCachedMemorySize() const
 {
  return 0;
 }

protected:
 friend struct FCurrentFrameCalls;






};
# 2 "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/parent-postfetch/header-layout-probe.cpp" 2
static_assert(sizeof(void*) == 8);
FExec::~FExec() {}
bool FExec::Exec(UWorld*, const TCHAR*, FOutputDevice&) { return false; }

bool FExec::Exec(UWorld*, const TCHAR*) { return false; }

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
