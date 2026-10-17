#include <OwnedMappedScanSource.hpp>
#include <Unreal/UnrealInitializer.hpp>
extern "C" bool ps_scan(PsCtx* ctx,PsScanResults* results) {
    const auto& config=RC::Unreal::UnrealInitializer::StaticStorage::GlobalConfig;
    return RC::Unreal::Platform::scan_owned_source(config.NativeScanSource,config.ModuleSource,ctx,results);
}
