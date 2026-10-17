#pragma once
#include "RuntimePakMount.hpp"
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <string>
namespace ModBridge::PakMount {
// [launch J fix, 2026-10-13] Console-proven: lstat-based checks failed on PS5 ("filesystem status unavailable") while
// stat (std::filesystem::status) and directory_iterator with cached entry types work (the #9 C++ mod scan, every
// launch). Only those two primitives are used here; every failure names the path and the OS error number.
[[noreturn]] inline void fs_fail(const char* what,const std::filesystem::path& p,const std::error_code& ec){
 throw std::runtime_error(std::string(what)+": "+p.string()+" (errno "+std::to_string(ec.value())+")");
}
inline std::filesystem::file_status checked_status(const std::filesystem::path& p,bool missing=false){
 std::error_code ec;auto s=std::filesystem::status(p,ec);
 if(ec&&!(missing&&ec==std::errc::no_such_file_or_directory))fs_fail("filesystem status unavailable",p,ec);
 return s;
}
inline uintmax_t checked_size(const std::filesystem::path& p){
 std::error_code ec;auto n=std::filesystem::file_size(p,ec);
 if(ec||!n)throw std::runtime_error("container size unavailable/empty");return n;
}
// [#7 step 2 source, launches K/L, 2026-10-14] Console-proven: the configured <root>/pakmods is not visible from
// the game process after module_start (launch K: stat ENOENT on a folder FTP lists), and this engine build does not
// auto-mount Paks/~mods (launch L). The engine reads its own paks from the game package, so pak mods go in its
// "~mods" child (the PC UE convention). The pak folder is derived from the configured game directory with the
// generic UE package shape IterateGameDirectories uses on PS5: <game dir>/<Project>/Content/Paks, where <Project> is
// the ONE child (not "Engine") holding a Content folder. Names compare ASCII case-insensitively (package paths are
// often lower-case). No name is guessed: zero or several matches is a miss. No filesystem call throws (only string building can, and
// the caller catches it); every miss returns
// an empty path and a reason naming the path and the OS error number. Symlinked entries are never followed.
inline bool ascii_iequals(const std::string& a,const char* b){
 size_t i=0;
 for(;i<a.size()&&b[i];++i){
  char x=a[i],y=b[i];
  if(x>='A'&&x<='Z')x=static_cast<char>(x-'A'+'a');
  if(y>='A'&&y<='Z')y=static_cast<char>(y-'A'+'a');
  if(x!=y)return false;
 }
 return i==a.size()&&!b[i];
}
inline std::string fs_why(const char* what,const std::filesystem::path& p,const std::error_code& ec){
 return std::string(what)+": "+p.string()+" (errno "+std::to_string(ec.value())+")";
}
// Directory children of `dir` named `name` (case-insensitive), or every child except "engine" when name is null.
inline bool list_child_dirs(const std::filesystem::path& dir,const char* name,std::vector<std::filesystem::path>& out,std::string& why){
 out.clear();std::error_code list_ec;
 std::filesystem::directory_iterator it(dir,list_ec),end;
 if(list_ec){why=fs_why("listing unavailable",dir,list_ec);return false;}
 for(size_t n=0;it!=end;it.increment(list_ec)){
  if(list_ec){why=fs_why("listing failed",dir,list_ec);return false;}
  if(++n>4096){why="listing limit: "+dir.string();return false;}
  const auto child=it->path().filename().string();
  if(name?!ascii_iequals(child,name):ascii_iequals(child,"engine"))continue;
  std::error_code type_ec;const bool link=it->is_symlink(type_ec);
  if(type_ec){why=fs_why("entry type unavailable",it->path(),type_ec);return false;}
  if(link)continue;
  const bool folder=it->is_directory(type_ec);
  if(type_ec){why=fs_why("entry type unavailable",it->path(),type_ec);return false;}
  if(folder)out.push_back(it->path());
 }
 if(list_ec){why=fs_why("listing failed",dir,list_ec);return false;}
 return true;
}
inline std::filesystem::path unique_child_dir(const std::filesystem::path& dir,const char* name,std::string& why){
 if(!name){why="internal: unnamed child lookup";return {};}
 std::vector<std::filesystem::path> hits;
 if(!list_child_dirs(dir,name,hits,why))return {};
 if(hits.size()!=1){why=std::string(hits.empty()?"no ":"several ")+name+" folder in "+dir.string();return {};}
 return hits[0];
}
inline std::filesystem::path game_pak_mods_directory(const std::filesystem::path& game_dir,std::string& why){
 if(game_dir.empty()||!game_dir.is_absolute()||game_dir==game_dir.root_path()||game_dir.lexically_normal()!=game_dir){
  why="configured game directory unavailable";return {};
 }
 std::vector<std::filesystem::path> children,content;
 if(!list_child_dirs(game_dir,nullptr,children,why))return {};
 // Real UE packages have about five children; more is not a UE package layout (bounds the game-thread walk).
 if(children.size()>256){why="too many children in "+game_dir.string()+" ("+std::to_string(children.size())+")";return {};}
 // A child that cannot be listed (a system folder of the package) is skipped and counted, never fatal. A miss names
 // every child examined with its Content-hit count or its listing error, so one log line explains it.
 size_t unreadable=0;std::string seen;
 for(const auto& child:children){
  std::vector<std::filesystem::path> hits;std::string child_why;
  const bool listed=list_child_dirs(child,"content",hits,child_why);
  if(seen.size()<360)seen+=" "+child.filename().string()+(listed?"="+std::to_string(hits.size()):" ["+child_why+"]");
  if(!listed){++unreadable;continue;}
  content.insert(content.end(),hits.begin(),hits.end());
 }
 if(content.size()!=1){
  why=std::string(content.empty()?"no":"several")+" project Content folder in "+game_dir.string()+" (children="+
      std::to_string(children.size())+" unreadable="+std::to_string(unreadable)+":"+seen+")";
  return {};
 }
 const auto paks=unique_child_dir(content[0],"paks",why);
 if(paks.empty())return {};
 const auto mods=unique_child_dir(paks,"~mods",why);
 if(mods.empty())return {};
 return mods;
}
// All-or-nothing bounded directory discovery; never reads or rewrites containers.
inline std::vector<MountRequest> discover_requests(const std::filesystem::path& directory) {
 const auto root=directory.lexically_normal();
 if(!valid_absolute_path(directory.string())||root!=directory)throw std::runtime_error("request root must be normalized and absolute");
 // The request directory must be a real directory entry (not a symlink) of its configured parent. Checked through
 // the parent's directory listing (cached entry type), never by walking "/", "/data", ... (not answerable on PS5).
 {
  const auto parent=directory.parent_path();std::error_code parent_ec;bool seen=false;
  std::filesystem::directory_iterator pit(parent,parent_ec),pend;
  if(parent_ec)fs_fail("configured root listing unavailable",parent,parent_ec);
  for(size_t n=0;pit!=pend;pit.increment(parent_ec)){
   if(parent_ec)fs_fail("configured root listing failed",parent,parent_ec);
   if(++n>4096)throw std::runtime_error("configured root listing limit");
   if(pit->path().filename()!=directory.filename())continue;
   std::error_code link_ec;const bool link=pit->is_symlink(link_ec);
   if(link_ec)fs_fail("request directory entry type unavailable",directory,link_ec);
   if(link)throw std::runtime_error("symlink root refused");
   seen=true;break;
  }
  if(parent_ec)fs_fail("configured root listing failed",parent,parent_ec);
  if(!seen)throw std::runtime_error("request directory entry not found in its parent listing");
 }
 if(checked_status(directory).type()!=std::filesystem::file_type::directory)throw std::runtime_error("request directory unavailable");
 std::vector<MountRequest> out;size_t entries=0;std::error_code ec;
 std::filesystem::directory_iterator it(directory,ec),end;
 if(ec)fs_fail("directory census unavailable",directory,ec);
 for(;it!=end;it.increment(ec)){
  if(ec)fs_fail("directory census failed",directory,ec);
  const auto& item=*it;
  if(++entries>128)throw std::runtime_error("directory census limit");
  std::error_code link_ec;const bool link=item.is_symlink(link_ec);
  if(link_ec)fs_fail("entry type unavailable",item.path(),link_ec);
  if(link)throw std::runtime_error("symlink entry refused");
  const auto status=checked_status(item.path());
  if(item.path().extension()!=".pak")continue;
  if(status.type()!=std::filesystem::file_type::regular||!checked_size(item.path())||item.path().filename().string().size()>255)throw std::runtime_error("invalid container entry");
  auto toc=item.path();toc.replace_extension(".utoc");auto data=item.path();data.replace_extension(".ucas");
  const auto ts=checked_status(toc,true),ds=checked_status(data,true);
  const bool t=ts.type()!=std::filesystem::file_type::not_found,d=ds.type()!=std::filesystem::file_type::not_found;
  if(t!=d||(t&&(ts.type()!=std::filesystem::file_type::regular||ds.type()!=std::filesystem::file_type::regular||!checked_size(toc)||!checked_size(data))))throw std::runtime_error("incomplete or nonregular sidecars");
  if(out.size()==16)throw std::runtime_error("selection limit");
  MountRequest r;r.pak_path_abs=item.path().string();out.push_back(r);
 }
 if(ec)throw std::runtime_error("directory census failed");
 std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.pak_path_abs<b.pak_path_abs;});return out;
}
}
