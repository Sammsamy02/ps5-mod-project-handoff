// Internal mapper seam for guest-segment-backed PE preparation.
// Public pe_load remains the strict, no-GS/no-real-TLS path.
#ifndef CAP13_PE_LOADER_INTERNAL_H
#define CAP13_PE_LOADER_INTERNAL_H

#include "pe_loader.h"
#include "pe_types.h"

namespace cap13 {

struct PeMappedPreparation
{
    uint8_t* image = nullptr;
    size_t image_size = 0;
    const PeTlsDirectory64* tls = nullptr;
    const PeSectionHeader* sections = nullptr;
    uint16_t section_count = 0;
};

using PePrepareHook = bool (*)(const PeMappedPreparation&, std::string& reason, void* user);

LoadResult pe_load_internal(const void* file_bytes, size_t file_size,
                            ImportResolver& resolver, const LoadOptions& options,
                            bool guest_segment_guarded, PePrepareHook prepare,
                            void* prepare_user);

} // namespace cap13
#endif
