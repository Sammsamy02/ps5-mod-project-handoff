// vmmap_walk.c — v3 NEW (worker-ingame-probe-v3, team 2026-10-04).
//
// Pure re-derivation of the KERN_PROC_VMMAP buffer walk. ROOT CAUSE of the
// v1/v2 corruption (verified this session against the pinned SDK header
// outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/
// target/include/sys/user.h AND the actual v2 log text): the SDK's
//
//   struct kinfo_vmentry {
//       int kve_structsize;   // "Variable size of record."
//       int kve_type;
//       uint64_t kve_start; kve_end; kve_offset; kve_vn_fileid;
//       uint32_t kve_vn_fsid; int kve_flags, kve_resident,
//                 kve_private_resident, kve_protection, kve_ref_count,
//                 kve_shadow_count, kve_vn_type;
//       uint64_t kve_vn_size; uint32_t kve_vn_rdev;
//       uint16_t kve_vn_mode, kve_status;
//       int _kve_ispare[12];
//       char kve_path[PATH_MAX];  // "Truncated before copyout in sysctl"
//   };  // sizeof == 1160 on the pinned SDK
//
// v1/v2 walked the buffer with a FIXED sizeof(struct kinfo_vmentry)=1160
// stride, which includes the full PATH_MAX path region. Real kernel records
// carry their own kve_structsize (variable; the path is truncated before
// copyout), so the fixed stride periodically re-aligns with real records —
// exactly the observed v2 pattern: entries 0/1/17/18/19 parse perfectly,
// the rest read path bytes and spare fields (zeros, 0x90, addresses as
// prot, repeating u32-pair patterns like (1,1),(1,3),(2,3),(0x18,0x18),
// (0x110,0),(0x24e7,3) — small ints, not addresses).
//
// This walk strides by each record's OWN kve_structsize with strict sanity
// bounds (mode 1). A fixed-stride walk (mode 2 = the exact v1/v2 behavior)
// is retained ONLY as a flagged fallback for comparison; raw first-record
// headers (the full 136 pre-path bytes) are snapshotted for offline
// re-derivation if the kernel layout diverges from the SDK header.
//
// Freestanding-pure: no OS calls, no allocation, byte-assembled loads only
// (no struct casts — record boundaries are 4-aligned, not 8-aligned).
// Host-unit-tested by tests/vmmap_walk_host.c (synthetic records with
// variable kve_structsize and 36-bit addresses; fallback walk compared).
#include "factstore.h"

/* Field offsets inside kinfo_vmentry per the pinned SDK header (bytes). */
#define KVE_STRUCTSIZE 0
#define KVE_START      8
#define KVE_END        16
#define KVE_PROT       56

static uint32_t ld32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t ld64(const uint8_t *p) {
    return (uint64_t)ld32(p) | ((uint64_t)ld32(p + 4) << 32);
}

uint32_t ig_vmmap_walk(const uint8_t *buf, uint64_t len, uint32_t kve_sizeof,
                       int mode, IgSpan *out, uint32_t cap, IgVmStats *st,
                       uint8_t *raws, uint32_t raw_cap) {
    uint32_t kept = 0;
    st->total = st->retained = st->truncated = st->insane = 0;
    st->raws_kept = 0;
    st->walk_mode = mode;
    st->stop_reason = 0;
    st->stop_off = 0;
    st->ksz_full = st->ksz_var = st->aligned16k = st->ksz_144 = 0;
    if (kve_sizeof < IG_VM_HDR) kve_sizeof = IG_VM_HDR;
    uint64_t off = 0;
    while (off + IG_VM_HDR <= len) {
        uint32_t ksz = ld32(buf + off);
        if (mode == 2) ksz = kve_sizeof;   // fixed stride: the exact v1/v2 walk
        if (off + ksz > len) {
            /* Record header present but the claimed record runs past the
               buffer: partial tail, distinguished from an insane structsize
               (mode 2 lands here BY CONSTRUCTION; mode 1 landing here means
               the sysctl buffer itself was cut mid-record — worth knowing). */
            st->stop_reason = 2;
            break;
        }
        if (ksz < IG_VM_HDR || ksz > kve_sizeof || (ksz & 3u) != 0) {
            st->stop_reason = 1;   // insane/short structsize: stop, never guess
            break;
        }
        /* Raw evidence: the full pre-path header of the first records. */
        if (raws && st->raws_kept < raw_cap) {
            uint8_t *dst = raws + (size_t)st->raws_kept * IG_VM_HDR;
            for (uint32_t i = 0; i < IG_VM_HDR; ++i) dst[i] = buf[off + i];
            ++st->raws_kept;
        }
        if (ksz == kve_sizeof) ++st->ksz_full; else ++st->ksz_var;
        if (ksz == 144) ++st->ksz_144;   /* prediction P1 (span-arbitration) */
        uint64_t start = ld64(buf + off + KVE_START);
        uint64_t end = ld64(buf + off + KVE_END);
        uint32_t prot = ld32(buf + off + KVE_PROT);
        ++st->total;
        /* Per-record sanity: a real mapping has start<=end, sane size and a
           SMALL protection word (v2's garbage prots like 0xff88c000 came
           from wrong offsets, not real protections). */
        int sane = start <= end && (end - start) <= (1ull << 44) &&
                   prot <= 0xffu;
        if (!sane) {
            ++st->insane;
        } else {
            ++st->retained;
            if (kept < cap) {
                IgSpan *s = &out[kept++];
                s->start = start;
                s->end = end;
                s->prot = prot;
                if (((start | end) & 0x3fffu) == 0) ++st->aligned16k;
            } else {
                ++st->truncated;
            }
        }
        off += ksz;
    }
    st->stop_off = off;
    return kept;
}

int ig_vmmap_trustworthy(const IgVmStats *st) {
    /* Enough sane records and <=20% insane ones. The v2 log's fixed-stride
       walk kept 1024 "spans" of which only a handful were real; that walk
       would fail the sane-shape checks here (start>end, huge prots) and the
       gate keeps every foreign deref refused when no walk passes. */
    return st->retained >= 8 && st->insane * 4 <= st->retained;
}
