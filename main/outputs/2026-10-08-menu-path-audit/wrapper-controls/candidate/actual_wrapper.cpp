#include "../host_model.hpp"
#pragma once

#include <functional>
#include <map>
#include <atomic>

namespace RC::Unreal
{
    class UObject;

    using UnrealScriptFunction = void(*)(UObject* Context, FFrame& TheStack, void* RESULT_DECL);
    using CallbackId = int32_t;
    static constexpr CallbackId InvalidCallbackId = -1;

    class RC_UE_API UnrealScriptFunctionCallableContext
    {
    public:
        UObject* Context;
        FFrame& TheStack;
        void* RESULT_DECL;
        // CallableId is set dynamically by FirePreCallbacks for each callback, not by 'UnrealScriptFunctionHook'.
        CallbackId CallableId{InvalidCallbackId};

        UnrealScriptFunctionCallableContext(UObject* Context, FFrame& TheStack, void* RESULT_DECL);

        template<typename ReturnType>
        auto SetReturnValue(ReturnType NewValue) -> void
        {
            *static_cast<ReturnType*>(RESULT_DECL) = NewValue;
        }

        template<typename ParamStruct>
        auto GetParams() -> ParamStruct&
        {
            return *static_cast<ParamStruct*>(static_cast<void*>(TheStack.Locals()));
        }
    };
    using UnrealScriptFunctionCallable = std::function<void(UnrealScriptFunctionCallableContext& Context, void* CustomData)>;

    struct UnrealScriptCallbackData
    {
        UnrealScriptFunctionCallable Callable;
        void* CustomData;
        std::atomic<bool> UnregistrationRequested{};
        UObject* FireOnInstance{};

        UnrealScriptCallbackData() = default;
        UnrealScriptCallbackData(UnrealScriptFunctionCallable Callable, void* CustomData, UObject* FireOnInstance) : Callable(Callable), CustomData(CustomData), FireOnInstance(FireOnInstance) {};
    };

    class RC_UE_API UnrealScriptFunctionData
    {
    private:
        UnrealScriptFunction OriginalFunc;
        std::map<int32_t, UnrealScriptCallbackData> PreCallbacks;
        std::map<int32_t, UnrealScriptCallbackData> PostCallbacks;

    private:
        static std::atomic<CallbackId> HookIndexCounter;

    public:
        std::atomic<bool> bIsMidExecution{};

    public:
        UnrealScriptFunctionData(UnrealScriptFunction OriginalFuncPtr);
        inline auto GetOriginalFuncPtr() const -> UnrealScriptFunction { return OriginalFunc; }

        CallbackId AddPreCallback(const UnrealScriptFunctionCallable& Callable, void* CustomData = nullptr, UObject* FireOnInstance = nullptr);
        CallbackId AddPostCallback(const UnrealScriptFunctionCallable& Callable, void* CustomData = nullptr, UObject* FireOnInstance = nullptr);
        bool RemoveCallback(CallbackId CallbackId);

        void RemoveAllCallbacks();

        void FirePreCallbacks(UnrealScriptFunctionCallableContext& Context);
        void FirePostCallbacks(UnrealScriptFunctionCallableContext& Context);

        UnrealScriptCallbackData* GetCallbackData(CallbackId);

        static CallbackId MakeNewId();
    };

    namespace Internal {
        using HookedUFunctionMap = std::unordered_map<const UFunction*, UnrealScriptFunctionData>;
        auto RC_UE_API GetHookedFunctionsMap() -> HookedUFunctionMap&;

        auto UnrealScriptFunctionHook(UObject* Context, FFrame& TheStack, void* RESULT_DECL) -> void;
    }
}


namespace RC::Unreal
{
    std::atomic<CallbackId> UnrealScriptFunctionData::HookIndexCounter{1};

    UnrealScriptFunctionData::UnrealScriptFunctionData(UnrealScriptFunction OriginalFuncPtr) {
        this->OriginalFunc = OriginalFuncPtr;
    }

    CallbackId UnrealScriptFunctionData::AddPreCallback(const UnrealScriptFunctionCallable& Callable, void* CustomData, UObject* FireOnInstance)
    {
        CallbackId NewCallbackId = MakeNewId();
        this->PreCallbacks.emplace(std::piecewise_construct, std::forward_as_tuple(NewCallbackId), std::forward_as_tuple(Callable, CustomData, FireOnInstance));
        return NewCallbackId;
    }

    CallbackId UnrealScriptFunctionData::AddPostCallback(const UnrealScriptFunctionCallable& Callable, void* CustomData, UObject* FireOnInstance)
    {
        CallbackId NewCallbackId = MakeNewId();
        this->PostCallbacks.emplace(std::piecewise_construct, std::forward_as_tuple(NewCallbackId), std::forward_as_tuple(Callable, CustomData, FireOnInstance));
        return NewCallbackId;
    }

    bool UnrealScriptFunctionData::RemoveCallback(CallbackId CallbackId)
    {
        auto CallbackData = GetCallbackData(CallbackId);
        if (CallbackData)
        {
            if (bIsMidExecution)
            {
                // Hook will be unregistered after the callback is done executing.
                CallbackData->UnregistrationRequested = true;
                return true;
            }
            else
            {
                return PreCallbacks.erase(CallbackId) || PostCallbacks.erase(CallbackId);
            }
        }
        else
        {
            return false;
        }
    }

    void UnrealScriptFunctionData::RemoveAllCallbacks()
    {
        for (auto PreCallbackIterator = PreCallbacks.begin(); PreCallbackIterator != PreCallbacks.end();)
        {
            if (bIsMidExecution)
            {
                // Hook will be unregistered after the callback is done executing.
                PreCallbackIterator->second.UnregistrationRequested = true;
                ++PreCallbackIterator;
            }
            else
            {
                PreCallbackIterator = PreCallbacks.erase(PreCallbackIterator);
            }
        }

        for (auto PostCallbackIterator = PostCallbacks.begin(); PostCallbackIterator != PostCallbacks.end();)
        {
            if (bIsMidExecution)
            {
                // Hook will be unregistered after the callback is done executing.
                PostCallbackIterator->second.UnregistrationRequested = true;
                ++PostCallbackIterator;
            }
            else
            {
                PostCallbackIterator = PostCallbacks.erase(PostCallbackIterator);
            }
        }
    }

    void UnrealScriptFunctionData::FirePreCallbacks(UnrealScriptFunctionCallableContext& Context)
    {
        for (auto PreCallbackIterator = PreCallbacks.begin(); PreCallbackIterator != PreCallbacks.end();)
        {
            // Skip callbacks that don't match the instance filter
            if (PreCallbackIterator->second.FireOnInstance && PreCallbackIterator->second.FireOnInstance != Context.Context)
            {
                ++PreCallbackIterator;
                continue;
            }
            // Skip callbacks that have been marked for unregistration or have invalid Callable
            if (PreCallbackIterator->second.UnregistrationRequested || !PreCallbackIterator->second.Callable)
            {
                PreCallbackIterator = PreCallbacks.erase(PreCallbackIterator);
                continue;
            }
            Context.CallableId = PreCallbackIterator->first;
            PreCallbackIterator->second.Callable(Context, PreCallbackIterator->second.CustomData);
            if (PreCallbackIterator->second.UnregistrationRequested)
            {
                PreCallbackIterator = PreCallbacks.erase(PreCallbackIterator);
            }
            else
            {
                ++PreCallbackIterator;
            }
        }
    }

    void UnrealScriptFunctionData::FirePostCallbacks(UnrealScriptFunctionCallableContext& Context)
    {
        for (auto PostCallbackIterator = PostCallbacks.begin(); PostCallbackIterator != PostCallbacks.end();)
        {
            // Skip callbacks that have been marked for unregistration or have invalid Callable
            if (PostCallbackIterator->second.UnregistrationRequested || !PostCallbackIterator->second.Callable)
            {
                PostCallbackIterator = PostCallbacks.erase(PostCallbackIterator);
                continue;
            }
            Context.CallableId = PostCallbackIterator->first;
            PostCallbackIterator->second.Callable(Context, PostCallbackIterator->second.CustomData);
            if (PostCallbackIterator->second.UnregistrationRequested)
            {
                PostCallbackIterator = PostCallbacks.erase(PostCallbackIterator);
            }
            else
            {
                ++PostCallbackIterator;
            }
        }
    }

    UnrealScriptCallbackData* UnrealScriptFunctionData::GetCallbackData(CallbackId CallbackId)
    {
        auto PreIterator = PreCallbacks.find(CallbackId);
        if (PreIterator != PreCallbacks.end()) { return &PreIterator->second; }

        auto PostIterator = PostCallbacks.find(CallbackId);
        if (PostIterator != PostCallbacks.end()) { return &PostIterator->second; }

        return nullptr;
    }

    CallbackId UnrealScriptFunctionData::MakeNewId()
    {
        return HookIndexCounter++;
    }

    UnrealScriptFunctionCallableContext::UnrealScriptFunctionCallableContext(UObject* Context, FFrame& TheStack, void* RESULT_DECL)
            : Context(Context),
              TheStack(TheStack),
              RESULT_DECL(RESULT_DECL) {}

    static Internal::HookedUFunctionMap GHookedScriptFunctions{};

    auto Internal::GetHookedFunctionsMap() -> HookedUFunctionMap&
    {
        return GHookedScriptFunctions;
    }

    auto Internal::UnrealScriptFunctionHook(UObject* Context, FFrame& TheStack, void* RESULT_DECL) -> void
    {
        try
        {
            HookedUFunctionMap& FunctionMap = GetHookedFunctionsMap();
            // LOCAL SAFETY EXPERIMENT ONLY: installation is not qualified until
            // every supported hook entry supplies a proven CurrentNativeFunction.
            // Code is an instruction cursor, not an authenticated identity prefix.
            auto FunctionBeingExecuted = TheStack.CurrentNativeFunction();
            if (!FunctionBeingExecuted)
            {
                Output::send<LogLevel::Error>(STR("Refused UFunction hook frame: current native function unavailable.\n"));
                return;
            }
            auto Iterator = FunctionMap.find(FunctionBeingExecuted);
            if (Iterator == FunctionMap.end())
            {
                Output::send<LogLevel::Error>(STR("Refused UFunction hook frame: no saved original for current function.\n"));
                return;
            }
            auto Original = Iterator->second.GetOriginalFuncPtr();
            if (!Original || Original == &Internal::UnrealScriptFunctionHook)
            {
                Output::send<LogLevel::Error>(STR("Refused UFunction hook frame: saved original unavailable or recursive.\n"));
                return;
            }
            {
                FFrame* NewStackPtr = &TheStack;
                std::map<FProperty*, FOutParmRec*> OriginalOutParmsMap; // Map to track original out params

                if (TheStack.Code() && GNatives_Internal)
                {
                    // Create a new stack frame
                    NewStackPtr = FFrame::MallocAndMemsetNewFrame();
                    NewStackPtr->Object() = Context;
                    NewStackPtr->Node() = FunctionBeingExecuted;
                    NewStackPtr->PreviousFrame() = &TheStack;
                    if (Version::IsBelow(4, 25))
                    {
                        NewStackPtr->PropertyChainForCompiledIn() = std::bit_cast<FField*>(FunctionBeingExecuted->GetChildren().Get());
                    }
                    else
                    {
                        NewStackPtr->PropertyChainForCompiledIn() = FunctionBeingExecuted->GetChildProperties();
                    }
                    auto NewLocals = static_cast<uint8*>(alloca(FunctionBeingExecuted->GetParmsSize()));
                    NewStackPtr->Locals() = NewLocals;

                    // Map out parameters for later use
                    FOutParmRec* CurrentOutParm = TheStack.OutParms();
                    while (CurrentOutParm)
                    {
                        OriginalOutParmsMap[CurrentOutParm->Property] = CurrentOutParm;
                        CurrentOutParm = CurrentOutParm->NextOutParm;
                    }

                    // Loop through parameters and evaluate them
                    for (FProperty* prop : TFieldRange<FProperty>(FunctionBeingExecuted, EFieldIterationFlags::IncludeDeprecated))
                    {
                        // Skip non parameters and return values
                        if (!prop->HasAnyPropertyFlags(CPF_Parm) || prop->HasAnyPropertyFlags(CPF_ReturnParm)) 
                            continue;

                        // Initialize property storage
                        uint8* PropertyData = prop->ContainerPtrToValuePtr<uint8>(NewStackPtr->Locals());
                        prop->InitializeValue(PropertyData);

                        if (prop->HasAnyPropertyFlags(CPF_OutParm))
                        {
                            // Create new out parameter slot and add it to the linked list
                            FOutParmRec* NewOutParam = static_cast<FOutParmRec*>(alloca(sizeof(Unreal::FOutParmRec)));
                            NewOutParam->Property = prop;
                            NewOutParam->NextOutParm = NewStackPtr->OutParms();
                            NewStackPtr->OutParms() = NewOutParam;

                            // Step the original stack to get the address for the out param
                            TheStack.StepCompiledIn(PropertyData, prop);
                            NewOutParam->PropAddr = TheStack.MostRecentPropertyAddress();
                        }
                        else
                        {
                            // Regular parameter - evaluate it from the original stack
                            TheStack.StepCompiledIn(PropertyData, prop);
                        }
                    }

                    // Code pointer needs to move past the parameters
                    TheStack.Code() += !!TheStack.Code();
                }

                // Create function context with our (possibly new) stack frame
                UnrealScriptFunctionCallableContext FuncContext(Context, *NewStackPtr, RESULT_DECL);
                
                Iterator->second.bIsMidExecution = true;

                try
                {
                    // Fire pre callbacks with all parameters now properly populated
                    Iterator->second.FirePreCallbacks(FuncContext);
                }
                catch (std::exception& e)
                {
                    Output::send(STR("Error executing hook pre-callback {}: {}\n"), 
                                 FunctionBeingExecuted->GetPathName(), ensure_str(e.what()));
                }

                // Execute the original function with our stack
                Iterator->second.GetOriginalFuncPtr()(Context, *NewStackPtr, RESULT_DECL);
                
                try
                {
                    // Fire post callbacks
                    Iterator->second.FirePostCallbacks(FuncContext);
                }
                catch (std::exception& e)
                {
                    Output::send(STR("Error executing hook post-callback {}: {}\n"), 
                                 FunctionBeingExecuted->GetPathName(), ensure_str(e.what()));
                }

                // Copy out parameter values back to the original frame
                if (TheStack.Code() && GNatives_Internal)
                {
                    // Copy out parameter values back to the original frame
                    for (FOutParmRec* OutParam = NewStackPtr->OutParms(); OutParam; OutParam = OutParam->NextOutParm)
                    {
                        auto OriginalIt = OriginalOutParmsMap.find(OutParam->Property);
                        if (OriginalIt != OriginalOutParmsMap.end())
                        {
                            // Copy the value back to the original out parameter
                            OutParam->Property->CopyCompleteValue(OriginalIt->second->PropAddr, OutParam->PropAddr);
                        }
                    }

                    // Clean up the temporary stack frame
                    for (FProperty* prop : TFieldRange<FProperty>(FunctionBeingExecuted, EFieldIterationFlags::IncludeDeprecated))
                    {
                        if (!prop->HasAnyPropertyFlags(CPF_Parm)) continue;
                        if (prop->HasAnyPropertyFlags(CPF_ReturnParm)) continue;

                        uint8* PropertyData = prop->ContainerPtrToValuePtr<uint8>(NewStackPtr->Locals());
                        prop->DestroyValue(PropertyData);
                    }
                    FMemory::Free(NewStackPtr);
                }
                    
                Iterator->second.bIsMidExecution = false;
            }
        }
        catch (std::exception& e)
        {
            Output::send(STR("Error executing hooked function: {}\n"), ensure_str(e.what()));
        }
    }
}

namespace RC::Unreal {
    auto UFunction::GetFunctionHookData() -> UnrealScriptFunctionData&
    {
        Internal::HookedUFunctionMap& HookedFunctions = Internal::GetHookedFunctionsMap();
        auto Iterator = HookedFunctions.find(this);

        if (Iterator == HookedFunctions.end())
        {
            Iterator = HookedFunctions.emplace(std::piecewise_construct, std::forward_as_tuple(this), std::forward_as_tuple(GetFuncPtr())).first;
            SetFuncPtr(&Internal::UnrealScriptFunctionHook);
        }
        return Iterator->second;
    }

    auto UFunction::RegisterPreHook(const UnrealScriptFunctionCallable& PreCallback, void* CustomData) -> CallbackId
    {
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.AddPreCallback(PreCallback, CustomData, nullptr);
    }

    auto UFunction::RegisterPostHook(const UnrealScriptFunctionCallable& PostCallback, void* CustomData) -> CallbackId
    {
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.AddPostCallback(PostCallback, CustomData, nullptr);
    }


}
#include "../cases.cpp"
