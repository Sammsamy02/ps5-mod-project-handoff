#include "mb_win_module_enum_abi.h"
#if !defined(__x86_64__)
#error "Windows module enumeration ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
#include <stddef.h> /* offsetof only; freestanding compilers provide it */
_Static_assert(sizeof(void *)==8 && sizeof(uint32_t)==4 && sizeof(int32_t)==4,
               "Microsoft x64 widths required");
/* Exact x64 MODULEINFO shape: {LPVOID lpBase; DWORD SizeOfImage; LPVOID EntryPoint}. */
_Static_assert(offsetof(mb_win_module_info,base)==0 &&
               offsetof(mb_win_module_info,size_of_image)==8 &&
               offsetof(mb_win_module_info,entry_point)==16 &&
               sizeof(mb_win_module_info)==24,"MODULEINFO x64 layout");

static mb_win_module_enum *installed;
static mb_win_module_enum_session *abi_sessions;
static uint32_t last_error;
static mb_win_module_enum_session **find_session(mb_win_module_enum_session *s){
    mb_win_module_enum_session **p=&abi_sessions;while(*p && *p!=s)p=&(*p)->next;return p;
}
static int has_sessions(void){return abi_sessions!=(mb_win_module_enum_session *)0;}
mb_win_module_enum_status mb_win_module_enum_abi_begin(mb_win_module_enum_session *s,
    mb_win_module_enum *o,struct mb_pe_module *m){
    if(!s || !o)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    if(installed && installed!=o && has_sessions())return MB_WIN_MODULE_ENUM_BUSY;
    mb_win_module_enum_status st=mb_win_module_enum_begin(s,o,m);
    if(st)return st;
    mb_win_module_enum_session **slot=find_session(s);
    if(*slot){(void)mb_win_module_enum_end(s);return MB_WIN_MODULE_ENUM_BAD_STATE;}
    s->next=abi_sessions;abi_sessions=s;
    if(!installed)installed=o;
    return MB_WIN_MODULE_ENUM_OK;
}
mb_win_module_enum_status mb_win_module_enum_abi_end(mb_win_module_enum_session *s){
    if(!s)return MB_WIN_MODULE_ENUM_INVALID_ARGUMENT;
    mb_win_module_enum_session **p=find_session(s);
    if(!*p || !s->active)return MB_WIN_MODULE_ENUM_BAD_STATE;
    mb_win_module_enum_session *next=s->next; /* core end zeroes the session */
    mb_win_module_enum_status st=mb_win_module_enum_end(s);
    if(st)return st;
    *p=next; /* unlink from the ABI registry after a clean core end */
    if(!abi_sessions)installed=(mb_win_module_enum *)0;
    return MB_WIN_MODULE_ENUM_OK;
}
uint32_t mb_win_module_enum_abi_last_error(void){return last_error;}

/* Wide names fold to ASCII (code units >127 unsupported), matching the loader
 * bridge conversion policy. Pointer thunks return NULL on failure; the mapped
 * Win32 error stays readable through mb_win_module_enum_abi_last_error. */
static uint32_t wide_name(const uint16_t *input,char *buffer,const char **ascii){
    if(!input)return 87;
    for(size_t i=0;i<=MB_WIN_MODULE_NAME_LIMIT;++i){
        uint16_t c=input[i];if(c>127)return 50;buffer[i]=(char)c;
        if(!c){if(!i)return 87;*ascii=buffer;return 0;}
    }
    return 87;
}
static uint32_t map(mb_win_module_enum_status st,int procedure){
    switch(st){
    case MB_WIN_MODULE_ENUM_OK:last_error=0;return 0;
    case MB_WIN_MODULE_ENUM_INVALID_ARGUMENT:return 87;
    case MB_WIN_MODULE_ENUM_NOT_FOUND:return procedure?127:126;
    case MB_WIN_MODULE_ENUM_CAPACITY:return 122;
    case MB_WIN_MODULE_ENUM_UNSUPPORTED:return 50;
    case MB_WIN_MODULE_ENUM_BUSY:return 170;
    default:return 1114;
    }
}
static uint32_t fail(mb_win_module_enum_status st,int procedure){
    last_error=map(st,procedure);return last_error;
}
static int live(void){return installed && installed->active;}
#define GUARD() do{if(!live()){last_error=6;return (void *)0;}}while(0)

static WIN64 void *get_module_handle_a(const char *name){
    void *base=(void *)0;GUARD();
    mb_win_module_enum_status st=mb_win_module_enum_find(installed,name,&base);
    if(st){(void)fail(st,0);return (void *)0;}
    last_error=0;return base;
}
static WIN64 void *get_module_handle_w(const uint16_t *name){
    char buffer[MB_WIN_MODULE_NAME_LIMIT+1];const char *ascii=(const char *)0;GUARD();
    uint32_t conversion=wide_name(name,buffer,&ascii);
    if(conversion){last_error=conversion;return (void *)0;}
    void *base=(void *)0;
    mb_win_module_enum_status st=mb_win_module_enum_find(installed,ascii,&base);
    if(st){(void)fail(st,0);return (void *)0;}
    last_error=0;return base;
}
static WIN64 int32_t get_module_handle_ex_w(uint32_t flags,const uint16_t *name,void **base){
    char buffer[MB_WIN_MODULE_NAME_LIMIT+1];const char *ascii=(const char *)0;
    if(!live()){last_error=6;return 0;}
    if(!base){last_error=87;return 0;}
    if(flags & MB_WIN_MODULE_EX_FLAG_FROM_ADDRESS)ascii=(const char *)name; /* address, not text */
    else{
        uint32_t conversion=wide_name(name,buffer,&ascii);
        if(conversion){last_error=conversion;return 0;}
    }
    mb_win_module_enum_status st=mb_win_module_enum_find_ex(installed,flags,ascii,base);
    if(st){(void)fail(st,0);return 0;}
    last_error=0;return 1;
}
static WIN64 uint32_t get_module_file_name_a(void *module,char *buffer,uint32_t size){
    uint32_t written=0;
    if(!live()){last_error=6;return 0;}
    mb_win_module_enum_status st=mb_win_module_enum_file_name(installed,module,buffer,size,&written);
    if(st==MB_WIN_MODULE_ENUM_OK){last_error=0;return written;}
    if(st==MB_WIN_MODULE_ENUM_CAPACITY){last_error=122;return written;} /* nSize, per Windows */
    {(void)fail(st,0);return 0;}
}
static WIN64 uint32_t get_module_file_name_w(void *module,uint16_t *buffer,uint32_t size){
    char narrow[MB_WIN_MODULE_FILE_LIMIT+1];uint32_t written=0;
    if(!live()){last_error=6;return 0;}
    if(size>MB_WIN_MODULE_FILE_LIMIT)size=MB_WIN_MODULE_FILE_LIMIT+1;
    mb_win_module_enum_status st=mb_win_module_enum_file_name(installed,module,narrow,size,&written);
    if(st==MB_WIN_MODULE_ENUM_OK || st==MB_WIN_MODULE_ENUM_CAPACITY){
        for(uint32_t i=0;i<written && i<size;++i)buffer[i]=(uint16_t)(unsigned char)narrow[i];
        buffer[written<size?written:size-1]=0;
        last_error=(st==MB_WIN_MODULE_ENUM_CAPACITY)?122:0;
        return written;
    }
    {(void)fail(st,0);return 0;}
}
static WIN64 void *get_proc_address(void *module,const char *procedure){
    void *address=(void *)0;uintptr_t raw=(uintptr_t)procedure;GUARD();
    mb_win_module_enum_status st=raw<=0xFFFFu ?
        mb_win_module_enum_proc_ordinal(installed,module,(uint32_t)raw,&address) :
        mb_win_module_enum_proc_name(installed,module,procedure,&address);
    if(st){(void)fail(st,1);return (void *)0;}
    last_error=0;return address;
}
static WIN64 void *load_library_a(const char *name){
    void *base=(void *)0;GUARD();
    mb_win_module_enum_status st=mb_win_module_enum_load(installed,name,&base);
    if(st){(void)fail(st,0);return (void *)0;}
    last_error=0;return base;
}
static WIN64 void *load_library_w(const uint16_t *name){
    char buffer[MB_WIN_MODULE_NAME_LIMIT+1];const char *ascii=(const char *)0;GUARD();
    uint32_t conversion=wide_name(name,buffer,&ascii);
    if(conversion){last_error=conversion;return (void *)0;}
    return load_library_a(ascii);
}
static WIN64 void *load_library_ex_a(const char *name,void *file,uint32_t flags){
    if(file || flags){last_error=50;return (void *)0;}
    return load_library_a(name);
}
static WIN64 void *load_library_ex_w(const uint16_t *name,void *file,uint32_t flags){
    if(file || flags){last_error=50;return (void *)0;}
    return load_library_w(name);
}
static WIN64 int32_t free_library(void *module){
    if(!live()){last_error=6;return 0;}
    mb_win_module_enum_status st=mb_win_module_enum_free(installed,module);
    if(st){(void)fail(st,0);return 0;}
    last_error=0;return 1;
}
static int current_process(void *handle){
    return !handle || handle==(void *)(uintptr_t)-1; /* NULL and pseudo-handle -1 */
}
static WIN64 int32_t k32_enum_process_modules(void *process,void **modules,
    uint32_t bytes,uint32_t *needed_bytes){
    if(!live()){last_error=6;return 0;}
    if(!current_process(process)){last_error=6;return 0;}
    uint32_t capacity=bytes/(uint32_t)sizeof(void *),needed=0;
    mb_win_module_enum_status st=mb_win_module_enum_list(installed,modules,capacity,&needed);
    if(needed_bytes)*needed_bytes=needed*(uint32_t)sizeof(void *);
    if(st){(void)fail(st,0);return 0;}
    last_error=0;return 1;
}
static WIN64 uint32_t k32_get_module_base_name_a(void *process,void *module,
    char *buffer,uint32_t size){
    uint32_t written=0;
    if(!live()){last_error=6;return 0;}
    if(!current_process(process)){last_error=6;return 0;}
    mb_win_module_enum_status st=mb_win_module_enum_base_name(installed,module,buffer,size,&written);
    if(st){(void)fail(st,0);return 0;}
    last_error=0;return written;
}
static WIN64 int32_t k32_get_module_information(void *process,void *module,
    mb_win_module_info *info,uint32_t bytes){
    if(!live()){last_error=6;return 0;}
    if(!current_process(process)){last_error=6;return 0;}
    if(!info || bytes<sizeof(mb_win_module_info)){last_error=87;return 0;}
    mb_win_module_enum_status st=mb_win_module_enum_info(installed,module,info);
    if(st){(void)fail(st,0);return 0;}
    last_error=0;return 1;
}

static int match(mb_pe_string view,const char *literal,size_t size,int fold){
    if(view.size!=size)return 0;
    for(size_t i=0;i<size;++i){
        unsigned char c=(unsigned char)view.data[i];
        if(fold && c>='A' && c<='Z')c=(unsigned char)(c+'a'-'A');
        if(c!=(unsigned char)literal[i])return 0;
    }
    return 1;
}
#define MATCH(v,s,fold) match((v),(s),sizeof(s)-1,(fold))
#define BIND(s,fn) if(MATCH(import->name,s,0)){*value=(uint64_t)(uintptr_t)&fn;return MB_PE_BIND_BOUND;}
mb_pe_bind_result mb_win_module_enum_bind(void *context,const mb_pe_import *import,uint64_t *value){
    mb_win_module_enum_session *session=context;
    if(!session || !import || !value ||
       (!import->library.data && import->library.size) || (!import->name.data && import->name.size))
        return MB_PE_BIND_ERROR;
    /* COMBINED-PAYLOAD ADAPTATION (worker-combined-dll-allfam): the single-pin
     * owner only ever holds the consumer record's session, so a record whose
     * service never began one (the helper fixture) must pass its imports
     * through UNRESOLVED instead of failing the whole bind chain. */
    if(!*find_session(session))return MB_PE_BIND_UNRESOLVED;
    if(!session->active || !session->module ||
       (session->module->state!=MB_PE_MODULE_LOADING && session->module->state!=MB_PE_MODULE_ACTIVE))
        return MB_PE_BIND_ERROR;
    if(import->by_ordinal || !MATCH(import->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    BIND("GetModuleHandleA",get_module_handle_a) BIND("GetModuleHandleW",get_module_handle_w)
    BIND("GetModuleHandleExW",get_module_handle_ex_w)
    BIND("GetModuleFileNameA",get_module_file_name_a) BIND("GetModuleFileNameW",get_module_file_name_w)
    BIND("GetProcAddress",get_proc_address)
    BIND("LoadLibraryA",load_library_a) BIND("LoadLibraryW",load_library_w)
    BIND("LoadLibraryExA",load_library_ex_a) BIND("LoadLibraryExW",load_library_ex_w)
    BIND("FreeLibrary",free_library)
    BIND("K32EnumProcessModules",k32_enum_process_modules)
    BIND("K32GetModuleBaseNameA",k32_get_module_base_name_a)
    BIND("K32GetModuleInformation",k32_get_module_information)
    return MB_PE_BIND_UNRESOLVED;
}
