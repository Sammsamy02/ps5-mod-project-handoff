#pragma once
// HOST-TEST SHIM — Unreal/FName.hpp (worker-l11-enqueue-ladder).
// Default-constructible stand-in: the marshal_state region only declares
// `Unreal::FName m_funcptr_marshal_carrier_id{}` (stored, never compared on
// the host path the harness drives).

#include <cstdint>

namespace RC
{
    namespace Unreal
    {
        struct FName
        {
            int32_t comparison_index{0};
        };
    } // namespace Unreal
} // namespace RC
