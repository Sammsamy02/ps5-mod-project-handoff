#include "../host_model.hpp"
#include <array>
#include <mutex>
#include <algorithm>
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


#pragma once
#include <mutex>
#include <cstddef>
namespace RC::Unreal::ThunkDetail {
// Generic bounded prototype configuration, not a game layout fact or final budget.
inline constexpr size_t Capacity = 8;
std::recursive_mutex& RegistryMutex();
UnrealScriptFunctionData& Install(UFunction*);
void Retire(UFunction*);
}
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

namespace RC::Unreal {
    auto UFunction::GetFunctionHookData() -> UnrealScriptFunctionData&
    { return ThunkDetail::Install(this); }

    auto UFunction::RegisterPreHook(const UnrealScriptFunctionCallable& PreCallback, void* CustomData) -> CallbackId
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.AddPreCallback(PreCallback, CustomData, nullptr);
    }

    auto UFunction::RegisterPostHook(const UnrealScriptFunctionCallable& PostCallback, void* CustomData) -> CallbackId
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.AddPostCallback(PostCallback, CustomData, nullptr);
    }

    auto UFunction::RegisterPreHookForInstance(const UnrealScriptFunctionCallable& PreCallback, void* CustomData, UObject* FireOnInstance) -> CallbackId
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.AddPreCallback(PreCallback, CustomData, FireOnInstance);
    }

    auto UFunction::RegisterPostHookForInstance(const UnrealScriptFunctionCallable& PostCallback, void* CustomData, UObject* FireOnInstance) -> CallbackId
    {
        std::lock_guard lock{ThunkDetail::RegistryMutex()};
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.AddPostCallback(PostCallback, CustomData, FireOnInstance);
    }

    auto UFunction::UnregisterHook(CallbackId CallbackId) -> bool
    {
        UnrealScriptFunctionData& FunctionData = GetFunctionHookData();
        return FunctionData.RemoveCallback(CallbackId);
    }

    auto UFunction::UnregisterAllHooks() -> void
    { ThunkDetail::Retire(this); }

    auto UFunction::GetFunctionHookCallbackData(CallbackId CallbackId) -> UnrealScriptCallbackData*
    {
        auto& FunctionData = GetFunctionHookData();
        return FunctionData.GetCallbackData(CallbackId);
    }
}
namespace RC::Unreal::ThunkDetail {
namespace {
enum class State { Free, Active, Retired };
struct Slot {
    State state{State::Free};
    UFunction* function{};
    UnrealScriptFunction original{};
    UnrealScriptFunctionData* data{};
    Internal::HookedUFunctionMap::node_type retained_node;
    size_t active_calls{};
};
std::recursive_mutex mutex;
std::array<Slot, Capacity> slots;

struct DispatchLifetime {
    Slot& slot;
    explicit DispatchLifetime(Slot& s) : slot(s) {}
    ~DispatchLifetime() {
        std::lock_guard lock{mutex};
        --slot.active_calls;
        slot.data->bIsMidExecution = slot.active_calls != 0;
    }
};

void Dispatch(size_t index, UObject* context, FFrame& frame, void* result) {
    Slot* slot;
    UnrealScriptFunction original;
    UnrealScriptFunctionData* data;
    bool callbacks;
    {
        std::lock_guard lock{mutex};
        slot = &slots[index];
        // Free thunks are never published. There is no valid original to guess.
        if (slot->state == State::Free || !slot->original || !slot->data) {
            Output::send<LogLevel::Error>(STR("Unpublished script thunk invoked; no installation admitted.\n"));
            return;
        }
        original = slot->original;
        data = slot->data;
        callbacks = slot->state == State::Active && !GNatives_Internal;
        ++slot->active_calls;
        data->bIsMidExecution = true;
    }
    DispatchLifetime lifetime{*slot};
    // Retirement and unsupported later GNatives changes preserve original game
    // execution, with no callback admission and no frame field interpretation.
    if (!callbacks) {
        original(context, frame, result);
        return;
    }
    UnrealScriptFunctionCallableContext callable_context{context, frame, result};
    try { data->FirePreCallbacks(callable_context); }
    catch (std::exception& e) { Output::send(STR("Error executing thunk pre callback: {}\n"), ensure_str(e.what())); }
    original(context, frame, result);
    try { data->FirePostCallbacks(callable_context); }
    catch (std::exception& e) { Output::send(STR("Error executing thunk post callback: {}\n"), ensure_str(e.what())); }
}

template<size_t Index>
void Thunk(UObject* context, FFrame& frame, void* result) {
    try { Dispatch(Index, context, frame, result); }
    catch (std::exception& e) { Output::send(STR("Error executing script thunk: {}\n"), ensure_str(e.what())); }
}
template<size_t... Index>
constexpr auto MakeThunks(std::index_sequence<Index...>) {
    return std::array<UnrealScriptFunction, sizeof...(Index)>{&Thunk<Index>...};
}
constexpr auto thunks = MakeThunks(std::make_index_sequence<Capacity>{});
bool IsOurPointer(UnrealScriptFunction function) {
    if (function == &Internal::UnrealScriptFunctionHook) return true;
    for (auto thunk : thunks) if (function == thunk) return true;
    return false;
}
}

std::recursive_mutex& RegistryMutex() { return mutex; }

UnrealScriptFunctionData& Install(UFunction* function) {
    std::lock_guard lock{mutex};
    auto& map = Internal::GetHookedFunctionsMap();
    auto found = map.find(function);
    if (found != map.end()) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].state == State::Active && slots[i].function == function &&
                slots[i].data == &found->second && function->GetFuncPtr() == thunks[i])
                return found->second;
        }
        throw std::runtime_error("Script hook installation conflicts with current Func or registry");
    }
    if (GNatives_Internal) throw std::runtime_error("Script thunk prototype requires GNatives-null borrowed-frame mode");
    auto original = function->GetFuncPtr();
    if (!original || IsOurPointer(original)) throw std::runtime_error("Script hook original unavailable or recursive");
    size_t index = 0;
    while (index < slots.size() && slots[index].state != State::Free) ++index;
    if (index == slots.size()) throw std::runtime_error("Script thunk capacity exhausted; Func unchanged");
    auto entry = map.emplace(std::piecewise_construct, std::forward_as_tuple(function), std::forward_as_tuple(original));
    auto& slot = slots[index];
    slot.function = function;
    slot.original = original;
    slot.data = &entry.first->second;
    slot.state = State::Active;
    try { function->SetFuncPtr(thunks[index]); }
    catch (...) {
        // A staged getter failure occurs before pointer storage. This rollback
        // does not authorize recovery from an unknown partial external write.
        slot.state = State::Free; slot.function = nullptr; slot.original = nullptr; slot.data = nullptr;
        map.erase(entry.first);
        throw;
    }
    return entry.first->second;
}

void Retire(UFunction* function) {
    std::lock_guard lock{mutex};
    auto& map = Internal::GetHookedFunctionsMap();
    auto entry = map.find(function);
    if (entry == map.end()) return;
    for (size_t i = 0; i < slots.size(); ++i) {
        auto& slot = slots[i];
        if (slot.state != State::Active || slot.function != function || slot.data != &entry->second) continue;
        entry->second.RemoveAllCallbacks();
        // Preserve an intervening replacement; never restore over another owner.
        if (function->GetFuncPtr() == thunks[i]) function->SetFuncPtr(slot.original);
        slot.state = State::Retired;
        slot.retained_node = map.extract(entry);
        return;
    }
    throw std::runtime_error("Script hook retirement has no owned slot");
}
}

#include "../cases.cpp"
