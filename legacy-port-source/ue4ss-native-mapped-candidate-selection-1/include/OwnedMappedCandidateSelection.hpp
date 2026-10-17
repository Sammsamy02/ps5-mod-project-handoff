#pragma once
#include <OwnedMappedResolverCollector.hpp>
#include <patternsleuth_mapped_candidates.h>
namespace RC::Unreal::Platform {
enum class CandidateSelectionPolicy : uint8_t { OriginalOnly,CollectEvidenceOnly,SelectUniqueAfterNoMatch };
enum class MappedResultProvenance : uint8_t { None,OriginalResolver,ObjectArrayDiagnosticCFG,FNameSysVArgumentCalls,GMallocDeleteFileSysV };
enum class CandidateAdmission : uint8_t { NotAttempted,Admitted,NoMatch,Ambiguous,IncompleteOrRoleRefused,InvalidArguments,InvalidImage,InvalidEvidence,RejectedRole };
template<class T> struct TypedCandidateEvidence { bool eligible{};int32_t endpoint_status{-1};CandidateAdmission admission{CandidateAdmission::NotAttempted};T raw{}; };
struct OwnedMappedCandidateCollection {
 OwnedMappedScanReport original{};
 OwnedMappedScanReport selected{};
 CandidateSelectionPolicy policy{CandidateSelectionPolicy::OriginalOnly};
 std::array<MappedResultProvenance,13> provenance{};
 TypedCandidateEvidence<PsMappedObjectArrayCandidateV1> object_array{};
 TypedCandidateEvidence<PsMappedFNameCandidateV1> fname{};
 TypedCandidateEvidence<PsMappedGMallocCandidateV1> gmalloc{};
 uint32_t selected_candidate_mask{};
};
// Original report remains verbatim. Selection is explicit, candidate-address
// only, and permitted solely after original NoMatch/status3 without image error.
// The caller retains the same read lease for all genuine endpoints and role
// checks. No logging or service callback runs here, no identity/ABI is asserted.
auto collect_owned_mapped_candidates(const OwnedMappedImageCaller&,const PsScanConfig&,
 const PsScanResults& previous={},CandidateSelectionPolicy policy=CandidateSelectionPolicy::OriginalOnly)
 noexcept -> OwnedMappedCandidateCollection;
}
