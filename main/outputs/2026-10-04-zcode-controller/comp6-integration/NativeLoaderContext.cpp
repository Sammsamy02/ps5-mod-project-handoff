#include <NativeLoaderContext.hpp>
#include <utility>
namespace RC::Platform {
auto MakeNativeProgramContext(NativeLoaderFacts facts) -> NativeProgramContext {
    return MakeNativeProgramContext(std::move(facts),{});
}
auto MakeNativeProgramContext(NativeLoaderFacts facts,const std::vector<uint8_t>& serialized_elf) -> NativeProgramContext {
    // This target's actual engine/module ABI is ELF64/SysV. Cross-format scanner
    // support is not admission to a native PS5 program or Windows DLL support.
    if(facts.mapped.format!=PS_MAPPED_IMAGE_ELF64)
        throw std::invalid_argument("Native PS5 program requires caller-supplied ELF64 mapped image facts");
    auto snapshot=Unreal::Platform::ModuleSnapshot::Create(std::move(facts.modules));
    NativeProgramContext context{};
    context.module_file_path=std::move(facts.module_file_path);
    context.game_executable_path=std::move(facts.game_executable_path);
    context.configured_game_directory=std::move(facts.configured_game_directory);
    context.configured_working_directory=std::move(facts.configured_working_directory);
    context.module_source=snapshot;
    context.input_source=std::move(facts.input_source);
    ValidateNativeProgramContext(context);
    context.scanner_source=std::make_shared<const Unreal::Platform::OwnedMappedScanSource>(snapshot,facts.mapped,facts.scan_policy,serialized_elf);
    return context;
}
}
