#![cfg_attr(feature="native-runtime",no_std)]
extern crate alloc;
use alloc::{vec,vec::Vec,string::String};
use object::{Object,ObjectSection,SectionFlags};
mod runtime;
pub mod engine_version;

pub mod resolvers {
 use alloc::{vec,vec::Vec,string::String};
 use core::fmt::Debug;
 pub struct AsyncContext<'a>{pub(crate) exe:&'a super::OwnedImage<'a>}
 impl AsyncContext<'_>{
  pub fn image(&self)->&super::OwnedImage<'_>{self.exe}
  pub fn scan(&self,p:scanner::Pattern)->Vec<u64>{
   self.exe.memory.sections.iter().flat_map(|s|scanner::scan_pattern(&[&p],s.address as usize,s.data).remove(0).into_iter().map(|a|a as u64)).collect()
  }
 }
 #[derive(Debug)]pub struct ResolveError{pub message:String}
 impl ResolveError{pub fn new_msg(s:impl Into<String>)->Self{Self{message:s.into()}}}
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
 use iced_x86::{Decoder,DecoderOptions,Instruction};
 use super::{MemoryTrait,OwnedImage};
 pub fn disassemble_single(image:&OwnedImage<'_>,address:u64)->Result<Option<Instruction>,()> {
  Ok(Decoder::with_ip(64,image.memory.range_from(address)?,address,DecoderOptions::NONE).iter().next())
 }
}
struct OwnedSection<'a>{address:u64,data:&'a [u8]}
pub struct OwnedMemory<'a>{sections:Vec<OwnedSection<'a>>}
pub struct OwnedImage<'a>{pub memory:OwnedMemory<'a>}
impl<'a> OwnedImage<'a>{
 pub fn parse(bytes:&'a [u8])->Result<Self,()> {
  let file=object::File::parse(bytes).map_err(|_|())?;
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
  Ok(Self{memory:OwnedMemory{sections}})
 }
}
pub trait MemoryTrait {
 fn range_from(&self,address:u64)->Result<&[u8],()>;
 fn u16_le(&self,address:u64)->Result<u16,()>{let d=self.range_from(address)?;Ok(u16::from_le_bytes(d.get(..2).ok_or(())?.try_into().map_err(|_|())?))}
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
