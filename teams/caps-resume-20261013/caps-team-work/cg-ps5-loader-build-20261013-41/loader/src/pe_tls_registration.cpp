#include "pe_tls_registration.h"

#include <cstring>
#include <exception>
#include <new>
#include <utility>

namespace cap13 {

namespace {

bool mapped_offset(uint8_t* image, size_t image_size, uint64_t va,
                   size_t bytes, size_t& offset)
{
    if (!image) return false;
    const uint64_t base = reinterpret_cast<uint64_t>(image);
    if (va < base) return false;
    const uint64_t delta = va - base;
    if (delta > image_size || bytes > image_size - delta) return false;
    offset = static_cast<size_t>(delta);
    return true;
}

} // namespace

PeTlsRegistration register_mapped_static_tls(StaticTlsBank& bank,
                                             uint8_t* image, size_t image_size,
                                             const PeTlsDirectory64& tls,
                                             TlsExecutableTarget executable_target,
                                             void* executable_user)
{
    PeTlsRegistration result;
    size_t template_off = 0;
    size_t index_off = 0;
    if (tls.EndAddressOfRawData < tls.StartAddressOfRawData ||
        tls.EndAddressOfRawData - tls.StartAddressOfRawData > image_size ||
        !mapped_offset(image, image_size, tls.StartAddressOfRawData,
                       static_cast<size_t>(tls.EndAddressOfRawData - tls.StartAddressOfRawData),
                       template_off))
    {
        result.reason = "TLS template lies outside relocated mapped image";
        return result;
    }
    if (!mapped_offset(image, image_size, tls.AddressOfIndex,
                       sizeof(uint32_t), index_off) ||
        index_off % alignof(uint32_t) != 0)
    {
        result.reason = "TLS index target lies outside mapped image or is unaligned";
        return result;
    }

    std::vector<uint64_t> callbacks;
    if (tls.AddressOfCallBacks)
    {
        size_t table_off = 0;
        if (!mapped_offset(image, image_size, tls.AddressOfCallBacks,
                           sizeof(uint64_t), table_off) ||
            table_off % alignof(uint64_t) != 0)
        {
            result.reason = "TLS callback table lies outside mapped image or is unaligned";
            return result;
        }
        bool terminated = false;
        for (size_t off = table_off; off <= image_size - sizeof(uint64_t);
             off += sizeof(uint64_t))
        {
            uint64_t callback = 0;
            std::memcpy(&callback, image + off, sizeof(callback));
            if (!callback) { terminated = true; break; }
            size_t callback_off = 0;
            if (!mapped_offset(image, image_size, callback, 1, callback_off))
            {
                result.reason = "TLS callback target lies outside mapped image";
                return result;
            }
            bool executable = false;
            try
            {
                executable = executable_target &&
                    executable_target(callback, executable_user);
            }
            catch (...)
            {
                result.reason = "TLS callback executable validation failed";
                return result;
            }
            if (!executable)
            {
                result.reason = "TLS callback target is not file-backed executable code";
                return result;
            }
            try { callbacks.push_back(callback); }
            catch (const std::bad_alloc&)
            {
                result.reason = "TLS callback table allocation failed";
                return result;
            }
        }
        if (!terminated)
        {
            result.reason = "TLS callback table has no in-image terminator";
            return result;
        }
    }

    try
    {
        const size_t template_size =
            static_cast<size_t>(tls.EndAddressOfRawData - tls.StartAddressOfRawData);
        std::vector<uint8_t> initial(image + template_off, image + template_off + template_size);
        result.index = bank.register_module(initial, tls.SizeOfZeroFill);
    }
    catch (const std::exception& error)
    {
        result.reason = std::string("TLS module registration failed: ") + error.what();
        return result;
    }
    std::memcpy(image + index_off, &result.index, sizeof(result.index));
    result.callbacks = std::move(callbacks);
    result.ok = true;
    return result;
}

// Registration alone does not run callbacks. The guarded PE loader validates
// executable callback targets and routes attachment through the guest TEB
// lifecycle; the public unguarded pe_load route still refuses real TLS.

}
