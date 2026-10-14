/* Owned no-entry consumer calls the actual Windows module API imports. */
typedef unsigned int u32;
typedef unsigned long long usize;
typedef unsigned short u16;
__declspec(dllimport) void *LoadLibraryA(const char *);
__declspec(dllimport) void *LoadLibraryW(const u16 *);
__declspec(dllimport) void *LoadLibraryExA(const char *,void *,u32);
__declspec(dllimport) void *LoadLibraryExW(const u16 *,void *,u32);
__declspec(dllimport) void *GetModuleHandleA(const char *);
__declspec(dllimport) void *GetModuleHandleW(const u16 *);
__declspec(dllimport) void *GetProcAddress(void *,const char *);
__declspec(dllimport) int FreeLibrary(void *);
__declspec(dllimport) u32 GetLastError(void);
__declspec(dllimport) void SetLastError(u32);
static const char helper[]="owned_helper.dll";
static const char *volatile relocated_helper=helper;
static const u16 wide_helper[]={'o','w','n','e','d','_','h','e','l','p','e','r','.','d','l','l',0};
typedef int (*helper_fn)(int);
#define CHECK(x,n) do {if(!(x)) return (n);}while(0)
__declspec(dllexport) int LoaderExercise(usize *results)
{
    CHECK(GetModuleHandleA(relocated_helper)==(void *)0,1);
    SetLastError(0xe234abcdu);
    void *a=LoadLibraryA(relocated_helper);CHECK(a && GetLastError()==0xe234abcdu,2);
    void *w=LoadLibraryW(wide_helper);CHECK(w==a,3);
    CHECK(GetModuleHandleA("OWNED_HELPER.DLL")==a && GetModuleHandleW(wide_helper)==a,4);
    CHECK(LoadLibraryExA(relocated_helper,(void *)0,0)==a,5);
    CHECK(LoadLibraryExW(wide_helper,(void *)0,0)==a,6);
    void *named=GetProcAddress(a,"HelperCompute");
    void *ordinal=GetProcAddress(a,(const char *)(usize)7);
    CHECK(named && ordinal==named,7);
    results[0]=(usize)a;results[1]=(usize)named;
    results[2]=(usize)((helper_fn)named)(11);
    results[3]=(usize)((helper_fn)ordinal)(12);
    CHECK(results[2]==86 && results[3]==93,8);
    SetLastError(0);
    CHECK(LoadLibraryExA(relocated_helper,(void *)0,1)==(void *)0 && GetLastError()!=0,9);
    SetLastError(0);
    CHECK(LoadLibraryExW(wide_helper,(void *)(usize)1,0)==(void *)0 && GetLastError()!=0,10);
    CHECK(FreeLibrary(a)!=0 && GetModuleHandleA(relocated_helper)==a,11);
    CHECK(FreeLibrary(a)!=0 && GetModuleHandleW(wide_helper)==a,12);
    CHECK(FreeLibrary(a)!=0 && GetModuleHandleA(relocated_helper)==a,13);
    return 0;
}
__declspec(dllexport) int LoaderUnload(void *handle) {
    SetLastError(0xe7771234u);int result=FreeLibrary(handle);
    if(result && GetLastError()!=0xe7771234u)return 0;return result;
}
__declspec(dllexport) void *LoaderLookup(void) {return GetModuleHandleA(relocated_helper);}
