// host_test.c — HOST LOGIC TEST for c2_data_probe.c (worker-c2-probe,
// team 2026-10-04). OFFLINE ONLY; this is not the console binary and it
// never ships. It links the REAL record_sink.c, ingame_libc.c,
// vmmap_walk.c, and mapscan.c (ig_map_cover gating included) and the REAL
// c2_data_probe.c via textual include with exactly TWO host-only shims:
//   1. IG_C2_HOST_READ_HOOK: the deref target becomes a copy from a
//      synthetic host-side window table (no address translation games);
//   2. ig_mapscan_run -> host_mapscan_run: fills the REAL g_map/g_spans
//      through the REAL ig_vmmap_walk (stride-fix path) with synthetic
//      144-byte records (kve_structsize-respecting).
// The console build defines NEITHER shim.
//
// Modes (argv[1]):
//   trusted                — full readable map: 5 gated reads, seq4..8
//   untrusted              — mapscan failed: 5 refusals "map_untrusted"
//   covered_not_readable   — rodata span prot=4: B..E refusals, A reads
#include "anchor_store.h"
#include "record_sink.h"
#include "ingame_libc.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Host implementations of the 3 console import identities record_sink.c
   calls (sceKernelOpen/Write/Close map 1:1 onto POSIX here; on console the
   real imported identities are used — the host link never ships). */
int sceKernelOpen(const char *path, int flags, uint16_t mode) {
    return open(path, flags, (mode_t)mode);
}
int64_t sceKernelWrite(int fd, const void *buf, size_t n) {
    return (int64_t)write(fd, buf, n);
}
int sceKernelClose(int fd) {
    return close(fd);
}

/* Host stand-ins for module_entry.c's counters (module_entry.c is not part
   of the host link; the probe only records them). */
unsigned g_ctor_events;
unsigned g_fini_events;

/* ---------------- synthetic VMMAP buffer (144-byte records) ------------- */
static uint8_t synbuf[16 * 144];

struct SynSpan { uint64_t start, end; uint32_t prot; };
static const struct SynSpan g_synth[] = {
    {0x400000ull, 0x787c000ull, 4},      /* exec phdr0 (derived base+extent) */
    {0x787c000ull, 0x82bb000ull, 1},     /* rodata phdr1 (B, C, D, E) */
    {0x82bc000ull, 0x953b000ull, 3},     /* RELRO phdr3 */
    {0x953c000ull, 0x9a34000ull, 3},     /* phdr7 data+BSS (igf4 GUA region) */
    {0x9a33000ull, 0xfc61000ull, 1},     /* phdr8 aux, READABLE (A) */
    {0xfc61000ull, 0xfc62000ull, 3},     /* misc */
    {0x2000000ull, 0x2100000ull, 3},     /* loader */
    {0x7f000000000ull, 0x7f000100000ull, 3}, /* stack/anon (64-bit sanity) */
};
static const uint32_t g_synth_n = sizeof(g_synth) / sizeof(g_synth[0]);
/* covered_not_readable mode flips ONLY the rodata span to prot 4. */

/* ---------------- synthetic live windows (the "game data") -------------- */
struct Window { uint64_t rt; uint32_t len; uint8_t bytes[64]; };
static struct Window g_win[8];
static uint32_t g_win_n;

static void host_read_hook(uint64_t rt, uint8_t *dst, uint32_t len) {
    for (uint32_t i = 0; i < g_win_n; ++i) {
        if (g_win[i].rt == rt && g_win[i].len == len) {
            ig_memcpy(dst, g_win[i].bytes, len);
            return;
        }
    }
    fprintf(stderr, "HOST FATAL: read outside synthetic windows\n");
    abort();
}

/* ---------------- mapscan shim: fills REAL g_map/g_spans ---------------- */
static int g_force_untrusted;
static int g_rodata_exec;  /* covered_not_readable mode */

int host_mapscan_run(void) {
    ig_memset(&g_map, 0, sizeof g_map);
    if (g_force_untrusted) {
        g_map.rc = 12;
        g_map.eno = 12;
        g_map.attempts = 1;
        g_map.buf_len = 0;
        g_map.trusted = 0;
        return 0;
    }
    ig_memset(synbuf, 0, sizeof synbuf);
    for (uint32_t i = 0; i < g_synth_n; ++i) {
        uint8_t *rec = synbuf + (size_t)i * 144;
        uint32_t prot = g_synth[i].prot;
        if (g_rodata_exec && i == 1) prot = 4;
        rec[0] = 144; rec[4] = 1;  /* kve_structsize, kve_type */
        for (int b = 0; b < 8; ++b) {
            rec[8 + b] = (uint8_t)((g_synth[i].start >> (8 * b)) & 0xff);
            rec[16 + b] = (uint8_t)((g_synth[i].end >> (8 * b)) & 0xff);
        }
        rec[56] = (uint8_t)(prot & 0xff);
    }
    uint64_t buflen = (uint64_t)g_synth_n * 144;
    g_map.rc = 0;
    g_map.eno = 0;
    g_map.attempts = 1;
    g_map.buf_len = buflen;
    g_map.first_ksz = 144;
    g_map.mod_ok = (buflen % 144) == 0;
    ig_vmmap_walk(synbuf, buflen, 1160 /*IG_KVE_SIZEOF*/, 1,
                  g_spans, IG_SPAN_MAX, &g_map.walk, 0, 0);
    g_map.trusted = ig_vmmap_trustworthy(&g_map.walk);
    uint32_t readable = 0;
    for (uint32_t i = 0; i < g_map.walk.retained; ++i)
        if (g_spans[i].prot & 1u) ++readable;
    g_map.readable_spans = readable;
    return g_map.trusted;
}

/* ---------------- the probe under test (real code) ---------------------- */
#define ig_mapscan_run host_mapscan_run
#define IG_C2_HOST_READ_HOOK host_read_hook
#include "../src/c2_data_probe.c"

/* ---------------- driver ------------------------------------------------ */
int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s trusted|untrusted|covered_not_readable "
                        "<out.jsonl> <patch.txt>\n", argv[0]);
        return 2;
    }
    const char *mode = argv[1];

    /* Load synthetic windows: lines "0xRT LEN HEX" (offline-only file). */
    FILE *pf = fopen(argv[3], "r");
    if (!pf) { perror("patch"); return 2; }
    char line[512];
    while (fgets(line, sizeof line, pf)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        unsigned long long rt;
        unsigned len;
        char hex[256];
        if (sscanf(line, "%llx %u %255s", &rt, &len, hex) != 3) continue;
        if (len > 64 || g_win_n >= 8) { fprintf(stderr, "bad patch\n"); return 2; }
        g_win[g_win_n].rt = rt;
        g_win[g_win_n].len = len;
        for (unsigned i = 0; i < len; ++i) {
            unsigned v;
            if (sscanf(hex + 2 * i, "%2x", &v) != 1) { return 2; }
            g_win[g_win_n].bytes[i] = (uint8_t)v;
        }
        ++g_win_n;
    }
    fclose(pf);
    if (g_win_n != 5) { fprintf(stderr, "expected 5 windows, got %u\n", g_win_n); return 2; }

    if (mode[0] == 'u') g_force_untrusted = 1;
    if (mode[0] == 'c') g_rodata_exec = 1;

    IgSink sink;
    sink.fd = sceKernelOpen(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (sink.fd < 0) { perror("open out"); return 2; }
    sink.failed = 0;
    sink.open_errno = 0;

    /* Minimal entry-marker/startup records so the JSONL shape matches the
       console sequence (module_entry.c is not part of the host link). */
    IgBuf b;
    ig_record_begin(&b, 1, "entry", 0, 0, IG_BUILD_ID);
    ig_kv_str(&b, "phase", "host_test", 16);
    flush_record(&sink, &b);
    ig_record_begin(&b, 2, "startup", 0, 0, IG_BUILD_ID);
    ig_kv_str(&b, "phase", "post_init_array", 32);
    ig_kv_int(&b, "ctors_run", 0);
    flush_record(&sink, &b);

    ig_run_c2_data_probe(&sink);
    sceKernelClose(sink.fd);
    return sink.failed ? 1 : 0;
}
