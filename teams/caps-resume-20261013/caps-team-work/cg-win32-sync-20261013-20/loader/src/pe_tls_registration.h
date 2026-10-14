// Prepare a relocated, writable mapped PE image's static TLS before guest
// entry. This does not execute callbacks or provide a guest GS/TEB.
#ifndef CAP13_PE_TLS_REGISTRATION_H
#define CAP13_PE_TLS_REGISTRATION_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pe_types.h"
#include "static_tls_bank.h"

namespace cap13 {

struct PeTlsRegistration
{
    bool ok = false;
    uint32_t index = UINT32_MAX;
    std::vector<uint64_t> callbacks;
    std::string reason;
};

// The caller must hold the loader's module-registration lock, pass a mapped
// image after base relocations, and keep it writable until this returns.
PeTlsRegistration register_mapped_static_tls(StaticTlsBank& bank,
                                             uint8_t* image, size_t image_size,
                                             const PeTlsDirectory64& tls);

} // namespace cap13
#endif
