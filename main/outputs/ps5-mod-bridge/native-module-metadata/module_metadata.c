#include "module_metadata.h"
#include <string.h>

static mb_module_metadata_result result(mb_module_metadata_status status, uint32_t handle) {
    return (mb_module_metadata_result){status, 0, SIZE_MAX, handle, UINT32_MAX};
}

mb_module_metadata_result mb_module_metadata_read(
    const mb_module_metadata_provider *provider, uint32_t handle, mb_module_metadata_record *out) {
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !provider || !provider->get_info)
        return result(MB_MODULE_METADATA_INVALID_ARGUMENT, handle);

    mb_prospero_module_info raw = {0};
    raw.size = sizeof(raw);
    int code = provider->get_info(provider->context, handle, &raw);
    if (code != 0) {
        mb_module_metadata_result error = result(MB_MODULE_METADATA_INFO_FAILED, handle);
        error.provider_code = code;
        return error;
    }
    if (raw.size != sizeof(raw))
        return result(MB_MODULE_METADATA_INFO_SIZE_MISMATCH, handle);
    const char *name_end = memchr(raw.name, 0, sizeof(raw.name));
    if (!name_end || name_end == raw.name)
        return result(MB_MODULE_METADATA_INVALID_NAME, handle);
    if (raw.segmentCount == 0 || raw.segmentCount > 4)
        return result(MB_MODULE_METADATA_SEGMENT_COUNT_OUT_OF_RANGE, handle);
    for (uint32_t i = 0; i < raw.segmentCount; ++i) {
        uint64_t address = (uint64_t)(uintptr_t)raw.segmentInfo[i].address;
        uint32_t size = raw.segmentInfo[i].size;
        mb_module_metadata_status status = MB_MODULE_METADATA_OK;
        if (address == 0 || size == 0)
            status = MB_MODULE_METADATA_INVALID_SEGMENT;
        else if (address > UINT64_MAX - size)
            status = MB_MODULE_METADATA_ADDRESS_OVERFLOW;
        if (status != MB_MODULE_METADATA_OK) {
            mb_module_metadata_result error = result(status, handle);
            error.segment_index = i;
            return error;
        }
    }

    out->handle = handle;
    memcpy(out->name, raw.name, (size_t)(name_end - raw.name) + 1);
    memcpy(out->fingerprint, raw.fingerprint, sizeof(out->fingerprint));
    out->segment_count = raw.segmentCount;
    for (uint32_t i = 0; i < raw.segmentCount; ++i) {
        out->segments[i].reported_address = (uint64_t)(uintptr_t)raw.segmentInfo[i].address;
        out->segments[i].reported_size = raw.segmentInfo[i].size;
        out->segments[i].reported_protection = raw.segmentInfo[i].prot;
    }
    return result(MB_MODULE_METADATA_OK, handle);
}

mb_module_metadata_result mb_module_metadata_capture(
    const mb_module_metadata_provider *provider, mb_module_metadata_snapshot *out) {
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !provider || !provider->get_list || !provider->get_info)
        return result(MB_MODULE_METADATA_INVALID_ARGUMENT, 0);

    mb_prospero_module_handle handles[MB_MODULE_METADATA_LIMIT] = {0};
    size_t count = SIZE_MAX;
    int code = provider->get_list(provider->context, handles, MB_MODULE_METADATA_LIMIT, &count);
    if (code != 0) {
        mb_module_metadata_result error = result(MB_MODULE_METADATA_LIST_FAILED, 0);
        error.provider_code = code;
        return error;
    }
    if (count > MB_MODULE_METADATA_LIMIT)
        return result(MB_MODULE_METADATA_COUNT_OUT_OF_RANGE, 0);
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (handles[j] == handles[i]) {
                mb_module_metadata_result error = result(MB_MODULE_METADATA_DUPLICATE_HANDLE, handles[i]);
                error.module_index = i;
                return error;
            }
        }
    }
    for (size_t i = 0; i < count; ++i) {
        mb_module_metadata_result item = mb_module_metadata_read(provider, handles[i], &out->records[i]);
        if (item.status != MB_MODULE_METADATA_OK) {
            memset(out, 0, sizeof(*out));
            item.module_index = i;
            return item;
        }
    }
    out->count = count;
    return result(MB_MODULE_METADATA_OK, 0);
}
