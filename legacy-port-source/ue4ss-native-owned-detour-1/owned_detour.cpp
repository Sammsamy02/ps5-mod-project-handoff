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
class PrefixRefusalDetour final : public PLH::x64Detour {
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
static bool owned_snapshot(uint64_t source,void* dest,size_t size) {
 PLH::OwnedMemory::ScopedRegion destination(dest,size,PLH::W);
 size_t count=0;PLH::MemAccessor accessor;
 return accessor.safe_mem_read(source,reinterpret_cast<uintptr_t>(dest),size,count)&&count==size;
}
int main() {
 const char* phase="start";
 try {
  phase="trampoline_pair";const auto extra=PLH::OwnedMemory::allocate_trampoline(128);
  if(!extra||!PLH::OwnedMemory::query(extra,128))return failure(phase,10);
  if(!PLH::OwnedMemory::release_trampoline(extra)||PLH::OwnedMemory::query(extra,1)||PLH::OwnedMemory::release_trampoline(extra))return failure(phase,11);
  asmjit::JitRuntime runtime;Function source=nullptr,replacement=nullptr;size_t source_size=0,replacement_size=0;
  phase="emit_source";auto error=emit(runtime,&source,42,&source_size);if(error||!source)return failure(phase,12);
  phase="emit_replacement";error=emit(runtime,&replacement,84,&replacement_size);if(error||!replacement)return failure(phase,13);
  const auto original=reinterpret_cast<uintptr_t>(source);const auto callback=reinterpret_cast<uintptr_t>(replacement);
  phase="owned_exec_preconditions";if(!executable(original,source_size)||!executable(callback,replacement_size))return failure(phase,14);
  phase="source42";const int before=source();if(before!=42)return failure(phase,15);
  uint64_t trampoline=0,saved_trampoline=0;
  phase="partial_refusal_snapshot";uint8_t beforeBytes[100],afterBytes[100];
  if(!owned_snapshot(original,beforeBytes,sizeof(beforeBytes)))return failure(phase,24);
  {
   PrefixRefusalDetour refused(original,callback,&trampoline);
   refused.setDetourScheme(PLH::x64Detour::INPLACE);
   phase="partial_prefix_write_refusal";
   if(refused.hook()||!refused.refused||trampoline||
      !refused.refusedTrampoline||PLH::OwnedMemory::query(refused.refusedTrampoline,1))return failure(phase,25);
   if(!owned_snapshot(original,afterBytes,sizeof(afterBytes))||
      std::memcmp(beforeBytes,afterBytes,sizeof(beforeBytes))||source()!=42)return failure(phase,26);
  }
  std::puts("PASS genuine owned prefix-write refusal restores bytes+42 and revokes trampoline");
  {
   PLH::x64Detour detour(original,callback,&trampoline);
   // Explicit in-place only: no bounded allocation, code caves, modules or remote memory.
   detour.setDetourScheme(PLH::x64Detour::INPLACE);
   phase="hook";if(!detour.hook())return failure(phase,16);
   saved_trampoline=trampoline;phase="owned_trampoline_preconditions";
   if(!trampoline||!executable(trampoline,1))return failure(phase,17);
   phase="hooked84";const int hooked=source();if(hooked!=84)return failure(phase,18);
   phase="trampoline42";const int through=reinterpret_cast<Function>(trampoline)();if(through!=42)return failure(phase,19);
   phase="unhook";if(!detour.unHook()||trampoline||PLH::OwnedMemory::query(saved_trampoline,1))return failure(phase,20);
   phase="restored42";const int restored=source();if(restored!=42)return failure(phase,21);
   phase="second_unhook_refuses";if(detour.unHook())return failure(phase,22);
  }
  phase="release_generated";if(runtime.release(source)||runtime.release(replacement))return failure(phase,23);
  std::printf("PASS genuine owned x64Detour source42 hooked84 trampoline42 restored42 source_bytes=%zu replacement_bytes=%zu scheme=INPLACE\n",source_size,replacement_size);return 0;
 } catch(const std::exception& e) {std::printf("FAIL phase=%s exception=%s\n",phase,e.what());return 30;}
 catch(...) {return failure(phase,31);}
}
