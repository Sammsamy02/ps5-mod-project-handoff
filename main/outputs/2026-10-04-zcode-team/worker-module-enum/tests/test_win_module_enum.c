#include "mb_win_module_enum_abi.h"
#include <stdio.h>
#include <string.h>

/* Host test binary (compile -arch x86_64; ABI thunks use the Microsoft x64
 * convention through binder-resolved pointers). Covers: module registration,
 * case-insensitive ANSI handle lookup, GetProcAddress for known exports,
 * LoadLibrary/FreeLibrary reference counting, NULL handle/name = executable
 * image, enumeration/MODULEINFO/truncation, binder dispatch and negative
 * controls. */

static int failures;
#define CHECK(cond) do{if(!(cond)){++failures;printf("FAIL %d %s\n",__LINE__,#cond);}}while(0)
#define CHECK_EQ(a,b) do{if((long long)(a)!=(long long)(b)){++failures;printf("FAIL %d %s == %s\n",__LINE__,#a,#b);}}while(0)

static mb_win_module_enum_session *test_live_session; /* single live ABI session */

static void *dll_base=(void *)0x00007F0000004000ull;
static void *exe_base=(void *)0x00007FF600000000ull;
static int acquired_calls,released_calls;

static int stub_acquire(void *ctx,const char *name,void **base,uint32_t *size,void **token){
    static unsigned char slide; /* distinct base per acquisition */
    (void)ctx;++acquired_calls;
    if(!name || !strcmp(name,"missing.dll"))return -1;
    *base=(void *)0x00007F0100000000ull+(uintptr_t)(slide++ *0x1000ull);
    *size=4096;*token=(void *)0x1234;return 0;
}
static int stub_release(void *ctx,void *base,void *token){
    (void)ctx;(void)base;(void)token;++released_calls;return 0;
}

static const mb_win_module_export kernel32_exports[]={
    {"CreateFileW",(void *)0xABC0},{"GetModuleHandleW",(void *)0xABC1}
};
static const mb_win_module_export exe_exports[]={
    {"real_export",(void *)0x2}
};
static const uint16_t w_kernel32_dll[]={'k','e','r','n','e','l','3','2','.','d','l','l',0};
static const uint16_t w_kernel32[]={'k','e','r','n','e','l','3','2',0};

typedef __attribute__((ms_abi)) void *(*handle_a_fn)(const char *);
typedef __attribute__((ms_abi)) void *(*handle_w_fn)(const uint16_t *);
typedef __attribute__((ms_abi)) int32_t (*handle_ex_w_fn)(uint32_t,const uint16_t *,void **);
typedef __attribute__((ms_abi)) uint32_t (*file_name_a_fn)(void *,char *,uint32_t);
typedef __attribute__((ms_abi)) uint32_t (*file_name_w_fn)(void *,uint16_t *,uint32_t);
typedef __attribute__((ms_abi)) void *(*proc_fn)(void *,const char *);
typedef __attribute__((ms_abi)) int32_t (*free_fn)(void *);
typedef __attribute__((ms_abi)) int32_t (*enum_fn)(void *,void **,uint32_t,uint32_t *);
typedef __attribute__((ms_abi)) uint32_t (*base_name_a_fn)(void *,void *,char *,uint32_t);
typedef __attribute__((ms_abi)) int32_t (*info_fn)(void *,void *,mb_win_module_info *,uint32_t);
typedef __attribute__((ms_abi)) void *(*load_ex_a_fn)(const char *,void *,uint32_t);
typedef __attribute__((ms_abi)) void *(*load_ex_w_fn)(const uint16_t *,void *,uint32_t);

static mb_pe_import make_import(const char *library,const char *name){
    mb_pe_import i;memset(&i,0,sizeof i);
    i.library.data=library;i.library.size=strlen(library);
    i.name.data=name;i.name.size=strlen(name);
    return i;
}
static void *bind_thunk(const char *api_name){
    mb_pe_import import=make_import("KERNEL32.dll",api_name); /* fold check */
    uint64_t value=0;
    mb_pe_bind_result r=mb_win_module_enum_bind(test_live_session,&import,&value);
    if(r!=MB_PE_BIND_BOUND){printf("FAIL bind %s -> %d\n",api_name,(int)r);++failures;return (void *)0;}
    return (void *)(uintptr_t)value;
}

int main(void){
    printf("MODULE_ENUM_SELFTEST_BEGIN\n");
    CHECK_EQ(sizeof(mb_win_module_info),24u);
    mb_win_module_enum owner;memset(&owner,0,sizeof owner);
    owner.ops.acquire=stub_acquire;owner.ops.release=stub_release;
    struct mb_pe_module module;memset(&module,0,sizeof module);

    mb_win_module_enum_session session;memset(&session,0,sizeof session);
    CHECK_EQ(mb_win_module_enum_abi_begin(&session,&owner,&module),MB_WIN_MODULE_ENUM_OK);
    test_live_session=&session;
    CHECK_EQ(mb_win_module_enum_abi_begin(&session,&owner,&module),MB_WIN_MODULE_ENUM_BAD_STATE);

    /* registration: exe + dll; duplicate base/name/refs refusals */
    mb_win_module_record entry;memset(&entry,0,sizeof entry);
    strcpy(entry.name,"kernel32.dll");strcpy(entry.file_name,"C:\\Windows\\System32\\Kernel32.dll");
    entry.base=dll_base;entry.image_size=0x1000;entry.exports=kernel32_exports;entry.export_count=2;
    CHECK_EQ(mb_win_module_enum_register(&owner,&entry),MB_WIN_MODULE_ENUM_OK);
    mb_win_module_record exe;memset(&exe,0,sizeof exe);
    strcpy(exe.name,"selftest.exe");strcpy(exe.file_name,"C:\\game\\SelfTest.exe");
    exe.base=exe_base;exe.image_size=0x2000;exe.executable_image=1;
    exe.exports=exe_exports;exe.export_count=2;
    CHECK_EQ(mb_win_module_enum_register(&owner,&exe),MB_WIN_MODULE_ENUM_OK);
    mb_win_module_record dup=entry;
    CHECK_EQ(mb_win_module_enum_register(&owner,&dup),MB_WIN_MODULE_ENUM_BUSY); /* same base+name */
    dup=entry;dup.base=(void *)0x5000;strcpy(dup.name,"KERNEL32.DLL");
    CHECK_EQ(mb_win_module_enum_register(&owner,&dup),MB_WIN_MODULE_ENUM_BUSY); /* folded name clash */
    dup=entry;dup.base=(void *)0x5000;dup.references=1;
    CHECK_EQ(mb_win_module_enum_register(&owner,&dup),MB_WIN_MODULE_ENUM_INVALID_ARGUMENT);

    /* handle lookup: case-insensitive ANSI, .dll key policy, NULL = exe */
    void *base=(void *)0;
    CHECK_EQ(mb_win_module_enum_find(&owner,"KERNEL32.DLL",&base),MB_WIN_MODULE_ENUM_OK);
    CHECK(base==dll_base);
    CHECK_EQ(mb_win_module_enum_find(&owner,"Kernel32",&base),MB_WIN_MODULE_ENUM_OK); /* gains .dll */
    CHECK(base==dll_base);
    CHECK_EQ(mb_win_module_enum_find(&owner,"selftest.EXE",&base),MB_WIN_MODULE_ENUM_OK);
    CHECK(base==exe_base);
    CHECK_EQ(mb_win_module_enum_find(&owner,"nope.dll",&base),MB_WIN_MODULE_ENUM_NOT_FOUND);
    CHECK_EQ(mb_win_module_enum_find(&owner,(const char *)0,&base),MB_WIN_MODULE_ENUM_OK); /* exe */
    CHECK(base==exe_base);
    CHECK_EQ(mb_win_module_enum_find(&owner,"selftest.",&base),MB_WIN_MODULE_ENUM_NOT_FOUND); /* dot suppresses addition */
    CHECK_EQ(mb_win_module_enum_find(&owner,"kernel32.",&base),MB_WIN_MODULE_ENUM_NOT_FOUND);

    /* find_ex: FROM_ADDRESS and flag validation */
    CHECK_EQ(mb_win_module_enum_find_ex(&owner,MB_WIN_MODULE_EX_FLAG_FROM_ADDRESS,
        (const char *)(uintptr_t)((char *)dll_base+0x100),&base),MB_WIN_MODULE_ENUM_OK);
    CHECK(base==dll_base);
    CHECK_EQ(mb_win_module_enum_find_ex(&owner,MB_WIN_MODULE_EX_FLAG_FROM_ADDRESS,
        (const char *)(uintptr_t)0x1,&base),MB_WIN_MODULE_ENUM_NOT_FOUND);
    CHECK_EQ(mb_win_module_enum_find_ex(&owner,0x8u,"x",&base),MB_WIN_MODULE_ENUM_UNSUPPORTED);

    /* GetProcAddress core: exact names, ordinals, NULL = exe */
    void *proc=(void *)0;
    CHECK_EQ(mb_win_module_enum_proc_name(&owner,dll_base,"CreateFileW",&proc),MB_WIN_MODULE_ENUM_OK);
    CHECK(proc==(void *)0xABC0);
    CHECK_EQ(mb_win_module_enum_proc_name(&owner,dll_base,"createfileW",&proc),MB_WIN_MODULE_ENUM_NOT_FOUND);
    CHECK_EQ(mb_win_module_enum_proc_name(&owner,(void *)0,"real_export",&proc),MB_WIN_MODULE_ENUM_OK);
    CHECK(proc==(void *)0x2);
    CHECK_EQ(mb_win_module_enum_proc_ordinal(&owner,dll_base,1,&proc),MB_WIN_MODULE_ENUM_OK);
    CHECK(proc==(void *)0xABC0);
    CHECK_EQ(mb_win_module_enum_proc_ordinal(&owner,dll_base,3,&proc),MB_WIN_MODULE_ENUM_NOT_FOUND);
    CHECK_EQ(mb_win_module_enum_proc_ordinal(&owner,dll_base,0,&proc),MB_WIN_MODULE_ENUM_INVALID_ARGUMENT);
    proc=(void *)0xDEAD; /* negative control: no write on failure */
    (void)mb_win_module_enum_proc_name(&owner,dll_base,"absent",&proc);
    CHECK(proc==(void *)0xDEAD);

    /* names: full/base, truncation policy */
    char text[64];uint32_t written=0;
    CHECK_EQ(mb_win_module_enum_file_name(&owner,(void *)0,text,sizeof text,&written),MB_WIN_MODULE_ENUM_OK);
    CHECK(written==20 && !strcmp(text,"C:\\game\\SelfTest.exe"));
    CHECK_EQ(mb_win_module_enum_file_name(&owner,dll_base,text,5,&written),MB_WIN_MODULE_ENUM_CAPACITY);
    CHECK(written==5 && !strncmp(text,"C:\\Wi",4) && text[4]==0);
    CHECK_EQ(mb_win_module_enum_base_name(&owner,dll_base,text,sizeof text,&written),MB_WIN_MODULE_ENUM_OK);
    CHECK(written==12 && !strcmp(text,"kernel32.dll"));
    CHECK_EQ(mb_win_module_enum_file_name(&owner,dll_base,text,0,&written),MB_WIN_MODULE_ENUM_INVALID_ARGUMENT);

    /* Load/Free reference counting (registered module: no hook traffic) */
    void *loaded=(void *)0;
    CHECK_EQ(mb_win_module_enum_load(&owner,"KERNEL32.dll",&loaded),MB_WIN_MODULE_ENUM_OK);
    CHECK(loaded==dll_base);
    CHECK_EQ(mb_win_module_enum_load(&owner,"kernel32",&loaded),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(acquired_calls,0);
    CHECK_EQ(mb_win_module_enum_free(&owner,dll_base),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(mb_win_module_enum_free(&owner,dll_base),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(mb_win_module_enum_find(&owner,"kernel32.dll",&base),MB_WIN_MODULE_ENUM_OK); /* stays */
    CHECK_EQ(released_calls,0);
    CHECK_EQ(mb_win_module_enum_free(&owner,(void *)0x99),MB_WIN_MODULE_ENUM_NOT_FOUND);

    /* Load of unknown name: acquire hook registers and release disposes */
    CHECK_EQ(mb_win_module_enum_load(&owner,"fresh.dll",&loaded),MB_WIN_MODULE_ENUM_OK);
    CHECK(loaded==(void *)0x00007F0100000000ull); /* stub slide base 0 */
    CHECK_EQ(acquired_calls,1);
    CHECK_EQ(mb_win_module_enum_free(&owner,loaded),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(released_calls,1);
    CHECK_EQ(mb_win_module_enum_find(&owner,"fresh.dll",&base),MB_WIN_MODULE_ENUM_NOT_FOUND);
    CHECK_EQ(mb_win_module_enum_load(&owner,"missing.dll",&loaded),MB_WIN_MODULE_ENUM_OWNER_ERROR);

    /* enumeration + info */
    void *bases[4];uint32_t needed=0;
    CHECK_EQ(mb_win_module_enum_list(&owner,bases,4,&needed),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(needed,2u);
    CHECK_EQ(mb_win_module_enum_list(&owner,bases,1,&needed),MB_WIN_MODULE_ENUM_CAPACITY);
    CHECK_EQ(needed,2u);
    mb_win_module_info info;memset(&info,0,sizeof info);
    CHECK_EQ(mb_win_module_enum_info(&owner,dll_base,&info),MB_WIN_MODULE_ENUM_OK);
    CHECK(info.base==dll_base && info.size_of_image==0x1000 && !info.entry_point);

    /* ABI thunk surface through the binder, exact census names. The binder is
     * live while the owning image is LOADING/ACTIVE. */
    module.state=MB_PE_MODULE_LOADING;
    handle_a_fn handle_a=(handle_a_fn)bind_thunk("GetModuleHandleA");
    handle_w_fn handle_w=(handle_w_fn)bind_thunk("GetModuleHandleW");
    handle_ex_w_fn handle_ex=(handle_ex_w_fn)bind_thunk("GetModuleHandleExW");
    file_name_a_fn file_a=(file_name_a_fn)bind_thunk("GetModuleFileNameA");
    file_name_w_fn file_w=(file_name_w_fn)bind_thunk("GetModuleFileNameW");
    proc_fn get_proc=(proc_fn)bind_thunk("GetProcAddress");
    handle_a_fn load_a=(handle_a_fn)bind_thunk("LoadLibraryA");
    handle_w_fn load_w=(handle_w_fn)bind_thunk("LoadLibraryW");
    load_ex_a_fn load_ex_a=(load_ex_a_fn)bind_thunk("LoadLibraryExA");
    load_ex_w_fn load_ex_w=(load_ex_w_fn)bind_thunk("LoadLibraryExW");
    free_fn free_lib=(free_fn)bind_thunk("FreeLibrary");
    enum_fn enum_mods=(enum_fn)bind_thunk("K32EnumProcessModules");
    base_name_a_fn base_name=(base_name_a_fn)bind_thunk("K32GetModuleBaseNameA");
    info_fn mod_info=(info_fn)bind_thunk("K32GetModuleInformation");
    if(failures){printf("MODULE_ENUM_SELFTEST_RESULT %d\n",failures);return 1;}

    CHECK(handle_a("KERNEL32.DLL")==dll_base);
    CHECK(handle_a("nope")==((void *)0) && mb_win_module_enum_abi_last_error()==126);
    CHECK(handle_a((const char *)0)==exe_base); /* NULL name = exe module */
    CHECK(handle_w(w_kernel32_dll)==dll_base);
    CHECK(handle_w(w_kernel32)==dll_base); /* W path folds + .dll policy */
    void *from=(void *)0;
    CHECK(handle_ex(4u,(const uint16_t *)(uintptr_t)((char *)dll_base+0x100),&from)==1 && from==dll_base);
    CHECK(handle_ex(8u,w_kernel32,&from)==0 && mb_win_module_enum_abi_last_error()==50);

    CHECK(file_a((void *)0,text,sizeof text)==20 && mb_win_module_enum_abi_last_error()==0);
    CHECK(file_a(dll_base,text,5)==5 && mb_win_module_enum_abi_last_error()==122);
    uint16_t wide[64];
    CHECK(file_w(dll_base,wide,64)==32 && wide[0]=='C' && wide[31]=='l' && wide[32]==0);

    CHECK(get_proc(dll_base,"CreateFileW")==(void *)0xABC0);
    CHECK(mb_win_module_enum_abi_last_error()==0);
    CHECK(get_proc(dll_base,"Absent")==((void *)0) && mb_win_module_enum_abi_last_error()==127);
    CHECK(get_proc(dll_base,(const char *)(uintptr_t)2)==(void *)0xABC1); /* ordinal */
    CHECK(get_proc((void *)0,"real_export")==(void *)0x2); /* NULL = exe */

    CHECK(load_a("kernel32.dll")==dll_base);
    CHECK(load_w(w_kernel32)==dll_base); /* second ref */
    CHECK(load_ex_a("kernel32.dll",(void *)0,2u)==((void *)0) && mb_win_module_enum_abi_last_error()==50);
    CHECK(load_ex_w(w_kernel32_dll,(void *)1,0u)==((void *)0) && mb_win_module_enum_abi_last_error()==50);
    CHECK(free_lib(dll_base)==1);
    CHECK(free_lib(dll_base)==1);
    CHECK(free_lib((void *)0x99)==0 && mb_win_module_enum_abi_last_error()==126);

    void *list[4];uint32_t needed_bytes=0;
    CHECK(enum_mods((void *)(uintptr_t)-1,list,sizeof list,&needed_bytes)==1);
    CHECK_EQ(needed_bytes,16u);
    CHECK(enum_mods((void *)(uintptr_t)-1,list,8u,&needed_bytes)==0);
    CHECK_EQ(needed_bytes,16u); /* required size still reported */
    CHECK(mb_win_module_enum_abi_last_error()==122);
    CHECK(enum_mods((void *)0x7,list,sizeof list,&needed_bytes)==0 && mb_win_module_enum_abi_last_error()==6);

    CHECK(base_name((void *)(uintptr_t)-1,dll_base,text,sizeof text)==12);
    CHECK(base_name((void *)(uintptr_t)-1,(void *)0,text,sizeof text)==12); /* exe basename */
    CHECK(!strcmp(text,"selftest.exe"));

    mb_win_module_info winfo;memset(&winfo,0,sizeof winfo);
    CHECK(mod_info((void *)(uintptr_t)-1,(void *)0,&winfo,(uint32_t)sizeof winfo)==1);
    CHECK(winfo.base==exe_base && winfo.size_of_image==0x2000 && !winfo.entry_point);
    CHECK(mod_info((void *)(uintptr_t)-1,dll_base,&winfo,8u)==0 && mb_win_module_enum_abi_last_error()==87);

    /* binder negatives: other library, ordinal import, dead context */
    mb_pe_import other=make_import("user32.dll","FreeLibrary");
    uint64_t value=0;
    CHECK_EQ(mb_win_module_enum_bind(test_live_session,&other,&value),MB_PE_BIND_UNRESOLVED);
    mb_pe_import ordinal=make_import("kernel32.dll","FreeLibrary");ordinal.by_ordinal=1;ordinal.ordinal=7;
    CHECK_EQ(mb_win_module_enum_bind(test_live_session,&ordinal,&value),MB_PE_BIND_UNRESOLVED);
    CHECK_EQ(mb_win_module_enum_bind((void *)0,&ordinal,&value),MB_PE_BIND_ERROR);
    CHECK_EQ(mb_win_module_enum_bind(test_live_session,&ordinal,(uint64_t *)0),MB_PE_BIND_ERROR);

    /* end-of-life: thunk after final end fails with 6; clean re-begin works */
    mb_win_module_enum_session second;memset(&second,0,sizeof second);
    module.state=MB_PE_MODULE_EMPTY;
    CHECK_EQ(mb_win_module_enum_abi_begin(&second,&owner,&module),MB_WIN_MODULE_ENUM_BUSY);
    CHECK_EQ(mb_win_module_enum_abi_end(&session),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(mb_win_module_enum_abi_end(&session),MB_WIN_MODULE_ENUM_BAD_STATE);
    CHECK(handle_a("kernel32.dll")==((void *)0) && mb_win_module_enum_abi_last_error()==6);
    CHECK_EQ(mb_win_module_enum_abi_begin(&second,&owner,&module),MB_WIN_MODULE_ENUM_OK);
    CHECK_EQ(mb_win_module_enum_abi_end(&second),MB_WIN_MODULE_ENUM_OK);
    /* owner close requires module back at EMPTY */
    mb_win_module_enum_session third;memset(&third,0,sizeof third);
    CHECK_EQ(mb_win_module_enum_abi_begin(&third,&owner,&module),MB_WIN_MODULE_ENUM_OK);
    module.state=MB_PE_MODULE_ACTIVE;
    CHECK_EQ(mb_win_module_enum_abi_end(&third),MB_WIN_MODULE_ENUM_BUSY);
    module.state=MB_PE_MODULE_EMPTY;
    CHECK_EQ(mb_win_module_enum_abi_end(&third),MB_WIN_MODULE_ENUM_OK);

    printf("MODULE_ENUM_SELFTEST_RESULT %d\n",failures);
    return failures?1:0;
}
