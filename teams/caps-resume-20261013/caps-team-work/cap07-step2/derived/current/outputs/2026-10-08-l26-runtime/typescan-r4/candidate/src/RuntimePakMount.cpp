#include "RuntimePakMount.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <limits>
namespace ModBridge::PakMount {
bool EnginePins::valid() const {
 if(!pak_vtable||!platform_mgr_get||!platform_mgr_getpf||!slot_get_lower_level||!slot_delegate_execute||!mountpak_delegate||!handle_mount_pak||!delegate_vtable||!execute||!lower_impl||!snapshot_delegate||!snapshot_vtable||!snapshot_execute||pak_count_offset!=0x18)return false;
 for(size_t i=0;i<64;++i)if(!((image_sha256[i]>='0'&&image_sha256[i]<='9')||(image_sha256[i]>='a'&&image_sha256[i]<='f')))return false;
 if(image_sha256[64])return false;
 const uint64_t targets[]={platform_mgr_get,platform_mgr_getpf,execute,handle_mount_pak,snapshot_execute,lower_impl};
 for(size_t i=0;i<6;++i)if(proofs[i].linked!=targets[i]||!proofs[i].size||proofs[i].size>64)return false;
 return true;
}
bool is_pak_layer(uint64_t a,uint64_t b){return a&&a==b;}
bool valid_absolute_path(const std::string& p){
 if(p.size()<2||p.size()>1024||p[0]!='/'||p.back()=='/')return false;
 size_t start=1;
 for(size_t i=1;i<=p.size();++i){
  if(i<p.size()&&(static_cast<unsigned char>(p[i])<32||p[i]=='\\'))return false;
  if(i==p.size()||p[i]=='/'){auto s=p.substr(start,i-start);if(s.empty()||s=="."||s=="..")return false;start=i+1;}
 }
 return true;
}
bool choose_order(const std::string& path,const std::vector<PakInfo>& before,int32_t& arg,uint32_t& effective){
 if(before.size()>4096)return false;
 uint64_t highest=0;
 for(const auto& p:before){if(!p.identity)return false;highest=std::max(highest,uint64_t(p.order));}
 uint64_t bonus=0;
 if(path.size()>=6&&path.compare(path.size()-6,6,"_P.pak")==0){
  uint64_t version=1;auto end=path.rfind('_');auto start=end?path.rfind('_',end-1):std::string::npos;
  if(start!=std::string::npos){auto part=path.substr(start+1,end-start-1);bool digits=!part.empty();for(char c:part)digits=digits&&(c>='0'&&c<='9');
   if(digits){uint64_t n=0;for(char c:part){n=n*10+unsigned(c-'0');if(n>INT_MAX)return false;}if(n>=1)version=n+1;}
   else if(part.find_first_of("+-.")!=std::string::npos)return false; // ambiguous engine numeric/Atoi spelling
  }
  bonus=100*version;
 }
 if(highest>=INT_MAX||bonus>uint64_t(INT_MAX)-highest-1)return false;
 arg=static_cast<int32_t>(highest+1);effective=static_cast<uint32_t>(highest+1+bonus);return true;
}
static uint64_t find_pak(const EnginePins&p,IEngineBridge&e,uint64_t b){
 uint64_t layer=e.platform_chain_head();uint64_t seen[16]{};
 for(unsigned depth=0;depth<16&&layer;++depth){
  for(unsigned j=0;j<depth;++j)if(seen[j]==layer)return 0;seen[depth]=layer;
  uint64_t vt=0;if(!e.read_ptr(layer,vt))return 0;
  if(vt==b+p.pak_vtable)return layer;
  if(vt>UINT64_MAX-p.slot_get_lower_level||layer>UINT64_MAX-8)return 0;
  uint64_t lower_fn=0,next=0;
  if(!e.read_ptr(vt+p.slot_get_lower_level,lower_fn)||lower_fn!=b+p.lower_impl||!e.read_ptr(layer+8,next))return 0;
  layer=next;
 }
 return 0;
}
MountOutcome mount(const EnginePins&p,const MountRequest&r,IEngineBridge&e){
 MountOutcome o;
 if(!p.valid()){o.verdict=Verdict::RefusedPins;o.why="profile incomplete";return o;}
 if(!valid_absolute_path(r.pak_path_abs)){o.verdict=Verdict::RefusedBadRequest;o.why="invalid absolute path";return o;}
 if(!e.is_game_thread()){o.verdict=Verdict::RefusedNotGameThread;o.why="not game thread";return o;}
 auto b=e.module_view_base();
 if(!b||b>UINT64_MAX-0xbc602c8||e.image_sha256()!=p.image_sha256){o.verdict=Verdict::RefusedPins;o.why="image identity/base mismatch";return o;}
 for(const auto& f:p.proofs)if(!e.verify_code(b+f.linked,f.bytes,f.size)){o.verdict=Verdict::RefusedPins;o.why="authenticated function prefix unavailable";return o;}
 o.pak_layer=find_pak(p,e,b);
 if(!o.pak_layer){o.verdict=Verdict::RefusedNoPakLayer;o.why="pak layer/chain semantics unavailable";return o;}
 uint64_t inst=0,vt=0,fn=0,receiver=0,target=0,adjust=1;uint32_t size=0;
 if(!e.read_ptr(b+p.mountpak_delegate+p.delegate_inst_off,inst)||!e.read_u32(b+p.mountpak_delegate+p.delegate_size_off,size)){o.verdict=Verdict::RefusedPins;o.why="delegate unreadable";return o;}
 if(!inst||!size){o.verdict=Verdict::RefusedUnbound;o.why="delegate unbound";return o;}
 if(size>64||inst>UINT64_MAX-0x30||!e.read_ptr(inst,vt)||vt!=b+p.delegate_vtable||!e.read_ptr(vt+p.slot_delegate_execute,fn)||fn!=b+p.execute||!e.read_ptr(inst+0x18,receiver)||receiver!=o.pak_layer||!e.read_ptr(inst+0x20,target)||target!=b+p.handle_mount_pak||!e.read_ptr(inst+0x28,adjust)||adjust!=0){o.verdict=Verdict::RefusedPins;o.why="delegate instance/target identity mismatch";return o;}
 std::vector<PakInfo> before;
 if(!e.snapshot_paks(o.pak_layer,before)||before.size()>4096){o.verdict=Verdict::RefusedSnapshot;o.why="synchronized snapshot unavailable";return o;}
 o.count_before=static_cast<uint32_t>(before.size());
 int32_t order=0;uint32_t effective=0;
 if(!choose_order(r.pak_path_abs,before,order,effective)){o.verdict=Verdict::RefusedPriority;o.why="priority/patch bonus overflow or unknown spelling";return o;}
 o.order_argument=order;
 void* f=e.make_fstring(r.pak_path_abs);
 if(!f){o.verdict=Verdict::RefusedBadRequest;o.why="FString allocation/conversion refused";return o;}
 struct StringOwner{IEngineBridge& e;void* f;~StringOwner() noexcept{try{e.free_fstring(f);}catch(...){}}} owner{e,f};
 o.call_made=true;
 try{o.returned_pak=e.call_execute(fn,inst,f,order);}catch(...){o.verdict=Verdict::MountedUnverified;o.why="engine call threw; side effect unknown";return o;}
 std::vector<PakInfo> after;
 o.verdict=Verdict::MountedUnverified;o.why="engine call completed; mount/precedence unverified";
 if(!e.snapshot_paks(o.pak_layer,after)||after.size()>4096)return o;
 o.count_after=static_cast<uint32_t>(after.size());
 size_t hits=0;bool outranks=true;
 for(const auto& x:after){if(x.identity==o.returned_pak){++hits;if(x.order!=effective)outranks=false;}else if(x.order>=effective)outranks=false;}
 if(o.returned_pak&&hits==1&&outranks&&after.size()>before.size()){o.verdict=Verdict::Mounted;o.why="returned identity present; highest at post-mount snapshot";}
 char line[256];std::snprintf(line,sizeof line,"[PAKMOUNT] call=%u ret=0x%llx before=%u after=%u order=%d verdict=%u",unsigned(o.call_made),static_cast<unsigned long long>(o.returned_pak),o.count_before,o.count_after,order,unsigned(o.verdict));e.log(line);
 return o;
}
}
