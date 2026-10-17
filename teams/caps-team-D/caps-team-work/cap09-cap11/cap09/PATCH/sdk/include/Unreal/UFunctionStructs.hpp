// Unreal/UFunctionStructs.hpp -- PS5 Mod Bridge native mod SDK.
// Hook callback shapes compatible with UE4SS RegisterHook for the documented
// subset: Context, GetParams<T>(), SetReturnValue<T>(). The upstream member
// `FFrame& TheStack` is NOT provided (no bytecode-stack access in ABI v1).
#pragma once
#include <Unreal/UObject.hpp>

#include <functional>

namespace RC::Unreal
{
    class UnrealScriptFunctionCallableContext
    {
      public:
        UObject* Context{};
        UFunction* Function{};
        void* Params{};
        void* RESULT_DECL{};

        template <typename ParamStruct>
        auto GetParams() -> ParamStruct&
        {
            return *static_cast<ParamStruct*>(Params);
        }
        template <typename ReturnType>
        auto SetReturnValue(ReturnType NewValue) -> void
        {
            if (RESULT_DECL) *static_cast<ReturnType*>(RESULT_DECL) = NewValue;
        }
    };
    using UnrealScriptFunctionCallable = std::function<void(UnrealScriptFunctionCallableContext& Context, void* CustomData)>;
} // namespace RC::Unreal
