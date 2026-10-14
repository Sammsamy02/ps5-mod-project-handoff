#include "ScriptHookThunks.hpp"
#include "ScriptFunctionStorage.hpp"
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Hooks.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/Format.hpp>
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
constinit std::atomic<bool> registry_ready{false};
std::recursive_mutex mutex;
std::array<Slot, MaximumCapacity> slots;
size_t configured_capacity = MaximumCapacity;
size_t published_slots{};
bool accepting_registrations = true;
struct RegistryReadinessMarker {
    RegistryReadinessMarker() { registry_ready.store(true, std::memory_order_release); }
    ~RegistryReadinessMarker() { registry_ready.store(false, std::memory_order_release); }
};
// Last registry object constructed, first destroyed. The indicator itself is
// constant-initialized and requires no registry mutex to refuse partial init.
RegistryReadinessMarker readiness_marker;

struct DispatchLifetime {
    Slot& slot;
    explicit DispatchLifetime(Slot& s) : slot(s) {}
    ~DispatchLifetime() {
        std::lock_guard lock{mutex};
        --slot.active_calls;
        slot.data->bIsMidExecution = slot.active_calls != 0;
        if (!slot.active_calls && (slot.state == State::Retired || !accepting_registrations))
            slot.data->RemoveAllCallbacks();
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
        callbacks = slot->state == State::Active && accepting_registrations && !GNatives_Internal;
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
constexpr auto thunks = MakeThunks(std::make_index_sequence<MaximumCapacity>{});
bool IsOurPointer(UnrealScriptFunction function) {
    if (function == &Internal::UnrealScriptFunctionHook) return true;
    for (auto thunk : thunks) if (function == thunk) return true;
    return false;
}
}

bool RegistryReady() { return registry_ready.load(std::memory_order_acquire); }
std::recursive_mutex& RegistryMutex() { return mutex; }

void ConfigureCapacity(size_t limit) {
    std::lock_guard lock{mutex};
    if (!limit || limit > MaximumCapacity || published_slots || !accepting_registrations)
        throw std::runtime_error("Script thunk capacity configuration refused");
    configured_capacity = limit;
}

UnrealScriptFunctionData& Lookup(UFunction* function) {
    std::lock_guard lock{mutex};
    auto& map = Internal::GetHookedFunctionsMap();
    auto found = map.find(function);
    if (found == map.end()) throw std::runtime_error("Script hook data absent; query does not install");
    return found->second;
}

UnrealScriptCallbackData* FindCallback(UFunction* function, CallbackId id) {
    std::lock_guard lock{mutex};
    auto& map = Internal::GetHookedFunctionsMap();
    auto found = map.find(function);
    return found == map.end() ? nullptr : found->second.GetCallbackData(id);
}

CallbackId Register(UFunction* function, Phase phase, const UnrealScriptFunctionCallable& callable, void* custom, UObject* instance) {
    std::lock_guard lock{mutex};
    if (!callable) throw std::runtime_error("Script hook callable unavailable; no installation admitted");
    if (!accepting_registrations) throw std::runtime_error("Script hook registration closed for teardown");
    if (GNatives_Internal) throw std::runtime_error("Script thunk requires GNatives-null borrowed-frame mode");
    auto& map = Internal::GetHookedFunctionsMap();
    auto found = map.find(function);
    auto add = [&](UnrealScriptFunctionData& data) {
        return phase == Phase::Pre ? data.AddPreCallback(callable, custom, instance) : data.AddPostCallback(callable, custom, instance);
    };
    if (found != map.end()) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].state == State::Active && slots[i].function == function &&
                slots[i].data == &found->second && function->GetFuncPtr() == thunks[i])
                return add(found->second);
        }
        throw std::runtime_error("Script hook registration conflicts with Func or registry");
    }
    auto original = function->GetFuncPtr();
    if (!original || IsOurPointer(original)) throw std::runtime_error("Script hook original unavailable or recursive");
    size_t index = 0;
    while (index < configured_capacity && slots[index].state != State::Free) ++index;
    if (index == configured_capacity) throw std::runtime_error("Script thunk capacity exhausted; Func unchanged");
    // Construct the callable before either global map admission or Func storage.
    Internal::HookedUFunctionMap staging;
    auto temporary = staging.emplace(std::piecewise_construct, std::forward_as_tuple(function), std::forward_as_tuple(original));
    auto id = add(temporary.first->second);
    auto committed = map.insert(staging.extract(temporary.first));
    if (!committed.inserted) throw std::runtime_error("Script hook map admission conflict; Func unchanged");
    auto& slot = slots[index];
    slot.function = function; slot.original = original; slot.data = &committed.position->second; slot.state = State::Active;
    try { function->SetFuncPtr(thunks[index]); }
    catch (...) {
        slot.state = State::Free; slot.function = nullptr; slot.original = nullptr; slot.data = nullptr;
        map.erase(committed.position);
        throw;
    }
    ++published_slots;
    return id;
}

bool Remove(UFunction* function, CallbackId id) {
    std::lock_guard lock{mutex};
    auto& map = Internal::GetHookedFunctionsMap();
    auto found = map.find(function);
    if (found == map.end()) return false;
    bool removed = found->second.RemoveCallback(id);
    if (removed && !found->second.HasCallbacks()) Retire(function);
    return removed;
}

TeardownSnapshot BeginTeardown() {
    if (!RegistryReady()) throw std::runtime_error("Script thunk registry lifetime unavailable");
    std::lock_guard lock{mutex};
    accepting_registrations = false;
    size_t active_calls = 0;
    for (auto& slot : slots) {
        active_calls += slot.active_calls;
        if (slot.state == State::Active) slot.data->RemoveAllCallbacks();
    }
    // No blind native field writes: caller must establish per-object validity
    // for explicit retirement. Cached thunk code may outlive all current calls.
    return {published_slots ? Residence::RequiresCodeResidence : Residence::Unpublished, published_slots, active_calls};
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
        ScriptFuncStorage::Replace(function->GetFunc(), thunks[i], slot.original);
        slot.state = State::Retired;
        slot.retained_node = map.extract(entry);
        return;
    }
    throw std::runtime_error("Script hook retirement has no owned slot");
}
}

extern "C" int bridge_native_code_residence_required(void) {
    if (!RC::Unreal::ThunkDetail::RegistryReady()) return -1;
    try {
        return RC::Unreal::ThunkDetail::BeginTeardown().residence ==
            RC::Unreal::ThunkDetail::Residence::RequiresCodeResidence ? 1 : 0;
    } catch (...) { return -1; }
}
