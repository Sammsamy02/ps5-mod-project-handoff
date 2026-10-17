/*
 * gdbstub.c - GDB Remote Serial Protocol (RSP) server core.
 *
 * Protocol reference: "Remote Serial Protocol" appendix of the GDB manual
 * (https://sourceware.org/gdb/current/onlinedocs/gdb.html/Remote-Protocol.html).
 * The same wire protocol is spoken by LLDB ("gdb-remote" / platform).
 *
 * Implemented packets:
 *   ?                halt reason (always "stopped": live-inspection model)
 *   g / G            read / write full register file (536-byte x86-64 layout)
 *   p n / P n=r      read / write single register
 *   m addr,len       read memory  (hex encoded)
 *   M addr,len:d     write memory (hex encoded)
 *   X addr,len:d     write memory (binary, RSP-escaped)
 *   Z0/z0            software breakpoint set/remove (0xCC patch)
 *   c / s / vCont    continue/step: answered with an immediate stop reply
 *                    (phase-1 inspection-only; real execution control needs
 *                    kernel stop/cont, see REPORT)
 *   vAttach;pid      attach to process id (routes to backend->attach)
 *   qSupported, QStartNoAckMode, qC, qAttached, H, qfThreads/qsThreads,
 *   qXfer:features:read:target.xml, qXfer:threads:read,
 *   qHostInfo, qProcessInfo, qRegisterInfoNNN (LLDB fallback),
 *   qRcmd (monitor: help / info / pid <n>)
 *   D / k / vKill    detach (NEVER kills the real game: answered OK + close)
 *   raw 0x03 byte    interrupt -> T02 stop reply
 *
 * Unhandled packets get an empty response, which per the spec means
 * "unsupported" and every client falls back gracefully.
 *
 * Wire layer: $<data>#<cksum> with '+'/'-' acks, '}' escaping, '*' RLE
 * decoding on receive (some GDBs RLE-compress M/X packets), optional
 * no-ack mode after QStartNoAckMode.
 */

#include "gdbstub.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stddef.h>

/* =====================================================================
 * Register table (drives target.xml, 'g'/'G', 'p'/'P', qRegisterInfo)
 * ===================================================================== */

#define OFF(field) ((uint16_t)(size_t)offsetof(struct gdb_regs, field))

const struct gdb_reg_desc gdb_reg_table[GDB_NUM_REGS] = {
    /* clang-format off */
    /* regnum bytes offset-in-struct                          group     */
    { "rax",    0,  8, OFF(gpr) + 0,  NULL },
    { "rbx",    1,  8, OFF(gpr) + 8,  NULL },
    { "rcx",    2,  8, OFF(gpr) + 16, NULL },
    { "rdx",    3,  8, OFF(gpr) + 24, NULL },
    { "rsi",    4,  8, OFF(gpr) + 32, NULL },
    { "rdi",    5,  8, OFF(gpr) + 40, NULL },
    { "rbp",    6,  8, OFF(gpr) + 48, NULL },
    { "rsp",    7,  8, OFF(gpr) + 56, NULL },
    { "r8",     8,  8, OFF(gpr) + 64, NULL },
    { "r9",     9,  8, OFF(gpr) + 72, NULL },
    { "r10",   10,  8, OFF(gpr) + 80, NULL },
    { "r11",   11,  8, OFF(gpr) + 88, NULL },
    { "r12",   12,  8, OFF(gpr) + 96, NULL },
    { "r13",   13,  8, OFF(gpr) + 104, NULL },
    { "r14",   14,  8, OFF(gpr) + 112, NULL },
    { "r15",   15,  8, OFF(gpr) + 120, NULL },
    { "rip",   16,  8, OFF(rip),       NULL },
    { "eflags",17,  4, OFF(eflags),    NULL },
    { "cs",    18,  4, OFF(seg) + 0,   NULL },
    { "ss",    19,  4, OFF(seg) + 4,   NULL },
    { "ds",    20,  4, OFF(seg) + 8,   NULL },
    { "es",    21,  4, OFF(seg) + 12,  NULL },
    { "fs",    22,  4, OFF(seg) + 16,  NULL },
    { "gs",    23,  4, OFF(seg) + 20,  NULL },
    { "st0",   24, 10, OFF(st) + 0,    "float" },
    { "st1",   25, 10, OFF(st) + 10,   "float" },
    { "st2",   26, 10, OFF(st) + 20,   "float" },
    { "st3",   27, 10, OFF(st) + 30,   "float" },
    { "st4",   28, 10, OFF(st) + 40,   "float" },
    { "st5",   29, 10, OFF(st) + 50,   "float" },
    { "st6",   30, 10, OFF(st) + 60,   "float" },
    { "st7",   31, 10, OFF(st) + 70,   "float" },
    { "fctrl", 32,  4, OFF(fpu) + 0,   "float" },
    { "fstat", 33,  4, OFF(fpu) + 4,   "float" },
    { "ftag",  34,  4, OFF(fpu) + 8,   "float" },
    { "fiseg", 35,  4, OFF(fpu) + 12,  "float" },
    { "fioff", 36,  4, OFF(fpu) + 16,  "float" },
    { "foseg", 37,  4, OFF(fpu) + 20,  "float" },
    { "fooff", 38,  4, OFF(fpu) + 24,  "float" },
    { "fop",   39,  4, OFF(fpu) + 28,  "float" },
    { "xmm0",  40, 16, OFF(xmm) + 0,   "vector" },
    { "xmm1",  41, 16, OFF(xmm) + 16,  "vector" },
    { "xmm2",  42, 16, OFF(xmm) + 32,  "vector" },
    { "xmm3",  43, 16, OFF(xmm) + 48,  "vector" },
    { "xmm4",  44, 16, OFF(xmm) + 64,  "vector" },
    { "xmm5",  45, 16, OFF(xmm) + 80,  "vector" },
    { "xmm6",  46, 16, OFF(xmm) + 96,  "vector" },
    { "xmm7",  47, 16, OFF(xmm) + 112, "vector" },
    { "xmm8",  48, 16, OFF(xmm) + 128, "vector" },
    { "xmm9",  49, 16, OFF(xmm) + 144, "vector" },
    { "xmm10", 50, 16, OFF(xmm) + 160, "vector" },
    { "xmm11", 51, 16, OFF(xmm) + 176, "vector" },
    { "xmm12", 52, 16, OFF(xmm) + 192, "vector" },
    { "xmm13", 53, 16, OFF(xmm) + 208, "vector" },
    { "xmm14", 54, 16, OFF(xmm) + 224, "vector" },
    { "xmm15", 55, 16, OFF(xmm) + 240, "vector" },
    { "mxcsr", 56,  4, OFF(mxcsr),     "vector" },
    /* clang-format on */
};

/* ---- target.xml (self-contained; no xi:include so every client parses
 * it standalone).  Generated once from gdb_reg_table so the XML can never
 * drift from the 'g'-packet layout. ------------------------------------*/

static char target_xml_buf[8192];
static char threads_xml_buf[256];

static const char *reg_xml_type(const struct gdb_reg_desc *r)
{
    if (r->regnum >= 40 && r->regnum <= 55) return "bytes16";   /* xmm   */
    if (r->regnum >= 24 && r->regnum <= 31) return "bytes10";   /* st    */
    if (r->regnum == 17)                    return "i386_eflags";
    if (r->regnum == 16)                    return "code_ptr";
    if (r->bytes == 8)                      return "int64";
    return "int32";
}

const char *gdb_target_xml(void)
{
    if (target_xml_buf[0])
        return target_xml_buf;

    char *p   = target_xml_buf;
    char *end = target_xml_buf + sizeof(target_xml_buf);
#define XPRINT(...)                                                           \
    do {                                                                      \
        if (p < end)                                                          \
            p += snprintf(p, (size_t)(end - p), __VA_ARGS__);                 \
    } while (0)

    XPRINT("<?xml version=\"1.0\"?>\n"
           "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n"
           "<target>\n"
           "  <architecture>i386:x86-64</architecture>\n"
           "  <feature name=\"org.gnu.gdb.i386.core\">\n"
           "    <flags id=\"i386_eflags\" size=\"4\">\n");
    static const char *const flags[] = {
        "CF", "0", "PF", "2", "AF", "4", "ZF", "6", "SF", "7",
        "TF", "8", "IF", "9", "DF", "10", "OF", "11",
    };
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); i += 2)
        XPRINT("      <field name=\"%s\" start=\"%s\" end=\"%s\"/>\n",
               flags[i], flags[i + 1], flags[i + 1]);
    XPRINT("    </flags>\n"
           "    <vector id=\"bytes10\" type=\"uint8\" count=\"10\"/>\n"
           "    <vector id=\"bytes16\" type=\"uint8\" count=\"16\"/>\n");

    for (int i = 0; i < GDB_NUM_REGS; i++) {
        const struct gdb_reg_desc *r = &gdb_reg_table[i];
        char line[256];
        if (r->regnum == 40)
            XPRINT("  </feature>\n"
                   "  <feature name=\"org.gnu.gdb.i386.sse\">\n");
        if (r->group)
            snprintf(line, sizeof(line),
                     "    <reg name=\"%s\" regnum=\"%u\" bitsize=\"%u\" "
                     "type=\"%s\" group=\"%s\"/>\n",
                     r->name, r->regnum, r->bytes * 8, reg_xml_type(r),
                     r->group);
        else
            snprintf(line, sizeof(line),
                     "    <reg name=\"%s\" regnum=\"%u\" bitsize=\"%u\" "
                     "type=\"%s\"/>\n",
                     r->name, r->regnum, r->bytes * 8, reg_xml_type(r));
        XPRINT("%s", line);
    }
    XPRINT("  </feature>\n</target>\n");
    return target_xml_buf;
#undef XPRINT
}

const char *gdb_threads_xml(void)
{
    snprintf(threads_xml_buf, sizeof(threads_xml_buf),
             "<?xml version=\"1.0\"?>\n<threads>\n"
             "  <thread id=\"1\" name=\"GameThread\" core=\"0\"/>\n"
             "</threads>\n");
    return threads_xml_buf;
}

/* Sanity-check the generated XML once at startup. */
static void xml_selfcheck(void)
{
    const char *x = gdb_target_xml();
    if (!strstr(x, "<architecture>i386:x86-64</architecture>") ||
        !strstr(x, "name=\"rip\"") || !strstr(x, "name=\"mxcsr\"") ||
        !strstr(x, "name=\"xmm15\"")) {
        fprintf(stderr, "gdbstub: FATAL: generated target.xml broken\n");
        exit(2);
    }
}

/* =====================================================================
 * Wire helpers
 * ===================================================================== */

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hexnib(int v) { return "0123456789abcdef"[v & 0xf]; }

/* addr,len,... parsers; all values are hex without 0x prefix */
static int parse_hex(const char **pp, uint64_t *out)
{
    uint64_t v = 0;
    int n = 0, d;
    while ((d = hexval((unsigned char)**pp)) >= 0) {
        v = (v << 4) | (uint64_t)d;
        (*pp)++;
        if (++n > 16)
            return -1;
    }
    *out = v;
    return n ? 0 : -1;
}

/* "addr,len" both hex; returns 0 on success */
static int parse_hex2(const char **pp, uint64_t *a, uint64_t *b)
{
    const char *p = *pp;
    if (parse_hex(&p, a) < 0 || *p != ',')
        return -1;
    p++;
    if (parse_hex(&p, b) < 0)
        return -1;
    *pp = p;
    return 0;
}

struct conn {
    int  fd;
    int  noack;
    const struct gdb_backend *be;
    uint64_t pid;
};

/* Read exactly one byte; returns -1 on EOF/error. */
static int rdbyte(int fd)
{
    unsigned char c;
    ssize_t n;
    do {
        n = recv(fd, &c, 1, 0);
    } while (n < 0 && errno == EINTR);
    if (n != 1)
        return -1;
    return c;
}

static int send_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Send one RSP packet with framing, escaping and checksum.  In ack mode,
 * waits for '+' (retransmit up to 3 times on '-'). */
static int putpkt(struct conn *c, const char *data, size_t len)
{
    char frame[GDB_PACKET_SIZE + 16];
    size_t o = 0;
    unsigned csum = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)data[i];
        switch (ch) {
        case '$': case '#': case '}': case '*':
            csum = (csum + '}' + (ch ^ 0x20)) & 0xff;
            frame[o++] = '}';
            frame[o++] = (char)(ch ^ 0x20);
            break;
        default:
            csum = (csum + ch) & 0xff;
            frame[o++] = (char)ch;
        }
    }
    frame[o++] = '#';
    frame[o++] = (char)hexnib((int)(csum >> 4));
    frame[o++] = (char)hexnib((int)(csum & 0xf));

    for (int attempt = 0; attempt < 4; attempt++) {
        char hdr[1] = { '$' };
        if (send_all(c->fd, hdr, 1) || send_all(c->fd, frame, o))
            return -1;
        if (c->noack)
            return 0;
        int a = rdbyte(c->fd);
        if (a == '+')
            return 0;
        if (a == -1)
            return -1;
        /* '-' or anything else: retransmit */
    }
    return -1;
}

/* Receive one packet (decoded: escapes resolved, RLE expanded).
 * Sends '+'/'-' acks in ack mode.  Returns packet length, -1 on EOF/error
 * or async interrupt (sets *intr for 0x03), -2 when a packet was dropped
 * because of a bad checksum in no-ack mode (no retransmit exists there). */
static int getpkt(struct conn *c, char *buf, size_t maxlen, int *intr)
{
    int ch;

    *intr = 0;
    for (;;) {
        ch = rdbyte(c->fd);
        if (ch == -1)
            return -1;
        if (ch == 0x03) {          /* async interrupt outside any packet */
            *intr = 1;
            return -1;
        }
        if (ch == '+' || ch == '-')
            continue;              /* ack noise from our sent packets */
        if (ch == '$')
            break;
        /* stray byte: ignore */
    }

    char raw[GDB_PACKET_SIZE];
    size_t rlen = 0;
    for (;;) {
        if (rlen >= sizeof(raw))
            goto resend_nak;       /* oversized: drop */
        ch = rdbyte(c->fd);
        if (ch == -1)
            return -1;
        if (ch == '#')
            break;
        raw[rlen++] = (char)ch;
    }

    int c1 = hexval(rdbyte(c->fd));
    int c2 = hexval(rdbyte(c->fd));
    if (c1 < 0 || c2 < 0)
        goto resend_nak;

    unsigned csum = 0;
    for (size_t i = 0; i < rlen; i++)
        csum = (csum + (unsigned char)raw[i]) & 0xff;
    if (csum != (unsigned)((c1 << 4) | c2)) {
        if (c->noack)
            return -2;             /* no retransmit in no-ack: drop */
        goto resend_nak;
    }

    if (!c->noack) {
        char ack[1] = { '+' };
        send_all(c->fd, ack, 1);
    }

    /* Decode: '}' escape, then '*' RLE (byte repeated (N-29) extra
     * times, N in [30..126]). */
    size_t o = 0;
    for (size_t i = 0; i < rlen; i++) {
        unsigned char b = (unsigned char)raw[i];
        if (b == '}') {
            if (++i >= rlen)
                return -1;
            b = (unsigned char)raw[i] ^ 0x20;
        } else if (b == '*' && i + 1 < rlen && o > 0) {
            int rep = (unsigned char)raw[++i] - 29;
            if (rep < 1 || rep > 97)
                return -1;
            while (rep-- > 0) {
                if (o >= maxlen)
                    return -1;
                buf[o] = buf[o - 1];
                o++;
            }
            continue;
        }
        if (o >= maxlen)
            return -1;
        buf[o++] = (char)b;
    }
    return (int)o;

resend_nak:
    if (!c->noack) {
        char nak[1] = { '-' };
        send_all(c->fd, nak, 1);
    }
    /* client will retransmit; recurse to read the next attempt */
    return getpkt(c, buf, maxlen, intr);
}

/* =====================================================================
 * Reply helpers
 * ===================================================================== */

static void reply(struct conn *c, const char *s)
{
    putpkt(c, s, strlen(s));
}

static void reply_err(struct conn *c, int err)
{
    char b[16];
    snprintf(b, sizeof(b), "E%02x", err);
    reply(c, b);
}

/* =====================================================================
 * Register packing
 * ===================================================================== */

static int pack_regs(const struct gdb_regs *regs, char *out /* 2*536+1 */)
{
    const unsigned char *r = (const unsigned char *)regs;
    size_t o = 0;
    for (int i = 0; i < GDB_NUM_REGS; i++) {
        const struct gdb_reg_desc *d = &gdb_reg_table[i];
        if (d->roff + d->bytes > sizeof(*regs))
            return -1;
        for (unsigned b = 0; b < d->bytes; b++) {
            unsigned char v = r[d->roff + b];
            out[o++] = (char)hexnib(v >> 4);
            out[o++] = (char)hexnib(v & 0xf);
        }
    }
    out[o] = 0;
    return (int)o;
}

static int unpack_regs(const char *hex, struct gdb_regs *regs)
{
    unsigned char *r = (unsigned char *)regs;
    size_t o = 0;
    for (int i = 0; i < GDB_NUM_REGS; i++) {
        const struct gdb_reg_desc *d = &gdb_reg_table[i];
        for (unsigned b = 0; b < d->bytes; b++) {
            int hi = hexval((unsigned char)hex[o++]);
            int lo = hexval((unsigned char)hex[o++]);
            if (hi < 0 || lo < 0)
                return -1;
            r[d->roff + b] = (unsigned char)((hi << 4) | lo);
        }
    }
    return 0;
}

/* hex dump of one register */
static int pack_one_reg(const struct gdb_regs *regs, unsigned regnum,
                        char *out /* 2*16+1 */)
{
    if (regnum >= GDB_NUM_REGS)
        return -1;
    const struct gdb_reg_desc *d = &gdb_reg_table[regnum];
    const unsigned char *r = (const unsigned char *)regs;
    size_t o = 0;
    for (unsigned b = 0; b < d->bytes; b++) {
        out[o++] = (char)hexnib(r[d->roff + b] >> 4);
        out[o++] = (char)hexnib(r[d->roff + b] & 0xf);
    }
    out[o] = 0;
    return 0;
}

/* =====================================================================
 * qXfer object serving (offset,length are hex per spec)
 * ===================================================================== */

static void serve_xfer_read(struct conn *c, const char *obj,
                            const char *annex, const char *rest)
{
    uint64_t off = 0, len = 0;
    const char *content = NULL;

    if (parse_hex2(&rest, &off, &len) < 0) {
        reply(c, "E00");           /* malformed offset,length */
        return;
    }

    if (!strcmp(obj, "features") && !strcmp(annex, "target.xml"))
        content = gdb_target_xml();
    else if (!strcmp(obj, "threads"))
        content = gdb_threads_xml();

    if (!content) {
        reply(c, "E00");           /* unknown object/annex */
        return;
    }

    uint64_t clen = strlen(content);
    if (off >= clen) {
        reply(c, "l");             /* at/after end: empty, done */
        return;
    }
    if (len > 1024)
        len = 1024;
    if (off + len > clen)
        len = clen - off;

    char out[2048 + 4];
    out[0] = 'm';
    memcpy(out + 1, content + off, len);
    out[1 + len] = 0;
    reply(c, out);
}

/* =====================================================================
 * qRcmd (monitor) support
 * ===================================================================== */

static void monitor_out(struct conn *c, const char *text)
{
    char hex[1024];
    size_t o = 0;
    for (const char *p = text; *p && o + 2 < sizeof(hex); p++) {
        hex[o++] = (char)hexnib((unsigned char)*p >> 4);
        hex[o++] = (char)hexnib(*p & 0xf);
    }
    hex[o] = 0;
    char pkt[1200];
    snprintf(pkt, sizeof(pkt), "O%s", hex);
    reply(c, pkt);
}

static void handle_monitor(struct conn *c, const char *cmdhex)
{
    char cmd[512] = { 0 };
    size_t o = 0;
    for (const char *p = cmdhex; p[0] && p[1] && o + 1 < sizeof(cmd); p += 2) {
        int hi = hexval((unsigned char)p[0]);
        int lo = hexval((unsigned char)p[1]);
        if (hi < 0 || lo < 0)
            break;
        cmd[o++] = (char)((hi << 4) | lo);
    }

    char line[256];
    if (!strncmp(cmd, "pid ", 4)) {
        char *end = NULL;
        unsigned long long pid = strtoull(cmd + 4, &end, 0);
        if (end && *end == 0 && c->be->attach &&
            c->be->attach(c->be->ctx, pid) == 0) {
            c->pid = pid;
            snprintf(line, sizeof(line),
                     "attached to pid %llu (0x%llx)\n",
                     pid, pid);
        } else {
            snprintf(line, sizeof(line), "attach to '%s' failed\n", cmd + 4);
        }
        monitor_out(c, line);
    } else if (!strcmp(cmd, "info")) {
        snprintf(line, sizeof(line),
                 "backend=%s pid=%llu (%#llx) arch=x86-64 "
                 "inspection-only phase-1\n",
                 c->be->name, (unsigned long long)c->pid,
                 (unsigned long long)c->pid);
        monitor_out(c, line);
    } else {
        monitor_out(c,
                    "commands: pid <n>   - attach to process id\n"
                    "          info      - backend status\n");
    }
    reply(c, "OK");
}

/* =====================================================================
 * LLDB flavor packets
 * ===================================================================== */

static void hexstr(char *dst, size_t dstsz, const char *s)
{
    size_t o = 0;
    for (; *s && o + 2 < dstsz; s++) {
        dst[o++] = (char)hexnib((unsigned char)*s >> 4);
        dst[o++] = (char)hexnib(*s & 0xf);
    }
    dst[o] = 0;
}

static void handle_qHostInfo(struct conn *c)
{
    char triple[128], out[512];
    hexstr(triple, sizeof(triple), "x86_64-unknown-freebsd12");
    snprintf(out, sizeof(out),
             "triple:%s;endian:little;ptrsize:8;pid:%llx;ostype:%s;vendor:%s;",
             triple, (unsigned long long)c->pid, "66726565627364" /*freebsd*/,
             "756e6b6e6f776e" /*unknown*/);
    reply(c, out);
}

static void handle_qProcessInfo(struct conn *c)
{
    char triple[128], out[512];
    hexstr(triple, sizeof(triple), "x86_64-unknown-freebsd12");
    snprintf(out, sizeof(out),
             "pid:%llx;parent:0;real_pid:%llx;triple:%s;endian:little;"
             "ptrsize:8;",
             (unsigned long long)c->pid, (unsigned long long)c->pid, triple);
    reply(c, out);
}

static void handle_qRegisterInfo(struct conn *c, const char *rest)
{
    uint64_t n;
    const char *p = rest;
    if (parse_hex(&p, &n) < 0 || n >= GDB_NUM_REGS) {
        reply(c, "E22");
        return;
    }
    const struct gdb_reg_desc *d = &gdb_reg_table[n];
    char out[256];
    const char *set = (d->regnum >= 40) ? "Advanced Vector Extensions"
                    : (d->regnum >= 24) ? "Floating Point Registers"
                                        : "General Purpose Registers";
    snprintf(out, sizeof(out),
             "name:%s;bitsize:%u;offset:%u;size:%u;encoding:uint;format:hex;"
             "set:%s;gcc:%u;",
             d->name, d->bytes * 8, d->roff, d->bytes, set, d->regnum);
    reply(c, out);
}

/* =====================================================================
 * Main packet dispatch
 * ===================================================================== */

static void handle_packet(struct conn *c, const char *in, size_t inlen,
                          int *done)
{
    char out[GDB_PACKET_SIZE];
    const struct gdb_backend *be = c->be;

    out[0] = 0;
    if (inlen == 0) {              /* empty keep-alive packet */
        reply(c, "");
        return;
    }

    switch (in[0]) {

    case '?':
        reply(c, "T05thread:1;");
        break;

    case 'c':                      /* continue: phase-1 answers with an
                                    * immediate stop (live inspection) */
    case 's':                      /* step: same */
        reply(c, "T05thread:1;");
        break;

    case 'g': {
        struct gdb_regs regs;
        if (!be->read_regs || be->read_regs(be->ctx, &regs) != 0) {
            reply_err(c, 1);
            break;
        }
        pack_regs(&regs, out);     /* out is 32 KiB, g packet is 1073 */
        reply(c, out);
        break;
    }

    case 'G': {
        struct gdb_regs regs;
        if (!be->write_regs ||
            unpack_regs(in + 1, &regs) != 0 ||
            be->write_regs(be->ctx, &regs) != 0) {
            reply_err(c, 1);
            break;
        }
        reply(c, "OK");
        break;
    }

    case 'p': {
        uint64_t n;
        const char *p = in + 1;
        struct gdb_regs regs;
        if (parse_hex(&p, &n) < 0 || n >= GDB_NUM_REGS ||
            !be->read_regs || be->read_regs(be->ctx, &regs) != 0 ||
            pack_one_reg(&regs, (unsigned)n, out) != 0) {
            reply_err(c, 1);
            break;
        }
        reply(c, out);
        break;
    }

    case 'P': {
        uint64_t n;
        const char *p = in + 1;
        char *eq = strchr(in, '=');
        if (parse_hex(&p, &n) < 0 || n >= GDB_NUM_REGS || !eq ||
            !be->write_reg) {
            reply_err(c, 1);
            break;
        }
        const char *hex = eq + 1;
        const struct gdb_reg_desc *d = &gdb_reg_table[n];
        unsigned nb = d->bytes;
        if (strlen(hex) != (size_t)nb * 2) {
            reply_err(c, 0x16);    /* EINVAL */
            break;
        }
        unsigned char tmp[16];
        for (unsigned b = 0; b < nb; b++) {
            int hi = hexval((unsigned char)hex[b * 2]);
            int lo = hexval((unsigned char)hex[b * 2 + 1]);
            if (hi < 0 || lo < 0) {
                reply_err(c, 0x16);
                return;
            }
            tmp[b] = (unsigned char)((hi << 4) | lo);
        }
        if (be->write_reg(be->ctx, (unsigned)n, tmp, nb) != 0)
            reply_err(c, 1);
        else
            reply(c, "OK");
        break;
    }

    case 'm': {
        uint64_t addr, len;
        const char *p = in + 1;
        if (parse_hex2(&p, &addr, &len) < 0 || len > GDB_MEM_XFER_MAX ||
            !be->read_mem) {
            reply_err(c, 0x16);
            break;
        }
        if (len == 0) {
            reply(c, "");
            break;
        }
        static __thread unsigned char rbuf[GDB_MEM_XFER_MAX];
        long got = be->read_mem(be->ctx, addr, rbuf, (size_t)len);
        if (got < 0) {
            reply_err(c, 1);       /* EFAULT-style */
            break;
        }
        for (long i = 0; i < got; i++) {
            out[i * 2] = (char)hexnib(rbuf[i] >> 4);
            out[i * 2 + 1] = (char)hexnib(rbuf[i] & 0xf);
        }
        out[got * 2] = 0;
        reply(c, out);
        break;
    }

    case 'x': {                     /* binary memory read (LLDB fast path;
                                     * same semantics as 'm', raw reply) */
        uint64_t addr, len;
        const char *p = in + 1;
        if (parse_hex2(&p, &addr, &len) < 0 || len > GDB_MEM_XFER_MAX ||
            !be->read_mem) {
            reply_err(c, 0x16);
            break;
        }
        if (len == 0) {
            reply(c, "");
            break;
        }
        static __thread unsigned char xrbuf[GDB_MEM_XFER_MAX];
        long got = be->read_mem(be->ctx, addr, xrbuf, (size_t)len);
        if (got < 0) {
            reply_err(c, 1);
            break;
        }
        putpkt(c, (const char *)xrbuf, (size_t)got);
        break;
    }

    case 'M':
    case 'X': {
        int binary = (in[0] == 'X');
        uint64_t addr, len;
        const char *p = in + 1;
        const char *colon;
        if (parse_hex2(&p, &addr, &len) < 0 || len > GDB_MEM_XFER_MAX ||
            (colon = strchr(p, ':')) == NULL || !be->write_mem) {
            reply_err(c, 0x16);
            break;
        }
        static __thread unsigned char wbuf[GDB_MEM_XFER_MAX];
        size_t wlen = 0;
        const char *data = colon + 1;

        if (binary) {
            /* getpkt already resolved '}' escapes; payload is raw bytes */
            wlen = inlen - (size_t)(data - in);
            if (wlen != len) {
                reply_err(c, 0x16);
                break;
            }
            memcpy(wbuf, data, wlen);
        } else {
            if (strlen(data) != (size_t)len * 2) {
                reply_err(c, 0x16);
                break;
            }
            for (uint64_t i = 0; i < len; i++) {
                int hi = hexval((unsigned char)data[i * 2]);
                int lo = hexval((unsigned char)data[i * 2 + 1]);
                if (hi < 0 || lo < 0) {
                    reply_err(c, 0x16);
                    return;
                }
                wbuf[i] = (unsigned char)((hi << 4) | lo);
            }
            wlen = (size_t)len;
        }

        long put = be->write_mem(be->ctx, addr, wbuf, wlen);
        if (put != (long)wlen)
            reply_err(c, 2);
        else
            reply(c, "OK");
        break;
    }

    case 'z':
    case 'Z': {
        int zset = (in[0] == 'Z');
        int type = in[1] - '0';
        uint64_t addr, len;
        const char *p = in + 3;   /* skip "Z0," / "z0," */
        if (inlen < 6 || in[1] < '0' || in[1] > '4' || in[2] != ',' ||
            parse_hex2(&p, &addr, &len) < 0) {
            reply_err(c, 0x16);
            break;
        }
        if (type != 0) {           /* only software breakpoints in phase 1 */
            reply(c, "");          /* unsupported */
            break;
        }
        int rc = zset
            ? (be->set_bp ? be->set_bp(be->ctx, addr, (unsigned)len) : -1)
            : (be->clear_bp ? be->clear_bp(be->ctx, addr, (unsigned)len) : -1);
        if (rc != 0)
            reply_err(c, 1);
        else
            reply(c, "OK");
        break;
    }

    case 'D':                      /* detach */
        reply(c, "OK");
        *done = 1;
        break;

    case 'k':                      /* kill request: NEVER forwarded to the
                                    * real game; treated as disconnect */
        reply(c, "OK");
        *done = 1;
        break;

    case 'H':                      /* set thread: single synthetic thread */
        reply(c, "OK");
        break;

    case 'q':
        if (!strncmp(in, "qSupported", 10)) {
            snprintf(out, sizeof(out),
                     "PacketSize=%x;"
                     "QStartNoAckMode+;"
                     "qXfer:features:read+;"
                     "qXfer:threads:read+;"
                     "vContSupported+;"
                     "multiprocess-;"
                     "qRelocInsn-",
                     GDB_PACKET_SIZE);
            reply(c, out);
        } else if (!strncmp(in, "qXfer:", 6) ) {
            /* qXfer:<object>:<op>:<annex>:<off>,<len> */
            char obj[32], op[8], annex[64];
            const char *p = in + 6;
            if (sscanf(p, "%31[^:]:%7[^:]:%63[^:]:", obj, op, annex) == 3 &&
                !strcmp(op, "read")) {
                const char *rest = strchr(strchr(strchr(p, ':') + 1, ':') + 1, ':') + 1;
                serve_xfer_read(c, obj, annex, rest);
            } else {
                reply(c, "");
            }
        } else if (!strncmp(in, "qHostInfo", 9)) {
            handle_qHostInfo(c);
        } else if (!strncmp(in, "qProcessInfo", 12)) {
            handle_qProcessInfo(c);
        } else if (!strncmp(in, "qRegisterInfo", 13)) {
            handle_qRegisterInfo(c, in + 13);
        } else if (!strcmp(in, "qC")) {
            snprintf(out, sizeof(out), "QC%llx",
                     (unsigned long long)c->pid ? c->pid : 1);
            reply(c, out);
        } else if (!strcmp(in, "qAttached")) {
            reply(c, "1");         /* existing process: don't kill on quit */
        } else if (!strcmp(in, "qfThreadInfo") ||   /* modern spelling */
                   !strcmp(in, "qfThreads")) {      /* old gdb alias   */
            reply(c, "m1");        /* one synthetic thread while prototyping */
        } else if (!strcmp(in, "qsThreadInfo") ||
                   !strcmp(in, "qsThreads")) {
            reply(c, "l");
        } else if (!strncmp(in, "qRcmd,", 6)) {
            handle_monitor(c, in + 6);
        } else if (!strncmp(in, "qEcho:", 6)) {
            reply(c, in + 6);
        } else {
            reply(c, "");          /* unsupported */
        }
        break;

    case 'Q':
        if (!strcmp(in, "QStartNoAckMode")) {
            reply(c, "OK");        /* reply itself is still checksummed;
                                     * client acks it, then no-ack begins */
            c->noack = 1;
        } else {
            reply(c, "");
        }
        break;

    case 'v':
        if (!strcmp(in, "vCont?")) {
            reply(c, "vCont;c;s");
        } else if (!strncmp(in, "vCont;", 6)) {
            /* phase-1: any continue/step resolves to an immediate stop */
            reply(c, "T05thread:1;");
        } else if (!strncmp(in, "vAttach;", 8)) {
            uint64_t pid;
            const char *p = in + 8;
            if (parse_hex(&p, &pid) < 0) {
                reply_err(c, 0x16);
                break;
            }
            if (be->attach && be->attach(be->ctx, pid) == 0) {
                c->pid = pid;
                reply(c, "T05thread:1;");
            } else {
                reply_err(c, 1);
            }
        } else if (!strncmp(in, "vKill;", 6)) {
            reply(c, "OK");        /* detach semantics, game keeps running */
            *done = 1;
        } else {
            reply(c, "");
        }
        break;

    case 'r':                      /* deprecated reset sequence: ignore */
    default:
        reply(c, "");              /* unsupported */
        break;
    }
}

/* =====================================================================
 * Connection loop
 * ===================================================================== */

static void serve_client(int fd, const struct gdb_backend *be, uint64_t pid)
{
    struct conn c = { fd, 0, be, pid };
    char buf[GDB_PACKET_SIZE];
    fprintf(stderr, "gdbstub: client connected (backend=%s pid=%llu)\n",
            be->name, (unsigned long long)pid);

    for (;;) {
        int intr = 0;
        int n = getpkt(&c, buf, sizeof(buf) - 1, &intr);
        if (intr) {
            reply(&c, "T02thread:1;");   /* SIGINT stop reply */
            continue;
        }
        if (n == -2)
            continue;              /* dropped bad-csum packet (no-ack) */
        if (n < 0)
            break;                 /* EOF or unrecoverable framing */
        buf[n] = 0;
        fprintf(stderr, "gdbstub: >> %s\n", buf);

        int done = 0;
        handle_packet(&c, buf, (size_t)n, &done);
        if (done)
            break;
    }
    fprintf(stderr, "gdbstub: client disconnected\n");
}

int gdbstub_serve(const struct gdb_backend *be, const char *listen_addr,
                  int port)
{
    signal(SIGPIPE, SIG_IGN);
    xml_selfcheck();

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (!listen_addr || !strcmp(listen_addr, "localhost"))
        listen_addr = "127.0.0.1";
    if (inet_pton(AF_INET, listen_addr, &sa.sin_addr) != 1) {
        fprintf(stderr, "gdbstub: bad listen address '%s'\n", listen_addr);
        return -1;
    }
    if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        return -1;
    }
    if (listen(s, 1) < 0) {
        perror("listen");
        return -1;
    }
    fprintf(stderr,
            "gdbstub: %s backend listening on %s:%d "
            "(gdb: target remote %s:%d / lldb: gdb-remote %s:%d)\n",
            be->name, listen_addr, port, listen_addr, port, listen_addr,
            port);

    for (;;) {
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int fd = accept(s, (struct sockaddr *)&ca, &cl);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            return -1;
        }
        serve_client(fd, be, 0 /* default pid: backend-attached */);
        close(fd);
    }
}
