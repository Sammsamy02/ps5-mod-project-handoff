// rung3_batch2_bridge.cpp — see rung3_batch2_bridge.hpp for the contract.

#include "rung3_batch2_bridge.hpp"

namespace rc3b {

size_t publish_memspans(const igu_span* spans, size_t n,
                        tlp::MemSpan* out, size_t cap, size_t* dropped) {
    size_t published = 0, withheld = 0;
    if (dropped != nullptr) *dropped = 0;
    if (spans == nullptr || out == nullptr) return 0;
    for (size_t i = 0; i < n; ++i) {
        const igu_span& s = spans[i];
        const bool readable = (s.prot & IGU_PROT_R) != 0u;
        const bool shaped = s.end > s.start && s.data != nullptr;
        if (!readable || !shaped) {
            ++withheld;  // recorded, never silent
            continue;
        }
        if (published >= cap) {
            ++withheld;  // cap overflow is also recorded
            continue;
        }
        out[published].start = s.start;
        out[published].size  = s.end - s.start;
        out[published].data  = s.data;
        ++published;
    }
    if (dropped != nullptr) *dropped = withheld;
    return published;
}

namespace {

bool info_checks_pass(const rc3_record& rec) {
    return rec.info_max_eq_chunks != IGU_CSK_CONTRADICTED &&
           rec.info_num_le_max != IGU_CSK_CONTRADICTED &&
           rec.info_nchunks_ok != IGU_CSK_CONTRADICTED;
}

}  // namespace

Batch2Result run_ustruct_chain(const rc3_record& rec, const rc3_vmstats& vm,
                               const igu_span* spans, size_t n) {
    Batch2Result r{};
    r.ran = 0;
    r.gate = BRIDGE_GATE_ARG;
    r.verdict = static_cast<uint8_t>(tlp::Verdict::NotProbed);

    if (spans == nullptr || n == 0) {
        return r;  // BRIDGE_GATE_ARG
    }
    if (rc3_vmmap_trustworthy(&vm) != 1) {
        r.gate = BRIDGE_GATE_MAP_UNTRUSTED;
        return r;
    }
    // Chain provenance gate: the view is assembled ONLY from a record whose
    // commissioned steps actually read OK on the fresh map. chunk[0] is the
    // predicate's entry chunk (probe_ustruct_chain walks index 0.. first).
    if (rec.st_hdr != IGU_OK || rec.st_a != IGU_OK ||
        rec.chunk[0].st_ptr != IGU_OK || rec.st_obj != IGU_OK) {
        r.gate = BRIDGE_GATE_STEP_FAILED;
        return r;
    }
    if (!info_checks_pass(rec)) {
        r.gate = BRIDGE_GATE_INFO_CONTRADICTED;
        return r;
    }

    // Gatekeeper pass: only readable spans cross into batch2's trusted
    // read_bytes world.
    tlp::MemSpan views[512];
    size_t dropped = 0;
    const size_t count = publish_memspans(spans, n, views, 512, &dropped);
    (void)dropped;  // recorded by the caller's own publish telemetry; the
                    // probe fact record carries the walk table verbatim.

    rfl::ValidatedGuaView gua;
    gua.objects_ptr  = rec.objects_ptr;
    gua.num_elements = static_cast<int32_t>(rec.num_elements);
    gua.num_chunks   = static_cast<int32_t>(rec.num_chunks);
    if (!gua.valid()) {
        r.gate = BRIDGE_GATE_STEP_FAILED;
        return r;
    }

    const rfl::StructChainResult res =
        rfl::probe_ustruct_chain(views, count, gua);

    r.ran = 1;
    r.gate = BRIDGE_GATE_OK;
    r.verdict = static_cast<uint8_t>(res.verdict);
    r.code_count = res.code_count;
    for (uint8_t i = 0; i < res.code_count && i < 8; ++i) {
        r.codes[i] = static_cast<uint16_t>(res.codes[i]);
    }
    r.items_examined      = res.items_examined;
    r.plausible_uobject   = res.plausible_uobject;
    r.implausible_uobject = res.implausible_uobject;
    r.roundtrips_ok       = res.roundtrips_ok;
    r.struct_like_count   = res.struct_like_count;
    r.walk_truncated      = res.walk_truncated ? 1u : 0u;
    r.first_plausible     = res.first_plausible;
    r.first_ustruct_like  = res.first_ustruct_like;
    r.first_uclass_like   = res.first_uclass_like;
    return r;
}

}  // namespace rc3b
