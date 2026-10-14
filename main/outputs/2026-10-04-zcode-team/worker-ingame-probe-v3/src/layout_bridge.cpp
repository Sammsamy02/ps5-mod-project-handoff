// layout_bridge.cpp — v3 (worker-ingame-probe-v3).
// Vendored from worker-ingame-probe-v2/src/layout_bridge.cpp (worker-5
// origin; predicate calls byte-identical). Labeled changes:
//   - the per-call `tlp::MemSpan mem[IG_SPAN_MAX]` STACK array (48 KiB at
//     IG_SPAN_MAX=2048) moved to a function-static buffer — this module is
//     single-threaded module_start by contract (no reentrancy possible);
//   - added *_prebuilt variants taking the once-built IgRMemSpan list so
//     base-hunt trials (potentially thousands) do not re-filter the span
//     table per trial.
#include "townfall_layout_probes.hpp"
#include "experiments.h"

#include <cstdint>

extern "C" {

static tlp::MemSpan s_mem[IG_SPAN_MAX];

static uint32_t conv_spans(const IgSpan *spans, uint32_t span_count) {
    if (span_count > IG_SPAN_MAX) span_count = IG_SPAN_MAX;
    uint32_t n = 0;
    for (uint32_t i = 0; i < span_count; ++i) {
        if ((spans[i].prot & 1u) == 0) continue;   // readable spans only
        s_mem[n].start = spans[i].start;
        s_mem[n].size = spans[i].end - spans[i].start;
        s_mem[n].data = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(spans[i].start));
        ++n;
    }
    return n;
}

static uint32_t conv_prebuilt(const IgRMemSpan *rm, uint32_t n) {
    if (n > IG_SPAN_MAX) n = IG_SPAN_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        s_mem[i].start = rm[i].start;
        s_mem[i].size = rm[i].size;
        s_mem[i].data = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(rm[i].start));
    }
    return n;
}

static void fill_gua(IgGuaOutC *out, const tlp::GuaProbeResult &r) {
    out->verdict = (int)r.verdict;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    out->layout_a.verdict = (int)r.layout_a.verdict;
    out->layout_a.code_count = r.layout_a.code_count;
    for (int i = 0; i < r.layout_a.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->layout_a.codes[i] = (unsigned)r.layout_a.codes[i];
    out->layout_b.verdict = (int)r.layout_b.verdict;
    out->layout_b.code_count = r.layout_b.code_count;
    for (int i = 0; i < r.layout_b.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->layout_b.codes[i] = (unsigned)r.layout_b.codes[i];
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
}

int ig_layout_guobject(const IgSpan *spans, uint32_t span_count,
                       uint64_t image_base, uint64_t goa_vaddr,
                       IgGuaOutC *out) {
    uint32_t n = conv_spans(spans, span_count);
    tlp::GuaProbeResult r = tlp::probe_guobject_array(
        s_mem, n, image_base, goa_vaddr);
    fill_gua(out, r);
    return 0;
}

int ig_layout_guobject_prebuilt(const IgRMemSpan *rm, uint32_t n,
                                uint64_t image_base, uint64_t goa_vaddr,
                                IgGuaOutC *out) {
    uint32_t m = conv_prebuilt(rm, n);
    tlp::GuaProbeResult r = tlp::probe_guobject_array(
        s_mem, m, image_base, goa_vaddr);
    fill_gua(out, r);
    return 0;
}

int ig_layout_code_shape(const IgSpan *spans, uint32_t span_count,
                         uint64_t image_base, uint64_t vaddr,
                         IgCodeOutC *out) {
    uint32_t n = conv_spans(spans, span_count);
    tlp::CodeProbeResult r =
        tlp::probe_code_shape(s_mem, n, image_base, vaddr, nullptr);
    out->verdict = (int)r.verdict;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < tlp::kMaxRationaleCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    out->bytes_read = r.bytes_read;
    for (int i = 0; i < 16; ++i) out->first_bytes[i] = r.first_bytes[i];
    return 0;
}

int ig_layout_code_shape_prebuilt(const IgRMemSpan *rm, uint32_t n,
                                  uint64_t image_base, uint64_t vaddr,
                                  IgCodeOutC *out) {
    uint32_t m = conv_prebuilt(rm, n);
    tlp::CodeProbeResult r =
        tlp::probe_code_shape(s_mem, m, image_base, vaddr, nullptr);
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
