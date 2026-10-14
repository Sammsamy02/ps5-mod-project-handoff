#include "modbridge/core.hpp"
#include "modbridge/dds.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <limits>
#include <memory>
namespace modbridge  {
  namespace  {
    struct Reader  {
      std::span<const std::uint8_t> b;
      bool le=true;
      std::shared_ptr<std::uint64_t> budget=std::make_shared<std::uint64_t>(100000);
      void need(std::uint64_t p,std::uint64_t n)const {
        if(p>b.size()||n>b.size()-p)throw std::runtime_error("structure outside inspected bytes");
      }
      void spend()const {
        if(!*budget)throw std::runtime_error("binary parser work budget exceeded");
        --*budget;
      }
      std::uint64_t u(std::uint64_t p,unsigned n)const {
        spend();
        need(p,n);
        std::uint64_t v=0;
        for(unsigned i=0;i<n;i++)v|=std::uint64_t(b[p+i])<<(8*(le?i:n-1-i));
        return v;
      }
      std::string str(std::uint64_t p)const {
        std::string s;
        for(unsigned i=0;i<4096;i++) {
          auto ch=u(p+i,1);
          if(!ch)return s;
          s+=char(ch);
        }
        throw std::runtime_error("unterminated or oversized string");
      }
    };
    void requirement(Json&j,const std::string&id,const std::string&path,const std::string&discovery,const std::string&context="game_process") {
      j["requirements"].push_back({{"id",id},{"consumer",path},{"discovery",discovery},{"status","unresolved"},{"context",context}});
    }
    struct BethesdaHeaderError:std::runtime_error {
      const char* code;
      BethesdaHeaderError(const char* c,const char* detail):std::runtime_error(detail),code(c) {}
    };
    [[noreturn]] void bethesda_error(const char* code,const char* detail) {
      throw BethesdaHeaderError(code,detail);
    }
    // Mutagen 417e22f6c502f8e3b37d46d4a933e60d18c644bb GameConstants.cs:
    // 20/24-byte roots, six-byte subrecords. xEdit 9fb016884bec138ea6c7b872cec831537d464c3e:
    // HEDR(float bits,u32,u32), forward-string MAST immediately followed by DATA(8).
    // These are structural candidates; layout and flags do not identify a game/platform.
    Json bethesda_candidate(Reader r,unsigned header,std::uint64_t body,std::uint64_t total) {
      if(total<header||body>total-header)
        bethesda_error("bethesda_header_invalid","declared root span exceeds total size");
      if(r.b.size()<header||body>r.b.size()-header)
        bethesda_error("bethesda_header_incomplete","declared root body is not fully captured");
      const auto end=header+body;
      std::uint64_t p=header,name_bytes=0;
      unsigned fields=0;
      bool pending_master=false;
      Json masters=Json::array(),pending,hedr;
      constexpr std::uint64_t HEDR=0x52444548,MAST=0x5453414d,DATA=0x41544144,XXXX=0x58585858;
      while(p<end) {
        if(++fields>1024)
          bethesda_error("bethesda_header_limit_exceeded","root subrecord count exceeds 1024");
        if(end-p<6)
          bethesda_error("bethesda_header_invalid","short root subrecord header");
        const auto tag=r.u(p,4),length=r.u(p+4,2);
        p+=6;
        if(length>end-p)
          bethesda_error("bethesda_header_invalid","root subrecord exceeds declared body");
        if(tag==XXXX)
          bethesda_error("bethesda_header_unsupported","XXXX extended root subrecords are not supported");
        if(pending_master&&tag!=DATA)
          bethesda_error("bethesda_header_invalid","MAST must be immediately followed by DATA");
        if(tag==HEDR) {
          if(fields!=1||length!=12)
            bethesda_error("bethesda_header_invalid","first and only HEDR must contain 12 bytes");
          hedr={{"version_bits",r.u(p,4)},{"declared_record_count",r.u(p+4,4)},{"next_object_id",r.u(p+8,4)}};
        }
        else if(fields==1)
          bethesda_error("bethesda_header_invalid","first root subrecord must be HEDR");
        else if(tag==MAST) {
          if(masters.size()>=256)
            bethesda_error("bethesda_header_limit_exceeded","master count exceeds 256");
          std::string hex,ascii;
          bool terminated=false,printable=true;
          static constexpr char digits[]="0123456789abcdef";
          for(std::uint64_t i=0;i<length;++i) {
            const auto ch=r.u(p+i,1);
            if(!ch){terminated=true;break;}
            if(name_bytes==4096)
              bethesda_error("bethesda_header_limit_exceeded","aggregate master name bytes exceed 4096");
            ++name_bytes;
            hex+=digits[ch>>4];hex+=digits[ch&15];
            if(ch<0x20||ch>0x7e)printable=false;
            if(printable)ascii+=static_cast<char>(ch);
          }
          if(!terminated||hex.empty())
            bethesda_error("bethesda_header_invalid","master name must be nonempty and NUL-terminated inside MAST");
          pending={{"name_bytes_hex",std::move(hex)}};
          if(printable)pending["name_ascii"]=std::move(ascii);
          pending_master=true;
        }
        else if(tag==DATA) {
          if(!pending_master||length!=8)
            bethesda_error("bethesda_header_invalid","master DATA requires a preceding MAST and eight bytes");
          pending["opaque_data_bits"]=r.u(p,8);
          masters.push_back(std::move(pending));pending_master=false;
        }
        // Unknown subrecords are bounded opaque fields, including bytes after a
        // MAST string terminator. No record payloads or localization files are read.
        p+=length;
      }
      if(!fields||pending_master)
        bethesda_error("bethesda_header_invalid","missing HEDR or incomplete master pair");
      Json result={{"record_header_bytes",header},{"declared_body_bytes",body},{"root_end",end},
        {"declared_flags",r.u(8,4)},{"form_id_bits",r.u(12,4)},{"revision_bits",r.u(16,4)},
        {"hedr",std::move(hedr)},{"masters",std::move(masters)}};
      if(header==24){result["form_version_bits"]=r.u(20,2);result["reserved_bits"]=r.u(22,2);}
      return result;
    }
    void bethesda(Reader r,Json&j,const std::string&path,std::uint64_t total) {
      j["kind"]="bethesda_plugin";j["facts"]["classification_confidence"]="magic_only";
      r.need(0,20);
      const auto body=r.u(4,4);
      if(body>65536)
        bethesda_error("bethesda_header_limit_exceeded","declared root body exceeds 64 KiB");
      // xEdit-maintained fopdoc Records.html: raw bit 0x00040000 is compression.
      // This operation never decompresses a root body.
      if(r.u(8,4)&0x00040000)
        bethesda_error("bethesda_header_unsupported","compressed root bodies are not supported");
      std::optional<Json> accepted;
      std::optional<BethesdaHeaderError> failure;
      for(const unsigned header:{20u,24u}) {
        if(r.b.size()<header+6||r.u(header,4)!=0x52444548||r.u(header+4,2)!=12)continue;
        std::optional<Json> candidate;
        try {candidate=bethesda_candidate(r,header,body,total);}
        catch(const BethesdaHeaderError& e){if(!failure)failure=e;continue;}
        if(accepted)
          bethesda_error("bethesda_header_unsupported","root layout is ambiguous between 20 and 24 bytes");
        accepted=std::move(candidate);
      }
      if(!accepted) {
        if(failure)throw *failure;
        if(r.b.size()<30)
          bethesda_error("bethesda_header_incomplete","insufficient captured bytes to select a root layout");
        bethesda_error("bethesda_header_unsupported","no verified root layout begins with HEDR(12)");
      }
      // Construct all owning facts/requirements before publishing any positive
      // metadata. Late parse or allocation failures leave only the magic claim.
      Json complete=j;
      complete["facts"]["tes4_header"]=std::move(*accepted);
      complete["facts"]["classification_confidence"]="structural_header";
      complete["facts"]["header_only"]=true;
      requirement(complete,"translation.bethesda_plugin",path,"root header only; platform conversion is unverified","preparation");
      requirement(complete,"dependencies.bethesda_masters",path,"declared master names require independent resolution","preparation");
      requirement(complete,"format.bethesda_plugin_records",path,"remaining records, scripts, resources and runtime acceptance are uninspected","preparation");
      j=std::move(complete);
    }
    void pe(Reader r,Json&j,const std::string&path) {
      auto&f=j["facts"];
      j["kind"]="native_pe";
      f["classification_confidence"]="structural";
      const auto p=r.u(0x3c,4);
      r.need(p,24);
      if(r.u(p,4)!=0x4550)throw std::runtime_error("invalid PE signature");
      f["machine"]=r.u(p+4,2);
      auto ns=r.u(p+6,2),os=r.u(p+20,2),o=p+24;
      r.need(o,os);
      auto magic=r.u(o,2);
      if(magic!=0x10b&&magic!=0x20b)throw std::runtime_error("unsupported PE optional header");
      bool wide=magic==0x20b;
      auto fixed=wide?112u:96u;
      if(os<fixed)throw std::runtime_error("short PE optional header");
      auto nd=r.u(o+(wide?108:92),4);
      if(nd>(os-fixed)/8)throw std::runtime_error("PE directory count exceeds optional header");
      auto image=r.u(o+(wide?24:28),wide?8:4),headers=r.u(o+60,4),s=o+os;
      r.need(s,ns*40);
      Json sections=Json::array();
      for(std::uint64_t i=0;i<ns;i++) {
        auto t=s+i*40;
        r.need(r.u(t+20,4),r.u(t+16,4));
        sections.push_back({{"rva",r.u(t+12,4)},{"raw_offset",r.u(t+20,4)},{"raw_size",r.u(t+16,4)},{"virtual_size",r.u(t+8,4)}});
      }
      f["sections"]=sections;
      f["optional_header"]=wide?"PE32+":"PE32";
      auto offset=[&](std::uint64_t a,std::uint64_t n)->std::uint64_t {
        if(a<headers) {
          if(n>headers-a)throw std::runtime_error("RVA crosses headers");
          r.need(a,n);
          return a;
        }
        for(const auto&t:sections) {
          r.spend();
          auto start=t.at("rva").get<std::uint64_t>(),raw=t.at("raw_size").get<std::uint64_t>();
          if(a>=start&&a-start<raw) {
            if(n>raw-(a-start))throw std::runtime_error("RVA crosses raw section");
            auto z=t.at("raw_offset").get<std::uint64_t>()+a-start;
            r.need(z,n);
            return z;
          }
        }
        throw std::runtime_error("unmapped RVA");
      };
      auto dir=[&](unsigned i) {
        return std::pair<std::uint64_t,std::uint64_t> {
          i<nd?r.u(o+fixed+i*8,4):0,i<nd?r.u(o+fixed+i*8+4,4):0
        };
      };
      auto cstr=[&](std::uint64_t a) {
        std::string result;
        for(unsigned i=0;i<4096;i++) {
          auto ch=r.u(offset(a+i,1),1);
          if(!ch)return result;
          result+=char(ch);
        }
        throw std::runtime_error("oversized RVA string");
      };
      f["imports"]=Json::array();
      auto [imp,isz]=dir(1);
      if(imp) {
        if(isz<20)throw std::runtime_error("short import directory");
        bool ended=false;
        for(std::uint64_t i=0;i<isz/20&&i<65536;i++) {
          auto t=offset(imp+i*20,20);
          auto thunk=r.u(t,4),name=r.u(t+12,4),iat=r.u(t+16,4);
          if(!thunk&&!name&&!iat) {
            ended=true;
            break;
          }
          Json lib= {
            {
              "library",cstr(name)
            }, {
              "symbols",Json::array()
            }
          };
          if(!thunk)thunk=iat;
          bool done=false;
          for(unsigned k=0;k<65536;k++) {
            auto v=r.u(offset(thunk+std::uint64_t(k)*(wide?8:4),wide?8:4),wide?8:4);
            if(!v) {
              done=true;
              break;
            }
            auto ord=wide?(std::uint64_t(1)<<63):(std::uint64_t(1)<<31);
            if(v&ord)lib["symbols"].push_back({{"ordinal",v&65535}});
            else {
              offset(v,2);
              lib["symbols"].push_back({{"name",cstr(v+2)}});
            }
          }
          if(!done)throw std::runtime_error("import thunk limit exceeded");
          f["imports"].push_back(lib);
        }
        if(!ended)throw std::runtime_error("unterminated import descriptors");
      }
      auto [ex,esz]=dir(3);
      f["exception_directory"]= {
        {
          "rva",ex
        }, {
          "size",esz
        }
      };
      if(ex) {
        offset(ex,esz);
        if(f["machine"]==0x8664) {
          if(esz%12)throw std::runtime_error("invalid AMD64 exception table size");
          f["exception_entries"]=esz/12;
          f["exception_functions"]=Json::array();
          for(std::uint64_t i=0;i<esz/12;i++) {
            auto t=offset(ex+i*12,12),begin=r.u(t,4),end=r.u(t+4,4),ur=r.u(t+8,4);
            if(end<begin)throw std::runtime_error("invalid exception function range");
            Json unwind= {
              {
                "rva",ur
              }, {
                "coverage","header_and_slot_bytes_only"
              }
            };
            if(ur) {
              auto u=offset(ur,4),vf=r.u(u,1),codes=r.u(u+2,1);
              auto version=vf&7,flags=vf>>3;
              unwind["version"]=version;
              unwind["flags"]=flags;
              unwind["prolog_size"]=r.u(u+1,1);
              unwind["code_slots"]=codes;
              unwind["frame_register_and_offset"]=r.u(u+3,1);
              offset(ur,4+codes*2);
              auto extra=4+((codes+1)&~std::uint64_t(1))*2;
              if(flags&4)offset(ur+extra,12);
              else if(flags&3)offset(ur+extra,4);
              if(version!=1&&version!=2)unwind["coverage"]="unknown_unwind_version";
            }
            f["exception_functions"].push_back({{"begin_rva",begin},{"end_rva",end},{"unwind",unwind}});
          }
        }
        else f["exception_coverage"]="directory_only_machine_semantics_unresolved";
      }
      auto [rel,rsz]=dir(5);
      std::uint64_t nr=0;
      if(rel) {
        std::uint64_t consumed=0;
        while(consumed<rsz) {
          if(rsz-consumed<8)throw std::runtime_error("short relocation block");
          auto t=offset(rel+consumed,8),sz=r.u(t+4,4);
          if(sz<8||sz>rsz-consumed||sz%2)throw std::runtime_error("invalid relocation block");
          offset(rel+consumed,sz);
          nr+=(sz-8)/2;
          consumed+=sz;
        }
      }
      f["relocation_entries"]=nr;
      auto [tls,tsz]=dir(9);
      f["tls"]= {
        {
          "present",bool(tls)
        }
      };
      if(tls) {
        auto n=wide?40u:24u;
        if(tsz<n)throw std::runtime_error("short TLS directory");
        auto t=offset(tls,n),cb=r.u(t+(wide?24:12),wide?8:4);
        std::uint64_t count=0;
        if(cb) {
          if(cb<image)throw std::runtime_error("invalid TLS callback VA");
          bool done=false;
          for(unsigned k=0;k<65536;k++) {
            auto a=r.u(offset(cb-image+std::uint64_t(k)*(wide?8:4),wide?8:4),wide?8:4);
            if(!a) {
              done=true;
              break;
            }
            count++;
          }
          if(!done)throw std::runtime_error("TLS callback limit exceeded");
        }
        f["tls"]["callback_count"]=count;
      }
      auto [clr,csz]=dir(14);
      f["clr"]= {
        {
          "present",bool(clr)
        }
      };
      if(clr) {
        j["kind"]="managed_pe";
        if(csz<72)throw std::runtime_error("short CLR directory");
        auto t=offset(clr,72);
        if(r.u(t,4)<72)throw std::runtime_error("short CLR header");
        f["clr"]["flags"]=r.u(t+16,4);
        auto mr=r.u(t+8,4),ms=r.u(t+12,4),m=offset(mr,ms);
        if(ms<20||r.u(m,4)!=0x424a5342)throw std::runtime_error("invalid CLR metadata root");
        auto len=r.u(m+12,4);
        if(len>ms-16||len>4096)throw std::runtime_error("invalid CLR version length");
        r.need(m+16,len);
        std::string version;
        for(std::uint64_t k=0;k<len&&r.u(m+16+k,1);k++)version+=char(r.u(m+16+k,1));
        f["clr"]["metadata_signature"]="BSJB";
        f["clr"]["runtime_version"]=version;
        auto stream=(16+len+3)&~std::uint64_t(3);
        if(stream+4>ms)throw std::runtime_error("short CLR stream header");
        auto count=r.u(m+stream+2,2);
        stream+=4;
        Json streams=Json::array();
        for(std::uint64_t k=0;k<count;k++) {
          if(stream+8>ms)throw std::runtime_error("short CLR stream record");
          auto pos=r.u(m+stream,4),sz=r.u(m+stream+4,4);
          auto name=r.str(m+stream+8);
          if(name.size()>32||stream+8+name.size()+1>ms||pos>ms||sz>ms-pos)throw std::runtime_error("invalid CLR stream bounds");
          streams.push_back({{"name",name},{"size",sz}});
          stream+=(8+name.size()+1+3)&~std::uint64_t(3);
        }
        f["clr"]["streams"]=streams;
        f["clr"]["assembly_reference_coverage"]="unresolved_metadata_tables";
      }
      for(auto i:{0,2,4,6,7,8,10,11,12,13,15}) {
        auto [a,n]=dir(i);
        if(a||n) {
          f["additional_directories"].push_back({{"index",i},{"address",a},{"size",n}});
          f["semantic_inventory_complete"]=false;
        }
      }
      requirement(j,clr?"runtime.clr":"runtime.windows",path,"PE structure");
      requirement(j,"dependencies.dynamic_native",path,"computed LoadLibrary and runtime bindings unresolved");
      if(clr)requirement(j,"dependencies.managed_metadata",path,"assembly references and managed/native interop unresolved");
    }
    void elf(Reader r,Json&j,const std::string&path) {
      j["kind"]="elf";
      auto&f=j["facts"];
      f["classification_confidence"]="structural";
      auto cls=r.u(4,1),end=r.u(5,1);
      if((cls!=1&&cls!=2)||(end!=1&&end!=2))throw std::runtime_error("unsupported ELF class/encoding");
      r.le=end==1;
      bool wide=cls==2;
      r.need(0,wide?64:52);
      f["class"]=wide?64:32;
      f["endianness"]=r.le?"little":"big";
      f["machine"]=r.u(18,2);
      f["type"]=r.u(16,2);
      auto ph=r.u(wide?32:28,wide?8:4),sh=r.u(wide?40:32,wide?8:4),ps=r.u(wide?54:42,2),pn=r.u(wide?56:44,2),ss=r.u(wide?58:46,2),sn=r.u(wide?60:48,2);
      if(pn==0xffff||(!sn&&sh))throw std::runtime_error("ELF extended numbering unsupported");
      if((pn&&ps<(wide?56:32))||(sn&&ss<(wide?64:40)))throw std::runtime_error("short ELF table entry");
      r.need(ph,pn*ps);
      r.need(sh,sn*ss);
      f["segments"]=Json::array();
      f["relocations"]=Json::array();
      f["needed_libraries"]=Json::array();
      for(std::uint64_t i=0;i<pn;i++) {
        auto t=ph+i*ps,off=r.u(t+(wide?8:4),wide?8:4),sz=r.u(t+(wide?32:16),wide?8:4);
        r.need(off,sz);
        f["segments"].push_back({{"type",r.u(t,4)},{"offset",off},{"file_size",sz}});
        if(r.u(t,4)==2) {
          f["dynamic_segment_coverage"]="sectionless_dynamic_metadata_unresolved";
          f["semantic_inventory_complete"]=false;
        }
      }
      for(std::uint64_t i=0;i<sn;i++) {
        auto t=sh+i*ss,type=r.u(t+4,4),off=r.u(t+(wide?24:16),wide?8:4),sz=r.u(t+(wide?32:20),wide?8:4),es=r.u(t+(wide?56:36),wide?8:4);
        if(type!=8)r.need(off,sz);
        if(type==4||type==9) {
          auto min=wide?(type==4?24u:16u):(type==4?12u:8u);
          if(es<min||sz%es)throw std::runtime_error("invalid ELF relocation entries");
          if(sz/es>100000)throw std::runtime_error("ELF relocation inspection limit");
          for(std::uint64_t n=0;n<sz/es;n++) {
            auto z=off+n*es,info=r.u(z+(wide?8:4),wide?8:4);
            f["relocations"].push_back({{"offset",r.u(z,wide?8:4)},{"type",wide?(info&0xffffffff):(info&255)},{"symbol",wide?(info>>32):(info>>8)}});
          }
        }
        if(type==6) {
          auto link=r.u(t+(wide?40:24),4);
          if(link>=sn||es<(wide?16:8)||sz%es)throw std::runtime_error("invalid ELF dynamic section");
          auto st=sh+link*ss,so=r.u(st+(wide?24:16),wide?8:4),szs=r.u(st+(wide?32:20),wide?8:4);
          r.need(so,szs);
          bool done=false;
          for(std::uint64_t n=0;n<sz/es;n++) {
            auto z=off+n*es,tag=r.u(z,wide?8:4),v=r.u(z+(wide?8:4),wide?8:4);
            if(!tag) {
              done=true;
              break;
            }
            if(tag==1) {
              if(v>=szs)throw std::runtime_error("ELF string outside table");
              auto lib=r.str(so+v);
              if(lib.size()>=szs-v)throw std::runtime_error("ELF string crosses table");
              f["needed_libraries"].push_back(lib);
            }
          }
          if(!done)throw std::runtime_error("unterminated ELF dynamic section");
        }
      }
      requirement(j,"runtime.elf_abi",path,"ELF structure");
      requirement(j,"dependencies.dynamic_native",path,"computed dlopen and runtime symbol resolution unresolved");
    }
  }
  // namespace
  Json inspect_binary(std::span<const std::uint8_t>b,const std::string&path,std::optional<std::uint64_t> total_size) {
    if(b.size()>=4&&b[0]=='D'&&b[1]=='D'&&b[2]=='S'&&b[3]==' ')
      return inspect_dds(b,path,total_size.value_or(b.size()));
    Json j= {
      {
        "kind","unknown"
      }, {
        "facts", {
          {
            "inspection_complete",true
          }, {
            "semantic_inventory_complete",false
          }
        }
      }, {
        "requirements",Json::array()
      }, {
        "issues",Json::array()
      }
    };
    if(total_size && *total_size<b.size()) {
      j["facts"]["inspection_complete"]=false;
      j["issues"].push_back({{"code","input_size_contradiction"},{"path",path}});
      requirement(j,"inspection.complete",path,"total size smaller than available span","preparation");
      return j;
    }
    std::string ext=std::filesystem::path(path).extension().string();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
    try {
      Reader r {
        b
      };
      if(b.size()>=4&&b[0]=='T'&&b[1]=='E'&&b[2]=='S'&&b[3]=='4')
        bethesda(r,j,path,total_size.value_or(b.size()));
      else if(b.size()>=2&&b[0]=='M'&&b[1]=='Z')pe(r,j,path);
      else if(b.size()>=4&&r.u(0,4)==0x464c457f)elf(r,j,path);
      else if(b.size()>=4&&r.u(0,4)==0x9e2a83c1) {
        j["kind"]="unreal_asset";
        j["facts"]["classification_confidence"]="magic_only";
        requirement(j,"translation.unreal_asset",path,"Unreal package magic","preparation");
      }
      else if(ext==".lua") {
        j["kind"]="lua";
        j["facts"]["classification_confidence"]="extension_only";
        requirement(j,"runtime.lua",path,"extension hint","game_process");
        requirement(j,"dependencies.dynamic_script",path,"computed script imports unresolved");
      }
      else if(ext==".pak"||ext==".uasset"||ext==".umap") {
        j["kind"]=ext==".pak"?"unreal_pak":"unreal_asset";
        j["facts"]["classification_confidence"]="extension_only";
        requirement(j,ext==".pak"?"translation.unreal_pak":"translation.unreal_asset",path,"extension hint","preparation");
      }
      else if(ext==".dll"||ext==".exe"||ext==".elf") {
        j["facts"]["classification_confidence"]="extension_only";
        requirement(j,"format.identification",path,"extension conflicts with missing signature","preparation");
      }
      else {
        j["kind"]="data";
        j["facts"]["classification_confidence"]="opaque";
        requirement(j,"format.data_semantics",path,"opaque data requires schema","preparation");
      }
    }
    catch(const BethesdaHeaderError&e) {
      j["facts"]["inspection_complete"]=false;
      j["issues"].push_back({{"code",e.code},{"path",path},{"detail",e.what()}});
      requirement(j,"inspection.complete",path,"root header is incomplete, invalid, unsupported or over budget","preparation");
    }
    catch(const std::exception&e) {
      j["facts"]["inspection_complete"]=false;
      j["issues"].push_back({{"code","binary_structure_incomplete"},{"path",path},{"detail",e.what()}});
      requirement(j,"inspection.complete",path,"malformed, truncated or unsupported structure","preparation");
    }
    return j;
  }
}
// namespace modbridge
