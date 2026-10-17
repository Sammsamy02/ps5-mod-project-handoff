// Platform-neutral PE static TLS blocks. A guest TEB backend must publish
// the returned pointer array at Windows GS:[0x58] before entering guest code.
#ifndef CAP13_STATIC_TLS_BANK_H
#define CAP13_STATIC_TLS_BANK_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cap13 {

class StaticTlsBank
{
  public:
    struct ThreadArray
    {
        std::vector<void*> slots;
        uint64_t epoch = 0;
    };

    StaticTlsBank();
    ~StaticTlsBank();
    StaticTlsBank(const StaticTlsBank&) = delete;
    StaticTlsBank& operator=(const StaticTlsBank&) = delete;

    // Retained PE modules receive monotonic indices. The caller writes the
    // returned index to the mapped image's AddressOfIndex location.
    uint32_t register_module(const std::vector<uint8_t>& initial, size_t zero_fill);

    // Call on every host thread before it enters any guest code, including
    // after late module registration. The caller owns the returned array and
    // must publish it through a separate guest TEB backend.
    ThreadArray attach_current_thread();
    uint8_t* current_block(uint32_t index) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace cap13
#endif
