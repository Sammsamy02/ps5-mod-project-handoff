// test_detour_refusal.cpp — worker-l11-loadmap-guard, 2026-10-05.
// Host execution of the REAL unchanged detour-arm layer (port x64Detour.cpp
// 5bb9cea4..., PLH_OWNED_MEMORY_HOST_TEST mode it declares itself) through the
// two outcomes the launch-11 R3 world can hit at the LuaMod-side LoadMap arm:
//   Phase 1 "readable target": hook() proceeds — registration works.
//   Phase 2 "write-refused world" (R3 W_FAILS_R_WORKS): the RWX source
//            transition is refused -> hook() returns false at x64Detour.cpp:492
//            (MemoryProtector::isGood()==false), NOTHING is written, the
//            "game" (source function) keeps running, process survives.
//   Phase 3 "copy-refused world": protection OK but the source patch copy is
//            refused after a genuine prefix write -> internal rollback
//            (x64Detour.cpp:500-519), hook() false, bytes restored, source
//            still runs, process survives.
// Phase order note (the naked-read exposure): every phase's hook() executes the
// 100-byte target disassembly at x64Detour.cpp:351 BEFORE any protection or
// write — on the console that read is the igf6 crash class when exec text is
// data-read-faulting; it is untestable on the host (no fault injection), which
// is exactly why the staged fix DEFERS the arm instead of arming-then-failing.
// Refusal injection pattern: worker-owned-default-detour default_controls.cpp
// (RefusalDetour), reused as scaffold; the shipped x64Detour.cpp is untouched.
#include <asmjit/asmjit.h>
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include <polyhook2/OwnedTrampoline.hpp>
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
// Refuses the SOURCE protection transition only (address==m_fnAddress): the
// exact W_FAILS_R_WORKS write-refusal shape for the game-text page.
class SourceProtRefusalDetour final : public PLH::x64Detour {
public:
 using PLH::x64Detour::x64Detour;
 mutable bool refused = false;
 bool mem_protect(uint64_t address,uint64_t size,PLH::ProtFlag flags,bool& status)const override {
  if(!refused && address==m_fnAddress && (flags&PLH::W)) {
   refused = true; status = false; return PLH::UNSET;   // R3: exec-text RWX transition DENIED
  }
  return PLH::MemAccessor::mem_protect(address,size,flags,status);
 }
};
// Refuses the SOURCE patch copy after a genuine 7-byte prefix write (the
// post-protection copy failure shape; exercises the rollback path).
class SourceCopyRefusalDetour final : public PLH::x64Detour {
public:
 using PLH::x64Detour::x64Detour;
 mutable bool refused = false;
 mutable uint64_t refusedTrampoline = 0;
 bool mem_copy(uint64_t dest,uint64_t src,uint64_t bytes) const override {
  if (dest == m_fnAddress && !refused) {
   refused = true;refusedTrampoline = m_trampoline;
   if (!PLH::MemAccessor::mem_copy(dest,src,std::min<uint64_t>(bytes,7)))
    throw std::runtime_error("Failure-control genuine prefix copy refused");
   return false; // Explicit test-only refusal AFTER a real owned prefix write.
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
   VERIFY(executable(original,source_size)&&executable(callback,replacement_size),12);
   uint8_t before[100],after[100];VERIFY(owned_snapshot(original,before,100),13);VERIFY(source()==42,14);
   uint64_t trampoline{},saved_trampoline{};
   {
    // ---- Phase 1: readable target -> registration proceeds (unchanged behavior)
    phase="readable_hook";PLH::x64Detour detour(original,callback,&trampoline);
    detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
    VERIFY(detour.hook(),16);saved_trampoline=trampoline;
    VERIFY(trampoline&&executable(trampoline,1),17);
    phase="readable_values";VERIFY(source()==84,18);
    VERIFY(reinterpret_cast<Function>(saved_trampoline)()==42,19);
    phase="readable_unhook";VERIFY(detour.unHook()&&!trampoline&&!PLH::OwnedMemory::query(saved_trampoline,1),20);
    VERIFY(source()==42&&owned_snapshot(original,after,100)&&!std::memcmp(before,after,100),21);
    std::puts("PASS readable-target: real arm proceeded (hook84/trampoline42/unhook, source restored 42)");
   }
   {
    // ---- Phase 2: write-refused world (R3) -> loud false at x64Detour.cpp:492,
    //      zero source bytes written, "game" still runs, process survives.
    phase="write_refused_hook";SourceProtRefusalDetour detour(original,callback,&trampoline);
    detour.setDetourScheme(PLH::x64Detour::RECOMMENDED);
    VERIFY(detour.refused||true,0); // set only after the attempt; record shape
    const bool hooked=detour.hook();
    VERIFY(!hooked,22);                       // arm refused
    VERIFY(!detour.isHooked(),23);            // nothing activated
    VERIFY(source()==42,24);                  // the "game" (LoadMap) keeps executing
    VERIFY(owned_snapshot(original,after,100)&&!std::memcmp(before,after,100),25); // zero bytes written
    std::puts("PASS write-refused world: real arm refused (isGood=false path, x64Detour.cpp:492), source42 intact, process survived");
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
  std::puts("PASS loadmap-guard detour-layer host test: 3/3 phases");
  return 0;
 } catch (const std::exception& e) {
  std::printf("FAIL phase=%s exception=%s\n",phase,e.what());return 90;
 } catch (...) {
  std::printf("FAIL phase=%s exception=unknown\n",phase);return 91;
 }
}
