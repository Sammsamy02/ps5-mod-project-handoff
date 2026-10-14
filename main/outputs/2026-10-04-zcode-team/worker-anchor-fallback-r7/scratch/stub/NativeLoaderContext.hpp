#pragma once
#include "lmf_to_native_loader_facts.hpp"
namespace RC { namespace Platform {
struct NativeProgramContext { int dummy; };
NativeProgramContext MakeNativeProgramContext(lmf::NativeLoaderFacts);
}}  // namespace RC::Platform
