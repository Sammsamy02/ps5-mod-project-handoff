/* host_compat/sys/user.h — HOST TEST ONLY (worker-c2-probe, team 2026-10-04).
   Minimal SDK-shaped struct kinfo_vmentry matching the pinned PS5 SDK header
   layout (sizeof == 1160) so mapscan.c compiles and links on the macOS host
   with its pinned sizeof semantics. Field offsets used by vmmap_walk.c:
   kve_structsize 0, kve_start 8, kve_end 16, kve_protection 56, path 136. */
#ifndef HOST_COMPAT_USER_H
#define HOST_COMPAT_USER_H

#include <stdint.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

struct kinfo_vmentry {
    int kve_structsize;      /* 0  */
    int kve_type;            /* 4  */
    uint64_t kve_start;      /* 8  */
    uint64_t kve_end;        /* 16 */
    uint64_t kve_offset;     /* 24 */
    uint64_t kve_vn_fileid;  /* 32 */
    uint32_t kve_vn_fsid;    /* 40 */
    int kve_flags;           /* 44 */
    int kve_resident;        /* 48 */
    int kve_private_resident;/* 52 */
    int kve_protection;      /* 56 */
    int kve_ref_count;       /* 60 */
    int kve_shadow_count;    /* 64 */
    int kve_vn_type;         /* 68 */
    uint64_t kve_vn_size;    /* 72 */
    uint32_t kve_vn_rdev;    /* 80 */
    uint16_t kve_vn_mode;    /* 84 */
    uint16_t kve_vn_status;  /* 86 */
    int _kve_ispare[12];     /* 88 */
    char kve_path[PATH_MAX]; /* 136 */
};                           /* sizeof == 1160 */

#endif /* HOST_COMPAT_USER_H */
