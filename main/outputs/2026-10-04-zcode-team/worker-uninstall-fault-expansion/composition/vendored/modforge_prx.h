/*
 * modforge_prx.h — check a PS5 module (.prx/.sprx) on the console and fix its
 * SDK stamp so it loads into a given game.
 *
 * Why: a module stamped with a different SDK than the game fails
 * sceKernelLoadStartModule with SCE_KERNEL_ERROR_ESDKVERSION (0x80020063).
 * Setting the module's PS5 SDK word to the game's (e.g. 0x11000000) is the
 * 4-byte edit that was verified to fix it. The host does this when it builds
 * bundles (modforge/ps5module.py); this header lets the console app do the
 * same for a module the user copied over by hand, or after a game update
 * changed the game's SDK.
 *
 * Same rules as modforge/ps5module.py, byte for byte (tests/test_prx.py and
 * tests/fuzz_c_readers.py compare the two):
 *
 *   container   7F 45 4C 46 plain ELF; 4F 15 3D 1D PS4 SELF; 54 14 F5 EE PS5
 *               SELF. A SELF's ELF header sits at 0x20 + 0x20 * entries.
 *   fake        SELF extended-info program type == 1 (what kstuff checks).
 *   encrypted   any SELF entry has the encrypted (0x2) or compressed (0x8) bit.
 *   param       first PT_SCE_MODULE_PARAM / PT_SCE_PROCPARAM program header,
 *               found in the file through the first has-blocks (0x800) SELF
 *               entry whose program header covers it; magic 0x3C13F4BF
 *               (module) or 'ORBI' (process) at +0x08; block version at
 *               +0x0C; PS4 SDK at +0x10; PS5 SDK at +0x14.
 *
 * Single header, C99, no libc calls, no allocation. File access goes through
 * one read callback, so a module never has to be loaded whole:
 *
 *   mfp_info info;
 *   const char *why;
 *   uint64_t at;
 *   unsigned char word[4];
 *   if (mfp_inspect(&io, file_size, &info, &why) < 0) { ... not a module: why ... }
 *   mfp_verdict v = mfp_assess(&info, game_sdk);       // e.g. 0x11000000
 *   if (v.problems) { ... won't load: mfp_problem_text(bit) ... }
 *   if (v.retarget && mfp_sdk_site(&info, &at) == 0) {
 *       mfp_put_u32le(word, game_sdk);
 *       ... write the 4 bytes of `word` at file offset `at` ...
 *   }
 */
#ifndef MODFORGE_PRX_H
#define MODFORGE_PRX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mfp_io {
    void *ctx;
    /* Read exactly n bytes at off into buf. Return n, or -1 on failure. */
    long (*read_at)(void *ctx, uint64_t off, void *buf, size_t n);
} mfp_io;

enum { MFP_ELF = 1, MFP_SELF_PS4, MFP_SELF_PS5 };
enum { MFP_PARAM_NONE = 0, MFP_PARAM_MODULE, MFP_PARAM_PROCESS };

#define MFP_ET_SCE_EXEC_ASLR   0xFE10u
#define MFP_ET_SCE_DYNAMIC     0xFE18u
#define MFP_PT_SCE_PROCPARAM   0x61000001u
#define MFP_PT_SCE_MODULE_PARAM 0x61000002u
#define MFP_MODULE_PARAM_MAGIC 0x3C13F4BFu
#define MFP_PROCESS_PARAM_MAGIC 0x4942524Fu   /* "ORBI" */
#define MFP_PARAM_MIN_SIZE     0x18u
#define MFP_PARAM_PS5_VERSION  2u

typedef struct {
    int container;          /* MFP_ELF / MFP_SELF_PS4 / MFP_SELF_PS5           */
    int fake;               /* -1 unknown (plain ELF or out of range), 0, 1     */
    int encrypted;          /* 1 if any SELF entry is encrypted or compressed   */
    unsigned elf_type;      /* e_type: 0xFE18 library, 0xFE10 executable        */
    unsigned abi_version;   /* e_ident[EI_ABIVERSION]: 2 on PS5 builds          */
    unsigned params_found;  /* parameter program headers present                */
    int param;              /* MFP_PARAM_NONE / _MODULE / _PROCESS              */
    uint64_t param_offset;  /* file offset of the parameter block               */
    uint64_t param_size;    /* +0x00                                            */
    uint32_t param_version; /* +0x0C                                            */
    uint32_t ps4_sdk;       /* +0x10                                            */
    uint32_t ps5_sdk;       /* +0x14                                            */
} mfp_info;

/* Assessment bits, matching ps5module.PROBLEMS / WARNINGS. */
#define MFP_P_NOT_LIBRARY   0x1u   /* "not-library"   */
#define MFP_P_ENCRYPTED     0x2u   /* "encrypted"     */
#define MFP_W_PLAIN_ELF     0x1u   /* "plain-elf"     */
#define MFP_W_NOT_FAKE      0x2u   /* "not-fake"      */
#define MFP_W_NO_SDK        0x4u   /* "no-sdk"        */
#define MFP_W_PS4_TOOLCHAIN 0x8u   /* "ps4-toolchain" */

typedef struct {
    unsigned problems;   /* certain failures: don't try to load it           */
    unsigned warnings;   /* untested, not known to fail                      */
    int retarget;        /* 1: write the game's SDK into the module first    */
} mfp_verdict;

static inline uint16_t mfp_le16(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t mfp_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static inline uint64_t mfp_le64(const unsigned char *p) {
    return (uint64_t)mfp_le32(p) | ((uint64_t)mfp_le32(p + 4) << 32);
}

static inline void mfp_put_u32le(unsigned char out[4], uint32_t v) {
    out[0] = (unsigned char)v;
    out[1] = (unsigned char)(v >> 8);
    out[2] = (unsigned char)(v >> 16);
    out[3] = (unsigned char)(v >> 24);
}

/* Read n bytes at off, refusing anything outside [0, size). */
static inline int mfp_get(const mfp_io *io, uint64_t size, uint64_t off, void *buf, size_t n) {
    if (off > size || size - off < n) return -1;
    return io->read_at(io->ctx, off, buf, n) == (long)n ? 0 : -1;
}

static inline int mfp_fail(const char **why, const char *msg) {
    if (why) *why = msg;
    return -1;
}

/* Program header fields we use. */
typedef struct { uint32_t type; uint64_t offset, filesz; } mfp_phdr;

static inline int mfp_read_phdr(const mfp_io *io, uint64_t size, uint64_t ph_start,
                         unsigned i, mfp_phdr *ph) {
    unsigned char b[0x38];
    if (mfp_get(io, size, ph_start + 0x38u * (uint64_t)i, b, sizeof b) < 0) return -1;
    ph->type = mfp_le32(b);
    ph->offset = mfp_le64(b + 0x08);
    ph->filesz = mfp_le64(b + 0x20);
    return 0;
}

/* File offset of `need` bytes at ELF offset `want`: 1 found, 0 not, -1 I/O. */
static inline int mfp_map(const mfp_io *io, uint64_t size, const mfp_info *info,
                   uint64_t ph_start, unsigned phnum, unsigned n_entries,
                   uint64_t want, uint64_t need, uint64_t *at) {
    unsigned e;
    if (info->container == MFP_ELF) {
        if (want > size || size - want < need) return 0;
        *at = want;
        return 1;
    }
    for (e = 0; e < n_entries; ++e) {
        unsigned char b[0x18];
        uint64_t props, eoff, efsz, delta;
        unsigned idx;
        mfp_phdr ph;
        if (mfp_get(io, size, 0x20u + 0x20u * (uint64_t)e, b, sizeof b) < 0) return -1;
        props = mfp_le64(b);
        eoff = mfp_le64(b + 8);
        efsz = mfp_le64(b + 0x10);
        idx = (unsigned)((props >> 20) & 0xFFFFu);
        if (!(props & 0x800u) || idx >= phnum) continue;
        if (mfp_read_phdr(io, size, ph_start, idx, &ph) < 0) return -1;
        if (ph.filesz != efsz) continue;
        if (want < ph.offset) continue;
        delta = want - ph.offset;
        if (delta > ph.filesz || ph.filesz - delta < need) continue;
        /* The first covering entry decides, in or out of the file. */
        if (eoff > size || delta > size - eoff || size - eoff - delta < need) return 0;
        *at = eoff + delta;
        return 1;
    }
    return 0;
}

/* Describe the module. Returns 0, or -1 with *why set when the file is not a
 * module (or a read failed). */
static inline int mfp_inspect(const mfp_io *io, uint64_t size, mfp_info *info, const char **why) {
    unsigned char h[0x40];
    uint64_t elf_off = 0, e_phoff, ph_start, ex, at = 0;
    unsigned n_entries = 0, phnum, i, phentsize;
    int first_param = -1, m;
    mfp_phdr ph;

    info->container = 0;
    info->fake = -1;
    info->encrypted = 0;
    info->elf_type = info->abi_version = info->params_found = 0;
    info->param = MFP_PARAM_NONE;
    info->param_offset = info->param_size = 0;
    info->param_version = info->ps4_sdk = info->ps5_sdk = 0;

    if (mfp_get(io, size, 0, h, 4) < 0) return mfp_fail(why, "not a PS5 module: no ELF or SELF magic");
    if (h[0] == 0x7F && h[1] == 'E' && h[2] == 'L' && h[3] == 'F') {
        info->container = MFP_ELF;
    } else if ((h[0] == 0x4F && h[1] == 0x15 && h[2] == 0x3D && h[3] == 0x1D) ||
               (h[0] == 0x54 && h[1] == 0x14 && h[2] == 0xF5 && h[3] == 0xEE)) {
        info->container = h[0] == 0x4F ? MFP_SELF_PS4 : MFP_SELF_PS5;
        if (mfp_get(io, size, 0, h, 0x20) < 0) return mfp_fail(why, "SELF header is truncated");
        n_entries = mfp_le16(h + 0x18);
        elf_off = 0x20u + 0x20u * (uint64_t)n_entries;
        if (mfp_get(io, size, elf_off, h, 4) < 0)
            return mfp_fail(why, "SELF entry table runs past the end of the file");
        if (!(h[0] == 0x7F && h[1] == 'E' && h[2] == 'L' && h[3] == 'F'))
            return mfp_fail(why, "SELF has no readable ELF header");
    } else {
        return mfp_fail(why, "not a PS5 module: no ELF or SELF magic");
    }

    if (mfp_get(io, size, elf_off, h, 0x40) < 0) return mfp_fail(why, "ELF header is truncated");
    if (h[4] != 2 || h[5] != 1) return mfp_fail(why, "not a 64-bit little-endian ELF");
    info->abi_version = h[8];
    info->elf_type = mfp_le16(h + 0x10);
    e_phoff = mfp_le64(h + 0x20);
    phentsize = mfp_le16(h + 0x36);
    phnum = mfp_le16(h + 0x38);
    if (phentsize != 0x38) return mfp_fail(why, "unexpected program header size");
    if (e_phoff > size || elf_off + e_phoff > size ||
        size - (elf_off + e_phoff) < 0x38u * (uint64_t)phnum)
        return mfp_fail(why, "program headers run past the end of the file");
    ph_start = elf_off + e_phoff;

    if (info->container != MFP_ELF) {
        for (i = 0; i < n_entries; ++i) {
            if (mfp_get(io, size, 0x20u + 0x20u * (uint64_t)i, h, 8) < 0)
                return mfp_fail(why, "read failed");
            if (mfp_le64(h) & 0xAu) info->encrypted = 1;
        }
        ex = (ph_start + 0x38u * (uint64_t)phnum + 15u) & ~(uint64_t)15u;
        if (ex <= size && size - ex >= 16) {
            if (mfp_get(io, size, ex + 8, h, 8) < 0) return mfp_fail(why, "read failed");
            info->fake = mfp_le64(h) == 1u;
        }
    }

    for (i = 0; i < phnum; ++i) {
        if (mfp_read_phdr(io, size, ph_start, i, &ph) < 0) return mfp_fail(why, "read failed");
        if (ph.type == MFP_PT_SCE_MODULE_PARAM || ph.type == MFP_PT_SCE_PROCPARAM) {
            if (first_param < 0) first_param = (int)i;
            info->params_found++;
        }
    }
    if (first_param < 0 || info->encrypted) return 0;
    if (mfp_read_phdr(io, size, ph_start, (unsigned)first_param, &ph) < 0)
        return mfp_fail(why, "read failed");
    if (ph.filesz < MFP_PARAM_MIN_SIZE) return 0;
    m = mfp_map(io, size, info, ph_start, phnum, n_entries, ph.offset, MFP_PARAM_MIN_SIZE, &at);
    if (m < 0) return mfp_fail(why, "read failed");
    if (m == 0) return 0;
    if (mfp_get(io, size, at, h, 0x18) < 0) return mfp_fail(why, "read failed");
    if (mfp_le32(h + 8) != (ph.type == MFP_PT_SCE_MODULE_PARAM ? MFP_MODULE_PARAM_MAGIC
                                                               : MFP_PROCESS_PARAM_MAGIC))
        return 0;
    info->param = ph.type == MFP_PT_SCE_MODULE_PARAM ? MFP_PARAM_MODULE : MFP_PARAM_PROCESS;
    info->param_offset = at;
    info->param_size = mfp_le64(h);
    info->param_version = mfp_le32(h + 0x0C);
    info->ps4_sdk = mfp_le32(h + 0x10);
    info->ps5_sdk = mfp_le32(h + 0x14);
    return 0;
}

/* Will it load into a game built with `game_sdk` (0 = unknown)? */
static inline mfp_verdict mfp_assess(const mfp_info *info, uint32_t game_sdk) {
    mfp_verdict v;
    v.problems = v.warnings = 0;
    v.retarget = 0;
    if (info->elf_type != MFP_ET_SCE_DYNAMIC) v.problems |= MFP_P_NOT_LIBRARY;
    if (info->encrypted) {
        v.problems |= MFP_P_ENCRYPTED;
        return v;
    }
    if (info->container == MFP_ELF) v.warnings |= MFP_W_PLAIN_ELF;
    else if (info->fake == 0) v.warnings |= MFP_W_NOT_FAKE;
    if (info->param == MFP_PARAM_NONE) v.warnings |= MFP_W_NO_SDK;
    else if (info->param_version < MFP_PARAM_PS5_VERSION) v.warnings |= MFP_W_PS4_TOOLCHAIN;
    else if (game_sdk && (info->ps5_sdk >> 16) != (game_sdk >> 16)) v.retarget = 1;
    return v;
}

/* File offset of the PS5 SDK word, when it may be rewritten: 0, or -1. */
static inline int mfp_sdk_site(const mfp_info *info, uint64_t *offset) {
    if (info->encrypted || info->param == MFP_PARAM_NONE ||
        info->param_version < MFP_PARAM_PS5_VERSION)
        return -1;
    *offset = info->param_offset + 0x14u;
    return 0;
}

static inline const char *mfp_problem_text(unsigned bit) {
    switch (bit) {
    case MFP_P_NOT_LIBRARY: return "is not a library (e_type 0xFE18); only libraries load as modules";
    case MFP_P_ENCRYPTED: return "has encrypted or compressed segments, so its SDK stamp can't be checked or fixed";
    default: return "unknown problem";
    }
}

static inline const char *mfp_warning_text(unsigned bit) {
    switch (bit) {
    case MFP_W_PLAIN_ELF: return "is a plain ELF; only fake-signed SELFs (make_fself) are known to load";
    case MFP_W_NOT_FAKE: return "is a SELF whose program type isn't 'fake'; kstuff only accepts fake SELFs";
    case MFP_W_NO_SDK: return "records no SDK version (no readable parameter block)";
    case MFP_W_PS4_TOOLCHAIN: return "was built with a PS4 toolchain (no PS5 SDK field); untested in PS5 games";
    default: return "unknown warning";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_PRX_H */
