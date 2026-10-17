// writer_host.c — host test main for the REAL record_sink.c (vendored v3
// lineage writer, byte-unchanged). Emits bounded records covering: plain
// fields, negative i64, 36-bit hex, string escaping, nested objects/arrays,
// and one TRUNCATED record (must carry "__trunc":1). The python driver
// json.loads-parses every non-truncated line.
#include "record_sink.h"

#include <stdio.h>
#include <string.h>

/* host stubs for the 7-import ABI (declared in ingame_abi.h) */
int *__error(void) { static int e; return &e; }
int sceKernelOpen(const char *p, int f, uint16_t m) {
    (void)p; (void)f; (void)m; return 3;
}
int64_t sceKernelWrite(int fd, const void *b, size_t n) {
    (void)fd; fwrite(b, 1, n, stdout); return (int64_t)n;
}
int sceKernelClose(int fd) { (void)fd; return 0; }

int main(void) {
    IgSink sink;
    sink.fd = 3; sink.failed = 0; sink.open_errno = 0;

    IgBuf b;
    ig_record_begin(&b, 1, "entry", 0, 0, "igf4-hosttest");
    ig_kv_str(&b, "phase", "module_start", 32);
    ig_record_flush(&b);
    if (ig_sink_write_record(&sink, &b) != 0) return 1;

    ig_record_begin(&b, 2, "fieldshapes", 0, 0, "igf4-hosttest");
    ig_kv_int(&b, "neg", -12345);
    ig_kv_hex(&b, "wide", 0x500000090ull);          /* 36-bit round-trip */
    /* NOTE: \x01 kept in its own string literal — C hex escapes are greedy */
    ig_kv_str(&b, "esc", "quote\"back\\slash\x01" "ctl", 64);
    ig_arr_open(&b, "arr");
    ig_buf_i64_dec(&b, 1);                          /* array = raw values, */
    ig_buf_ch(&b, ',');                             /* manual separators   */
    ig_buf_i64_dec(&b, 2);
    ig_arr_close(&b);
    ig_kv_int(&b, "after_arr", 7);
    ig_kv_str(&b, "last", "end", 8);
    ig_record_flush(&b);
    if (ig_sink_write_record(&sink, &b) != 0) return 1;

    /* forced truncation: a field that cannot possibly fit */
    ig_record_begin(&b, 3, "trunc", 0, 0, "igf4-hosttest");
    {
        char blob[3000];
        memset(blob, 'z', sizeof blob - 1);
        blob[sizeof blob - 1] = '\0';
        ig_kv_str(&b, "blob", blob, sizeof blob - 1);
    }
    ig_record_flush(&b);
    if (ig_sink_write_record(&sink, &b) != 0) return 1;

    return sink.failed ? 2 : 0;
}
