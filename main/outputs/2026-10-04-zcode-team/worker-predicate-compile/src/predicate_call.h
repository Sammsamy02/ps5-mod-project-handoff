/* predicate_call.h
 *
 * Minimal C-callable wrapper around the vendored batch1 GUObjectArray
 * layout predicate (townfall_layout_probes.hpp / .cpp, worker-5-layout-
 * validator, vendored verbatim by worker-predicate-compile; sha256s in
 * REPORT.md).
 *
 * Contract (worker-predicate-compile CLAIM.json):
 *   takes a raw pointer + length + image_base + vaddr, constructs the ONE
 *   tlp::MemSpan {start=image_base, size=length, data=ptr}, calls
 *   tlp::probe_guobject_array(spans,1,image_base,goa_vaddr), and returns the
 *   verdict as a fixed-layout POD (no pointers, memcpy-safe).
 *
 * The wrapper is PURE: it never dereferences `image` beyond [image,
 * image+length) and only through the predicate's own span logic; no OS calls,
 * no allocation, no exceptions.
 *
 * NOTE ON COMPILATION: predicate_call.c is written in the common C/C++
 * subset but MUST be compiled with a C++ compiler (the PS5 probes use
 * prospero-clang++ with the frozen CPPFLAGS — see ps5/CONSUME.md), because
 * it calls the C++ predicate entry point directly. This header itself is
 * consumable from plain C translation units.
 */

#ifndef PREDICATE_CALL_H
#define PREDICATE_CALL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- mirrored tlp::Verdict (enum class : uint8_t) ---------------------- */
#define PC_V_NOT_PROBED   ((uint8_t)0)
#define PC_V_VALIDATED    ((uint8_t)1)
#define PC_V_INCONCLUSIVE ((uint8_t)2)
#define PC_V_REFUTED      ((uint8_t)3)

/* --- mirrored tlp::GuaLayout (enum class : uint8_t) -------------------- */
#define PC_LAYOUT_NONE                ((uint8_t)0)
#define PC_LAYOUT_FUOBJECTARRAY_OUTER ((uint8_t)1) /* hypothesis A */
#define PC_LAYOUT_TUOBJECTARRAY_DIRECT ((uint8_t)2) /* hypothesis B */

/* --- mirrored tlp::RCode (enum class : uint16_t) ------------------------ */
#define PC_R_OK                            ((uint16_t)0x0000)
#define PC_R_GOA_E_ADDR_UNREADABLE         ((uint16_t)0x1001)
#define PC_R_GOA_E_CHUNK_EXTENT_UNVERIFIABLE ((uint16_t)0x1002)
#define PC_R_GOA_E_LAYOUT_AMBIGUOUS        ((uint16_t)0x1003)
#define PC_R_GOA_R_PTR_NONCANONICAL        ((uint16_t)0x1101)
#define PC_R_GOA_R_PTR_OUTSIDE_READABLE    ((uint16_t)0x1102)
#define PC_R_GOA_R_PTRS_BOTH_NULL          ((uint16_t)0x1103)
#define PC_R_GOA_R_COUNTS_INSANE           ((uint16_t)0x1104)
#define PC_R_GOA_R_GC_ORDER                ((uint16_t)0x1105)
#define PC_R_GOA_R_CHUNK_NULL_WHILE_NEEDED ((uint16_t)0x1106)
#define PC_R_GOA_R_CHUNK_UNREADABLE        ((uint16_t)0x1107)
#define PC_R_GOA_R_CHUNK_DUPLICATE         ((uint16_t)0x1108)
#define PC_R_GOA_R_CHUNK_OVERLAP           ((uint16_t)0x1109)
#define PC_R_GOA_R_CAPACITY_MISMATCH       ((uint16_t)0x110A)

/* Wrapper-local argument codes (NOT tlp rcodes) */
#define PC_R_CALL_E_BAD_ARGS               ((uint16_t)0xFFFE)

#define PC_MAX_CODES 8 /* mirrors tlp::kMaxRationaleCodes */

/* Verdict POD. Field order/types are FIXED for cross-ABI use; all scalars,
 * no pointers, memcpy-safe, 8-byte aligned, 96 bytes on LP64. */
typedef struct pc_gua_verdict {
    /* 0x00 */ uint8_t  verdict;            /* PC_V_* (combined) */
    /* 0x01 */ uint8_t  layout;             /* PC_LAYOUT_* matched, else NONE */
    /* 0x02 */ uint8_t  layout_a_verdict;   /* PC_V_* for hypothesis A */
    /* 0x03 */ uint8_t  layout_b_verdict;   /* PC_V_* for hypothesis B */
    /* 0x04 */ uint8_t  code_count;
    /* 0x05 */ uint8_t  reserved[3];
    /* 0x08 */ uint16_t codes[PC_MAX_CODES];
    /* 0x18 */ int32_t  obj_first_gc_index;     /* layout A only, else -1 */
    /* 0x1C */ int32_t  obj_last_non_gc_index;  /* layout A only, else -1 */
    /* 0x20 */ int32_t  max_elements;
    /* 0x24 */ int32_t  num_elements;
    /* 0x28 */ int32_t  max_chunks;
    /* 0x2C */ int32_t  num_chunks;
    /* 0x30 */ uint32_t chunks_scanned;
    /* 0x34 */ uint32_t reserved2;
    /* 0x38 */ uint64_t objects_ptr;
    /* 0x40 */ uint64_t preallocated_objects_ptr;
    /* 0x48 */ uint64_t chunk0_ptr;
    /* 0x50 */ uint64_t chunk1_ptr;
    /* 0x58 */ uint64_t reserved3;      /* explicit tail; sizeof == 0x60 */
} pc_gua_verdict;

/* Run the GUObjectArray predicate over ONE readable image.
 *
 *   image      — host pointer to the readable bytes (in-PRX: the mapped
 *                module base itself is normally passed via spans; here the
 *                caller hands one contiguous readable region)
 *   length     — readable byte count at `image` (> 0)
 *   image_base — virtual address where `image` is (or is assumed to be)
 *                mapped; goa_vaddr is relative to it
 *   goa_vaddr  — profile vaddr of the GUObjectArray label
 *                (worker-2 profile fact 0x93ba110 for townfall)
 *   out        — required; receives the verdict POD
 *
 * Returns 0 on success (out->verdict is Validated/Inconclusive/Refuted),
 * -1 on bad arguments (image==NULL || out==NULL || length==0), in which case
 * *out is zeroed except verdict=PC_V_NOT_PROBED, codes[0]=PC_R_CALL_E_BAD_ARGS.
 */
int predicate_call_guobject_array(const void* image, size_t length,
                                  uint64_t image_base, uint64_t goa_vaddr,
                                  pc_gua_verdict* out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PREDICATE_CALL_H */
