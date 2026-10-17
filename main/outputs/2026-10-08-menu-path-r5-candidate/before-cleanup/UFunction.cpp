#include <Unreal/CoreUObject/UObject/Class.hpp>
#include "ScriptHookThunks.hpp"

#include <utility>
#include <type_traits>

#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Helpers/Casting.hpp>

namespace RC::Unreal
{
    IMPLEMENT_EXTERNAL_OBJECT_CLASS(UFunction);
    IMPLEMENT_EXTERNAL_OBJECT_CLASS(UDelegateFunction);
    IMPLEMENT_EXTERNAL_OBJECT_CLASS(USparseDelegateFunction);

#include <MemberVariableLayout_SrcWrapper_UFunction.hpp>
#include <MemberVariableLayout_SrcWrapper_USparseDelegateFunction.hpp>

    const uint32& UFunction::GetFunctionFlags() const
    {
        return GetFunctionFlagsBase();
    }

    uint32& UFunction::GetFunctionFlags()
    {
        return GetFunctionFlagsBase();
    }

    auto UFunction::GetFuncPtr() -> UnrealScriptFunction
    {
        return std::bit_cast<UnrealScriptFunction>(GetFunc());
    }

    auto UFunction::SetFuncPtr(UnrealScriptFunction NewFuncPtr) -> void
    {
        GetFunc() = std::bit_cast<std::remove_reference_t<decltype(std::declval<UFunction>().GetFunc())>>(NewFuncPtr);
    }

    //auto UFunction::GetNumParms() -> uint8_t
    //{
    //    uint8_t NumParameters = 0;
//
    //    this->ForEachProperty([&](FProperty* Param) {
    //        if (Param->HasAnyPropertyFlags(Unreal::CPF_Parm))
    //        {
    //            NumParameters++;
    //        }
    //        return LoopAction::Continue;
    //    });
    //    return NumParameters;
    //}

    //auto UFunction::GetParmsSize() -> int32_t
    //{
    //    int32_t ParamsSize = 0;
//
    //    this->ForEachProperty([&](FProperty* Param) {
    //        if (Param->HasAnyPropertyFlags(Unreal::CPF_Parm))
    //        {
    //            ParamsSize = Param->GetOffset_Internal() + Param->GetSize();
    //        }
    //        return LoopAction::Continue;
    //    });
    //    return ParamsSize;
    //}

    auto UFunction::GetReturnProperty() -> FProperty*
    {
        for (FProperty* Param : TFieldRange<FProperty>(this, EFieldIterationFlags::IncludeDeprecated)) 
        {
            if (Param->HasAnyPropertyFlags(Unreal::CPF_ReturnParm))
            {
                return Param;
            }
        }
        return nullptr;
    }

    auto UFunction::GetFunctionHookData() -> UnrealScriptFunctionData&
    { return ThunkDetail::Lookup(this); }

    auto UFunction::RegisterPreHook(const UnrealScriptFunctionCallable& PreCallback, void* CustomData) -> CallbackId
    { return ThunkDetail::Register(this, ThunkDetail::Phase::Pre, PreCallback, CustomData, nullptr); }

    auto UFunction::RegisterPostHook(const UnrealScriptFunctionCallable& PostCallback, void* CustomData) -> CallbackId
    { return ThunkDetail::Register(this, ThunkDetail::Phase::Post, PostCallback, CustomData, nullptr); }

    auto UFunction::RegisterPreHookForInstance(const UnrealScriptFunctionCallable& PreCallback, void* CustomData, UObject* FireOnInstance) -> CallbackId
    { return ThunkDetail::Register(this, ThunkDetail::Phase::Pre, PreCallback, CustomData, FireOnInstance); }

    auto UFunction::RegisterPostHookForInstance(const UnrealScriptFunctionCallable& PostCallback, void* CustomData, UObject* FireOnInstance) -> CallbackId
    { return ThunkDetail::Register(this, ThunkDetail::Phase::Post, PostCallback, CustomData, FireOnInstance); }

    auto UFunction::UnregisterHook(CallbackId CallbackId) -> bool
    { return ThunkDetail::Remove(this, CallbackId); }

    auto UFunction::UnregisterAllHooks() -> void
    { ThunkDetail::Retire(this); }

    auto UFunction::GetFunctionHookCallbackData(CallbackId CallbackId) -> UnrealScriptCallbackData*
    { return ThunkDetail::FindCallback(this, CallbackId); }
}
