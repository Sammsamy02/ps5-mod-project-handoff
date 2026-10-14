/* mfa_host.c — host fake of the vendored modforge_activation C API.
 * Fails on demand (MRS_FAKE_MFA_FAIL=<code>) and records the status-file
 * protocol exactly (append-only whole lines) so the test can prove the
 * failure observability contract. */
#include <modforge_activation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int mfa_release_count;

int mfa_activate(const mfa_config *cfg, mfa_activation **out, mfa_error *err)
{
    *out = NULL;
    if (err != NULL) {
        memset(err, 0, sizeof(*err));
        err->code = MFA_E_NO_LOADLIST;
    }
    int fail = 0;
    const char *f = getenv("MRS_FAKE_MFA_FAIL");
    if (f != NULL)
        fail = atoi(f);
    if (fail != 0) {
        if (err != NULL) {
            err->code = fail;
            err->os_error = 2;
            snprintf(err->message, sizeof(err->message), "fake activation refusal");
            snprintf(err->path, sizeof(err->path), "loadlist.txt");
        }
        return fail;
    }
    if (cfg == NULL || cfg->root == NULL || cfg->title_id == NULL)
        return MFA_E_INVALID;
    *out = malloc(8); /* opaque dummy activation */
    return MFA_OK;
}

void mfa_release(mfa_activation *a)
{
    if (a != NULL)
        ++mfa_release_count;
    free(a);
}

const char *mfa_result_name(int code)
{
    switch (code) {
    case MFA_OK: return "MFA_OK";
    case MFA_E_NO_LOADLIST: return "MFA_E_NO_LOADLIST";
    case MFA_E_TITLE: return "MFA_E_TITLE";
    case MFA_E_OPEN: return "MFA_E_OPEN";
    default: return "MFA_E_OTHER";
    }
}

uint64_t mfa_preloaded_bytes(const mfa_activation *a)
{
    (void)a;
    return 0x123456789au;
}

/* --- status file: same file protocol as the vendored implementation --- */

int mfa_status_open(mfa_status *s, const char *root, long pid, uint64_t now_ms)
{
    (void)root; /* the real vendored join is <root>/runtime-status.txt; the
                 * fake writes to $MRS_STATUS_PATH for direct inspection */
    const char *p = getenv("MRS_STATUS_PATH");
    s->fd = open(p != NULL ? p : "/tmp/mrs-status.txt",
                 O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (s->fd < 0)
        return -1;
    s->seq = 0;
    char line[128];
    int n = snprintf(line, sizeof(line),
                     "modforge-runtime-status\t1\nsession\t%ld\t%llu\n", pid,
                     (unsigned long long)now_ms);
    if (n > 0)
        (void)!write(s->fd, line, (size_t)n);
    return 0;
}

int mfa_status_write(mfa_status *s, const char *state, int code, const char *detail)
{
    if (s->fd < 0)
        return -9;
    char line[256];
    int n = snprintf(line, sizeof(line), "state\t%u\t%s\t%d\t%s\n",
                     ++s->seq, state, code, detail != NULL ? detail : "");
    if (n > 0)
        (void)!write(s->fd, line, (size_t)n);
    return 0;
}

int mfa_status_note(mfa_status *s, const char *key, const char *text)
{
    if (s->fd < 0)
        return -9;
    char line[256];
    int n = snprintf(line, sizeof(line), "note\t%u\t%s\t%s\n", ++s->seq, key,
                     text != NULL ? text : "");
    if (n > 0)
        (void)!write(s->fd, line, (size_t)n);
    return 0;
}

void mfa_status_close(mfa_status *s)
{
    if (s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
}
