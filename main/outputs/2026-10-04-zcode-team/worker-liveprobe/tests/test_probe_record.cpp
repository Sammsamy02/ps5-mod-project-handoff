// test_probe_record.cpp — host test for the bounded JSONL record writer.
// Compiled and run on the host with the PINNED LLVM clang (not the target
// toolchain); the target TU livefacts_probe.cpp is NOT executed here.
// Exit 0 = all assertions passed; the JSONL lines are also validated as JSON
// by build_probe.py via python json.loads.
#include "../src/probe_record.hpp"

#include <stdio.h>
#include <string.h>

using namespace livefacts;

static int failures = 0;

static void check(int ok, const char *what) {
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: test_probe_record <out.jsonl>\n");
        return 2;
    }

    // 1. Envelope + escaping + valid close.
    {
        Buf b;
        record_begin(b, 3, "e1_dtdebug_map", -2, 96, "buildid-0123");
        kv_int(b, "a", -9223372036854775807LL - 1);  // INT64_MIN
        kv_int(b, "b", 18446744073709551615ULL);
        kv_hex(b, "h", 0xdeadbeefcafebabeULL);
        kv_str(b, "esc", "q\"u\\o\te\nl", 16);
        kv_str(b, "hi", "\xC3\xA5\xFF", 8);  // non-ASCII -> \u00xx escapes
        record_flush(b);
        check(b.len <= kRecordMax, "record bound");
        check(b.data[b.len - 1] == '\n', "record newline");
        check(strstr(b.data, "\"marker\":\"modbridge.livefacts\"") != nullptr,
              "marker");
        check(strstr(b.data, "\"schema\":1") != nullptr, "schema");
        check(strstr(b.data, "\"build_id\":\"buildid-0123\"") != nullptr,
              "build_id");
        check(strstr(b.data, "\"seq\":3") != nullptr, "seq");
        check(strstr(b.data, "\"rc\":-2") != nullptr, "rc");
        check(strstr(b.data, "\"errno\":96") != nullptr, "errno");
        // Escaper output for q " u \ TAB e NL l: controls become \u00xx.
        check(strstr(b.data, "q\\\"u\\\\o\\u0009e\\u000al") != nullptr,
              "json escapes");
        check(strstr(b.data, "\\u00c3") != nullptr, "non-ascii escape");
        check(strstr(b.data, "\"h\":\"0xdeadbeefcafebabe\"") != nullptr, "hex");
        check(b.data[0] == '{', "record starts with {");
    }

    // 2. Budget enforcement: overlong content truncates, stays <= 2KB and
    // still ends with a newline and closes the JSON object.
    {
        Buf b;
        record_begin(b, 1, "e4_module_list_unit", 0, 0, "buildid-0123");
        char key[32];
        for (int i = 0; i < 400; ++i) {
            // 400 members * ~22 chars far exceeds the 2KB budget.
            strcpy(key, "kkkkkkkkkkkkkkkkkkk");
            key[3] = (char)('0' + (i / 10) % 10);
            key[4] = (char)('0' + i % 10);
            kv_int(b, key, 123456789);
        }
        record_flush(b);
        check(b.truncated, "truncation detected");
        check(b.len <= kRecordMax, "truncated record bound");
        check(b.data[b.len - 1] == '\n', "truncated record newline");
        check(b.data[b.len - 3] == '}' && b.data[b.len - 2] == '}',
              "truncated record closes object");
    }

    // 3. Auto-separator: consecutive members are comma separated, and none
    // appear right after '{'.
    {
        Buf b;
        record_begin(b, 7, "summary", 0, 0, "buildid-0123");
        kv_int(b, "x", 1);
        kv_int(b, "y", 2);
        record_flush(b);
        check(strstr(b.data, "\"fields\":{\"x\":1,\"y\":2}") != nullptr,
              "auto comma");
        check(strstr(b.data, ",,") == nullptr, "no double comma");
    }

    // 4. Sink: bounded write path produces the exact lines on the host.
    {
        LogSink s;
        if (log_open(s, argv[1]) != 0) {
            printf("FAIL: log_open %s\n", argv[1]);
            return 1;
        }
        Buf b;
        record_begin(b, 1, "e9_host_demo", 0, 0, "buildid-0123");
        kv_str(b, "path", "/tmp/ho\"st", 64);
        record_flush(b);
        check(log_write_record(s, b) == 0, "host write");
        check(log_close(s) == 0, "host close");
    }

    printf(failures == 0 ? "PROBE_RECORD_HOST_TESTS_PASSED\n"
                        : "PROBE_RECORD_HOST_TESTS_FAILED\n");
    return failures == 0 ? 0 : 1;
}
