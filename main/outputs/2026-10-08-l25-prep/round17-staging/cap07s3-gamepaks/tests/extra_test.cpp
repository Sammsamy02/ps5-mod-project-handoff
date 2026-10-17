#include "PakRequestProducer.hpp"
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
namespace fs = std::filesystem;
using namespace ModBridge::PakMount;
static int run=0,failed=0;
static void check(bool ok,const char* n,const std::string& d){++run;if(!ok)++failed;std::printf("%s %s %s\n",ok?"PASS":"FAIL",n,d.c_str());}
static void touch(const fs::path& p){fs::create_directories(p.parent_path());std::ofstream(p)<<"x";}
int main(int argc,char**argv){
 if(argc!=2)return 2;const fs::path root=argv[1];std::string why;
 { auto app=root/"r1";touch(app/"eboot.bin");fs::create_directories(app/"sce_sys");
  touch(app/"townfall/content/paks/~mods/m_P.pak");touch(app/"townfall/content/paks/~mods/m_P.utoc");touch(app/"townfall/content/paks/~mods/m_P.ucas");
  fs::path slash=app.string()+"/";why.clear();auto got=game_pak_mods_directory(slash,why);
  check(!got.empty()&&got.lexically_normal()==got&&valid_absolute_path(got.string()),"trailing-slash",got.string()+" "+why);
  if(!got.empty()){auto rows=discover_requests(got);check(rows.size()==1&&valid_absolute_path(rows[0].pak_path_abs)&&rows[0].pak_path_abs.find("//")==std::string::npos,"trailing-slash-census",rows.empty()?"none":rows[0].pak_path_abs);} }
 { auto app=root/"r2";touch(app/"townfall/content/paks/~mods");why.clear();auto got=game_pak_mods_directory(app,why);
  check(got.empty()&&why.find("no ~mods folder")!=std::string::npos,"tilde-mods-file",why); }
 { auto app=root/"r3";fs::create_directories(app/"ENGINE/Content/Paks/~mods");why.clear();auto got=game_pak_mods_directory(app,why);
  check(got.empty(),"ENGINE-excluded",why); }
 { auto app=root/"r4";fs::create_directories(app/"sce_sys");fs::create_directories(app/"townfall/content/paks/~mods");
  ::chmod((app/"townfall").c_str(),0);why.clear();auto got=game_pak_mods_directory(app,why);::chmod((app/"townfall").c_str(),0755);
  const bool names_child=why.find("townfall")!=std::string::npos&&why.find("errno")!=std::string::npos;
  check(got.empty(),"unreadable-project-miss",why);
  std::printf("INFO unreadable-project names child+errno: %s\n",names_child?"yes":"NO (diagnosability gap)"); }
 { auto app=root/"r5";fs::create_directories(app/"townfall/content/paks/~mods");::chmod((app/"townfall/content/paks").c_str(),0);
  why.clear();auto got=game_pak_mods_directory(app,why);::chmod((app/"townfall/content/paks").c_str(),0755);
  check(got.empty()&&why.find("listing unavailable")!=std::string::npos&&why.find("errno 13")!=std::string::npos,"unreadable-paks-errno",why); }
 { auto app=root/"r6";fs::create_directories(app/"townfall/content/paks/~mods");fs::create_directories(root/"r6out/content");
  fs::create_directory_symlink(root/"r6out",app/"other");why.clear();auto got=game_pak_mods_directory(app,why);
  check(got==app/"townfall/content/paks/~mods","symlink-sibling-ignored",got.string()+" "+why); }
 { auto app=root/"r7";fs::create_directories(app/"townfall/content/paks/~mods");fs::create_directories(app/"big");
  for(int i=0;i<4097;++i)std::ofstream(app/"big"/("f"+std::to_string(i)))<<"";
  why.clear();auto got=game_pak_mods_directory(app,why);
  check(got==app/"townfall/content/paks/~mods","bound-sibling-skipped",got.string()+" "+why); }
 std::printf("RUN %s run=%d failed=%d\n",(run>0&&failed==0)?"PASS":"FAIL",run,failed);
 return (run>0&&failed==0)?0:1;
}
