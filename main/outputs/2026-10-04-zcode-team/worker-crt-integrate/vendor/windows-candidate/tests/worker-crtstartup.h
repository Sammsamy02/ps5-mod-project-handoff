#ifndef MB_OWNED_WORKER_CRTSTARTUP_H
#define MB_OWNED_WORKER_CRTSTARTUP_H
/* Owned PE fixture: the census-observed UCRT startup family through actual
 * api-ms-win-crt-runtime-l1-1-0.dll imports (all seven def exports are
 * called, so the PE import census really exercises them; the optional
 * vcruntime140.dll _seh_filter_dll row has zero census rows and is not
 * imported here). The retained image owns CRT table storage and this family
 * never allocates, so the fixture carries its own static __onexit_table_t-
 * shaped {first,last,end} triple plus slot array; the documented owner-only
 * attach service is mirrored by the fixture owning those fields directly.
 * Worker thread: _initialize_onexit_table EINVAL on NULL and zeroing of a
 * caller-backed table, attach, register→execute LIFO (executed slots
 * cleared, table emptied, second execute a no-op, reuse after execute),
 * _initterm over a sentinel table (interior NULLs skipped, exclusive end),
 * _initterm_e error-stop, then _crt_atexit LIFO drained by _cexit (double
 * _cexit a no-op). Creator-side finish is pure recorded-state verification.
 * Error returns -55..-59 are unique across this tree's fixtures (sync
 * -40..-44, initonce -45..-54, crt startup -55..-59, mutex -60..-77, base
 * worker -1..-18, fileio -80..-89). Execution-order record: '7','6' (onexit LIFO), '8'
 * (reuse), '1','2' (_initterm), '3' (_initterm_e stop), '5','4' (atexit
 * LIFO) - eight entries in total. */
typedef int (*mb_crtstartup_pif)(void);  /* _PIFV */
typedef void (*mb_crtstartup_pvf)(void); /* _PVFV / _onexit_t */
__declspec(dllimport) void _initterm(mb_crtstartup_pif *,mb_crtstartup_pif *);
__declspec(dllimport) int _initterm_e(mb_crtstartup_pif *,mb_crtstartup_pif *);
__declspec(dllimport) int _crt_atexit(mb_crtstartup_pvf);
__declspec(dllimport) void _cexit(void);
__declspec(dllimport) int _initialize_onexit_table(void *);
__declspec(dllimport) mb_crtstartup_pvf _register_onexit_function(void *,mb_crtstartup_pvf);
__declspec(dllimport) int _execute_onexit_table(void *);
#define MB_WORKER_CRTSTARTUP_SLOTS 4u
#define MB_WORKER_CRTSTARTUP_BEFORE 0x51d5beef5a5a3333ULL
#define MB_WORKER_CRTSTARTUP_AFTER 0x51d6beef5a5a4444ULL
static struct {
 mb_worker_u64 guard_before,guard_after;
 mb_worker_u32 order[8],n;
 mb_worker_i32 init_null_rc,init_rc,execute_rc,reexecute_rc,execute2_rc,atexit_rc1,atexit_rc2,initterm_e_rc;
 mb_worker_u32 done;
 void *register_a,*register_b,*register_c;
 mb_worker_u32 table_emptied,never_ran;
} mb_worker_crtstartup_state;
static void *mb_worker_crtstartup_slots[MB_WORKER_CRTSTARTUP_SLOTS];
static struct {void **first,**last,**end;} mb_worker_crtstartup_table;
static mb_worker_i32 mb_worker_crtstartup_i1(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='1';return 0;}
static mb_worker_i32 mb_worker_crtstartup_i2(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='2';return 0;}
static mb_worker_i32 mb_worker_crtstartup_i3(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='3';return 7;}
static mb_worker_i32 mb_worker_crtstartup_i4(void){mb_worker_crtstartup_state.never_ran=1;return 0;}
static void mb_worker_crtstartup_x1(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='4';}
static void mb_worker_crtstartup_x2(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='5';}
static void mb_worker_crtstartup_o1(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='6';}
static void mb_worker_crtstartup_o2(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='7';}
static void mb_worker_crtstartup_o3(void){mb_worker_crtstartup_state.order[mb_worker_crtstartup_state.n++]='8';}
/* Worker thread: the full census-observed startup exercise. */
static mb_worker_i32 mb_worker_crtstartup_thread_begin(void){
 mb_crtstartup_pif table_i[5]={mb_worker_crtstartup_i1,(mb_crtstartup_pif)0,mb_worker_crtstartup_i2,(mb_crtstartup_pif)0,(mb_crtstartup_pif)0};
 mb_crtstartup_pif table_e[4]={mb_worker_crtstartup_i3,(mb_crtstartup_pif)0,mb_worker_crtstartup_i4,(mb_crtstartup_pif)0};
 mb_worker_crtstartup_state.guard_before=MB_WORKER_CRTSTARTUP_BEFORE;
 mb_worker_crtstartup_state.guard_after=MB_WORKER_CRTSTARTUP_AFTER;
 mb_worker_crtstartup_state.n=0;mb_worker_crtstartup_state.never_ran=0;
 /* EINVAL on NULL, then the documented zeroing of a caller-backed table. */
 mb_worker_crtstartup_table.first=mb_worker_crtstartup_table.last=mb_worker_crtstartup_slots;
 mb_worker_crtstartup_table.end=mb_worker_crtstartup_slots+MB_WORKER_CRTSTARTUP_SLOTS;
 if((mb_worker_crtstartup_state.init_null_rc=_initialize_onexit_table((void *)0))==0)return 0;
 if((mb_worker_crtstartup_state.init_rc=_initialize_onexit_table(&mb_worker_crtstartup_table))!=0)return 0;
 if(mb_worker_crtstartup_table.first||mb_worker_crtstartup_table.last||mb_worker_crtstartup_table.end)return 0;
 /* The retained image owns table storage: the fixture mirrors the documented
  * entry-adapter attach by owning the fields directly (never allocates). */
 mb_worker_crtstartup_table.first=mb_worker_crtstartup_table.last=mb_worker_crtstartup_slots;
 mb_worker_crtstartup_table.end=mb_worker_crtstartup_slots+MB_WORKER_CRTSTARTUP_SLOTS;
 if((mb_worker_crtstartup_state.register_a=_register_onexit_function(&mb_worker_crtstartup_table,mb_worker_crtstartup_o1))==(void *)0)return 0;
 if((mb_worker_crtstartup_state.register_b=_register_onexit_function(&mb_worker_crtstartup_table,mb_worker_crtstartup_o2))==(void *)0)return 0;
 if((mb_worker_crtstartup_state.execute_rc=_execute_onexit_table(&mb_worker_crtstartup_table))!=0)return 0;
 if(mb_worker_crtstartup_state.n!=2 || mb_worker_crtstartup_state.order[0]!='7' || mb_worker_crtstartup_state.order[1]!='6')return 0;
 if(mb_worker_crtstartup_slots[0] || mb_worker_crtstartup_slots[1])return 0;
 /* Emptied means last walked back onto first with every executed slot
  * cleared; first/end keep the caller's attached storage (still-backed
  * reuse is the documented model choice, so registration works again). */
 if(mb_worker_crtstartup_table.last!=mb_worker_crtstartup_table.first)return 0;
 mb_worker_crtstartup_state.table_emptied=1;
 if((mb_worker_crtstartup_state.reexecute_rc=_execute_onexit_table(&mb_worker_crtstartup_table))!=0)return 0;
 if(mb_worker_crtstartup_state.n!=2)return 0; /* second execution is a no-op */
 if((mb_worker_crtstartup_state.register_c=_register_onexit_function(&mb_worker_crtstartup_table,mb_worker_crtstartup_o3))==(void *)0)return 0;
 if((mb_worker_crtstartup_state.execute2_rc=_execute_onexit_table(&mb_worker_crtstartup_table))!=0)return 0;
 if(mb_worker_crtstartup_state.n!=3 || mb_worker_crtstartup_state.order[2]!='8')return 0;
 /* _initterm: sentinel-terminated, interior NULLs skipped, end exclusive. */
 _initterm(table_i,table_i+4);
 if(mb_worker_crtstartup_state.n!=5 || mb_worker_crtstartup_state.order[3]!='1' || mb_worker_crtstartup_state.order[4]!='2')return 0;
 /* _initterm_e: stops at the first nonzero errno_t; later entries never run. */
 if((mb_worker_crtstartup_state.initterm_e_rc=_initterm_e(table_e,table_e+4))!=7)return 0;
 if(mb_worker_crtstartup_state.never_ran)return 0;
 /* Global atexit table LIFO drained by _cexit; double _cexit is a no-op. */
 if((mb_worker_crtstartup_state.atexit_rc1=_crt_atexit(mb_worker_crtstartup_x1))!=0)return 0;
 if((mb_worker_crtstartup_state.atexit_rc2=_crt_atexit(mb_worker_crtstartup_x2))!=0)return 0;
 _cexit();
 if(mb_worker_crtstartup_state.n!=8 || mb_worker_crtstartup_state.order[6]!='5' || mb_worker_crtstartup_state.order[7]!='4')return 0;
 _cexit();
 if(mb_worker_crtstartup_state.n!=8)return 0;
 mb_worker_crtstartup_state.done=0x51c7beefU;
 return 1;
}
/* Creator-side after join: pure verification of the recorded state. */
static mb_worker_i32 mb_worker_crtstartup_finish(void){
 if(mb_worker_crtstartup_state.done!=0x51c7beefU ||
    mb_worker_crtstartup_state.guard_before!=MB_WORKER_CRTSTARTUP_BEFORE ||
    mb_worker_crtstartup_state.guard_after!=MB_WORKER_CRTSTARTUP_AFTER)return -55;
 if(mb_worker_crtstartup_state.init_null_rc==0 || mb_worker_crtstartup_state.init_rc ||
    mb_worker_crtstartup_state.execute_rc ||
    mb_worker_crtstartup_state.reexecute_rc || mb_worker_crtstartup_state.execute2_rc)return -56;
 if(!mb_worker_crtstartup_state.table_emptied || mb_worker_crtstartup_state.never_ran)return -57;
 if(!mb_worker_crtstartup_state.register_a || !mb_worker_crtstartup_state.register_b ||
    !mb_worker_crtstartup_state.register_c)return -58;
 if(mb_worker_crtstartup_state.n!=8 || mb_worker_crtstartup_state.atexit_rc1 ||
    mb_worker_crtstartup_state.atexit_rc2 || mb_worker_crtstartup_state.initterm_e_rc!=7)return -59;
 return 1;
}
#endif
