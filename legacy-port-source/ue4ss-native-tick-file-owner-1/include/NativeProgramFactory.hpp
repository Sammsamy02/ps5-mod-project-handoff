#pragma once
#include <NativeLoaderContext.hpp>
#include <UE4SSProgram.hpp>
namespace RC::Platform {
// Same-SDK C++ construction only. The real constructor performs filesystem/log
// setup and may load configured native mods. It does not initialize the engine.
// Stored constructor errors surface as runtime_error; no readiness is certified.
// Caller runs init() on its legitimate initialization worker, not the game
// thread or an injected payload worker pretending to be it. init() blocks while
// its update thread runs. Caller must request_shutdown(), wait for its init()
// caller to return, and establish actual engine-hook/callback quiescence before
// destroying the returned owner or unloading UE4SS/any retained module bytes.
// No async/session framework, scheduling provider or cancellation is added.
auto CreateNativeUE4SSProgram(NativeLoaderFacts,
    std::initializer_list<MProgram::BinaryOptions> options={}) -> std::unique_ptr<UE4SSProgram>;
// Optional already-owned serialized ELF copy, admitted against the same actual
// mapped context. Acquisition and lifetime facts remain the caller's obligation.
auto CreateNativeUE4SSProgram(NativeLoaderFacts, const std::vector<uint8_t>& serialized_elf,
    std::initializer_list<MProgram::BinaryOptions> options) -> std::unique_ptr<UE4SSProgram>;
}
