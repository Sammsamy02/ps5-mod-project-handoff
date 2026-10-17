// Replicates e2e_host's exec-page + stamping resolver around the real pe_load.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/mman.h>
#include "pe_loader.h"
#include "win64_bridge.h"

static uint8_t* g_exec_page;
static void* make_tramp(void* body) {
    const void* th = cap13::bridge_select_thunk(4);
    cap13::bridge_stamp_tramp(g_exec_page, body, th);
    return g_exec_page;
}
class R : public cap13::ImportResolver {
  public:
    void* resolve(const char* d, const char* s, uint64_t o) override {
        return make_tramp((void*)&R::noop);
    }
    static int64_t noop(void*, uint64_t) { return 1; }
};
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    g_exec_page = (uint8_t*)mmap(0, 0x4000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    printf("exec_page=%p\n", g_exec_page);
    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(n);
    if (n) fread(buf.data(), 1, n, f);
    fclose(f);
    R r;
    cap13::LoadOptions o; o.allow_rwx_partition = true;
    errno = 0;
    cap13::LoadResult lr = cap13::pe_load(buf.data(), buf.size(), r, o);
    printf("ok=%d reason='%s'\n", (int)lr.ok, lr.reason.c_str());
    printf("errno after pe_load = %d (%s)\n", errno, strerror(errno));
    return 0;
}
