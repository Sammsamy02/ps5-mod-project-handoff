#pragma once
// Vendored for rung 5 by worker-real-input, 2026-10-04. Originals immutable.
//
// Two host-testable halves of the dispatch leg, vendored so the pad-edge → Lua-callback
// → ExecuteInGameThread discipline can be exercised without the full UE4SSProgram:
//
// 1. KeybindDispatch — the EXACT matching rules of
//    .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/src/Input/Handler.cpp:66-99
//    (Handler::process_event): key bounds check, key-set lookup, required-modifier
//    equality, callback snapshot before invocation (registration inside a callback
//    stays safe). Registration surface mirrors UE4SSProgram/LuaMod register_keydown_event.
//
// 2. GameThreadGate — the refusal contract of
//    .superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp
//    (RC::Unreal::NativeThreadDetail::Identity): identity is published once by the
//    genuine Tick publisher; delivery REFUSES while the identity is unknown and REFUSES
//    on any other thread. Nothing is queued across the refusal (the caller observes the
//    refusal; replay is the caller's policy decision, never the gate's).

#include <Input/Handler.hpp> // Key, InputEvent, ModifierKeys, max_keys (retained definitions)
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace modbridge::input
{
class KeybindDispatch
{
public:
    // Mirrors Handler::register_keydown_event surface (Handler.hpp:register_keydown_event).
    void register_keydown_event(RC::Input::Key key, RC::Input::ModifierKeys required_modifiers,
                                std::function<void()> callback);
    bool is_keydown_event_registered(RC::Input::Key key) const;
    std::vector<bool> subscribed_keys() const; // std::array<bool,max_keys> shape for PadEdgeSource::poll

    // Handler::process_event rule, split for testability: match() snapshots callbacks;
    // invoke() runs them outside the internal lock (Handler.cpp:69-96 ordering).
    std::vector<std::function<void()>> match(const std::vector<RC::Input::InputEvent>& events) const;
    static void invoke(const std::vector<std::function<void()>>& callbacks);

private:
    struct VendoredKeyData
    {
        RC::Input::ModifierKeys required_modifier_keys{};
        std::function<void()> callback{};
    };
    mutable std::mutex m_mutex;
    std::unordered_map<int, std::vector<VendoredKeyData>> m_key_set; // keyed by int(Key)
};

class GameThreadGate
{
public:
    enum class Deliver { Delivered, RefusedUnknownIdentity, RefusedOtherThread };
    // Only the genuine Tick publisher calls publish_current (mirrors
    // NativeThreadDetail::Identity::publish_current; first publication wins).
    void publish_current();
    bool initialized() const;
    // Runs action only when the game-thread identity is known AND the caller is that
    // thread. Unknown identity or any other thread => refusal, action NOT run, NOT queued.
    Deliver deliver_in_game_thread(const std::function<void()>& action);

private:
    mutable std::mutex m_mutex;
    std::thread::id m_game_thread{};
};
}
