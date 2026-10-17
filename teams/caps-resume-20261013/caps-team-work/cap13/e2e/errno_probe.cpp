// errno probe: call the real cap13::pe_load and report errno after refusal.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include "pe_loader.h"

class Dummy : public cap13::ImportResolver {
  public:
    void* resolve(const char*, const char*, uint64_t) override { return (void*)0x1234; }
};

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(n);
    if (n) fread(buf.data(), 1, n, f);
    fclose(f);
    Dummy r;
    cap13::LoadOptions o; o.allow_rwx_partition = true;
    errno = 0;
    cap13::LoadResult lr = cap13::pe_load(buf.data(), buf.size(), r, o);
    printf("ok=%d reason='%s' missing=%zu\n", (int)lr.ok, lr.reason.c_str(), lr.missing.size());
    printf("errno after pe_load = %d (%s)\n", errno, strerror(errno));
    return 0;
}
