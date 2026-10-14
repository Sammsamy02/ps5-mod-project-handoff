#pragma once
#include <filesystem>
#include <memory>
#include "live_module_facts.hpp"
namespace lmf { struct NativeLoaderFacts { int dummy; }; }
namespace RC { namespace Platform {
struct LmfConversionOptions { bool allow_vm_scan_base_as_module_image_base; };
struct LmfConversionResult {
  explicit operator bool() const { return ok; }
  const char *error_message = "";
  lmf::NativeLoaderFacts facts{};
  bool ok = false;
};
LmfConversionResult TryMakeNativeLoaderFactsFromSupplier(
    std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier>,
    const std::filesystem::path &, const LmfConversionOptions &);
}}  // namespace RC::Platform
