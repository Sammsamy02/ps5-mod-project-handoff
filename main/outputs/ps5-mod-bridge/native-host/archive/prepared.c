#include "prepared.h"
#include <stdint.h>
#include <string.h>

typedef struct prepared_span { uintptr_t begin,end; } prepared_span;
static int span(const void *p,size_t count,size_t width,size_t alignment,prepared_span *s) {
    size_t n;uintptr_t a=(uintptr_t)p;
    if(width&&count>SIZE_MAX/width)return 0;
    n=count*width;if(!p){s->begin=s->end=0;return n==0;}
    if(a%alignment||n>UINTPTR_MAX-a)return 0;
    s->begin=a;s->end=a+n;return 1;
}
static int disjoint(const prepared_span *s,unsigned n) {
    unsigned i,j;for(i=0;i<n;i++)for(j=0;j<i;j++)
        if(s[i].begin<s[i].end&&s[j].begin<s[j].end&&s[i].begin<s[j].end&&s[j].begin<s[i].end)return 0;
    return 1;
}
static void wipe(void *p,size_t n) {volatile unsigned char *b=p;while(n--)*b++=0;}
static void clear_workspace(const mb_archive_prepared_workspace_v1 *w) {
    wipe(w->sort_indices,(size_t)w->sort_capacity*sizeof(*w->sort_indices));
    wipe(w->groups,(size_t)w->group_capacity*sizeof(*w->groups));
    wipe(w->members,(size_t)w->member_capacity*sizeof(*w->members));wipe(w->keys,w->key_capacity);
}
static int failure(const mb_archive_prepared_workspace_v1 *w,uint32_t *selected,uint32_t sc,
    mb_archive_prepared_file_v1 *files,uint32_t fc,mb_archive_prepared_result_v1 *out,int status) {
    if(selected)wipe(selected,(size_t)sc*sizeof(*selected));
    wipe(files,(size_t)fc*sizeof(*files));clear_workspace(w);wipe(out,sizeof(*out));
    out->struct_size=sizeof(*out);out->abi_version=MB_ARCHIVE_PREPARED_ABI_V1;out->status=(uint32_t)status;
    return status;
}
static int preflight(const mb_archive_prepared_view_v1 *v,const mb_archive_prepared_profile_v1 *p,
    const mb_archive_prepared_workspace_v1 *w,uint32_t *selected,uint32_t sc,
    mb_archive_prepared_file_v1 *files,uint32_t fc,mb_archive_prepared_result_v1 *out,
    const mb_archive_prepared_result_v1 *c,const mb_archive_prepared_file_v1 *cf,uint32_t cc,
    const mb_archive_prepared_materialization_v1 *m) {
    prepared_span s[20];unsigned n=0;
    if(!v||!p||!w||!out||
       (uintptr_t)v%_Alignof(mb_archive_prepared_view_v1)||
       (uintptr_t)p%_Alignof(mb_archive_prepared_profile_v1)||
       (uintptr_t)w%_Alignof(mb_archive_prepared_workspace_v1))return 0;
    if(!v->inventory||p->struct_size!=sizeof(*p)||p->abi_version!=1||
       v->row_capacity>MB_ARCHIVE_MAX_ENTRIES||v->name_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
       w->sort_capacity>MB_ARCHIVE_MAX_ENTRIES||w->group_capacity>MB_ARCHIVE_MAX_ENTRIES||
       w->member_capacity>MB_ARCHIVE_MAX_ENTRIES||w->key_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
       sc>MB_ARCHIVE_MAX_ENTRIES||fc>MB_ARCHIVE_MAX_ENTRIES)return 0;
#define SP(P,C,T) do {if(!span((P),(C),sizeof(T),_Alignof(T),s+n))return 0;++n;} while(0)
    SP(v,1,mb_archive_prepared_view_v1);SP(p,1,mb_archive_prepared_profile_v1);
    SP(w,1,mb_archive_prepared_workspace_v1);SP(v->inventory,1,mb_archive_inventory_v1);
    SP(v->rows,v->row_capacity,mb_archive_row_v1);SP(v->names,v->name_capacity,char);
    SP(w->sort_indices,w->sort_capacity,uint32_t);SP(w->groups,w->group_capacity,mb_archive_group_v1);
    SP(w->members,w->member_capacity,mb_archive_group_member_v1);SP(w->keys,w->key_capacity,char);
    SP(files,fc,mb_archive_prepared_file_v1);SP(out,1,mb_archive_prepared_result_v1);
    if(selected||sc)SP(selected,sc,uint32_t);
    if(m) {
        if(!c||(uintptr_t)m%_Alignof(mb_archive_prepared_materialization_v1)||!m->result||
           cc>MB_ARCHIVE_MAX_ENTRIES||m->journal_capacity>MB_ARCHIVE_MAX_ENTRIES+3u)return 0;
        SP(c,1,mb_archive_prepared_result_v1);SP(cf,cc,mb_archive_prepared_file_v1);
        SP(m,1,mb_archive_prepared_materialization_v1);SP(m->result,1,mb_archive_materialized_v1);
        SP(m->journal,m->journal_capacity,mb_archive_owned_file_v1);
    }
#undef SP
    if(!disjoint(s,n)||v->inventory->struct_size!=sizeof(*v->inventory)||v->inventory->abi_version!=1)return 0;
    if(m&&(c->struct_size!=sizeof(*c)||c->abi_version!=1||
        m->result->struct_size!=sizeof(*m->result)||m->result->abi_version!=1))return 0;
    return 1;
}
static int text(const char *s,size_t capacity,int empty,size_t *length) {
    size_t i;for(i=0;i<capacity;i++){
        unsigned char c=(unsigned char)s[i];
        if(!c){*length=i;return empty||i>0;}
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;
    }return 0;
}
static int profile_ok(const mb_archive_prepared_profile_v1 *p,size_t *prefix,size_t *suffix) {
    unsigned i;for(i=0;i<32;i++)
        if(!((p->nonce[i]>='0'&&p->nonce[i]<='9')||(p->nonce[i]>='a'&&p->nonce[i]<='f')))return 0;
    return p->nonce[32]==0&&text(p->prefix,sizeof(p->prefix),0,prefix)&&text(p->suffix,sizeof(p->suffix),1,suffix);
}
static void decimal8(char *p,uint32_t n) {unsigned i=8;while(i){p[--i]=(char)('0'+n%10);n/=10;}}
static void names_for(mb_archive_prepared_file_v1 *f,const mb_archive_prepared_profile_v1 *p,size_t prefix,size_t suffix) {
    char *d=f->destination_name;const char *ext=f->role==MB_ARCHIVE_GROUP_ROLE_PAK?".pak":f->role==MB_ARCHIVE_GROUP_ROLE_UCAS?".ucas":".utoc";
    memcpy(f->source_name,"member-",7);decimal8(f->source_name+7,f->row_index);memcpy(f->source_name+15,".bin",5);
    memcpy(d,p->prefix,prefix);d+=prefix;memcpy(d,p->nonce,32);d+=32;*d++='-';
    decimal8(d,f->group_index);d+=8;memcpy(d,p->suffix,suffix);d+=suffix;memcpy(d,ext,strlen(ext)+1);
}
static int build(const mb_archive_prepared_view_v1 *v,const mb_archive_prepared_profile_v1 *p,
    const mb_archive_prepared_workspace_v1 *w,uint32_t *selected,uint32_t sc,
    mb_archive_prepared_file_v1 *files,uint32_t fc,mb_archive_prepared_result_v1 *out) {
    mb_archive_groups_result_v1 grouped;uint32_t g,i,r,n=0;size_t prefix,suffix;int e;
    const uint32_t order[3]={MB_ARCHIVE_GROUP_ROLE_UCAS,MB_ARCHIVE_GROUP_ROLE_PAK,MB_ARCHIVE_GROUP_ROLE_UTOC};
    wipe(out,sizeof(*out));wipe(files,(size_t)fc*sizeof(*files));if(selected)wipe(selected,(size_t)sc*sizeof(*selected));
    out->struct_size=sizeof(*out);out->abi_version=1;
    if(!profile_ok(p,&prefix,&suffix))return MB_ARCHIVE_PREPARED_BAD_ARGUMENT;
    if(v->inventory->struct_size!=sizeof(*v->inventory)||v->inventory->abi_version!=1||
       v->inventory->status!=MB_ARCHIVE_OK||v->inventory->complete!=1||v->inventory->installation_supported)
        return MB_ARCHIVE_PREPARED_INCOMPLETE;
    if(v->inventory->row_count>v->row_capacity)return MB_ARCHIVE_PREPARED_INCOMPLETE;
    if(v->inventory->row_count>w->sort_capacity)return MB_ARCHIVE_PREPARED_LIMIT;
    if(memcmp(p->target_token,v->inventory->target_token,32))return MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH;
    memset(&grouped,0,sizeof(grouped));
    e=mb_archive_groups_v1(v->inventory,v->rows,v->row_capacity,v->names,v->name_capacity,
        w->sort_indices,w->sort_capacity,w->groups,w->group_capacity,w->members,w->member_capacity,
        w->keys,w->key_capacity,&grouped);
    if(e)return e==MB_ARCHIVE_GROUPS_LIMIT?MB_ARCHIVE_PREPARED_LIMIT:MB_ARCHIVE_PREPARED_INCOMPLETE;
    if(grouped.member_count>fc||(selected&&grouped.member_count>sc))return MB_ARCHIVE_PREPARED_LIMIT;
    for(g=0;g<grouped.group_count;g++) {
        const mb_archive_group_v1 *group=w->groups+g;
        if(group->status!=MB_ARCHIVE_GROUP_STATUS_CANDIDATE||group->members_complete!=1||group->reason_flags)
            return MB_ARCHIVE_PREPARED_UNRESOLVED;
        for(r=0;r<3;r++)for(i=0;i<group->member_count;i++) {
            const mb_archive_group_member_v1 *member=w->members+group->member_begin+i;
            const mb_archive_row_v1 *row;
            mb_archive_prepared_file_v1 *f;
            if(member->role!=order[r])continue;
            row=v->rows+member->row_index;
            if(row->size>MB_ARCHIVE_MAX_MEMBER_BYTES||row->size>MB_ARCHIVE_MAX_EXPANDED_BYTES-out->total_bytes)
                return MB_ARCHIVE_PREPARED_LIMIT;
            f=files+n;f->group_index=g;f->row_index=member->row_index;f->role=member->role;
            f->bytes=row->size;memcpy(f->sha256,row->sha256,32);names_for(f,p,prefix,suffix);
            if(selected)selected[n]=member->row_index;
            ++n;out->total_bytes+=row->size;
        }
    }
    if(n!=grouped.member_count)return MB_ARCHIVE_PREPARED_INCOMPLETE;
    out->phase=MB_ARCHIVE_PREPARED_CANDIDATE;out->group_count=grouped.group_count;out->file_count=n;
    out->path_work_used=grouped.path_work_used;out->source_size=v->inventory->source_size;
    out->source_device=v->inventory->source_device;out->source_inode=v->inventory->source_inode;
    memcpy(out->source_sha256,v->inventory->source_sha256,32);memcpy(out->inventory_sha256,v->inventory->inventory_sha256,32);
    memcpy(out->target_token,p->target_token,32);memcpy(out->nonce,p->nonce,32);
    memcpy(out->prefix,p->prefix,prefix);memcpy(out->suffix,p->suffix,suffix);out->complete=1;return 0;
}
int mb_archive_prepare_groups_v1(const mb_archive_prepared_view_v1 *v,
    const mb_archive_prepared_profile_v1 *p,const mb_archive_prepared_workspace_v1 *w,
    uint32_t *selected,uint32_t sc,mb_archive_prepared_file_v1 *files,uint32_t fc,mb_archive_prepared_result_v1 *out) {
    int e;if(!preflight(v,p,w,selected,sc,files,fc,out,NULL,NULL,0,NULL))return MB_ARCHIVE_PREPARED_BAD_ARGUMENT;
    e=build(v,p,w,selected,sc,files,fc,out);
    if(e)return failure(w,selected,sc,files,fc,out,e);
    clear_workspace(w);return 0;
}
static int same_name(const char actual[40],const char *expected) {
    size_t n=strlen(expected);return n<40&&memcmp(actual,expected,n)==0&&actual[n]==0;
}
static int bind_materialization(const mb_archive_prepared_view_v1 *v,
    const mb_archive_prepared_materialization_v1 *m,mb_archive_prepared_file_v1 *files,
    mb_archive_prepared_result_v1 *out,const mb_archive_prepared_workspace_v1 *w) {
    const mb_archive_materialized_v1 *r=m->result;
    const mb_archive_owned_file_v1 *intent,*temp,*final;uint32_t i,n=out->file_count;
    if(!n||r->struct_size!=sizeof(*r)||r->abi_version!=1||r->status!=MB_ARCHIVE_OK||r->complete!=1||
       r->cleanup_complete!=1||r->installation_supported)return MB_ARCHIVE_PREPARED_INCOMPLETE;
    if(r->selected_count!=n||r->journal_count!=n+3u||r->journal_count>m->journal_capacity||
       r->decoded_bytes!=out->total_bytes||memcmp(r->inventory_sha256,out->inventory_sha256,32)||
       memcmp(r->target_token,out->target_token,32)||!same_name(r->manifest_name,"manifest-v1.bin"))
        return MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH;
    intent=m->journal;temp=m->journal+n+1;final=temp+1;
    if(intent->state!=MB_ARCHIVE_CREATED||intent->row_index!=UINT32_MAX||!same_name(intent->name,"intent-v1.bin")||
       intent->device!=m->quarantine_device||!intent->written_bytes||
       temp->state!=MB_ARCHIVE_REMOVED||temp->row_index!=UINT32_MAX||!same_name(temp->name,"manifest-v1.part")||
       final->state!=MB_ARCHIVE_CREATED||final->row_index!=UINT32_MAX||!same_name(final->name,"manifest-v1.bin")||
       temp->device!=m->quarantine_device||final->device!=m->quarantine_device||!temp->inode||!final->inode||
       !final->written_bytes||final->expected_size!=final->written_bytes||temp->expected_size!=final->expected_size||
       temp->written_bytes!=final->written_bytes||memcmp(temp->sha256,final->sha256,32))
        return MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH;
    /* The same row-index scratch has finished grouping and becomes a bounded
     * journal lookup map. No shared I/O ledger is reset or charged here. */
    for(i=0;i<v->inventory->row_count;i++)w->sort_indices[i]=UINT32_MAX;
    for(i=1;i<=n;i++) {
        const mb_archive_owned_file_v1 *f=m->journal+i;
        if(f->row_index>=v->inventory->row_count||w->sort_indices[f->row_index]!=UINT32_MAX||
           f->state!=MB_ARCHIVE_CREATED||f->device!=m->quarantine_device)
            return MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH;
        w->sort_indices[f->row_index]=i;
    }
    for(i=0;i<n;i++) {
        const mb_archive_owned_file_v1 *j;
        uint32_t index=w->sort_indices[files[i].row_index];
        if(index==UINT32_MAX)return MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH;
        j=m->journal+index;
        if(!same_name(j->name,files[i].source_name)||j->expected_size!=files[i].bytes||
           j->written_bytes!=files[i].bytes||memcmp(j->sha256,files[i].sha256,32))
            return MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH;
        files[i].device=j->device;files[i].inode=j->inode;
    }
    out->phase=MB_ARCHIVE_PREPARED_MATERIALIZED;out->quarantine_device=m->quarantine_device;
    out->quarantine_inode=m->quarantine_inode;memcpy(out->manifest_name,r->manifest_name,40);return 0;
}
int mb_archive_bind_prepared_v1(const mb_archive_prepared_view_v1 *v,
    const mb_archive_prepared_profile_v1 *p,const mb_archive_prepared_workspace_v1 *w,
    const mb_archive_prepared_result_v1 *candidate,const mb_archive_prepared_file_v1 *cf,uint32_t cc,
    const mb_archive_prepared_materialization_v1 *m,mb_archive_prepared_file_v1 *files,uint32_t fc,
    mb_archive_prepared_result_v1 *out) {
    int e;if(!m||!preflight(v,p,w,NULL,0,files,fc,out,candidate,cf,cc,m))return MB_ARCHIVE_PREPARED_BAD_ARGUMENT;
    e=build(v,p,w,NULL,0,files,fc,out);
    if(e)return failure(w,NULL,0,files,fc,out,e);
    if(out->file_count>cc||memcmp(candidate,out,sizeof(*out))||
       (out->file_count&&memcmp(cf,files,(size_t)out->file_count*sizeof(*files))))
        return failure(w,NULL,0,files,fc,out,MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH);
    e=bind_materialization(v,m,files,out,w);
    if(e)return failure(w,NULL,0,files,fc,out,e);
    clear_workspace(w);return 0;
}
