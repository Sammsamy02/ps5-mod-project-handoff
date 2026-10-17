// GENERATED-equivalent host config for the worker-combined-igf7 host tests.
// The PRX build script writes the real one (same macros, IG_BUILD_ID differs).
#pragma once
#define IG_BUILD_ID "igf7-host-test"
#define IG_LOG_PATH "/data/modbridge-ingame-facts.jsonl"
#define IG_VMMAP_MAX (4u << 20)

// C2 alternative candidate windows (C2-ALTERNATIVE-SPEC.md sec.5; vaddrs).
#define IG_VADDR_CAND_B 0x7826782ull  /* L"UE5" branch-name UTF-16 (8 B)   */
#define IG_VADDR_CAND_C 0x75cffdaull  /* UTF-16 "BuildIdOverride" (32 B)   */
#define IG_VADDR_CAND_D 0x7b8845dull  /* ASCII "Current Changelist" (19 B) */
#define IG_VADDR_CAND_E 0x7dd2a40ull  /* AES S-box (64 B)                  */
#define IG_VADDR_CAND_A 0xbc5f250ull  /* NT_GNU_BUILD_ID note (36 B)       */
#define IG_LEN_CAND_B 8u
#define IG_LEN_CAND_C 32u
#define IG_LEN_CAND_D 19u
#define IG_LEN_CAND_E 64u
#define IG_LEN_CAND_A 36u

// Rung-3 anchor: GUA runtime address (single-load-bias base 0x400000 +
// profile vaddr 0x93ba110; live-confirmed by the igf4 gua_read record).
#define IG_RT_GUOBJECT_ARRAY 0x97ba110ull

// Geometry cross-check constants (informational; the map decides).
#define IG_EXPECTED_BASE 0x400000ull
#define IG_EXPECTED_EXEC_EXTENT 0x747c000ull
