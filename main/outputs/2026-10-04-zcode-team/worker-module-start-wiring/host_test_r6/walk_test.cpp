// Host test for bridge r6's pure vmmap walk contract (user directive 1).
#include <cstdint>
#include <cstdio>
#include <cstring>
struct OsVmMapping { uint64_t start, end; uint32_t protection; };
static constexpr uint32_t kKveHdr = 64;
static uint32_t ld32b(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint64_t ld64b(const unsigned char *p) { return (uint64_t)ld32b(p) | ((uint64_t)ld32b(p+4)<<32); }
static void bridge_vmmap_walk(const unsigned char *buf, uint64_t len, OsVmMapping *out, size_t capacity,
                              size_t *entries_out, uint32_t *stop_reason) {
    const uint32_t kve_sizeof = 1160; size_t entries = 0; uint32_t stop = 0; uint64_t off = 0;
    while (off + kKveHdr <= len) {
        const uint32_t ksz = ld32b(buf + off);
        if (ksz < kKveHdr || ksz > kve_sizeof || (ksz & 3u) != 0) { stop = 1; break; }
        if (off + ksz > len) { stop = (ksz <= kve_sizeof) ? 2u : 1u; break; }
        const uint64_t s = ld64b(buf + off + 8), e = ld64b(buf + off + 16);
        const uint32_t prot = ld32b(buf + off + 56);
        if (out != nullptr && entries < capacity) { out[entries] = {s, e, prot}; }
        ++entries; off += ksz;
    }
    if (stop == 0 && off != len) stop = 2;
    *entries_out = entries; *stop_reason = stop;
}
static unsigned char rec[1160];
static void mkrec(unsigned char *buf, uint32_t ksz, uint64_t s, uint64_t e, uint32_t prot) {
    memset(buf, 0, 1160); memcpy(buf, &ksz, 4); memcpy(buf+8, &s, 8); memcpy(buf+16, &e, 8); memcpy(buf+56, &prot, 4);
}
int fails = 0;
void chk(const char *name, bool ok) { printf("%s %s\n", ok?"PASS":"FAIL", name); if(!ok) fails++; }
int main() {
    unsigned char b[4096]; OsVmMapping m[8]; size_t ent; uint32_t stop;
    // 1 clean: two records, buffer exactly full
    mkrec(rec,144,0x400000,0x787c000,4); memcpy(b,rec,144);
    mkrec(rec,144,0x8018c000,0x100118000,1); memcpy(b+144,rec,144);
    bridge_vmmap_walk(b,288,m,8,&ent,&stop);
    chk("clean: stop=0 ent=2", stop==0 && ent==2);
    chk("clean: fields parsed", m[0].start==0x400000 && m[0].end==0x787c000 && m[0].protection==4 && m[1].protection==1);
    // 2 partial tail: header claims 144, only 100 remain after first record
    memcpy(b+144,rec,100); // first 100 bytes of a 144-record header
    bridge_vmmap_walk(b,288-44,m,8,&ent,&stop);
    chk("partial tail: stop=2 ent=1", stop==2 && ent==1);
    // 3 trailing fragment < 64
    memcpy(b+288,rec,10);
    bridge_vmmap_walk(b,298,m,8,&ent,&stop);
    chk("sub-64 fragment: stop=2 ent=2", stop==2 && ent==2);
    // 4 insane ksz mid-buffer
    mkrec(rec,144,1,2,3); memcpy(b,rec,144);
    mkrec(rec,40,9,9,9); memcpy(b+144,rec,64); // insane size 40
    bridge_vmmap_walk(b,208,m,8,&ent,&stop);
    chk("insane mid: stop=1 ent=1", stop==1 && ent==1);
    // 5 insane first
    bridge_vmmap_walk(b+144,64,m,8,&ent,&stop);
    chk("insane first: stop=1 ent=0", stop==1 && ent==0);
    // 6 oversized ksz
    mkrec(rec,1164,1,2,3); memcpy(b,rec,64);
    bridge_vmmap_walk(b,1164,m,8,&ent,&stop);
    chk("ksz>1160: stop=1", stop==1);
    // 7 misaligned ksz
    mkrec(rec,66,1,2,3); memcpy(b,rec,64);
    bridge_vmmap_walk(b,70,m,8,&ent,&stop);
    chk("ksz%4!=0: stop=1", stop==1);
    // 8 capacity clamp
    mkrec(rec,144,5,6,7); memcpy(b,rec,144); memcpy(b+144,rec,144);
    bridge_vmmap_walk(b,288,m,1,&ent,&stop);
    chk("capacity clamp: ent=2 copied=1", ent==2 && m[0].start==5 && stop==0);
    // 9 verified-subset: required mappings early, garbage deep later
    mkrec(rec,144,0x400000,0x787c000,4); memcpy(b,rec,144);
    mkrec(rec,144,0x8018c000,0x100118000,1); memcpy(b+144,rec,144);
    mkrec(rec,144,1,2,3); memcpy(b+288,rec,144);
    *(uint32_t*)(b+432) = 0xDEAD0000; // insane deep record
    bridge_vmmap_walk(b,432+64,m,8,&ent,&stop);
    chk("deep malformed: stop=1 ent=3 (prefix verified)", stop==1 && ent==3);
    printf("RESULT: %s (%d failures)\n", fails?"FAILED":"ALL PASS", fails);
    return fails ? 1 : 0;
}
