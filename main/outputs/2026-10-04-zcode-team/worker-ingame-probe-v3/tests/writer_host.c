// tests/writer_host.c — host unit test for the v3 record writer.
// Compiles the REAL src/record_sink.c (sink functions satisfied by local
// stubs) and exercises the exact v1/v2 bug shapes plus 36-bit hex values,
// i64 negatives (incl. INT64_MIN), empty arrays, nested objects and a
// truncation case. tests/run_host_checks.py json.loads every emitted line.
#include "../src/record_sink.h"
#include "../src/ingame_abi.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Stubs for the lineage sink trio (never called through a real fd here). */
int sceKernelOpen(const char *p, int f, uint16_t m) { (void)p; (void)f; (void)m; return -1; }
int64_t sceKernelWrite(int fd, const void *b, size_t n) { (void)fd; (void)b; return (int64_t)n; }
int sceKernelClose(int fd) { (void)fd; return 0; }
int g_errno_word;
int *__error(void) { return &g_errno_word; }

static FILE *g_out;

static void write_record(const IgBuf *b) {
    fwrite(b->data, 1, b->len, g_out);
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    g_out = fopen(argv[1], "wb");
    if (!g_out) return 3;

    /* Case 1: the exact v1 log field pattern that exposed the missing
       commas (`"raw":0"raw_hex":"0x..."`) plus extremes. */
    IgBuf b;
    ig_record_begin(&b, 1, "writer_shape", 0, 12, "igf3-hosttest");
    ig_kv_int(&b, "raw", 0);
    ig_kv_hex(&b, "raw_hex", 0x0000000000000000ull);
    ig_kv_int(&b, "neg_min", INT64_MIN);
    ig_kv_int(&b, "neg", -9223372036854775807ll - 1 + 1);
    ig_kv_int(&b, "i64_max", INT64_MAX);
    ig_kv_int(&b, "rc_neg", -1);
    ig_kv_hex(&b, "span36_lo", 0x100000090ull);       /* 36-bit */
    ig_kv_hex(&b, "span36_hi", 0x7ed7f8000ull);       /* 36-bit */
    ig_kv_hex(&b, "read_end", 0x100118000ull);        /* 33-bit */
    ig_kv_str(&b, "esc", "quote\" back\\ ctrl\x01\x7f", 32);
    ig_arr_open(&b, "empty_arr");
    ig_arr_close(&b);
    ig_record_flush(&b);
    write_record(&b);

    /* Case 2: vmmap-page-like record: array of objects, nested members. */
    ig_record_begin(&b, 2, "vm_page_shape", 0, 0, "igf3-hosttest");
    ig_kv_int(&b, "part", 0);
    ig_kv_int(&b, "parts", 128);
    ig_arr_open(&b, "entries");
    for (int i = 0; i < 16; ++i) {
        if (i != 0) ig_buf_ch(&b, ',');
        ig_obj_open(&b);
        ig_kv_hex(&b, "start", 0x100000090ull + (uint64_t)i * 0x40000000ull);
        ig_kv_hex(&b, "end", 0x100000090ull + (uint64_t)(i + 1) * 0x40000000ull);
        ig_kv_int(&b, "prot", (int64_t)(i & 7));
        ig_obj_close(&b);
        if (b.truncated) break;
    }
    ig_arr_close(&b);
    ig_arr_open(&b, "prot_histogram");
    ig_obj_open(&b);
    ig_kv_hex(&b, "prot", 0x5ull);
    ig_kv_int(&b, "count", 9);
    ig_obj_close(&b);
    ig_arr_close(&b);
    ig_kv_int(&b, "after_array", 1);
    ig_record_flush(&b);
    write_record(&b);

    /* Case 3: nested array inside object (raw hdr words shape). */
    ig_record_begin(&b, 3, "raw_hdr_shape", 0, 0, "igf3-hosttest");
    ig_arr_open(&b, "hdrs");
    ig_obj_open(&b);
    ig_kv_int(&b, "rec", 0);
    ig_arr_open(&b, "w");
    for (int w = 0; w < 17; ++w) {
        if (w != 0) ig_buf_ch(&b, ',');
        ig_buf_ch(&b, '"');
        ig_buf_u64_hex(&b, 0x7ed7f8000ull * (uint64_t)(w + 1));
        ig_buf_ch(&b, '"');
        if (b.truncated) break;
    }
    ig_arr_close(&b);
    ig_obj_close(&b);
    ig_arr_close(&b);
    ig_record_flush(&b);
    write_record(&b);

    /* Case 4: truncation — a huge string that overflows the 2 KiB bound.
       The line must carry "__trunc":1 and is EXEMPT from the parse check
       (same rule as v1/v2). */
    ig_record_begin(&b, 4, "trunc_shape", 0, 0, "igf3-hosttest");
    ig_kv_int(&b, "before", 7);
    {
        static char big[3000];
        memset(big, 'x', sizeof big - 1);
        big[sizeof big - 1] = 0;
        ig_kv_str(&b, "big", big, sizeof big);
    }
    ig_kv_int(&b, "after", 8);
    ig_record_flush(&b);
    write_record(&b);

    /* Case 5: empty fields object. */
    ig_record_begin(&b, 5, "empty_fields", 0, 0, "igf3-hosttest");
    ig_record_flush(&b);
    write_record(&b);

    fclose(g_out);
    return 0;
}
