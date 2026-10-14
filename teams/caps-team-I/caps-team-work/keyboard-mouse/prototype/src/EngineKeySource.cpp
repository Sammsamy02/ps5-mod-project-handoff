// Team I2 prototype (Mac-only, not in any runtime build). See EngineKeySource.hpp.
#include <kbm/EngineKeySource.hpp>
#include <kbm/KeyboardMouseMap.hpp>

#include <algorithm>
#include <chrono>

namespace modbridge::kbm
{
    namespace
    {
        uint64_t steady_now_ns()
        {
            return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }
    } // namespace

    EngineKeySampler::EngineKeySampler(std::shared_ptr<EngineKeyChannel> channel, std::unique_ptr<KeyStateReader> reader,
                                       SamplerConfig config, std::function<uint64_t()> now_ns)
        : m_channel(std::move(channel)), m_reader(std::move(reader)), m_config(config),
          m_now(now_ns ? std::move(now_ns) : std::function<uint64_t()>(steady_now_ns))
    {
        m_work.reserve(RC::Input::max_keys);
        if (m_config.max_divisor == 0) m_config.max_divisor = 1;
        if (m_config.max_keys_per_frame == 0) m_config.max_keys_per_frame = 1;
        // No reader = this route can never answer: fail closed.
        if (!m_reader && m_channel) m_channel->support.store(-1, std::memory_order_release);
    }

    void EngineKeySampler::reset_baseline()
    {
        // Every key goes back to "no baseline": a key that is already held when
        // sampling (re)starts never produces an event (no phantom presses after
        // a level load, a controller change or a loading screen).
        m_state.fill(State::Unknown);
    }

    ModifierKeys EngineKeySampler::read_modifiers()
    {
        auto& ch = *m_channel;
        ModifierKeys mods{};
        for (const auto& row : modifier_table())
        {
            const auto left = m_reader->is_down(row.fkey_left);
            const auto right = m_reader->is_down(row.fkey_right);
            ch.reads.fetch_add(2, std::memory_order_relaxed);
            if (!left) ch.read_failures.fetch_add(1, std::memory_order_relaxed);
            if (!right) ch.read_failures.fetch_add(1, std::memory_order_relaxed);
            // A failed modifier read counts as "not held" (never invents a modifier).
            if (left.value_or(false) || right.value_or(false)) mods |= row.modifier;
        }
        return mods;
    }

    void EngineKeySampler::account(uint64_t cost_ns)
    {
        auto& ch = *m_channel;
        if (cost_ns <= m_config.budget_ns)
        {
            m_over = 0;
            return;
        }
        if (++m_over < m_config.over_budget_frames) return;
        m_over = 0;
        const uint32_t current = ch.divisor.load(std::memory_order_relaxed);
        if (current >= m_config.max_divisor) return;
        ch.divisor.store(std::min(current * 2, m_config.max_divisor), std::memory_order_relaxed);
        ch.throttle_steps.fetch_add(1, std::memory_order_relaxed);
    }

    void EngineKeySampler::tick()
    {
        if (!m_channel) return;
        auto& ch = *m_channel;
        ch.frames.fetch_add(1, std::memory_order_relaxed);
        if (!ch.open.load(std::memory_order_acquire) || ch.support.load(std::memory_order_acquire) < 0) return;
        if (++m_phase < ch.divisor.load(std::memory_order_relaxed)) return;
        m_phase = 0;

        const uint64_t t0 = m_now();
        const auto info = m_reader->begin_frame();
        if (info.status == KeyStateReader::Frame::Unsupported)
        {
            ch.support.store(-1, std::memory_order_release);
            reset_baseline();
            return;
        }
        if (info.status == KeyStateReader::Frame::NoController)
        {
            reset_baseline();
            m_generation = ~0ull;
            return;
        }
        if (info.controller_generation != m_generation)
        {
            reset_baseline();
            m_generation = info.controller_generation;
        }
        int unknown = 0;
        ch.support.compare_exchange_strong(unknown, 1, std::memory_order_acq_rel);
        ch.sampled_frames.fetch_add(1, std::memory_order_relaxed);

        // Only keys a mod actually registered, and only keys the engine has a name for.
        m_work.clear();
        for (unsigned word = 0; word < ch.subscribed.size(); ++word)
        {
            const uint64_t bits = ch.subscribed[word].load(std::memory_order_acquire);
            for (unsigned bit = 0; bit < 64; ++bit)
            {
                const unsigned value = word * 64 + bit;
                if (value == 0 || value >= RC::Input::max_keys) continue;
                const Key key = static_cast<Key>(value);
                if (!((bits >> bit) & 1u) || !fkey_for(key))
                {
                    m_state[value] = State::Unknown;
                    continue;
                }
                m_work.push_back(key);
            }
        }
        if (m_work.empty())
        {
            account(m_now() - t0);
            return;
        }

        size_t count = m_work.size();
        size_t start = 0;
        if (count > m_config.max_keys_per_frame)
        {
            start = m_round_robin % count;
            m_round_robin = static_cast<uint32_t>((start + m_config.max_keys_per_frame) % count);
            count = m_config.max_keys_per_frame;
        }

        const ModifierKeys mods = read_modifiers();
        for (size_t i = 0; i < count; ++i)
        {
            const Key key = m_work[(start + i) % m_work.size()];
            const auto down = m_reader->is_down(fkey_for(key));
            ch.reads.fetch_add(1, std::memory_order_relaxed);
            if (!down)
            {
                ch.read_failures.fetch_add(1, std::memory_order_relaxed);
                continue; // keep the previous state; no event from a failed read
            }
            State& state = m_state[static_cast<uint8_t>(key)];
            if (state == State::Up && *down)
            {
                // Key-down edge only (UE4SS RegisterKeyBind fires on press, no auto-repeat).
                if (ch.ring.push(InputEvent{key, mods}))
                    ch.events.fetch_add(1, std::memory_order_relaxed);
                else
                    ch.drops.fetch_add(1, std::memory_order_relaxed);
            }
            state = *down ? State::Down : State::Up;
        }
        account(m_now() - t0);
    }

    EngineKeySource::EngineKeySource(std::shared_ptr<EngineKeyChannel> channel) : m_channel(std::move(channel))
    {
        m_events.reserve(EngineKeyChannel::ring_slots);
    }

    bool EngineKeySource::is_available()
    {
        return m_channel && m_channel->open.load(std::memory_order_acquire) &&
               m_channel->support.load(std::memory_order_acquire) == 1;
    }

    bool EngineKeySource::activate()
    {
        if (!m_channel) return false;
        m_channel->open.store(true, std::memory_order_release);
        return is_available();
    }

    bool EngineKeySource::deactivate()
    {
        m_events.clear();
        if (m_channel) m_channel->open.store(false, std::memory_order_release);
        return true;
    }

    std::vector<InputEvent>& EngineKeySource::process_event(RC::Input::Handler* handler)
    {
        m_events.clear();
        if (!handler || !m_channel) return m_events;
        auto& ch = *m_channel;

        // Publish which keys have binds, so the game-thread sampler asks the
        // engine about those keys only.
        const auto& subscribed = handler->get_subscribed_keys();
        std::array<uint64_t, 4> words{};
        for (size_t value = 0; value < subscribed.size(); ++value)
            if (subscribed[value]) words[value / 64] |= 1ull << (value % 64);
        for (size_t word = 0; word < words.size(); ++word) ch.subscribed[word].store(words[word], std::memory_order_release);

        // Always drain (never let stale presses pile up); deliver only when allowed.
        const bool deliver = ch.open.load(std::memory_order_acquire) && handler->get_allow_input();
        while (auto event = ch.ring.pop())
        {
            const auto value = static_cast<size_t>(event->key);
            if (!deliver || value >= subscribed.size() || !subscribed[value]) continue;
            m_events.push_back(*event);
        }
        return m_events;
    }
} // namespace modbridge::kbm
