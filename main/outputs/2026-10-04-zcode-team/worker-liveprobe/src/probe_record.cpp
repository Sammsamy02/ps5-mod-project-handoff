// probe_record.cpp — implementation of the bounded JSONL record writer.
// No libc formatting functions are used (own decimal/hex formatters), so the
// only imports this TU can introduce are open/write/close from <fcntl.h>/
// <unistd.h> plus whatever the compiler emits for zeroing (none here).
#include "probe_record.hpp"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace livefacts {
namespace {

// Reserve kept free so record_flush() can always close the JSON object.
constexpr size_t kTailReserve = 24;

const char kHexDigits[] = "0123456789abcdef";

}  // namespace

void buf_reset(Buf &b) {
    b.len = 0;
    b.truncated = false;
    b.data[0] = '\0';
}

bool buf_room(const Buf &b, size_t n) {
    return b.len + n + kTailReserve <= kRecordMax && b.len + n <= kRecordMax;
}

void buf_ch(Buf &b, char c) {
    if (!buf_room(b, 1)) {
        b.truncated = true;
        return;
    }
    b.data[b.len++] = c;
    b.data[b.len] = '\0';
}

void buf_raw(Buf &b, const char *s) {
    while (*s != '\0') {
        buf_ch(b, *s++);
    }
}

void buf_str_escaped(Buf &b, const char *s, size_t max_bytes) {
    buf_ch(b, '"');
    for (size_t i = 0; i < max_bytes && s[i] != '\0'; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            buf_ch(b, '\\');
            buf_ch(b, (char)c);
        } else if (c >= 0x20 && c < 0x7f) {
            buf_ch(b, (char)c);
        } else {  // control bytes and >= 0x7f become \u00xx (valid UTF-8 JSON)
            buf_ch(b, '\\');
            buf_ch(b, 'u');
            buf_ch(b, '0');
            buf_ch(b, '0');
            buf_ch(b, kHexDigits[(c >> 4) & 0xf]);
            buf_ch(b, kHexDigits[c & 0xf]);
        }
    }
    buf_ch(b, '"');
}

void buf_u64_dec(Buf &b, uint64_t v) {
    char tmp[20];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);
    if (!buf_room(b, n)) {
        b.truncated = true;
        return;
    }
    while (n != 0) {
        b.data[b.len++] = tmp[--n];
    }
    b.data[b.len] = '\0';
}

void buf_i64_dec(Buf &b, int64_t v) {
    if (v < 0) {
        buf_ch(b, '-');
        buf_u64_dec(b, (uint64_t)(-(v + 1)) + 1);  // avoids INT64_MIN UB
        return;
    }
    buf_u64_dec(b, (uint64_t)v);
}

void buf_u64_hex(Buf &b, uint64_t v) {
    char tmp[16];
    for (size_t i = 0; i < 16; ++i) {
        tmp[15 - i] = kHexDigits[v & 0xf];
        v >>= 4;
    }
    if (!buf_room(b, 16)) {
        b.truncated = true;
        return;
    }
    for (size_t i = 0; i < 16; ++i) {
        b.data[b.len++] = tmp[i];
    }
    b.data[b.len] = '\0';
}

void kv_key(Buf &b, const char *key) {
    // Auto-separator: prepend ',' when the previous token was a value or a
    // closing bracket, so every kv_* member yields valid JSON.
    if (b.len != 0) {
        const char last = b.data[b.len - 1];
        if (last != '{' && last != '[' && last != ',') {
            buf_ch(b, ',');
        }
    }
    buf_ch(b, '"');
    buf_raw(b, key);  // keys are ASCII literals in this probe
    buf_ch(b, '"');
    buf_ch(b, ':');
}

void kv_int(Buf &b, const char *key, int64_t v) {
    kv_key(b, key);
    buf_i64_dec(b, v);
}

void kv_hex(Buf &b, const char *key, uint64_t v) {
    kv_key(b, key);
    buf_ch(b, '"');
    buf_raw(b, "0x");
    buf_u64_hex(b, v);
    buf_ch(b, '"');
}

void kv_str(Buf &b, const char *key, const char *v, size_t max_bytes) {
    kv_key(b, key);
    buf_str_escaped(b, v, max_bytes);
}

int log_open(LogSink &s, const char *path) {
    s.failed = 0;
    s.fd = -1;
    for (unsigned i = 0; i < 8; ++i) {
        s.fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
        if (s.fd >= 0) {
            return 0;
        }
        if (errno != EINTR) {
            break;
        }
    }
    return -1;
}

int log_write_record(LogSink &s, const Buf &b) {
    size_t off = 0;
    while (off < b.len) {
        const ssize_t n = write(s.fd, b.data + off, b.len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

int log_close(LogSink &s) {
    const int fd = s.fd;
    s.fd = -1;
    if (close(fd) != 0 && errno != EINTR) {
        return -1;
    }
    return 0;
}

void record_begin(Buf &b, int seq, const char *exp, int64_t rc, int64_t eno,
                  const char *build_id) {
    buf_reset(b);
    buf_raw(b, "{\"marker\":\"modbridge.livefacts\",\"schema\":1,\"build_id\":");
    buf_str_escaped(b, build_id, 64);
    buf_raw(b, ",\"seq\":");
    buf_i64_dec(b, (int64_t)seq);
    buf_raw(b, ",\"exp\":");
    buf_str_escaped(b, exp, 40);
    buf_raw(b, ",\"rc\":");
    buf_i64_dec(b, rc);
    buf_raw(b, ",\"errno\":");
    buf_i64_dec(b, eno);
    buf_raw(b, ",\"fields\":{");
}

void record_flush(Buf &b) {
    // Whole-token-or-nothing: only add the truncation marker when the full
    // 12-char member fits and a separator is actually needed.
    if (b.truncated && buf_room(b, 12) && b.len != 0) {
        const char last = b.data[b.len - 1];
        if (last != '{' && last != '[' && last != ',') {
            buf_raw(b, ",\"__trunc\":1");
        }
    }
    // Closing tokens always fit: every append kept kTailReserve bytes free.
    b.data[b.len++] = '}';
    b.data[b.len++] = '}';
    b.data[b.len++] = '\n';
    b.data[b.len] = '\0';
    if (b.len > kRecordMax) {  // unreachable; defensive
        b.len = kRecordMax;
        b.data[kRecordMax - 1] = '\n';
    }
}

}  // namespace livefacts
