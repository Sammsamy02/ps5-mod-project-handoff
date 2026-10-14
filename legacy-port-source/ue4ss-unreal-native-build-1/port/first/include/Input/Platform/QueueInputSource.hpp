#pragma once

#include <Input/PlatformInputSource.hpp>
#include <Input/KeyDef.hpp>
#include <Input/RingBuffer.hpp>

namespace RC::Input
{
    class QueueInputSource : public PlatformInputSource
    {
      private:
        static constexpr int max_inputs = 256;
        RingBufferSPSC<InputEvent, max_inputs> m_input_queue;

      protected:
        std::atomic<bool> m_activated{false};

      private:
        /// SAFETY: Only update and return m_input_events
        /// in process_event and flush_events functions
        std::vector<InputEvent> m_input_events;

      public:
        ~QueueInputSource() = default;

        // QueueInputSource has no producer and is not an implemented input source
        // and should not be used directly
        bool is_available() override
        {
            return false;
        };

        bool activate() override
        {
            return m_activated = is_available();
        };

        bool deactivate() override
        {
            m_activated = false;
            return true;
        };

        std::vector<InputEvent>& process_event(Handler* handler) override;

        int source_priority() override
        {
            return 0;
        }

        const char* get_name() override
        {
            return "Queue";
        }

      public:
        // One provider producer thread may call push_input_event. The main thread
        // is the sole consumer through process_event. Multiple producers require
        // serialization by the provider. Overflow intentionally drops the event.
        auto push_input_event(const InputEvent& event) -> void;
    };

}; // namespace RC::Input