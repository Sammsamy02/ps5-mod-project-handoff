#include "candidate_selector.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct span { const void *pointer; size_t bytes; } span;

static int overlap(span a, span b) {
    uintptr_t ap, bp;
    if (!a.bytes || !b.bytes) return 0;
    if (!a.pointer || !b.pointer) return 1;
    ap=(uintptr_t)a.pointer; bp=(uintptr_t)b.pointer;
    if (a.bytes>UINTPTR_MAX-ap || b.bytes>UINTPTR_MAX-bp) return 1;
    return ap<bp+b.bytes && bp<ap+a.bytes;
}

static unsigned char fold_ascii(unsigned char c) {
    return c>='A'&&c<='Z'?(unsigned char)(c+('a'-'A')):c;
}

static int valid_basename(const char *name,uint32_t bytes) {
    uint32_t i;
    if (!name || !bytes || bytes>MB_ARCHIVE_CONFIG_SELECTOR_MAX_BASENAME) return 0;
    if ((bytes==1&&name[0]=='.')||(bytes==2&&name[0]=='.'&&name[1]=='.')) return 0;
    for(i=0;i<bytes;i++) {
        unsigned char c=(unsigned char)name[i];
        if(c<0x21||c>0x7e||c=='/'||c=='\\')return 0;
    }
    return 1;
}

static void reset_result(mb_archive_config_candidate_v1 *out,int status) {
    memset(out,0,sizeof(*out));
    out->struct_size=sizeof(*out);out->abi_version=MB_ARCHIVE_CONFIG_SELECTOR_ABI_V1;
    out->status=(uint32_t)status;out->row_index=UINT32_MAX;
}

static int rows_valid(const mb_archive_inventory_v1 *inv,const mb_archive_row_v1 *rows,
    const char *names,uint32_t name_capacity) {
    uint32_t i,j;
    for(i=0;i<inv->row_count;i++) {
        const mb_archive_row_v1 *r=rows+i;
        uint64_t off=r->path_offset,n=r->path_length;
        if((r->kind!=MB_ARCHIVE_REGULAR&&r->kind!=MB_ARCHIVE_DIRECTORY)||!n||
           off>=inv->name_bytes||n>=((uint64_t)inv->name_bytes-off)||
           off+n>=name_capacity||names[(size_t)(off+n)]!='\0'||
           n>MB_ARCHIVE_MAX_PATH_BYTES)return 0;
        for(j=0;j<(uint32_t)n;j++) {
            unsigned char c=(unsigned char)names[(size_t)off+j];
            if(c==0||c<0x20||c==0x7f||c=='\\')return 0;
        }
        if(r->kind==MB_ARCHIVE_DIRECTORY&&r->size!=0)return 0;
    }
    return 1;
}

int mb_archive_config_select_v1(const mb_archive_inventory_v1 *inventory,
    const mb_archive_row_v1 *rows,uint32_t row_capacity,const char *names,uint32_t name_capacity,
    const mb_archive_config_selector_profile_v1 *profile,mb_archive_config_candidate_v1 *result) {
    span spans[6];size_t i,j;uint32_t found=UINT32_MAX,matches=0;int status;
    if(!result||((uintptr_t)result%_Alignof(mb_archive_config_candidate_v1)))return MB_ARCHIVE_CONFIG_SELECTOR_BAD_ARGUMENT;
    if(!inventory||!profile)return MB_ARCHIVE_CONFIG_SELECTOR_BAD_ARGUMENT;
    if(row_capacity>MB_ARCHIVE_MAX_ENTRIES||name_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
       profile->struct_size!=sizeof(*profile)||profile->abi_version!=MB_ARCHIVE_CONFIG_SELECTOR_ABI_V1||
       profile->reserved||!valid_basename(profile->basename,profile->basename_bytes)||
       !profile->max_source_bytes||profile->max_source_bytes>MB_ARCHIVE_CONFIG_SELECTOR_MAX_SOURCE_BYTES||
       (row_capacity&&!rows)||(name_capacity&&!names)||
       inventory->struct_size!=sizeof(*inventory)||inventory->abi_version!=MB_ARCHIVE_ABI_V1||
       inventory->row_count>row_capacity||inventory->name_bytes>name_capacity||
       (inventory->row_count&&!rows)||(inventory->name_bytes&&!names)) {
        /* No result mutation before disjoint storage is established. */
        return MB_ARCHIVE_CONFIG_SELECTOR_BAD_ARGUMENT;
    }
    spans[0]=(span){inventory,sizeof(*inventory)};
    spans[1]=(span){rows,(size_t)row_capacity*sizeof(*rows)};
    spans[2]=(span){names,name_capacity};
    spans[3]=(span){profile,sizeof(*profile)};
    spans[4]=(span){profile->basename,profile->basename_bytes};
    spans[5]=(span){result,sizeof(*result)};
    for(i=0;i<6;i++)for(j=i+1;j<6;j++)if(overlap(spans[i],spans[j]))
        return MB_ARCHIVE_CONFIG_SELECTOR_BAD_ARGUMENT;
    reset_result(result,MB_ARCHIVE_CONFIG_SELECTOR_BAD_ARGUMENT);
    if(inventory->status!=MB_ARCHIVE_OK||inventory->complete!=1||inventory->installation_supported!=0||
       inventory->reserved!=0||inventory->row_count>MB_ARCHIVE_MAX_ENTRIES||
       inventory->name_bytes>MB_ARCHIVE_MAX_NAME_BYTES||
       memcmp(inventory->target_token,profile->target_token,32)!=0) {
        result->status=MB_ARCHIVE_CONFIG_SELECTOR_INCOMPLETE;
        return MB_ARCHIVE_CONFIG_SELECTOR_INCOMPLETE;
    }
    if(!rows_valid(inventory,rows,names,name_capacity)) {
        result->status=MB_ARCHIVE_CONFIG_SELECTOR_INVALID_INVENTORY;
        return MB_ARCHIVE_CONFIG_SELECTOR_INVALID_INVENTORY;
    }
    for(i=0;i<inventory->row_count;i++) {
        const mb_archive_row_v1 *r=rows+i;
        uint32_t start=r->path_offset,end=start+r->path_length,k;
        if(r->kind!=MB_ARCHIVE_REGULAR)continue;
        for(k=start;k<end;k++)if(names[k]=='/')start=k+1;
        if(end-start!=profile->basename_bytes)continue;
        for(k=0;k<profile->basename_bytes;k++)
            if(fold_ascii((unsigned char)names[start+k])!=fold_ascii((unsigned char)profile->basename[k]))break;
        if(k!=profile->basename_bytes)continue;
        matches++;
        if(found==UINT32_MAX)found=(uint32_t)i;
    }
    result->candidate_count=matches;
    memcpy(result->source_sha256,inventory->source_sha256,32);
    memcpy(result->inventory_sha256,inventory->inventory_sha256,32);
    memcpy(result->target_token,inventory->target_token,32);
    if(!matches)status=MB_ARCHIVE_CONFIG_SELECTOR_NO_CANDIDATE;
    else if(matches>1)status=MB_ARCHIVE_CONFIG_SELECTOR_AMBIGUOUS;
    else if(rows[found].size>profile->max_source_bytes)status=MB_ARCHIVE_CONFIG_SELECTOR_SOURCE_TOO_LARGE;
    else {
        result->row_index=found;
        result->member_bytes=rows[found].size;
        memcpy(result->member_sha256,rows[found].sha256,32);
        result->complete=1;
        status=MB_ARCHIVE_CONFIG_SELECTOR_OK;
    }
    result->status=(uint32_t)status;
    return status;
}
