#include "win64_bridge.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

static std::array<uint64_t, 9> observed;
static int context_anchor = 0x42;
static int failures = 0;
static constexpr uint64_t kReturn = 0xA13B5C7D9E2468F0ull;

extern "C" uint64_t cap13_test_context_nonvolatiles(void* win64_entry);
extern "C" uint64_t cap13_hostile_sysv_target(void* context);
extern "C" uint64_t cap13_sysv_alignment_target(void* context);

static void* context() { return &context_anchor; }
static void record0(void* c) { observed[0] = reinterpret_cast<uint64_t>(c); }
static uint64_t body0(void* c) { record0(c); return kReturn; }
static uint64_t body1(void* c, uint64_t a) { record0(c); observed[1]=a; return kReturn; }
static uint64_t body2(void* c, uint64_t a, uint64_t b) { record0(c); observed[1]=a; observed[2]=b; return kReturn; }
static uint64_t body3(void* c, uint64_t a, uint64_t b, uint64_t d) { record0(c); observed[1]=a; observed[2]=b; observed[3]=d; return kReturn; }
static uint64_t body4(void* c, uint64_t a, uint64_t b, uint64_t d, uint64_t e) { record0(c); observed[1]=a; observed[2]=b; observed[3]=d; observed[4]=e; return kReturn; }
static uint64_t body5(void* c, uint64_t a, uint64_t b, uint64_t d, uint64_t e, uint64_t f) { record0(c); observed[1]=a; observed[2]=b; observed[3]=d; observed[4]=e; observed[5]=f; return kReturn; }
static uint64_t body6(void* c, uint64_t a, uint64_t b, uint64_t d, uint64_t e, uint64_t f, uint64_t g) { record0(c); observed[1]=a; observed[2]=b; observed[3]=d; observed[4]=e; observed[5]=f; observed[6]=g; return kReturn; }
static uint64_t body7(void* c, uint64_t a, uint64_t b, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h) { record0(c); observed[1]=a; observed[2]=b; observed[3]=d; observed[4]=e; observed[5]=f; observed[6]=g; observed[7]=h; return kReturn; }
static uint64_t body8(void* c, uint64_t a, uint64_t b, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h, uint64_t i) { record0(c); observed[1]=a; observed[2]=b; observed[3]=d; observed[4]=e; observed[5]=f; observed[6]=g; observed[7]=h; observed[8]=i; return kReturn; }

using Win0 = uint64_t(__attribute__((ms_abi)) *)();
using Win1 = uint64_t(__attribute__((ms_abi)) *)(uint64_t);
using Win2 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t);
using Win3 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t,uint64_t);
using Win4 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t,uint64_t,uint64_t);
using Win5 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
using Win6 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
using Win7 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
using Win8 = uint64_t(__attribute__((ms_abi)) *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);

static void check(bool good, const char* name)
{
    std::printf("%s %s\n", good ? "PASS" : "FAIL", name);
    if (!good) ++failures;
}

static bool args_match(unsigned count, uint64_t returned)
{
    constexpr uint64_t values[8] = {0x101, 0x202, 0x303, 0x404,
                                    0x505, 0x606, 0x707, 0x808};
    if (returned != kReturn || observed[0] != reinterpret_cast<uint64_t>(context()))
        return false;
    for (unsigned i=0; i<8; ++i)
        if (observed[i+1] != (i<count ? values[i] : UINT64_MAX)) return false;
    return true;
}

int main()
{
    using cap13::ContextBridgeTramp;
    const long host_page = sysconf(_SC_PAGESIZE);
    if (host_page < 4096) return 2;
    auto* page = static_cast<uint8_t*>(mmap(nullptr, static_cast<size_t>(host_page),
                                           PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANON, -1, 0));
    if (page == MAP_FAILED) return 2;
    std::memset(page, 0xCC, static_cast<size_t>(host_page));
    void* targets[9] = {reinterpret_cast<void*>(&body0), reinterpret_cast<void*>(&body1),
                        reinterpret_cast<void*>(&body2), reinterpret_cast<void*>(&body3),
                        reinterpret_cast<void*>(&body4), reinterpret_cast<void*>(&body5),
                        reinterpret_cast<void*>(&body6), reinterpret_cast<void*>(&body7),
                        reinterpret_cast<void*>(&body8)};
    bool bounds_ok = true;
    for (unsigned n=0; n<=8; ++n)
    {
        auto* slot = page + n*sizeof(ContextBridgeTramp);
        const void* thunk = cap13::bridge_select_context_thunk(n);
        if (!thunk) return 2;
        cap13::bridge_stamp_context_tramp(slot, context(), targets[n], thunk);
        bounds_ok &= slot[sizeof(ContextBridgeTramp)] == 0xCC;
    }
    cap13::bridge_stamp_context_tramp(
        page + 9*sizeof(ContextBridgeTramp), context(),
        reinterpret_cast<void*>(&cap13_hostile_sysv_target),
        cap13::bridge_select_context_thunk(0));
    cap13::bridge_stamp_context_tramp(
        page + 10*sizeof(ContextBridgeTramp), context(),
        reinterpret_cast<void*>(&cap13_sysv_alignment_target),
        cap13::bridge_select_context_thunk(0));
    check(bounds_ok && sizeof(ContextBridgeTramp) == 32,
          "each stamped context trampoline stays in its 32-byte slot");
    check(cap13::bridge_select_context_thunk(9) == nullptr,
          "ninth guest integer argument is refused");
    if (mprotect(page, static_cast<size_t>(host_page), PROT_READ | PROT_EXEC) != 0)
        return 2;

#define CASE(N, CALL) do { \
    observed.fill(UINT64_MAX); \
    const uint64_t result = (CALL); \
    char label[100]; \
    std::snprintf(label, sizeof(label), "context and %u Win64 integer args reach SysV target", N); \
    check(args_match(N, result), label); \
} while (0)
    CASE(0, reinterpret_cast<Win0>(page + 0*sizeof(ContextBridgeTramp))());
    CASE(1, reinterpret_cast<Win1>(page + 1*sizeof(ContextBridgeTramp))(0x101));
    CASE(2, reinterpret_cast<Win2>(page + 2*sizeof(ContextBridgeTramp))(0x101,0x202));
    CASE(3, reinterpret_cast<Win3>(page + 3*sizeof(ContextBridgeTramp))(0x101,0x202,0x303));
    CASE(4, reinterpret_cast<Win4>(page + 4*sizeof(ContextBridgeTramp))(0x101,0x202,0x303,0x404));
    CASE(5, reinterpret_cast<Win5>(page + 5*sizeof(ContextBridgeTramp))(0x101,0x202,0x303,0x404,0x505));
    CASE(6, reinterpret_cast<Win6>(page + 6*sizeof(ContextBridgeTramp))(0x101,0x202,0x303,0x404,0x505,0x606));
    CASE(7, reinterpret_cast<Win7>(page + 7*sizeof(ContextBridgeTramp))(0x101,0x202,0x303,0x404,0x505,0x606,0x707));
    CASE(8, reinterpret_cast<Win8>(page + 8*sizeof(ContextBridgeTramp))(0x101,0x202,0x303,0x404,0x505,0x606,0x707,0x808));
#undef CASE
    check(cap13_test_context_nonvolatiles(page + 9*sizeof(ContextBridgeTramp)) == 0x1F,
          "SysV target clobbers do not escape through Win64 nonvolatile registers");
    check(reinterpret_cast<Win0>(page + 10*sizeof(ContextBridgeTramp))() == 8,
          "SysV target receives an ABI-aligned stack");

    munmap(page, static_cast<size_t>(host_page));
    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
