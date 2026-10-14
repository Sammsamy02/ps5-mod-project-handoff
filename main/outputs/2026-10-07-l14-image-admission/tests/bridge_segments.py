from pathlib import Path
import subprocess,json,hashlib
root=Path(__file__).resolve().parents[1]
src=(root/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text()
publish=src[src.index('static rc_seed::VmSpanRecord g_spanpub_records'):src.index('// Seed-admission span table (class (ii)')]
getter=src[src.index('static uint64_t g_loaded_main_base'):src.index('// Captured-envelope end')]
pre=r'''
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "ImageCoverage.hpp"
#include "ImageLoadProfile.hpp"
#include "SeedValidation.hpp"
constexpr size_t kBridgeVmmapEntries=32768;constexpr uint64_t kCandidateBase=0x400000,kImageExtentVaddr=0xbc602c8;
namespace lmf{
 struct OsVmMapping{uint64_t start,end;int32_t protection;};
 struct Seg{void* address;uint32_t size;int32_t prot;};
 struct LiveModuleFactsSnapshot{void*observed_base;struct{bool obtained;uint32_t segment_count;Seg segments[4];}module_info;};
}
void diagnostic_event(const char*,uint32_t){}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s\\n",#x);std::exit(1);}}while(0)
'''
post=r'''
int main(){
 lmf::OsVmMapping mappings[]={{0x96dc000,0x9a34000,3},{0x22c0000000,0x22c0020000,3}};
 bridge_spanpub_publish(mappings,2,kCandidateBase,kImageExtentVaddr);CHECK(g_published_readable_span_count==1);CHECK(g_published_readable_spans[0].lo==0x22c0000000);
 lmf::LiveModuleFactsSnapshot facts{};facts.observed_base=reinterpret_cast<void*>(kCandidateBase);bridge_capture_loaded_image(facts);
 rc::imagecoverage::Hit hit;CHECK(bridge_image_range(0x9765ec8,4,&hit)==1);CHECK(hit.index==7);CHECK(hit.start==0x953c000);CHECK(hit.end==0x9a33418);
 CHECK(bridge_image_range(0x9a33416,4,&hit)==0);CHECK(bridge_image_range(0x22c0000000,4,&hit)==0);
 facts.observed_base=nullptr;bridge_capture_loaded_image(facts);CHECK(bridge_image_range(0x9765ec8,4,&hit)==0);
 facts.observed_base=reinterpret_cast<void*>(kCandidateBase);facts.module_info.obtained=true;facts.module_info.segment_count=1;facts.module_info.segments[0]={reinterpret_cast<void*>(0x953c000),0x4f7418,3};bridge_capture_loaded_image(facts);CHECK(bridge_image_range(0x9765ec8,4,&hit)==1);CHECK(hit.source==std::string("loaded-module-info"));
 facts.module_info.segments[0].prot=4;bridge_capture_loaded_image(facts);CHECK(bridge_image_range(0x9765ec8,4,&hit)==0);
 std::puts("PASS production bridge publisher/capture/getter: image exclusion preserved; segment proof succeeds");
}
'''
p=root/'tests/bridge_segments_extracted.cpp';p.write_text(pre+'\n#include <string>\n'+publish+getter+post)
b=root/'tests/bridge_segments_extracted';cmd=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-fsanitize=address,undefined','-I'+str(root/'units'),str(p),'-o',str(b)]
r=subprocess.run(cmd,capture_output=True,text=True);records=[{'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr}]
if not r.returncode:
 r=subprocess.run([str(b)],capture_output=True,text=True);records.append({'argv':[str(b)],'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
(root/'receipts/bridge-segments.json').write_text(json.dumps({'source_sha256':hashlib.sha256(src.encode()).hexdigest(),'commands':records},indent=2)+'\n');print(r.stdout,r.stderr);raise SystemExit(r.returncode)
