// Host test (Mac clang++) for Cap10OfflineNet.cpp: the owned, fail-closed
// SceNet resolver entry points. Proves each returns its failure value, the
// errno-loc points at a readable ENETDOWN slot, and diagnostic_event fires at
// most once per function (first refusal only). Compiled WITHOUT __PROSPERO__,
// so it is a pure host check of the owned stub logic. Prints a count.
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string_view>

extern "C" int* sceNetErrnoLoc();
extern "C" int sceNetPoolCreate(const char*, int, int);
extern "C" int sceNetPoolDestroy(int);
extern "C" int sceNetResolverCreate(const char*, int, int);
extern "C" int sceNetResolverDestroy(int);
extern "C" int sceNetResolverStartAton(int, const void*, char*, int, int, int, int);
extern "C" int sceNetResolverStartNtoa(int, const char*, void*, int, int, int);

// Capture the owned code's diagnostic_event calls (the real runtime's symbol).
static int g_events = 0;
static unsigned g_last = 0;
extern "C" void diagnostic_event(const char* tag, unsigned value)
{
    if (tag && std::string_view(tag) == "CAP10_NET_REFUSED") { ++g_events; g_last = value; }
}

static int checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

int main()
{
    CHECK(sceNetPoolCreate("p", 0x4000, 0) == -1);
    CHECK(sceNetPoolDestroy(3) == -1);
    CHECK(sceNetResolverCreate("r", 3, 0) == -1);
    CHECK(sceNetResolverDestroy(3) == -1);
    CHECK(sceNetResolverStartAton(3, nullptr, nullptr, 0, 0, 0, 0) == -1);
    CHECK(sceNetResolverStartNtoa(3, "host", nullptr, 0, 0, 0) == -1);
    int* e = sceNetErrnoLoc();
    CHECK(e != nullptr);
    CHECK(*e == ENETDOWN);  // readable, offline errno
    // Seven distinct functions each refused once -> seven events.
    CHECK(g_events == 7);
    // Calling again must NOT emit again (first-refusal-only).
    (void)sceNetPoolCreate("p", 0, 0);
    (void)sceNetResolverStartNtoa(3, "host", nullptr, 0, 0, 0);
    CHECK(g_events == 7);
    std::printf("test_offline_net items_checked=%d PASS\n", checks);
    return checks == 0 ? 2 : 0;
}
