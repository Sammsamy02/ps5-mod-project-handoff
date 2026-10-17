// Team I2 prototype (Mac-only, not in any runtime build). See CompositeInputSource.hpp.
#include <kbm/CompositeInputSource.hpp>

#include <algorithm>

namespace modbridge::kbm
{
    using RC::Input::InputEvent;
    using RC::Input::PlatformInputSource;

    CompositeInputSource::CompositeInputSource(std::vector<std::shared_ptr<PlatformInputSource>> children)
    {
        for (auto& child : children)
            if (child) m_children.push_back(std::move(child));
        m_events.reserve(256);
    }

    bool CompositeInputSource::is_available()
    {
        for (auto& child : m_children)
        {
            try
            {
                if (child->is_available()) return true;
            }
            catch (...)
            {
                ++m_child_exceptions;
            }
        }
        return false;
    }

    bool CompositeInputSource::activate()
    {
        bool any = false;
        for (auto& child : m_children)
        {
            try
            {
                // Only children that are available now; the others stay attached
                // and keep retrying from their own poll path.
                if (child->is_available() && child->activate()) any = true;
            }
            catch (...)
            {
                ++m_child_exceptions;
            }
        }
        return any;
    }

    bool CompositeInputSource::deactivate()
    {
        bool all = true;
        m_events.clear();
        for (auto& child : m_children)
        {
            try
            {
                if (!child->deactivate()) all = false; // child keeps its owner for a retry
            }
            catch (...)
            {
                ++m_child_exceptions;
                all = false;
            }
        }
        return all;
    }

    int CompositeInputSource::source_priority()
    {
        int best = 999;
        for (auto& child : m_children) best = std::min(best, child->source_priority());
        return best;
    }

    std::vector<InputEvent>& CompositeInputSource::process_event(RC::Input::Handler* handler)
    {
        m_events.clear();
        for (auto& child : m_children)
        {
            try
            {
                // Merge only ACROSS children: two presses queued by the same child
                // (e.g. after an update-loop stall) are two presses, as on PC.
                const auto earlier = static_cast<std::ptrdiff_t>(m_events.size());
                for (const auto& event : child->process_event(handler))
                {
                    const bool duplicate = std::any_of(m_events.begin(), m_events.begin() + earlier, [&](const InputEvent& seen) {
                        return seen.key == event.key && seen.modifier_keys == event.modifier_keys;
                    });
                    if (duplicate)
                    {
                        ++m_duplicates;
                        continue;
                    }
                    m_events.push_back(event);
                }
            }
            catch (...)
            {
                ++m_child_exceptions; // isolate: one broken child never silences the others
            }
        }
        return m_events;
    }
} // namespace modbridge::kbm
