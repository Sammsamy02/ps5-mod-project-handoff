#include "guest_pe_loader.h"
#include "win_api_shim.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
class RefuseSegment final : public cap13::GuestSegmentSwitch
{
public:
    bool enter(void*) override { return false; }
    void leave() noexcept override {}
};
static bool allocate(size_t minimum, void** address, size_t* size, void*)
{
    size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* memory = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (memory == MAP_FAILED) return false;
    *address = memory;
    *size = page;
    return true;
}
static bool seal(void* memory, size_t size, void*)
{
    return mprotect(memory, size, PROT_READ | PROT_EXEC) == 0;
}
static void release(void* memory, size_t size, void*) { munmap(memory, size); }
int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate;
    backend.seal_executable = seal;
    backend.release = release;
    if (!shim.set_executable_backend(backend)) return 2;
    cap13::LoadOptions options;
    options.page_size = 0x1000;
    options.allow_rwx_partition = true;
    cap13::StaticTlsBank bank;
    RefuseSegment segment;
    for (int i = 1; i < argc; ++i)
    {
        std::ifstream file(argv[i], std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        auto prepared = cap13::pe_prepare_guest_with_shim(
            bytes.data(), bytes.size(), shim, options, bank, segment, argv[i]);
        std::printf("FILE %s ok=%d reason=%s missing=%zu\n", argv[i],
                    prepared.ok, prepared.reason.c_str(), prepared.missing.size());
        for (const auto& name : prepared.missing)
            std::printf("  %s\n", name.c_str());
    }
    return 0;
}
