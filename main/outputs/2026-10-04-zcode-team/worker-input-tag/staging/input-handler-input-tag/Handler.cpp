#include <algorithm>
#include <array>
#include <climits>
#include <cstring>

#include <Input/Handler.hpp>
#include <Input/PlatformInputSource.hpp>
#include <DynamicOutput/Output.hpp>

// worker-input-tag: shared composition diagnostic sink (diagnostic.c), the
// same namespace-scope extern "C" pattern the coexist PS5PadSource.cpp TU
// already uses. The symbol is DEFINED in-link by the composition's
// diagnostic.c bootstrap unit, so this adds NO new import and appears as no
// new undefined symbol at the combined-supplier PRX boundary.
extern "C" void diagnostic_event(const char* tag, unsigned int value);

namespace
{
    // worker-input-tag: per-boot pad_key_edge tag budget (see process_event).
    constexpr unsigned int pad_key_edge_tag_cap = 60;
}

namespace RC::Input
{
    std::unordered_map<std::string, std::shared_ptr<PlatformInputSource>> Handler::m_input_sources_store;

    Handler::~Handler()
    {
        if (m_source_injected_mode) shutdown();
    }

    auto Handler::shutdown() noexcept -> bool
    {
        try
        {
            auto event_update_lock = std::lock_guard(m_event_mutex);
            if (m_platform_handler && !m_platform_handler->deactivate()) return false;
            m_platform_handler.reset();
            m_injected_source.reset();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    auto Handler::is_input_available() const -> bool
    {
        return m_platform_handler && m_platform_handler->is_available();
    }

    auto Handler::activate_input_source(std::shared_ptr<PlatformInputSource> candidate) -> bool
    {
        // Caller holds m_event_mutex and owns serialized source reconfiguration.
        // Both named selection and explicit injection use this activation boundary.
        if (candidate != m_platform_handler)
        {
            if (!candidate->activate()) return false;
            if (m_platform_handler) m_platform_handler->deactivate();
            m_platform_handler = std::move(candidate);
        }
        return true;
    }

    auto Handler::init(std::shared_ptr<PlatformInputSource> input_source) -> bool
    {
        // Serialized injection/selection contract described in Handler.hpp. An
        // unsuccessful replacement leaves current ownership/activation untouched.
        auto event_update_lock = std::lock_guard(m_event_mutex);
        if (!m_platform_handler) m_source_injected_mode = true;
        if (!input_source) return false;
        // worker-pad-coexist: DEFERRED ATTACH. An injected provider that is not
        // (yet) available no longer refuses init. The live evidence (launch-10
        // pids 317/341/355/370) is the host game holding the exclusive pad
        // handle from before injection; wpd1 made startup survive by NOT
        // supplying such a source, which also made recovery unreachable. The
        // deferred-attached source is polled by process_event below and recovers
        // through its own poll()-side retry on the sole processing thread - same
        // thread as every other source call, never a cross-thread re-injection.
        // An unavailable candidate never displaces an existing attachment.
        if (!input_source->is_available())
        {
            if (m_platform_handler) return false;
            m_platform_handler = input_source;
            m_source_injected_mode = true;
            m_injected_source = std::move(input_source);
            return true;
        }
        if (!activate_input_source(input_source)) return false;
        m_source_injected_mode = true;
        m_injected_source = std::move(input_source);
        return true;
    }

    auto Handler::process_event() -> void
    {
        // worker-pad-coexist: the availability gate moved INTO the source's own
        // processing path. A deferred-attached source must be polled so its
        // bounded event-driven retry can run (PS5PadSource::poll returns no
        // events while unavailable); every other attached source already returns
        // empty events through its own unavailable path. m_platform_handler is
        // only ever assigned by init/activate paths (activated source, or the
        // deferred attach above), so process_event stays same-thread serialized
        // per the Handler.hpp lifecycle contract.
        if (m_platform_handler == nullptr)
        {
            return;
        }

        std::vector<EventCallbackCallable> callbacks_to_call{};

        auto events = m_platform_handler->process_event(this);
        // worker-input-tag: bounded input-arrival evidence (acceptance gap
        // G-AP1, dispatch half). One tag per ARRIVING key edge, value = the
        // UE4SS Key code; the six mod keybind codes are pairwise distinct
        // (F7=0x76 = the OPTIONS bind, RETURN=0x0D, arrows 0x25-0x28), so
        // pad_key_edge=0x76 identifies the first OPTIONS press
        // unambiguously. After pad_key_edge_tag_cap (60) tags per boot: ONE
        // pad_key_edge_silent marker (value = the cap), then total silence -
        // the diagnostic sink is append-only, so per-press tagging MUST be
        // bounded. Deliberately BEFORE the m_allow_input gate: arrival is
        // arrival regardless of the later dispatch gates. Function-local
        // statics are per-boot state (the composition has exactly one
        // Handler) and unsynchronized exactly like every other Handler
        // mutation - the sole-processing-thread lifecycle contract. No
        // behavior change: tagged events continue through the unchanged
        // dispatch below even once tagging has gone silent.
        {
            static unsigned int pad_key_edge_tags = 0;
            static bool pad_key_edge_silent = false;
            if (!pad_key_edge_silent)
            {
                for (const auto& event : events)
                {
                    diagnostic_event("pad_key_edge", static_cast<unsigned int>(event.key));
                    ++pad_key_edge_tags;
                    if (pad_key_edge_tags >= pad_key_edge_tag_cap)
                    {
                        pad_key_edge_silent = true;
                        diagnostic_event("pad_key_edge_silent", pad_key_edge_tag_cap);
                        break;
                    }
                }
            }
        }
        if (!m_allow_input) return;

        {
            // Lock the event mutex to access the key_set
            auto event_update_lock = std::lock_guard(m_event_mutex);
            for (auto& event : events)
            {
                if (static_cast<size_t>(event.key) >= max_keys) continue;
                const auto found = m_key_set.key_data.find(event.key);
                if (found == m_key_set.key_data.end()) continue;
                const auto& key_set_array = found->second;
                for (auto& key_data : key_set_array)
                {
                    if (key_data.required_modifier_keys == event.modifier_keys)
                    {
                        callbacks_to_call.emplace_back(key_data.callback);
                    }
                }
            }
        }

        // No need to lock the event mutex to call the callbacks
        // this avoids key registration inside the callback
        for (const auto& callback : callbacks_to_call)
        {
            callback();
        }
    }

    auto Handler::register_keydown_event(Input::Key key, EventCallbackCallable callback, uint8_t custom_data, void* custom_data2) -> void
    {
        auto event_update_lock = std::lock_guard(m_event_mutex);
        if (static_cast<size_t>(key) >= max_keys) throw std::out_of_range("Input key is outside the supported key range");
        // worker-pad-coexist: registration requires an INJECTED SOURCE, not
        // current availability. The deferred-attached provider is a real input
        // source; its binds must register at mod load (the load banner rung)
        // and fire when the poll-side retry later opens the pad. With no
        // injected source at all the refusal is unchanged.
        if (m_source_injected_mode && !m_injected_source) throw std::runtime_error("Input registration unavailable: no active input source");
        if (!callback) throw std::invalid_argument("Input callback is empty");
        KeyData& key_data = m_key_set.key_data[key].emplace_back();
        key_data.callback = callback;
        key_data.custom_data = custom_data;
        key_data.custom_data2 = custom_data2;
        m_subscribed_keys[key] = true;
    }

    auto Handler::register_keydown_event(
            Input::Key key, const ModifierKeyArray& modifier_keys, const EventCallbackCallable& callback, uint8_t custom_data, void* custom_data2) -> void
    {
        auto event_update_lock = std::lock_guard(m_event_mutex);
        if (static_cast<size_t>(key) >= max_keys) throw std::out_of_range("Input key is outside the supported key range");
        // worker-pad-coexist: same deferred-attach tolerance as above.
        if (m_source_injected_mode && !m_injected_source) throw std::runtime_error("Input registration unavailable: no active input source");
        if (!callback) throw std::invalid_argument("Input callback is empty");
        KeyData& key_data = m_key_set.key_data[key].emplace_back();
        key_data.callback = callback;
        key_data.custom_data = custom_data;
        key_data.custom_data2 = custom_data2;
        key_data.requires_modifier_keys = true;
        key_data.required_modifier_keys = modifier_keys;
        m_subscribed_keys[key] = true;
    }

    auto Handler::is_keydown_event_registered(Input::Key key) -> bool
    {
        auto event_update_lock = std::lock_guard(m_event_mutex);
        auto key_data = m_key_set.key_data.find(key);
        if (key_data == m_key_set.key_data.end())
        {
            return false;
        }
        for (const auto& key_data_container : key_data->second)
        {
            if (key_data_container.required_modifier_keys.empty())
            {
                return true;
            }
        }
        return false;
    }

    auto Handler::is_keydown_event_registered(Input::Key key, const ModifierKeyArray& modifier_keys_array) -> bool
    {
        auto event_update_lock = std::lock_guard(m_event_mutex);
        auto key_data = m_key_set.key_data.find(key);
        auto modifier_keys = ModifierKeys(modifier_keys_array);
        if (key_data == m_key_set.key_data.end())
        {
            return false;
        }
        for (const auto& key_data_container : key_data->second)
        {
            if (key_data_container.required_modifier_keys == modifier_keys)
            {
                return true;
            }
        }
        return false;
    }

    auto Handler::has_event_on_key(Input::Key key) -> bool
    {
        return static_cast<size_t>(key) < max_keys && m_subscribed_keys[key];
    }

    auto Handler::get_events_safe(std::function<void(KeySet&)> callback) -> void
    {
        auto event_update_lock = std::lock_guard(m_event_mutex);
        callback(m_key_set);
    }

    auto Handler::clear_subscribed_keys() -> void
    {
        m_subscribed_keys.fill(false);
    }

    auto Handler::clear_subscribed_key(Key k) -> void
    {
        if (static_cast<size_t>(k) < max_keys) m_subscribed_keys[k] = false;
    }

    auto Handler::get_allow_input() -> bool
    {
        return m_allow_input;
    }

    auto Handler::set_allow_input(bool new_value) -> void
    {
        m_allow_input = new_value;
    }

    /// Set the input source to the given source
    /// SAFETY: Windows sources: main thread. Injected sources: serialized lifecycle
    /// owner only, with no process_event or reconfiguration overlap.
    auto Handler::set_input_source(std::string source) -> bool
    {
        auto event_update_lock = std::lock_guard(m_event_mutex);
        std::shared_ptr<PlatformInputSource> next_input_source = nullptr;
        if (m_source_injected_mode)
        {
            next_input_source = m_injected_source;
            // worker-pad-coexist: the injected-source identity check is
            // decoupled from current availability. The program unit calls this
            // right after init with the constant configured name; a
            // deferred-attached (not yet opened) provider is still the supplied
            // provider and must not fail the composition's validation gate.
            // activate_input_source below never displaces the attachment and
            // never activates a different source in injected mode.
            if (!next_input_source) return false;
            if (source != "Default" && source != next_input_source->get_name()) return false;
        }
        else if (source == "Default")
        {
            // find the highest priority input source
            int highest_priority = INT_MAX;
            for (auto& input_source : m_input_sources_store)
            {
                if (!input_source.second->is_available())
                {
                    continue;
                }
                auto priority = input_source.second->source_priority();
                if (priority < highest_priority)
                {
                    next_input_source = input_source.second;
                    highest_priority = priority;
                }
            }
            if (highest_priority == INT_MAX)
            {
                Output::send<LogLevel::Error>(STR("Input source: Source not found\n"));
                return false;
            }
        }
        else
        {
            auto input_source = m_input_sources_store.find(source);
            if (input_source == m_input_sources_store.end())
            {
                return false;
            }
            next_input_source = input_source->second;
            if (!next_input_source->is_available())
            {
                return false;
            }
        }
        return activate_input_source(std::move(next_input_source));
    }

    // register the input source to the input source store
    auto Handler::register_input_source(std::shared_ptr<PlatformInputSource> input_source) -> void
    {
        std::string name = input_source->get_name();
        if (m_input_sources_store.find(name) == m_input_sources_store.end())
        {
            m_input_sources_store[name] = input_source;
        }
    }

    auto Handler::get_current_input_source() -> std::string
    {
        if (m_platform_handler == nullptr)
        {
            return "None";
        }
        return m_platform_handler->get_name();
    }
} // namespace RC::Input
