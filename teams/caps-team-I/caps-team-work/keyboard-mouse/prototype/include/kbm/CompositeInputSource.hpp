#pragma once
// Team I2 prototype (Mac-only, not in any runtime build).
//
// The UE4SS key-bind handler takes ONE input source. This source holds
// several (today's controller-chord source + the engine-keys source) and
// merges their key events. Plain words: the controller keeps working exactly
// as today, and real keys are added next to it. If the keyboard route is not
// available, only the controller events arrive (the fallback is automatic).
//
// Rules:
//   - every child is polled on every call (each child keeps doing its own
//     retries, e.g. the pad's borrowed-handle retry);
//   - the same key+modifiers from two DIFFERENT children in one poll is
//     delivered once (pad chord F7 + keyboard F7 = one press);
//   - a child that throws is skipped for that poll and counted; it never
//     stops the other children;
//   - available = at least one child available; activation never fails just
//     because one child could not activate (startup must never abort on input).
#include <Input/Handler.hpp>
#include <Input/PlatformInputSource.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace modbridge::kbm
{
    class CompositeInputSource final : public RC::Input::PlatformInputSource
    {
      public:
        explicit CompositeInputSource(std::vector<std::shared_ptr<RC::Input::PlatformInputSource>> children);
        bool is_available() override;
        bool activate() override;
        bool deactivate() override;
        int source_priority() override;
        const char* get_name() override { return "Merged"; }
        std::vector<RC::Input::InputEvent>& process_event(RC::Input::Handler* handler) override;

        uint64_t child_exceptions() const { return m_child_exceptions; }
        uint64_t duplicates_merged() const { return m_duplicates; }

      private:
        std::vector<std::shared_ptr<RC::Input::PlatformInputSource>> m_children;
        std::vector<RC::Input::InputEvent> m_events;
        uint64_t m_child_exceptions{};
        uint64_t m_duplicates{};
    };
} // namespace modbridge::kbm
