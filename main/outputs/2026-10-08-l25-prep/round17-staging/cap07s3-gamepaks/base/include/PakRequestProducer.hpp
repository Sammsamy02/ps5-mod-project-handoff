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
