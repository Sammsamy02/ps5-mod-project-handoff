// pred_host.cpp — host test for the vendored batch1 GUA predicate
// (townfall_layout_probes, byte-identical to worker-5) in the igf4 anchor
// probe's usage shapes:
//   1. synthetic layout-A image (FUObjectArray outer)  -> Validated, A
//   2. synthetic layout-B image (TUObjectArray direct) -> Validated, B
//   3. header-span-only feed, pointer target outside the 96-byte span
//      -> Refuted GOA_R_PTR_OUTSIDE_READABLE (documents the expected cfgH
//         outcome shape for a heap-backed GUA; the console probe records
//         this honestly under cfgH and runs cfgL for the real verdict)
//   4. code-shape classifier on the EXPECTED retained-image bytes
//      -> CODE_I_PROLOGUE_LIKE (corroborates the byte anchors)
#include "townfall_layout_probes.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace tlp;

static int failures;

#define CHECK(cond, msg)                                             \
    do {                                                             \
        if (!(cond)) {                                               \
            std::printf("FAIL %s\n", msg);                           \
            ++failures;                                              \
        } else {                                                     \
            std::printf("ok   %s\n", msg);                           \
        }                                                            \
    } while (0)

// Build a synthetic image: base 0x400000. The GUA header sits at vaddr
// 0x1000; chunk-pointer array at 0x2000; chunk data at 0x3000.
struct Synth {
    std::vector<uint8_t> bytes;
    uint64_t base = 0x400000;
    uint64_t gua = 0x1000;
    uint64_t ptrtab = 0x2000;
    uint64_t chunk = 0x3000;

    Synth() : bytes(0x10000, 0) {}

    void wr64(uint64_t off, uint64_t v) {
        for (int i = 0; i < 8; ++i)
            bytes[off + i] = (uint8_t)((v >> (8 * i)) & 0xff);
    }
    void wr32(uint64_t off, uint32_t v) {
        for (int i = 0; i < 4; ++i)
            bytes[off + i] = (uint8_t)((v >> (8 * i)) & 0xff);
    }

    // Layout A: FUObjectArray outer with the embedded TUObjectArray at +0x10.
    void fill_a(int32_t num_elements) {
        wr32(gua + 0x00, 0);            // ObjFirstGCIndex = 0
        wr32(gua + 0x04, 100);          // ObjLastNonGCIndex = 100
        wr32(gua + 0x08, 1);            // (OpenForDisregardForGC = 1)
        wr64(gua + 0x10, base + ptrtab);  // ObjObjects.Objects
        wr64(gua + 0x18, 0);            // PreAllocatedObjects = null
        wr32(gua + 0x20, 4194304);      // MaxElements = 64*65536
        wr32(gua + 0x24, num_elements); // NumElements
        wr32(gua + 0x28, 64);           // MaxChunks
        wr32(gua + 0x2c, 1);            // NumChunks
        wr64(ptrtab, base + chunk);
        const uint32_t need =
            (uint32_t)num_elements * 0x18;  // FUObjectItem stride
        for (uint32_t i = 0; i < need; ++i) bytes[chunk + i] = 0x7e;
    }

    // Layout B: bare TUObjectArray at the label.
    void fill_b(int32_t num_elements) {
        wr64(gua + 0x00, base + ptrtab);  // Objects
        wr64(gua + 0x08, 0);              // PreAllocatedObjects
        wr32(gua + 0x10, 4194304);
        wr32(gua + 0x14, num_elements);
        wr32(gua + 0x18, 64);
        wr32(gua + 0x1c, 1);
        wr64(ptrtab, base + chunk);
        const uint32_t need = (uint32_t)num_elements * 0x18;
        for (uint32_t i = 0; i < need; ++i) bytes[chunk + i] = 0x7e;
    }

    GuaProbeResult probe() {
        MemSpan s{base, bytes.size(), bytes.data()};
        return probe_guobject_array(&s, 1, base, gua);
    }
};

int main() {
    // 1. layout A unambiguous
    {
        Synth im;
        im.fill_a(200);
        GuaProbeResult r = im.probe();
        CHECK(r.verdict == Verdict::Validated, "A: validated");
        CHECK(r.observed.layout_matched == GuaLayout::FUObjectArray_Outer,
              "A: matched layout is FUObjectArray_Outer");
        CHECK(r.observed.num_elements == 200, "A: num_elements 200");
        CHECK(r.observed.max_elements == 4194304, "A: max_elements 4194304");
        CHECK(r.observed.chunk0_ptr == im.base + im.chunk, "A: chunk0 ptr");
        CHECK(r.layout_b.verdict == Verdict::Refuted, "A: layout B refuted");
    }
    // 2. layout B unambiguous
    {
        Synth im;
        im.fill_b(200);
        GuaProbeResult r = im.probe();
        CHECK(r.verdict == Verdict::Validated, "B: validated");
        CHECK(r.observed.layout_matched == GuaLayout::TUObjectArray_Direct,
              "B: matched layout is TUObjectArray_Direct");
        CHECK(r.layout_a.verdict == Verdict::Refuted, "B: layout A refuted");
    }
    // 3. header-span-only feed with the pointer target outside the span
    {
        Synth im;
        im.fill_a(200);
        MemSpan hs{im.base + im.gua, 96, im.bytes.data() + im.gua};
        GuaProbeResult r = probe_guobject_array(&hs, 1, im.base, im.gua);
        CHECK(r.verdict == Verdict::Refuted,
              "cfgH shape: header-only feed refutes (ptr outside span)");
        bool saw_outside = false;
        for (int i = 0; i < r.layout_a.code_count; ++i)
            if (r.layout_a.codes[i] == RCode::GOA_R_PTR_OUTSIDE_READABLE)
                saw_outside = true;
        CHECK(saw_outside, "cfgH shape: rcode GOA_R_PTR_OUTSIDE_READABLE");
    }
    // 4. code-shape classifier on the retained-image expected bytes
    {
        // Tick/SCO expected prologue: 55 48 89 e5 41 57 41 56 41 55 41 54 53
        // 48 81 ec (push rbp; mov rbp,rsp; push r15/r14/r13/r12/rbx;
        // sub rsp,imm32) — the file-image bytes at profile vaddr 0.
        const uint8_t prologue[16] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57,
                                      0x41, 0x56, 0x41, 0x55, 0x41, 0x54,
                                      0x53, 0x48, 0x81, 0xec};
        std::vector<uint8_t> img(0x100, 0xcc);
        for (int i = 0; i < 16; ++i) img[0x40 + i] = prologue[i];
        MemSpan s{0x400000, img.size(), img.data()};
        CodeProbeResult r = probe_code_shape(&s, 1, 0x400000, 0x40, nullptr);
        CHECK(r.verdict == Verdict::Inconclusive,
              "code shape stays Inconclusive-class by design");
        bool like = false;
        for (int i = 0; i < r.code_count; ++i)
            if (r.codes[i] == RCode::CODE_I_PROLOGUE_LIKE) like = true;
        CHECK(like, "expected prologue classifies CODE_I_PROLOGUE_LIKE");
    }

    if (failures) {
        std::printf("PRED-HOST-FAILURES: %d\n", failures);
        return 1;
    }
    std::printf("PRED-HOST-CHECKS-PASSED\n");
    return 0;
}
