// Host test for r7 multi-window identity gate + kve_path extraction.
#include <cstdint>
#include <cstdio>
#include <cstring>
struct OsVmMapping { uint64_t start, end; uint32_t protection; };
static constexpr uint64_t kBase = 0x400000;
static const unsigned char WE[64] = {0x63,0x7c,0x77,0x7b};  // prefix suffices w/ short len overrides
static const unsigned char WD[19] = {'C','u','r','r','e','n','t',' ','C','h','a','n','g','e','l','i','s','t',0};
static const unsigned char WB[8] = {0x55,0,0x45,0,0x35,0,0,0};
struct A { uint64_t vaddr; uint32_t len; const unsigned char *b; bool req; };
// gate replica of r7 semantics
static bool gate(const A *anchors, size_t n, OsVmMapping *spans, size_t ns,
                 const unsigned char *mem, uint64_t mem_base, bool *ok) {
    for (size_t w = 0; w < n; ++w) ok[w] = false;
    for (size_t w = 0; w < n; ++w) {
        uint64_t rt = kBase + anchors[w].vaddr;
        bool cov = false;
        for (size_t i = 0; i < ns; ++i)
            if ((spans[i].protection & 1) && spans[i].start <= rt &&
                rt + anchors[w].len <= spans[i].end) { cov = true; break; }
        if (!cov) continue;
        const unsigned char *p = mem + (rt - mem_base);
        bool m = true;
        for (uint32_t i = 0; i < anchors[w].len; ++i) if (p[i] != anchors[w].b[i]) { m = false; break; }
        ok[w] = m;
    }
    if (ok[3] || ok[4]) { ok[3] = ok[4] = true; }
    bool all = true;
    for (size_t w = 0; w < n; ++w) if (anchors[w].req && !ok[w]) all = false;
    return all;
}
int fails = 0;
void chk(const char *n, bool ok) { printf("%s %s\n", ok?"PASS":"FAIL", n); if(!ok) fails++; }
int main() {
    // memory: one readable span [0x818c000,0x100118000) containing windows
    static unsigned char mem[0x800000]; memset(mem, 0, sizeof mem);
    uint64_t mem_base = 0x7c00000;
    auto put = [&](uint64_t rt, const unsigned char *b, uint32_t n) { memcpy(mem + (rt - mem_base), b, n); };
    put(0x81d2a40, WE, 4); put(0x7f8845d, WD, 19); put(0x7c26782, WB, 8);
    // NOTE: D at 0x7f8845d is NOT inside either span above (0x7f8845d < 0x818c000): add covering span
    OsVmMapping spans2[3] = {{0x400000, 0x787c000, 4}, {0x7f00000, 0x818c000, 3}, {0x818c000, 0x100118000, 1}};
    A anchors[6] = {{0x7dd2a40, 4, WE, true}, {0x7b8845d, 19, WD, true}, {0x7826782, 8, WB, true},
                    {0x75cffda, 32, nullptr, false}, {0x768ce58, 32, nullptr, false}, {0xbc5f250, 36, nullptr, false}};
    bool ok[6];
    chk("all required match -> identity", gate(anchors, 6, spans2, 3, mem, mem_base, ok) == true);
    // flip D byte -> refuse
    mem[(0x7f8845d - mem_base) + 0] ^= 0xff;
    chk("D mismatch -> refused", gate(anchors, 6, spans2, 3, mem, mem_base, ok) == false);
    mem[(0x7f8845d - mem_base) + 0] ^= 0xff;
    // B uncovered (drop its span coverage by moving it out) -> refuse
    OsVmMapping spans3[3] = {{0x400000, 0x787c000, 4}, {0x7f00000, 0x818c000, 3}, {0x81d2a40, 0x100118000, 1}};
    chk("B uncovered -> refused", gate(anchors, 6, spans3, 3, mem, mem_base, ok) == false);
    // A never gates: A uncovered in all cases above yet gate can pass
    chk("A irrelevant to gate", gate(anchors, 6, spans2, 3, mem, mem_base, ok) == true);
    // kve_path extraction replica: record covering 0x400000 with path at 136
    unsigned char rec[1160]; memset(rec, 0, sizeof rec);
    uint32_t ksz = 144; uint64_t s = 0x400000, e = 0x787c000; const char *path = "/app0/eboot.bin";
    memcpy(rec, &ksz, 4); memcpy(rec+8, &s, 8); memcpy(rec+16, &e, 8); memcpy(rec+136, path, 16);
    uint64_t off2 = 0; char out[1040] = {0}; bool valid = false;
    while (off2 + 64 <= 144) {
        memcpy(&ksz, rec+off2, 4);
        memcpy(&s, rec+off2+8, 8); memcpy(&e, rec+off2+16, 8);
        if (s <= kBase && kBase < e && ksz > 136) {
            const unsigned char *pb = rec + off2 + 136; uint64_t cap = ksz - 136, n = 0;
            while (n < cap && pb[n] != 0) ++n;
            if (n > 0 && n < cap) { memcpy(out, pb, n); valid = out[0] == '/'; }
            break;
        }
        off2 += ksz;
    }
    chk("kve_path extracted", valid && !strcmp(out, "/app0/eboot.bin"));
    // truncated-no-NUL path -> invalid
    memset(rec+136, 'x', 8); rec[144-136+136]=0; // fill to record end w/o NUL
    uint32_t ksz2 = 144; memcpy(rec, &ksz2, 4);
    // path region is 8 bytes all 'x', no NUL within cap
    valid = false; out[0]=0; off2=0;
    { memcpy(&ksz, rec+0,4); memcpy(&s,rec+8,8); memcpy(&e,rec+16,8);
      const unsigned char *pb = rec+136; uint64_t cap = ksz-136, n=0;
      while (n < cap && pb[n] != 0) ++n;
      if (n > 0 && n < cap && pb[n]==0) { memcpy(out,pb,n); valid=true; } }
    chk("unterminated path -> refused", !valid);
    printf("RESULT: %s (%d failures)\n", fails?"FAILED":"ALL PASS", fails);
    return fails ? 1 : 0;
}
