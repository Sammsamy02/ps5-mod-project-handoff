// record_sink.h — VERBATIM VENDOR from worker-ingame-probe-v3/src/
// record_sink.h (the v3 WRITER FIX: separator-correct JSONL, sink functions
// byte-unchanged from the console-proven lineage). Envelope marker text in
// ig_record_begin (record_sink.c) is the ONLY pad-probe change:
// "modbridge.ingamefacts" -> "modbridge.padprobe". Everything else —
// IgBuf/IgSink shapes, IG_RECORD_MAX, helper contracts, truncation
// guarantee — is unchanged.
#ifndef INGAME_RECORD_SINK_H
#define INGAME_RECORD_SINK_H

#include <stddef.h>
#include <stdint.h>

#define IG_RECORD_MAX 2048

typedef struct IgBuf {
    char data[IG_RECORD_MAX];
    size_t len;
    int truncated;
    int need_sep;   // 1 iff the current JSON object already has a member
} IgBuf;

void ig_buf_reset(IgBuf *b);
int ig_buf_room(const IgBuf *b, size_t n);   /* reserve for closing tokens */
void ig_buf_ch(IgBuf *b, char c);
void ig_buf_raw(IgBuf *b, const char *s);
void ig_buf_str_escaped(IgBuf *b, const char *s, size_t max_bytes);
void ig_buf_u64_dec(IgBuf *b, uint64_t v);
void ig_buf_i64_dec(IgBuf *b, int64_t v);
void ig_buf_u64_hex(IgBuf *b, uint64_t v);   /* 0x + 16 lowercase digits */

void ig_kv_key(IgBuf *b, const char *key);
void ig_kv_int(IgBuf *b, const char *key, int64_t v);
void ig_kv_hex(IgBuf *b, const char *key, uint64_t v);
void ig_kv_str(IgBuf *b, const char *key, const char *v, size_t max_bytes);

void ig_arr_open(IgBuf *b, const char *key);  /* ,"key":[ */
void ig_arr_close(IgBuf *b);                  /* ] */
void ig_obj_open(IgBuf *b);                   /* { */
void ig_obj_close(IgBuf *b);                  /* } */

typedef struct IgSink {
    int fd;
    int failed;
    int open_errno;
} IgSink;

/* 0 ok; -1 open failed (open_errno captured). Path from generated config. */
int ig_sink_open(IgSink *s, const char *path);
/* 0 ok; -1 write error (bounded retry over partial/zero writes). */
int ig_sink_write_record(IgSink *s, const IgBuf *b);
/* 0 ok; -1 close error. */
int ig_sink_close(IgSink *s);

void ig_record_begin(IgBuf *b, int seq, const char *exp, int64_t rc,
                     int64_t eno, const char *build_id);
void ig_record_flush(IgBuf *b);

#endif  // INGAME_RECORD_SINK_H
