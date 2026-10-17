use alloc::{vec,vec::Vec};
use core::ops::Range;
use object::{Object,ObjectSection};
use crate::{MemoryTrait,OwnedImage};
#[derive(Clone,Copy,Debug,Eq,PartialEq)]pub enum ImageKind{Elf,PE}
#[derive(Clone,Debug,Eq,PartialEq)]pub struct OwnedFunction{pub range:Range<u64>,pub unwind:u64}
pub type RootFunction=OwnedFunction;
#[derive(Clone,Copy,Debug,Eq,PartialEq)]pub enum MetadataError{Unavailable,Malformed,OutOfFile,IndirectPointer,Ambiguous}
pub struct FunctionRanges{records:Vec<OwnedFunction>,roots:Vec<OwnedFunction>,pub kind:ImageKind}
impl FunctionRanges{
 pub fn parse(bytes:&[u8],image:&OwnedImage<'_>)->Result<Self,MetadataError>{if bytes.starts_with(b"\x7fELF"){Self::elf(bytes,image)}else{Self::pe(bytes,image)}}
 pub fn records(&self)->&[OwnedFunction]{&self.records}
 pub fn get_function(&self,address:u64)->Result<Option<OwnedFunction>,MetadataError>{let mut found=self.records.iter().enumerate().filter(|(_,f)|f.range.contains(&address));let first=found.next();if found.next().is_some(){return Err(MetadataError::Ambiguous);}Ok(first.map(|(_,f)|f.clone()))}
 pub fn get_root_function(&self,address:u64)->Result<Option<OwnedFunction>,MetadataError>{let mut found=self.records.iter().enumerate().filter(|(_,f)|f.range.contains(&address));let first=found.next();if found.next().is_some(){return Err(MetadataError::Ambiguous);}Ok(first.map(|(i,_)|self.roots[i].clone()))}
 pub fn get_root_functions(&self)->Vec<Range<u64>>{let mut roots=self.roots.clone();roots.sort_by_key(|f|(f.range.start,f.range.end,f.unwind));roots.dedup();roots.into_iter().map(|f|f.range).collect()}
 pub fn get_child_functions(&self,address:u64)->Result<Vec<OwnedFunction>,MetadataError>{let Some(root)=self.get_root_function(address)? else{return Ok(vec![]);};Ok(self.records.iter().zip(&self.roots).filter(|(_,r)|**r==root).map(|(f,_)|f.clone()).collect())}
 fn checked_function(image:&OwnedImage<'_>,start:u64,length:u64,unwind:u64)->Result<OwnedFunction,MetadataError>{let end=start.checked_add(length).ok_or(MetadataError::Malformed)?;if length==0{return Err(MetadataError::Malformed);}let available=image.memory.range_from(start).map_err(|_|MetadataError::OutOfFile)?;if length>available.len() as u64{return Err(MetadataError::OutOfFile);}Ok(OwnedFunction{range:start..end,unwind})}
 fn elf(bytes:&[u8],image:&OwnedImage<'_>)->Result<Self,MetadataError>{
  use object::read::elf::{FileHeader,ProgramHeader};
  let header=object::elf::FileHeader64::<object::LittleEndian>::parse(bytes).map_err(|_|MetadataError::Malformed)?;let endian=object::LittleEndian;
  let phs=header.program_headers(endian,bytes).map_err(|_|MetadataError::Malformed)?;
  let mut text=phs.iter().filter(|p|p.p_type(endian)==object::elf::PT_LOAD&&p.p_flags(endian)&object::elf::PF_X!=0).map(|p|p.p_vaddr(endian)).min().ok_or(MetadataError::Unavailable)?;
  let mut frame_data=None;let mut hdr_data=None;
  if let Ok(file)=object::File::parse(bytes){
   if let Some(s)=file.section_by_name(".text"){text=s.address();}
   if let Some(s)=file.section_by_name(".eh_frame"){frame_data=Some((s.address(),s.data().map_err(|_|MetadataError::OutOfFile)?));}
   if let Some(s)=file.section_by_name(".eh_frame_hdr"){hdr_data=Some((s.address(),s.data().map_err(|_|MetadataError::OutOfFile)?));}
  }
  if hdr_data.is_none(){if let Some(p)=phs.iter().find(|p|p.p_type(endian)==object::elf::PT_GNU_EH_FRAME){let end=p.p_offset(endian).checked_add(p.p_filesz(endian)).ok_or(MetadataError::OutOfFile)?;let d=bytes.get(p.p_offset(endian) as usize..usize::try_from(end).map_err(|_|MetadataError::OutOfFile)?).ok_or(MetadataError::OutOfFile)?;hdr_data=Some((p.p_vaddr(endian),d));}}
  Self::elf_from_spans(image,text,frame_data,hdr_data)
 }
 // Shared real gimli/FDE parser: addresses and bytes supplied by the image.
 pub(crate) fn elf_from_spans<'a>(image:&'a OwnedImage<'a>,text:u64,frame_data:Option<(u64,&'a [u8])>,hdr_data:Option<(u64,&'a [u8])>)->Result<Self,MetadataError>{
  Self::elf_spans(image,text,frame_data,hdr_data,false)
 }
 pub(crate) fn elf_from_mapped_spans<'a>(image:&'a OwnedImage<'a>,text:u64,frame_data:Option<(u64,&'a [u8])>,hdr_data:Option<(u64,&'a [u8])>)->Result<Self,MetadataError>{
  Self::elf_spans(image,text,frame_data,hdr_data,true)
 }
 fn elf_spans<'a>(image:&'a OwnedImage<'a>,text:u64,frame_data:Option<(u64,&'a [u8])>,hdr_data:Option<(u64,&'a [u8])>,gather_frame:bool)->Result<Self,MetadataError>{
  use gimli::{BaseAddresses,CieOrFde,EhFrame,EhFrameHdr,LittleEndian,Pointer,UnwindSection};
  let mut bases=BaseAddresses::default().set_text(text);
  let parsed_hdr=if let Some((address,data))=hdr_data{bases=bases.set_eh_frame_hdr(address);Some(EhFrameHdr::new(data,LittleEndian).parse(&bases,8).map_err(|_|MetadataError::Malformed)?)}else{None};
  let fallback;
  let (frame_address,data)=match frame_data{
   Some(pair)=>pair,
   None=>{let h=parsed_hdr.as_ref().ok_or(MetadataError::Unavailable)?;let address=match h.eh_frame_ptr(){Pointer::Direct(a)=>a,Pointer::Indirect(_)=>return Err(MetadataError::IndirectPointer)};
    fallback=if gather_frame{crate::mapped_metadata::to_end(image,address)?}else{alloc::borrow::Cow::Borrowed(image.memory.range_from(address).map_err(|_|MetadataError::OutOfFile)?)};
    (address,fallback.as_ref())
   }
  };if let Some(h)=&parsed_hdr{match h.eh_frame_ptr(){Pointer::Direct(a) if a==frame_address=>{},Pointer::Direct(_)=>return Err(MetadataError::Malformed),Pointer::Indirect(_)=>return Err(MetadataError::IndirectPointer)}}if data.is_empty(){return Err(MetadataError::Unavailable);}bases=bases.set_eh_frame(frame_address);
  let eh=EhFrame::new(data,LittleEndian);let mut entries=eh.entries(&bases);let mut parsed=vec![];
  while let Some(entry)=entries.next().map_err(|_|MetadataError::Malformed)?{if let CieOrFde::Fde(partial)=entry{let fde=partial.parse(&mut EhFrame::cie_from_offset).map_err(|_|MetadataError::Malformed)?;if fde.cie().fde_address_encoding().is_some_and(|e|e.0 & gimli::DW_EH_PE_indirect.0!=0){return Err(MetadataError::IndirectPointer);}let record=Self::checked_function(image,fde.initial_address(),fde.len(),0)?;parsed.push((record,fde.offset()));}}
  // Validate header pairs against exact FDE starts. A header lookup alone is
  // deliberately insufficient: its nearest preceding row can lie in a gap.
  let mut indexed:Vec<_>=parsed.iter().map(|(f,o)|(*o as u64,f.range.start)).collect();indexed.sort_unstable();
  if let Some(h)=parsed_hdr{if let Some(table)=h.table(){let mut rows=table.iter(&bases);let mut checked_rows=vec![];while let Some((initial,pointer))=rows.next().map_err(|_|MetadataError::Malformed)?{let (Pointer::Direct(start),Pointer::Direct(pointer))=(initial,pointer)else{return Err(MetadataError::IndirectPointer)};let offset=pointer.checked_sub(frame_address).ok_or(MetadataError::OutOfFile)?;if indexed.binary_search(&(offset,start)).is_err(){return Err(MetadataError::Malformed);}checked_rows.push((offset,start));}checked_rows.sort_unstable();if checked_rows!=indexed{return Err(MetadataError::Malformed);}}}
  let mut records:Vec<_>=parsed.into_iter().map(|(r,_)|r).collect();records.sort_by_key(|f|(f.range.start,f.range.end));records.dedup();Ok(Self{roots:records.clone(),records,kind:ImageKind::Elf})
 }
 fn pe(bytes:&[u8],image:&OwnedImage<'_>)->Result<Self,MetadataError>{
  let file=object::File::parse(bytes).map_err(|_|MetadataError::Malformed)?;let object::File::Pe64(inner)=&file else{return Err(MetadataError::Malformed)};
  let base=file.relative_address_base();let directory=inner.data_directory(object::pe::IMAGE_DIRECTORY_ENTRY_EXCEPTION).ok_or(MetadataError::Unavailable)?;let (rva,size)=directory.address_range();if size==0{return Err(MetadataError::Unavailable);}if size%12!=0{return Err(MetadataError::Malformed);}let address=base.checked_add(rva as u64).ok_or(MetadataError::Malformed)?;Self::pe_from_spans(image,base,address,size as usize)
 }
 // Shared genuine PE runtime-function/CHAININFO parser over owned memory.
 pub(crate) fn pe_from_spans(image:&OwnedImage<'_>,base:u64,address:u64,size:usize)->Result<Self,MetadataError>{
  if size==0{return Err(MetadataError::Unavailable);}if size%12!=0{return Err(MetadataError::Malformed);}
  let table=image.memory.range_from(address).map_err(|_|MetadataError::OutOfFile)?.get(..size as usize).ok_or(MetadataError::OutOfFile)?;
  Self::pe_table(image,base,table,false)
 }
 // Mapped caller supplies the bounded exception-table bytes at original RVAs.
 pub(crate) fn pe_from_mapped_table(image:&OwnedImage<'_>,base:u64,table:&[u8])->Result<Self,MetadataError>{
  if table.is_empty(){return Err(MetadataError::Unavailable);}if table.len()%12!=0{return Err(MetadataError::Malformed);}
  Self::pe_table(image,base,table,true)
 }
 fn pe_table(image:&OwnedImage<'_>,base:u64,table:&[u8],gather_unwind:bool)->Result<Self,MetadataError>{
  let read_record=|bytes:&[u8]|->Result<OwnedFunction,MetadataError>{let begin=u32::from_le_bytes(bytes[0..4].try_into().unwrap()) as u64;let end=u32::from_le_bytes(bytes[4..8].try_into().unwrap()) as u64;let unwind=u32::from_le_bytes(bytes[8..12].try_into().unwrap()) as u64;Self::checked_function(image,base.checked_add(begin).ok_or(MetadataError::Malformed)?,end.checked_sub(begin).ok_or(MetadataError::Malformed)?,base.checked_add(unwind).ok_or(MetadataError::Malformed)?)};
  let mut records=vec![];for entry in table.chunks_exact(12){records.push(read_record(entry)?);}records.sort_by_key(|f|f.range.start);if records.windows(2).any(|p|p[0].range.end>p[1].range.start){return Err(MetadataError::Ambiguous);}let mut roots=vec![];
  for record in &records{let mut root=record.clone();let mut visited=vec![];loop{if visited.contains(&root.unwind){return Err(MetadataError::Malformed);}visited.push(root.unwind);let data=if gather_unwind {
    let header=crate::mapped_metadata::bytes(image,root.unwind,4)?;
    let version=header[0]&7;let flags=header[0]>>3;
    if version!=1&&version!=2{return Err(MetadataError::Malformed);}
    let code_bytes=(header[2]as usize).checked_mul(2).and_then(|n|n.checked_add(4)).ok_or(MetadataError::Malformed)?;
    let needed=if flags&4!=0 {code_bytes.checked_add(3).map(|n|n&!3).and_then(|n|n.checked_add(12)).ok_or(MetadataError::Malformed)?}else{code_bytes};
    crate::mapped_metadata::bytes(image,root.unwind,needed)?
   }else{alloc::borrow::Cow::Borrowed(image.memory.range_from(root.unwind).map_err(|_|MetadataError::OutOfFile)?)};let info=data.get(..4).ok_or(MetadataError::OutOfFile)?;let version=info[0]&7;let flags=info[0]>>3;if version!=1&&version!=2{return Err(MetadataError::Malformed);}let code_bytes=(info[2] as usize).checked_mul(2).and_then(|n|n.checked_add(4)).ok_or(MetadataError::Malformed)?;if code_bytes>data.len(){return Err(MetadataError::OutOfFile);}if flags&4==0{break;}if flags&3!=0{return Err(MetadataError::Malformed);}let offset=code_bytes.checked_add(3).ok_or(MetadataError::Malformed)?&!3;let chain=data.get(offset..offset.checked_add(12).ok_or(MetadataError::Malformed)?).ok_or(MetadataError::OutOfFile)?;root=read_record(chain)?;}roots.push(root);}
  Ok(Self{records,roots,kind:ImageKind::PE})
 }
}
