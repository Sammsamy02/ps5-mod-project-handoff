//! Additive owned mapped candidates. No original strategy fallback or engine
//! assignment; candidates are evidence only, not singleton/function identity.
use alloc::collections::BTreeSet;
use core::mem::{align_of,size_of};
use crate::{mapped_api,mapped_image::{MappedImage,EXECUTE,WRITE},ps_scan_abi::PsMappedImageViewV1,resolvers::AsyncContext,ImageKind,PsObjectArrayStaticCandidate};
#[repr(C)]#[derive(Clone,Copy,Default)]
pub struct PsMappedFNameCandidateV1{pub address:u64,pub anchor_call_counts:[u32;5],pub reserved:u32}
#[repr(C)]#[derive(Clone,Copy,Default)]
pub struct PsMappedGMallocCandidateV1{pub address:u64,pub direct_callee:u64,pub receiver_call:u64,pub vtable_offset:u64,pub proof_count:u32,pub reserved:u32}
const _:()={assert!(size_of::<PsObjectArrayStaticCandidate>()==48);assert!(size_of::<PsMappedFNameCandidateV1>()==32);assert!(size_of::<PsMappedGMallocCandidateV1>()==40);};
fn aligned<T>(p:*const T)->bool{!p.is_null()&&(p as usize)%align_of::<T>()==0}
unsafe fn image<'a>(view:*const PsMappedImageViewV1)->Result<MappedImage<'a>,i32>{
 if !aligned(view){return Err(1);}
 let mapped=unsafe{mapped_api::from_view(&*view)}.map_err(|_|2)?;
 if mapped.image().kind!=ImageKind::Elf{return Err(2);}
 Ok(mapped)
}
fn role(image:&MappedImage<'_>,address:u64,size:usize,required:u32)->bool{image.permissions(address,size).is_some_and(|p|p&required==required)}
fn error(e:crate::object_cfg::Error)->i32{match e{crate::object_cfg::Error::Malformed=>2,crate::object_cfg::Error::NoCandidate=>3,crate::object_cfg::Error::Ambiguous=>4,crate::object_cfg::Error::Incomplete=>5}}
/// Same immutable/lifetime/nonalias borrow obligations as mapped_core_v1. Valid
/// aligned output cleared on every subsequent failure. Status0 means candidate.
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_guobject_array_cfg_mapped_v1(view:*const PsMappedImageViewV1,out:*mut PsObjectArrayStaticCandidate)->i32{
 if !aligned(out){return 1;}unsafe{out.write(Default::default())};
 let mapped=match unsafe{image(view)}{Ok(v)=>v,Err(e)=>return e};
 let spans=match crate::object_cfg::FileImage::from_mapped(mapped.segments()){Ok(v)=>v,Err(e)=>return error(e)};
 match crate::object_cfg::resolve_image(&spans){Ok(proof)=>{if !role(&mapped,proof.address,8,WRITE)||![proof.allocation_entry,proof.free_entry,proof.allocation_call,proof.free_call].into_iter().all(|a|role(&mapped,a,1,EXECUTE)){return 5;}unsafe{out.write(proof)};0},Err(e)=>error(e)}
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_fname_ctor_call_mapped_v1(view:*const PsMappedImageViewV1,out:*mut PsMappedFNameCandidateV1)->i32{
 if !aligned(out){return 1;}unsafe{out.write(Default::default())};
 let mapped=match unsafe{image(view)}{Ok(v)=>v,Err(e)=>return e};
 let code=mapped.segments().iter().filter(|s|s.permissions&EXECUTE!=0).map(|s|s.runtime_address..s.runtime_address+s.bytes.len()as u64).collect::<alloc::vec::Vec<_>>();
 let ctx=AsyncContext{exe:mapped.image()};
 match crate::fname_call::resolve_with_ranges(&ctx,&code){
  Ok(proof)=>{if !role(&mapped,proof.address,1,EXECUTE){return 5;}let out_value=PsMappedFNameCandidateV1{address:proof.address,anchor_call_counts:core::array::from_fn(|i|proof.calls_by_anchor[i].len()as u32),reserved:0};unsafe{out.write(out_value)};0},
  Err(e)=>match e{crate::fname_call::Error::NoCandidate=>3,crate::fname_call::Error::Ambiguous=>4,crate::fname_call::Error::Malformed=>2,crate::fname_call::Error::Incomplete=>5},
 }
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_gmalloc_deletefile_candidate_mapped_v1(view:*const PsMappedImageViewV1,out:*mut PsMappedGMallocCandidateV1)->i32{
 if !aligned(out){return 1;}unsafe{out.write(Default::default())};
 let mapped=match unsafe{image(view)}{Ok(v)=>v,Err(e)=>return e};let ctx=AsyncContext{exe:mapped.image()};
 let code=mapped.segments().iter().filter(|s|s.permissions&EXECUTE!=0).map(|s|s.runtime_address..s.runtime_address+s.bytes.len()as u64).collect::<alloc::vec::Vec<_>>();
 let proofs=match crate::deletefile_candidate::prove_mapped_deletefile_candidates(&ctx,&code){Ok(v)=>v,Err(_)=>return 5};
 let slots=proofs.iter().map(|p|p.slot).collect::<BTreeSet<_>>();
 if slots.is_empty(){return 3;}if slots.len()!=1{return 4;}
 if !proofs.iter().all(|p|role(&mapped,p.slot,8,WRITE)&&role(&mapped,p.direct_callee,1,EXECUTE)&&role(&mapped,p.receiver_call,1,EXECUTE)){return 5;}
 let Ok(proof_count)=u32::try_from(proofs.len())else{return 5;};
 let proof=&proofs[0];let out_value=PsMappedGMallocCandidateV1{address:proof.slot,direct_callee:proof.direct_callee,receiver_call:proof.receiver_call,vtable_offset:proof.vtable_offset,proof_count,reserved:0};
 unsafe{out.write(out_value)};0
}
