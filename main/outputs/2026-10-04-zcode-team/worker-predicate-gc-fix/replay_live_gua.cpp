
// replay_live_gua.cpp — offline replay of the captured GUObjectArray bytes.
// Header bytes are the EXACT 96 bytes read at 0x97ba110 in the live target
// (image_base 0x400000, GOA vaddr 0x93ba110, span [0x970c000,0x9a34000) RW).
// cfgH          = captured span only (no chunk memory retained).
// cfgL-recon    = captured header + RECONSTRUCTED chunk scaffolding at the
//                 observed Objects pointer 0x13800220a0 so the full P1-P5
//                 chain runs; scaffolding is NOT live capture.
// Compiled twice: against vendored-original (buggy P3, historical evidence)
// and vendored-fixed (corrected UE invariant, pending evaluation).
#include "townfall_layout_probes.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tlp;
static const uint8_t kLive[96] = {0x95, 0x8a, 0x00, 0x00, 0x94, 0x8a, 0x00, 0x00, 0x95, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x20, 0x02, 0x80, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x17, 0x19, 0x01, 0x00, 0x21, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x06, 0x01, 0x80, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xd7, 0x32, 0x40, 0x15, 0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static void run_cfg(const char* label, const MemSpan* spans, size_t n) {
    GuaProbeResult r = probe_guobject_array(spans, n, 0x400000ull, 0x93ba110ull);
    std::printf("%s combined=%s codes=[", label, verdict_string(r.verdict));
    for (uint8_t i = 0; i < r.code_count; ++i)
        std::printf("%s%s", i ? "," : "", rcode_string(r.codes[i]));
    std::printf("] layout_a=%s(%s) layout_b=%s(%s)",
                verdict_string(r.layout_a.verdict), rcode_string(r.layout_a.codes[0]),
                verdict_string(r.layout_b.verdict), rcode_string(r.layout_b.codes[0]));
    std::printf(" first=%d last=%d num=%d max=%d chunks=%d/%d matched=%d\n",
                r.observed.obj_first_gc_index, r.observed.obj_last_non_gc_index,
                r.observed.num_elements, r.observed.max_elements,
                r.observed.num_chunks, r.observed.max_chunks,
                (int)r.observed.layout_matched);
}
int main() {
    std::vector<uint8_t> hdrmem(0x328000u, 0);
    std::memcpy(hdrmem.data() + (size_t)(0x97ba110ull - 0x970c000ull), kLive, 96);
    std::printf("=== cfgH: captured header span [0x970c000,0x9a34000) only ===\n");
    { MemSpan sp{0x970c000ull, 0x328000ull, hdrmem.data()}; run_cfg("cfgH", &sp, 1); }
    const uint64_t c0 = 0x20000000ull, c1 = 0x30000000ull;
    std::vector<uint8_t> objarr(16, 0);
    for (int i = 0; i < 8; ++i) {
        objarr[(size_t)i] = (uint8_t)(c0 >> (8 * i));
        objarr[8 + (size_t)i] = (uint8_t)(c1 >> (8 * i));
    }
    std::vector<uint8_t> chunk0((size_t)65536 * kFuObjectItemSize, 0);
    std::vector<uint8_t> chunk1((size_t)(71959 - 65536) * kFuObjectItemSize, 0);
    std::printf("=== cfgL: captured header + reconstructed chunk scaffolding ===\n");
    MemSpan sp[4]{
        {0x970c000ull, 0x328000ull, hdrmem.data()},
        {0x13800220a0ull, 16, objarr.data()},
        {c0, chunk0.size(), chunk0.data()},
        {c1, chunk1.size(), chunk1.data()},
    };
    run_cfg("cfgL", sp, 4);
    return 0;
}
