// test_detour_refusal_r2.cpp — worker-l11-loadmap-guard r2, 2026-10-05.
// Host execution of the REAL COMPOSED detour-arm layer: x64Detour.cpp 5bb9cea4
// (the exact unit in the launch-10 walkguard and every launch-10/11 lineage
// receipt) plus the read-lease OwnedMemory d824528d, under the
// PLH_OWNED_MEMORY_HOST_TEST mode both declare. These are the sources the
// LuaMod-side LoadMap arm actually routes through
// (Hooks.cpp:211 -> DetourInstance AddCallback -> EnsureHooked -> PLH hook()).
//
// CORRECTS the r1 draft (test_detour_refusal.cpp): its header claimed PLH's
// 100-byte disassembly is a naked exec-text read (igf6 class). In the composed
// units the read goes through the owned-memory registry
// (ZydisDisassembler.cpp:65 -> MemAccessor::safe_mem_read -> OwnedMemory::copy)
// and soft-refuses non-owned targets with zero dereference and zero syscalls.
//
// Fixture fact (measured): the pinned host-deps build replaces asmjit's
// virtmem with the owned-allocator port (build.py virtmem.cpp swap ->
// AllocatorBridge::record_created at virtmem.cpp:910), so asmjit JIT pages on
// this host ARE registry-backed (page-granted, RWX) — the only genuine host
// analog of "live text that the owned layer can see". True unregistered text
// is simulated with raw mmaps in Phase 0.
//
// Phase 0a "exec-only denied text": a kernel-denied PROT_NONE page — hook()
//   soft-refuses with ZERO dereference (survival + ASan prove it).
// Phase 0b "readable-but-unregistered text" (the live game-text analog —
//   readable in a W-works world yet never registered): raw mmap R|X page →
//   hook() refuses at the empty-insts check (x64Detour.cpp:355-358) BEFORE any
//   protection or write; the refusal is registry-based, not readability-based;
//   zero bytes written; the "game" keeps running.
// Phase 1 "owned readable+writeable world": plain x64Detour on the registry-
//   backed JIT page — the arm PROCEEDS END-TO-END: hook() true, the REAL patch
//   lands (source() returns the replacement value), the owned trampoline
//   executes the ORIGINAL bytes, unHook restores source and releases.
// Phase 2 "write-refused world (R3)": a labeled refusal fixture denies the RWX
//   source transition -> hook() false at the isGood gate (x64Detour.cpp:492),
//   zero source bytes written, the "game" keeps running, process survives.
// Phase 3 "copy-refused after genuine prefix": mem_copy refused after a real
//   7-byte prefix write -> internal rollback (x64Detour.cpp:500-522), source
//   restored byte-identical, trampoline released, process survives.
// Refusal injection pattern: worker-owned-default-detour default_controls.cpp
// (RefusalDetour) scaffold; shipped sources untouched.
#include <asmjit/asmjit.h>
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include <polyhook2/OwnedTrampoline.hpp>
#include <sys/mman.h>
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cstdint>
using Function=int(*)();
static_assert(sizeof(void*)==8);
static_assert(sizeof(uint64_t)==8);
static int failure(const char* phase,int code) {std::printf("FAIL phase=%s code=%d\n",phase,code);return code;}
static bool executable(uint64_t address,size_t bytes) {
 auto info=PLH::OwnedMemory::query(address,bytes);
 return info && info->protectable && (info->protection&PLH::R) && (info->protection&PLH::X);
}
static asmjit::Error emit(asmjit::JitRuntime& runtime,Function* out,int value,size_t* size) {
 asmjit::CodeHolder code;auto error=code.init(runtime.environment());if(error)return error;
 asmjit::x86::Assembler a(&code);
 for(int i=0;i<32;++i)if((error=a.nop()))return error;
 if((error=a.mov(asmjit::x86::eax,value)))return error;
 if((error=a.ret()))return error;
 // Own mapped decoder span >=100 bytes; pads are emitted, never module bytes.
 for(int i=0;i<128;++i)if((error=a.nop()))return error;
 *size=code.codeSize();return runtime.add(out,&code);
}
static bool owned_snapshot(uint64_t source,void* dest,size_t size) {
 PLH::OwnedMemory::ScopedRegion destination(dest,size,PLH::W);
 size_t count=0;PLH::MemAccessor accessor;
 return accessor.safe_mem_read(source,reinterpret_cast<uintptr_t>(dest),size,count)&&count==size;
}
// Refuses the SOURCE protection transition only (dest==m_fnAddress): the exact
// W_FAILS_R_WORKS write-refusal shape. Reads stay granted.
class SourceProtRefusalDetour final : public PLH::x64Detour {
public:
 using PLH::x64Detour::x64Detour;
 mutable bool refused=false;
 PLH::ProtFlag mem_protect(uint64_t dest,uint64_t size,PLH::ProtFlag flags,bool& status) const override {
  if(dest==m_fnAddress && (flags&PLH::W)) {
   refused=true; status=false; return PLH::UNSET;   // R3: exec-text RWX transition DENIED
  }
  return PLH::MemAccessor::mem_protect(dest,size,flags,status);
 }
};
// Refuses the SOURCE patch copy after a genuine 7-byte prefix write (the
// post-protection copy failure shape; exercises the rollback path).
class SourceCopyRefusalDetour final : public PLH::x64Detour {
public:
 using PLH::x64Detour::x64Detour;
 mutable bool refused=false;
 mutable uint64_t refusedTrampoline=0;
 bool mem_copy(uint64_t dest,uint64_t src,uint64_t bytes) const override {
  if(dest==m_fnAddress && !refused) {
   refused=true; refusedTrampoline=m_trampoline;
   if(!PLH::MemAccessor::mem_copy(dest,src,std::min<uint64_t>(bytes,7)))
    throw std::runtime_error("Failure-control genuine prefix copy refused");
   return false; // Explicit test-only refusal AFTER a real prefix write.
  }
  return PLH::MemAccessor::mem_copy(dest,src,bytes);
 }
};
#define VERIFY(c,n) do{if(!(c))return failure(phase,n);}while(false)
int main() {
 const char*phase="start";uint64_t original{},callback{};
 try {
  {
   asmjit::JitRuntime runtime;Function source{},replacement{};size_t source_size{},replacement_size{};
   phase="emit_source";VERIFY(!emit(runtime,&source,42,&source_size)&&source,10);
   phase="emit_replacement";VERIFY(!emit(runtime,&replacement,84,&replacement_size)&&replacement,11);
   original=reinterpret_cast<uintptr_t>(source);callback=reinterpret_cast<uintptr_t>(replacement);
   phase="fixture_fact";                        // the owned-virtmem port registered the JIT pages
   auto reg=PLH::OwnedMemory::query(original,1);
   VERIFY(reg && reg->protectable && (reg->protection&PLH::R),39);
   uint8_t before[100],after[100];VERIFY(owned_snapshot(original,before,100),13);VERIFY(source()==42,14);
   uint64_t trampoline{},saved_trampoline{};
   {
    // ---- Phase 0a: kernel-denied exec-only-text simulation (PROT_NONE, unregistered)
    phase="exec_only_denied";
    void* denied=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    VERIFY(denied!=MAP_FAILED,40);
    VERIFY(!PLH::OwnedMemory::query(reinterpret_cast<uint64_t>(denied),16),41);
    {
     PLH::x64Detour detour(reinterpret_cast<uint64_t>(denied),callback,&trampoline);
     detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
     VERIFY(!detour.hook(),42);            // soft refusal at the owned layer
     VERIFY(!detour.isHooked(),43);        // nothing activated
     std::printf("PASS exec-only-denied: hook() refused with zero dereference, process survived\n");
    }
    munmap(denied,4096);
    // ---- Phase 0b: readable-but-unregistered text (the live game-text analog)
    phase="readable_unregistered";
    void* rx=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    VERIFY(rx!=MAP_FAILED,47);
    {
     auto* bytes=static_cast<uint8_t*>(rx);
     std::memset(bytes,0x90,4096);                       // nop sled (decoder window)
     const uint8_t movRet[6]={0xB8,42,0x00,0x00,0x00,0xC3}; // mov eax,42; ret
     std::memcpy(bytes,movRet,sizeof(movRet));
     VERIFY(mprotect(rx,4096,PROT_READ|PROT_EXEC)==0,48);
     Function fakeSource=reinterpret_cast<Function>(rx);
     VERIFY(fakeSource()==42,49);
     VERIFY(!PLH::OwnedMemory::query(reinterpret_cast<uint64_t>(rx),16),50); // NOT registered
     PLH::x64Detour detour(reinterpret_cast<uint64_t>(rx),callback,&trampoline);
     detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
     uint8_t before2[100],after2[100];
     std::memcpy(before2,rx,100);          // direct read: the page IS readable
     VERIFY(!detour.hook(),44);            // registry-based refusal, not readability-based
     VERIFY(!detour.isHooked(),45);
     std::memcpy(after2,rx,100);
     VERIFY(!std::memcmp(before2,after2,100),51); // zero bytes mutated by the refused arm
     VERIFY(fakeSource()==42,52);          // the "game" keeps executing unhooked
     std::printf("PASS readable-unregistered: hook() refused at the owned layer, zero bytes mutated, still running\n");
    }
    munmap(rx,4096);
   }
   {
    // ---- Phase 1: owned readable+writeable world -> the arm PROCEEDS for real
    phase="owned_world_hook";
    PLH::x64Detour detour(original,callback,&trampoline);
    detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
    VERIFY(detour.hook(),16);saved_trampoline=trampoline;
    VERIFY(trampoline&&executable(trampoline,1),17);
    phase="owned_values";
    VERIFY(source()==84,18);                              // the REAL patch landed
    VERIFY(reinterpret_cast<Function>(saved_trampoline)()==42,19); // trampoline runs ORIGINAL bytes
    phase="owned_unhook";VERIFY(detour.unHook()&&!trampoline&&!PLH::OwnedMemory::query(saved_trampoline,1),20);
    VERIFY(source()==42&&owned_snapshot(original,after,100)&&!std::memcmp(before,after,100),21);
    std::puts("PASS owned readable+writeable world: real arm proceeded (hook84/trampoline42/unhook, source restored 42)");
   }
   {
    // ---- Phase 2: write-refused world (R3) -> loud false at x64Detour.cpp:492,
    //      zero source bytes written, "game" still runs, process survives.
    phase="write_refused_hook";SourceProtRefusalDetour detour(original,callback,&trampoline);
    detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
    const bool hooked=detour.hook();
    VERIFY(!hooked,22);                       // arm refused
    VERIFY(detour.refused,29);                // at the protection step, not earlier
    VERIFY(!detour.isHooked(),23);            // nothing activated
    VERIFY(source()==42,24);                  // the "game" (LoadMap) keeps executing
    VERIFY(owned_snapshot(original,after,100)&&!std::memcmp(before,after,100),25); // zero bytes written
    std::puts("PASS write-refused world: real arm refused (isGood=false, x64Detour.cpp:492), source42 intact, process survived");
   }
   {
    // ---- Phase 3: copy-refused after genuine prefix -> internal rollback,
    //      hook() false, bytes restored, source still runs, process survives.
    phase="copy_refused_hook";SourceCopyRefusalDetour detour(original,callback,&trampoline);
    detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
    VERIFY(!detour.hook()&&detour.refused&&detour.refusedTrampoline,26);
    VERIFY(source()==42&&owned_snapshot(original,after,100)&&!std::memcmp(before,after,100),27);
    const uint64_t rt=detour.refusedTrampoline;
    VERIFY(!PLH::OwnedMemory::query(rt,1),28); // rollback retained-then-released resources
    std::puts("PASS copy-refused world: real arm rolled back after genuine prefix write, source42 intact, process survived");
   }
   phase="release_generated";VERIFY(!runtime.release(source)&&!runtime.release(replacement),34);
  }
  phase="runtime_mapping_revoked";VERIFY(!PLH::OwnedMemory::query(original,1)&&!PLH::OwnedMemory::query(callback,1),35);
  std::puts("PASS loadmap-guard r2 detour-layer host test: 5/5 phases");
  return 0;
 } catch (const std::exception& e) {
  std::printf("FAIL phase=%s exception=%s\n",phase,e.what());return 90;
 } catch (...) {
  std::printf("FAIL phase=%s exception=unknown\n",phase);return 91;
 }
}
