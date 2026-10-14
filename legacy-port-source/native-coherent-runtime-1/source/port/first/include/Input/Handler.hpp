#pragma once

// TODO: Abstract more... need to get rid of Windows.h from InputHandler.cpp

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <array>
#include <mutex>

#include <Input/Common.hpp>
#include <Input/KeyDef.hpp>

namespace RC::Input
{
    using EventCallbackCallable = std::function<void()>;

    struct InputEvent
    {
        Key key;
        ModifierKeys modifier_keys{};
    };

    struct KeyData
    {
        ModifierKeys required_modifier_keys{};
        EventCallbackCallable callback{};
        uint8_t custom_data{};
        void* custom_data2{};
        bool requires_modifier_keys{};
        bool is_down{};
    };

    struct KeySet
    {
        std::unordered_map<Key, std::vector<KeyData>> key_data;
    };

    using ModifierKeyArray = std::array<Input::ModifierKey, max_modifier_keys>;

#ifdef HAS_INPUT
    class PlatformInputSource;
    class RC_INPUT_API Handler
    {
      private:
        // std::vector<KeySet> m_key_sets{};
        KeySet m_key_set{};
        bool m_allow_input{true};
        std::array<bool, max_keys> m_subscribed_keys{};

        std::shared_ptr<PlatformInputSource> m_platform_handler;
        std::mutex m_event_mutex;
        std::shared_ptr<PlatformInputSource> m_injected_source;
#ifdef __PROSPERO__
        bool m_source_injected_mode{true};
#else
        bool m_source_injected_mode{false};
#endif

      public:
        // Windows factory sources retain upstream main-thread selection semantics.
        // Injected providers must support serialized constructor-thread activation,
        // exclusive update-thread processing, and teardown after that thread joins.
        // Reconfiguration must never overlap process_event. Provider-specific thread
        // affinity must fit this lifecycle; the Handler does not marshal provider calls.
        // Registration in dispatched callbacks is supported by callback snapshotting.
        Handler() = default;
        ~Handler();
        Handler(const Handler&) = delete;
        Handler& operator=(const Handler&) = delete;

        // Input source and event processing
      public:
        auto set_input_source(std::string source) -> bool;
        auto process_event() -> void;

        // Interfaces for UE4SS and ModSystem for event registration
      public:
        auto init() -> void;
        // Explicit source injection selects only this handler's owned source. It
        // never discovers hardware or registers the Windows source factories.
        auto init(std::shared_ptr<PlatformInputSource> input_source) -> bool;
        auto is_input_available() const -> bool;
        // Call after the sole processing thread has stopped. False reports failed
        // provider deactivation (including an exception), retaining owner for retry.
        auto shutdown() noexcept -> bool;

        auto register_keydown_event(Input::Key, EventCallbackCallable, uint8_t custom_data = 0, void* custom_data2 = nullptr) -> void;

        using ModifierKeyArray = Input::ModifierKeyArray;
        auto register_keydown_event(Input::Key, const ModifierKeyArray&, const EventCallbackCallable&, uint8_t custom_data = 0, void* custom_data2 = nullptr) -> void;

        auto is_keydown_event_registered(Input::Key) -> bool;
        auto is_keydown_event_registered(Input::Key, const ModifierKeyArray&) -> bool;

        auto get_events_safe(std::function<void(KeySet&)>) -> void;
        auto clear_subscribed_keys() -> void;
        auto clear_subscribed_key(Key k) -> void;

        auto has_event_on_key(Input::Key key) -> bool;
        auto get_subscribed_keys() const -> const std::array<bool, max_keys>&
        {
            return m_subscribed_keys;
        }

        auto get_allow_input() -> bool;
        auto set_allow_input(bool new_value) -> void;

        auto get_current_input_source() -> std::string;

      private:
        auto activate_input_source(std::shared_ptr<PlatformInputSource> candidate) -> bool;
        static std::unordered_map<std::string, std::shared_ptr<PlatformInputSource>> m_input_sources_store;
        static auto register_input_source(std::shared_ptr<PlatformInputSource> input_source) -> void;

      public:
        static auto get_input_source(std::string source) -> std::shared_ptr<PlatformInputSource>
        {
            if (m_input_sources_store.find(source) != m_input_sources_store.end())
            {
                return m_input_sources_store[source];
            }
            return nullptr;
        }
    };
#else
    class Handler
    {
      public:
        using ModifierKeyArray = Input::ModifierKeyArray;
    };
#endif // HAS_INPUT
} // namespace RC::Input
