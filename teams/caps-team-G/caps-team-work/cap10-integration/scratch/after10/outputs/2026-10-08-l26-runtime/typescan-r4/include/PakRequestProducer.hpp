#pragma once
#include "RuntimePakMount.hpp"
#include <filesystem>
#include <algorithm>
#include <stdexcept>
namespace ModBridge::PakMount {
inline std::filesystem::file_status checked_status(const std::filesystem::path& p,bool missing=false){
 std::error_code ec;auto s=std::filesystem::symlink_status(p,ec);
 if(ec&&!(missing&&ec==std::errc::no_such_file_or_directory))throw std::runtime_error("filesystem status unavailable");
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
 std::filesystem::path prefix;
 for(const auto& part:directory){prefix/=part;if(checked_status(prefix).type()==std::filesystem::file_type::symlink)throw std::runtime_error("symlink root refused");}
 if(checked_status(directory).type()!=std::filesystem::file_type::directory)throw std::runtime_error("request directory unavailable");
 std::vector<MountRequest> out;size_t entries=0;std::error_code ec;
 std::filesystem::directory_iterator it(directory,ec),end;
 if(ec)throw std::runtime_error("directory census unavailable");
 for(;it!=end;it.increment(ec)){
  if(ec)throw std::runtime_error("directory census failed");
  const auto& item=*it;
  if(++entries>128)throw std::runtime_error("directory census limit");
  const auto status=checked_status(item.path());
  if(status.type()==std::filesystem::file_type::symlink)throw std::runtime_error("symlink entry refused");
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
