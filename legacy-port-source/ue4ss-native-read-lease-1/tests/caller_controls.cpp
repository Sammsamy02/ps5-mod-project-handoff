#include <OwnedMappedImageCaller.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <cassert>
#include <cstdio>
using namespace RC::Unreal::Platform;
static auto context(std::shared_ptr<const void> owner,const uint8_t* base,size_t size) {
 ModuleInput input{};input.lifetime=std::move(owner);input.monolithic=true;
 input.images.emplace(RC::ScanTarget::MainExe,ModuleImage{base,size,{{base,size}}});
 RC::Platform::NativeProgramContext result{};result.module_source=ModuleSnapshot::Create(std::move(input));return result;
}
template<class F>static bool refuses(F f) {try{f();return false;}catch(const std::invalid_argument&){return true;}}
int main() {
 auto first=std::make_shared<PLH::OwnedMemory::OwnedMapping>(1);const auto page=first->size();
 auto memory=std::make_shared<PLH::OwnedMemory::OwnedMapping>(page*2);auto* data=static_cast<uint8_t*>(memory->data());const auto base=reinterpret_cast<uintptr_t>(data);
 auto source=context(memory,data,memory->size());PLH::MemAccessor accessor;bool ok=false;
 assert(refuses([&]{OwnedMappedImageCaller image(source,PS_MAPPED_IMAGE_ELF64,base);}));
 accessor.mem_protect(base,page,PLH::R|PLH::X,ok);assert(ok);
 {
  OwnedMappedImageCaller image(source,PS_MAPPED_IMAGE_ELF64,base);
  assert(image.view().segment_count==2);
  assert(image.view().segments[0].permissions==(PS_MAPPED_READ|PS_MAPPED_EXECUTE));
  assert(image.view().segments[1].permissions==(PS_MAPPED_READ|PS_MAPPED_WRITE));
  assert(image.view().segments[0].runtime_address==base && image.view().segments[1].runtime_address==base+page);
  assert(image.permits_code(base+1) && !image.permits_code(base+page));
  assert(image.permits_pointer_slot(base+page) && !image.permits_pointer_slot(base+page+1));
  assert(!image.permits_data(base+page*2,1) && !image.permits_data(0,1));
  assert(image.permits_data(base+page-4,8));
  assert(image.source_owner()==source.module_source);
 }
 assert(refuses([&]{OwnedMappedImageCaller image(source,PS_MAPPED_IMAGE_PE64,base,base+1);}));
 assert(refuses([&]{OwnedMappedImageCaller image(source,PS_MAPPED_IMAGE_ELF64,base,0,{base+memory->size(),8});}));
 {
  auto bytes=std::make_shared<std::vector<uint8_t>>(32);
  auto unregistered=context(bytes,bytes->data(),bytes->size());
  assert(refuses([&]{OwnedMappedImageCaller image(unregistered,PS_MAPPED_IMAGE_ELF64,reinterpret_cast<uintptr_t>(bytes->data()));}));
 }
 std::puts("owned mapped caller: real lease, permission conversion, source coverage and role admission passed");
}
