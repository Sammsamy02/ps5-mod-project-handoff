/* Owned helper loaded from disk; its own imports require per-module sessions. */
typedef unsigned int u32;
typedef unsigned long long usize;
__declspec(dllimport) void *malloc(usize);
__declspec(dllimport) void free(void *);
__declspec(dllimport) u32 TlsAlloc(void);
__declspec(dllimport) int TlsFree(u32);
__declspec(dllimport) void *TlsGetValue(u32);
__declspec(dllimport) int TlsSetValue(u32,void *);
static const int bias=9;
static const int *volatile relocated_bias=&bias;
__declspec(dllexport) int HelperCompute(int value) {
    int *stored=malloc(sizeof(*stored));if(!stored)return -1;
    *stored=value*7+*relocated_bias;
    u32 index=TlsAlloc();if(index==0xffffffffu){free(stored);return -2;}
    int result=-3;
    if(TlsSetValue(index,stored) && TlsGetValue(index)==stored)result=*stored;
    if(!TlsFree(index))result=-4;
    free(stored);return result;
}
