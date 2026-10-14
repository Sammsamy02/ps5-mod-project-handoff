// layout_bridge.cpp
//
// C bridge to the vendored layout predicates (townfall_layout_probes.{hpp,cpp},
// worker-5-layout-validator origin). Pure functions only; no OS calls. The C
// side supplies readable spans as raw addresses; data pointers alias the live
// mappings (readable bit asserted by the caller's vmmap facts).
#include "townfall_layout_probes.hpp"
#include "experiments.h"

#include <cstdint>

extern "C" {

static void fill_codes(IgGuaLayoutOutC *out, const tlp::GuaLayoutVerdict &v) {
    out->verdict = (int)v.verdict;
    out->code_count = v.code_count;
    for (int i = 0; i < v.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->codes[i] = (unsigned)v.codes[i];
}

int ig_layout_guobject(const IgSpan *spans, uint32_t span_count,
                       uint64_t image_base, uint64_t goa_vaddr,
                       IgGuaOutC *out) {
    tlp::MemSpan mem[IG_SPAN_MAX];
    if (span_count > IG_SPAN_MAX) span_count = IG_SPAN_MAX;
    uint32_t n = 0;
    for (uint32_t i = 0; i < span_count; ++i) {
        if ((spans[i].prot & 1u) == 0) continue;   // readable spans only
        mem[n].start = spans[i].start;
        mem[n].size = spans[i].end - spans[i].start;
        mem[n].data = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(spans[i].start));
        ++n;
    }
    tlp::GuaProbeResult r = tlp::probe_guobject_array(
        mem, n, image_base, goa_vaddr);
    out->verdict = (int)r.verdict;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    fill_codes(&out->layout_a, r.layout_a);
    fill_codes(&out->layout_b, r.layout_b);
    out->layout_matched = (int)r.observed.layout_matched;
    out->obj_first_gc_index = r.observed.obj_first_gc_index;
    out->obj_last_non_gc_index = r.observed.obj_last_non_gc_index;
    out->max_elements = r.observed.max_elements;
    out->num_elements = r.observed.num_elements;
    out->max_chunks = r.observed.max_chunks;
    out->num_chunks = r.observed.num_chunks;
    out->objects_ptr = r.observed.objects_ptr;
    out->preallocated_objects_ptr = r.observed.preallocated_objects_ptr;
    out->chunk0_ptr = r.observed.chunk0_ptr;
    out->chunk1_ptr = r.observed.chunk1_ptr;
    out->chunks_scanned = r.observed.chunks_scanned;
    return 0;
}

int ig_layout_code_shape(const IgSpan *spans, uint32_t span_count,
                         uint64_t image_base, uint64_t vaddr,
                         IgCodeOutC *out) {
    tlp::MemSpan mem[IG_SPAN_MAX];
    if (span_count > IG_SPAN_MAX) span_count = IG_SPAN_MAX;
    uint32_t n = 0;
    for (uint32_t i = 0; i < span_count; ++i) {
        if ((spans[i].prot & 1u) == 0) continue;
        mem[n].start = spans[i].start;
        mem[n].size = spans[i].end - spans[i].start;
        mem[n].data = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(spans[i].start));
        ++n;
    }
    tlp::CodeProbeResult r =
        tlp::probe_code_shape(mem, n, image_base, vaddr, nullptr);
    out->verdict = (int)r.verdict;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    out->bytes_read = r.bytes_read;
    for (int i = 0; i < 16; ++i) out->first_bytes[i] = r.first_bytes[i];
    return 0;
}

// Stable short names for records (bounded, printable).
const char *ig_verdict_string(int v) { return tlp::verdict_string((tlp::Verdict)v); }
const char *ig_rcode_string(unsigned c) {
    return tlp::rcode_string((tlp::RCode)c);
}

}  // extern "C"
