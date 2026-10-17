#include "pages.h"
#include <string.h>

/* All loops have fixed input/field/page caps. Count and write share one encoder.
 * Borrowed pointers below exist only on this synchronous bounded call's stack. */
typedef struct span { uintptr_t begin,end; } span;
typedef struct text_view { const char *bytes; size_t length; unsigned state; } text_view;
typedef struct row_view { text_view id,name,version; } row_view;
typedef struct checked_view {
    const mb_title_catalog_result_v1 *catalog;
    const uint32_t *issues;
    const char *source;
    row_view rows[MB_TITLE_PAGES_RECORD_LIMIT];
} checked_view;
typedef struct encoder { uint8_t *bytes; size_t size,limit; unsigned failed; } encoder;

static const char placeholder[33]="ffffffffffffffffffffffffffffffff";
static const char *const reasons[7]={NULL,"platform_provider_unavailable","provider_error",
    "invalid_provider_capture","inventory_resource_limit","source_changed",
    "provider_observation_incomplete"};
static const char *const statuses[4]={"observed","partial","unavailable","error"};

static int add_span(span *spans,unsigned *count,const void *data,size_t bytes,size_t alignment) {
    uintptr_t begin=(uintptr_t)data,end;
    unsigned n;
    if(!bytes) return data==NULL;
    if(!data || begin%alignment || bytes>UINTPTR_MAX-begin) return 0;
    end=begin+bytes;
    for(n=0;n<*count;++n)
        if(begin<spans[n].end && spans[n].begin<end) return 0;
    spans[*count].begin=begin; spans[*count].end=end; ++*count;
    return 1;
}
static int preflight_view(const mb_title_pages_view_v1 *v,span *spans,unsigned *count) {
    if(!add_span(spans,count,v,sizeof(*v),_Alignof(mb_title_pages_view_v1))) return 0;
    if(v->struct_size!=sizeof(*v) || v->abi_version!=1 || v->reserved[0] || v->reserved[1] ||
       v->records_capacity>MB_TITLE_PAGES_RECORD_LIMIT || v->issues_capacity>MB_TITLE_PAGES_ISSUE_LIMIT)
        return 0;
    return add_span(spans,count,v->catalog,sizeof(*v->catalog),_Alignof(mb_title_catalog_result_v1)) &&
        add_span(spans,count,v->records,(size_t)v->records_capacity*sizeof(*v->records),
                 _Alignof(mb_title_metadata_result_v1)) &&
        add_span(spans,count,v->issues,(size_t)v->issues_capacity*sizeof(*v->issues),_Alignof(uint32_t));
}
static int preflight_tokens(const mb_title_pages_tokens_v1 *t,span *spans,unsigned *count) {
    if(!t) return 1;
    if(!add_span(spans,count,t,sizeof(*t),_Alignof(mb_title_pages_tokens_v1))) return 0;
    if(t->struct_size!=sizeof(*t) || t->abi_version!=1 || t->reserved[0] || t->reserved[1] ||
       t->title_count>MB_TITLE_PAGES_RECORD_LIMIT || t->cursor_count>=MB_TITLE_PAGES_PAGE_LIMIT) return 0;
    return add_span(spans,count,t->title_keys,(size_t)t->title_count*33u,1) &&
        add_span(spans,count,t->cursors,(size_t)t->cursor_count*33u,1);
}
static void initialize_plan(mb_title_pages_plan_v1 *p,unsigned status,unsigned reason) {
    memset(p,0,sizeof(*p)); p->struct_size=sizeof(*p); p->abi_version=1;
    p->status=status; p->reason=reason;
}
static mb_title_pages_status fail_plan(mb_title_pages_plan_v1 *p,unsigned status,unsigned reason) {
    initialize_plan(p,status,reason); return (mb_title_pages_status)status;
}
static mb_title_pages_status fail_cache(mb_title_pages_cache_v1 *c,uint8_t *b,size_t bytes,
                                       unsigned status,unsigned reason) {
    if(bytes) memset(b,0,bytes);
    memset(c,0,sizeof(*c)); c->struct_size=sizeof(*c); c->abi_version=1;
    c->status=status; c->reason=reason; initialize_plan(&c->plan,status,reason);
    return (mb_title_pages_status)status;
}

/* UTF-8 strict scalar decoding: excludes overlong forms, surrogates, >10ffff,
 * incomplete tails and continuation bytes used as a leader. */
static int next_scalar(const char *bytes,size_t length,size_t *offset,uint32_t *cp) {
    const unsigned char *s=(const unsigned char *)bytes;
    size_t at=*offset; unsigned c,extra,min; uint32_t value;
    if(at>=length) return 0;
    c=s[at++];
    if(c<0x80) { *cp=c; *offset=at; return 1; }
    if(c>=0xc2 && c<=0xdf) { extra=1; value=c&31u; min=0x80; }
    else if(c>=0xe0 && c<=0xef) { extra=2; value=c&15u; min=0x800; }
    else if(c>=0xf0 && c<=0xf4) { extra=3; value=c&7u; min=0x10000; }
    else return 0;
    if(extra>length-at) return 0;
    while(extra--) {
        c=s[at++]; if((c&0xc0u)!=0x80u) return 0;
        value=(value<<6)|(c&63u);
    }
    if(value<min || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return 0;
    *offset=at; *cp=value; return 1;
}
static int edge_space(uint32_t cp) {
    return (cp>=9 && cp<=13) || (cp>=0x1c && cp<=0x20) || cp==0x85 || cp==0xa0 ||
        cp==0x1680 || (cp>=0x2000 && cp<=0x200a) || cp==0x2028 || cp==0x2029 ||
        cp==0x202f || cp==0x205f || cp==0x3000;
}
static int owning_text(const char *data,size_t capacity,size_t *length,unsigned *representable) {
    size_t end=0,n,at=0; uint32_t first=0,last=0,cp; unsigned safe=1;
    while(end<capacity && data[end]) ++end;
    if(end==capacity) return 0;
    for(n=end;n<capacity;++n) if(data[n]) return 0;
    while(at<end) {
        if(!next_scalar(data,end,&at,&cp)) return 0;
        if(!first) first=cp;
        last=cp;
        if(cp<32 || cp==127) safe=0;
    }
    *length=end; *representable=end && safe && !edge_space(first) && !edge_space(last);
    return 1;
}
static int title_id_valid(const char *bytes,size_t size) {
    size_t n; unsigned char c;
    if(!size || size>64) return 0;
    for(n=0;n<size;++n) {
        c=(unsigned char)bytes[n];
        if((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9')) continue;
        if(!n || (c!='.' && c!='_' && c!='-')) return 0;
    }
    return 1;
}
static int same_title_id(const text_view *a,const text_view *b) {
    size_t n;
    if(a->length!=b->length) return 0;
    for(n=0;n<a->length;++n) {
        unsigned x=(unsigned char)a->bytes[n],y=(unsigned char)b->bytes[n];
        if(x>='A' && x<='Z') x+='a'-'A';
        if(y>='A' && y<='Z') y+='a'-'A';
        if(x!=y) return 0;
    }
    return 1;
}
static int lower_hex(const char *s,size_t width) {
    size_t n;
    for(n=0;n<width;++n) if(!((s[n]>='0' && s[n]<='9') || (s[n]>='a' && s[n]<='f'))) return 0;
    return s[width]==0;
}
static int validate_record(const mb_title_metadata_result_v1 *r,row_view *out) {
    size_t id,name,version,locale,sha;
    unsigned id_safe,name_safe,version_safe,locale_safe,sha_safe;
    if(r->struct_size!=sizeof(*r) || r->abi_version!=1 || r->status!=MB_TITLE_METADATA_OK ||
       r->reason!=MB_TITLE_METADATA_REASON_NONE || !r->captured_bytes ||
       r->captured_bytes>MB_TITLE_METADATA_INPUT_LIMIT ||
       r->source_scope!=MB_TITLE_SOURCE_METADATA_CAPTURE ||
       r->metadata_identity_scope!=MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256 ||
       r->metadata_complete!=1 || r->title_id_available!=1 || r->name_available>1 ||
       r->version_available>1 || r->locale_available>1 || r->metadata_sha256_available!=1 ||
       r->installed_observed || r->running_observed || r->build_fingerprint_available ||
       r->engine_acceptance_verified || r->runtime_supported || r->installation_supported ||
       r->authenticity_verified || r->game_acceptance_verified || r->reserved[0] || r->reserved[1]) return 0;
    if(!owning_text(r->title_id,sizeof(r->title_id),&id,&id_safe) ||
       !owning_text(r->name,sizeof(r->name),&name,&name_safe) ||
       !owning_text(r->version,sizeof(r->version),&version,&version_safe) ||
       !owning_text(r->locale,sizeof(r->locale),&locale,&locale_safe) ||
       !owning_text(r->metadata_sha256,sizeof(r->metadata_sha256),&sha,&sha_safe)) return 0;
    (void)locale_safe; (void)sha_safe;
    if(!id_safe || !title_id_valid(r->title_id,id) || sha!=64 || !lower_hex(r->metadata_sha256,64) ||
       (!r->name_available && name) || (!r->version_available && version) ||
       (!r->locale_available && locale)) return 0;
    out->id=(text_view){r->title_id,id,1};
    out->name=(text_view){r->name,name,!name?0u:name_safe?1u:2u};
    out->version=(text_view){r->version,version,!version?0u:version_safe?1u:2u};
    return 1;
}
static mb_title_pages_status validate(const mb_title_pages_view_v1 *v,checked_view *out,unsigned *why) {
    const mb_title_catalog_result_v1 *c=v->catalog;
    unsigned n,j;
    *why=MB_TITLE_PAGES_CATALOG_INVALID;
    if(c->struct_size!=sizeof(*c) || c->abi_version!=1 || c->reserved[0] || c->reserved[1] ||
       c->status>MB_TITLE_CATALOG_STATUS_ERROR ||
       (c->profile!=MB_TITLE_CACHE_SYSTEM_PRIVATE && c->profile!=MB_TITLE_CACHE_USER) ||
       c->source_scope!=c->profile || c->complete>1 || c->root_identity_checked>1 ||
       c->enumeration_eof>1 || c->cleanup_complete>1 ||
       c->record_count>v->records_capacity || c->issue_count>v->issues_capacity ||
       c->entries_seen>MB_TITLE_CATALOG_MAX_ENTRIES || c->candidates_seen>MB_TITLE_CATALOG_MAX_CANDIDATES ||
       c->record_count>c->candidates_seen || c->candidates_seen>c->entries_seen ||
       c->operations_reserved>MB_TITLE_CATALOG_OPERATION_LIMIT ||
       c->operations_attempted>c->operations_reserved ||
       c->requested_read_bytes>MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT ||
       c->actual_read_bytes>c->requested_read_bytes ||
       c->entry_name_bytes>MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT ||
       c->entry_name_bytes>MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT-c->requested_read_bytes) return MB_TITLE_PAGES_BAD_CATALOG;
    if(c->status==MB_TITLE_CATALOG_STATUS_OBSERVED) {
        if(c->complete!=1 || c->reason!=MB_TITLE_CATALOG_REASON_NONE ||
           c->root_identity_checked!=1 || c->enumeration_eof!=1 || c->cleanup_complete!=1 ||
           c->record_count!=c->candidates_seen) return MB_TITLE_PAGES_BAD_CATALOG;
    } else if(c->complete || c->reason<1 || c->reason>6 ||
              (c->status!=MB_TITLE_CATALOG_STATUS_PARTIAL && c->record_count)) return MB_TITLE_PAGES_BAD_CATALOG;
    out->catalog=c; out->issues=v->issues;
    out->source=c->profile==MB_TITLE_CACHE_SYSTEM_PRIVATE?
        "system_private_metadata_cache":"user_metadata_cache";
    for(n=0;n<c->record_count;++n) {
        *why=MB_TITLE_PAGES_RECORD_INVALID;
        if(!validate_record(&v->records[n],&out->rows[n])) return MB_TITLE_PAGES_BAD_CATALOG;
        for(j=0;j<n;++j) if(same_title_id(&out->rows[j].id,&out->rows[n].id)) {
            *why=MB_TITLE_PAGES_DUPLICATE_TITLE; return MB_TITLE_PAGES_BAD_CATALOG;
        }
    }
    for(n=0;n<c->issue_count;++n) if(v->issues[n]<1 || v->issues[n]>6) {
        *why=MB_TITLE_PAGES_ISSUE_INVALID; return MB_TITLE_PAGES_BAD_CATALOG;
    }
    *why=MB_TITLE_PAGES_REASON_NONE; return MB_TITLE_PAGES_OK;
}

static void emit(encoder *e,const char *bytes,size_t size) {
    if(e->failed) return;
    if(size>e->limit-e->size) { e->failed=1; return; }
    if(e->bytes) memcpy(e->bytes+e->size,bytes,size);
    e->size+=size;
}
#define LIT(e,text) emit((e),(text),sizeof(text)-1u)
static void literal(encoder *e,const char *s) { emit(e,s,strlen(s)); }
static void number(encoder *e,unsigned value) {
    char digits[10]; unsigned end=10,at;
    do { digits[--end]=(char)('0'+value%10); value/=10; } while(value);
    at=10-end; emit(e,digits+end,at);
}
static void hex_escape(encoder *e,uint32_t value) {
    static const char hex[]="0123456789abcdef";
    char bytes[6]={'\\','u',hex[(value>>12)&15],hex[(value>>8)&15],hex[(value>>4)&15],hex[value&15]};
    emit(e,bytes,6);
}
static void string(encoder *e,const char *bytes,size_t size) {
    size_t at=0; uint32_t cp;
    LIT(e,"\"");
    while(at<size && !e->failed) {
        if(!next_scalar(bytes,size,&at,&cp)) { e->failed=1; return; }
        if(cp=='"') LIT(e,"\\\"");
        else if(cp=='\\') LIT(e,"\\\\");
        else if(cp=='\b') LIT(e,"\\b");
        else if(cp=='\f') LIT(e,"\\f");
        else if(cp=='\n') LIT(e,"\\n");
        else if(cp=='\r') LIT(e,"\\r");
        else if(cp=='\t') LIT(e,"\\t");
        else if(cp<32 || cp>=127) {
            if(cp<0x10000) hex_escape(e,cp);
            else { cp-=0x10000; hex_escape(e,0xd800+(cp>>10)); hex_escape(e,0xdc00+(cp&1023)); }
        } else { char c=(char)cp; emit(e,&c,1); }
    }
    LIT(e,"\"");
}
static void cstring(encoder *e,const char *s) { string(e,s,strlen(s)); }
static void nullable_token(encoder *e,const char *token) {
    if(token) string(e,token,32); else LIT(e,"null");
}
static void metadata(encoder *e,const text_view *v,const char *source) {
    if(v->state==1) {
        LIT(e,"{\"source\":"); cstring(e,source);
        LIT(e,",\"status\":\"observed\",\"value\":"); string(e,v->bytes,v->length); LIT(e,"}");
    } else if(v->state==0) LIT(e,"{\"source\":null,\"status\":\"unavailable\",\"value\":null}");
    else LIT(e,"{\"source\":null,\"status\":\"error\",\"value\":null}");
}
static void item(encoder *e,const row_view *v,const char *source,const char *key) {
    LIT(e,"{\"build_fingerprint\":null,\"build_version\":"); metadata(e,&v->version,source);
    LIT(e,",\"display_name\":"); metadata(e,&v->name,source);
    LIT(e,",\"title_id\":"); metadata(e,&v->id,source);
    LIT(e,",\"title_key\":"); string(e,key,32); LIT(e,"}");
}
static void items(encoder *e,const checked_view *v,unsigned start,unsigned count,
                  const mb_title_pages_tokens_v1 *tokens) {
    unsigned n;
    LIT(e,"[");
    for(n=0;n<count;++n) {
        if(n) LIT(e,",");
        item(e,&v->rows[start+n],v->source,tokens?tokens->title_keys[start+n]:placeholder);
    }
    LIT(e,"]");
}
static void snapshot(encoder *e,const checked_view *v,unsigned first,const char *generation) {
    const mb_title_catalog_result_v1 *c=v->catalog;
    unsigned n;
    LIT(e,"{\"complete\":"); literal(e,c->complete?"true":"false");
    LIT(e,",\"issues\":[");
    if(first) for(n=0;n<c->issue_count;++n) {
        if(n) LIT(e,","); LIT(e,"{\"code\":"); cstring(e,reasons[v->issues[n]]); LIT(e,"}");
    }
    LIT(e,"],\"reason\":");
    if(c->reason) cstring(e,reasons[c->reason]); else LIT(e,"null");
    LIT(e,",\"snapshot_id\":"); nullable_token(e,generation);
    LIT(e,",\"source_scope\":"); cstring(e,v->source);
    LIT(e,",\"status\":"); cstring(e,statuses[c->status]); LIT(e,"}");
}
static void page(encoder *e,const checked_view *v,unsigned index,unsigned page_count,
                 unsigned start,unsigned count,const mb_title_pages_tokens_v1 *tokens) {
    unsigned observed=v->catalog->status==MB_TITLE_CATALOG_STATUS_OBSERVED;
    const char *generation=observed?(tokens?tokens->generation:placeholder):NULL;
    LIT(e,"{\"items\":"); items(e,v,start,observed?count:0,tokens);
    LIT(e,",\"kind\":\"title_inventory_page\",\"page\":");
    if(!observed) LIT(e,"null");
    else {
        LIT(e,"{\"index\":"); number(e,index);
        LIT(e,",\"item_offset\":"); number(e,start);
        LIT(e,",\"next_cursor\":");
        nullable_token(e,start+count==v->catalog->record_count?NULL:
                       tokens?tokens->cursors[index]:placeholder);
        LIT(e,",\"page_count\":"); number(e,page_count);
        LIT(e,",\"total_count\":"); number(e,v->catalog->record_count); LIT(e,"}");
    }
    LIT(e,",\"schema_version\":2,\"snapshot\":"); snapshot(e,v,index==0,generation); LIT(e,"}");
}
static void capture(encoder *e,const checked_view *v) {
    const char *generation=v->catalog->status==MB_TITLE_CATALOG_STATUS_OBSERVED?placeholder:NULL;
    LIT(e,"{\"items\":"); items(e,v,0,v->catalog->record_count,NULL);
    LIT(e,",\"kind\":\"title_inventory_capture\",\"schema_version\":2,\"snapshot\":");
    snapshot(e,v,1,generation); LIT(e,"}");
}
static void current(encoder *e,const char *generation) {
    LIT(e,"{\"current\":true,\"kind\":\"title_inventory_current\",\"schema_version\":2,\"snapshot_id\":");
    string(e,generation,32); LIT(e,"}");
}
static int partition(const checked_view *v,unsigned assumed,mb_title_pages_plan_v1 *p) {
    unsigned at=0,index=0,total=v->catalog->record_count;
    if(!total) { p->page_count=1; p->pages[0].item_offset=0; p->pages[0].item_count=0; return 1; }
    while(at<total && index<MB_TITLE_PAGES_PAGE_LIMIT) {
        unsigned count=0;
        while(at+count<total && count<MB_TITLE_PAGES_ITEMS_PER_PAGE) {
            encoder e={NULL,0,MB_TITLE_PAGES_WIRE_LIMIT,0};
            page(&e,v,index,assumed,at,count+1,NULL);
            if(e.failed) break;
            ++count;
        }
        if(!count) return 0;
        p->pages[index].item_offset=at; p->pages[index].item_count=count;
        at+=count; ++index;
    }
    p->page_count=index; return at==total;
}
static mb_title_pages_status measure(const checked_view *v,mb_title_pages_plan_v1 *p) {
    encoder logical={NULL,0,MB_TITLE_PAGES_CAPTURE_LIMIT,0};
    unsigned assumed=1,iteration,n; size_t offset=0;
    capture(&logical,v);
    if(logical.failed) return fail_plan(p,MB_TITLE_PAGES_RESOURCE_LIMIT,MB_TITLE_PAGES_CAPTURE_TOO_LARGE);
    p->logical_capture_bytes=logical.size;
    p->source_scope=v->catalog->source_scope; p->issue_count=v->catalog->issue_count;
    p->observed_complete=v->catalog->status==MB_TITLE_CATALOG_STATUS_OBSERVED;
    p->record_count=p->observed_complete?v->catalog->record_count:0;
    if(p->observed_complete) {
        for(iteration=0;iteration<5;++iteration) {
            if(!partition(v,assumed,p)) return fail_plan(p,MB_TITLE_PAGES_RESOURCE_LIMIT,MB_TITLE_PAGES_PAGE_TOO_LARGE);
            if(p->page_count==assumed) break;
            assumed=p->page_count;
        }
        if(iteration==5) return fail_plan(p,MB_TITLE_PAGES_RESOURCE_LIMIT,MB_TITLE_PAGES_PAGE_TOO_LARGE);
    } else p->page_count=1;
    for(n=0;n<p->page_count;++n) {
        encoder e={NULL,0,MB_TITLE_PAGES_WIRE_LIMIT,0};
        page(&e,v,n,p->page_count,p->pages[n].item_offset,p->pages[n].item_count,NULL);
        if(e.failed) return fail_plan(p,MB_TITLE_PAGES_RESOURCE_LIMIT,MB_TITLE_PAGES_PAGE_TOO_LARGE);
        p->pages[n].byte_offset=(uint32_t)offset; p->pages[n].byte_count=(uint32_t)e.size;
        offset+=e.size;
    }
    if(p->observed_complete) {
        encoder e={NULL,0,MB_TITLE_PAGES_CURRENT_LIMIT,0}; current(&e,placeholder);
        if(e.failed) return fail_plan(p,MB_TITLE_PAGES_INTERNAL_ERROR,MB_TITLE_PAGES_INTERNAL_FAILURE);
        p->current_offset=(uint32_t)offset; p->current_bytes=(uint32_t)e.size; offset+=e.size;
    }
    if(offset>MB_TITLE_PAGES_BACKING_LIMIT) return fail_plan(p,MB_TITLE_PAGES_RESOURCE_LIMIT,MB_TITLE_PAGES_BACKING_TOO_SMALL);
    p->required_backing_bytes=offset; p->valid=1; return MB_TITLE_PAGES_OK;
}
static mb_title_pages_status validate_tokens(const mb_title_pages_tokens_v1 *t,
                                             const mb_title_pages_plan_v1 *p,unsigned *why) {
    const char *all[1+MB_TITLE_PAGES_RECORD_LIMIT+MB_TITLE_PAGES_PAGE_LIMIT-1];
    unsigned n,j,count=0;
    *why=MB_TITLE_PAGES_TOKEN_INVALID;
    if(!p->observed_complete) return t?MB_TITLE_PAGES_BAD_TOKEN:MB_TITLE_PAGES_OK;
    if(!t || t->title_count!=p->record_count || t->cursor_count!=p->page_count-1) return MB_TITLE_PAGES_BAD_TOKEN;
    all[count++]=t->generation;
    for(n=0;n<t->title_count;++n) all[count++]=t->title_keys[n];
    for(n=0;n<t->cursor_count;++n) all[count++]=t->cursors[n];
    for(n=0;n<count;++n) {
        if(!lower_hex(all[n],32)) return MB_TITLE_PAGES_BAD_TOKEN;
        for(j=0;j<n;++j) if(!memcmp(all[n],all[j],33)) {
            *why=MB_TITLE_PAGES_TOKEN_DUPLICATE; return MB_TITLE_PAGES_BAD_TOKEN;
        }
    }
    *why=MB_TITLE_PAGES_REASON_NONE; return MB_TITLE_PAGES_OK;
}

mb_title_pages_status mb_title_pages_measure_v1(const mb_title_pages_view_v1 *v,mb_title_pages_plan_v1 *p) {
    span spans[5]; unsigned count=0,why; checked_view checked;
    mb_title_pages_status status;
    if(!preflight_view(v,spans,&count) ||
       !add_span(spans,&count,p,sizeof(*p),_Alignof(mb_title_pages_plan_v1))) return MB_TITLE_PAGES_BAD_ARGUMENT;
    initialize_plan(p,MB_TITLE_PAGES_OK,MB_TITLE_PAGES_REASON_NONE);
    status=validate(v,&checked,&why);
    if(status!=MB_TITLE_PAGES_OK) return fail_plan(p,status,why);
    return measure(&checked,p);
}
mb_title_pages_status mb_title_pages_prepare_v1(const mb_title_pages_view_v1 *v,
    const mb_title_pages_tokens_v1 *t,uint8_t *b,size_t bytes,mb_title_pages_cache_v1 *c) {
    span spans[9]; unsigned count=0,why,n; checked_view checked;
    mb_title_pages_plan_v1 p; mb_title_pages_status status;
    if(bytes>MB_TITLE_PAGES_BACKING_LIMIT || !preflight_view(v,spans,&count) ||
       !preflight_tokens(t,spans,&count) || !add_span(spans,&count,b,bytes,1) ||
       !add_span(spans,&count,c,sizeof(*c),_Alignof(mb_title_pages_cache_v1))) return MB_TITLE_PAGES_BAD_ARGUMENT;
    fail_cache(c,b,bytes,MB_TITLE_PAGES_OK,MB_TITLE_PAGES_REASON_NONE);
    initialize_plan(&p,MB_TITLE_PAGES_OK,MB_TITLE_PAGES_REASON_NONE);
    status=validate(v,&checked,&why);
    if(status!=MB_TITLE_PAGES_OK) return fail_cache(c,b,bytes,status,why);
    status=measure(&checked,&p);
    if(status!=MB_TITLE_PAGES_OK) return fail_cache(c,b,bytes,status,p.reason);
    status=validate_tokens(t,&p,&why);
    if(status!=MB_TITLE_PAGES_OK) return fail_cache(c,b,bytes,status,why);
    if(p.required_backing_bytes>bytes)
        return fail_cache(c,b,bytes,MB_TITLE_PAGES_RESOURCE_LIMIT,MB_TITLE_PAGES_BACKING_TOO_SMALL);
    for(n=0;n<p.page_count;++n) {
        mb_title_pages_descriptor_v1 *d=&p.pages[n];
        encoder e={b+d->byte_offset,0,d->byte_count,0};
        page(&e,&checked,n,p.page_count,d->item_offset,d->item_count,t);
        if(e.failed || e.size!=d->byte_count)
            return fail_cache(c,b,bytes,MB_TITLE_PAGES_INTERNAL_ERROR,MB_TITLE_PAGES_INTERNAL_FAILURE);
        if(n && t) memcpy(d->cursor,t->cursors[n-1],33);
    }
    if(p.observed_complete) {
        encoder e={b+p.current_offset,0,p.current_bytes,0}; current(&e,t->generation);
        if(e.failed || e.size!=p.current_bytes)
            return fail_cache(c,b,bytes,MB_TITLE_PAGES_INTERNAL_ERROR,MB_TITLE_PAGES_INTERNAL_FAILURE);
        memcpy(c->snapshot_id,t->generation,33);
    }
    memcpy(&c->plan,&p,sizeof(p)); c->prepared=1;
    return MB_TITLE_PAGES_OK;
}
