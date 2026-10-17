#include <cstddef>
#include <Unreal/UObjectGlobals.hpp>

// Contract derived from the official 5.6.1 declaration and saved native geometry.
// Removing callback alignment must fail this real-header test even though its
// member offsets and size remain unchanged.
using Params = RC::Unreal::UObjectGlobals::Above55::FStaticConstructObjectParameters;
static_assert(sizeof(RC::Unreal::FName) == 8, "selected FName width");
static_assert(offsetof(Params, PropertyInitCallback) == 0x40, "callback offset");
static_assert(offsetof(Params, SerialNumber) == 0x70, "serial offset");
static_assert(offsetof(Params, SubobjectOverrides) == 0x78, "subobject offset");
static_assert(sizeof(Params) == 0x80, "SCO parameter size");
static_assert(alignof(Params) == 16, "SCO parameter alignment must be 16");

extern "C" void l22_consume_sco_params(const void*);
extern "C" void l22_sco_caller_alignment_probe()
{
    Params parameters{};
    l22_consume_sco_params(&parameters);
}
