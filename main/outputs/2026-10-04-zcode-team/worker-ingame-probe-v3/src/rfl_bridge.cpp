// rfl_bridge.cpp — v3 NEW (worker-ingame-probe-v3, team 2026-10-04).
// C bridge to the vendored rfl batch-2 reflection predicates
// (reflection_layout_predicates.{hpp,cpp}, worker-layout-batch2 origin,
// byte-logic preserved except the include path to the sibling vendored tlp
// header). Pure functions only; no OS calls; the mem[] conversion uses the
// same static single-threaded buffer discipline as layout_bridge.cpp.
#include "reflection_layout_predicates.hpp"
#include "experiments.h"

#include <cstdint>

extern "C" {

static tlp::MemSpan s_mem[IG_SPAN_MAX];

static uint32_t conv(const IgRMemSpan *rm, uint32_t n) {
    if (n > IG_SPAN_MAX) n = IG_SPAN_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        s_mem[i].start = rm[i].start;
        s_mem[i].size = rm[i].size;
        s_mem[i].data = reinterpret_cast<const uint8_t *>(
            static_cast<uintptr_t>(rm[i].start));
    }
    return n;
}

int ig_rfl_struct_chain(const IgRMemSpan *rm, uint32_t n,
                        const IgGuaViewC *gua, IgRflStructOutC *out) {
    if (!gua || !gua->objects_ptr) return -1;
    rfl::ValidatedGuaView v{gua->objects_ptr, gua->num_elements,
                            gua->num_chunks};
    rfl::StructChainResult r =
        rfl::probe_ustruct_chain(s_mem, conv(rm, n), v);
    out->verdict = (int)r.verdict;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < rfl::kMaxCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    out->items_examined = r.items_examined;
    out->plausible_uobject = r.plausible_uobject;
    out->implausible_uobject = r.implausible_uobject;
    out->roundtrips_ok = r.roundtrips_ok;
    out->walk_truncated = r.walk_truncated ? 1 : 0;
    out->first_plausible = r.first_plausible;
    out->first_ustruct_like = r.first_ustruct_like;
    out->first_uclass_like = r.first_uclass_like;
    out->struct_like_count = r.struct_like_count;
    return 0;
}

int ig_rfl_child_variant(const IgRMemSpan *rm, uint32_t n,
                         uint64_t struct_addr, const IgGuaViewC *gua,
                         IgRflFieldOutC *out) {
    rfl::ValidatedGuaView v{gua ? gua->objects_ptr : 0,
                            gua ? gua->num_elements : 0,
                            gua ? gua->num_chunks : 0};
    const rfl::ValidatedGuaView *vp = (gua && gua->objects_ptr) ? &v : nullptr;
    rfl::FieldVariantResult r =
        rfl::probe_ustruct_child_variant(s_mem, conv(rm, n), struct_addr, vp);
    out->verdict = (int)r.verdict;
    out->variant = (int)r.variant;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < rfl::kMaxCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    out->h5_verdict = (int)r.h5_verdict;
    out->h5_code = (unsigned)r.h5_code;
    out->h4_verdict = (int)r.h4_verdict;
    out->h4_code = (unsigned)r.h4_code;
    // settling_evidence is a stable short string; bounded copy.
    for (int i = 0; i < 95 && r.settling_evidence && r.settling_evidence[i]; ++i)
        out->settling[i] = r.settling_evidence[i];
    out->settling[95] = '\0';
    return 0;
}

int ig_rfl_ufunction(const IgRMemSpan *rm, uint32_t n, uint64_t addr,
                     IgRflFuncOutC *out) {
    rfl::UFunctionProbeResult r = rfl::probe_ufunction(s_mem, conv(rm, n), addr);
    out->verdict = (int)r.verdict;
    out->code_count = r.code_count;
    for (int i = 0; i < r.code_count && i < rfl::kMaxCodes; ++i)
        out->codes[i] = (unsigned)r.codes[i];
    out->func_ptr = r.func_ptr;
    out->function_flags = r.function_flags;
    out->num_parms = r.num_parms;
    out->parms_size = r.parms_size;
    out->retval_offset = r.retval_offset;
    return 0;
}

const char *ig_rfl_rcode_string(unsigned c) {
    return rfl::rcode_string((rfl::RCode)c);
}

}  // extern "C"
