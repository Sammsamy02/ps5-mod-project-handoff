#include "nexus_bridge.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

static bool fail_allocation = false;
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }

int main() {
    static const unsigned char body[] =
        "[{\"id\":444,\"domain_name\":\"newgame2028\",\"name\":\"Fixture\"}]";
    char output[2048] = {'x'};
    std::size_t length = 123;
    fail_allocation = true;
    assert(mb_nexus_decode(MB_NEXUS_GAMES, body, sizeof(body)-1, nullptr, 0, 0,
                           output, sizeof(output), &length)==MB_NEXUS_INTERNAL_ERROR);
    assert(length==0 && output[0]==0);
    output[0]='x'; length=123;
    assert(mb_nexus_metadata_url(MB_NEXUS_FILES, "future_game-2", 13, 71,
                                 output, MB_NEXUS_URL_LIMIT_BYTES, &length)==MB_NEXUS_INTERNAL_ERROR);
    assert(length==0 && output[0]==0);
    fail_allocation = false;
    assert(mb_nexus_metadata_url(MB_NEXUS_FILES, "future_game-2", 13, 71,
                                 output, MB_NEXUS_URL_LIMIT_BYTES, &length)==MB_NEXUS_OK);
    assert(!std::strcmp(output, "https://api.nexusmods.com/v1/games/future_game-2/mods/71/files"));
    assert(mb_nexus_decode(MB_NEXUS_GAMES, body, sizeof(body)-1, nullptr, 0, 0,
                           output, sizeof(output), &length)==MB_NEXUS_OK);
    assert(length > 0 && std::strlen(output)==length);
    assert(std::strstr(output, "\"status\":\"decoded\"") != nullptr);
}
