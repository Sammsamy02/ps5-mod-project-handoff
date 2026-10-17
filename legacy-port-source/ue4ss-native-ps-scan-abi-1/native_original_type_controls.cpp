#include "PsScanAbi.h"
#include <String/StringType.hpp>
#include <type_traits>
static_assert(std::is_same_v<RC::CharType,PsChar16>);
static_assert(std::is_same_v<decltype(PsCtx::default_),void(*)(RC::CharType*)>);
#include "PsMappedImageCaller.hpp"
static_assert(std::is_same_v<decltype(RC::Platform::NativeProgramContext::module_source),std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot>>);
