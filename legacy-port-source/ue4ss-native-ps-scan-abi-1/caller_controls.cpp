#include "PsMappedImageCaller.hpp"
#include <cassert>
#include <cstdio>
#include <array>
#include <functional>
using namespace RC;using namespace RC::Unreal::Platform;
static bool refuses(const std::function<void()>& f){try{f();return false;}catch(const std::invalid_argument&){return true;}}
int main(){
 auto bytes=std::make_shared<std::array<uint8_t,64>>();
 ModuleInput input{bytes,true,{}, {}};
 input.images.emplace(ScanTarget::MainExe,ModuleImage{bytes->data(),bytes->size(),{{bytes->data(),32},{bytes->data()+40,24}}});
 auto snapshot=ModuleSnapshot::Create(std::move(input));
 RC::Platform::NativeProgramContext context;context.module_source=snapshot;
 auto p=[&](size_t n){return reinterpret_cast<uintptr_t>(bytes->data()+n);};
 auto split=std::vector<PsMappedSegment>{{bytes->data(),16,p(0),PS_MAPPED_READ|PS_MAPPED_EXECUTE,0},{bytes->data()+16,16,p(16),PS_MAPPED_READ,0},{bytes->data()+40,24,p(40),PS_MAPPED_READ,0}};
 unsigned checks=0;
 {PsMappedImageCaller call(context,split,PS_MAPPED_IMAGE_ELF64,p(0),0,{p(16),16},{p(40),24});assert(call.view().segment_count==3);assert(call.source_owner()==snapshot);++checks;}
 auto bad=split;bad[0].runtime_address++;assert(refuses([&]{PsMappedImageCaller call(context,bad,1,p(0),0);}));++checks;
 bad=split;bad[0].size=17;assert(refuses([&]{PsMappedImageCaller call(context,bad,1,p(0),0);}));++checks;
 bad=split;bad[1].size=15;assert(refuses([&]{PsMappedImageCaller call(context,bad,1,p(0),0);}));++checks;
 bad=split;bad.push_back({bytes->data()+32,8,p(32),1,0});assert(refuses([&]{PsMappedImageCaller call(context,bad,1,p(0),0);}));++checks;
 bad=split;bad[1].permissions=PS_MAPPED_WRITE;assert(refuses([&]{PsMappedImageCaller call(context,bad,1,p(0),0);}));++checks;
 bad=split;bad[1].reserved=1;assert(refuses([&]{PsMappedImageCaller call(context,bad,1,p(0),0);}));++checks;
 assert(refuses([&]{PsMappedImageCaller call(context,split,1,p(0),0,{p(24),24});}));++checks;
 assert(refuses([&]{PsMappedImageCaller call(context,split,1,p(0),0,{0,1});}));++checks;
 assert(refuses([&]{PsMappedImageCaller call(context,split,1,p(0),p(0));}));++checks;
 assert(refuses([&]{PsMappedImageCaller call(context,split,2,p(0),p(0),{p(16),1});}));++checks;
 auto noowner=context;noowner.module_source.reset();assert(refuses([&]{PsMappedImageCaller call(noowner,split,1,p(0),0);}));++checks;
 assert(refuses([&]{PsMappedImageCaller call(context,split,99,p(0),0);}));++checks;
 std::weak_ptr<const ModuleSnapshot> weak=snapshot;
 {PsMappedImageCaller call(context,split,1,p(0),0);snapshot.reset();context.module_source.reset();assert(!weak.expired());++checks;}
 assert(weak.expired());++checks;
 std::printf("PASS %u actual ModuleSnapshot/NativeProgramContext owned caller geometry+lifetime controls; no permission authority claim\n",checks);
}
