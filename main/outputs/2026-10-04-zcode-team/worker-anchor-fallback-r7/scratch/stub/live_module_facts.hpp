#pragma once
#include <cstdint>
#include <memory>
namespace lmf {
struct OsVmMapping { std::uint64_t start, end; std::uint32_t protection; };
struct LiveFirmwareProfile { std::uint32_t module_info_abi_size; std::uint32_t module_segment_capacity; std::uint32_t fw_version; bool allow_loader_handle_as_module_handle; char exe_name_suffix[32]; };
struct VmScanResult { int chosen_index; std::uint64_t chosen_base; std::uint64_t chosen_extent; int candidate_count; int truncated; };
struct LiveModuleFactsSnapshot {
  struct Capabilities { std::uint32_t vmentry, vm_scan, link_map_walk; } capabilities;
  std::uint32_t provider_code, identity_source;
};
class RetainedLiveModuleFactsSupplier {
 public:
  RetainedLiveModuleFactsSupplier(const struct LiveModuleOsOps &, const LiveFirmwareProfile &);
  void capture();
  const LiveModuleFactsSnapshot &facts() const;
};
}  // namespace lmf
