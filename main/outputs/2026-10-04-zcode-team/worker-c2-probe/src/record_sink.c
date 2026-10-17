// record_sink.c — v3 WRITER FIX (worker-ingame-probe-v3).
// Vendored from worker-ingame-probe-v2/src/record_sink.c. Sink functions are
// BYTE-UNCHANGED (same lineage-proven sceKernelOpen/Write/Close trio, same
// bounded retry). ONLY the record-building half changed: IgBuf gained the
// need_sep member-separator state and the ig_kv_*/array/object helpers now
// emit separators correctly, fixing the v1/v2 missing-comma JSONL bug that
// was verified line-by-line against the actual v2 log (see record_sink.h).
#include "record_sink.h"
#include "ingame_abi.h"
#include "ingame_libc.h"

#include <sys/fcntl.h>

#ifndef IG_LOG_PATH_DEFAULT
#define IG_LOG_PATH_DEFAULT "/data/modbridge-ingame-facts.jsonl"
#endif

#define IG_WRITE_ATTEMPTS 64u

void ig_buf_reset(IgBuf *b) {
    b->len = 0; b->truncated = 0; b->need_sep = 0; b->data[0] = '\0';
}

int ig_buf_room(const IgBuf *b, size_t n) {
    /* Reserve space for the closing "}}\n" tail (4 bytes) plus margin. */
    return b->len + n + 16 <= IG_RECORD_MAX;
}

void ig_buf_ch(IgBuf *b, char c) {
    if (!ig_buf_room(b, 1)) { b->truncated = 1; return; }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
}

void ig_buf_raw(IgBuf *b, const char *s) {
    while (s && *s) ig_buf_ch(b, *s++);
}

void ig_buf_str_escaped(IgBuf *b, const char *s, size_t max_bytes) {
    static const char hexd[] = "0123456789abcdef";
    ig_buf_ch(b, '"');
    for (size_t i = 0; s && s[i] && i < max_bytes; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            ig_buf_ch(b, '\\'); ig_buf_ch(b, (char)c);
        } else if (c < 0x20 || c >= 0x7f) {
            ig_buf_raw(b, "\\u00");
            ig_buf_ch(b, hexd[(c >> 4) & 15]);
            ig_buf_ch(b, hexd[c & 15]);
        } else {
            ig_buf_ch(b, (char)c);
        }
        if (b->truncated) break;
    }
    ig_buf_ch(b, '"');
}

void ig_buf_u64_dec(IgBuf *b, uint64_t v) {
    char tmp[24];
    size_t n = 0;
    if (v == 0) { ig_buf_ch(b, '0'); return; }
    while (v > 0 && n < sizeof tmp) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0) ig_buf_ch(b, tmp[--n]);
}

void ig_buf_i64_dec(IgBuf *b, int64_t v) {
    uint64_t u;
    if (v < 0) { ig_buf_ch(b, '-'); u = (uint64_t)(-(v + 1)) + 1u; }
    else u = (uint64_t)v;
    ig_buf_u64_dec(b, u);
}

void ig_buf_u64_hex(IgBuf *b, uint64_t v) {
    static const char hexd[] = "0123456789abcdef";
    ig_buf_raw(b, "0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        ig_buf_ch(b, hexd[(v >> (unsigned)shift) & 15]);
}

/* v3: separator-correct member open. The v1/v2 writers emitted `"key":value`
   with NO leading comma; the only commas in the v1/v2 logs came from
   hard-coded `,"array":[` prefixes and array-element loops — exactly the
   corruption pattern seen in the raw v2 JSONL. */
void ig_kv_key(IgBuf *b, const char *key) {
    if (b->need_sep) ig_buf_ch(b, ',');
    b->need_sep = 0;
    ig_buf_ch(b, '"'); ig_buf_raw(b, key); ig_buf_raw(b, "\":");
}

void ig_kv_int(IgBuf *b, const char *key, int64_t v) {
    ig_kv_key(b, key); ig_buf_i64_dec(b, v); b->need_sep = 1;
}

void ig_kv_hex(IgBuf *b, const char *key, uint64_t v) {
    ig_kv_key(b, key);
    ig_buf_ch(b, '"'); ig_buf_u64_hex(b, v); ig_buf_ch(b, '"');
    b->need_sep = 1;
}

void ig_kv_str(IgBuf *b, const char *key, const char *v, size_t max_bytes) {
    ig_kv_key(b, key); ig_buf_str_escaped(b, v, max_bytes); b->need_sep = 1;
}

void ig_arr_open(IgBuf *b, const char *key) {
    if (b->need_sep) ig_buf_ch(b, ',');
    b->need_sep = 0;
    ig_buf_ch(b, '"'); ig_buf_raw(b, key); ig_buf_raw(b, "\":[");
}

void ig_arr_close(IgBuf *b) { ig_buf_ch(b, ']'); b->need_sep = 1; }

void ig_obj_open(IgBuf *b) { ig_buf_ch(b, '{'); b->need_sep = 0; }

void ig_obj_close(IgBuf *b) { ig_buf_ch(b, '}'); b->need_sep = 1; }

int ig_sink_open(IgSink *s, const char *path) {
    s->failed = 0; s->open_errno = 0;
    s->fd = sceKernelOpen(path, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW,
                          (uint16_t)0600);
    if (s->fd < 0) {
        int *e = __error();
        s->open_errno = e ? *e : 0;
        return -1;
    }
    return 0;
}

int ig_sink_write_record(IgSink *s, const IgBuf *b) {
    if (s->fd < 0 || b->len == 0 || b->data[b->len - 1] != '\n') return -1;
    size_t offset = 0;
    unsigned attempts = 0;
    while (offset < b->len && attempts++ < IG_WRITE_ATTEMPTS) {
        int64_t w = sceKernelWrite(s->fd, b->data + offset, b->len - offset);
        if (w <= 0 || (uint64_t)w > b->len - offset) { s->failed = 1; return -1; }
        offset += (size_t)w;
    }
    if (offset != b->len) { s->failed = 1; return -1; }
    return 0;
}

int ig_sink_close(IgSink *s) {
    if (s->fd < 0) return 0;
    int fd = s->fd;
    s->fd = -1;
    return sceKernelClose(fd);
}

void ig_record_begin(IgBuf *b, int seq, const char *exp, int64_t rc,
                     int64_t eno, const char *build_id) {
    ig_buf_reset(b);
    ig_buf_raw(b, "{\"marker\":\"modbridge.ingamefacts\",\"schema\":1,\"build_id\":\"");
    ig_buf_raw(b, build_id);
    ig_buf_raw(b, "\",\"seq\":");
    ig_buf_i64_dec(b, (int64_t)seq);
    ig_buf_raw(b, ",\"exp\":\"");
    ig_buf_raw(b, exp);
    ig_buf_raw(b, "\",\"rc\":");
    ig_buf_i64_dec(b, rc);
    ig_buf_raw(b, ",\"errno\":");
    ig_buf_i64_dec(b, eno);
    ig_buf_raw(b, ",\"fields\":{");
    b->need_sep = 0;   /* fields object starts empty */
}

void ig_record_flush(IgBuf *b) {
    if (b->truncated) {
        /* v3: the truncation marker + closing tail MUST always fit. v1/v2
           could end up with the buffer so full that neither "__trunc":1
           nor "}}" fit, producing a line cut mid-string with a raw '\n'
           and NO truncation flag (host test case 4 reproduces this). */
        while (b->len + 24 > IG_RECORD_MAX - 1) {
            if (b->len == 0) break;
            --b->len;
        }
        b->data[b->len] = '\0';
        const char *tail = ",\"__trunc\":1}}\n";
        while (*tail) {
            if (b->len + 1 >= IG_RECORD_MAX) break;
            b->data[b->len++] = *tail++;
        }
        b->data[b->len] = '\0';
        return;
    }
    ig_buf_raw(b, "}}\n");
    /* Guarantee the hard bound even if a future editor breaks the reserve. */
    if (b->len >= IG_RECORD_MAX) { b->len = IG_RECORD_MAX - 1; }
    if (b->len == 0) { b->data[0] = '\n'; b->len = 1; }
    b->data[b->len] = '\0';
    if (b->data[b->len - 1] != '\n') {
        b->data[b->len - 1] = '\n';
    }
}
