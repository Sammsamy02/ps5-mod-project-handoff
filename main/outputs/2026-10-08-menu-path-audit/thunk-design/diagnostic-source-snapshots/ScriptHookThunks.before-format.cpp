#include "ScriptHookThunks.hpp"
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Hooks.hpp>
#include <DynamicOutput/Output.hpp>
#include <array>
#include <stdexcept>
#include <utility>

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
    catch (std::exception& e) { Output::send(STR("Error executing thunk pre callback: {}\n"), e.what()); }
    original(context, frame, result);
    try { data->FirePostCallbacks(callable_context); }
    catch (std::exception& e) { Output::send(STR("Error executing thunk post callback: {}\n"), e.what()); }
}

template<size_t Index>
void Thunk(UObject* context, FFrame& frame, void* result) {
    try { Dispatch(Index, context, frame, result); }
    catch (std::exception& e) { Output::send(STR("Error executing script thunk: {}\n"), e.what()); }
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
