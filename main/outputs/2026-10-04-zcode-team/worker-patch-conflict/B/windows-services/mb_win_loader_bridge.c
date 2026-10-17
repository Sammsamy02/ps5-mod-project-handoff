#include "mb_win_loader_bridge.h"
#if !defined(__x86_64__)
#error "Windows loader bridge requires an AMD64 target"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void *)==8 && sizeof(uint32_t)==4 && sizeof(int32_t)==4,
               "Windows loader bridge requires 64-bit pointers and 32-bit DWORD/BOOL");
static mb_win_loader_bridge_config installed;
static mb_win_loader_bridge_session *sessions;
static mb_win_loader_bridge_session **find_session(mb_win_loader_bridge_session *session){
    mb_win_loader_bridge_session **cursor=&sessions;while(*cursor && *cursor!=session)cursor=&(*cursor)->next;return cursor;
}
static int valid_session(mb_win_loader_bridge_session *session){
    return session && *find_session(session) && session->active &&
        (session->module->state==MB_PE_MODULE_LOADING || session->module->state==MB_PE_MODULE_ACTIVE);
}
static int same_config(const mb_win_loader_bridge_config *c){
    return installed.loader==c->loader && installed.domain==c->domain && installed.context==c->context &&
        installed.lock==c->lock && installed.unlock==c->unlock;
}
mb_win_loader_bridge_status mb_win_loader_bridge_begin(mb_win_loader_bridge_session *s,
    const mb_win_loader_bridge_config *c,mb_pe_module *module){
    if(!s || !c || !c->loader || !c->domain || !c->lock || !c->unlock || !module)return MB_WIN_LOADER_BRIDGE_INVALID_ARGUMENT;
    if(s->active || s->loader_pin.owner || s->loader_pin.next || s->domain_pin.owner || s->domain_pin.next ||
       s->module || s->next || module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_LOADER_BRIDGE_BAD_STATE;
    for(mb_win_loader_bridge_session *p=sessions;p;p=p->next)if(p==s || p->module==module)return MB_WIN_LOADER_BRIDGE_BAD_STATE;
    if(sessions && !same_config(c))return MB_WIN_LOADER_BRIDGE_BUSY;
    if(mb_pe_loader_acquire(c->loader,&s->loader_pin)!=MB_PE_LOADER_OK)return MB_WIN_LOADER_BRIDGE_OWNER_ERROR;
    if(mb_win_threads_acquire(c->domain,&s->domain_pin)!=MB_WIN_THREADS_OK){
        /* Fresh original loader pin, no callback/reentry between acquire/release. */
        (void)mb_pe_loader_release_pin(&s->loader_pin);return MB_WIN_LOADER_BRIDGE_OWNER_ERROR;
    }
    if(!sessions)installed=*c;
    s->module=module;s->next=sessions;s->active=1;sessions=s;return MB_WIN_LOADER_BRIDGE_OK;
}
mb_win_loader_bridge_status mb_win_loader_bridge_end(mb_win_loader_bridge_session *s){
    if(!s)return MB_WIN_LOADER_BRIDGE_INVALID_ARGUMENT;
    mb_win_loader_bridge_session **cursor=find_session(s);
    if(!*cursor || !s->active)return MB_WIN_LOADER_BRIDGE_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_LOADER_BRIDGE_BUSY;
    if(s->domain_pin.owner && mb_win_threads_release_pin(&s->domain_pin)!=MB_WIN_THREADS_OK)return MB_WIN_LOADER_BRIDGE_OWNER_ERROR;
    if(s->loader_pin.owner && mb_pe_loader_release_pin(&s->loader_pin)!=MB_PE_LOADER_OK)return MB_WIN_LOADER_BRIDGE_OWNER_ERROR;
    *cursor=s->next;*s=(mb_win_loader_bridge_session){0};
    if(!sessions)installed=(mb_win_loader_bridge_config){0};return MB_WIN_LOADER_BRIDGE_OK;
}
static void set_error(uint32_t value){*mb_win_threads_last_error(installed.domain)=value;}
static uint32_t windows_error(mb_pe_loader_status status,int procedure){
    switch(status){
    case MB_PE_LOADER_NOT_FOUND:return procedure?127:126;
    case MB_PE_LOADER_SOURCE_ERROR:return 126;
    case MB_PE_LOADER_INVALID_ARGUMENT:return 87;
    case MB_PE_LOADER_UNSUPPORTED:return 50;
    case MB_PE_LOADER_LIMIT:return 8;
    case MB_PE_LOADER_BUSY:case MB_PE_LOADER_CLEANUP_REQUIRED:return 170;
    default:return 1114;
    }
}
static uint32_t ascii_name(const char *input,char *buffer,mb_pe_string *view){
    if(!input)return 87;
    for(size_t i=0;i<=MB_WIN_LOADER_NAME_LIMIT;++i){
        unsigned char c=(unsigned char)input[i];if(c>127)return 50;buffer[i]=(char)c;
        if(!c){if(!i)return 87;*view=(mb_pe_string){buffer,i};return 0;}
    }
    return 87;
}
static uint32_t wide_name(const uint16_t *input,char *buffer,mb_pe_string *view){
    if(!input)return 87;
    for(size_t i=0;i<=MB_WIN_LOADER_NAME_LIMIT;++i){
        uint16_t c=input[i];if(c>127)return 50;buffer[i]=(char)c;
        if(!c){if(!i)return 87;*view=(mb_pe_string){buffer,i};return 0;}
    }
    return 87;
}
static void *load_or_find(mb_pe_string name,int find){
    uint32_t saved=*mb_win_threads_last_error(installed.domain);
    void *handle=NULL;mb_pe_loader_error error;
    installed.lock(installed.context);
    mb_pe_loader_status status=find?mb_pe_loader_find(installed.loader,name,&handle,&error):
        mb_pe_loader_load(installed.loader,name,&handle,&error);
    installed.unlock(installed.context);
    if(status!=MB_PE_LOADER_OK){set_error(windows_error(status,0));return NULL;}set_error(saved);return handle;
}
static WIN64 void *loader_load_a(const char *name){
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view;uint32_t error=ascii_name(name,buffer,&view);
    if(error){set_error(error);return NULL;}return load_or_find(view,0);
}
static WIN64 void *loader_load_w(const uint16_t *name){
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view;uint32_t error=wide_name(name,buffer,&view);
    if(error){set_error(error);return NULL;}return load_or_find(view,0);
}
static WIN64 void *loader_load_ex_a(const char *name,void *file,uint32_t flags){
    if(file || flags){set_error(50);return NULL;}
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view;uint32_t error=ascii_name(name,buffer,&view);
    if(error){set_error(error);return NULL;}return load_or_find(view,0);
}
static WIN64 void *loader_load_ex_w(const uint16_t *name,void *file,uint32_t flags){
    if(file || flags){set_error(50);return NULL;}
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view;uint32_t error=wide_name(name,buffer,&view);
    if(error){set_error(error);return NULL;}return load_or_find(view,0);
}
static WIN64 void *loader_find_a(const char *name){
    if(!name){set_error(50);return NULL;}
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view;uint32_t error=ascii_name(name,buffer,&view);
    if(error){set_error(error);return NULL;}return load_or_find(view,1);
}
static WIN64 void *loader_find_w(const uint16_t *name){
    if(!name){set_error(50);return NULL;}
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view;uint32_t error=wide_name(name,buffer,&view);
    if(error){set_error(error);return NULL;}return load_or_find(view,1);
}
static int known_handle(void *handle){
    if(!handle)return 0;
    for(size_t i=0;i<installed.loader->config.catalog_count;++i){
        const mb_pe_loader_record *r=&installed.loader->config.records[i];
        if(r->state!=MB_PE_LOADER_EMPTY && r->handle==handle)return 1;
    }
    return 0;
}
static WIN64 void *loader_proc(void *handle,const char *name){
    uint32_t saved=*mb_win_threads_last_error(installed.domain);
    char buffer[MB_WIN_LOADER_NAME_LIMIT+1];mb_pe_string view={0};uintptr_t raw=(uintptr_t)name;
    uint32_t conversion=0;
    if(!raw)conversion=87;
    else if(raw>UINT16_MAX)conversion=ascii_name(name,buffer,&view);
    if(conversion){set_error(conversion);return NULL;}
    void *address=NULL;mb_pe_loader_error error;
    installed.lock(installed.context);
    if(!known_handle(handle)){installed.unlock(installed.context);set_error(6);return NULL;}
    mb_pe_loader_status status=raw<=UINT16_MAX?
        mb_pe_loader_proc_ordinal(installed.loader,handle,(uint32_t)raw,&address,&error):
        mb_pe_loader_proc_name(installed.loader,handle,view,&address,&error);
    installed.unlock(installed.context);
    if(status!=MB_PE_LOADER_OK){set_error(windows_error(status,1));return NULL;}set_error(saved);return address;
}
static WIN64 int32_t loader_free(void *handle){
    uint32_t saved=*mb_win_threads_last_error(installed.domain);
    mb_pe_loader_error error;installed.lock(installed.context);
    if(!known_handle(handle)){installed.unlock(installed.context);set_error(6);return 0;}
    mb_pe_loader_status status=mb_pe_loader_free(installed.loader,handle,&error);
    installed.unlock(installed.context);
    if(status!=MB_PE_LOADER_OK){set_error(windows_error(status,0));return 0;}set_error(saved);return 1;
}
static int match(mb_pe_string view,const char *literal,size_t size,int fold){
    if(view.size!=size)return 0;for(size_t i=0;i<size;++i){unsigned char c=(unsigned char)view.data[i];
        if(fold && c>='A' && c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)literal[i])return 0;}return 1;
}
#define MATCH(v,s,fold) match((v),(s),sizeof(s)-1,(fold))
#define BIND(s,fn) if(MATCH(import->name,s,0)){*value=(uint64_t)(uintptr_t)&fn;return MB_PE_BIND_BOUND;}
mb_pe_bind_result mb_win_loader_bridge_bind(void *context,const mb_pe_import *import,uint64_t *value){
    if(!import || !value || (!import->library.data && import->library.size) || (!import->name.data && import->name.size) ||
       !valid_session(context))return MB_PE_BIND_ERROR;
    if(import->by_ordinal || !MATCH(import->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    BIND("LoadLibraryA",loader_load_a) BIND("LoadLibraryW",loader_load_w)
    BIND("LoadLibraryExA",loader_load_ex_a) BIND("LoadLibraryExW",loader_load_ex_w)
    BIND("GetModuleHandleA",loader_find_a) BIND("GetModuleHandleW",loader_find_w)
    BIND("GetProcAddress",loader_proc) BIND("FreeLibrary",loader_free)
    return MB_PE_BIND_UNRESOLVED;
}
mb_pe_bind_result mb_win_loader_runtime_bind(void *opaque,const mb_pe_import *import,uint64_t *value){
    mb_win_loader_runtime_context *context=opaque;
    if(!context || !valid_session(context->loader) || !context->runtime || !context->runtime->tls ||
       context->loader->module!=context->runtime->tls->module)return MB_PE_BIND_ERROR;
    mb_pe_bind_result result=mb_win_runtime_bind(context->runtime,import,value);
    return result==MB_PE_BIND_UNRESOLVED?mb_win_loader_bridge_bind(context->loader,import,value):result;
}
