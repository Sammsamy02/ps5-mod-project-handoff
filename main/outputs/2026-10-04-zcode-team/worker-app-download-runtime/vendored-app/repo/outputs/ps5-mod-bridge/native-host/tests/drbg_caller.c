/* Deterministic, nonsecret, real-library integration fixture. No fixture bytes
   or byte hashes are printed on success or failure. */
#define _POSIX_C_SOURCE 200809L
#include "drbg_internal.h"
#include "http.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysctl.h>

static unsigned os_calls, clock_calls, add_calls, seed_calls, random_calls;
static unsigned entropy_calls, source_entries, prohibited_calls, observer_calls;
static int contracts_ok = 1, observer_match, fault, fault_active;
static unsigned failure_at;
static int reply_rc, reply_errno;
static size_t reply_length = 32, reply_write = 32;
static mbedtls_entropy_f_source_ptr registered_source;
static void *registered_state;
static const unsigned char expected_owned[32] = { 0xcd,0x33,0x6a,0x27,0x34,0xc1,0x5b,0x93,0x40,0x08,0x0e,0xf5,0x64,0xcb,0xa1,0xde,0xef,0x1c,0x5b,0xc6,0xfd,0x2a,0x93,0xa7,0x20,0xb6,0xe9,0x50,0x32,0xda,0x52,0x9b };

static int zeros(const void *storage, size_t size) {
    const unsigned char *bytes = storage;
    for (size_t i = 0; i < size; ++i) if (bytes[i]) return 0;
    return 1;
}
static int canary(const void *storage, size_t size) {
    const unsigned char *bytes = storage;
    for (size_t i = 0; i < size; ++i) if (bytes[i] != 0xa5) return 0;
    return 1;
}
/* Narrow actual library allocation boundary; no mocked accumulator or cipher. */
void *calloc(size_t count, size_t size) {
    if (fault == 2 && fault_active) return NULL;
    if (size && count > SIZE_MAX / size) return NULL;
    size_t total = count * size;
    void *memory = malloc(total ? total : 1);
    if (memory) memset(memory, 0, total);
    return memory;
}
int sysctl(const int *name, unsigned int name_length, void *old_value,
           size_t *old_length, const void *new_value, size_t new_length) {
    ++os_calls;
    contracts_ok &= name && name_length == 2 && name[0] == 1 && name[1] == 37 &&
                    old_value && old_length && *old_length == 32 && !new_value &&
                    new_length == 0 && errno == 0;
    int fail = os_calls == failure_at;
    size_t written = fail ? reply_write : 32;
    unsigned char *out = old_value;
    for (size_t i = 0; out && i < written && i < 32; ++i)
        out[i] = (unsigned char)((os_calls - 1) * 0x40 + i);
    if (old_length) *old_length = fail ? reply_length : 32;
    errno = fail ? reply_errno : 0;
    return fail ? reply_rc : 0;
}
int clock_gettime(clockid_t id, struct timespec *time) {
    ++clock_calls;
    contracts_ok &= id == CLOCK_MONOTONIC;
    time->tv_sec = fault==5 && clock_calls==1 ? 10 : 7;
    time->tv_nsec = clock_calls * 1000000L;
    errno = 93;
    return fault==4 ? -1 : 0;
}
int mbedtls_platform_entropy_poll(void *state, unsigned char *out, size_t size, size_t *length) {
    (void)state; (void)out; (void)size; (void)length;
    ++prohibited_calls;
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}
static int fixture_poll(void *state, unsigned char *out, size_t size, size_t *length) {
    (void)state;
    ++source_entries;
    contracts_ok &= size == 128;
    return registered_source(registered_state, out, size, length);
}
int fixture_add_source(mbedtls_entropy_context *ctx, mbedtls_entropy_f_source_ptr source,
                       void *state, size_t threshold, int strong) {
    ++add_calls;
    contracts_ok &= threshold == 32 && strong == MBEDTLS_ENTROPY_SOURCE_STRONG;
    if (fault == 1) return MBEDTLS_ERR_ENTROPY_MAX_SOURCES;
    registered_source = source;
    registered_state = state;
    return mbedtls_entropy_add_source(ctx, fixture_poll, NULL, threshold, strong);
}
int fixture_entropy_func(void *ctx, unsigned char *out, size_t length) {
    ++entropy_calls;
    contracts_ok &= length == 48;
    fault_active = 1;
    int rc = mbedtls_entropy_func(ctx, out, length);
    fault_active = 0;
    return rc;
}
int fixture_seed(mbedtls_ctr_drbg_context *ctx, int (*entropy)(void *, unsigned char *, size_t),
                 void *state, const unsigned char *custom, size_t length) {
    static const unsigned char personal[] = "modbridge-owned-drbg-checkpoint-v1";
    ++seed_calls;
    contracts_ok &= length == sizeof(personal) - 1 &&
                    memcmp(custom, personal, sizeof(personal) - 1) == 0;
    return mbedtls_ctr_drbg_seed(ctx, entropy, state, custom, length);
}
int fixture_random(void *ctx, unsigned char *out, size_t length) {
    ++random_calls;
    contracts_ok &= length == 32;
    int rc = mbedtls_ctr_drbg_random(ctx, out, length);
    if (!rc && fault == 3) { memset(out, 0x57, 13); return MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG; }
    return rc;
}
void mb_drbg_test_observe(const unsigned char *out, size_t length) {
    ++observer_calls;
    observer_match = length == 32 && memcmp(out, expected_owned, sizeof(expected_owned)) == 0;
}

static void setup(struct mb_drbg_workspace *workspace) {
    memset(workspace, 0, sizeof(*workspace));
    workspace->source.budget = &workspace->budget;
    workspace->source.staging = workspace->staging;
}
static int report_checkpoint(void) {
    struct { unsigned char before[19]; struct mb_drbg_workspace workspace; unsigned char after[19]; } owned;
    struct mb_entropy_result raw;
    struct mb_drbg_result drbg;
    struct host_info info = {.version="drbg-fixture",.build_id="fixture",.profile="fixture",.origin="test-only"};
    struct http_response first, second;
    const unsigned char request[] = "GET /report HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n\r\n";
    memset(&owned, 0xa5, sizeof(owned));
    mb_random_checkpoint_in(&raw, &drbg, &owned.workspace);
    info.entropy = raw; info.drbg = drbg;
    strcpy(info.authority,"127.0.0.1:18080"); strcpy(info.url_origin,"http://127.0.0.1:18080"); strcpy(info.core_report,"{}");
    http_handle(request,sizeof(request)-1,&info,&first);
    http_handle(request,sizeof(request)-1,&info,&second);
    printf("{\"os_calls\":%u,\"clock_calls\":%u,\"add_calls\":%u,\"seed_calls\":%u,\"random_calls\":%u,"
           "\"entropy_calls\":%u,\"source_entries\":%u,\"prohibited_calls\":%u,\"contracts_ok\":%d,"
           "\"observer_calls\":%u,\"observer_match\":%d,\"workspace_cleared\":%d,\"canaries_ok\":%d,"
           "\"cached\":%d,\"body_size\":%zu,\"http_status\":%d,\"report\":%s}\n",
           os_calls,clock_calls,add_calls,seed_calls,random_calls,entropy_calls,source_entries,prohibited_calls,contracts_ok,
           observer_calls,observer_match,zeros(&owned.workspace,sizeof(owned.workspace)),
           canary(owned.before,sizeof(owned.before))&&canary(owned.after,sizeof(owned.after)),
           first.size==second.size&&!memcmp(first.body,second.body,first.size),first.size,first.status,first.body);
    return 0;
}

static int direct(const char *mode, size_t capacity, int invalid, unsigned used) {
    struct mb_drbg_workspace workspace;
    struct { unsigned char before[19], output[128], after[19]; } owned;
    size_t length = 999;
    unsigned char extracted[48];
    int rc=0, second_rc=0, last_rc=0, last_errno=0, has_last=0, failed=0;
    size_t last_length=0;
    unsigned callbacks=0,calls=0,total=0;
    const char *policy="none";
    int output_cleared = 0, staging_cleared = 0, tail_untouched = 1;
    setup(&workspace);
    workspace.budget.api_count=used;
    memset(&owned,0xa5,sizeof(owned));
    memset(extracted,0xa5,sizeof(extracted));
    if (!strcmp(mode,"callback")) {
        memset(workspace.staging,0xa5,sizeof(workspace.staging));
        if (invalid==4) workspace.source.budget=NULL;
        if (invalid==5) workspace.source.staging=NULL;
        rc=mb_drbg_source(invalid==1?NULL:&workspace.source,invalid==2?NULL:owned.output,
                          capacity,invalid==3?NULL:&length);
        callbacks=workspace.source.callback_count; calls=workspace.source.os_call_count; total=workspace.budget.api_count;
        has_last=workspace.source.has_last; last_rc=workspace.source.last.return_code;
        last_errno=workspace.source.last.error_number; last_length=workspace.source.last.returned_length;
        failed=workspace.source.failed; policy=workspace.source.policy_failure?workspace.source.policy_failure:"none";
        output_cleared=zeros(owned.output,capacity>=32&&capacity<=128?capacity:128);
        staging_cleared=zeros(workspace.staging,sizeof(workspace.staging));
        if (capacity>=32&&capacity<=128)
            tail_untouched=canary(owned.output+capacity,128-capacity);
        /* An already failed callback must make no later OS call. */
        size_t original_length=length;
        if (rc && invalid != 1) second_rc=mb_drbg_source(&workspace.source,owned.output,128,&length);
        length=original_length;
    } else {
        mbedtls_entropy_init(&workspace.entropy); workspace.entropy_initialized=1;
        mbedtls_ctr_drbg_init(&workspace.ctr); workspace.ctr_initialized=1;
        if (!strcmp(mode,"no_sources")) {
            rc=mbedtls_entropy_func(&workspace.entropy,extracted,sizeof(extracted));
        } else {
            rc=fixture_add_source(&workspace.entropy,mb_drbg_source,&workspace.source,32,MBEDTLS_ENTROPY_SOURCE_STRONG);
            if (!rc && !strcmp(mode,"extract")) rc=mbedtls_entropy_func(&workspace.entropy,extracted,sizeof(extracted));
            if (!rc && !strcmp(mode,"reseed")) {
                struct mb_entropy_result raw;
                rc=mb_entropy_read32(&raw,workspace.staging,&workspace.budget);
                memset(workspace.staging,0,sizeof(workspace.staging));
                const unsigned char personal[]="modbridge-owned-drbg-checkpoint-v1";
                if (!rc) rc=fixture_seed(&workspace.ctr,fixture_entropy_func,&workspace.entropy,personal,sizeof(personal)-1);
                if (!rc) second_rc=mbedtls_ctr_drbg_reseed(&workspace.ctr,NULL,0);
                has_last=workspace.source.has_last; last_rc=workspace.source.last.return_code;
                last_errno=workspace.source.last.error_number; last_length=workspace.source.last.returned_length;
                failed=workspace.source.failed; policy=workspace.source.policy_failure?workspace.source.policy_failure:"none";
                length=88;
                int denied=mb_drbg_source(&workspace.source,owned.output,128,&length);
                if (denied!=MBEDTLS_ERR_ENTROPY_SOURCE_FAILED) contracts_ok=0;
            }
            callbacks=workspace.source.callback_count; calls=workspace.source.os_call_count; total=workspace.budget.api_count;
        }
    }
    if (strcmp(mode,"callback")) {
        output_cleared=zeros(owned.output,capacity>=32&&capacity<=128?capacity:128);
        staging_cleared=zeros(workspace.staging,sizeof(workspace.staging));
    }
    struct host_info info={.version="drbg-fixture",.build_id="fixture",.profile="fixture",.origin="test-only",
        .entropy={.status="unavailable",.reason="desktop_fixture",.elapsed_ms=-1},
        .drbg={.status="failed",.reason="fixture_source_failure",.elapsed_ms=-1,.api_call_limit=4}};
    struct http_response report;
    const unsigned char request[]="GET /report HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n\r\n";
    info.drbg.source_failed=workspace.source.failed;
    info.drbg.has_source_result=workspace.source.has_last;
    info.drbg.source_result=workspace.source.last;
    info.drbg.policy_failure=workspace.source.policy_failure;
    info.drbg.source_callback_count=workspace.source.callback_count;
    info.drbg.source_os_call_count=workspace.source.os_call_count;
    info.drbg.total_api_call_count=workspace.budget.api_count;
    strcpy(info.authority,"127.0.0.1:18080"); strcpy(info.url_origin,"http://127.0.0.1:18080"); strcpy(info.core_report,"{}");
    mb_drbg_cleanup(&workspace);
    http_handle(request,sizeof(request)-1,&info,&report);
    volatile unsigned char *erase=extracted;
    for(size_t i=0;i<sizeof(extracted);++i) erase[i]=0;
    printf("{\"rc\":%d,\"second_rc\":%d,\"length\":%zu,\"os_calls\":%u,\"source_entries\":%u,"
           "\"callbacks\":%u,\"source_os_calls\":%u,\"total\":%u,\"contracts_ok\":%d,\"prohibited_calls\":%u,"
           "\"has_last\":%d,\"last_rc\":%d,\"last_errno\":%d,\"last_length\":%zu,\"failed\":%d,\"policy\":\"%s\","
           "\"output_cleared\":%d,\"staging_cleared\":%d,\"tail_untouched\":%d,\"workspace_cleared\":%d,\"canaries_ok\":%d,\"report\":%s}\n",
           rc,second_rc,length,os_calls,source_entries,callbacks,calls,total,contracts_ok,prohibited_calls,
           has_last,last_rc,last_errno,last_length,failed,policy,output_cleared,staging_cleared,tail_untouched,
           zeros(&workspace,sizeof(workspace)),canary(owned.before,sizeof(owned.before))&&canary(owned.after,sizeof(owned.after)),report.body);
    return 0;
}
int main(int argc,char **argv) {
    if(argc<2) return 2;
    /* Explicit reset before any real initializer/no-source probe. */
    os_calls=clock_calls=add_calls=seed_calls=random_calls=entropy_calls=source_entries=prohibited_calls=observer_calls=0;
    if(!strcmp(argv[1],"checkpoint")) {
        if(argc!=8) return 2;
        failure_at=(unsigned)strtoul(argv[2],NULL,10);
        reply_rc=(int)strtol(argv[3],NULL,10); reply_errno=(int)strtol(argv[4],NULL,10);
        reply_length=!strcmp(argv[5],"max")?SIZE_MAX:(size_t)strtoull(argv[5],NULL,10);
        reply_write=(size_t)strtoull(argv[6],NULL,10); fault=(int)strtol(argv[7],NULL,10);
        return report_checkpoint();
    }
    if(argc!=5 && argc!=9) return 2;
    if(argc==9) {
        failure_at=1; reply_rc=(int)strtol(argv[5],NULL,10); reply_errno=(int)strtol(argv[6],NULL,10);
        reply_length=!strcmp(argv[7],"max")?SIZE_MAX:(size_t)strtoull(argv[7],NULL,10);
        reply_write=(size_t)strtoull(argv[8],NULL,10);
    }
    size_t capacity=!strcmp(argv[2],"max")?SIZE_MAX:(size_t)strtoull(argv[2],NULL,10);
    return direct(argv[1],capacity,(int)strtol(argv[3],NULL,10),(unsigned)strtoul(argv[4],NULL,10));
}
