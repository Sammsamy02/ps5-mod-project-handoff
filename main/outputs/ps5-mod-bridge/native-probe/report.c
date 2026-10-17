/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "report.h"

struct buffer { char *bytes; unsigned long used, capacity; int failed; };
static void character(struct buffer *out, char value) {
    if (out->used + 1 >= out->capacity) { out->failed = 1; return; }
    out->bytes[out->used++] = value;
}
static void text(struct buffer *out, const char *value) {
    while (*value) character(out, *value++);
}
static void string(struct buffer *out, const char *value) {
    character(out, '"');
    while (*value) {
        unsigned char c = (unsigned char)*value++;
        if (c == '"' || c == '\\') { character(out, '\\'); character(out, (char)c); }
        else if (c < 32) {
            const char *hex = "0123456789abcdef";
            text(out, "\\u00"); character(out, hex[c >> 4]); character(out, hex[c & 15]);
        } else character(out, (char)c);
    }
    character(out, '"');
}
static void number(struct buffer *out, long value) {
    char digits[32];
    unsigned int count = 0;
    unsigned long magnitude = (unsigned long)value;
    if (value < 0) { character(out, '-'); magnitude = 0UL - magnitude; }
    do { digits[count++] = (char)('0' + magnitude % 10); magnitude /= 10; } while (magnitude);
    while (count) character(out, digits[--count]);
}
static void capability(struct buffer *out, const char *id, const char *context, int comma) {
    if (comma) character(out, ',');
    text(out, "{\"id\":"); string(out, id);
    text(out, ",\"context\":"); string(out, context);
    text(out, ",\"status\":\"unknown\",\"evidence\":null}");
}

unsigned long probe_format(const struct probe_facts *facts, const char *origin, char *bytes, unsigned long capacity) {
    struct buffer out = {bytes, 0, capacity, 0};
    if (!facts || !origin || !bytes || !capacity) return 0;
    text(&out, "{\"schema_version\":1,\"record_kind\":\"native_probe_report\",\"evidence_origin\":");
    string(&out, origin);
    text(&out, ",\"build\":{\"compiler\":");
    string(&out, __clang_version__);
#if defined(__PROSPERO__) && defined(__x86_64__)
    text(&out, ",\"target\":\"x86_64-sie-ps5\"");
#else
    text(&out, ",\"target\":\"desktop_test\"");
#endif
    text(&out, ",\"sizeof_pointer\":"); number(&out, sizeof(void *));
    text(&out, ",\"sizeof_long\":"); number(&out, sizeof(long));
    text(&out, ",\"sizeof_int\":"); number(&out, sizeof(int));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    text(&out, ",\"byte_order\":\"little\"}");
#else
    text(&out, ",\"byte_order\":\"other\"}");
#endif
    text(&out, ",\"candidate_entry_abi\":\"sdk-payload-args-v0.43\",\"environment\":{\"firmware_verified\":null,\"loader\":null,\"loader_version\":null,\"game\":null,\"game_update\":null},\"observations\":{");
    for (int i = 0; i < 4; ++i) {
        const struct probe_observation *observation = &facts->observation[i];
        if (i) character(&out, ',');
        string(&out, i == 0 ? "getpid" : i == 1 ? "getuid" : i == 2 ? "geteuid" : "getpagesize");
        text(&out, ":{\"status\":");
        string(&out, observation->state == PROBE_OBSERVED ? "observed" : observation->state == PROBE_UNAVAILABLE ? "unavailable" : "unmeasured");
        text(&out, ",\"value\":");
        if (!observation->has_value) text(&out, "null");
        else number(&out, observation->value);
        text(&out, ",\"resolve_code\":"); number(&out, observation->resolve_code);
        character(&out, '}');
    }
    text(&out, "},\"output_resolve_code\":"); number(&out, facts->output_resolve_code);
    text(&out, ",\"capabilities\":[");
    capability(&out, "host.payload", "standalone", 0);
    capability(&out, "filesystem.scratch", "standalone", 1);
    capability(&out, "network.tls", "standalone", 1);
    capability(&out, "ui.controller_display", "standalone", 1);
    capability(&out, "host.native_module", "game_process", 1);
    capability(&out, "runtime.lua", "game_process", 1);
    capability(&out, "runtime.ue4ss", "game_process", 1);
    capability(&out, "runtime.windows", "game_process", 1);
    capability(&out, "translation.unreal_pak", "preparation", 1);
    capability(&out, "translation.unreal_asset", "preparation", 1);
    text(&out, "]}\n");
    if (out.failed) return 0;
    out.bytes[out.used] = 0;
    return out.used;
}
