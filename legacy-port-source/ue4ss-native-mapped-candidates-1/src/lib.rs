#![cfg_attr(feature="native-runtime",no_std)]
extern crate alloc;
use alloc::{vec,vec::Vec,string::String};
use object::{Object,ObjectSection,SectionFlags};
mod runtime;
mod bytes_api;
pub mod engine_version;
pub mod guobject_array;
pub mod xrefs;
pub mod function_ranges;
pub use function_ranges::{ImageKind,OwnedFunction,RootFunction};
pub mod game_loop;
pub mod fname;
pub mod fname_util;
pub mod object_cfg;

pub mod resolvers {
 use alloc::{vec,vec::Vec,string::String};
 use core::fmt::Debug;
 pub struct AsyncContext<'a>{pub(crate) exe:&'a super::OwnedImage<'a>}
 impl AsyncContext<'_>{
  pub fn image(&self)->&super::OwnedImage<'_>{self.exe}
  pub fn scan_tagged2<T:Copy>(&self,tag:T,p:scanner::Pattern)->Vec<(T,u64)>{self.scan(p).into_iter().map(|a|(tag,a)).collect()}
  pub fn scan_tagged<T>(&self,tag:T,p:scanner::Pattern)->(T,scanner::Pattern,Vec<u64>){
   let addresses=self.scan(p.clone());(tag,p,addresses)
  }
  pub fn scan_many(&self,patterns:&[scanner::Pattern])->Vec<Vec<u64>> {
   let mut bins:Vec<Vec<u64>>=patterns.iter().map(|_|Vec::new()).collect();
   let refs:Vec<_>=patterns.iter().collect();
   for section in &self.exe.memory.sections {
    for (out,hits) in bins.iter_mut().zip(scanner::scan_pattern(&refs,section.address as usize,section.data)) {
     out.extend(hits.into_iter().map(|a|a as u64));
    }
   }
   bins
  }
  pub fn scan(&self,p:scanner::Pattern)->Vec<u64>{
   self.exe.memory.sections.iter().flat_map(|s|scanner::scan_pattern(&[&p],s.address as usize,s.data).remove(0).into_iter().map(|a|a as u64)).collect()
  }
 }
 pub trait Context<T>{fn context(self,message:&str)->Result<T,ResolveError>;}
 impl<T> Context<T> for Option<T>{fn context(self,message:&str)->Result<T,ResolveError>{self.ok_or_else(||ResolveError::new_msg(message))}}
 #[derive(Debug)]pub struct ResolveError{pub message:String}
 impl ResolveError{pub fn new_msg(s:impl Into<String>)->Self{Self{message:s.into()}}}
 impl From<()> for ResolveError {fn from(_:())->Self{Self::new_msg("serialized section operand is out of bounds or overflows")}}
 impl core::fmt::Display for ResolveError{fn fmt(&self,f:&mut core::fmt::Formatter<'_>)->core::fmt::Result{core::fmt::Display::fmt(&self.message,f)}}
 pub fn ensure_one<T:Debug+PartialEq>(data:impl IntoIterator<Item=T>)->Result<T,ResolveError>{try_ensure_one(data.into_iter().map(|v|Ok(v)))}
 pub fn try_ensure_one<T:Debug+PartialEq>(data:impl IntoIterator<Item=Result<T,ResolveError>>)->Result<T,ResolveError>{
  let mut reached_max=false;
  let mut unique=vec![];
  for value in data.into_iter(){let value=value?;if !unique.contains(&value){unique.push(value);}if unique.len()>=4{reached_max=true;break;}}
  match unique.len(){0=>Err(ResolveError::new_msg("expected at least one value")),1=>Ok(unique.swap_remove(0)),len=>Err(ResolveError::new_msg(alloc::format!("found {}{len} unique values {unique:X?}",if reached_max{">="}else{""})))}
 }
}
pub mod util {
 use alloc::vec::Vec;
 pub fn utf16(s:&str)->Vec<u8>{s.encode_utf16().flat_map(u16::to_le_bytes).collect()}
 pub fn utf16_pattern(s:&str)->scanner::Pattern{scanner::Pattern::from_bytes(utf16(s)).unwrap()}
}
pub mod disassemble {
 pub use crate::cfg_disassemble::{Control,disassemble};
 use iced_x86::{Decoder,DecoderOptions,Instruction};
 use super::{MemoryTrait,OwnedImage};
 pub fn disassemble_single(image:&OwnedImage<'_>,address:u64)->Result<Option<Instruction>,()> {
  Ok(Decoder::with_ip(64,image.memory.range_from(address)?,address,DecoderOptions::NONE).iter().next())
 }
}
pub struct OwnedSection<'a>{address:u64,data:&'a [u8]}
impl OwnedSection<'_>{
 pub fn address(&self)->u64{self.address}
 pub fn data(&self)->&[u8]{self.data}
 pub fn matches(&self,p:&scanner::Pattern,index:usize)->bool{
  index.checked_add(p.simple.len()).is_some_and(|end|end<=self.data.len()) && p.is_match(self.data,self.address as usize,index)
 }
}

pub struct OwnedMemory<'a>{sections:Vec<OwnedSection<'a>>}
pub struct OwnedImage<'a>{serialized_bytes:&'a [u8],pub memory:OwnedMemory<'a>,pub kind:ImageKind,function_ranges:Result<function_ranges::FunctionRanges,function_ranges::MetadataError>}
impl<'a> OwnedImage<'a>{
 pub fn serialized_file_bytes(&self)->&[u8]{self.serialized_bytes}
 fn with_metadata(bytes:&'a [u8],sections:Vec<OwnedSection<'a>>)->Self {let kind=if bytes.starts_with(b"\x7fELF"){ImageKind::Elf}else{ImageKind::PE};let mut image=Self{serialized_bytes:bytes,memory:OwnedMemory{sections},kind,function_ranges:Err(function_ranges::MetadataError::Unavailable)};image.function_ranges=function_ranges::FunctionRanges::parse(bytes,&image);image}
 pub fn get_function(&self,address:u64)->Result<Option<OwnedFunction>,()> {self.function_ranges.as_ref().map_err(|_|())?.get_function(address).map_err(|_|())}
 pub fn get_root_function(&self,address:u64)->Result<Option<RootFunction>,()> {self.function_ranges.as_ref().map_err(|_|())?.get_root_function(address).map_err(|_|())}
 pub fn get_root_function_range(&self,address:u64)->Result<Option<core::ops::Range<u64>>,()> {Ok(self.get_root_function(address)?.map(|f|f.range))}
 pub fn get_root_functions(&self)->Result<Vec<core::ops::Range<u64>>,()> {Ok(self.function_ranges.as_ref().map_err(|_|())?.get_root_functions())}
 pub fn get_child_functions(&self,address:u64)->Result<Vec<OwnedFunction>,()> {self.function_ranges.as_ref().map_err(|_|())?.get_child_functions(address).map_err(|_|())}
 pub fn function_records(&self)->Result<&[OwnedFunction],function_ranges::MetadataError>{self.function_ranges.as_ref().map(|f|f.records()).map_err(|e|*e)}
}

impl<'a> OwnedImage<'a>{
 pub fn parse(bytes:&'a [u8])->Result<Self,()> {
  // Executable PT_LOAD file extents are independent of ELF section tables.
  // Validate every load span before falling back from stale/absent sections.
  let is_elf=bytes.starts_with(b"\x7fELF");
  let load_spans=if is_elf {Some(Self::elf_load_spans(bytes)?)} else {None};
  let file=match object::File::parse(bytes) {
   Ok(file)=>file,
   Err(_)=>return load_spans.filter(|s|!s.is_empty()).map(|sections|Self::with_metadata(bytes,sections)).ok_or(()),
  };
  if !matches!(file,object::File::Elf64(_)|object::File::Pe64(_)) || file.architecture()!=object::Architecture::X86_64 || !file.is_little_endian(){return Err(());}
  let mut sections=vec![];
  for s in file.sections(){
   let readable=match s.flags(){SectionFlags::Elf{sh_flags}=>sh_flags&object::elf::SHF_ALLOC as u64!=0,SectionFlags::Coff{characteristics}=>characteristics&object::pe::IMAGE_SCN_MEM_READ!=0,_=>false};
   if !readable {continue;}
   // Serialized file sections only. No compression, relocations or mapped-memory fiction.
   if s.compressed_file_range().map_err(|_|())?.format!=object::CompressionFormat::None{return Err(());}
   let data=s.data().map_err(|_|())?;if data.is_empty(){continue;}
   s.address().checked_add(data.len() as u64).ok_or(())?;
   sections.push(OwnedSection{address:s.address(),data});
  }
  if sections.is_empty() {if let Some(loads)=load_spans {sections=loads;}}
  Ok(Self::with_metadata(bytes,sections))
 }
 fn elf_load_spans(bytes:&'a [u8])->Result<Vec<OwnedSection<'a>>,()> {
  use object::read::elf::{FileHeader,ProgramHeader};
  let header=object::elf::FileHeader64::<object::LittleEndian>::parse(bytes).map_err(|_|())?;
  let endian=object::LittleEndian;
  if !header.is_class_64() || !header.is_little_endian() || header.e_machine(endian)!=object::elf::EM_X86_64 || header.e_version(endian)!=1 || header.e_ehsize(endian)!=64{return Err(());}
  let headers=header.program_headers(endian,bytes).map_err(|_|())?;
  let mut sections=vec![];
  for ph in headers {
   if ph.p_type(endian)!=object::elf::PT_LOAD {continue;}
   let offset=ph.p_offset(endian);let size=ph.p_filesz(endian);let mem=ph.p_memsz(endian);let address=ph.p_vaddr(endian);
   let end=offset.checked_add(size).ok_or(())?;address.checked_add(mem).ok_or(())?;
   if size>mem || end>bytes.len() as u64 {return Err(());}
   // File-backed R/X spans only. Permissions are metadata, not a native
   // readable-memory claim. The unbacked memsz tail is never materialized.
   if size==0 || ph.p_flags(endian)&(object::elf::PF_R|object::elf::PF_X)==0 {continue;}
   let data=bytes.get(offset as usize..end as usize).ok_or(())?;
   sections.push(OwnedSection{address,data});
  }
  Ok(sections)
 }
}
pub trait MemoryTrait {
 fn range_from(&self,address:u64)->Result<&[u8],()>;
 fn range(&self,range:core::ops::Range<u64>)->Result<&[u8],()>{
  let length=usize::try_from(range.end.checked_sub(range.start).ok_or(())?).map_err(|_|())?;
  let data=self.range_from(range.start)?;data.get(..length).ok_or(())
 }

 fn u16_le(&self,address:u64)->Result<u16,()>{let d=self.range_from(address)?;Ok(u16::from_le_bytes(d.get(..2).ok_or(())?.try_into().map_err(|_|())?))}
 fn u32_le(&self,address:u64)->Result<u32,()>{let d=self.range_from(address)?;Ok(u32::from_le_bytes(d.get(..4).ok_or(())?.try_into().map_err(|_|())?))}
 fn rip4(&self,address:u64)->Result<u64,()>{address.checked_add(4).and_then(|next|next.checked_add_signed(self.u32_le(address).ok()? as i32 as i64)).ok_or(())}
 fn read_wstring(&self,address:u64)->Result<String,()>{
  let data=self.range_from(address)?;let mut words=vec![];
  for pair in data.chunks_exact(2){let word=u16::from_le_bytes([pair[0],pair[1]]);if word==0{return String::from_utf16(&words).map_err(|_|());}words.push(word);}
  // An unterminated file-span string is unavailable; never continue into another span.
  Err(())
 }
}
impl MemoryTrait for OwnedMemory<'_>{
 fn range_from(&self,address:u64)->Result<&[u8],()>{
  self.sections.iter().find_map(|s|address.checked_sub(s.address).filter(|n|*n<(s.data.len() as u64)).map(|n|&s.data[n as usize..])).ok_or(())
 }
}
#[repr(C)]pub struct PsOwnedEngineVersion{pub major:u16,pub minor:u16}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_engine_version_file_image(data:*const u8,length:usize,out:*mut PsOwnedEngineVersion)->i32{
 if out.is_null() || data.is_null() || length==0 || length>isize::MAX as usize{return 1;}
 unsafe{out.write(PsOwnedEngineVersion{major:0,minor:0})};
 let bytes=unsafe{core::slice::from_raw_parts(data,length)};
 let Ok(image)=OwnedImage::parse(bytes) else{return 2;};
 let ctx=resolvers::AsyncContext{exe:&image};
 match engine_version::resolve_engine_version(&ctx){Ok(v)=>{unsafe{out.write(PsOwnedEngineVersion{major:v.major,minor:v.minor})};0},Err(_)=>3}
}

impl OwnedMemory<'_>{
 pub fn get_section_containing(&self,address:u64)->Result<&OwnedSection<'_>,()>{
  self.sections.iter().find(|s|address.checked_sub(s.address).is_some_and(|n|n<s.data.len() as u64)).ok_or(())
 }
 pub fn captures(&self,p:&scanner::Pattern,address:u64)->Result<Option<Vec<scanner::Capture<'_>>>,()> {
  let s=self.sections.iter().find(|s|address.checked_sub(s.address).is_some_and(|n|n<s.data.len() as u64)).ok_or(())?;
  let index=usize::try_from(address.checked_sub(s.address).ok_or(())?).map_err(|_|())?;
  let width=p.simple.len();
  if index.checked_add(width).is_none_or(|end|end>s.data.len()) {return Err(());}
  if p.captures.iter().any(|c|c.start>c.end || c.end>width) || p.xrefs.iter().any(|(offset,_)|offset.checked_add(4).is_none_or(|end|end>width)){return Err(());}
  Ok(p.captures(s.data,usize::try_from(s.address).map_err(|_|())?,index))
 }
}
pub trait Addressable{fn rip(&self)->Result<u64,()>;}
impl Addressable for scanner::Capture<'_>{
 fn rip(&self)->Result<u64,()>{
  let displacement=i32::from_le_bytes(self.data.try_into().map_err(|_|())?) as i64;
  (self.address as u64).checked_add(4).and_then(|next|next.checked_add_signed(displacement)).ok_or(())
 }
}
// Caller owns valid readable input and a distinct writable aligned output for this
// synchronous call. A returned address is an operand-derived file-image label only;
// it is never read, called or certified to be a native engine singleton.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_guobject_array_file_image(data:*const u8,length:usize,out:*mut u64)->i32{
 if out.is_null() || data.is_null() || length==0 || length>isize::MAX as usize{return 1;}
 unsafe{out.write(0)};
 let bytes=unsafe{core::slice::from_raw_parts(data,length)};
 let Ok(image)=OwnedImage::parse(bytes) else{return 2;};
 let ctx=resolvers::AsyncContext{exe:&image};
 match guobject_array::resolve_guobject_array(&ctx){
  Ok(v)=>{unsafe{out.write(v.0)};0},
  Err(error) if error.message=="expected at least one value"=>3,
  Err(error) if error.message.starts_with("found ")=>4,
  Err(_)=>5,
 }
}
#[cfg(test)]
mod owned_memory_controls {
 use super::*;
 #[test] fn operands_cannot_cross_serialized_sections(){
  let left=[1u8,2,3];let right=[4u8,5,6,7];
  let memory=OwnedMemory{sections:vec![OwnedSection{address:100,data:&left},OwnedSection{address:103,data:&right}]};
  assert!(memory.u32_le(100).is_err());assert_eq!(memory.u32_le(103),Ok(0x07060504));
  assert!(memory.range_from(107).is_err());
 }
 #[test] fn checked_rip_rejects_both_arithmetic_directions(){
  let negative=i32::MIN.to_le_bytes();let positive=1i32.to_le_bytes();
  let low=OwnedMemory{sections:vec![OwnedSection{address:100,data:&negative}]};
  assert!(low.rip4(100).is_err());
  let high=scanner::Capture{address:usize::MAX-4,data:&positive};
  assert!(high.rip().is_err());
 }
 #[test] fn captures_call_genuine_pattern_only_with_a_complete_section_span(){
  let pattern=scanner::Pattern::new("8B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ]").unwrap();
  let bytes=[0x8b,0x05,0,0,0,0,0x2b,0x05,0,0,0,0,0x2b,0x05,0,0,0,0];
  let full=OwnedMemory{sections:vec![OwnedSection{address:0x1000,data:&bytes}]};
  let captures=full.captures(&pattern,0x1000).unwrap().unwrap();
  assert_eq!(captures.len(),3);assert_eq!(captures[0].rip(),Ok(0x1006));assert_eq!(captures[2].rip(),Ok(0x1012));
  let short=OwnedMemory{sections:vec![OwnedSection{address:0x1000,data:&bytes[..17]}]};
  assert!(short.captures(&pattern,0x1000).is_err());assert!(short.captures(&pattern,0x1011).is_err());
 }
}

// Explicit serialized-file endpoint. A returned address is a file-image label,
// not a native mapping or proof of a callable engine/game-thread function.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_ugameengine_tick_file_image(data:*const u8,length:usize,out:*mut u64)->i32 {
 if data.is_null() || out.is_null() || length==0 || length>isize::MAX as usize {return 1;}
 unsafe{out.write(0)};
 let Ok(image)=OwnedImage::parse(unsafe{core::slice::from_raw_parts(data,length)}) else{return 2;};
 let ctx=resolvers::AsyncContext{exe:&image};
 match game_loop::resolve_ugameengine_tick(&ctx) {Ok(value)=>{unsafe{out.write(value)};0},Err(error)=>{
  if error.message.starts_with("found "){4}else if error.message.contains("out of bounds"){5}else{3}
 }}
}

// Frozen FName endpoint bodies copied below; semantic GU fallback integration
// belongs at the existing genuine resolver entry after its source is supplied.
// Synchronous serialized-image lookup only: valid nonoverlapping input/output
// storage is the C caller's obligation. A success is a file-image label, not a
// callable native engine function or a proof of engine ABI compatibility.
unsafe fn fname_file_image(data:*const u8,length:usize,out:*mut u64,constructor:bool)->i32 {
 if out.is_null()||data.is_null()||length==0||length>isize::MAX as usize{return 1;}
 unsafe{out.write(0)};
 let bytes=unsafe{core::slice::from_raw_parts(data,length)};
 let Ok(image)=OwnedImage::parse(bytes)else{return 2;};
 let ctx=resolvers::AsyncContext{exe:&image};
 let result=match (constructor,image.kind){
  (true,ImageKind::Elf)=>fname::resolve_ctor_elf(&ctx).map(|v|v.0),
  (true,ImageKind::PE)=>fname::resolve_ctor_pe(&ctx).map(|v|v.0),
  (false,ImageKind::Elf)=>fname::resolve_to_string_elf(&ctx).map(|v|v.0),
  (false,ImageKind::PE)=>fname::resolve_to_string_pe(&ctx).map(|v|v.0),
 };
 match result{Ok(v)=>{unsafe{out.write(v)};0},Err(e)if e.message=="expected at least one value"=>3,Err(e)if e.message.starts_with("found ")=>4,Err(_)=>5}
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_fname_ctor_wchar_file_image(data:*const u8,length:usize,out:*mut u64)->i32{unsafe{fname_file_image(data,length,out,true)}}
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_fname_to_string_file_image(data:*const u8,length:usize,out:*mut u64)->i32{unsafe{fname_file_image(data,length,out,false)}}
#[cfg(test)]mod fname_controls;

#[repr(C)]#[derive(Clone,Copy,Default)]
pub struct PsObjectArrayStaticCandidate {
 pub address:u64,pub allocation_entry:u64,pub free_entry:u64,pub allocation_call:u64,pub free_call:u64,
 pub evidence_flags:u32,pub reserved:u32,
}
// Caller-owned serialized file bytes and distinct aligned output only. No runtime
// address/memory access occurs; status0 means STATIC evidence candidate only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_guobject_array_cfg_file_image(data:*const u8,length:usize,out:*mut PsObjectArrayStaticCandidate)->i32 {
 if data.is_null() || out.is_null() || length==0 || length>isize::MAX as usize{return 1;}
 unsafe{out.write(PsObjectArrayStaticCandidate::default())};
 let bytes=unsafe{core::slice::from_raw_parts(data,length)};
 match object_cfg::resolve_owned_file(bytes) {
  Ok(c)=>{unsafe{out.write(c)};0},
  Err(object_cfg::Error::Malformed)=>2,
  Err(object_cfg::Error::NoCandidate)=>3,
  Err(object_cfg::Error::Ambiguous)=>4,
  Err(object_cfg::Error::Incomplete)=>5,
 }
}


#[cfg(test)]mod object_cfg_owned_tests;

pub mod fname_call;
pub mod gmalloc;
pub mod static_construct_object;
pub mod cfg_disassemble;
pub mod required_util;
// Explicit ELF call-site candidate endpoint. Output is never a live pointer.
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_fname_ctor_call_file_image(data:*const u8,length:usize,out:*mut u64)->i32 {
 if out.is_null()||data.is_null()||length==0||length>isize::MAX as usize{return 1;}
 unsafe{out.write(0)};let bytes=unsafe{core::slice::from_raw_parts(data,length)};
 let Ok(image)=OwnedImage::parse(bytes)else{return 2;};let ctx=resolvers::AsyncContext{exe:&image};
 match fname_call::resolve(&ctx,bytes){Ok(v)=>{unsafe{out.write(v.address)};0},Err(fname_call::Error::NoCandidate)=>3,Err(fname_call::Error::Ambiguous)=>4,Err(fname_call::Error::Malformed)=>2,Err(fname_call::Error::Incomplete)=>5}
}

// Serialized caller-file addresses only; no live singleton/function use.
unsafe fn required_file_image(data:*const u8,length:usize,out:*mut u64,is_malloc:bool)->i32{
 if data.is_null()||out.is_null()||length==0||length>isize::MAX as usize{return 1;}unsafe{out.write(0)};
 let Ok(image)=OwnedImage::parse(unsafe{core::slice::from_raw_parts(data,length)})else{return 2;};let ctx=resolvers::AsyncContext{exe:&image};
 let result=if is_malloc{gmalloc::resolve_gmalloc(&ctx).map(|v|v.0)}else{static_construct_object::resolve_static_construct(&ctx).map(|v|v.0)};
 match result{Ok(v)=>{unsafe{out.write(v)};0},Err(e)=>if e.message=="expected at least one value"{3}else if e.message.starts_with("found "){4}else{5}}
}
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_gmalloc_file_image(data:*const u8,length:usize,out:*mut u64)->i32{unsafe{required_file_image(data,length,out,true)}}
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_static_construct_object_file_image(data:*const u8,length:usize,out:*mut u64)->i32{unsafe{required_file_image(data,length,out,false)}}

// Serialized ELF static candidate only; no live singleton/provider wiring.
pub mod deletefile_candidate;
#[unsafe(no_mangle)]pub unsafe extern "C" fn ps_gmalloc_deletefile_candidate_file_image(data:*const u8,length:usize,out:*mut u64)->i32{
 if data.is_null()||out.is_null()||length==0||length>isize::MAX as usize{return 1;}unsafe{out.write(0)};
 let Ok(image)=OwnedImage::parse(unsafe{core::slice::from_raw_parts(data,length)})else{return 2;};let ctx=resolvers::AsyncContext{exe:&image};
 match deletefile_candidate::resolve_deletefile_candidate(&ctx){Ok(v)=>{unsafe{out.write(v)};0},Err(e)=>if e.message=="expected at least one value"{3}else if e.message.starts_with("found "){4}else{5}}
}

pub mod mapped_image;
pub mod ps_scan_abi;
pub mod mapped_api;

#[cfg(test)]mod mapped_owned_controls;

pub mod build_properties;
pub mod mapped_property_api;
#[cfg(test)]mod property_owned_controls;

mod mapped_metadata;

#[cfg(test)]mod mapped_candidate_controls;

pub mod mapped_candidates;
