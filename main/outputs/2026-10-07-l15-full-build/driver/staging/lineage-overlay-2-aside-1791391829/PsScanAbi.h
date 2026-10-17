#ifndef UE4SS_PS_SCAN_ABI_H
#define UE4SS_PS_SCAN_ABI_H
#include <stdint.h>
#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#ifdef __cplusplus
// Preserve the actual PS5/Windows C++ caller character type. The wire is UTF16.
#if defined(__PROSPERO__) || defined(_WIN32)
typedef wchar_t PsChar16;
static_assert(sizeof(PsChar16) == 2, "ps_scan logging requires UTF16 code units");
#else
typedef char16_t PsChar16;
#endif
extern "C" {
#else
typedef uint16_t PsChar16;
#endif
// Synchronous read-only NUL-terminated UTF16. Message expires on return.
// All callbacks are required; must not unwind or retain the pointer.
typedef void (*PsLogCallback)(PsChar16* message);
typedef struct PsScanConfig {
    bool g_uobject_array;
    bool fname_tostring_fstring;
    bool fname_ctor_wchar;
    bool gmalloc;
    bool static_construct_object_internal;
    bool ftext_fstring;
    bool engine_version;
    bool fuobject_hash_tables_get;
    bool gnatives;
    bool console_manager_singleton;
    bool gameengine_tick;
    bool build_configuration;
    bool stats;
} PsScanConfig;
typedef struct PsCtx {
    PsLogCallback default_;
    PsLogCallback normal;
    PsLogCallback verbose;
    PsLogCallback warning;
    PsLogCallback error;
    PsScanConfig config;
} PsCtx;
typedef struct PsEngineVersion { uint16_t major; uint16_t minor; } PsEngineVersion;
typedef struct PsScanResults {
    void* g_uobject_array;
    void* fname_tostring_fstring;
    void* fname_ctor_wchar;
    void* gmalloc;
    void* static_construct_object_internal;
    void* ftext_fstring;
    PsEngineVersion engine_version;
    void* fuobject_hash_tables_get;
    void* gnatives;
    void* console_manager_singleton;
    void* gameengine_tick;
    uint32_t build_configuration; // 0 unknown; 1 shipping; 2 development; 3 test
    uint32_t stats; // 0 unknown; 1 off; 2 on, independent of configuration
} PsScanResults;
// Existing C++ references use this same pointer-based wire calling convention.
// This declaration does not provide a resolver or a source-discovery service.
bool ps_scan(PsCtx* ctx, PsScanResults* results);

#define PS_MAPPED_IMAGE_ABI_V1 UINT32_C(1)
#define PS_MAPPED_IMAGE_ELF64 UINT32_C(1)
#define PS_MAPPED_IMAGE_PE64 UINT32_C(2)
#define PS_MAPPED_READ UINT32_C(1)
#define PS_MAPPED_WRITE UINT32_C(2)
#define PS_MAPPED_EXECUTE UINT32_C(4)
typedef struct PsMappedSegment {
    const uint8_t* bytes;
    size_t size;
    uint64_t runtime_address; // Must equal the actual bytes pointer.
    uint32_t permissions; // Derived from real owner/service; hints do not authorize.
    uint32_t reserved; // Must be zero.
} PsMappedSegment;
typedef struct PsMappedExtent { uint64_t address; size_t size; } PsMappedExtent;
typedef struct PsMappedImageViewV1 {
    uint32_t abi_version;
    uint32_t format;
    const PsMappedSegment* segments;
    size_t segment_count;
    uint64_t text_base; // ELF actual .text base; PE actual code base.
    uint64_t image_base; // PE actual image base; ELF zero.
    PsMappedExtent eh_frame;
    PsMappedExtent eh_frame_hdr;
    PsMappedExtent pe_exception_directory;
} PsMappedImageViewV1;
// Supplied readable spans are synchronous borrows retained/pinned by the caller.
// Metadata is either {0,0} or wholly contained in supplied readable spans.
// No numeric runtime address permits constructing an unsupplied raw byte slice.
#ifdef __cplusplus
}
#endif
#endif
