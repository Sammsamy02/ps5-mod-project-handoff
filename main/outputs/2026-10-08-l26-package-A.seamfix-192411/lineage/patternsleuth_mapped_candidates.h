#ifndef PATTERNSLEUTH_MAPPED_CANDIDATES_H
#define PATTERNSLEUTH_MAPPED_CANDIDATES_H
#include "PsScanAbi.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Additive ELF SysV candidate strategies only; no original dispatch/fallback,
 * engine assignment or callable/singleton ABI claim. Same VALID immutable,
 * actual-pointer segment lifetime, stable protection, descriptor/array storage
 * and NONALIASING aligned writable output obligations as mapped_core_v1.
 * Status0 candidate,1 arguments,2 invalid descriptor/unsupported format,
 * 3 no candidate,4 ambiguity,5 incomplete path/budget or output-role refusal.
 * Valid outputs clear on all subsequent failures. GU/GMalloc address must be
 * entirely within one supplied writable8-byte span; function/call witnesses
 * require supplied executable spans. These permission facts do not constitute
 * service registration; the caller must retain its real owned read lease.
 * GU feature flags are observed instruction features, not register dataflow or
 * object-array identity proof. FName counts describe five encoded-anchor call
 * groups, not proven function roots. GMalloc virtual-call shape is a candidate
 * receiver/slot relation; its vtable offset alone establishes no actual ABI.
 * No file results are rebased/cast; parsing operates on supplied mapped bytes.
 */
typedef struct PsMappedObjectArrayCandidateV1 {
 uint64_t address,allocation_entry,free_entry,allocation_call,free_call;
 uint32_t evidence_flags,reserved;
} PsMappedObjectArrayCandidateV1;
typedef struct PsMappedFNameCandidateV1 {
 uint64_t address;uint32_t anchor_call_counts[5],reserved;
} PsMappedFNameCandidateV1;
typedef struct PsMappedGMallocCandidateV1 {
 uint64_t address,direct_callee,receiver_call,vtable_offset;
 uint32_t proof_count,reserved;
} PsMappedGMallocCandidateV1;
int ps_guobject_array_cfg_mapped_v1(const PsMappedImageViewV1*,PsMappedObjectArrayCandidateV1*);
int ps_fname_ctor_call_mapped_v1(const PsMappedImageViewV1*,PsMappedFNameCandidateV1*);
int ps_gmalloc_deletefile_candidate_mapped_v1(const PsMappedImageViewV1*,PsMappedGMallocCandidateV1*);
#ifdef __cplusplus
}
#endif
#endif
