#pragma once
#include "live_module_facts.hpp"
namespace lmf {
struct LiveModuleOsOps {
  void *context;
  int (*query_vmentry)(void *, OsVmMapping *, std::size_t, std::size_t *);
  int (*scan_image_base)(void *, const OsVmMapping *, std::size_t, VmScanResult *);
};
LiveModuleOsOps make_sce_live_module_os_ops(void *context) noexcept;
}  // namespace lmf
