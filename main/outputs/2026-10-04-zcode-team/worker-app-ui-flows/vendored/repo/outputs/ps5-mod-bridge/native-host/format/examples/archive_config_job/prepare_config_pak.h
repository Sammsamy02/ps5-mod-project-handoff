#ifndef PM_PREPARE_CONFIG_PAK_HOST_H
#define PM_PREPARE_CONFIG_PAK_HOST_H
#include <stdio.h>
#include <stdint.h>
typedef struct pm_prepare_output_owner {
    uint64_t device, inode;
    int created;
} pm_prepare_output_owner;
/* Same-process host preparation only. args match the previous profiled CLI;
 * argv[3] is an owning journal label, not opened as a filesystem path.
 * selected_member is caller-owned, read-only, identity/hash bound and stable;
 * this function leaves it open. No external process/helper is invoked. */
int pm_prepare_config_pak_host(int argc,char **argv,FILE *selected_member,
                              pm_prepare_output_owner *output_owner);
#endif
