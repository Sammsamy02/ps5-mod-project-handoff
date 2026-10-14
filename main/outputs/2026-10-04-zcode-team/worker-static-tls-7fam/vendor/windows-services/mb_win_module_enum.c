#include "mb_win_module_enum.h"
#if !defined(__x86_64__)
#error "Windows module enumeration requires an AMD64 target"
#endif
_Static_assert(sizeof(void *)==8 && sizeof(uint32_t)==4 && sizeof(int32_t)==4,
               "Windows module enumeration requires 64-bit pointers, 32-bit DWORD/BOOL");

mb_win_module_enum_status mb_win_module_enum_acquire_pin(mb_win_module_enum *o,mb_win_module_enum_pin *pin){
    if(!o || !pin)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(pin->owner || pin->next)return MB_WIN_MODULE_ENUM_BAD_STATE;
    pin->owner=o;return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_release_pin(mb_win_module_enum_pin *pin){
    if(!pin || !pin->owner)return MB_WIN_MODULE_ENUM_BAD_STATE;
    pin->owner=(mb_win_module_enum *)0;return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_begin(mb_win_module_enum_session *s,
    mb_win_module_enum *o,struct mb_pe_module *m){
    if(!s || !o || !m)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(s->active || s->pin.owner || s->pin.next || s->module || s->next ||
       m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_MODULE_ENUM_BAD_STATE;
    if(o->active)return MB_WIN_MODULE_ENUM_BUSY;
    mb_win_module_enum_status st=mb_win_module_enum_acquire_pin(o,&s->pin);
    if(st)return st;
    o->active=1;s->module=m;s->active=1;
    return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_end(mb_win_module_enum_session *s){
    if(!s)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(!s->active || !s->pin.owner)return MB_WIN_MODULE_ENUM_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_MODULE_ENUM_BUSY;
    mb_win_module_enum *owner=s->pin.owner;
    mb_win_module_enum_status st=mb_win_module_enum_release_pin(&s->pin);
    if(st)return st;
    owner->active=0;
    s->pin.owner=(mb_win_module_enum *)0;s->pin.next=(mb_win_module_enum_pin *)0;
    s->module=(struct mb_pe_module *)0;s->next=(mb_win_module_enum_session *)0;s->active=0;
    return MB_WIN_MODULE_ENUM_OK;
}

static char fold(char c){return (c>='A' && c<='Z')?(char)(c+'a'-'A'):c;}
static size_t length(const char *s){size_t n=0;while(s && s[n])++n;return n;}
static int same_folded(const char *a,const char *b,size_t n){
    for(size_t i=0;i<n;++i)if(fold(a[i])!=fold(b[i]))return 0;return 1;
}
/* pe_loader key policy: no dot gains ".dll"; one trailing dot suppresses. */
static int key_matches(const mb_win_module_record *r,const char *query,size_t qlen){
    if(!qlen || qlen>MB_WIN_MODULE_NAME_LIMIT)return 0;
    if(same_folded(r->name,query,qlen) && r->name[qlen]==0)return 1;
    if(query[qlen-1]=='.')return 0; /* exact-only after suppression dot */
    if(qlen+4<=MB_WIN_MODULE_NAME_LIMIT){
        char keyed[MB_WIN_MODULE_NAME_LIMIT+1];
        for(size_t i=0;i<qlen;++i)keyed[i]=query[i];
        keyed[qlen]='.';keyed[qlen+1]='d';keyed[qlen+2]='l';keyed[qlen+3]='l';
        if(same_folded(r->name,keyed,qlen+4) && r->name[qlen+4]==0)return 1;
    }
    return 0;
}
static mb_win_module_record *by_base(mb_win_module_enum *o,void *base){
    for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES;++i){
        mb_win_module_record *r=&o->modules[i];
        if(r->registered && r->base==base)return r;
    }
    return (mb_win_module_record *)0;
}
static mb_win_module_record *by_name(mb_win_module_enum *o,const char *name){
    size_t qlen=length(name);
    for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES;++i){
        mb_win_module_record *r=&o->modules[i];
        if(r->registered && key_matches(r,name,qlen))return r;
    }
    return (mb_win_module_record *)0;
}
static mb_win_module_record *exe_record(mb_win_module_enum *o){
    for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES;++i){
        mb_win_module_record *r=&o->modules[i];
        if(r->registered && r->executable_image)return r;
    }
    return (mb_win_module_record *)0;
}
static mb_win_module_record *resolve_handle(mb_win_module_enum *o,void *handle){
    return handle?by_base(o,handle):exe_record(o);
}
static size_t copy_ascii(char *dst,const char *src,size_t room){
    size_t n=0;while(src[n] && n+1<room){dst[n]=src[n];++n;}
    dst[n]=0;return n;
}

mb_win_module_enum_status mb_win_module_enum_register(mb_win_module_enum *o,
    const mb_win_module_record *entry){
    if(!o || !o->active || !entry || !entry->base || !entry->name[0])
        return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(entry->references || entry->registered)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(length(entry->name)>MB_WIN_MODULE_NAME_LIMIT ||
       length(entry->file_name)>MB_WIN_MODULE_FILE_LIMIT)
        return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(by_base(o,entry->base) || by_name(o,entry->name))return MB_WIN_MODULE_ENUM_BUSY;
    mb_win_module_record *slot=(mb_win_module_record *)0;
    for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES && !slot;++i)
        if(!o->modules[i].registered)slot=&o->modules[i];
    if(!slot)return MB_WIN_MODULE_ENUM_CAPACITY;
    slot->registered=1;slot->executable_image=entry->executable_image;slot->acquired=0;
    for(size_t i=0;i<=MB_WIN_MODULE_NAME_LIMIT;++i)slot->name[i]=entry->name[i];
    for(size_t i=0;i<=MB_WIN_MODULE_FILE_LIMIT;++i)slot->file_name[i]=entry->file_name[i];
    slot->base=entry->base;slot->entry_point=entry->entry_point;slot->source_token=entry->source_token;
    slot->image_size=entry->image_size;slot->references=0;
    slot->exports=entry->exports;slot->export_count=entry->export_count;
    return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_find(mb_win_module_enum *o,
    const char *name,void **base){
    if(!o || !o->active || !base)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=name?by_name(o,name):exe_record(o);
    if(!r)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    *base=r->base;return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_find_ex(mb_win_module_enum *o,
    uint32_t flags,const char *name_or_address,void **base){
    if(!o || !o->active || !base)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(flags & ~(uint32_t)(MB_WIN_MODULE_EX_FLAG_PIN|MB_WIN_MODULE_EX_FLAG_UNCHANGED_REFCOUNT|
        MB_WIN_MODULE_EX_FLAG_FROM_ADDRESS))return MB_WIN_MODULE_ENUM_UNSUPPORTED;
    if(flags & MB_WIN_MODULE_EX_FLAG_FROM_ADDRESS){
        uintptr_t address=(uintptr_t)name_or_address;
        if(!address)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
        for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES;++i){
            mb_win_module_record *r=&o->modules[i];
            if(!r->registered)continue;
            uintptr_t lo=(uintptr_t)r->base,hi=lo+(uintptr_t)r->image_size;
            if(hi>lo && address>=lo && address<hi){*base=r->base;return MB_WIN_MODULE_ENUM_OK;}
        }
        return MB_WIN_MODULE_ENUM_NOT_FOUND;
    }
    return mb_win_module_enum_find(o,name_or_address,base);
}
mb_win_module_enum_status mb_win_module_enum_load(mb_win_module_enum *o,
    const char *name,void **base){
    if(!o || !o->active || !name || !base)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=by_name(o,name);
    if(r){
        if(r->references==0xFFFFFFFFu)return MB_WIN_MODULE_ENUM_CAPACITY;
        ++r->references;*base=r->base;return MB_WIN_MODULE_ENUM_OK;
    }
    if(!o->ops.acquire)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    size_t qlen=length(name);
    if(!qlen || qlen>MB_WIN_MODULE_NAME_LIMIT)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    char keyed[MB_WIN_MODULE_NAME_LIMIT+1];
    if(name[qlen-1]=='.'){for(size_t i=0;i+1<qlen;++i)keyed[i]=name[i];keyed[qlen-1]=0;}
    else{for(size_t i=0;i<qlen;++i)keyed[i]=name[i];keyed[qlen]=0;}
    if(!keyed[0])return MB_WIN_MODULE_ENUM_NOT_FOUND;
    void *token=(void *)0,*acquired=(void *)0;uint32_t size=0;
    if(o->ops.acquire(o->ops.context,keyed,&acquired,&size,&token))
        return MB_WIN_MODULE_ENUM_OWNER_ERROR;
    if(!acquired)return MB_WIN_MODULE_ENUM_OWNER_ERROR;
    mb_win_module_record fresh;
    fresh.registered=0;fresh.executable_image=0;fresh.acquired=0;fresh.entry_point=(void *)0;
    for(size_t i=0;i<=MB_WIN_MODULE_NAME_LIMIT;++i)fresh.name[i]=0;
    for(size_t i=0;i<=MB_WIN_MODULE_FILE_LIMIT;++i)fresh.file_name[i]=0;
    fresh.base=(void *)0;fresh.source_token=(void *)0;fresh.image_size=0;fresh.references=0;
    fresh.exports=(const mb_win_module_export *)0;fresh.export_count=0;
    copy_ascii(fresh.name,keyed,sizeof(fresh.name));
    copy_ascii(fresh.file_name,keyed,sizeof(fresh.file_name));
    fresh.base=acquired;fresh.image_size=size;fresh.source_token=token;
    fresh.acquired=1;
    mb_win_module_enum_status st=mb_win_module_enum_register(o,&fresh);
    if(st){
        if(o->ops.release)(void)o->ops.release(o->ops.context,acquired,token);
        return st;
    }
    mb_win_module_record *stored=by_base(o,acquired);
    if(!stored)return MB_WIN_MODULE_ENUM_BAD_STATE;
    stored->references=1; /* the load itself is the first reference */
    stored->acquired=1;stored->source_token=token; /* register normalizes these */
    *base=acquired;return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_free(mb_win_module_enum *o,void *handle){
    if(!o || !o->active || !handle)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=by_base(o,handle);
    if(!r)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    if(r->references){
        --r->references;
        if(!r->references && r->acquired && o->ops.release){
            if(o->ops.release(o->ops.context,r->base,r->source_token))
                return MB_WIN_MODULE_ENUM_OWNER_ERROR; /* retained for retry */
            r->registered=0;r->acquired=0;r->executable_image=0;
            for(size_t i=0;i<=MB_WIN_MODULE_NAME_LIMIT;++i)r->name[i]=0;
            for(size_t i=0;i<=MB_WIN_MODULE_FILE_LIMIT;++i)r->file_name[i]=0;
            r->base=(void *)0;r->entry_point=(void *)0;r->source_token=(void *)0;
            r->image_size=0;r->references=0;r->exports=(const mb_win_module_export *)0;r->export_count=0;
        }
    }
    return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_proc_name(mb_win_module_enum *o,
    void *handle,const char *symbol,void **address){
    if(!o || !o->active || !symbol || !symbol[0] || !address)
        return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=resolve_handle(o,handle);
    if(!r)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    for(size_t i=0;i<r->export_count;++i){
        const char *candidate=r->exports[i].name;
        if(!candidate)continue;
        size_t n=0;int equal=1;
        while(candidate[n] || symbol[n]){
            if(candidate[n]!=symbol[n]){equal=0;break;}
            ++n;
        }
        if(equal && r->exports[i].address){
            *address=r->exports[i].address;return MB_WIN_MODULE_ENUM_OK;
        }
    }
    return MB_WIN_MODULE_ENUM_NOT_FOUND;
}
mb_win_module_enum_status mb_win_module_enum_proc_ordinal(mb_win_module_enum *o,
    void *handle,uint32_t ordinal,void **address){
    if(!o || !o->active || !address)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(!ordinal)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=resolve_handle(o,handle);
    if(!r)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    if(ordinal>r->export_count)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    void *value=r->exports[ordinal-1].address;
    if(!value)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    *address=value;return MB_WIN_MODULE_ENUM_OK;
}
static mb_win_module_enum_status name_of(mb_win_module_enum *o,void *handle,
    int full,char *buffer,uint32_t size,uint32_t *written){
    if(!o || !o->active || !buffer || !size || !written)
        return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=resolve_handle(o,handle);
    if(!r)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    const char *text=full?r->file_name:r->name;
    size_t total=length(text);
    if(total>=(size_t)size){
        copy_ascii(buffer,text,(size_t)size); /* Windows truncates at nSize */
        *written=size;return MB_WIN_MODULE_ENUM_CAPACITY;
    }
    copy_ascii(buffer,text,(size_t)size);*written=(uint32_t)total;
    return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_file_name(mb_win_module_enum *o,
    void *handle,char *buffer,uint32_t size,uint32_t *written){
    return name_of(o,handle,1,buffer,size,written);
}
mb_win_module_enum_status mb_win_module_enum_base_name(mb_win_module_enum *o,
    void *handle,char *buffer,uint32_t size,uint32_t *written){
    return name_of(o,handle,0,buffer,size,written);
}
mb_win_module_enum_status mb_win_module_enum_list(mb_win_module_enum *o,
    void **bases,uint32_t capacity,uint32_t *needed){
    if(!o || !o->active || !needed)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    uint32_t count=0;
    for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES;++i)
        if(o->modules[i].registered)++count;
    *needed=count;
    if(!bases && count)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(count>capacity)return MB_WIN_MODULE_ENUM_CAPACITY;
    uint32_t out=0;
    for(size_t i=0;i<MB_WIN_MODULE_MAX_MODULES && out<count;++i)
        if(o->modules[i].registered)bases[out++]=o->modules[i].base;
    return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_info(mb_win_module_enum *o,
    void *handle,mb_win_module_info *info){
    if(!o || !o->active || !info)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_record *r=resolve_handle(o,handle);
    if(!r)return MB_WIN_MODULE_ENUM_NOT_FOUND;
    info->base=r->base;info->size_of_image=r->image_size;info->entry_point=r->entry_point;
    return MB_WIN_MODULE_ENUM_OK;
}
