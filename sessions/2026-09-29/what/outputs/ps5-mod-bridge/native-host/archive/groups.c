#include "groups.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct groups_span { uintptr_t begin, end; } groups_span;
typedef struct groups_context {
    const mb_archive_inventory_v1 *inventory;
    const mb_archive_row_v1 *rows;
    const char *names;
    uint32_t *indices;
    mb_archive_group_v1 *groups;
    mb_archive_group_member_v1 *members;
    char *keys;
    uint64_t work;
    int exhausted;
    uint32_t group_count, member_count, key_bytes;
} groups_context;
typedef struct groups_key_view {
    const char *parent, *stem;
    uint32_t parent_length, stem_length, role, row_index;
} groups_key_view;

static int make_span(const void *pointer,size_t count,size_t item_size,size_t alignment,groups_span *out) {
    const uintptr_t begin=(uintptr_t)pointer;
    size_t bytes;
    if(item_size&&count>SIZE_MAX/item_size)return 0;
    bytes=count*item_size;
    if(!pointer){out->begin=0;out->end=0;return bytes==0;}
    if(!alignment||begin%alignment||bytes>UINTPTR_MAX-begin)return 0;
    out->begin=begin;out->end=begin+(uintptr_t)bytes;return 1;
}

static int disjoint_spans(const groups_span *spans,uint32_t count) {
    uint32_t i,j;
    for(i=0;i<count;++i)for(j=0;j<i;++j)
        if(spans[i].begin<spans[i].end&&spans[j].begin<spans[j].end&&
           spans[i].begin<spans[j].end&&spans[j].begin<spans[i].end)return 0;
    return 1;
}

static void clear_bytes(void *memory,size_t bytes) {
    volatile unsigned char *p=(volatile unsigned char *)memory;
    while(bytes--)*p++=0;
}

static void clear_outputs(mb_archive_group_v1 *groups,uint32_t group_capacity,
    mb_archive_group_member_v1 *members,uint32_t member_capacity,char *keys,uint32_t key_capacity,
    uint32_t *indices,uint32_t index_capacity,mb_archive_groups_result_v1 *result,uint32_t status) {
    if(groups&&group_capacity)clear_bytes(groups,(size_t)group_capacity*sizeof(*groups));
    if(members&&member_capacity)clear_bytes(members,(size_t)member_capacity*sizeof(*members));
    if(keys&&key_capacity)clear_bytes(keys,key_capacity);
    if(indices&&index_capacity)clear_bytes(indices,(size_t)index_capacity*sizeof(*indices));
    clear_bytes(result,sizeof(*result));
    result->struct_size=sizeof(*result);result->abi_version=MB_ARCHIVE_GROUPS_ABI_V1;
    result->status=status;
}

static int charge(groups_context *context) {
    if(context->work>=MB_ARCHIVE_GROUPS_MAX_PATH_WORK){context->exhausted=1;return 0;}
    ++context->work;return 1;
}

static int path_is_normalized(groups_context *context,const char *path,uint32_t length) {
    uint32_t i,component=0;
    if(!length||length>MB_ARCHIVE_MAX_PATH_BYTES||path[0]=='/'||path[0]=='\\')return 0;
    for(i=0;i<length;++i) {
        const unsigned char ch=(unsigned char)path[i];
        if(!charge(context))return 0;
        if(ch==0||ch=='\\'||ch<32||ch==127||ch==':')return 0;
        if(ch=='/') {
            const uint32_t part=i-component;
            if(!part||(part==1&&path[component]=='.')||
               (part==2&&path[component]=='.'&&path[component+1]=='.')||
               path[i-1]=='.'||path[i-1]==' ')return 0;
            component=i+1;
        }
    }
    if(component>=length)return 0;
    if((length-component==1&&path[component]=='.')||
       (length-component==2&&path[component]=='.'&&path[component+1]=='.')||
       path[length-1]=='.'||path[length-1]==' ')return 0;
    return 1;
}

static unsigned char ascii_lower(unsigned char value) {
    return value>='A'&&value<='Z'?(unsigned char)(value-'A'+'a'):value;
}

static int suffix_role(const char *path,uint32_t dot,uint32_t length,uint32_t *role) {
    const uint32_t suffix_length=length-dot-1;
    if(dot>=length)return 0;
    if(suffix_length==3&&ascii_lower((unsigned char)path[dot+1])=='p'&&
       ascii_lower((unsigned char)path[dot+2])=='a'&&ascii_lower((unsigned char)path[dot+3])=='k'){
        *role=MB_ARCHIVE_GROUP_ROLE_PAK;return 1;
    }
    if(suffix_length==4&&ascii_lower((unsigned char)path[dot+1])=='u'&&
       ascii_lower((unsigned char)path[dot+2])=='t'&&ascii_lower((unsigned char)path[dot+3])=='o'&&
       ascii_lower((unsigned char)path[dot+4])=='c'){
        *role=MB_ARCHIVE_GROUP_ROLE_UTOC;return 1;
    }
    if(suffix_length==4&&ascii_lower((unsigned char)path[dot+1])=='u'&&
       ascii_lower((unsigned char)path[dot+2])=='c'&&ascii_lower((unsigned char)path[dot+3])=='a'&&
       ascii_lower((unsigned char)path[dot+4])=='s'){
        *role=MB_ARCHIVE_GROUP_ROLE_UCAS;return 1;
    }
    return 0;
}

static int row_key(groups_context *context,uint32_t row_index,groups_key_view *view,int validate) {
    const mb_archive_row_v1 *row=&context->rows[row_index];
    const char *path=context->names+row->path_offset;
    const uint32_t length=row->path_length;
    uint32_t i,leaf=0,dot=UINT32_MAX,role=0;
    if(validate&&!path_is_normalized(context,path,length))return 0;
    for(i=0;i<length;++i) {
        if(!charge(context))return 0;
        if(path[i]=='/') {leaf=i+1;dot=UINT32_MAX;}
        else if(path[i]=='.') dot=i;
    }
    if(dot==UINT32_MAX||dot<leaf)return 0;
    if(!suffix_role(path,dot,length,&role))return 0;
    view->parent=path;view->parent_length=leaf?leaf-1:0;
    view->stem=path+leaf;view->stem_length=dot-leaf;
    view->role=role;view->row_index=row_index;
    return 1;
}

static int compare_bytes(groups_context *context,const char *a,uint32_t a_length,
    const char *b,uint32_t b_length,int fold_ascii) {
    const uint32_t common=a_length<b_length?a_length:b_length;
    uint32_t i;
    for(i=0;i<common;++i) {
        unsigned char left=(unsigned char)a[i],right=(unsigned char)b[i];
        if(!charge(context))return 0;
        if(fold_ascii){left=ascii_lower(left);right=ascii_lower(right);}
        if(left<right)return -1;
        if(left>right)return 1;
    }
    return a_length<b_length?-1:a_length>b_length?1:0;
}

static int compare_views(groups_context *context,const groups_key_view *a,const groups_key_view *b) {
    int value=compare_bytes(context,a->parent,a->parent_length,b->parent,b->parent_length,0);
    if(value||context->exhausted)return value;
    value=compare_bytes(context,a->stem,a->stem_length,b->stem,b->stem_length,0);
    if(value||context->exhausted)return value;
    if(a->role<b->role)return -1;
    if(a->role>b->role)return 1;
    return a->row_index<b->row_index?-1:a->row_index>b->row_index?1:0;
}

static int compare_rows(groups_context *context,uint32_t left,uint32_t right) {
    groups_key_view a,b;
    if(!row_key(context,left,&a,0)||!row_key(context,right,&b,0))return 0;
    return compare_views(context,&a,&b);
}

static void swap_indices(uint32_t *a,uint32_t *b) {const uint32_t value=*a;*a=*b;*b=value;}

typedef int (*groups_compare_fn)(groups_context *,uint32_t,uint32_t);

static void sift_down(groups_context *context,uint32_t *values,uint32_t root,uint32_t count,
    groups_compare_fn compare) {
    while(root<count/2&&!context->exhausted) {
        const uint32_t child=root*2+1;
        uint32_t largest=root;
        if(compare(context,values[largest],values[child])<0)largest=child;
        if(child+1<count&&compare(context,values[largest],values[child+1])<0)largest=child+1;
        if(largest==root)break;
        swap_indices(&values[root],&values[largest]);root=largest;
    }
}

static void heapsort(groups_context *context,uint32_t *values,uint32_t count,groups_compare_fn compare) {
    uint32_t start,end;
    if(count<2)return;
    start=count/2;
    while(start>0&&!context->exhausted){--start;sift_down(context,values,start,count,compare);}
    end=count;
    while(end>1&&!context->exhausted){--end;swap_indices(&values[0],&values[end]);sift_down(context,values,0,end,compare);}
}

static int compare_groups_folded(groups_context *context,uint32_t left,uint32_t right) {
    const mb_archive_group_v1 *a=&context->groups[left],*b=&context->groups[right];
    int value=compare_bytes(context,context->keys+a->parent_offset,a->parent_length,
        context->keys+b->parent_offset,b->parent_length,0);
    if(value||context->exhausted)return value;
    value=compare_bytes(context,context->keys+a->stem_offset,a->stem_length,
        context->keys+b->stem_offset,b->stem_length,1);
    if(value||context->exhausted)return value;
    value=compare_bytes(context,context->keys+a->stem_offset,a->stem_length,
        context->keys+b->stem_offset,b->stem_length,0);
    if(value||context->exhausted)return value;
    return left<right?-1:left>right?1:0;
}

static int same_folded_key(groups_context *context,uint32_t left,uint32_t right) {
    const mb_archive_group_v1 *a=&context->groups[left],*b=&context->groups[right];
    return compare_bytes(context,context->keys+a->parent_offset,a->parent_length,
        context->keys+b->parent_offset,b->parent_length,0)==0&&
        compare_bytes(context,context->keys+a->stem_offset,a->stem_length,
        context->keys+b->stem_offset,b->stem_length,1)==0;
}

static int group_has_role(const groups_context *context,const mb_archive_group_v1 *group,uint32_t role) {
    uint32_t i;
    for(i=0;i<group->member_count;++i)
        if(context->members[group->member_begin+i].role==role)return 1;
    return 0;
}

static int compare_group_to_key(groups_context *context,const mb_archive_group_v1 *group,
    const char *parent,uint32_t parent_length,const char *stem,uint32_t stem_length) {
    int value=compare_bytes(context,context->keys+group->parent_offset,group->parent_length,
        parent,parent_length,0);
    if(value||context->exhausted)return value;
    return compare_bytes(context,context->keys+group->stem_offset,group->stem_length,
        stem,stem_length,0);
}

static int find_group(groups_context *context,const char *parent,uint32_t parent_length,
    const char *stem,uint32_t stem_length,uint32_t *found) {
    uint32_t low=0,high=context->group_count;
    while(low<high&&!context->exhausted) {
        const uint32_t middle=low+(high-low)/2;
        const int compare=compare_group_to_key(context,&context->groups[middle],parent,parent_length,stem,stem_length);
        if(compare==0){*found=middle;return 1;}
        if(compare<0)low=middle+1;else high=middle;
    }
    return 0;
}

static int group_suffix_partition(groups_context *context,const mb_archive_group_v1 *group,uint32_t *suffix_out) {
    uint32_t cursor=group->stem_length;
    const char *stem=context->keys+group->stem_offset;
    while(cursor>=2&&!context->exhausted) {
        --cursor;
        if(!charge(context))return -1;
        if(stem[cursor]=='s'&&stem[cursor-1]=='_') {
            uint32_t i;
            if(cursor<2||cursor+1>=group->stem_length)return 0;
            for(i=cursor+1;i<group->stem_length;++i) {
                if(!charge(context))return -1;
                if(stem[i]<'0'||stem[i]>'9')return 0;
            }
            *suffix_out=cursor-1;return 1;
        }
    }
    return context->exhausted?-1:0;
}

static int validate_argument_spans(const mb_archive_inventory_v1 *inventory,
    const mb_archive_row_v1 *rows,uint32_t row_capacity,const char *names,uint32_t name_capacity,
    uint32_t *indices,uint32_t index_capacity,mb_archive_group_v1 *groups,uint32_t group_capacity,
    mb_archive_group_member_v1 *members,uint32_t member_capacity,char *keys,uint32_t key_capacity,
    mb_archive_groups_result_v1 *result) {
    groups_span spans[8];uint32_t n=0,i;
    if(!inventory||!result||row_capacity>MB_ARCHIVE_MAX_ENTRIES||name_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
       index_capacity>MB_ARCHIVE_GROUPS_MAX_GROUPS||group_capacity>MB_ARCHIVE_GROUPS_MAX_GROUPS||
       member_capacity>MB_ARCHIVE_GROUPS_MAX_MEMBERS||key_capacity>MB_ARCHIVE_GROUPS_MAX_KEY_BYTES)return 0;
    if(inventory->struct_size!=sizeof(*inventory)||inventory->abi_version!=MB_ARCHIVE_ABI_V1||
       inventory->reserved||inventory->installation_supported||inventory->row_count>MB_ARCHIVE_MAX_ENTRIES||
       inventory->row_count>row_capacity||inventory->name_bytes>MB_ARCHIVE_MAX_NAME_BYTES||
       inventory->name_bytes>name_capacity)return 0;
    if(!make_span(inventory,1,sizeof(*inventory),_Alignof(mb_archive_inventory_v1),&spans[n++])||
       !make_span(rows,row_capacity,sizeof(*rows),_Alignof(mb_archive_row_v1),&spans[n++])||
       !make_span(names,name_capacity,1,1,&spans[n++])||
       !make_span(indices,index_capacity,sizeof(*indices),_Alignof(uint32_t),&spans[n++])||
       !make_span(groups,group_capacity,sizeof(*groups),_Alignof(mb_archive_group_v1),&spans[n++])||
       !make_span(members,member_capacity,sizeof(*members),_Alignof(mb_archive_group_member_v1),&spans[n++])||
       !make_span(keys,key_capacity,1,1,&spans[n++])||
       !make_span(result,1,sizeof(*result),_Alignof(mb_archive_groups_result_v1),&spans[n++])||
       !disjoint_spans(spans,n))return 0;
    if(inventory->name_bytes&& !names)return 0;
    for(i=0;i<inventory->row_count;++i) {
        const mb_archive_row_v1 *row=&rows[i];
        if(row->path_length>MB_ARCHIVE_MAX_PATH_BYTES||
           (row->kind==MB_ARCHIVE_REGULAR&&!row->path_length)||row->path_offset>inventory->name_bytes||
           row->path_length>inventory->name_bytes-row->path_offset)return 0;
        if(row->path_length&&!names)return 0;
    }
    return 1;
}

static int fail_late(groups_context *context,uint32_t status,uint32_t group_capacity,
    uint32_t member_capacity,uint32_t key_capacity,uint32_t index_capacity,
    mb_archive_groups_result_v1 *result) {
    clear_outputs(context->groups,group_capacity,context->members,member_capacity,context->keys,key_capacity,
        context->indices,index_capacity,result,status);
    return (int)status;
}

int mb_archive_groups_v1(const mb_archive_inventory_v1 *inventory,
    const mb_archive_row_v1 *rows,uint32_t row_capacity,const char *names,uint32_t name_capacity,
    uint32_t *sort_indices,uint32_t sort_capacity,mb_archive_group_v1 *groups,uint32_t group_capacity,
    mb_archive_group_member_v1 *members,uint32_t member_capacity,char *key_bytes,uint32_t key_capacity,
    mb_archive_groups_result_v1 *result) {
    groups_context context;uint32_t i,candidate_count=0;
    if(!validate_argument_spans(inventory,rows,row_capacity,names,name_capacity,sort_indices,sort_capacity,
       groups,group_capacity,members,member_capacity,key_bytes,key_capacity,result))
        return MB_ARCHIVE_GROUPS_BAD_ARGUMENT;
    memset(&context,0,sizeof(context));context.inventory=inventory;context.rows=rows;context.names=names;
    context.indices=sort_indices;context.groups=groups;context.members=members;context.keys=key_bytes;
    clear_outputs(groups,group_capacity,members,member_capacity,key_bytes,key_capacity,sort_indices,sort_capacity,
        result,MB_ARCHIVE_GROUPS_BAD_ARGUMENT);
    if(inventory->status!=MB_ARCHIVE_OK||inventory->complete!=1)
        return fail_late(&context,MB_ARCHIVE_GROUPS_INCOMPLETE_INVENTORY,group_capacity,member_capacity,
            key_capacity,sort_capacity,result);

    /* Validate every regular path before any path-derived candidate key is sorted. */
    for(i=0;i<inventory->row_count;++i) {
        const mb_archive_row_v1 *row=&rows[i];
        if(row->reserved|| (row->kind!=MB_ARCHIVE_REGULAR&&row->kind!=MB_ARCHIVE_DIRECTORY))
            return fail_late(&context,MB_ARCHIVE_GROUPS_INVALID_ROW,group_capacity,member_capacity,
                key_capacity,sort_capacity,result);
        if(row->kind==MB_ARCHIVE_REGULAR&&!path_is_normalized(&context,names+row->path_offset,row->path_length))
            return fail_late(&context,context.exhausted?MB_ARCHIVE_GROUPS_LIMIT:MB_ARCHIVE_GROUPS_INVALID_ROW,
                group_capacity,member_capacity,key_capacity,sort_capacity,result);
    }

    for(i=0;i<inventory->row_count;++i) {
        groups_key_view key;
        if(rows[i].kind!=MB_ARCHIVE_REGULAR)continue;
        if(!row_key(&context,i,&key,0)) {
            if(context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
                key_capacity,sort_capacity,result);
            continue;
        }
        if(candidate_count>=sort_capacity)
            return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,key_capacity,sort_capacity,result);
        sort_indices[candidate_count++]=i;
    }
    heapsort(&context,sort_indices,candidate_count,compare_rows);
    if(context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
        key_capacity,sort_capacity,result);

    for(i=0;i<candidate_count;++i) {
        groups_key_view key;mb_archive_group_v1 *group;
        const uint32_t row_index=sort_indices[i];int new_group=0;
        if(!row_key(&context,row_index,&key,0))
            return fail_late(&context,context.exhausted?MB_ARCHIVE_GROUPS_LIMIT:MB_ARCHIVE_GROUPS_INVALID_ROW,
                group_capacity,member_capacity,key_capacity,sort_capacity,result);
        if(!context.group_count) new_group=1;
        else {
            group=&groups[context.group_count-1];
            const int parent_compare=compare_bytes(&context,key.parent,key.parent_length,
                key_bytes+group->parent_offset,group->parent_length,0);
            const int stem_compare=parent_compare?parent_compare:compare_bytes(&context,key.stem,key.stem_length,
                key_bytes+group->stem_offset,group->stem_length,0);
            if(context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
                key_capacity,sort_capacity,result);
            new_group=parent_compare!=0||stem_compare!=0;
            if(!new_group&&group->member_count&&members[group->member_begin+group->member_count-1].role==key.role)
                return fail_late(&context,MB_ARCHIVE_GROUPS_DUPLICATE_ROLE,group_capacity,member_capacity,
                    key_capacity,sort_capacity,result);
        }
        if(new_group) {
            uint32_t key_need;
            if(context.group_count>=group_capacity||context.group_count>=MB_ARCHIVE_GROUPS_MAX_GROUPS)
                return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,key_capacity,sort_capacity,result);
            if(key.parent_length>UINT32_MAX-key.stem_length)
                return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,key_capacity,sort_capacity,result);
            key_need=key.parent_length+key.stem_length;
            if(key_need>key_capacity-context.key_bytes)
                return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,key_capacity,sort_capacity,result);
            group=&groups[context.group_count];memset(group,0,sizeof(*group));
            group->parent_offset=context.key_bytes;group->parent_length=key.parent_length;
            if(key.parent_length)memcpy(key_bytes+context.key_bytes,key.parent,key.parent_length);
            context.key_bytes+=key.parent_length;
            group->stem_offset=context.key_bytes;group->stem_length=key.stem_length;
            if(key.stem_length)memcpy(key_bytes+context.key_bytes,key.stem,key.stem_length);
            context.key_bytes+=key.stem_length;
            group->member_begin=context.member_count;group->kind=MB_ARCHIVE_GROUP_PAK;
            group->validation_unverified=1;group->installation_supported=0;group->runtime_supported=0;group->format_validated=0;
            ++context.group_count;
        }
        group=&groups[context.group_count-1];
        if(context.member_count>=member_capacity||context.member_count>=MB_ARCHIVE_GROUPS_MAX_MEMBERS)
            return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,key_capacity,sort_capacity,result);
        members[context.member_count].row_index=row_index;members[context.member_count].role=key.role;
        ++context.member_count;++group->member_count;
    }

    for(i=0;i<context.group_count;++i) {
        mb_archive_group_v1 *group=&groups[i];
        const int toc=group_has_role(&context,group,MB_ARCHIVE_GROUP_ROLE_UTOC);
        const int cas=group_has_role(&context,group,MB_ARCHIVE_GROUP_ROLE_UCAS);
        const int pak=group_has_role(&context,group,MB_ARCHIVE_GROUP_ROLE_PAK);
        if(toc||cas) {
            group->kind=MB_ARCHIVE_GROUP_IOSTORE;
            if(!(toc&&cas))group->reason_flags|=MB_ARCHIVE_GROUP_REASON_MEMBER_MISSING;
        } else {
            group->kind=MB_ARCHIVE_GROUP_PAK;
            if(!pak)group->reason_flags|=MB_ARCHIVE_GROUP_REASON_MEMBER_MISSING;
        }
    }

    for(i=0;i<context.group_count;++i)sort_indices[i]=i;
    heapsort(&context,sort_indices,context.group_count,compare_groups_folded);
    if(context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
        key_capacity,sort_capacity,result);
    i=0;
    while(i<context.group_count) {
        uint32_t end=i+1,j;
        while(end<context.group_count&&same_folded_key(&context,sort_indices[i],sort_indices[end]))++end;
        if(context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
            key_capacity,sort_capacity,result);
        if(end-i>1)for(j=i;j<end;++j)
            groups[sort_indices[j]].reason_flags|=MB_ARCHIVE_GROUP_REASON_CASE_AMBIGUITY;
        i=end;
    }

    for(i=0;i<context.group_count;++i) {
        mb_archive_group_v1 *partition=&groups[i];uint32_t suffix=0,base_index=0;
        int found;
        const char *stem=key_bytes+partition->stem_offset;
        if(!group_has_role(&context,partition,MB_ARCHIVE_GROUP_ROLE_UCAS))continue;
        found=group_suffix_partition(&context,partition,&suffix);
        if(found<0||context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
            key_capacity,sort_capacity,result);
        if(!found)continue;
        const mb_archive_group_v1 *base=partition;
        if(!find_group(&context,key_bytes+partition->parent_offset,partition->parent_length,
            stem,suffix,&base_index)) {
            if(context.exhausted)return fail_late(&context,MB_ARCHIVE_GROUPS_LIMIT,group_capacity,member_capacity,
                key_capacity,sort_capacity,result);
            continue;
        }
        base=&groups[base_index];
        if(group_has_role(&context,base,MB_ARCHIVE_GROUP_ROLE_UTOC)) {
            partition->reason_flags|=MB_ARCHIVE_GROUP_REASON_PARTITION_UNRESOLVED;
            groups[base_index].reason_flags|=MB_ARCHIVE_GROUP_REASON_PARTITION_UNRESOLVED;
        }
    }

    for(i=0;i<context.group_count;++i) {
        groups[i].members_complete=groups[i].reason_flags==0;
        groups[i].status=groups[i].members_complete?MB_ARCHIVE_GROUP_STATUS_CANDIDATE:MB_ARCHIVE_GROUP_STATUS_UNRESOLVED;
        groups[i].installation_supported=0;groups[i].runtime_supported=0;groups[i].format_validated=0;
    }
    result->struct_size=sizeof(*result);result->abi_version=MB_ARCHIVE_GROUPS_ABI_V1;
    result->status=MB_ARCHIVE_GROUPS_OK;result->complete=1;
    result->group_count=context.group_count;result->member_count=context.member_count;result->key_bytes=context.key_bytes;
    result->path_work_used=context.work;
    memcpy(result->source_sha256,inventory->source_sha256,32);
    memcpy(result->inventory_sha256,inventory->inventory_sha256,32);
    memcpy(result->target_token,inventory->target_token,32);
    result->installation_supported=0;result->runtime_supported=0;result->format_validated=0;
    return MB_ARCHIVE_GROUPS_OK;
}
