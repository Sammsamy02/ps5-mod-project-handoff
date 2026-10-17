#include <optional>
#include <stdexcept>
#include <iostream>
enum class DebugBuildMode{Detect,ForceDebug,ForceTest,ForceShipping};enum class StatsMode{Detect,ForceOn,ForceOff};struct Config{DebugBuildMode DebugBuild=DebugBuildMode::Detect;StatsMode Stats=StatsMode::Detect;} UnrealConfig;struct Results{unsigned build_configuration=0,stats=0;}results;namespace RC::Platform::NativeLayouts{void SetResolvedNativeLayoutAxes(std::optional<bool>a,std::optional<bool>b){if(a!=std::optional<bool>(false)||b!=std::optional<bool>(false))throw std::runtime_error("authenticated shipping/stats-off profile must resolve both axes; current seeded results are unknown");}}
int main(){try{
        std::optional<bool> native_non_shipping;
        std::optional<bool> native_stats;
        switch (UnrealConfig.DebugBuild)
        {
        case DebugBuildMode::ForceDebug:
        case DebugBuildMode::ForceTest: native_non_shipping = true; break;
        case DebugBuildMode::ForceShipping: native_non_shipping = false; break;
        case DebugBuildMode::Detect:
            if (results.build_configuration >= 1 && results.build_configuration <= 3)
                native_non_shipping = results.build_configuration != 1;
            break;
        }
        switch (UnrealConfig.Stats)
        {
        case StatsMode::ForceOn: native_stats = true; break;
        case StatsMode::ForceOff: native_stats = false; break;
        case StatsMode::Detect:
            if (results.stats == 1 || results.stats == 2) native_stats = results.stats == 2;
            break;
        }
        RC::Platform::NativeLayouts::SetResolvedNativeLayoutAxes(native_non_shipping, native_stats);
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}std::cout<<"PASS exact profile axes\n";}
