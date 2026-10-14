#include "groups.h"
#include <string.h>
int mb_archive_groups_v1(const mb_archive_inventory_v1 *inventory,
 const mb_archive_row_v1 *rows,uint32_t row_capacity,const char *names,uint32_t name_capacity,
 uint32_t *sort_indices,uint32_t sort_capacity,mb_archive_group_v1 *groups,uint32_t group_capacity,
 mb_archive_group_member_v1 *members,uint32_t member_capacity,char *key_bytes,uint32_t key_capacity,
 mb_archive_groups_result_v1 *result) {
 (void)rows;(void)row_capacity;(void)names;(void)name_capacity;
 if(groups==(void *)members) return MB_ARCHIVE_GROUPS_BAD_ARGUMENT;
 if(sort_indices)memset(sort_indices,0,(size_t)sort_capacity*sizeof(*sort_indices));
 if(groups)memset(groups,0,(size_t)group_capacity*sizeof(*groups));
 if(members)memset(members,0,(size_t)member_capacity*sizeof(*members));
 if(key_bytes)memset(key_bytes,0,key_capacity);
 if(result){memset(result,0,sizeof(*result));result->struct_size=sizeof(*result);
  result->abi_version=MB_ARCHIVE_GROUPS_ABI_V1;result->status=MB_ARCHIVE_GROUPS_INCOMPLETE_INVENTORY;}
 (void)inventory;return MB_ARCHIVE_GROUPS_INCOMPLETE_INVENTORY;
}
