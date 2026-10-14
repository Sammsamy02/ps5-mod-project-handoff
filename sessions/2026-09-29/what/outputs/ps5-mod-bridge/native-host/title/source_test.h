#ifndef MODBRIDGE_TITLE_SOURCE_TEST_H
#define MODBRIDGE_TITLE_SOURCE_TEST_H
#ifndef MB_TITLE_SOURCE_HOST_TESTING
#error "Fixture root/provider seam is host-test-only"
#endif
#ifdef __FreeBSD__
#error "Fixture root/provider seam must never compile for the native SDK target"
#endif
#include "source.h"
#include <sys/stat.h>
#include <sys/types.h>

typedef struct mb_title_source_test_ops_v1 {
    uint32_t struct_size, abi_version;
    void *context;
    int (*lstat_path)(void *,const char *,struct stat *);
    int (*open_path)(void *,const char *,int);
    int (*stat_relative)(void *,int,const char *,struct stat *,int);
    int (*open_relative)(void *,int,const char *,int);
    int (*stat_fd)(void *,int,struct stat *);
    ssize_t (*read_fd)(void *,int,void *,size_t);
    int (*close_fd)(void *,int);
} mb_title_source_test_ops_v1;
#ifdef __cplusplus
extern "C" {
#endif
/* Borrowed trusted test backing only, absolute <=512-byte root with <=16
 * components, each <=255 bytes, no empty/dot/dot-dot component. Production
 * API and target object never accept this root/provider argument or export
 * this symbol. Fixture-only output scope is explicitly HOST_FIXTURE. */
mb_title_source_status mb_title_source_capture_fixture_v1(
    const mb_title_source_request_v1 *,mb_title_source_budget_v1 *,
    uint8_t *,size_t,mb_title_source_result_v1 *,
    const char *fixture_root,size_t fixture_root_bytes,
    const mb_title_source_test_ops_v1 *ops);
#ifdef __cplusplus
}
#endif
#endif
