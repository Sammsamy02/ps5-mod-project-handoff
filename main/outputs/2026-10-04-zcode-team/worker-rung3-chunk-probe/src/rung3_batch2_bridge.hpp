// rung3_batch2_bridge.hpp
//
// C++ BRIDGE: rung-3 chunk/object add-on -> batch2 UStruct/UClass chain
// predicate. Team: 2026-10-04-zcode-team / worker-rung3-chunk-probe.
//
// Lineage-consistent composition (the probe tree already mixes C sources
// with C++ predicate bridges: worker-ingame-probe-v3 carries
// reflection_layout_predicates.cpp / rfl_bridge.cpp alongside C sources).
// Sibling headers are included READ-ONLY by relative path (the exact
// pattern worker-layout-batch2 uses for worker-5's header).
//
// Contract (mirrors worker-batch2-live-prep REPORT sec.3 integration note):
//   * tlp::MemSpan has NO prot field and batch2's read_bytes trusts spans
//     unconditionally. publish_memspans() is the GATEKEEPER: only spans
//     with KVME READ prot are published; skipped spans are counted, never
//     silent.
//   * run_ustruct_chain() assembles a rfl::ValidatedGuaView ONLY from a
//     record whose chain steps validated (map trusted, header/Objects**/
//     chunk[0]/UObjectBase steps OK, no info-check contradiction), then
//     runs rfl::probe_ustruct_chain — the UStruct chain (SuperStruct
//     acyclicity, Children/ChildProperties heads) and the UClass CDO
//     extension — on spans this module has proven covered-and-readable.
//   * Everything is recorded POD: verdict/codes are copied as integers so
//     the result can live in the probe's fact records verbatim.

#ifndef RUNG3_BATCH2_BRIDGE_HPP
#define RUNG3_BATCH2_BRIDGE_HPP

#include <cstddef>
#include <cstdint>

#include "rung3_chunk_addon.h"

#include "../../worker-5-layout-validator/src/townfall_layout_probes.hpp"
#include "../../worker-layout-batch2/src/reflection_layout_predicates.hpp"

namespace rc3b {

// Why the batch2 predicate did not run (recorded, never silent).
enum Gate : uint32_t {
    BRIDGE_GATE_OK                = 0,  // predicate ran
    BRIDGE_GATE_ARG               = 1,  // null spans / empty table
    BRIDGE_GATE_MAP_UNTRUSTED     = 2,  // walk stats failed the trust gate
    BRIDGE_GATE_STEP_FAILED       = 3,  // st_hdr/st_a/chunk0-ptr/UObjectBase
                                        // step not IGU_OK (coverage,
                                        // exhaustion, or refutation)
    BRIDGE_GATE_INFO_CONTRADICTED = 4,  // a non-gating info check contradicted
                                        // the layout-A header invariants —
                                        // recorded evidence, blocks handoff
};

// Publish ONLY READABLE spans as tlp::MemSpan views. Returns the published
// count; *dropped (may be nullptr) receives the number of spans withheld
// for lacking READ prot (or malformed zero-size spans). Never publishes a
// span this module has not proven readable.
size_t publish_memspans(const igu_span* spans, size_t n,
                        tlp::MemSpan* out, size_t cap, size_t* dropped);

// Fixed-size POD mirror of rfl::StructChainResult (fact-record friendly).
struct Batch2Result {
    uint32_t ran;                 // 1 iff the predicate executed
    uint32_t gate;                // Gate code (BRIDGE_GATE_OK when ran)
    uint8_t  verdict;             // tlp::Verdict (0 NotProbed when !ran)
    uint8_t  code_count;
    uint16_t codes[8];            // rfl::RCode values, dedup'd by predicate
    uint32_t items_examined;
    uint32_t plausible_uobject;
    uint32_t implausible_uobject;
    uint32_t roundtrips_ok;
    uint32_t struct_like_count;
    uint32_t walk_truncated;
    uint64_t first_plausible;
    uint64_t first_ustruct_like;
    uint64_t first_uclass_like;   // struct-like AND UClass CDO ext passed
};

// Gate + run rfl::probe_ustruct_chain on published-readable spans.
// `vm` must be the walk stats behind `rec` (trust gate); `spans` the
// walked igu_span table. The ValidatedGuaView carries the record's
// objects_ptr / num_elements / num_chunks ONLY when every gate passes.
Batch2Result run_ustruct_chain(const rc3_record& rec, const rc3_vmstats& vm,
                               const igu_span* spans, size_t n);

}  // namespace rc3b

#endif  // RUNG3_BATCH2_BRIDGE_HPP
