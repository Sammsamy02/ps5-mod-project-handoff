/* Owned, freestanding PE fixture: only the declared imports may provide CRT. */
typedef unsigned long long usize;
__declspec(dllimport) void *malloc(usize);
__declspec(dllimport) void *calloc(usize,usize);
__declspec(dllimport) void *realloc(void *,usize);
__declspec(dllimport) void free(void *);
__declspec(dllimport) int *_errno(void);
__declspec(dllimport) usize strlen(const char *);
static const char text[]="heap";
static const char *volatile relocated_text=text;
#define CHECK(x,n) do { if (!(x)) return (n); } while(0)
static int aligned(void *p) {return p && ((usize)p&15)==0;}
__declspec(dllexport) int HeapExercise(unsigned stage,void **retained)
{
    int *error=_errno();CHECK(error,1);
    if (stage==0) {
        *error=1777;
        CHECK(strlen(relocated_text)==4,2);
        unsigned char *p=malloc(13);CHECK(aligned(p) && *error==1777,3);
        for (unsigned i=0;i<13;++i) p[i]=(unsigned char)(i+1);
        unsigned char *z=calloc(3,7);CHECK(aligned(z),4);
        for (unsigned i=0;i<21;++i) CHECK(z[i]==0,5);
        free(z);
        unsigned char *q=realloc(p,40);CHECK(aligned(q),6);
        for (unsigned i=0;i<13;++i) CHECK(q[i]==i+1,7);
        p=realloc(q,7);CHECK(aligned(p),8);
        for (unsigned i=0;i<7;++i) CHECK(p[i]==i+1,9);
        void *zero=malloc(0);CHECK(aligned(zero),10);free(zero);
        zero=calloc(0,19);CHECK(aligned(zero),11);free(zero);
        zero=calloc(19,0);CHECK(aligned(zero),12);free(zero);
        zero=realloc((void *)0,0);CHECK(aligned(zero),13);
        CHECK(realloc(zero,0)==(void *)0 && *error==1777,14);
        *retained=p;
        return 0;
    }
    if (stage==1) {
        unsigned char *original=*retained;
        CHECK(original && realloc(original,48)==(void *)0 && *error==12,15);
        for (unsigned i=0;i<7;++i) CHECK(original[i]==i+1,16);
        CHECK(*retained==original,17);
        *error=1888;
        CHECK(calloc(~(usize)0,2)==(void *)0 && *error==12,18);
        *error=1888;
        CHECK(malloc(~(usize)0)==(void *)0 && *error==12,19);
        *error=1999;free((void *)0);CHECK(*error==1999,20);
        return 0;
    }
    return 99;
}
__declspec(dllexport) int *HeapErrno(void) {return _errno();}
