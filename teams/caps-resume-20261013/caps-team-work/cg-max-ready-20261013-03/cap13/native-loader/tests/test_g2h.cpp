#include <cstdint>
#include <cstdio>

static uint64_t seen[8];
static constexpr uint64_t kReturn = UINT64_C(0x8877665544332211);

extern "C" uint64_t target5(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e)
{ seen[0]=a;seen[1]=b;seen[2]=c;seen[3]=d;seen[4]=e;return kReturn; }
extern "C" uint64_t target6(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f)
{ seen[0]=a;seen[1]=b;seen[2]=c;seen[3]=d;seen[4]=e;seen[5]=f;return kReturn; }
extern "C" uint64_t target7(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g)
{ seen[0]=a;seen[1]=b;seen[2]=c;seen[3]=d;seen[4]=e;seen[5]=f;seen[6]=g;return kReturn; }
extern "C" uint64_t target8(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g,uint64_t h)
{ seen[0]=a;seen[1]=b;seen[2]=c;seen[3]=d;seen[4]=e;seen[5]=f;seen[6]=g;seen[7]=h;return kReturn; }

extern "C" __attribute__((ms_abi)) uint64_t guest_bridge5(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
extern "C" __attribute__((ms_abi)) uint64_t guest_bridge6(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
extern "C" __attribute__((ms_abi)) uint64_t guest_bridge7(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
extern "C" __attribute__((ms_abi)) uint64_t guest_bridge8(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);

static int check(int n,uint64_t ret)
{
    bool ok=ret==kReturn;
    for(int i=0;i<n;++i)ok=ok && seen[i]==uint64_t((i+1)*11);
    std::printf("g2h arity %d %s",n,ok?"PASS":"FAIL");
    if(!ok) { std::printf(" args=");for(int i=0;i<n;++i)std::printf("%s%llu",i?",":"",(unsigned long long)seen[i]); }
    std::printf("\n");
    return ok?0:1;
}

int main()
{
    int failures=0;
    failures+=check(5,guest_bridge5(11,22,33,44,55));
    failures+=check(6,guest_bridge6(11,22,33,44,55,66));
    failures+=check(7,guest_bridge7(11,22,33,44,55,66,77));
    failures+=check(8,guest_bridge8(11,22,33,44,55,66,77,88));
    return failures?1:0;
}
