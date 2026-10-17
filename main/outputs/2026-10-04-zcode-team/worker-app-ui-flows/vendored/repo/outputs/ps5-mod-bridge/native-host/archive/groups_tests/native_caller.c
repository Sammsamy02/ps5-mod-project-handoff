#include "groups.h"
#include <string.h>

int main(void) {
    mb_archive_inventory_v1 inventory;
    mb_archive_row_v1 rows[1];
    uint32_t sort_indices[1];
    mb_archive_group_v1 groups[1];
    mb_archive_group_member_v1 members[1];
    char names[]="fixture.pak", keys[32];
    mb_archive_groups_result_v1 result;
    memset(&inventory,0,sizeof(inventory));
    memset(rows,0,sizeof(rows));
    inventory.struct_size=sizeof(inventory); inventory.abi_version=MB_ARCHIVE_ABI_V1;
    inventory.status=MB_ARCHIVE_OK; inventory.complete=1; inventory.row_count=1;
    inventory.name_bytes=(uint32_t)sizeof(names)-1;
    rows[0].kind=MB_ARCHIVE_REGULAR; rows[0].path_length=(uint32_t)sizeof(names)-1;
    rows[0].size=1;
    return mb_archive_groups_v1(&inventory,rows,1,names,(uint32_t)sizeof(names)-1,
        sort_indices,1,groups,1,members,1,keys,sizeof(keys),&result)==MB_ARCHIVE_GROUPS_OK&&
        result.complete==1&&result.group_count==1&&groups[0].members_complete==1?0:1;
}
