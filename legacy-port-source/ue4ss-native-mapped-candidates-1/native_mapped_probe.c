#include "include/patternsleuth_mapped_core.h"
#include <stddef.h>
_Static_assert(sizeof(PsMappedCoreResult)==16,"result size");
_Static_assert(_Alignof(PsMappedCoreResult)==8,"result align");
_Static_assert(offsetof(PsMappedCoreResult,address)==0,"address");
_Static_assert(offsetof(PsMappedCoreResult,major)==8,"major");
_Static_assert(offsetof(PsMappedCoreResult,minor)==10,"minor");
_Static_assert(offsetof(PsMappedCoreResult,reserved)==12,"reserved");
int mapped_consumer_probe(const PsMappedImageViewV1* view,PsMappedCoreResult* out){
 return ps_resolve_mapped_core_v1(view,1,out);
}

#include "include/patternsleuth_mapped_properties.h"
_Static_assert(sizeof(PsMappedPropertyResult)==8,"property result size");
_Static_assert(_Alignof(PsMappedPropertyResult)==4,"property result align");
_Static_assert(offsetof(PsMappedPropertyResult,value)==0,"property value");
_Static_assert(offsetof(PsMappedPropertyResult,reserved)==4,"property reserved");
int mapped_property_consumer_probe(const PsMappedImageViewV1*view,PsMappedPropertyResult*out){return ps_resolve_mapped_property_v1(view,1,out);}
#include "include/patternsleuth_mapped_candidates.h"
_Static_assert(sizeof(PsMappedObjectArrayCandidateV1)==48,"GU candidate size");
_Static_assert(_Alignof(PsMappedObjectArrayCandidateV1)==8,"GU candidate align");
_Static_assert(offsetof(PsMappedObjectArrayCandidateV1,evidence_flags)==40,"GU evidence flags");
_Static_assert(offsetof(PsMappedObjectArrayCandidateV1,reserved)==44,"GU reserved");
_Static_assert(sizeof(PsMappedFNameCandidateV1)==32,"FName candidate size");
_Static_assert(_Alignof(PsMappedFNameCandidateV1)==8,"FName candidate align");
_Static_assert(offsetof(PsMappedFNameCandidateV1,anchor_call_counts)==8,"FName anchor counts");
_Static_assert(offsetof(PsMappedFNameCandidateV1,reserved)==28,"FName reserved");
_Static_assert(sizeof(PsMappedGMallocCandidateV1)==40,"GMalloc candidate size");
_Static_assert(_Alignof(PsMappedGMallocCandidateV1)==8,"GMalloc candidate align");
_Static_assert(offsetof(PsMappedGMallocCandidateV1,direct_callee)==8,"GMalloc callee");
_Static_assert(offsetof(PsMappedGMallocCandidateV1,receiver_call)==16,"GMalloc receiver call");
_Static_assert(offsetof(PsMappedGMallocCandidateV1,vtable_offset)==24,"GMalloc vtable offset");
_Static_assert(offsetof(PsMappedGMallocCandidateV1,proof_count)==32,"GMalloc proof count");
_Static_assert(offsetof(PsMappedGMallocCandidateV1,reserved)==36,"GMalloc reserved");
int mapped_candidate_consumer_probe(const PsMappedImageViewV1*v,PsMappedObjectArrayCandidateV1*g,PsMappedFNameCandidateV1*f,PsMappedGMallocCandidateV1*m){return ps_guobject_array_cfg_mapped_v1(v,g)+ps_fname_ctor_call_mapped_v1(v,f)+ps_gmalloc_deletefile_candidate_mapped_v1(v,m);}
