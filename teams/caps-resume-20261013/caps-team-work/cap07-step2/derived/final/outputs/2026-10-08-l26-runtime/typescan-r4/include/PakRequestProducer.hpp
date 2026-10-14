#pragma once
#include "RuntimePakMount.hpp"
#include <filesystem>
#include <algorithm>
#include <stdexcept>
namespace ModBridge::PakMount {
// All-or-nothing bounded directory discovery; never reads or rewrites containers.
inline std::vector<MountRequest> discover_requests(const std::filesystem::path& directory) {
 const auto root=directory.lexically_normal();
 if(!valid_absolute_path(directory.string())||root!=directory)throw std::runtime_error("request root must be normalized and absolute");
 std::filesystem::path prefix;
 for(const auto& part:directory){prefix/=part;if(std::filesystem::is_symlink(std::filesystem::symlink_status(prefix)))throw std::runtime_error("symlink root refused");}
 if(!std::filesystem::is_directory(std::filesystem::symlink_status(directory)))throw std::runtime_error("request directory unavailable");
 std::vector<MountRequest> out;size_t entries=0;
 for(const auto& item:std::filesystem::directory_iterator(directory)){
  if(++entries>128)throw std::runtime_error("directory census limit");
  const auto status=item.symlink_status();
  if(std::filesystem::is_symlink(status))throw std::runtime_error("symlink entry refused");
  if(item.path().extension()!=".pak")continue;
  if(!std::filesystem::is_regular_file(status)||!item.file_size()||item.path().filename().string().size()>255)throw std::runtime_error("invalid container entry");
  auto toc=item.path();toc.replace_extension(".utoc");auto data=item.path();data.replace_extension(".ucas");
  const auto ts=std::filesystem::symlink_status(toc),ds=std::filesystem::symlink_status(data);
  const bool t=std::filesystem::exists(ts),d=std::filesystem::exists(ds);
  if(t!=d||(t&&(!std::filesystem::is_regular_file(ts)||!std::filesystem::is_regular_file(ds)||!std::filesystem::file_size(toc)||!std::filesystem::file_size(data))))throw std::runtime_error("incomplete or nonregular sidecars");
  if(out.size()==16)throw std::runtime_error("selection limit");
  MountRequest r;r.pak_path_abs=item.path().string();out.push_back(r);
 }
 std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.pak_path_abs<b.pak_path_abs;});return out;
}
}
