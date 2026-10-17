// Team I2 host stress test (Mac-only), built with ThreadSanitizer by ../run.sh.
// Two real threads, like on the console:
//   "game thread"   : EngineKeySampler::tick() (sole producer, sole reader user)
//   "update thread" : real Handler::process_event() -> Merged -> EngineKeySource
// Checks: no data race reported, every press delivered exactly once or counted
// as dropped, and nothing delivered after shutdown.
#include <kbm/CompositeInputSource.hpp>
#include <kbm/EngineKeySource.hpp>

#include <Input/Handler.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace modbridge::kbm;

extern "C" void diagnostic_event(const char*, unsigned int) {}

namespace
{
    // Toggles F8 every other frame; counts the down edges it shows the sampler.
    class TogglingReader final : public KeyStateReader
    {
      public:
        std::atomic<uint64_t> shown_presses{0};
        FrameInfo begin_frame() override
        {
            ++m_frame;
            return {Frame::Ready, 1};
        }
        std::optional<bool> is_down(const char* name) override
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
    EngineKeySampler sampler(channel, std::move(reader));
    auto merged = std::make_shared<CompositeInputSource>(
            std::vector<std::shared_ptr<RC::Input::PlatformInputSource>>{std::make_shared<EngineKeySource>(channel)});

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

    handler.shutdown();
    for (int i = 0; i < 10; ++i) sampler.tick();
    handler.process_event();
    const bool silent_after_shutdown = fired.load() == delivered;

    std::printf("threads: shown=%llu events=%llu drops=%llu delivered=%llu accounted=%d silent_after_shutdown=%d\n",
                (unsigned long long)shown, (unsigned long long)events, (unsigned long long)drops,
                (unsigned long long)delivered, accounted ? 1 : 0, silent_after_shutdown ? 1 : 0);
    if (shown == 0) return 3; // nothing exercised = failure
    return accounted && silent_after_shutdown ? 0 : 1;
}
