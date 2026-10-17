// record_sink.h
//
// Bounded JSONL record builder + in-game file sink for the MODBRIDGE IN-GAME
// FACTS PRX. Adapted from worker-liveprobe/src/probe_record.hpp (C++ host/
// payload probe, team 2026-10-04) into freestanding C for the PRX context,
// with the file backend switched to the lineage-proven sceKernelOpen/Write/
// Close trio (see ingame_abi.h provenance).
//
// Envelope (one JSONL line per experiment, <= 2 KiB incl. '\n'):
//   {"marker":"modbridge.ingamefacts","schema":1,"build_id":"<id>","seq":N,
//    "exp":"<id>","rc":R,"errno":E,"fields":{...}}
// Truncation is always marked inside the record with "__trunc":1.
#ifndef INGAME_RECORD_SINK_H
#define INGAME_RECORD_SINK_H

#include <stddef.h>
#include <stdint.h>

#define IG_RECORD_MAX 2048

typedef struct IgBuf {
    char data[IG_RECORD_MAX];
    size_t len;
    int truncated;
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
