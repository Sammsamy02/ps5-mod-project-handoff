// record_sink.h — v3 WRITER FIX (worker-ingame-probe-v3, team 2026-10-04).
// Vendored from worker-ingame-probe-v2/src/record_sink.h (lineage:
// worker-liveprobe probe_record.hpp). Envelope and sink contract UNCHANGED;
// the v1/v2 JSONL bug (verified against the actual v2 log text,
// outputs/2026-10-04-zcode-controller/console/ingame-v2-facts-2026-10-04.jsonl:
// `"raw":0"raw_hex":"0x..."`, `{"start":"0x...""end":"0x..."`) is fixed by
// member-separator state in IgBuf: every ig_kv_* and array/object helper now
// emits a leading comma exactly when a previous member exists, so every
// NON-truncated emitted line is strictly valid JSON (host-verified by
// tests/writer_host.c + tests/run_host_checks.py via json.loads on every
// line, including 36-bit hex values, i64 negatives and empty arrays).
// Truncated lines still carry "__trunc":1 and remain exempt from the parse
// guarantee (structure may be cut mid-value), as in v1/v2.
#ifndef INGAME_RECORD_SINK_H
#define INGAME_RECORD_SINK_H

#include <stddef.h>
#include <stdint.h>

#define IG_RECORD_MAX 2048

typedef struct IgBuf {
    char data[IG_RECORD_MAX];
    size_t len;
    int truncated;
    int need_sep;   // v3: 1 iff the current JSON object already has a member
                    // (the missing-comma root cause of the v1/v2 log)
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

/* v3 array/object helpers: separator-correct at every nesting level. */
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
