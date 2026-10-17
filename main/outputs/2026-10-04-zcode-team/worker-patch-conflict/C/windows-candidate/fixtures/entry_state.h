/* Owned dynamic DllMain fixture. No CRT startup, static TLS, unwind, worker
 * threads, callbacks escaping this image, or loader calls from DllMain. */
#ifndef MB_FIXTURE_REJECT_ATTACH
#define MB_FIXTURE_REJECT_ATTACH 0
#endif
__declspec(dllimport) void *malloc(usize);
__declspec(dllimport) void free(void *);
__declspec(dllimport) u32 TlsAlloc(void);
__declspec(dllimport) int TlsFree(u32);
__declspec(dllimport) void *TlsGetValue(u32);
__declspec(dllimport) int TlsSetValue(u32,void *);
__declspec(dllimport) u32 GetLastError(void);
__declspec(dllimport) void SetLastError(u32);
#if MB_OWNED_CLOCK
#include "clock_state.h"
#endif
extern unsigned char __ImageBase;
static void *volatile entry_base=&__ImageBase;
static u32 entry_slot=0xffffffffu;
static u32 *entry_token;
static unsigned entry_ready;
static int entry_check(void){
 return entry_ready==1 && entry_token && *entry_token==0x71a55ac3u && TlsGetValue(entry_slot)==entry_token;
}
int DllMain(void *image,u32 reason,void *reserved){
 if(image!=entry_base || image!=(void *)&__ImageBase || reserved || reason>1)return 0;
 if(reason==1){
  if(entry_ready || entry_token)return 0;
  entry_token=malloc(sizeof(*entry_token));if(!entry_token)return 0;
  *entry_token=0x71a55ac3u;entry_slot=TlsAlloc();if(entry_slot==0xffffffffu)return 0;
  if(!TlsSetValue(entry_slot,entry_token))return 0;
  SetLastError(0xefa7135bu);if(GetLastError()!=0xefa7135bu)return 0;
#if MB_OWNED_CLOCK
  if(!clock_check())return 0;
#endif
  entry_ready=1;return MB_FIXTURE_REJECT_ATTACH?0:7;
 }
 int ok=entry_check();
 if(entry_slot!=0xffffffffu){if(!TlsFree(entry_slot))ok=0;entry_slot=0xffffffffu;}
 free(entry_token);entry_token=(void *)0;entry_ready=0;
 SetLastError(0xfdea3210u);
#if MB_OWNED_CLOCK
 if(!clock_check())ok=0;
#endif
 return ok?0:-71; /* The native owner must ignore the zero detach BOOL. */
}
