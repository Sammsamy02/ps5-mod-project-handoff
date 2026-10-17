// probe_record.hpp
//
// Bounded JSONL record builder + file sink for LIVEFACTS_PROBE, in the spirit
// of outputs/ps5-mod-bridge/windows-candidate/result_sink.c: one bounded
// write per record, errors captured, never a partial/garbled line.
//
// Hard bound: every emitted record is <= livefacts::kRecordMax bytes
// including the trailing '\n' (probe brief: each JSONL record <= 2KB).
// Truncation is always marked explicitly inside the record.
#ifndef LIVEFACTS_PROBE_RECORD_HPP
#define LIVEFACTS_PROBE_RECORD_HPP

#include <stddef.h>
#include <stdint.h>

namespace livefacts {

constexpr size_t kRecordMax = 2048;

struct Buf {
    char data[kRecordMax];
    size_t len;
    bool truncated;
};

void buf_reset(Buf &b);
// True when n more bytes can be appended while keeping the closing-token
// reserve (the record tail always fits).
bool buf_room(const Buf &b, size_t n);
void buf_ch(Buf &b, char c);
void buf_raw(Buf &b, const char *s);
// Appends a JSON string literal (quotes included) for at most max_bytes
// bytes of s; stops early at NUL. Escapes ", \, control bytes as \u00xx and
// every byte >= 0x7f as \u00xx so the line is always valid UTF-8 JSON.
void buf_str_escaped(Buf &b, const char *s, size_t max_bytes);
void buf_u64_dec(Buf &b, uint64_t v);
void buf_i64_dec(Buf &b, int64_t v);
void buf_u64_hex(Buf &b, uint64_t v);  // 0x + 16 lowercase hex digits

// Field helpers; each appends a complete JSON member without a leading comma
// (the caller inserts commas), keeping the buffer a valid JSON prefix even
// when a later helper declines for space (then b.truncated is set).
void kv_key(Buf &b, const char *key);
void kv_int(Buf &b, const char *key, int64_t v);
void kv_hex(Buf &b, const char *key, uint64_t v);  // "0x..." string value
void kv_str(Buf &b, const char *key, const char *v, size_t max_bytes);

struct LogSink {
    int fd;
    int failed;
};

// 0 ok; -1 open failed (caller records errno in its exit path).
int log_open(LogSink &s, const char *path);
// 0 ok; -1 write error (after bounded retry over EINTR/partial writes).
int log_write_record(LogSink &s, const Buf &b);
// 0 ok; -1 close error.
int log_close(LogSink &s);

// Writes: {"marker":"modbridge.livefacts","schema":1,"build_id":"<id>",
//          "seq":N,"exp":"<id>","rc":R,"errno":E,"fields":{
// The caller then appends field members and calls record_flush().
void record_begin(Buf &b, int seq, const char *exp, int64_t rc, int64_t eno,
                  const char *build_id);
// Closes fields{} and the record object and appends '\n'. Always succeeds
// within the reserve; marks truncation with ,"__trunc":1 when space allows.
void record_flush(Buf &b);

}  // namespace livefacts

#endif  // LIVEFACTS_PROBE_RECORD_HPP
