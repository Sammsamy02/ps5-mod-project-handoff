#pragma once

#ifndef _WIN32
#include <Helpers/UETargetModules.hpp>
#include <SigScanner/SinglePassSigScanner.hpp>
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RC::Unreal::Platform
{
    struct ModuleImage
    {
        const uint8_t* base{};
        size_t size{};
        std::vector<ReadableRange> readable_ranges;
    };

    struct ModuleInput
    {
        // This owner must actually retain all described bytes and resolver state.
        // Validation cannot establish mapping permissions or engine identity.
        std::shared_ptr<const void> lifetime;
        bool monolithic{};
        std::unordered_map<ScanTarget, ModuleImage> images;
        std::function<void*(std::string_view)> find_export;
    };

    class ModuleSnapshot final
    {
    public:
        static auto Create(ModuleInput input) -> std::shared_ptr<const ModuleSnapshot>
        {
            return std::shared_ptr<const ModuleSnapshot>{new ModuleSnapshot(std::move(input))};
        }
        ModuleSnapshot(const ModuleSnapshot&) = delete;
        auto operator=(const ModuleSnapshot&) -> ModuleSnapshot& = delete;
        ModuleSnapshot(ModuleSnapshot&&) = delete;
        auto operator=(ModuleSnapshot&&) -> ModuleSnapshot& = delete;

        auto Views() const noexcept -> const ScanTargetArray& { return m_views; }
        auto ReadableRanges() const noexcept -> const SinglePassScanner::ReadableRangeMap& { return m_ranges; }
        auto IsMonolithic() const noexcept -> bool { return m_input.monolithic; }
        auto FindExport(std::string_view name) const -> void*
        {
            if (!m_input.find_export)
                throw std::logic_error("Native module export resolver is not configured");
            if (name.empty() || name.find('\0') != std::string_view::npos)
                throw std::invalid_argument("Export name must be nonempty and contain no NUL");
            return m_input.find_export(name);
        }

    private:
        ModuleInput m_input;
        ScanTargetArray m_views;
        SinglePassScanner::ReadableRangeMap m_ranges;

        static auto End(const uint8_t* base, size_t size) -> uintptr_t
        {
            const auto begin = reinterpret_cast<uintptr_t>(base);
            if (!base || !size || size > std::numeric_limits<uintptr_t>::max() - begin)
                throw std::invalid_argument("Module or readable range has an invalid extent");
            return begin + size;
        }

        explicit ModuleSnapshot(ModuleInput input) : m_input(std::move(input))
        {
            if (!m_input.lifetime || !m_input.images.contains(ScanTarget::MainExe))
                throw std::invalid_argument("Native modules require a lifetime owner and MainExe image");
            if (m_input.monolithic && m_input.images.size() != 1)
                throw std::invalid_argument("A monolithic snapshot must describe only MainExe");
            for (auto& [target, module] : m_input.images)
            {
                if (static_cast<size_t>(target) >= static_cast<size_t>(ScanTarget::Max))
                    throw std::invalid_argument("Invalid module scan target");
                const auto image_begin = reinterpret_cast<uintptr_t>(module.base);
                const auto image_end = End(module.base, module.size);
                if (module.readable_ranges.empty())
                    throw std::invalid_argument("Module has no supplied readable ranges");
                std::sort(module.readable_ranges.begin(), module.readable_ranges.end(),
                    [](const ReadableRange& a, const ReadableRange& b) {
                        return reinterpret_cast<uintptr_t>(a.data) < reinterpret_cast<uintptr_t>(b.data);
                    });
                auto previous_end = image_begin;
                for (const auto& range : module.readable_ranges)
                {
                    const auto begin = reinterpret_cast<uintptr_t>(range.data);
                    const auto end = End(range.data, range.size);
                    if (begin < previous_end || end > image_end)
                        throw std::invalid_argument("Readable ranges overlap or lie outside their module");
                    previous_end = end;
                }
            }
            // Publish only after every module passed validation. The input's
            // vectors remain immutable for this snapshot's entire lifetime.
            for (const auto& [target, module] : m_input.images)
            {
                auto publish = [&](ScanTarget destination) {
                    m_views.array[static_cast<size_t>(destination)] = {module.base, module.size, module.readable_ranges};
                    m_ranges.emplace(destination, module.readable_ranges);
                };
                if (m_input.monolithic)
                    for (size_t i = 0; i < static_cast<size_t>(ScanTarget::Max); ++i)
                        publish(static_cast<ScanTarget>(i));
                else
                    publish(target);
            }
        }
    };
}
#endif
