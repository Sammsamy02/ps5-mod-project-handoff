#ifndef MB_PROSPERODEV_MODULE_ABI_H
#define MB_PROSPERODEV_MODULE_ABI_H

#include <stddef.h>
#include <stdint.h>

/* Private spellings of the MIT prosperodev declaration, not SDK replacements.
 * Source: prosperodev/prosperodev-libprospero, commit
 * 3cdd09cc86ba90ef371bd0a834e07cc3883c49e7, libkernel_sys/include/libkernel.h.
 * Copyright (c) 2022 prosperodev. See LICENSE.prosperodev and provenance.json.
 */
typedef uint32_t mb_prospero_module_handle;
typedef struct mb_prospero_module_segment {
    void *address;
    uint32_t size;
    int32_t prot;
} mb_prospero_module_segment;

typedef struct mb_prospero_module_info {
    size_t size;
    char name[256];
    mb_prospero_module_segment segmentInfo[4];
    uint32_t segmentCount;
    uint8_t fingerprint[20];
} mb_prospero_module_info;

#if defined(__cplusplus)
#define MB_ABI_ASSERT static_assert
#else
#define MB_ABI_ASSERT _Static_assert
#endif
MB_ABI_ASSERT(sizeof(void *) == 8 && sizeof(size_t) == 8, "LP64 ABI required");
MB_ABI_ASSERT(sizeof(mb_prospero_module_segment) == 16, "segment ABI size");
MB_ABI_ASSERT(offsetof(mb_prospero_module_segment, size) == 8, "segment size offset");
MB_ABI_ASSERT(offsetof(mb_prospero_module_segment, prot) == 12, "segment prot offset");
MB_ABI_ASSERT(sizeof(mb_prospero_module_info) == 352, "module info ABI size");
MB_ABI_ASSERT(offsetof(mb_prospero_module_info, name) == 8, "name offset");
MB_ABI_ASSERT(offsetof(mb_prospero_module_info, segmentInfo) == 264, "segments offset");
MB_ABI_ASSERT(offsetof(mb_prospero_module_info, segmentCount) == 328, "count offset");
MB_ABI_ASSERT(offsetof(mb_prospero_module_info, fingerprint) == 332, "fingerprint offset");
#undef MB_ABI_ASSERT
#endif
