#include <NativeProgramFactory.hpp>
#include <string>
#include <utility>
namespace RC::Platform {
auto CreateNativeUE4SSProgram(NativeLoaderFacts facts,std::initializer_list<MProgram::BinaryOptions> options) -> std::unique_ptr<UE4SSProgram> {
    return CreateNativeUE4SSProgram(std::move(facts),{},options);
}
auto CreateNativeUE4SSProgram(NativeLoaderFacts facts,const std::vector<uint8_t>& serialized_elf,std::initializer_list<MProgram::BinaryOptions> options) -> std::unique_ptr<UE4SSProgram> {
    // MProgram directly indexes its options array; validate before constructing.
    for(auto option:options)
        if(static_cast<int>(option)<0 || static_cast<int>(option)>=static_cast<int>(MProgram::Max))
            throw std::invalid_argument("Native program binary option is invalid");
    auto context=MakeNativeProgramContext(std::move(facts),serialized_elf);
    auto program=std::make_unique<UE4SSProgram>(std::move(context),options);
    const auto error=program->get_error_object();
    if(error->has_error()) {
        const std::string message=error->get_message();
        program.reset();
        throw std::runtime_error("Native UE4SS construction rejected: "+message);
    }
    return program;
}
}
