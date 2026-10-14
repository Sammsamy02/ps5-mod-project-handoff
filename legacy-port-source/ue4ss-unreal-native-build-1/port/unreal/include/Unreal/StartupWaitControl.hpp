#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>

namespace RC::Unreal::UnrealInitializer
{
    enum class StartupWaitStatus
    {
        Continue,
        Cancelled,
        DeadlineExceeded,
    };

    class StartupCancelled final : public std::runtime_error
    {
    public:
        StartupCancelled() : std::runtime_error{"Unreal initialization cancelled"} {}
    };

    class StartupDeadlineExceeded final : public std::runtime_error
    {
    public:
        StartupDeadlineExceeded() : std::runtime_error{"Unreal initialization startup deadline exceeded"} {}
    };

    // Optional cooperative bound for startup readiness waits. The owner creates
    // one immutable control before Initialize and shares its absolute deadline
    // across all retries. Cancellation/deadline checks run between opaque
    // engine calls; they cannot interrupt an engine call already in progress.
    struct StartupWaitControl
    {
        using Clock = std::chrono::steady_clock;
        std::function<bool()> IsCancelled;
        Clock::time_point Deadline{};
        std::chrono::milliseconds PollInterval{10};

        auto StatusAt(Clock::time_point now) const -> StartupWaitStatus
        {
            if (IsCancelled && IsCancelled())
            {
                return StartupWaitStatus::Cancelled;
            }
            return now >= Deadline ? StartupWaitStatus::DeadlineExceeded : StartupWaitStatus::Continue;
        }

        auto ThrowIfStoppedAt(Clock::time_point now) const -> void
        {
            switch (StatusAt(now))
            {
            case StartupWaitStatus::Continue:
                return;
            case StartupWaitStatus::Cancelled:
                throw StartupCancelled{};
            case StartupWaitStatus::DeadlineExceeded:
                throw StartupDeadlineExceeded{};
            }
        }

        auto RetrySleepForAt(Clock::time_point now, Clock::duration requested) const -> Clock::duration
        {
            if (StatusAt(now) != StartupWaitStatus::Continue || requested <= Clock::duration::zero())
            {
                return Clock::duration::zero();
            }

            auto sleep = requested;
            if (PollInterval > std::chrono::milliseconds::zero())
            {
                sleep = std::min(sleep, std::chrono::duration_cast<Clock::duration>(PollInterval));
            }
            return std::min(sleep, Deadline - now);
        }

        template <typename Now, typename Sleep>
        auto WaitForRetry(std::chrono::milliseconds requested, Now&& now, Sleep&& sleep) const -> void
        {
            const auto retry_deadline = now() + std::chrono::duration_cast<Clock::duration>(requested);
            while (true)
            {
                const auto current = now();
                ThrowIfStoppedAt(current);
                const auto remaining = retry_deadline - current;
                if (remaining <= Clock::duration::zero())
                {
                    return;
                }

                const auto slice = RetrySleepForAt(current, remaining);
                if (slice <= Clock::duration::zero())
                {
                    ThrowIfStoppedAt(current);
                    return;
                }
                sleep(slice);
            }
        }
    };

    inline constexpr std::chrono::minutes DefaultStartupWaitTimeout{5};
}
