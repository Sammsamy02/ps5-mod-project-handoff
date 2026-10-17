use alloc::{vec,vec::Vec};
use std::fs;
use object::{Object,ObjectSymbol};
use object::read::elf::{FileHeader,ProgramHeader};
use crate::ps_scan_abi::{PsMappedSegment,PsMappedImageViewV1,PsMappedExtent};
struct Fixture{storage:Vec<u8>,loads:Vec<(usize,usize,u32)>,text:usize,symbols:alloc::collections::BTreeMap<String,usize>}
impl Fixture{
 fn load(name:&str)->Self{
  let bytes=fs::read(alloc::format!("candidate-fixtures/{name}.elf")).unwrap();let h=object::elf::FileHeader64::<object::LittleEndian>::parse(&*bytes).unwrap();let e=object::LittleEndian;let ph=h.program_headers(e,&*bytes).unwrap();let mut loads=vec![];let mut max=0;
  for p in ph{if p.p_type(e)==object::elf::PT_LOAD{let address=p.p_vaddr(e)as usize;let size=p.p_memsz(e)as usize;assert!(address+size<=1024*1024);max=max.max(address+size);let flags=p.p_flags(e);loads.push((address,size,(if flags&4!=0{1}else{0})|(if flags&2!=0{2}else{0})|(if flags&1!=0{4}else{0})));}}
  let mut storage=vec![0;max];for p in ph{if p.p_type(e)==object::elf::PT_LOAD{let address=p.p_vaddr(e)as usize;let offset=p.p_offset(e)as usize;let n=p.p_filesz(e)as usize;storage[address..address+n].copy_from_slice(&bytes[offset..offset+n]);}}
  let file=object::File::parse(&*bytes).unwrap();let symbols=file.symbols().filter_map(|s|s.name().ok().map(|n|(n.to_string(),s.address()as usize))).collect();let text=loads.iter().find(|x|x.2&4!=0).unwrap().0;
  Self{storage,loads,text,symbols}
 }
 fn address(&self,offset:usize)->u64{self.storage.as_ptr()as usize as u64+offset as u64}
 fn segments(&self)->Vec<PsMappedSegment>{self.loads.iter().filter(|x|x.1!=0).map(|&(offset,size,permissions)|PsMappedSegment{bytes:self.storage[offset..offset+size].as_ptr(),size,runtime_address:self.address(offset),permissions,reserved:0}).collect()}
 fn view(&self,segments:&[PsMappedSegment])->PsMappedImageViewV1{PsMappedImageViewV1{abi_version:1,format:1,segments:segments.as_ptr(),segment_count:segments.len(),text_base:self.address(self.text),image_base:0,eh_frame:PsMappedExtent{address:0,size:0},eh_frame_hdr:PsMappedExtent{address:0,size:0},pe_exception_directory:PsMappedExtent{address:0,size:0}}}
 fn symbol(&self,name:&str)->u64{self.address(self.symbols[name])}
}
fn candidate(view:&PsMappedImageViewV1,id:u32)->(i32,u64){
 match id{
  1=>{let mut out=crate::PsObjectArrayStaticCandidate::default();let status=unsafe{crate::mapped_candidates::ps_guobject_array_cfg_mapped_v1(view,&mut out)};if status==0{assert_ne!(out.allocation_entry,out.free_entry);assert_eq!(out.evidence_flags&63,63);}else{assert_eq!(out.address,0);} (status,out.address)},
  2=>{let mut out=crate::mapped_candidates::PsMappedFNameCandidateV1::default();let status=unsafe{crate::mapped_candidates::ps_fname_ctor_call_mapped_v1(view,&mut out)};if status==0{assert!(out.anchor_call_counts.iter().all(|n|*n>0));}else{assert_eq!(out.address,0);} (status,out.address)},
  3=>{let mut out=crate::mapped_candidates::PsMappedGMallocCandidateV1::default();let status=unsafe{crate::mapped_candidates::ps_gmalloc_deletefile_candidate_mapped_v1(view,&mut out)};if status==0{assert!(out.proof_count>0);assert_eq!(out.vtable_offset,16);}else{assert_eq!(out.address,0);} (status,out.address)},
  _=>unreachable!()
 }
}
#[test]
fn mapped_pic_candidates_use_actual_supplied_operands(){
 let a=Fixture::load("positive");let b=Fixture::load("positive");assert_ne!(a.storage.as_ptr(),b.storage.as_ptr());
 for f in [&a,&b]{let spans=f.segments();let view=f.view(&spans);for(id,name)in [(1,"candidate_gu"),(2,"candidate_ctor"),(3,"candidate_malloc")]{let(status,address)=candidate(&view,id);assert_eq!(status,0,"strategy{id}");assert_eq!(address,f.symbol(name));}}
}
#[test]
fn mapped_candidates_require_complete_writable_slot_roles(){
 let f=Fixture::load("positive");let mut spans=f.segments();
 for s in &mut spans{if s.permissions&2!=0{s.permissions&=!2;}}
 assert_eq!(candidate(&f.view(&spans),3).0,5,"readonly GMalloc slot must refuse");
 let mut spans=f.segments();let slot=f.symbol("candidate_gu");
 let index=spans.iter().position(|s|s.runtime_address<=slot&&slot<s.runtime_address+s.size as u64).unwrap();
 let s=&mut spans[index];let shift=(slot-s.runtime_address)as usize;s.bytes=unsafe{s.bytes.add(shift)};s.runtime_address=slot;s.size=1;
 assert_eq!(candidate(&f.view(&spans),1).0,5,"one supplied slot byte is insufficient for candidate admission");
}
#[test]
fn mapped_candidates_preserve_missing_and_incomplete_refusals(){
 for(name,id,expected)in [("missing-free",1,3),("incomplete-gu",1,5),("incomplete-malloc",3,5)]{
  let f=Fixture::load(name);let spans=f.segments();assert_eq!(candidate(&f.view(&spans),id).0,expected,"{}",name);
 }
}
#[test]
fn mapped_candidates_preserve_ambiguity_clobber_and_relevant_incomplete_competitor(){
 for(name,id,expected)in [("ambiguous-gu",1,4),("ambiguous-fname",2,4),("ambiguous-malloc",3,4),("bad-argument",1,3),("bad-argument",2,3),("incomplete-malloc-competitor",3,5)]{
  let f=Fixture::load(name);let spans=f.segments();assert_eq!(candidate(&f.view(&spans),id).0,expected,"{}",name);
 }
}
#[test]
fn candidate_outputs_clear_on_invalid_view_and_preserve_original_dispatch(){
 use crate::mapped_candidates::*;
 assert_eq!(core::mem::size_of::<PsMappedFNameCandidateV1>(),32);assert_eq!(core::mem::offset_of!(PsMappedFNameCandidateV1,reserved),28);
 assert_eq!(core::mem::size_of::<PsMappedGMallocCandidateV1>(),40);assert_eq!(core::mem::offset_of!(PsMappedGMallocCandidateV1,proof_count),32);
 let f=Fixture::load("positive");let mut spans=f.segments();let view=f.view(&spans);
 for id in [2,3,5]{let mut original=crate::mapped_api::PsMappedCoreResult::default();assert_eq!(unsafe{crate::mapped_api::ps_resolve_mapped_core_v1(&view,id,&mut original)},3);assert_eq!(original.address,0);}
 let mut out=PsMappedGMallocCandidateV1{address:123,..Default::default()};assert_eq!(unsafe{ps_gmalloc_deletefile_candidate_mapped_v1(core::ptr::null(),&mut out)},1);assert_eq!(out.address,0);
 let mut bad=f.view(&spans);bad.format=2;assert_eq!(unsafe{ps_gmalloc_deletefile_candidate_mapped_v1(&bad,&mut out)},2);
 spans[0].runtime_address+=1;assert_eq!(candidate(&f.view(&spans),3).0,2);
 assert_eq!(unsafe{ps_gmalloc_deletefile_candidate_mapped_v1(&view,core::ptr::null_mut())},1);
}
#[test]
fn mapped_gmalloc_requires_executable_null_return_path(){
 use iced_x86::{Decoder,DecoderOptions};
 let f=Fixture::load("positive");let entry=f.symbol("malloc_free");let offset=(entry-f.address(0))as usize;
 let mut decoder=Decoder::with_ip(64,&f.storage[offset..],entry,DecoderOptions::NONE);let _=decoder.decode();let branch=decoder.decode();let null_return=branch.near_branch_target();
 let mut result=vec![];
 for s in f.segments(){
  if s.runtime_address<=null_return&&null_return<s.runtime_address+s.size as u64{
   let left=(null_return-s.runtime_address)as usize;assert!(left>0);
   result.push(PsMappedSegment{bytes:s.bytes,size:left,runtime_address:s.runtime_address,permissions:s.permissions,reserved:0});
   result.push(PsMappedSegment{bytes:unsafe{s.bytes.add(left)},size:1,runtime_address:null_return,permissions:1,reserved:0});
   if left+1<s.size{result.push(PsMappedSegment{bytes:unsafe{s.bytes.add(left+1)},size:s.size-left-1,runtime_address:null_return+1,permissions:s.permissions,reserved:0});}
  }else{result.push(s);}
 }
 assert_eq!(candidate(&f.view(&result),3).0,5,"non-executable decoded null-return path must refuse");
}
