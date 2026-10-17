#include "summary.h"

static void put_le(uint8_t *p, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        p[i] = (uint8_t)value;
        value >>= 8;
    }
}

int main(void) {
    uint8_t header[87] = {0};
    put_le(header + 4, sizeof(header), 4);
    put_le(header + 8, 0, 4);
    put_le(header + 16, UINT32_C(0x80002200), 4);
    put_le(header + 20, 100, 4);
    for (unsigned i = 0; i < 7; ++i) put_le(header + 24 + i * 4, sizeof(header), 4);
    put_le(header + 52, sizeof(header), 4);
    put_le(header + 56, sizeof(header), 4);
    put_le(header + 60, 1, 4);
    put_le(header + 64, 1, 4);
    put_le(header + 68, 0, 8);
    put_le(header + 76, UINT64_C(0x0102030405060708), 8);
    header[84] = 0; header[85] = 1; header[86] = 'X';

    const mb_zen_summary_view_v1 view = {
        sizeof(view), MB_ZEN_SUMMARY_ABI_VERSION, header, sizeof(header), sizeof(header)
    };
    const mb_zen_summary_profile_v1 profile = {
        sizeof(profile), MB_ZEN_SUMMARY_ABI_VERSION, MB_ZEN_LAYOUT_UE5_3_PLUS, 1, 0
    };
    mb_zen_summary_limits_v1 limits;
    mb_zen_summary_limits_v1_default(&limits);
    mb_zen_summary_result_v1 result = {
        .struct_size = sizeof(result), .abi_version = MB_ZEN_SUMMARY_ABI_VERSION
    };
    mb_zen_name_v1 name = {0};
    uint8_t arena[1] = {0};
    if (mb_zen_read_summary_names_v1(&view, &profile, &limits, &result,
        &name, 1, arena, sizeof(arena)) != MB_ZEN_SUMMARY_OK) return 1;
    return result.summary_complete && result.name_map_complete &&
        result.mapped_name_resolved && result.mapped_name_index == 0 &&
        name.encoding == MB_ZEN_NAME_UTF8 && name.byte_length == 1 && arena[0] == 'X' ? 0 : 2;
}
