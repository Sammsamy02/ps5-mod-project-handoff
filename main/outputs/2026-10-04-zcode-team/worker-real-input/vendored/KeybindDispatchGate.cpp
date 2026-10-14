// Vendored for rung 5 by worker-real-input, 2026-10-04. Originals immutable.
// Matching rules vendored from Handler::process_event (coherent port
// first/src/Input/Handler.cpp:66-99); gate contract vendored from
// NativeGameThreadIdentity.hpp (RC::Unreal::NativeThreadDetail::Identity).
#include "KeybindDispatchGate.hpp"
#include <array>
#include <thread>

namespace modbridge::input
{
using RC::Input::InputEvent;
using RC::Input::Key;
using RC::Input::max_keys;

void KeybindDispatch::register_keydown_event(Key key, RC::Input::ModifierKeys required_modifiers,
                                             std::function<void()> callback)
{
    if (static_cast<size_t>(key) >= max_keys || !callback) return;
    std::lock_guard lock{m_mutex};
    m_key_set[static_cast<int>(key)].push_back({required_modifiers, std::move(callback)});
}

bool KeybindDispatch::is_keydown_event_registered(Key key) const
{
    std::lock_guard lock{m_mutex};
    const auto found = m_key_set.find(static_cast<int>(key));
    return found != m_key_set.end() && !found->second.empty();
}

std::vector<bool> KeybindDispatch::subscribed_keys() const
{
    // Handler keeps m_subscribed_keys true for every key with registered data; the pad
    // source's poll() only emits edges for subscribed keys (PS5PadSource.cpp:85).
    std::vector<bool> subscribed(max_keys, false);
    std::lock_guard lock{m_mutex};
    for (const auto& [key, data] : m_key_set)
        if (!data.empty()) subscribed[static_cast<size_t>(key)] = true;
    return subscribed;
}

std::vector<std::function<void()>> KeybindDispatch::match(const std::vector<InputEvent>& events) const
{
    // VENDORED: Handler::process_event loop, verbatim rule order (Handler.cpp:79-94):
    // skip out-of-range keys, skip unregistered keys, require modifier equality,
    // snapshot callbacks while holding the lock...
    std::vector<std::function<void()>> callbacks_to_call{};
    std::lock_guard lock{m_mutex};
    for (const auto& event : events)
    {
        if (static_cast<size_t>(event.key) >= max_keys) continue;
        const auto found = m_key_set.find(static_cast<int>(event.key));
        if (found == m_key_set.end()) continue;
        for (const auto& key_data : found->second)
            if (key_data.required_modifier_keys == event.modifier_keys)
                callbacks_to_call.emplace_back(key_data.callback);
    }
    return callbacks_to_call;
}

void KeybindDispatch::invoke(const std::vector<std::function<void()>>& callbacks)
{
    // VENDORED: ...then run them WITHOUT the lock (Handler.cpp:96-99), which is what
    // makes registration inside a callback safe.
    for (const auto& callback : callbacks) callback();
}

void GameThreadGate::publish_current()
{
    std::lock_guard lock{m_mutex};
    if (m_game_thread == std::thread::id{}) m_game_thread = std::this_thread::get_id();
}

bool GameThreadGate::initialized() const
{
    std::lock_guard lock{m_mutex};
    return m_game_thread != std::thread::id{};
}

GameThreadGate::Deliver GameThreadGate::deliver_in_game_thread(const std::function<void()>& action)
{
    std::thread::id known{};
    {
        std::lock_guard lock{m_mutex};
        known = m_game_thread;
    }
    if (known == std::thread::id{}) return Deliver::RefusedUnknownIdentity;
    if (known != std::this_thread::get_id()) return Deliver::RefusedOtherThread;
    if (action) action();
    return Deliver::Delivered;
}
}
