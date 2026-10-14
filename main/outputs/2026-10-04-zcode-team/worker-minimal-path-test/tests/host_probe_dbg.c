// HOST-ONLY format test (worker-minimal-path-test). Not shipped.
// PS5 kernel calls shimmed to host libc; timestamp stubbed. Line formatting,
// hex encoding, write-all and branch shape are the REAL probe code.
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#define PT_LOG_PATH "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-minimal-path-test/tests/UE4SS.log"
#define PT_LOADLIST_PATH "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-minimal-path-test/tests/loadlist.txt"
#define PT_FALLBACK_PATH "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-minimal-path-test/tests/fallback.log"


#include <stdint.h>
#include <stddef.h>

/* ---- verbatim frozen-contract import identities (see import_references.S).
   Signatures follow the FreeBSD/PS5 kernel ABI; values checked at the only
   use sites (all failures are recorded, never dereferenced blindly). ---- */
extern int *__error(void);
extern int sceKernelOpen(const char *path, int flags, unsigned int mode);
extern long sceKernelWrite(int fd, const void *buf, unsigned long n);
extern int sceKernelClose(int fd);
extern long read(int fd, void *buf, unsigned long nbyte);       /* libScePosix row */
extern int clock_gettime(int clock_id, void *ts);               /* libScePosix row */

/* FreeBSD/PS5 open(2) flags (NOT Linux values). */
#define PT_O_RDONLY  0x0000
#define PT_O_WRONLY  0x0001
#define PT_O_CREAT   0x0200  /* macOS O_CREAT is also 0x0200; shim bug fixed */
#define PT_O_APPEND  0x0008
#define PT_CLOCK_MONOTONIC 4

unsigned g_ctor_events;
unsigned g_fini_events;

static unsigned char entry_guard;

/* ---- freestanding helpers (no libc; keeps the import table exact). ---- */
static char *pt_emit_str(char *p, char *e, const char *s)
{
    while (*s && p < e) *p++ = *s++;
    return p;
}

static char *pt_emit_u64(char *p, char *e, uint64_t v)
{
    char tmp[24];
    int n = 0;
    do { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; } while (v && n < 24);
    while (n-- && p < e) *p++ = tmp[n];
    return p;
}

static char *pt_emit_i64(char *p, char *e, long v)
{
    unsigned long m = (v < 0) ? (unsigned long)-(v + 1) + 1ul : (unsigned long)v;
    if (v < 0 && p < e) *p++ = '-';
    return pt_emit_u64(p, e, m);
}

static char *pt_emit_hex(char *p, char *e, const unsigned char *b, int n)
{
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < n && p + 1 < e; ++i) {
        *p++ = hx[(b[i] >> 4) & 0xF];
        *p++ = hx[b[i] & 0xF];
    }
    return p;
}

/* Bounded write-all: at most 64 attempts, advances on partial writes.
   Returns 0 when the full buffer was written. */
static int pt_write_all(int fd, const char *buf, unsigned long len)
{
    unsigned long off = 0;
    for (int attempts = 0; attempts < 64 && off < len; ++attempts) {
        long w = sceKernelWrite(fd, buf + off, len - off);
        if (w <= 0) return -1;
        off += (unsigned long)w;
    }
    return (off == len) ? 0 : -1;
}

/* ---- probe state (static: no stack pressure, no reentrancy). ---- */
static char s_line1[176];
static char s_line2[208];
static char s_fb1[192];
static unsigned char s_rbuf[16];

__attribute__((visibility("default")))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;

    /* Boot-relative timestamp FIRST so it stamps module_start itself. */
    struct { long tv_sec; long tv_nsec; } ts;
    ts.tv_sec = 0; ts.tv_nsec = 0;
    int ts_ok = clock_gettime(PT_CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;

    /* ---- ACTIVATION_TEST data first (both lines leave together). ---- */
    char *p2 = s_line2;
    char *e2 = s_line2 + sizeof(s_line2);
    p2 = pt_emit_str(p2, e2,
        "ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: ");
    int lfd = sceKernelOpen(PT_LOADLIST_PATH, PT_O_RDONLY, 0);
    if (lfd < 0) {
        p2 = pt_emit_str(p2, e2, "OPEN_FAILED errno=");
        p2 = pt_emit_i64(p2, e2, (long)*__error());
    } else {
        long n = read(lfd, s_rbuf, 16);
        int rc = sceKernelClose(lfd);
        if (n < 0) {
            p2 = pt_emit_str(p2, e2, "OPEN_OK_READ_FAILED errno=");
            p2 = pt_emit_i64(p2, e2, (long)*__error());
        } else if (n == 16) {
            p2 = pt_emit_hex(p2, e2, s_rbuf, 16);
        } else {
            /* short read (includes n==0): still evidence the path read. */
            p2 = pt_emit_hex(p2, e2, s_rbuf, (int)n);
            p2 = pt_emit_str(p2, e2, " (short n=");
            p2 = pt_emit_i64(p2, e2, n);
            if (rc != 0) { p2 = pt_emit_str(p2, e2, ",close="); p2 = pt_emit_i64(p2, e2, rc); }
            p2 = pt_emit_str(p2, e2, ")");
        }
    }
    if (p2 < e2) *p2++ = '\n';

    char *p1 = s_line1;
    char *e1 = s_line1 + sizeof(s_line1);
    p1 = pt_emit_str(p1, e1, "PATH_TEST: " PT_LOG_PATH " writable at ");
    p1 = pt_emit_u64(p1, e1, ns);
    p1 = pt_emit_str(p1, e1, ts_ok ? " monotonic-ns\n" : " monotonic-ns CLOCK_FAILED\n");

    /* ---- Primary channel: the output path under test. ---- */
    fprintf(stderr, "TRACE p1=%ld line1=[%.*s]\n", (long)(p1 - s_line1),
            (int)(p1 - s_line1), s_line1);
    fprintf(stderr, "TRACE p2=%ld line2=[%.*s]\n", (long)(p2 - s_line2),
            (int)(p2 - s_line2), s_line2);
    int fd = sceKernelOpen(PT_LOG_PATH, PT_O_WRONLY | PT_O_CREAT | PT_O_APPEND,
                           0644);
    if (fd >= 0) {
        (void)pt_write_all(fd, s_line1, (unsigned long)(p1 - s_line1));
        (void)pt_write_all(fd, s_line2, (unsigned long)(p2 - s_line2));
        (void)sceKernelClose(fd);
    } else {
        /* Primary path refused the open. The mission answer for UE4SS.log is
           then "not writable", but the activation result must not vanish:
           one fallback file in the same directory records the errno and both
           lines. If THIS also fails, /data/plugins itself is not writable. */
        int e = *__error();
        int ffd = sceKernelOpen(PT_FALLBACK_PATH,
                                PT_O_WRONLY | PT_O_CREAT | PT_O_APPEND, 0644);
        if (ffd >= 0) {
            char *f1 = s_fb1;
            char *fe1 = s_fb1 + sizeof(s_fb1);
            f1 = pt_emit_str(f1, fe1, "PATH_TEST: " PT_LOG_PATH
                             " OPEN_FAILED errno=");
            f1 = pt_emit_i64(f1, fe1, (long)e);
            f1 = pt_emit_str(f1, fe1, " at ");
            f1 = pt_emit_u64(f1, fe1, ns);
            f1 = pt_emit_str(f1, fe1, " monotonic-ns\n");
            (void)pt_write_all(ffd, s_fb1, (unsigned long)(f1 - s_fb1));
            (void)pt_write_all(ffd, s_line2, (unsigned long)(p2 - s_line2));
            (void)sceKernelClose(ffd);
        }
    }

    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}

__attribute__((visibility("default")))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return 0;
}

/* ---- host shims (after the source's extern decls) ---- */
int clock_gettime(int clock_id, void *ts) { (void)clock_id;
    struct { long s; long ns; } *t = ts; t->s = 111; t->ns = 222; return 0; }
int sceKernelOpen(const char *p, int f, unsigned int m) { return open(p, f, (int)m); }
long sceKernelWrite(int fd, const void *b, unsigned long n) { return (long)write(fd, b, (size_t)n); }
int sceKernelClose(int fd) { return close(fd); }
int main(void) { return module_start(0, 0); }
