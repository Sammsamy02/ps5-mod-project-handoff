/* host_main.c — worker-fname-runtime-probe host checks (macOS host).
 *
 * Runs the FULL FC-1 probe flow end-to-end against synthetic arenas whose
 * bytes are the REAL captured window bytes relocated to synthetic runtime
 * addresses (disp32/rel32 recomputed; everything else byte-identical):
 *
 *   scenario 1  all_pass               -> validated 1, 9/9 sites, D 5/5,
 *                                         mprotect log = 10 x (R|E request,
 *                                         restore-to-0x4) pairs
 *   scenario 2  target byte drift      -> A refuted specifically
 *                                         (first_divergence recorded),
 *                                         D skipped, validated 0
 *   scenario 3  mov edx,1 corrupted    -> that site fails, FC1-C
 *                                         fail_closed 1, validated 0
 *   scenario 4  mprotect refused       -> NO read anywhere (no
 *                                         bytes_read_hex in any record),
 *                                         validated 0, no anomaly
 *   scenario 5  restore fails          -> read performed, anomaly recorded,
 *                                         validated still 1 (igf5 semantics)
 *   scenario 6  map untrusted          -> orig_prot from LABELED fallback,
 *                                         FC1-D refused (never deref'd),
 *                                         validated still 1
 *   scenario 7  call rel32 drift       -> site fails T2/decode, fail_closed 1
 *   scenario 8  unsupported encoding   -> fail-closed decode status
 *
 * Every scenario emits the probe's own JSONL records via the REAL lineage
 * record writer (record_sink.c); run_host_checks.py json-parses every line
 * and asserts the verdicts. C-side assertions cover the mprotect call log
 * (ordering + protections) that JSONL cannot show.
 */
#include "fc1_fixtures.h"
#include "fc1_host_env.h"
#include "fc1_windows.h"
#include "record_sink.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Host stubs for the 4 lineage imports record_sink.c uses (igf5 pattern). */
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
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL: %s\n", msg);                                     \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

static void write_scenario(const char *dir, int n, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/scenario_%d_%s.jsonl", dir, n, name);
    IgSink sink;
    sink.fd = -1;
    sink.failed = 0;
    sink.open_errno = 0;
    if (ig_sink_open(&sink, path) != 0) {
        printf("FAIL: cannot open %s (errno %d)\n", path, sink.open_errno);
        ++g_fail;
        return;
    }
    ig_run_fc1_probe_env(&sink, fc1_host_env());
    (void)ig_sink_close(&sink);
}

/* After a clean run: 10 reads, each = (request R|E ok, restore 0x4 ok),
   all on 16 KiB pages, requests strictly precede restores. */
static void check_clean_mprotect_log(void) {
    CHECK(fc1_host_ncalls == 20, "clean run makes exactly 20 mprotect calls");
    for (int i = 0; i < fc1_host_ncalls; i += 2) {
        const Fc1HostCall *req = &fc1_host_calls[i];
        const Fc1HostCall *res = &fc1_host_calls[i + 1];
        if (!(req->prot == (1 | 4)) || !(req->rc == 0) ||
            !((req->len % 0x4000ull) == 0 && req->len >= 0x4000ull) ||
            !((req->addr & 0x3fffull) == 0) || !(res->prot == 0x4) ||
            !(res->rc == 0))
            printf("call %d: req(a=%llx l=%llx p=%d rc=%d) "
                   "res(a=%llx l=%llx p=%d rc=%d)\n",
                   i, (unsigned long long)req->addr,
                   (unsigned long long)req->len, req->prot, req->rc,
                   (unsigned long long)res->addr,
                   (unsigned long long)res->len, res->prot, res->rc);
        CHECK(req->prot == (1 | 4), "request adds READ keeping EXEC");
        CHECK(req->rc == 0, "request succeeds");
        CHECK(req->len % 0x4000ull == 0 && req->len >= 0x4000ull,
              "request length is whole 16 KiB pages");
        CHECK((req->addr & 0x3fffull) == 0, "request is page-aligned");
        CHECK(res->prot == 0x4, "restore returns original prot 0x4");
        CHECK(res->rc == 0, "restore succeeds");
        CHECK(res->addr == req->addr && res->len == req->len,
              "restore covers the same pages");
    }
    CHECK(fc1_host_exec_prot() == 0x4,
          "exec arena back to execute-only after all reads");
}

static void scenario_all_pass(const char *dir) {
    fc1_host_reset(1);
    write_scenario(dir, 1, "all_pass");
    check_clean_mprotect_log();
}

static void scenario_target_drift(const char *dir) {
    fc1_host_reset(1);
    fc1_host_corrupt_target(10, 0xff);
    write_scenario(dir, 2, "target_drift");
    CHECK(fc1_host_exec_prot() == 0x4, "target_drift: prot restored");
}

static void scenario_site_edx(const char *dir) {
    fc1_host_reset(1);
    fc1_host_corrupt_site_edx_imm(4, 0x02);   /* Renderer2 window */
    write_scenario(dir, 3, "site_edx_imm");
}

static void scenario_mprotect_refused(const char *dir) {
    fc1_host_reset(1);
    fc1_host_fail_next_mprotect(99);          /* every request refused */
    write_scenario(dir, 4, "mprotect_refused");
    /* 10 requests attempted and refused; NO restore calls (a restore only
       follows a successful mprotect + read, which never happened). */
    CHECK(fc1_host_ncalls == 10, "refused run attempts exactly the 10 requests");
    CHECK(fc1_host_exec_prot() == 0x4, "refused: exec prot untouched");
}

static void scenario_restore_anomaly(const char *dir) {
    fc1_host_reset(1);
    fc1_host_fail_next_restore(1);            /* only the first restore */
    write_scenario(dir, 5, "restore_anomaly");
    CHECK(fc1_host_calls[1].rc != 0, "first restore attempt failed");
    CHECK(fc1_host_calls[1].prot == 0x4, "restore attempted orig prot 0x4");
}

static void scenario_map_untrusted(const char *dir) {
    fc1_host_reset(0);
    write_scenario(dir, 6, "map_untrusted");
    check_clean_mprotect_log();
}

static void scenario_call_drift(const char *dir) {
    fc1_host_reset(1);
    fc1_host_corrupt_site_call_target(5, fc1_host_exec_base() + 0x100);
    write_scenario(dir, 7, "call_rel32_drift");
}

static void scenario_unsupported(const char *dir) {
    fc1_host_reset(1);
    fc1_host_corrupt_site_unsupported(2);
    write_scenario(dir, 8, "unsupported_encoding");
}

/* Pure-decoder spot checks beyond the full-flow scenarios. */
static void check_decoder_units(void) {
    /* Engine1 captured window, relocated by hand for a tiny synthetic
       layout: window at 0x1000, anchor string at 0x2000, target 0x63d590
       (real candidate rt — pure math, no memory access). */
    uint8_t w[28];
    memcpy(w, fc1_fix_window[0], 28);
    const uint64_t w_rt = 0x1000ull, a_rt = 0x2000ull, t_rt = 0x63d590ull;
    uint64_t c_rt = w_rt + 28 - 5;
    uint32_t disp = (uint32_t)(a_rt - (w_rt + 7));
    w[3] = (uint8_t)(disp & 0xff);
    w[4] = (uint8_t)((disp >> 8) & 0xff);
    w[5] = (uint8_t)((disp >> 16) & 0xff);
    w[6] = (uint8_t)((disp >> 24) & 0xff);
    uint32_t rel = (uint32_t)(uint64_t)((int64_t)t_rt - (int64_t)(c_rt + 5));
    w[23] = 0xe8;
    w[24] = (uint8_t)(rel & 0xff);
    w[25] = (uint8_t)((rel >> 8) & 0xff);
    w[26] = (uint8_t)((rel >> 16) & 0xff);
    w[27] = (uint8_t)((rel >> 24) & 0xff);
    Fc1WindowOut wo;
    int st = igfc1_decode_window(w, 28, w_rt, c_rt, t_rt, a_rt, &wo);
    CHECK(st == FC1_WOK, "decoder unit: relocated Engine1 window passes");
    CHECK(wo.rdi_writes == 1 && wo.rdx_writes == 1,
          "decoder unit: exactly one RDI and one RDX write");
    CHECK(wo.lea_rsi_target == a_rt, "decoder unit: lea rsi target");
    CHECK(wo.call_target_rt == t_rt, "decoder unit: call target");

    uint64_t tgt = 0;
    CHECK(igfc1_check_call_bytes(w + 23, c_rt, t_rt, &tgt) == 0 &&
              tgt == t_rt,
          "decoder unit: standalone T1/T2 on captured call bytes");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: host_main <out_dir>\n");
        return 2;
    }
    check_decoder_units();
    scenario_all_pass(argv[1]);
    scenario_target_drift(argv[1]);
    scenario_site_edx(argv[1]);
    scenario_mprotect_refused(argv[1]);
    scenario_restore_anomaly(argv[1]);
    scenario_map_untrusted(argv[1]);
    scenario_call_drift(argv[1]);
    scenario_unsupported(argv[1]);
    if (g_fail == 0) printf("HOST_CHECKS_PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
