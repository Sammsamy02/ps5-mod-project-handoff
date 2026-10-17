/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "report.h"
#include "payload_args.h"
extern int probe_entry(payload_args_t *args);

static char captured[8192];
static unsigned long captured_length;
static unsigned long chunk = 4096;
static int write_mode;
static int writes;
static int missing_name;
static int resolver_calls;
static int fallback;
static int bad_page;
static int missing_writer;
static int fake_pid(void) { return 321; }
static unsigned int fake_uid(void) { return UINT_MAX; }
static unsigned int fake_euid(void) { return 0; }
static int fake_page(void) { return bad_page ? -1 : 16384; }

static long fake_write(int fd, const void *bytes, unsigned long length) {
    assert(fd == 1);
    ++writes;
    if (write_mode == 1) return 0;
    if (write_mode == 2) return -1;
    if (write_mode == 3) return (long)length + 1;
    if (length > chunk) length = chunk;
    assert(captured_length + length <= sizeof(captured));
    memcpy(captured + captured_length, bytes, length);
    captured_length += length;
    return (long)length;
}

static int fake_resolve(int handle, const char *name, void *out) {
    void *address = NULL;
    ++resolver_calls;
    assert(handle == 1 || handle == 0x2001);
    if (fallback && handle == 1) return 19;
    if (!strcmp(name, "getpid")) address = (void *)fake_pid;
    else if (!strcmp(name, "getuid")) address = (void *)fake_uid;
    else if (!strcmp(name, "geteuid")) address = (void *)fake_euid;
    else if (!strcmp(name, "getpagesize")) address = (void *)fake_page;
    else if (!strcmp(name, "write")) address = (void *)fake_write;
    else assert(!"resolver requested a function outside the read-only whitelist");
    if (missing_name && !strcmp(name, "getuid")) return 77;
    if (missing_writer && !strcmp(name, "write")) return 78;
    *(void **)out = address;
    return 0;
}

int main(void) {
    assert(sizeof(payload_args_t) == 48);
    assert(__builtin_offsetof(payload_args_t, sys_dynlib_dlsym) == 0);
    assert(__builtin_offsetof(payload_args_t, rwpipe) == 8);
    assert(__builtin_offsetof(payload_args_t, rwpair) == 16);
    assert(__builtin_offsetof(payload_args_t, kpipe_addr) == 24);
    assert(__builtin_offsetof(payload_args_t, kdata_base_addr) == 32);
    assert(__builtin_offsetof(payload_args_t, payloadout) == 40);
    struct probe_facts facts;
    probe_writer writer = NULL;
    char report[4096];
    char tiny[8] = "canary";
    unsigned long length;

    assert(probe_collect(NULL, &facts, &writer) != 0);
    assert(writer == NULL);
    for (int i = 0; i < 4; ++i) assert(facts.observation[i].state == PROBE_UNMEASURED);
    assert(resolver_calls == 0);

    assert(probe_collect(fake_resolve, &facts, &writer) == 0);
    assert(facts.observation[0].state == PROBE_OBSERVED);
    assert(facts.observation[0].value == 321);
    assert(facts.observation[1].value == 4294967295L);
    assert(facts.observation[2].value == 0);
    assert(facts.observation[3].value == 16384);
    assert(writer != NULL);

    missing_name = 1;
    fallback = 1;
    bad_page = 1;
    assert(probe_collect(fake_resolve, &facts, &writer) == 0);
    assert(facts.observation[0].value == 321);
    assert(facts.observation[1].state == PROBE_UNAVAILABLE);
    assert(facts.observation[1].resolve_code == 77);
    assert(facts.observation[2].state == PROBE_OBSERVED);
    assert(facts.observation[3].state == PROBE_UNAVAILABLE);
    assert(facts.observation[3].value == -1);

    length = probe_format(&facts, "desktop_fixture", report, sizeof(report));
    assert(length > 0 && length < sizeof(report));
    assert(report[length] == '\0');
    assert(probe_format(&facts, "desktop_fixture", tiny, 4) == 0);
    assert(tiny[4] == 'r' && tiny[5] == 'y');
    assert(probe_format(&facts, "desktop_fixture", NULL, 4096) == 0);
    assert(probe_format(&facts, "desktop_fixture", report, 0) == 0);

    /* Signed minimum must serialize without negation overflow. */
    facts.observation[0].value = LONG_MIN;
    length = probe_format(&facts, "desktop_fixture", report, sizeof(report));
    assert(length > 0);
    chunk = 512;
    assert(probe_emit(fake_write, report, length) == 0);
    assert(captured_length == length && memcmp(captured, report, length) == 0);
    assert(writes <= 8);

    captured_length = 0;
    writes = 0;
    chunk = 1;
    assert(probe_emit(fake_write, report, length) != 0);
    assert(writes == 8 && captured_length == 8);
    for (write_mode = 1; write_mode <= 3; ++write_mode) {
        writes = 0;
        assert(probe_emit(fake_write, report, length) != 0);
        assert(writes == 1);
    }
    writes = 0;
    assert(probe_emit(NULL, report, length) != 0);
    assert(probe_emit(fake_write, report, 4097) != 0);
    assert(writes == 0);

    /* The callable C entry changes only its report/result slot. */
    int result_slot = 99;
    payload_args_t args = {fake_resolve, (int *)1, (int *)2, 3, 4, &result_slot};
    writes = 0;
    assert(probe_entry(NULL) == 2);
    args.sys_dynlib_dlsym = NULL;
    assert(probe_entry(&args) == 2 && result_slot == 2 && writes == 0);
    args.sys_dynlib_dlsym = fake_resolve;
    missing_writer = 1;
    assert(probe_entry(&args) == 3 && result_slot == 3 && writes == 0);
    missing_writer = 0;
    write_mode = 2;
    assert(probe_entry(&args) == 3 && result_slot == 3 && writes == 1);
    write_mode = 0;
    chunk = 4096;
    captured_length = 0;
    writes = 0;
    assert(probe_entry(&args) == 0 && result_slot == 0 && writes == 1);
    assert(captured_length > 0 && captured_length <= 4096);
    assert(strstr(captured, "\"evidence_origin\":\"desktop_fixture\""));
    assert(args.rwpipe == (int *)1 && args.rwpair == (int *)2);
    assert(args.kpipe_addr == 3 && args.kdata_base_addr == 4);
    args.payloadout = NULL;
    captured_length = 0;
    assert(probe_entry(&args) == 0);

    /* Independent JSON assertions live in test_report.py. */
    fwrite(report, 1, length, stdout);
    return 0;
}
