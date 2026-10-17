#include <Unreal/UFunctionStructs.hpp>
#include "ScriptHookThunks.hpp"
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/Format.hpp>
#include <Unreal/Hooks.hpp>
#include <vector>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace RC::Unreal
{
    std::atomic<CallbackId> UnrealScriptFunctionData::HookIndexCounter{1};

    UnrealScriptFunctionData::UnrealScriptFunctionData(UnrealScriptFunction OriginalFuncPtr) {
        this->OriginalFunc = OriginalFuncPtr;
    }

    CallbackId UnrealScriptFunctionData::AddPreCallback(const UnrealScriptFunctionCallable& Callable, void* CustomData, UObject* FireOnInstance)
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        CallbackId NewCallbackId = MakeNewId();
        this->PreCallbacks.emplace(std::piecewise_construct, std::forward_as_tuple(NewCallbackId), std::forward_as_tuple(Callable, CustomData, FireOnInstance));
        return NewCallbackId;
    }

    CallbackId UnrealScriptFunctionData::AddPostCallback(const UnrealScriptFunctionCallable& Callable, void* CustomData, UObject* FireOnInstance)
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        CallbackId NewCallbackId = MakeNewId();
        this->PostCallbacks.emplace(std::piecewise_construct, std::forward_as_tuple(NewCallbackId), std::forward_as_tuple(Callable, CustomData, FireOnInstance));
        return NewCallbackId;
    }

    bool UnrealScriptFunctionData::RemoveCallback(CallbackId CallbackId)
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        auto pre = PreCallbacks.find(CallbackId);
        auto post = PostCallbacks.find(CallbackId);
        auto* callback = pre != PreCallbacks.end() ? &pre->second : post != PostCallbacks.end() ? &post->second : nullptr;
        if (!callback) return false;
        if (bIsMidExecution) { callback->UnregistrationRequested = true; return true; }
        return PreCallbacks.erase(CallbackId) || PostCallbacks.erase(CallbackId);
    }

    void UnrealScriptFunctionData::RemoveAllCallbacks()
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
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
        // Snapshot identifiers and retain each callable outside the registry lock.
        // New callbacks start on the next dispatch; removal may cancel admission.
        std::vector<CallbackId> ids;
        { std::lock_guard lock{ThunkDetail::RegistryMutex()}; for (auto& pair : PreCallbacks) ids.push_back(pair.first); }
        for (auto id : ids) {
            UnrealScriptFunctionCallable callable;
            void* custom_data;
            {
                std::lock_guard lock{ThunkDetail::RegistryMutex()};
                auto entry = PreCallbacks.find(id);
                if (entry == PreCallbacks.end()) continue;
                if (entry->second.FireOnInstance && entry->second.FireOnInstance != Context.Context) continue;
                if (entry->second.UnregistrationRequested || !entry->second.Callable) { PreCallbacks.erase(entry); continue; }
                callable = entry->second.Callable;
                custom_data = entry->second.CustomData;
            }
            Context.CallableId = id;
            callable(Context, custom_data);
            {
                std::lock_guard lock{ThunkDetail::RegistryMutex()};
                auto entry = PreCallbacks.find(id);
                if (entry != PreCallbacks.end() && entry->second.UnregistrationRequested) PreCallbacks.erase(entry);
            }
        }
    }

    void UnrealScriptFunctionData::FirePostCallbacks(UnrealScriptFunctionCallableContext& Context)
    {
        // Snapshot identifiers and retain each callable outside the registry lock.
        // New callbacks start on the next dispatch; removal may cancel admission.
        std::vector<CallbackId> ids;
        { std::lock_guard lock{ThunkDetail::RegistryMutex()}; for (auto& pair : PostCallbacks) ids.push_back(pair.first); }
        for (auto id : ids) {
            UnrealScriptFunctionCallable callable;
            void* custom_data;
            {
                std::lock_guard lock{ThunkDetail::RegistryMutex()};
                auto entry = PostCallbacks.find(id);
                if (entry == PostCallbacks.end()) continue;
                
                if (entry->second.UnregistrationRequested || !entry->second.Callable) { PostCallbacks.erase(entry); continue; }
                callable = entry->second.Callable;
                custom_data = entry->second.CustomData;
            }
            Context.CallableId = id;
            callable(Context, custom_data);
            {
                std::lock_guard lock{ThunkDetail::RegistryMutex()};
                auto entry = PostCallbacks.find(id);
                if (entry != PostCallbacks.end() && entry->second.UnregistrationRequested) PostCallbacks.erase(entry);
            }
        }
    }

    UnrealScriptCallbackData* UnrealScriptFunctionData::GetCallbackData(CallbackId CallbackId)
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
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
        // Legacy shared entry is never installed by the scoped thunk prototype.
        Output::send<LogLevel::Error>(STR("Legacy script wrapper has no admitted registration identity.\n"));
    }
}
