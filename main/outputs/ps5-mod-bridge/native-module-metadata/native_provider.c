#include "module_metadata.h"
#include <string.h>

/* Exact pinned public prototypes with private, layout-asserted type spellings.
 * This TU deliberately does not include or replace an SDK libkernel header.
 * get_list is declared for provenance only; its size unit remains unverified.
 */
extern int sceKernelGetModuleList(mb_prospero_module_handle *array, size_t size, size_t *available);
extern int sceKernelGetModuleInfo(mb_prospero_module_handle handle, mb_prospero_module_info *info);

static int native_info(void *context, uint32_t handle, mb_prospero_module_info *out) {
    (void)context;
    return sceKernelGetModuleInfo(handle, out);
}

mb_module_metadata_result mb_module_metadata_native_read(uint32_t handle, mb_module_metadata_record *out) {
    const mb_module_metadata_provider provider = {NULL, NULL, native_info};
    return mb_module_metadata_read(&provider, handle, out);
}
mb_module_metadata_result mb_module_metadata_native_capture(mb_module_metadata_snapshot *out) {
    if (out) memset(out, 0, sizeof(*out));
    return (mb_module_metadata_result){out ? MB_MODULE_METADATA_LIST_ABI_UNVERIFIED : MB_MODULE_METADATA_INVALID_ARGUMENT, 0, SIZE_MAX, 0, UINT32_MAX};
}
