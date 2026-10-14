#pragma once
#include <cstdint>

namespace RC::Unreal::NativeAssetLoadingMemory
{
    enum class BudgetCheck { Disabled, InvalidPercentage, MonitorUnavailable };

    // Upstream uses GlobalMemoryStatusEx.dwMemoryLoad, a system-pressure percentage,
    // NOT total bytes or a per-process RSS limit. The native SDK does not provide a
    // verified equivalent. getrusage.ru_maxrss is a peak, not current WorkingSetSize.
    // Refuse a monitored load instead of inventing usage or reporting availability.
    constexpr auto CheckBudget(std::uint64_t maximum_percentage) -> BudgetCheck
    {
        if (maximum_percentage == 0) { return BudgetCheck::Disabled; }
        if (maximum_percentage > 100) { return BudgetCheck::InvalidPercentage; }
        return BudgetCheck::MonitorUnavailable;
    }

    constexpr auto MayLoad(BudgetCheck check) -> bool
    {
        // Zero explicitly disables the upstream budget. It does not mean that a
        // monitor is available or that memory pressure is safe.
        return check == BudgetCheck::Disabled;
    }
}
