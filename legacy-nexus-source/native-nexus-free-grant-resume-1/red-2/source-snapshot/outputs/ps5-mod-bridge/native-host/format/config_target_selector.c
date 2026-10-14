#include "config_target_selector.h"

#include <stdint.h>
#include <string.h>

typedef struct path_segment {
    uint32_t offset;
    uint32_t bytes;
} path_segment;

static int aligned_to(const void *p, size_t alignment) {
    return p != NULL && ((uintptr_t)p % alignment) == 0;
}

static int span_end(const void *p, size_t n, uintptr_t *begin, uintptr_t *end) {
    uintptr_t b;
    if (p == NULL || begin == NULL || end == NULL) return 0;
    b = (uintptr_t)p;
    if (n > (size_t)(UINTPTR_MAX - b)) return 0;
    *begin = b;
    *end = b + n;
    return 1;
}

static int overlaps(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t ab, ae, bb, be;
    if (an == 0 || bn == 0) return 0;
    if (!span_end(a, an, &ab, &ae) || !span_end(b, bn, &bb, &be)) return -1;
    return ab < be && bb < ae;
}

static uint8_t ascii_fold(uint8_t c) {
    return (c >= (uint8_t)'A' && c <= (uint8_t)'Z')
        ? (uint8_t)(c + ((uint8_t)'a' - (uint8_t)'A')) : c;
}

static int equal_folded(const uint8_t *a, uint32_t an,
                        const uint8_t *b, uint32_t bn) {
    uint32_t i;
    if (an != bn) return 0;
    for (i = 0; i < an; ++i) {
        if (ascii_fold(a[i]) != ascii_fold(b[i])) return 0;
    }
    return 1;
}

static int split_path(const uint8_t *path, uint32_t path_bytes,
                      path_segment *segments, uint32_t wanted) {
    uint32_t i, start = 0, count = 0;
    if (path == NULL || segments == NULL || path_bytes == 0 ||
        path[0] == (uint8_t)'/' || path[path_bytes - 1] == (uint8_t)'/') return 0;
    for (i = 0; i <= path_bytes; ++i) {
        if (i == path_bytes || path[i] == (uint8_t)'/') {
            if (i == start || count >= wanted) return 0;
            segments[count].offset = start;
            segments[count].bytes = i - start;
            ++count;
            start = i + 1;
        }
    }
    return count == wanted;
}

static int path_is_printable_ascii(const uint8_t *path, uint32_t path_bytes) {
    uint32_t i;
    for (i = 0; i < path_bytes; ++i) {
        if (path[i] < 0x20u || path[i] > 0x7eu || path[i] == (uint8_t)'\\') return 0;
    }
    return 1;
}

static int valid_logical_path(const uint8_t *path, uint32_t path_bytes) {
    uint32_t i, start = 0;
    if (!path_is_printable_ascii(path, path_bytes) ||
        path[0] == (uint8_t)'/' || path[path_bytes - 1] == (uint8_t)'/') return 0;
    for (i = 0; i <= path_bytes; ++i) {
        if (i == path_bytes || path[i] == (uint8_t)'/') {
            uint32_t n = i - start;
            if (n == 0 || (n == 1 && path[start] == (uint8_t)'.') ||
                (n == 2 && path[start] == (uint8_t)'.' &&
                           path[start + 1] == (uint8_t)'.')) return 0;
            start = i + 1;
        }
    }
    return 1;
}

static int valid_platform(const uint8_t *platform, uint32_t bytes) {
    uint32_t i;
    if (platform == NULL || bytes == 0 ||
        bytes > MB_UNREAL_CONFIG_TARGET_SELECTOR_MAX_PLATFORM_BYTES) return 0;
    for (i = 0; i < bytes; ++i) {
        uint8_t c = platform[i];
        if (!((c >= (uint8_t)'A' && c <= (uint8_t)'Z') ||
              (c >= (uint8_t)'a' && c <= (uint8_t)'z') ||
              (c >= (uint8_t)'0' && c <= (uint8_t)'9') ||
              c == (uint8_t)'_' || c == (uint8_t)'-')) return 0;
    }
    return 1;
}

static int segment_is(const uint8_t *path, const path_segment *segment,
                      const char *literal, uint32_t literal_bytes) {
    return equal_folded(path + segment->offset, segment->bytes,
                        (const uint8_t *)literal, literal_bytes);
}

static int is_anchor(const mb_pak_stream_file_v1 *file,
                     path_segment segments[3]) {
    static const uint8_t engine[] = "Engine";
    static const uint8_t config[] = "Config";
    static const uint8_t default_engine[] = "DefaultEngine.ini";
    const uint8_t *path = file->path;
    if (!path_is_printable_ascii(path, file->path_bytes) ||
        !split_path(path, file->path_bytes, segments, 3)) return 0;
    if (equal_folded(path + segments[0].offset, segments[0].bytes,
                     engine, (uint32_t)(sizeof(engine) - 1))) return 0;
    return segment_is(path, &segments[1], (const char *)config,
                      (uint32_t)(sizeof(config) - 1)) &&
           segment_is(path, &segments[2], (const char *)default_engine,
                      (uint32_t)(sizeof(default_engine) - 1));
}

static int is_target(const mb_pak_stream_file_v1 *file,
                     const uint8_t *root, uint32_t root_bytes,
                     const uint8_t *platform, uint32_t platform_bytes,
                     path_segment segments[5]) {
    static const uint8_t platforms[] = "Platforms";
    static const uint8_t config[] = "Config";
    static const uint8_t engine_suffix[] = "Engine.ini";
    const uint8_t *path = file->path;
    uint32_t expected_name_bytes, i;

    if (!path_is_printable_ascii(path, file->path_bytes) ||
        !split_path(path, file->path_bytes, segments, 5)) return 0;
    if (!equal_folded(path + segments[0].offset, segments[0].bytes,
                      root, root_bytes) ||
        !segment_is(path, &segments[1], (const char *)platforms,
                    (uint32_t)(sizeof(platforms) - 1)) ||
        !equal_folded(path + segments[2].offset, segments[2].bytes,
                      platform, platform_bytes) ||
        !segment_is(path, &segments[3], (const char *)config,
                    (uint32_t)(sizeof(config) - 1))) return 0;

    if (platform_bytes > UINT32_MAX - ((uint32_t)sizeof(engine_suffix) - 1u)) return 0;
    expected_name_bytes = platform_bytes + ((uint32_t)sizeof(engine_suffix) - 1u);
    if (segments[4].bytes != expected_name_bytes) return 0;
    for (i = 0; i < platform_bytes; ++i) {
        if (ascii_fold(path[segments[4].offset + i]) != ascii_fold(platform[i])) return 0;
    }
    return equal_folded(path + segments[4].offset + platform_bytes,
                        (uint32_t)(sizeof(engine_suffix) - 1u),
                        engine_suffix, (uint32_t)(sizeof(engine_suffix) - 1u));
}

static void init_result(mb_unreal_config_target_result_v1 *result) {
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = MB_UNREAL_CONFIG_TARGET_SELECTOR_ABI_V1;
    result->target_row_index = UINT32_MAX;
    result->status = MB_UNREAL_CONFIG_TARGET_INVALID_INPUT;
}

mb_unreal_config_target_status_v1 mb_unreal_config_target_select_v1(
    const mb_pak_stream_file_v1 *files, uint32_t file_count,
    const mb_unreal_config_target_profile_v1 *profile,
    mb_unreal_config_target_result_v1 *result) {
    size_t files_bytes = 0;
    uint32_t i, root_index = UINT32_MAX, target_index = UINT32_MAX;
    path_segment seg3[3], seg5[5];
    const uint8_t *root = NULL;
    uint32_t root_bytes = 0;
    int ov;

    if (result == NULL ||
        !aligned_to(result, _Alignof(mb_unreal_config_target_result_v1)))
        return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    if (profile == NULL || !aligned_to(profile, _Alignof(mb_unreal_config_target_profile_v1)))
        return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    ov = overlaps(result, sizeof(*result), profile, sizeof(*profile));
    if (ov != 0) return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    if (file_count != 0 && files == NULL) return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    if (files != NULL && !aligned_to(files, _Alignof(mb_pak_stream_file_v1)))
        return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    if ((size_t)file_count > SIZE_MAX / sizeof(*files))
        return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    files_bytes = (size_t)file_count * sizeof(*files);
    if (files_bytes != 0) {
        ov = overlaps(result, sizeof(*result), files, files_bytes);
        if (ov != 0) return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    }
    if (profile->platform != NULL && profile->platform_bytes != 0) {
        uintptr_t b, e;
        if (!span_end(profile->platform, profile->platform_bytes, &b, &e))
            return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
        ov = overlaps(result, sizeof(*result), profile->platform,
                      profile->platform_bytes);
        if (ov != 0) return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
    }
    if (file_count > MB_UNREAL_CONFIG_TARGET_SELECTOR_MAX_FILES)
        return MB_UNREAL_CONFIG_TARGET_LIMIT;
    for (i = 0; i < file_count; ++i) {
        const mb_pak_stream_file_v1 *f = &files[i];
        if (f->path != NULL && f->path_bytes != 0) {
            uintptr_t b, e;
            if (!span_end(f->path, f->path_bytes, &b, &e))
                return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
            ov = overlaps(result, sizeof(*result), f->path, f->path_bytes);
            if (ov != 0) return MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT;
        }
    }

    init_result(result);
    if (profile->struct_size != sizeof(*profile) ||
        profile->abi_version != MB_UNREAL_CONFIG_TARGET_SELECTOR_ABI_V1 ||
        profile->reserved != 0 ||
        !valid_platform(profile->platform, profile->platform_bytes)) {
        result->status = MB_UNREAL_CONFIG_TARGET_INVALID_INPUT;
        return result->status;
    }
    for (i = 0; i < file_count; ++i) {
        const mb_pak_stream_file_v1 *f = &files[i];
        uintptr_t b, e;
        if (f->struct_size != sizeof(*f) ||
            f->abi_version != MB_PAK_STREAM_ABI_VERSION ||
            f->path == NULL || f->path_bytes == 0 ||
            f->path_bytes > MB_PAK_STREAM_MAX_PATH_BYTES ||
            f->verify_sha1 > 1u || f->reserved != 0 ||
            !span_end(f->path, f->path_bytes, &b, &e)) {
            result->status = (f->path_bytes > MB_PAK_STREAM_MAX_PATH_BYTES)
                ? MB_UNREAL_CONFIG_TARGET_LIMIT : MB_UNREAL_CONFIG_TARGET_INVALID_INPUT;
            return result->status;
        }
        if (!valid_logical_path(f->path, f->path_bytes)) {
            result->status = MB_UNREAL_CONFIG_TARGET_INVALID_INPUT;
            return result->status;
        }
    }

    for (i = 0; i < file_count; ++i) {
        if (is_anchor(&files[i], seg3)) {
            if (result->root_count == 0) {
                root_index = i;
                root = files[i].path + seg3[0].offset;
                root_bytes = seg3[0].bytes;
            }
            ++result->root_count;
        }
    }
    if (result->root_count == 0) {
        result->status = MB_UNREAL_CONFIG_TARGET_NO_PROJECT_ROOT;
        return result->status;
    }
    if (result->root_count != 1) {
        result->status = MB_UNREAL_CONFIG_TARGET_AMBIGUOUS_PROJECT_ROOT;
        return result->status;
    }
    (void)root_index;

    for (i = 0; i < file_count; ++i) {
        if (is_target(&files[i], root, root_bytes, profile->platform,
                      profile->platform_bytes, seg5)) {
            target_index = i;
            ++result->target_count;
        }
    }
    if (result->target_count == 0) {
        result->status = MB_UNREAL_CONFIG_TARGET_NO_PLATFORM_TARGET;
        return result->status;
    }
    if (result->target_count != 1) {
        result->status = MB_UNREAL_CONFIG_TARGET_AMBIGUOUS_PLATFORM_TARGET;
        return result->status;
    }

    result->target_row_index = target_index;
    result->target_path_bytes = files[target_index].path_bytes;
    memcpy(result->target_path, files[target_index].path,
           result->target_path_bytes);
    result->status = MB_UNREAL_CONFIG_TARGET_OK;
    result->complete = 1;
    return result->status;
}
