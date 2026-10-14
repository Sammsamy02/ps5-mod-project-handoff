// host_main.c — worker-code-anchor-reads host checks (macOS arm64 host).
// Exercises the PURE parts of the igf5 probe on the host:
//   1. igcar_page_range: the exact anchor pages (Tick 0x33ac8c0 ->
//      page 0x33ac000; SCO 0x890aa0 -> page 0x890000), a page-crossing
//      case, and the rejection cases (bad page size, zero len, overflow).
//   2. igcar_bytes_match: equality semantics + diff_mask bit placement.
//   3. The REAL lineage record writer (record_sink.c, byte-identical):
//      emits anchor-shaped records (success + refusal shapes) to a JSONL
//      file; host_checks.py then json.loads EVERY line.
// The x86-64 syscall shim itself cannot run on this host — it is verified
// by the PS5 cross-compile (-Werror) + the objdump syscall-instruction gate
// in check_code_anchor_artifact.py.
#include "anchor_pages.h"
#include "record_sink.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Host stubs for the 4 lineage imports record_sink.c uses. */
int sceKernelOpen(const char *path, int flags, int mode) {
    return open(path, flags, mode);
}
int64_t sceKernelWrite(int fd, const void *buf, uint64_t n) {
    return (int64_t)write(fd, buf, (size_t)n);
}
int sceKernelClose(int fd) { return close(fd); }
/* NOTE: on macOS `errno` IS __error() — returning &errno here would make
   this stub recurse into itself. Use a private cell instead. */
static int g_host_errno_cell;
int *__error(void) { return &g_host_errno_cell; }

static int g_fail;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL: %s\n", msg);                                      \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

static void check_pages(void) {
    uint64_t first;
    uint32_t n;
    CHECK(igcar_page_range(0x33ac8c0ull, 16, 0x4000ull, &first, &n) == 0 &&
              first == 0x33ac000ull && n == 1,
          "tick page 0x33ac000");
    CHECK(igcar_page_range(0x890aa0ull, 16, 0x4000ull, &first, &n) == 0 &&
              first == 0x890000ull && n == 1,
          "sco page 0x890000");
    CHECK(igcar_page_range(0x33afff8ull, 16, 0x4000ull, &first, &n) == 0 &&
              first == 0x33ac000ull && n == 2,
          "page-crossing read -> 2 pages");
    CHECK(igcar_page_range(0x1000ull, 16, 0ull, &first, &n) == -2,
          "zero page size refused");
    CHECK(igcar_page_range(0x1000ull, 16, 3000ull, &first, &n) == -2,
          "non-power-of-two page size refused");
    CHECK(igcar_page_range(0x1000ull, 0ull, 0x4000ull, &first, &n) == -1,
          "zero length refused");
    CHECK(igcar_page_range(~0ull, 64, 0x4000ull, &first, &n) == -1,
          "overflow refused");
}

static void check_match(void) {
    const uint8_t a[16] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56,
                           0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x81, 0xec};
    uint8_t b[16];
    unsigned mask = 0;
    for (int i = 0; i < 16; ++i) b[i] = a[i];
    CHECK(igcar_bytes_match(a, b, 16, &mask) == 1 && mask == 0,
          "identical bytes -> match 1, mask 0");
    b[3] ^= 0xff;
    b[15] ^= 0x01;
    CHECK(igcar_bytes_match(a, b, 16, &mask) == 0 && mask == 0x8008u,
          "differing bytes -> match 0, diff_mask bits 3+15");
}

static void emit_records(const char *path) {
    IgSink sink;
    IgBuf b;
    sink.fd = -1;
    sink.failed = 0;
    sink.open_errno = 0;
    if (ig_sink_open(&sink, path) != 0) {
        printf("FAIL: cannot open %s (errno %d)\n", path, sink.open_errno);
        ++g_fail;
        return;
    }
    /* Success-shape anchor record (tick). */
    ig_record_begin(&b, 4, "tick_anchor", 0, 0, "igf5-hosttest");
    ig_kv_hex(&b, "rt_addr", 0x33ac8c0ull);
    ig_kv_hex(&b, "profile_vaddr", 0x2fac8c0ull);
    ig_kv_hex(&b, "image_file_off", 0x2fb08c0ull);
    ig_kv_str(&b, "expected_bytes_hex",
              "554889e54157415641554154534881ec", 32);
    ig_kv_str(&b, "method_used", "mprotect_then_read", 40);
    ig_kv_hex(&b, "page_start", 0x33ac000ull);
    ig_kv_int(&b, "page_size", 16384);
    ig_kv_int(&b, "pages", 1);
    ig_kv_int(&b, "orig_prot", 4);
    ig_kv_str(&b, "orig_prot_source", "current_vmmap_cover", 24);
    ig_kv_int(&b, "mprotect_called", 1);
    ig_kv_int(&b, "mprotect_rc", 0);
    ig_kv_hex(&b, "mprotect_errno_hex", 0);
    ig_kv_int(&b, "mprotect_prot_requested", 5);
    ig_kv_int(&b, "read_performed", 1);
    ig_kv_str(&b, "bytes_read_hex", "554889e54157415641554154534881ec", 32);
    ig_kv_int(&b, "bytes_read", 16);
    ig_kv_int(&b, "byte_match", 1);
    ig_kv_int(&b, "diff_mask", 0);
    ig_kv_int(&b, "restore_called", 1);
    ig_kv_int(&b, "restore_rc", 0);
    ig_kv_int(&b, "restore_prot", 4);
    ig_kv_int(&b, "mprotect_restored", 1);
    ig_record_flush(&b);
    if (ig_sink_write_record(&sink, &b) != 0) ++g_fail;

    /* Refusal-shape anchor record (sco, mprotect failed). */
    ig_record_begin(&b, 5, "sco_anchor", -1, 0, "igf5-hosttest");
    ig_kv_hex(&b, "rt_addr", 0x890aa0ull);
    ig_kv_str(&b, "expected_bytes_hex", "554889e54157415641554154534881ec", 32);
    ig_kv_str(&b, "method_used", "refused_mprotect_failed", 40);
    ig_kv_hex(&b, "page_start", 0x890000ull);
    ig_kv_int(&b, "mprotect_called", 1);
    ig_kv_int(&b, "mprotect_rc", -1);
    ig_kv_hex(&b, "mprotect_errno_hex", 1);
    ig_kv_int(&b, "read_performed", 0);
    ig_kv_int(&b, "mprotect_restored", 0);
    ig_kv_str(&b, "refusal", "mprotect_failed_no_read_attempted", 40);
    ig_record_flush(&b);
    if (ig_sink_write_record(&sink, &b) != 0) ++g_fail;

    /* Summary shape. */
    ig_record_begin(&b, 7, "summary", 0, 0, "igf5-hosttest");
    ig_kv_int(&b, "anchors_confirmed", 1);
    ig_kv_int(&b, "promotion_gate_indicator_anchors_only", 1);
    ig_kv_int(&b, "promotion_gate_indicator", 0);
    ig_kv_int(&b, "mprotect_anomalies", 0);
    ig_record_flush(&b);
    if (ig_sink_write_record(&sink, &b) != 0) ++g_fail;

    (void)ig_sink_close(&sink);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: host_main <out.jsonl>\n");
        return 2;
    }
    check_pages();
    check_match();
    emit_records(argv[1]);
    if (g_fail == 0) printf("HOST_CHECKS_PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
