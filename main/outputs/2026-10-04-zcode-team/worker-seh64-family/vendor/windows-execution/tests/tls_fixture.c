/* Owned dynamic-TLS fixture: no PE static TLS directory or Windows CRT startup. */
typedef unsigned int u32;
typedef unsigned long long usize;
__declspec(dllimport) u32 GetLastError(void);
__declspec(dllimport) void SetLastError(u32);
__declspec(dllimport) u32 TlsAlloc(void);
__declspec(dllimport) int TlsFree(u32);
__declspec(dllimport) void *TlsGetValue(u32);
__declspec(dllimport) int TlsSetValue(u32,void *);
__declspec(dllimport) int *_errno(void);
__declspec(dllimport) usize strlen(const char *);
static const char text[]="tls";
static const char *volatile relocated_text=text;
#define CHECK(x,n) do {if(!(x)) return (n);}while(0)
__declspec(dllexport) int TlsExercise(u32 stage,u32 *index,void *payload,void **observed)
{
    int *error=_errno();CHECK(error,1);
    if(stage==0) {
        CHECK(strlen(relocated_text)==3,2);
        *error=1717;SetLastError(0xf1234567u);CHECK(GetLastError()==0xf1234567u,3);
        *index=TlsAlloc();CHECK(*index!=0xffffffffu && GetLastError()==0xf1234567u,4);
        CHECK(TlsGetValue(*index)==(void *)0 && GetLastError()==0,5);
        SetLastError(0xe1111111u);CHECK(TlsSetValue(*index,payload)!=0 && GetLastError()==0xe1111111u,6);
        *observed=TlsGetValue(*index);CHECK(*observed==payload && GetLastError()==0,7);
        SetLastError(0xe1111111u);CHECK(*error==1717,8);
        return 0;
    }
    if(stage==1) {
        CHECK(GetLastError()==0 && *error==0,9);
        SetLastError(0xe2222222u);CHECK(TlsGetValue(*index)==(void *)0 && GetLastError()==0,10);
        CHECK(TlsSetValue(*index,payload)!=0,11);
        *observed=TlsGetValue(*index);CHECK(*observed==payload && GetLastError()==0,12);
        *error=2727;SetLastError(0xe2222222u);return 0;
    }
    if(stage==2) {
        CHECK(GetLastError()==0xe1111111u && *error==1717,13);
        CHECK(TlsGetValue(*index)==payload && GetLastError()==0,14);
        u32 old=*index;SetLastError(0xf3333333u);
        CHECK(TlsFree(old)!=0 && GetLastError()==0xf3333333u,15);
        *index=TlsAlloc();
        /* Lowest available index is this bounded adapter's declared policy. */
        CHECK(*index==old && GetLastError()==0xf3333333u,16);
        *observed=TlsGetValue(*index);CHECK(*observed==(void *)0 && GetLastError()==0,17);
        SetLastError(0xe1111111u);CHECK(*error==1717,18);return 0;
    }
    if(stage==3) {
        CHECK(GetLastError()==0xe2222222u && *error==2727,19);
        *observed=TlsGetValue(*index);CHECK(*observed==(void *)0 && GetLastError()==0,20);
        CHECK(*error==2727,21);return 0;
    }
    if(stage==4) {
        CHECK(GetLastError()==0xe1111111u && *error==1717,22);
        SetLastError(0xf4444444u);CHECK(TlsFree(*index)!=0 && GetLastError()==0xf4444444u,23);
        return 0;
    }
    return 99;
}
