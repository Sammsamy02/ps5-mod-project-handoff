// Cap10OfflineNet.cpp -- owned, fail-closed SceNet resolver entry points (#10).
//
// Why: linking the C# runtime pulls the owned libc's netdb.o (getaddrinfo,
// gethostbyname, getnameinfo, ...), whose resolver calls seven libSceNet
// functions. Project rule: modded games stay offline. Defining the seven
// names here resolves them inside the runtime, so the PRX gains NO SceNet
// import, and every name lookup fails closed with ENETDOWN.
//
// ABI (two sources): (1) the only caller in the link, the owned libc's
// netdb.o (resolve_aton/resolve_ntoa: create/start return int checked with
// `js`; sceNetErrnoLoc's result is dereferenced as a 32-bit int); (2) the
// payload SDK stub libSceNet.so exports all seven as FUNC symbols
// (cap10/sdk-evidence/sdk-libSceNet.nm). The refusals ignore their
// arguments, never write through them, and return -1 (or the errno slot).
//
// Each function reports its first refusal once per process through the
// runtime's existing diagnostic_event channel ("CAP10_NET_REFUSED", value =
// function id 1..7), so a C# mod that tries the network is visible in the log.
// Visibility: the source asks for hidden, but on the PS5 target the toolchain
// flag -fvisibility-nodllstorageclass=default leaves these symbols default in
// the object (reviewer finding, verified in the target .o). That is harmless:
// the PRX export table is decided by the packager (R15e exports only
// module_start/module_stop) and libSceNet is not a needed module. APPLY.md adds
// a post-package check: exactly those 2 exports and 0 sceNet rows either way.
#include <atomic>
#include <cerrno>
#include <cstdint>

extern "C" void diagnostic_event(const char*, unsigned int);

namespace
{
    std::atomic<unsigned> g_reported{0};
    // Read-only after static initialization (constant-initialized, no
    // constructor). netdb.o only reads through the returned pointer.
    int g_offline_errno = ENETDOWN;

    enum : unsigned
    {
        kErrnoLoc = 1,
        kPoolCreate = 2,
        kPoolDestroy = 3,
        kResolverCreate = 4,
        kResolverDestroy = 5,
        kResolverStartAton = 6,
        kResolverStartNtoa = 7,
    };

    void refuse_once(unsigned id) noexcept
    {
        const unsigned bit = 1u << id;
        if ((g_reported.fetch_or(bit, std::memory_order_relaxed) & bit) != 0) return;
        try { diagnostic_event("CAP10_NET_REFUSED", id); }
        catch (...) {}
    }
}

#define CAP10_OFFLINE extern "C" __attribute__((visibility("hidden")))

CAP10_OFFLINE int* sceNetErrnoLoc() noexcept
{
    refuse_once(kErrnoLoc);
    return &g_offline_errno;
}

CAP10_OFFLINE int sceNetPoolCreate(const char*, int, int) noexcept
{
    refuse_once(kPoolCreate);
    return -1;
}

CAP10_OFFLINE int sceNetPoolDestroy(int) noexcept
{
    refuse_once(kPoolDestroy);
    return -1;
}

CAP10_OFFLINE int sceNetResolverCreate(const char*, int, int) noexcept
{
    refuse_once(kResolverCreate);
    return -1;
}

CAP10_OFFLINE int sceNetResolverDestroy(int) noexcept
{
    refuse_once(kResolverDestroy);
    return -1;
}

CAP10_OFFLINE int sceNetResolverStartAton(int, const void*, char*, int, int, int, int) noexcept
{
    refuse_once(kResolverStartAton);
    return -1;
}

CAP10_OFFLINE int sceNetResolverStartNtoa(int, const char*, void*, int, int, int) noexcept
{
    refuse_once(kResolverStartNtoa);
    return -1;
}
