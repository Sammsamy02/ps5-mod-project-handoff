/* predicate_call.c — implementation of predicate_call.h
 *
 * Written in the common C/C++ subset; MUST be compiled with a C++ compiler
 * because it calls the C++ predicate entry point tlp::probe_guobject_array
 * directly. On the PS5 probe toolchain this means prospero-clang++ with the
 * frozen CPPFLAGS (identical to how townfall_layout_probes.cpp itself is
 * compiled by worker-ingame-probe-v2/v3 build_ingame.py — see ps5/CONSUME.md).
 */

#include "predicate_call.h"

#if !defined(__cplusplus)
#error "predicate_call.c requires a C++ compiler (it calls tlp C++ entry points); the header stays C-consumable"
#endif

#include "townfall_layout_probes.hpp"

int predicate_call_guobject_array(const void* image, size_t length,
                                  uint64_t image_base, uint64_t goa_vaddr,
                                  pc_gua_verdict* out) {
    if (image == nullptr || out == nullptr || length == 0) {
        if (out != nullptr) {
            *out = pc_gua_verdict{};
            out->verdict = PC_V_NOT_PROBED;
            out->code_count = 1;
            out->codes[0] = PC_R_CALL_E_BAD_ARGS;
        }
        return -1;
    }

    /* One span: the whole caller-supplied readable region. */
    tlp::MemSpan span;
    span.start = image_base;
    span.size = (uint64_t)length;
    span.data = (const uint8_t*)image;

    const tlp::GuaProbeResult r =
        tlp::probe_guobject_array(&span, 1, image_base, goa_vaddr);

    static_assert(sizeof(r.codes) / sizeof(r.codes[0]) == PC_MAX_CODES,
                  "rcode array widths must match");

    out->verdict = (uint8_t)r.verdict;
    out->layout = (uint8_t)r.observed.layout_matched;
    out->layout_a_verdict = (uint8_t)r.layout_a.verdict;
    out->layout_b_verdict = (uint8_t)r.layout_b.verdict;
    out->code_count = r.code_count;
    out->reserved[0] = 0;
    out->reserved[1] = 0;
    out->reserved[2] = 0;
    for (int i = 0; i < PC_MAX_CODES; ++i) {
        out->codes[i] = (i < r.code_count) ? (uint16_t)r.codes[i] : 0;
    }
    out->obj_first_gc_index = r.observed.obj_first_gc_index;
    out->obj_last_non_gc_index = r.observed.obj_last_non_gc_index;
    out->max_elements = r.observed.max_elements;
    out->num_elements = r.observed.num_elements;
    out->max_chunks = r.observed.max_chunks;
    out->num_chunks = r.observed.num_chunks;
    out->chunks_scanned = r.observed.chunks_scanned;
    out->reserved2 = 0;
    out->objects_ptr = r.observed.objects_ptr;
    out->preallocated_objects_ptr = r.observed.preallocated_objects_ptr;
    out->chunk0_ptr = r.observed.chunk0_ptr;
    out->chunk1_ptr = r.observed.chunk1_ptr;
    out->reserved3 = 0;
    return 0;
}
