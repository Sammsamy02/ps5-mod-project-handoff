//! Typed synchronous caller-borrow ABI. No address discovery or registration.
use alloc::vec::Vec;
use core::mem::{align_of,size_of};
use crate::{mapped_image::{MappedImage,MappedSegment,MappedFormat,MappedExtent},ps_scan_abi::{PsMappedImageViewV1,PsMappedSegment,PsMappedExtent},function_ranges::MetadataError};
#[repr(C)]#[derive(Clone,Copy,Debug,Default)]
pub struct PsMappedCoreResult{pub address:u64,pub major:u16,pub minor:u16,pub reserved:u32}
const _:()={assert!(size_of::<PsMappedCoreResult>()==16);assert!(align_of::<PsMappedCoreResult>()==8);};
fn aligned<T>(p:*const T)->bool{!p.is_null()&&(p as usize)%align_of::<T>()==0}
fn extent(e:&PsMappedExtent)->Result<Option<MappedExtent>,()>{
 if e.address==0&&e.size==0{return Ok(None);}
 if e.address==0||e.size==0||e.size>isize::MAX as usize||e.address.checked_add(e.size as u64).is_none(){return Err(());}
 Ok(Some(MappedExtent{address:e.address,size:e.size}))
}
/// Caller supplies valid descriptor and array storage and valid immutable owned
/// segment borrows. Non-aliasing writable output is distinct from ALL descriptor,
/// array and byte spans. Root's service lease, if used, remains held throughout.
/// Permission facts and storage must remain stable; no numeric label authorizes
/// constructing a slice. Only the explicitly supplied bytes pointers are read.
pub(crate) unsafe fn from_view<'a>(view:&PsMappedImageViewV1)->Result<MappedImage<'a>,()>{
 if view.abi_version!=1||!matches!(view.format,1|2)||!aligned(view.segments)||view.segment_count==0||view.segment_count.checked_mul(size_of::<PsMappedSegment>()).is_none_or(|n|n>isize::MAX as usize){return Err(());}
 let array_bytes=view.segment_count.checked_mul(size_of::<PsMappedSegment>()).ok_or(())?;
 if (view.segments as usize).checked_add(array_bytes).is_none(){return Err(());}
 let raw=unsafe{core::slice::from_raw_parts(view.segments,view.segment_count)};
 let mut segments=Vec::with_capacity(raw.len());
 for s in raw{
  if s.reserved!=0||s.bytes.is_null()||s.size==0||s.size>isize::MAX as usize||s.runtime_address!=s.bytes as usize as u64||s.runtime_address.checked_add(s.size as u64).is_none(){return Err(());}
  // Valid storage/retained ownership is the caller's explicit C borrow contract,
  // not inferred from a numeric runtime address or permission bit.
  segments.push(MappedSegment{bytes:unsafe{core::slice::from_raw_parts(s.bytes,s.size)},runtime_address:s.runtime_address,permissions:s.permissions});
 }
 let frame=extent(&view.eh_frame)?;let header=extent(&view.eh_frame_hdr)?;let exceptions=extent(&view.pe_exception_directory)?;
 let format=match view.format{
  1=>{if view.image_base!=0||exceptions.is_some(){return Err(());}MappedFormat::Elf64{text_base:view.text_base,eh_frame:frame,eh_frame_hdr:header}},
  2=>{if frame.is_some()||header.is_some(){return Err(());}MappedFormat::Pe64{text_base:view.text_base,image_base:view.image_base,exception_directory:exceptions}},
  _=>return Err(()),
 };
 MappedImage::new(format,segments).map_err(|_|())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_resolve_mapped_core_v1(view:*const PsMappedImageViewV1,resolver_id:u32,out:*mut PsMappedCoreResult)->i32{
 // Invalid/unaligned output cannot safely be written. Valid output is cleared on
 // every subsequent failure. Storage and non-aliasing are caller obligations.
 if !aligned(out){return 1;}
 unsafe{out.write(PsMappedCoreResult::default())};
 if !aligned(view)||!(1..=7).contains(&resolver_id){return 1;}
 let Ok(image)=(unsafe{from_view(&*view)})else{return 2;};
 if resolver_id==1{return match image.resolve_engine_version(){Ok(v)=>{unsafe{out.write(PsMappedCoreResult{address:0,major:v.major,minor:v.minor,reserved:0})};0},Err(e) if e.message.starts_with("found ")=>4,Err(_)=>3};}
 let result=match resolver_id{
  2=>image.resolve_guobject_array(),3=>image.resolve_fname_ctor_wchar(),4=>image.resolve_fname_to_string(),
  5=>image.resolve_gmalloc(),6=>image.resolve_static_construct(),7=>image.resolve_ugameengine_tick(),_=>unreachable!(),
 };
 match result{
  Ok(address)=>{unsafe{out.write(PsMappedCoreResult{address,major:0,minor:0,reserved:0})};0},
  Err(e) if e.message.starts_with("found ")=>4,
  Err(e)=>{
   // A genuine successful pattern branch need not depend on unwind metadata.
   // For failed root-dependent resolution, preserve absence vs corruption.
   let metadata=image.image().function_records();
   if matches!(metadata,Err(MetadataError::Malformed|MetadataError::OutOfFile|MetadataError::IndirectPointer|MetadataError::Ambiguous)){return 5;}
   if e.message=="expected at least one value"||e.message=="cannot find address"||e.message=="function-root groups unavailable"||matches!(metadata,Err(MetadataError::Unavailable))&&e.message.contains("out of bounds"){3}else{5}
  },
 }
}
