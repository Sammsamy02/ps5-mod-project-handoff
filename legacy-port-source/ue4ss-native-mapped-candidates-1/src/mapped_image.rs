//! Borrowed caller-owned mapped bytes. No discovery, mapping, registration,
//! relocations, pointer dereference outside borrowed spans, or engine setters.
use alloc::vec::Vec;
use crate::{OwnedImage,OwnedMemory,OwnedSection,ImageKind,
 function_ranges::{FunctionRanges,MetadataError},resolvers::{AsyncContext,ResolveError}};

pub const READ:u32=1;
pub const WRITE:u32=2;
pub const EXECUTE:u32=4;
#[derive(Clone,Copy)]
pub struct MappedSegment<'a>{pub bytes:&'a [u8],pub runtime_address:u64,pub permissions:u32}
#[derive(Clone,Copy,Debug)]
pub struct MappedExtent{pub address:u64,pub size:usize}
#[derive(Clone,Copy)]
pub enum MappedFormat{
 Elf64{text_base:u64,eh_frame:Option<MappedExtent>,eh_frame_hdr:Option<MappedExtent>},
 Pe64{text_base:u64,image_base:u64,exception_directory:Option<MappedExtent>},
}
#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum InputError{Empty,Unreadable,Permissions,AddressMismatch,Overflow,Overlap,MetadataExtent,TextExtent,ImageBase}

pub struct MappedImage<'a>{image:OwnedImage<'a>,segments:Vec<MappedSegment<'a>>}
impl<'a> MappedImage<'a>{
 /// Bytes must be genuinely caller-owned mapped image storage, already
 /// materialized/relocated by the caller. These are actual slice addresses.
 /// The borrow retains Rust storage; unsafe external protection/lifetime
 /// changes must be prevented for the complete synchronous resolver use.
 /// Permission flags are caller facts, not OS queries or service registrations.
 pub fn new(format:MappedFormat,mut segments:Vec<MappedSegment<'a>>)->Result<Self,InputError>{
  if segments.is_empty(){return Err(InputError::Empty);}
  for s in &segments{
   if s.bytes.is_empty(){return Err(InputError::Empty);}
   if s.permissions==0||s.permissions & !(READ|WRITE|EXECUTE)!=0{return Err(InputError::Permissions);}
   if s.permissions & READ==0{return Err(InputError::Unreadable);}
   if s.runtime_address==0||s.runtime_address!=s.bytes.as_ptr() as usize as u64{return Err(InputError::AddressMismatch);}
   s.runtime_address.checked_add(s.bytes.len() as u64).ok_or(InputError::Overflow)?;
  }
  segments.sort_by_key(|s|s.runtime_address);
  if segments.windows(2).any(|p|p[0].runtime_address+p[0].bytes.len() as u64>p[1].runtime_address){return Err(InputError::Overlap);}
  let sections=segments.iter().map(|s|OwnedSection{address:s.runtime_address,data:s.bytes}).collect();
  let kind=match format{MappedFormat::Elf64{..}=>ImageKind::Elf,MappedFormat::Pe64{..}=>ImageKind::PE};
  let mut image=OwnedImage{serialized_bytes:&[],memory:OwnedMemory{sections},kind,function_ranges:Err(MetadataError::Unavailable)};
  let extent_bytes=|extent:MappedExtent| {
   crate::mapped_metadata::bytes(&image,extent.address,extent.size)
    .map(|bytes|(extent.address,bytes)).map_err(|_|InputError::MetadataExtent)
  };
  image.function_ranges=match format{
   MappedFormat::Elf64{text_base,eh_frame,eh_frame_hdr}=>{
    if !segments.iter().any(|s|s.permissions&EXECUTE!=0&&s.runtime_address<=text_base&&text_base<s.runtime_address+s.bytes.len() as u64){return Err(InputError::TextExtent);}
    let frame=eh_frame.map(extent_bytes).transpose()?;
    let header=eh_frame_hdr.map(extent_bytes).transpose()?;
    FunctionRanges::elf_from_mapped_spans(&image,text_base,frame.as_ref().map(|(a,b)|(*a,b.as_ref())),header.as_ref().map(|(a,b)|(*a,b.as_ref())))
   },
   MappedFormat::Pe64{text_base,image_base,exception_directory}=>{
    if !segments.iter().any(|s|s.permissions&EXECUTE!=0&&s.runtime_address<=text_base&&text_base<s.runtime_address+s.bytes.len() as u64){return Err(InputError::TextExtent);}
    if !segments.iter().any(|s|s.runtime_address==image_base){return Err(InputError::ImageBase);}
    match exception_directory{Some(e)=>{let (_,table)=extent_bytes(e)?;FunctionRanges::pe_from_mapped_table(&image,image_base,table.as_ref())},None=>Err(MetadataError::Unavailable)}
   },
  };
  Ok(Self{image,segments})
 }
 pub fn image(&self)->&OwnedImage<'_>{&self.image}
 pub(crate) fn segments(&self)->&[MappedSegment<'a>]{&self.segments}
 pub fn permissions(&self,address:u64,size:usize)->Option<u32>{
  if size==0{return None;}
  let end=address.checked_add(size as u64)?;
  self.segments.iter().find(|s|s.runtime_address<=address&&end<=s.runtime_address+s.bytes.len() as u64).map(|s|s.permissions)
 }
 pub fn resolve_engine_version(&self)->Result<crate::engine_version::EngineVersion,ResolveError>{crate::engine_version::resolve_engine_version(&AsyncContext{exe:&self.image})}
 /// Genuine original GU pattern body only. Serialized-file CFG is not a
 /// mapped-memory provider and is intentionally never invoked here.
 pub fn resolve_guobject_array(&self)->Result<u64,ResolveError>{crate::guobject_array::resolve_guobject_array_patterns(&AsyncContext{exe:&self.image}).map(|v|v.0)}
 pub fn resolve_fname_ctor_wchar(&self)->Result<u64,ResolveError>{let ctx=AsyncContext{exe:&self.image};match self.image.kind{ImageKind::Elf=>crate::fname::resolve_ctor_elf(&ctx).map(|v|v.0),ImageKind::PE=>crate::fname::resolve_ctor_pe(&ctx).map(|v|v.0)}}
 pub fn resolve_fname_to_string(&self)->Result<u64,ResolveError>{let ctx=AsyncContext{exe:&self.image};match self.image.kind{ImageKind::Elf=>crate::fname::resolve_to_string_elf(&ctx).map(|v|v.0),ImageKind::PE=>crate::fname::resolve_to_string_pe(&ctx).map(|v|v.0)}}
 pub fn resolve_gmalloc(&self)->Result<u64,ResolveError>{crate::gmalloc::resolve_gmalloc(&AsyncContext{exe:&self.image}).map(|v|v.0)}
 pub fn resolve_static_construct(&self)->Result<u64,ResolveError>{crate::static_construct_object::resolve_static_construct(&AsyncContext{exe:&self.image}).map(|v|v.0)}
 pub fn resolve_ugameengine_tick(&self)->Result<u64,ResolveError>{crate::game_loop::resolve_ugameengine_tick(&AsyncContext{exe:&self.image})}
}
