#ifndef PM_CONFIG_ARCHIVE_SCOPE_H
#define PM_CONFIG_ARCHIVE_SCOPE_H
#include "candidate_selector.h"
enum { PM_CONFIG_SCOPE_OK=0,PM_CONFIG_SCOPE_BAD_INVENTORY=1,PM_CONFIG_SCOPE_UNHANDLED_REGULAR=2 };
/* Already admitted scanner/selector metadata only. This config-only route
 * permits directories and exactly its selected regular row. Additional
 * regular members are refused, never ignored or inferred to be optional. */
int pm_config_archive_scope(const mb_archive_inventory_v1 *,const mb_archive_row_v1 *,uint32_t,
 const mb_archive_config_candidate_v1 *);
#endif
