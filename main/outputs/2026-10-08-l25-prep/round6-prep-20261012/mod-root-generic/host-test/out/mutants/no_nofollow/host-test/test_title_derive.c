/*
 * test_title_derive.c - Mac host test of the r8 running-title derivation.
 *
 * Compiles the STAGED patched entry (../staged/lifecycle/runtime_module_entry.c,
 * NOT the build's copy) into this test by #include, so the exact patched code
 * runs, statics included. Every platform call the entry makes is stubbed:
 *   sceKernelOpen/Pread/Write/Close -> host open/pread/write/close with every
 *     absolute READ path re-rooted under fixtures/<case>/root (writes go to
 *     out/<case>/<basename>), so the entry reads the fixture's
 *     /data/plugins/ploader.ini and /mnt/sandbox/... files;
 *   diagnostic_*, probe_*, bridge_native_*: recorders / no-ops;
 *   mfa_activate / mfa_status_* / modbridge_runtime_start_with_activation:
 *     recorders, so the test sees which root/title the entry hands over.
 * Each case resets the entry's statics and calls start_composed_runtime().
 * Checks per case: derivation result code, derived title + root, that
 * mfa_activate and the bridge got exactly the derived values on success and
 * were NOT called on failure, that no status file is opened on a failed
 * derivation, the UE4SS.log line, and descriptor balance (opens == closes).
 * Prints the number of cases checked; exits 1 when any check fails or when
 * zero cases ran. Mac-only; no console contact.
 */
#ifdef HOST_UNBOUND_PREAD
/* Variant build (run_host_test.sh step 4b): sceKernelPread is left UNBOUND.
 * The entry's declaration is renamed to a weak-import symbol that nothing
 * defines (linked with -Wl,-U), so its address is NULL at run time, as an
 * unresolved import slot would be. The entry must refuse before any call. */
#include <stddef.h>
#include <stdint.h>
#define sceKernelPread mbt_host_unbound_pread
extern int64_t mbt_host_unbound_pread(int, void *, size_t, int64_t) __attribute__((weak_import));
#endif
#include "../staged/lifecycle/runtime_module_entry.c"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- linker-provided symbols on target; plain storage on the host ---- */
probe_callback __init_array_start[1], __init_array_end[1];
probe_callback __fini_array_start[1], __fini_array_end[1];
unsigned char __eh_frame_start[1], __eh_frame_end[1];
unsigned char __eh_frame_hdr_start[1], __eh_frame_hdr_end[1];

/* ---- fixture plumbing ---- */
static char g_root[1024];
static char g_out[1024];
static int g_opens, g_closes, g_read_opens;

static void remap(const char *path, int writing, char *out, size_t cap)
{
    if (writing) {
        const char *base = strrchr(path, '/');
        snprintf(out, cap, "%s/%s", g_out, base ? base + 1 : path);
    } else {
        snprintf(out, cap, "%s%s", g_root, path);
    }
}

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    char real[2048];
    int writing = (flags & (O_WRONLY | O_RDWR)) != 0;
    remap(path, writing, real, sizeof(real));
    int fd = open(real, flags, mode);
    if (!writing)
        ++g_read_opens;
    if (fd >= 0)
        ++g_opens;
    return fd >= 0 ? fd : (int)(0x80020000u | (unsigned)errno); /* SCE-style negative */
}
#ifndef HOST_UNBOUND_PREAD
int64_t sceKernelPread(int fd, void *buf, size_t n, int64_t off)
{
    ssize_t got = pread(fd, buf, n, (off_t)off);
    return got >= 0 ? (int64_t)got : -1;
}
#endif
int64_t sceKernelWrite(int fd, const void *buf, size_t n)
{
    ssize_t put = write(fd, buf, n);
    return put >= 0 ? (int64_t)put : -1;
}
int sceKernelClose(int fd)
{
    ++g_closes;
    return close(fd);
}

/* ---- diagnostic channel recorder ---- */
static int g_derive_result_seen;
static uint32_t g_derive_result;
static uint32_t g_ini_invalid;
int diagnostic_ready(void) { return 1; }
void diagnostic_event(const char *tag, uint32_t value)
{
    if (strcmp(tag, "title_derive_result") == 0) {
        g_derive_result_seen = 1;
        g_derive_result = value;
    }
    if (strcmp(tag, "title_ini_invalid_sections") == 0)
        g_ini_invalid = value;
}
int diagnostic_open_channel(void) { return 3; }
int diagnostic_close_channel(void) { return 0; }

/* ---- probe / bridge lifecycle stubs (not reached by these cases) ---- */
int probe_lifecycle_start(struct probe_lifecycle *l, const struct probe_lifecycle_ops *o,
                          probe_callback *a, probe_callback *b, probe_callback *c,
                          probe_callback *d, const void *e, const void *f,
                          const void *g, const void *h)
{ (void)l; (void)o; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; return 0; }
int probe_lifecycle_stop(struct probe_lifecycle *l, const struct probe_lifecycle_ops *o)
{ (void)l; (void)o; return 0; }
int probe_call_constructor_catching(probe_callback cb) { (void)cb; return 0; }
int probe_runtime_validate_and_preload(void) { return 0; }
void probe_runtime_log_status(void) {}
int probe_worker_start(void) { return 0; }
int probe_worker_quiesce_join(void) { return 0; }
int probe_worker_is_current(void) { return 0; }
void probe_runtime_release_input(void) {}
int bridge_native_code_residence_required(void) { return 0; }
int bridge_native_pad_code_residence_required(void) { return 0; }
int bridge_native_thread_authority_retire(void) { return 0; }

/* ---- activation / bridge recorders ---- */
static int g_mfa_calls, g_bridge_calls, g_status_opens;
static char g_mfa_root[128], g_mfa_title[32], g_bridge_root[128], g_bridge_title[32];
static int g_fake_activation;
int mfa_activate(const mfa_config *cfg, mfa_activation **out, mfa_error *err)
{
    (void)err;
    ++g_mfa_calls;
    snprintf(g_mfa_root, sizeof(g_mfa_root), "%s", cfg->root ? cfg->root : "(null)");
    snprintf(g_mfa_title, sizeof(g_mfa_title), "%s", cfg->title_id ? cfg->title_id : "(null)");
    *out = (mfa_activation *)&g_fake_activation;
    return MFA_OK;
}
const char *mfa_result_name(int code) { (void)code; return "MFA_STUB"; }
uint64_t mfa_preloaded_bytes(const mfa_activation *a) { (void)a; return 0; }
int mfa_status_open(mfa_status *s, const char *root, long pid, uint64_t now)
{ (void)s; (void)root; (void)pid; (void)now; ++g_status_opens; return -1; }
int mfa_status_write(mfa_status *s, const char *st, int code, const char *d)
{ (void)s; (void)st; (void)code; (void)d; return 0; }
void mfa_status_close(mfa_status *s) { (void)s; }
int modbridge_runtime_start_with_activation(const char *module_file_path, const char *mod_root,
                                            const char *title_id, struct mfa_activation *activation,
                                            char *detail, unsigned detail_capacity)
{
    (void)module_file_path; (void)activation;
    ++g_bridge_calls;
    snprintf(g_bridge_root, sizeof(g_bridge_root), "%s", mod_root);
    snprintf(g_bridge_title, sizeof(g_bridge_title), "%s", title_id);
    if (detail_capacity)
        detail[0] = '\0';
    return MRS_OK;
}
int modbridge_runtime_stop(void) { return 0; }

/* ---- the cases ---- */
struct expect { const char *name; int result; const char *title; };
static const struct expect CASES[] = {
    {"agree_real_townfall_files", MBT_OK, "PPSA33286"},
    {"agree_other_title_two_sections", MBT_OK, "ABCD12345"},
    {"agree_instance_007_plain_name_crlf", MBT_OK, "ZZZZ00001"},
    {"agree_bom_param_json", MBT_OK, "ABCD12345"},
    {"agree_duplicate_section_same_title", MBT_OK, "ABCD12345"},
    {"agree_nested_titleid_ignored", MBT_OK, "ABCD12345"},
    {"fail_missing_ini", MBT_E_INI_MISSING, NULL},
    {"fail_ini_mode_zero", MBT_E_INI_NO_TITLE, NULL},
    {"fail_ini_mode_three", MBT_E_INI_NO_TITLE, NULL},
    {"fail_ini_other_module_only", MBT_E_INI_NO_TITLE, NULL},
    {"fail_ini_invalid_section_name", MBT_E_INI_NO_TITLE, NULL},
    {"fail_ini_line_before_section", MBT_E_INI_NO_TITLE, NULL},
    {"fail_ini_dotdot_path", MBT_E_INI_NO_TITLE, NULL},
    {"fail_ini_section_with_comment_is_not_a_section", MBT_E_NOT_CONFIRMED, NULL},
    {"fail_ini_nul_byte", MBT_E_INI_READ, NULL},
    {"fail_ini_oversized", MBT_E_INI_READ, NULL},
    {"fail_ini_too_many_titles", MBT_E_INI_TOO_MANY, NULL},
    {"fail_no_sandbox_at_all", MBT_E_NOT_CONFIRMED, NULL},
    {"fail_missing_param_json", MBT_E_NOT_CONFIRMED, NULL},
    {"fail_only_unconfigured_title_running", MBT_E_NOT_CONFIRMED, NULL},
    {"fail_disagree_param_title", MBT_E_PARAM_DISAGREE, NULL},
    {"fail_disagree_second_instance", MBT_E_PARAM_DISAGREE, NULL},
    {"fail_ambiguous_two_live", MBT_E_AMBIGUOUS, NULL},
    {"fail_param_truncated", MBT_E_PARAM_JSON, NULL},
    {"fail_param_duplicate_titleid", MBT_E_PARAM_JSON, NULL},
    {"fail_param_nested_titleid_only", MBT_E_PARAM_JSON, NULL},
    {"fail_param_trailing_garbage", MBT_E_PARAM_JSON, NULL},
    {"fail_param_titleid_escaped", MBT_E_PARAM_JSON, NULL},
    {"fail_param_titleid_number", MBT_E_PARAM_JSON, NULL},
    {"fail_param_escaped_root_key", MBT_E_PARAM_JSON, NULL},
    {"fail_param_invalid_utf8", MBT_E_PARAM_JSON, NULL},
    {"fail_param_too_deep", MBT_E_PARAM_JSON, NULL},
    {"fail_param_root_is_array", MBT_E_PARAM_JSON, NULL},
    {"fail_param_oversized", MBT_E_PARAM_READ, NULL},
    {"fail_param_symlink_not_followed", MBT_E_NOT_CONFIRMED, NULL},
};

static int g_failures;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_failures; printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void reset_entry_statics(void)
{
    runtime_started = 0;
    runtime_active = 0;
    entry_guard = 0;
    resolved_title[0] = '\0';
    resolved_mod_root[0] = '\0';
    g_derive_result_seen = 0; g_derive_result = 0xdeadbeefu; g_ini_invalid = 0;
    g_mfa_calls = g_bridge_calls = g_status_opens = 0;
    g_mfa_root[0] = g_mfa_title[0] = g_bridge_root[0] = g_bridge_title[0] = '\0';
    g_opens = g_closes = g_read_opens = 0;
}

static int read_log(char *buf, size_t cap)
{
    char path[1200];
    snprintf(path, sizeof(path), "%s/UE4SS.log", g_out);
    FILE *f = fopen(path, "rb");
    if (!f) { buf[0] = '\0'; return 0; }
    size_t n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
    return 1;
}

/* Direct parser checks on in-memory documents (no files). */
static int json_cases(void)
{
    struct { const char *doc; int ok; } J[] = {
        {"{\"titleId\":\"ABCD12345\"}", 1},
        {" \r\n{ \"a\" : [1, -2.5e+3, true, false, null, {\"b\": \"\\u00e9\\n\"}], \"titleId\" : \"ABCD12345\" } \n", 1},
        {"{\"titleId\":\"ABCD1234\"}", 0},          /* 8 chars */
        {"{\"titleId\":\"ABCD123456\"}", 0},        /* 10 chars */
        {"{\"titleId\":\"abcd12345\"}", 0},         /* lower case */
        {"{\"titleId\":\"../../x/y\"}", 0},         /* path characters */
        {"{\"titleId\":\"ABCD12345\",}", 0},        /* trailing comma */
        {"{\"titleId\":\"ABCD12345\"", 0},          /* unterminated */
        {"{\"titleId\":\"ABCD12345\"}{}", 0},       /* two documents */
        {"{\"n\": 01, \"titleId\":\"ABCD12345\"}", 0}, /* leading zero */
        {"{\"n\": 1., \"titleId\":\"ABCD12345\"}", 0},  /* bad fraction */
        {"{\"s\": \"a\tb\", \"titleId\":\"ABCD12345\"}", 0}, /* raw control char */
        {"{\"s\": \"\\x\", \"titleId\":\"ABCD12345\"}", 0},  /* bad escape */
        {"{\"aVeryLongRootKeyNameBeyondSixteen\": 1, \"titleId\":\"ABCD12345\"}", 1},
        {"{\"titleIdX\": \"ABCD12345\"}", 0},       /* not the key */
        {"", 0},
        {"{}", 0},
    };
    int n = (int)(sizeof(J) / sizeof(J[0]));
    for (int i = 0; i < n; ++i) {
        char out[MBT_TITLE_LEN + 1] = {0};
        int got = mbt_param_json_title((const unsigned char *)J[i].doc, strlen(J[i].doc), out);
        CHECK(got == J[i].ok, "json case %d expected %d got %d (%s)", i, J[i].ok, got, J[i].doc);
    }
    return n;
}

#ifdef HOST_UNBOUND_PREAD
/* One case: the real Townfall files are present and would agree, but the
 * pread binding is unbound. Expect: refusal (-102) before ANY read-open,
 * nothing activated, no status file, the reason in UE4SS.log. */
int main(int argc, char **argv)
{
    const char *here = argc > 1 ? argv[1] : ".";
    char log[8192], cmd[2400];
    snprintf(g_root, sizeof(g_root), "%s/fixtures/agree_real_townfall_files/root", here);
    snprintf(g_out, sizeof(g_out), "%s/out/unbound_pread", here);
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", g_out, g_out);
    if (system(cmd) != 0) { printf("UNBOUND-PREAD TEST: FAIL setup\n"); return 1; }
    (void)CASES; (void)json_cases; /* shared with the main build */
    printf("UNBOUND-PREAD TEST: sceKernelPread address %s\n",
           mbt_host_unbound_pread == NULL ? "NULL (unbound, as intended)" : "NON-NULL (variant broken)");
    if (mbt_host_unbound_pread != NULL) { printf("UNBOUND-PREAD TEST: FAIL - checked 0 case(s)\n"); return 1; }
    reset_entry_statics();
    start_composed_runtime();
    read_log(log, sizeof(log));
    CHECK(g_derive_result_seen, "no title_derive_result event");
    CHECK((int)(int32_t)g_derive_result == MBT_E_INI_READ, "result %d, expected %d",
          (int)(int32_t)g_derive_result, MBT_E_INI_READ);
    CHECK(g_read_opens == 0, "%d read-open(s) happened with pread unbound", g_read_opens);
    CHECK(g_mfa_calls == 0 && g_bridge_calls == 0 && g_status_opens == 0,
          "mfa=%d bridge=%d status=%d after refusal", g_mfa_calls, g_bridge_calls, g_status_opens);
    CHECK(resolved_title[0] == '\0' && resolved_mod_root[0] == '\0', "a title/root survived");
    CHECK(strstr(log, "unbound") != NULL, "UE4SS.log lacks the unbound reason: %s", log);
    printf("  [unbound_pread] %s  result=%d\n      UE4SS.log: %s", g_failures == 0 ? "PASS" : "FAIL",
           (int)(int32_t)g_derive_result, log);
    printf("UNBOUND-PREAD TEST: checked 1 case(s); %d failed check(s)\n", g_failures);
    printf("UNBOUND-PREAD TEST: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
#else
int main(int argc, char **argv)
{
    const char *here = argc > 1 ? argv[1] : ".";
    int n = (int)(sizeof(CASES) / sizeof(CASES[0]));
    int checked = 0;
    printf("TITLE-DERIVE HOST TEST: staged entry, %d fixture case(s)\n", n);
    for (int c = 0; c < n; ++c) {
        const struct expect *e = &CASES[c];
        char log[8192];
        snprintf(g_root, sizeof(g_root), "%s/fixtures/%s/root", here, e->name);
        snprintf(g_out, sizeof(g_out), "%s/out/%s", here, e->name);
        struct stat st;
        if (stat(g_root, &st) != 0) {
            ++g_failures;
            printf("  [%s] FAIL: fixture missing (%s)\n", e->name, g_root);
            continue;
        }
        char cmd[2400];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", g_out, g_out);
        if (system(cmd) != 0) { ++g_failures; continue; }
        reset_entry_statics();
        int before = g_failures;
        start_composed_runtime();
        ++checked;
        CHECK(g_derive_result_seen, "no title_derive_result event");
        CHECK((int)(int32_t)g_derive_result == e->result, "result %d, expected %d",
              (int)(int32_t)g_derive_result, e->result);
        CHECK(g_opens == g_closes, "descriptor imbalance opens=%d closes=%d", g_opens, g_closes);
        read_log(log, sizeof(log));
        if (e->result == MBT_OK) {
            char root[64];
            snprintf(root, sizeof(root), "/data/modbridge/roots/%s", e->title);
            CHECK(strcmp(resolved_title, e->title) == 0, "title '%s' expected '%s'", resolved_title, e->title);
            CHECK(strcmp(resolved_mod_root, root) == 0, "root '%s' expected '%s'", resolved_mod_root, root);
            CHECK(g_mfa_calls == 1 && strcmp(g_mfa_root, root) == 0 && strcmp(g_mfa_title, e->title) == 0,
                  "mfa_activate got root='%s' title='%s' calls=%d", g_mfa_root, g_mfa_title, g_mfa_calls);
            CHECK(g_bridge_calls == 1 && strcmp(g_bridge_root, root) == 0 && strcmp(g_bridge_title, e->title) == 0,
                  "bridge got root='%s' title='%s' calls=%d", g_bridge_root, g_bridge_title, g_bridge_calls);
            CHECK(strstr(log, "modbridge title: ") != NULL && strstr(log, e->title) != NULL,
                  "UE4SS.log lacks the title line: %s", log);
        } else {
            CHECK(resolved_title[0] == '\0' && resolved_mod_root[0] == '\0',
                  "a title/root survived a failure: '%s' '%s'", resolved_title, resolved_mod_root);
            CHECK(g_mfa_calls == 0, "mfa_activate called %d time(s) after a failed derivation", g_mfa_calls);
            CHECK(g_bridge_calls == 0, "bridge called %d time(s) after a failed derivation", g_bridge_calls);
            CHECK(g_status_opens == 0, "status file opened %d time(s) with no derived root", g_status_opens);
            CHECK(strstr(log, "modbridge module_start failed: title_derive_failed") != NULL,
                  "UE4SS.log lacks the failure line: %s", log);
        }
        printf("  [%s] %s  result=%d%s%s\n", e->name, g_failures == before ? "PASS" : "FAIL",
               (int)(int32_t)g_derive_result, e->result == MBT_OK ? " title=" : "",
               e->result == MBT_OK ? resolved_title : "");
        if (strcmp(e->name, "agree_real_townfall_files") == 0 || strcmp(e->name, "fail_disagree_param_title") == 0 ||
            strcmp(e->name, "fail_ambiguous_two_live") == 0)
            printf("      UE4SS.log: %s", log);
    }
    int json_n = json_cases();
    printf("TITLE-DERIVE HOST TEST: checked %d fixture case(s) + %d in-memory JSON case(s); %d failed check(s)\n",
           checked, json_n, g_failures);
    if (checked == 0 || json_n == 0) {
        printf("TITLE-DERIVE HOST TEST: FAIL - zero cases checked\n");
        return 1;
    }
    printf("TITLE-DERIVE HOST TEST: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
#endif /* HOST_UNBOUND_PREAD */
