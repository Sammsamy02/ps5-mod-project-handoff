#pragma once
// Team I2 prototype (Mac-only, not in any runtime build).
//
// "Engine keys" input source = option C of DESIGN.md. Plain words: the game
// already owns the USB keyboard and mouse and turns them into Unreal key
// presses. We do not open the devices; once per game frame we ASK the game's
// own player controller "is key X down?" for the keys mods registered, and
// turn up->down changes into UE4SS key events.
//
// Two halves, one lock-free channel between them:
//   EngineKeySampler  game thread   (the only caller of KeyStateReader)
//   EngineKeySource   UE4SS update thread (a normal PlatformInputSource)
// The channel is single-producer (sampler) / single-consumer (source).
#include <Input/Handler.hpp>
#include <Input/PlatformInputSource.hpp>
#include <Input/RingBuffer.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace modbridge::kbm
{
    using RC::Input::InputEvent;
    using RC::Input::Key;
    using RC::Input::ModifierKeys;

    // Engine boundary, game thread only. The real reader (runtime work, NOT
    // in this prototype) finds the local PlayerController (cache rebuilt on
    // miss), finds its reflected IsInputKeyDown function and checks its
    // parameter layout by reflection; a missing function or an unexpected
    // layout = Unsupported (permanent, fail closed).
    class KeyStateReader
    {
      public:
        enum class Frame
        {
            Ready,        // controller found, function usable
            NoController, // loading screen / no local player yet: try later
            Unsupported,  // this engine build cannot answer: stop for good
        };
        struct FrameInfo
        {
            Frame status;
            uint64_t controller_generation; // changes when the controller object changes
        };
        virtual ~KeyStateReader() = default;
        virtual FrameInfo begin_frame() = 0;
        // nullopt = this one read failed (unknown key name in this game, call failed)
        virtual std::optional<bool> is_down(const char* fkey_name) = 0;
    };

    struct EngineKeyChannel
    {
        static constexpr int ring_slots = 256; // RingBufferSPSC holds slots-1 events
        RC::Input::RingBufferSPSC<InputEvent, ring_slots> ring;
        std::array<std::atomic<uint64_t>, 4> subscribed{}; // bit k = UE4SS key k has a bind
        std::atomic<bool> open{true};                      // false after deactivate
        std::atomic<int> support{0};                       // 0 unknown, 1 yes, -1 no (permanent)
        std::atomic<uint32_t> divisor{1};                  // sample every Nth frame
        // Diagnostics (monotonic counters).
        std::atomic<uint64_t> frames{}, sampled_frames{}, reads{}, read_failures{};
        std::atomic<uint64_t> events{}, drops{}, throttle_steps{};
    };

    struct SamplerConfig
    {
        uint64_t budget_ns = 250'000;    // sampling cost allowed per sampled frame
        uint32_t over_budget_frames = 3; // consecutive overruns before slowing down
        uint32_t max_divisor = 8;        // never slower than every 8th frame
        uint32_t max_keys_per_frame = 64; // above this, keys are sampled round-robin
    };

    class EngineKeySampler
    {
      public:
        EngineKeySampler(std::shared_ptr<EngineKeyChannel> channel, std::unique_ptr<KeyStateReader> reader,
                         SamplerConfig config = {}, std::function<uint64_t()> now_ns = {});
        void tick(); // once per game frame, game thread only

      private:
        enum class State : uint8_t
        {
            Unknown, // no baseline yet: the next read never produces an event
            Up,
            Down,
        };
        void reset_baseline();
        ModifierKeys read_modifiers();
        void account(uint64_t cost_ns);

        std::shared_ptr<EngineKeyChannel> m_channel;
        std::unique_ptr<KeyStateReader> m_reader;
        SamplerConfig m_config;
        std::function<uint64_t()> m_now;
        std::array<State, 256> m_state{};
        std::vector<Key> m_work;
        uint64_t m_generation{~0ull};
        uint32_t m_phase{};
        uint32_t m_round_robin{};
        uint32_t m_over{};
    };

    class EngineKeySource final : public RC::Input::PlatformInputSource
    {
      public:
        explicit EngineKeySource(std::shared_ptr<EngineKeyChannel> channel);
        bool is_available() override; // true once the engine answered at least once
        bool activate() override;     // nothing to open: (re)opens the channel
        bool deactivate() override;   // closes the channel; the sampler goes idle
        int source_priority() override { return 90; }
        const char* get_name() override { return "EngineKeys"; }
        std::vector<InputEvent>& process_event(RC::Input::Handler* handler) override;

      private:
        std::shared_ptr<EngineKeyChannel> m_channel;
        std::vector<InputEvent> m_events;
    };
} // namespace modbridge::kbm
