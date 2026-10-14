#pragma once

#include <Unreal/Core/Clang/ClangPlatformCodeAnalysis.hpp>
#if !__has_feature(cxx_exceptions) || !__has_feature(cxx_rtti)
#error This Unreal source profile requires exceptions and RTTI.
#endif
