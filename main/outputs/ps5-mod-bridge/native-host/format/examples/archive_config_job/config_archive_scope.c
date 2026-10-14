#include "config_archive_scope.h"
int pm_config_archive_scope(const mb_archive_inventory_v1 *inventory,
 const mb_archive_row_v1 *rows,uint32_t capacity,const mb_archive_config_candidate_v1 *candidate) {
 if(!inventory||!rows||!candidate||inventory->complete!=1||inventory->status!=MB_ARCHIVE_OK||
 !inventory->row_count||inventory->row_count>capacity||candidate->status!=MB_ARCHIVE_CONFIG_SELECTOR_OK||
 candidate->complete!=1||candidate->candidate_count!=1||candidate->row_index>=inventory->row_count||
 rows[candidate->row_index].kind!=MB_ARCHIVE_REGULAR)return PM_CONFIG_SCOPE_BAD_INVENTORY;
 for(uint32_t i=0;i<inventory->row_count;++i){
  if(rows[i].kind!=MB_ARCHIVE_REGULAR&&rows[i].kind!=MB_ARCHIVE_DIRECTORY)return PM_CONFIG_SCOPE_BAD_INVENTORY;
  if(rows[i].kind==MB_ARCHIVE_REGULAR&&i!=candidate->row_index)return PM_CONFIG_SCOPE_UNHANDLED_REGULAR;
 }
 return PM_CONFIG_SCOPE_OK;
}
