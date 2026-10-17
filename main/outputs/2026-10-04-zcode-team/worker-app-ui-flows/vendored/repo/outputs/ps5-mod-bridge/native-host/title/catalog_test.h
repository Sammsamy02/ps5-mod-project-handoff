#ifndef MODBRIDGE_TITLE_CATALOG_TEST_H
#define MODBRIDGE_TITLE_CATALOG_TEST_H
#ifndef MB_TITLE_CATALOG_HOST_TESTING
#error "Fixture root/provider seam is host-test-only"
#endif
#ifdef __FreeBSD__
#error "Fixture root/provider seam must never compile for the native SDK target"
#endif

#include "catalog.h"
#include "source_test.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct mb_title_catalog_test_ops_v1 {
    uint32_t struct_size, abi_version;
    void *context;
    int (*lstat_path)(void *,const char *,struct stat *);
    int (*open_path)(void *,const char *,int);
    int (*stat_relative)(void *,int,const char *,struct stat *,int);
    int (*open_relative)(void *,int,const char *,int);
    int (*stat_fd)(void *,int,struct stat *);
    int (*close_fd)(void *,int);
    DIR *(*fdopendir_stream)(void *,int);
    struct dirent *(*readdir_stream)(void *,DIR *);
    int (*closedir_stream)(void *,DIR *);
} mb_title_catalog_test_ops_v1;

#ifdef __cplusplus
extern "C" {
#endif
/* The only arbitrary-root entry point. It exists solely in host test builds;
 * NULL operation tables select ordinary local POSIX calls. The source root is
 * the already-selected fixture cache directory containing <candidate>/
 * param.json. Neither root nor callbacks are accepted by the native API. */
mb_title_catalog_status mb_title_catalog_scan_fixture_v1(
    const mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,
    char *entry_name_scratch,size_t entry_name_scratch_bytes,
    char *candidate_scratch,size_t candidate_scratch_bytes,
    uint8_t *source_workspace,size_t source_workspace_bytes,
    mb_title_metadata_result_v1 *records,uint32_t record_capacity,
    uint32_t *issue_codes,uint32_t issue_capacity,
    mb_title_catalog_result_v1 *result,
    const char *fixture_root,size_t fixture_root_bytes,
    const mb_title_source_test_ops_v1 *source_ops,
    const mb_title_catalog_test_ops_v1 *catalog_ops);
#ifdef __cplusplus
}
#endif
#endif
