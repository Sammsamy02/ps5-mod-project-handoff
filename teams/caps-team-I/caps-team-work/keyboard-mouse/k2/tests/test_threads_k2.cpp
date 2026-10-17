// K2 host stress test (Mac-only, Team I2), built with ThreadSanitizer by ./run.sh against the SHIPPING header.
// Two real threads, like on the console:
//   "game thread"   : EngineKeySampler::tick() (sole producer, sole reader user)
//   "update thread" : real Handler::process_event() -> Merged -> EngineKeySource
// Checks: no data race reported, every press delivered exactly once or counted as dropped, nothing delivered after
// shutdown, and the retirement flag (channel closed) stops the sampler.
#include <Platform/KbmEngineKeys.hpp>

#include <Input/Handler.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>

using namespace RC::Platform::Kbm;

extern "C" void diagnostic_event(const char*, unsigned int) {}

namespace
{
    // Toggles F8 every other frame; counts the down edges it shows the sampler.
    class TogglingReader final : public KeyStateReader
    {
      public:
        std::atomic<uint64_t> shown_presses{0};
        std::atomic<uint64_t> begins{0};
        FrameInfo begin_frame() override
        {
            ++m_frame;
            begins.fetch_add(1, std::memory_order_relaxed);
            return {Frame::Ready, 1};
        }
        std::optional<bool> is_down(uint16_t, const char* name) override
        {
            if (std::string_view(name) != "F8") return false;
            const bool down = (m_frame / 2) % 2 == 1;
            if (down && !m_last) shown_presses.fetch_add(1, std::memory_order_relaxed);
            m_last = down;
            return down;
        }

      private:
        uint64_t m_frame{};
        bool m_last{};
    };
} // namespace

int main()
{
    auto channel = std::make_shared<EngineKeyChannel>();
    auto reader = std::make_unique<TogglingReader>();
    TogglingReader* reader_view = reader.get();
    SamplerConfig cfg;
    cfg.min_interval_ns = 0; // stress: sample on every pump call
    std::atomic<uint64_t> log_lines{0};
    EngineKeySampler sampler(channel, std::move(reader), cfg, {}, [&](const char*) { log_lines.fetch_add(1); });
    auto merged = make_merged_input(nullptr, channel);

    RC::Input::Handler handler;
    if (!handler.init(merged)) return 2;
    std::atomic<uint64_t> fired{0};
    handler.register_keydown_event(RC::Input::Key::F8, [&] { fired.fetch_add(1, std::memory_order_relaxed); });
    handler.process_event(); // publish subscriptions before the game thread starts

    std::atomic<bool> stop{false};
    std::thread update([&] {
        while (!stop.load(std::memory_order_acquire))
        {
            handler.process_event();
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        handler.process_event(); // final drain
    });
    std::thread game([&] {
        for (int i = 0; i < 40000; ++i) sampler.tick();
    });
    game.join();
    stop.store(true, std::memory_order_release);
    update.join();

    const uint64_t shown = reader_view->shown_presses.load();
    const uint64_t events = channel->events.load(), drops = channel->drops.load();
    const uint64_t delivered = fired.load();
    // The first shown press may fall on the baseline frame and is not an edge.
    const bool accounted = (events + drops == shown || events + drops + 1 == shown) && delivered == events;

    handler.shutdown(); // closes the channel (teardown step 1); the sampler must go idle
    const uint64_t begins_at_shutdown = reader_view->begins.load();
    for (int i = 0; i < 10; ++i) sampler.tick();
    handler.process_event();
    const bool silent_after_shutdown = fired.load() == delivered && reader_view->begins.load() == begins_at_shutdown;

    std::printf("threads: ticks=40000 shown=%llu events=%llu drops=%llu delivered=%llu accounted=%d silent_after_shutdown=%d log_lines=%llu\n",
                (unsigned long long)shown, (unsigned long long)events, (unsigned long long)drops, (unsigned long long)delivered,
                accounted ? 1 : 0, silent_after_shutdown ? 1 : 0, (unsigned long long)log_lines.load());
    if (shown == 0)
    {
        std::printf("threads: FAIL (nothing exercised)\n");
        return 3;
    }
    const bool ok = accounted && silent_after_shutdown && log_lines.load() == 1;
    std::printf("threads: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
